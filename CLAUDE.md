# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Naming Philosophy

JSOM stands for **JavaScript Object Models**. Where JSON's "N" is *Notation* (a text format for writing data), JSOM's "M" is *Models* (in-memory data structures for holding data). This distinction matters for naming:
- **JSON** refers to the string/text representation. `to_json()` means "produce the notation string."
- **JsonDocument** is the in-memory model. `parse_document()` means "parse notation into a model."

Keep this convention when adding or renaming API methods.

## Development Commands

### Building (⚠️ Always stay in project root)
```bash
# Recommended safe build (stay in root directory)
mkdir -p build
cmake -S . -B build
cmake --build build -j$(nproc)

# All executables accessible via ./build/jsom, ./build/jsom_tests, etc.
# Never cd to build/ directory to prevent accidental rm -rf * on project root

# Build with benchmarks enabled (OFF by default)
cmake -S . -B build -DJSOM_BUILD_BENCHMARKS=ON
cmake --build build -j$(nproc)

# Build without tests (when used as subdirectory, auto-disabled)
cmake -S . -B build -DJSOM_BUILD_TESTS=OFF
cmake --build build -j$(nproc)
```

**Build Options:**
- `JSOM_BUILD_BENCHMARKS` - Build performance benchmarks (default: OFF)
- `JSOM_BUILD_TESTS` - Build test suite (default: ON when top-level, OFF when used as dependency)

### Testing
```bash
# Run all tests
./build/jsom_tests
# Or via make target
cmake --build build --target run_tests

# Run specific test suites
./build/jsom_tests --gtest_filter="JsonPointerTest.*"
./build/jsom_tests --gtest_filter="ParseDocumentTest.*"
./build/jsom_tests --gtest_filter="PerformanceRegressionTest.*"

# Memory checking
cmake --build build --target memcheck_tests
```

### Code Quality
```bash
# Format code
cmake --build build --target format

# Static analysis
cmake --build build --target tidy

# Comprehensive validation
cmake --build build --target validation
```

### Local CI (the gates, run by the hooks)

`tools/ci.sh` runs every gate from `CODING_STANDARDS.md` (`tree format build tests asan
fuzz tsan std cli conform tidy pristine`, and `coverage` on request); `.githooks/pre-commit`
runs `build tests`, and
`.githooks/pre-push` runs the full set and additionally requires a clean worktree. Enable
once per clone with `git config core.hooksPath .githooks`. Per-machine settings live in
`.ci.env` (gitignored; see `.ci.env.example`) — stage output goes to `.ci-logs/`.
Use it directly while working: `tools/ci.sh build tests`, `tools/ci.sh pristine`,
`tools/ci.sh --list`.

### Benchmarking
```bash
# Run all benchmarks
./build/jsom_benchmarks

# Specific benchmark categories  
./build/jsom_benchmarks --benchmark_filter="Parse.*"
./build/jsom_benchmarks --benchmark_filter="NumberHeavy.*"

# CLI benchmarking
./build/jsom benchmark test_data.json
```

### Fuzzing
```bash
cmake --build build --target fuzz_quick     # 1-minute test
cmake --build build --target fuzz           # 10-minute test  
cmake --build build --target fuzz_long      # 1-hour test
```

## Architecture Overview

JSOM is a high-performance C++17 JSON parser with RFC 6901 JSON Pointer support and advanced formatting capabilities.

### Core Components

**JsonDocument** (`include/jsom/json_document.hpp`):
- Central document class using `std::variant` for type-safe value storage
- Implicit construction from primitives (`int`, `double`, `bool`, `string`, `const char*`, `nullptr`)
- Lazy number parsing via `LazyNumber` class preserves original format
- Iteration: `begin()`/`end()` for arrays, `items()` for objects (structured bindings), `keys()`
- Full comparison operators (`==`, `!=`, `<`, `>`, `<=`, `>=`) with deep structural comparison
- Built-in JSON Pointer navigation (RFC 6901), reading the document directly
- Advanced formatting via `JsonFormatOptions`

**JSON Pointer System** (`include/jsom/json_pointer.hpp`, `navigation_engine.hpp`):
- Full RFC 6901 compliance with escape sequence handling
- `NavigationEngine::find()`: the one navigation implementation — const-correct, no cache,
  no mutation, so a const document can be navigated from several threads at once (gated
  by the `tsan` stage). The mutable overload holds the single documented `const_cast`.
- `find_multiple()` for batches; path introspection (`list_paths`, `find_paths`, `count_paths`)

