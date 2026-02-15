# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

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
- Built-in JSON Pointer navigation with multi-level caching
- Advanced formatting via `JsonFormatOptions`

**JSON Pointer System** (`include/jsom/json_pointer.hpp`, `navigation_engine.hpp`, `path_cache.hpp`):
- Full RFC 6901 compliance with escape sequence handling
- `NavigationEngine`: Core path navigation with prefix optimization
- `PathCache`: Multi-level LRU+prefix caching for performance
- Batch operations and path introspection capabilities

**Parsing System** (`include/jsom/fast_parser.hpp`, `streaming_parser.hpp`):
- `FastParser`: Optimized direct-construction parsing with optional comment support
- `StreamingParser`: Event-based parsing for large documents
- `BatchParser`: Optimized for parsing multiple documents
- `JsonParseOptions`: Configurable Unicode handling and comment tolerance (`allow_comments`)

**Formatting System** (`include/jsom/json_formatter.hpp`, `json_format_options.hpp`):
- Intelligent formatting with 5 built-in presets (compact, pretty, config, api, debug)
- Smart inlining decisions based on content size and complexity
- Advanced options: key sorting, Unicode handling, alignment, wrapping

### Key Design Patterns

**Lazy Evaluation**: Numbers stored as strings until accessed, preserving original format for round-trip fidelity

**Caching Strategy**: Three-level path cache (exact paths, prefixes, recent prefixes) for optimal JSON Pointer performance

**Template-Heavy Headers**: Most functionality in headers for compile-time optimization, minimal .cpp files

**Zero-Cost Abstractions**: JSON Pointer functionality only activated when used, path cache created lazily

### File Organization

- `include/jsom/`: All public headers, fully self-contained
- `src/`: Minimal implementation files (CLI + path operations + formatting)  
- `tests/`: Comprehensive test suite with 161 test cases
- `benchmarks/`: Performance comparison against nlohmann/json
- `FORMATTING.md`: Detailed formatting system documentation

### Constants and Configuration

Key constants defined in `include/jsom/constants.hpp`:
- Cache sizes and eviction policies
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