**Parsing System** (`include/jsom/fast_parser.hpp`, `parse_document.hpp`):
- `FastParser`: the only parser — direct-construction recursive-descent, with optional comment support and the RFC 8259 lexical rules AND number grammar always enforced (the grammar is checked inside the scan, which measured *faster* than not checking it)
- `parse_document.hpp`: the `parse_document()` entry points (default and with options)
- `JsonParseOptions`: Unicode escape handling (`convert_unicode_escapes`), comment tolerance (`allow_comments`), resource limits (`max_depth`) and the loose-number extension switch (`allow_loose_numbers`, off by default — the §6 grammar is enforced in the scan)
- Version: `project(JSOM VERSION ...)` in `CMakeLists.txt` is the single source; CMake generates `<jsom/version.hpp>` (`JSOM_VERSION`). Never hard-code a version anywhere else.

**Formatting System** (`include/jsom/json_formatter.hpp`, `json_format_options.hpp`, `utf8.hpp`):
- Intelligent formatting with 5 built-in presets (compact, pretty, config, api, debug)
- Smart inlining decisions based on content size and complexity — decided PER container, so
  a container holding a container goes multiline while a small container of scalars inlines
- Escaping: control characters are ALWAYS escaped (a raw one is not valid JSON);
  `escape_unicode` escapes CODEPOINTS via `utf8::decode` (surrogate pairs above U+FFFF),
  never UTF-8 bytes — `\u00c3\u00a4` is not "ä"
- Behaviour is pinned by `tests/test_formatter_invariants.cpp` (round-trip, idempotence),
  `tests/test_formatter_options.cpp` (one test per option) and `tests/test_utf8.cpp`

### Key Design Patterns

**Lazy Evaluation**: Numbers stored as strings until accessed, preserving original format for round-trip fidelity

**Pure Navigation**: Path lookups read the document directly — no cache between the caller
and the data, so a lookup cannot go stale and const reads stay thread-safe. A three-level
path cache was measured (17.97x slower on reading every path once) and deleted; see the
"Path cache removed" section in `OPTIMIZATIONS.md` before adding any memoisation back.

**Template-Heavy Headers**: Most functionality in headers for compile-time optimization, minimal .cpp files

**Zero-Cost Abstractions**: JSON Pointer functionality is only paid for when used, and
navigation allocates nothing on the way down

### File Organization

- `include/jsom/`: All public headers, fully self-contained
- `src/`: Minimal implementation files (CLI + path operations + formatting)  
- `tests/`: Comprehensive test suite (run `./build/jsom_tests` for current count)
- `benchmarks/`: Performance comparison against nlohmann/json
- `FORMATTING.md`: Detailed formatting system documentation

### Constants and Configuration

Key constants defined in `include/jsom/constants.hpp`:
- Nesting and size limits, and the JSON Pointer/parse presets
- Parser buffer sizes and allocation strategies
- Character handling and Unicode processing settings
- CLI formatting and benchmark parameters

### CLI Application

The `jsom` executable (`src/jsom_cli.cpp`) provides:
- JSON formatting with all preset options
- `--comments` flag for comment-tolerant parsing (format and validate commands)
- Complete JSON Pointer operations (get, set, remove, list, etc.)
- Validation and benchmarking tools
- Full integration with the library's advanced features

Always use `./build/jsom` from project root rather than changing to build directory.

## C++ Standards (MANDATORY)

All C++ work in this repo MUST follow `CODING_STANDARDS.md` — modern C++17 in
the spirit of the C++ Core Guidelines (Type/Bounds/Lifetime profiles),
exceptions allowed for error handling. This is binding for every agent run.

Gates before any change is done (see the Definition of Done in
CODING_STANDARDS.md):

1. `cmake --build build` — zero warnings (project targets use `-Werror`).
2. `./build/jsom_tests` — all tests pass; TDD (failing test first) for every
   behavior change or bug fix.
3. Sanitizer gate: `cmake -B build-asan -DJSOM_SANITIZE=ON && cmake --build
   build-asan -j$(nproc) && ./build-asan/jsom_tests` — clean under ASan+UBSan.
4. Thread gate: `tools/ci.sh tsan` — const reads from several threads must stay
   race-free (`-DJSOM_SANITIZE=thread`, ~6 s). Const access may not modify hidden
   state: no `mutable` written from a const member, no `const_cast` on `this`
   (CODING_STANDARDS rule 11).
5. Fuzzing: input-handling changes run the fuzz targets briefly
   (`cmake --build build --target fuzz_quick`); a crash is a bug, not user
   error. Performance work is measured before it lands (interleaved A/B with
   `tools/perf_probe.cpp`; see `OPTIMIZATIONS.md`).
6. Never introduce raw owning pointers, `new`/`delete`, or
   `reinterpret_cast`/C-style casts.
7. If a build under these gates fails because of a pre-existing warning, fix
   the warning (small, targeted change) rather than weakening the flags.
