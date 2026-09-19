# JSOM - JavaScript Object Models

## Why "JSOM"?

In **JSON** (JavaScript Object **Notation**), the "N" stands for *Notation* — a way of **writing** data as text. JSOM replaces "Notation" with **Models** — the in-memory data structures that **hold** the data.

This distinction is central to the library's design:

- **JSON** is the string representation: `{"name": "Alice", "age": 30}`
- **JSOM** is the in-memory model: a `JsonDocument` with typed fields, navigation, and mutation

The API reflects this consistently:
- `parse_document(str)` — reads *notation* into a *model*
- `doc.to_json()` — converts a *model* back to *notation* (a string)

---

A fast, modern C++17 library for working with JSON data in memory. Features lazy evaluation, RFC 6901 JSON Pointer support, and intelligent formatting. Fully compatible with C++20, C++23, and beyond.

## Features

- **Lazy number parsing** - 2x performance improvement for number-heavy JSON
- **RFC 6901 JSON Pointers** - Full standard compliance with path caching optimization
- **Intelligent formatting** - Multiple presets for different use cases (compact, pretty, config, API, debug)
- **Memory efficient** - Optimized allocation patterns, ~50% reduction vs naive double-buffering
- **Modern C++** - Safe, clean C++17 implementation using std::variant and RAII, compatible with C++20+
- **Zero dependencies** - Self-contained with optional benchmarking against nlohmann/json
- **Comprehensive CLI** - Full-featured command-line tool for JSON operations
- **Implicit construction** - Natural syntax: `JsonDocument doc = 42;`, clean object init-lists
- **Iteration support** - Range-for on arrays, structured bindings on objects via `items()`
- **Comparison operators** - Full set of `==`, `!=`, `<`, `>`, `<=`, `>=` with deep structural comparison
- **Comment-tolerant parsing** - Optional `//` and `/* */` comment support for config files
- **Bounded recursion** - Every traversal is depth-limited (default 256, tunable), so no input can exhaust the stack

## Performance

JSOM achieves significant performance improvements through:
- **Lazy number evaluation** - Numbers parsed on-demand, preserving format
- **Direct construction parsing** - Eliminates intermediate allocations (22% allocation overhead eliminated)
- **Path prefix caching** - Intelligent caching for related JSON Pointer operations
- **Move semantics** - Optimal C++17 move operations throughout

Benchmark results show 2.01x performance improvement over baseline with full functionality preserved.

## Building

### Version

The version lives in exactly one place — the `project(JSOM VERSION ...)` line in
`CMakeLists.txt`. CMake generates `<jsom/version.hpp>` from it at configure time, so the
CLI (`jsom version`), your own code, and the release tag all read the same value:

```cpp
#include <jsom/version.hpp>
std::cout << JSOM_VERSION;        // "2.0.0"; also JSOM_VERSION_MAJOR/MINOR/PATCH
```

To release: edit that one line, rebuild, tag `v<version>`.

### Requirements
- **C++17 or newer** (C++20, C++23, etc. fully supported)
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.10+

**Note:** The library uses C++17 internally but works seamlessly with projects using C++20, C++23, or any future C++ standard. Your project is not forced to use a specific C++ version.

### ⚠ Safety Note
**Always stay in the project root directory when possible.** Avoid changing to `./build/` directory during development to prevent accidental `rm -rf *` commands from deleting your entire project. Use relative paths like `./build/jsom_tests` instead of changing directories.

### Quick Build
```bash
cmake -B build
cmake --build build
```

### Recommended Safe Build (Stay in Root)
```bash
mkdir -p build
cmake -S . -B build
cmake --build build -j$(nproc)
# All executables accessible via ./build/jsom, ./build/jsom_tests, etc.
```

### Using in Your C++20/C++23 Project
```bash
# JSOM works with any C++17+ standard
cmake -S . -B build -DCMAKE_CXX_STANDARD=20  # Or 23, 26, etc.
cmake --build build -j$(nproc)
```

### Build Options
```bash
# Release build with optimizations (from project root)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release   # (Release is now the DEFAULT)
cmake --build build -j$(nproc)

# Build with benchmarks enabled (OFF by default)
cmake -S . -B build -DJSOM_BUILD_BENCHMARKS=ON
cmake --build build -j$(nproc)

# Run all tests (from project root)
./build/jsom_tests
# Or use make targets:
cmake --build build --target run_tests

# Run benchmarks (from project root)
./build/jsom_benchmarks
# Or use make targets:
cmake --build build --target run_benchmarks
```

## Local CI (git hooks)

Every gate in `CODING_STANDARDS.md` runs locally, from one script — no GitHub, no
network, no framework. `tools/ci.sh` is non-destructive: it never commits, stages,
reverts or reformats anything.

```bash
git config core.hooksPath .githooks    # once per clone — enables the hooks
tools/ci.sh                            # all stages, by hand, any time
tools/ci.sh build tests                # named stages only, in the order given
tools/ci.sh --list                     # what the stages are
```

| hook | runs | cost |
|---|---|---|
| `pre-commit` | `build tests` | ~15 s |
| `pre-push` | `tree format build tests asan fuzz conform tidy pristine` | ~2–4 min |

A failing stage stops the run, blocks the commit or push, and prints its reason plus the
tail of its log; full output lands in `.ci-logs/<stage>.log`. The bypass is git's own:
`git commit --no-verify`, `git push --no-verify`.

**Pristine** is the stage worth knowing about: it does `git archive HEAD` into a temp
directory and then configures, builds and tests *that*. It proves the committed tree is
complete on its own — the only way to catch a file that is needed to build but was never
committed, or one that `.gitignore` swallows by mistake.

**Conform** asserts the strict verdict (`--validation=numbers`: `y_` 95/95, `n_` 188/188)
and reports the default-mode count; the number grammar is opt-in, so the default 162/188
is reported, not failed.

**Tidy** runs clang-tidy over `src/` and `include/` and requires **zero** findings. An
optional baseline (`.ci/tidy-baseline.txt`, see `.ci.env.example`) can tolerate known
findings while still failing on new ones — the repo currently needs none.

Machine-specific settings — job count, fuzz seconds, stage list, whether a missing tool
fails the run — live in `.ci.env`, which is gitignored. Copy `.ci.env.example` and edit;
every knob has a default in `tools/ci.sh`, so an unconfigured clone still works.

## Installing
```bash
sudo cmake --build build --target install
```

## Integrating JSOM into Your Project

### Option 1: Install System-Wide (Recommended for frequent use)

Install JSOM once on your system:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
sudo cmake --install build
```

Then in your project's `CMakeLists.txt`:
```cmake
find_package(jsom REQUIRED)
target_link_libraries(your_target PRIVATE JSOM::jsom)
```

### Option 2: CMake FetchContent (Fast and Simple)

For projects using CMake FetchContent, JSOM integrates cleanly with fast configuration times (~2 seconds):

```cmake
include(FetchContent)

FetchContent_Declare(
    JSOM
    GIT_REPOSITORY https://github.com/HarryPehkonen/JSOM.git
    GIT_TAG main  # Or pin to specific version: GIT_TAG v1.0.0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(JSOM)

target_link_libraries(your_target PRIVATE JSOM::jsom)
```

**Optional: Enable benchmarks if needed**
```bash
cmake -B build -DJSOM_BUILD_BENCHMARKS=ON
```

**Important Notes:**
- JSOM requires C++17, but your project can use C++20, C++23, or later
- When used as a dependency, tests and benchmarks are automatically disabled for fast builds
- Use the namespaced `JSOM::jsom` target (modern CMake convention)

**Type Handling Tip:** If you encounter ambiguous overload errors with `int64_t`, use explicit casts:
```cpp
// Use int for JSON numbers
JsonDocument(static_cast<int>(count))   // Good
// Avoid int64_t (can be ambiguous)
JsonDocument(static_cast<int64_t>(val)) // May cause overload issues
```

## Usage

### Command Line Tool
```bash
# Format JSON with different presets (from project root)
./build/jsom format --preset=pretty data.json
./build/jsom format --preset=compact data.json
./build/jsom format --preset=config settings.json
./build/jsom format --indent=4 --max-width=80 data.json

# Format JSON with comments (e.g., config files)
./build/jsom format --comments config.jsonc

# Validate JSON files
./build/jsom validate file1.json file2.json
./build/jsom validate --comments config.jsonc

# JSON Pointer operations (RFC 6901)
./build/jsom pointer get "/users/0/name" data.json
./build/jsom pointer exists "/config/database/host" config.json
./build/jsom pointer list --max-depth=3 data.json
./build/jsom pointer find "/users/*/email" data.json
./build/jsom pointer bulk-get "/users/0/name,/users/0/age" data.json

# Quick performance benchmarking (simple timing)
./build/jsom benchmark large.json
```

### C++ API

#### Parsing JSON
```cpp
#include <jsom/jsom.hpp>
using namespace jsom;
using namespace jsom::literals;  // For _jsom literal

// Parse JSON (traditional way)
auto doc = parse_document(R"({"name": "John", "age": 30, "scores": [85, 92]})");

// Parse JSON with _jsom literal (cleaner syntax)
auto config = R"({
    "server": "localhost",
    "port": 8080,
    "features": ["auth", "logging"]
})"_jsom;

// Type-safe access
if (doc.is_object()) {
    std::string name = doc["name"].as<std::string>();
    int age = doc["age"].as<int>();

    // Array access
    if (doc["scores"].is_array()) {
        int first_score = doc["scores"][0].as<int>();
    }
}
```

#### Constructing JSON Documents

Implicit construction from primitives enables clean, natural syntax:

```cpp
// Implicit construction from primitives
JsonDocument name = "Alice";
JsonDocument age = 30;
JsonDocument pi = 3.14;
JsonDocument active = true;
JsonDocument nothing = nullptr;

// Object initializer-list syntax (no wrappers needed)
JsonDocument user{{"name", "Alice"}, {"age", 30}, {"active", true}};

// Implicit conversion works in set() and push_back() too
auto obj = JsonDocument::make_object();
obj.set("name", "Bob");
obj.set("score", 95.5);

auto arr = JsonDocument::make_array();
arr.push_back(1);
arr.push_back("hello");
arr.push_back(true);

// Arrays via vector constructor
auto numbers = JsonDocument(std::vector<JsonDocument>{1, 2, 3});
```

#### Building JSON from C++ Containers

JSOM provides ergonomic ways to construct JSON documents from standard C++ containers:

```cpp
// From C++ containers with automatic type conversion
std::map<std::string, int> scores = {{"alice", 95}, {"bob", 87}};
auto doc = JsonDocument::from_map(scores);
// Result: {"alice":95,"bob":87}

std::vector<std::string> names = {"Alice", "Bob", "Charlie"};
auto arr = JsonDocument::from_vector(names);
// Result: ["Alice","Bob","Charlie"]

// From containers requiring conversion
std::map<std::string, size_t> frequency_map = {{"apple", 5}, {"banana", 3}};
auto doc = JsonDocument::from_map(frequency_map, [](size_t v) {
    return JsonDocument(static_cast<int>(v));
});

// Direct construction from JsonDocument containers (zero-copy move)
std::map<std::string, JsonDocument> obj_map;
obj_map["name"] = "Alice";
obj_map["age"] = 30;
JsonDocument doc(std::move(obj_map));
```

#### Creating Empty Documents

```cpp
// Static factory methods for empty containers
auto arr = JsonDocument::make_array();
auto obj = JsonDocument::make_object();
```

#### Querying Documents

```cpp
// Type-safe value access: as<T>() throws TypeException on mismatch
int age = doc["age"].as<int>();

// Safe access without exceptions: try_as<T>() returns std::optional<T>
std::optional<int> maybe_age = doc["age"].try_as<int>();
if (auto name = doc["name"].try_as<std::string>()) {
    std::cout << *name << "\n";
}

// Reference accessors (no copy, const access to underlying containers)
const auto& elems = doc.as_array();    // throws if not array
const auto& fields = doc.as_object();  // throws if not object

// Size and emptiness
doc.size();       // element count for arrays and objects; throws on primitives
doc.empty();      // true for null, empty arrays, and empty objects; throws on primitives

// Key lookup
doc.contains("name");  // true if object has key; throws on non-objects
doc.keys();             // returns vector<string> of object keys
```

#### Iterating Documents

```cpp
// Range-for on arrays
auto arr = JsonDocument(std::vector<JsonDocument>{1, 2, 3});
for (const auto& elem : arr) {
    std::cout << elem.as<int>() << "\n";
}

// Structured bindings on objects via items()
JsonDocument config{{"host", "localhost"}, {"port", 8080}};
for (const auto& [key, value] : config.items()) {
    std::cout << key << ": " << value.to_json() << "\n";
}

// Mutable iteration
for (auto& [key, value] : config.items()) {
    value = "overwritten";
}
```

#### Comparing Documents

```cpp
// Deep structural equality
JsonDocument a{{"x", 1}, {"y", 2}};
JsonDocument b{{"x", 1}, {"y", 2}};
assert(a == b);

// Implicit conversion in comparisons
JsonDocument age = 30;
assert(age == 30);
assert(age < 100);
assert(age != "thirty");

// Total ordering (Null < Bool < Number < String < Object < Array)
assert(JsonDocument() < JsonDocument(false));
assert(JsonDocument(0) < JsonDocument("zero"));
```

#### Mutating Documents

```cpp
// Set object keys (implicit conversion, no wrapper needed)
doc.set("name", "Alice");
doc.set("age", 30);

// Set array elements by index (resizes if needed)
doc.set(0, "replaced");

// Append to arrays
auto arr = JsonDocument::make_array();
arr.push_back(1);
arr.push_back("two");
```

##### Reference Invalidation and Cache Safety

References obtained from `operator[]`, `at()`, `as_array()`, or `as_object()` follow
standard C++ container rules: any mutation that can reallocate the underlying container
(`push_back`, or an index-based `set()` that grows an array) invalidates references and
pointers into it.

The internal JSON Pointer caches are exempt from this concern: every mutation increments
a global mutation epoch, so cached paths held by this document *or any ancestor* are
detected as stale and re-navigated instead of dereferencing freed memory. You never need
to manually clear caches after mutating.

```cpp
auto& name = doc.at("/users/0/name");   // reference into the document
doc.at("/users").push_back(new_user);   // may reallocate the users array...
// `name` may now dangle (standard vector rules) -- re-navigate instead:
auto& fresh = doc.at("/users/0/name");  // cache detects the mutation, safe

// For structural changes deep in a document, prefer the root-level
// JSON Pointer mutators, which avoid holding references across mutations:
doc.set_at("/users/0/name", "Alice");
doc.remove_at("/users/0/temp");
```

#### JSON Pointer Operations
```cpp
// JSON Pointer operations
auto value = doc.at("/users/0/name");
bool exists = doc.exists("/config/database/host");
auto paths = doc.list_paths();

// Formatting with options
std::string compact = doc.to_json(FormatPresets::Compact);
std::string pretty = doc.to_json(FormatPresets::Pretty);

JsonFormatOptions custom;
custom.indent_size = 4;
custom.max_line_width = 80;
std::string formatted = doc.to_json(custom);

// Create documents with _jsom literal
auto user = R"({"name": "Alice", "age": 25})"_jsom;
auto numbers = R"([1, 2, 3, 4, 5])"_jsom;
auto simple = R"("hello world")"_jsom;

// Unicode escape handling
auto default_doc = parse_document(R"({"text": "\u0041\uD83D\uDE00"})");
// Preserves as literal: "\\u0041\\uD83D\\uDE00" (round-trip compatible)

auto unicode_doc = parse_document(R"({"text": "\u0041\uD83D\uDE00"})", ParsePresets::Unicode);
// Converts to UTF-8: "A "
```

### Parsing

`parse_document()` is the entry point. Behind it is one parser — `FastParser`, a
direct-construction recursive-descent parser that applies the RFC 8259 lexical rules to
every input (see "Lexical rules" below):

```cpp
auto doc  = parse_document(json);                          // default options
auto doc2 = parse_document(json, ParsePresets::Unicode);   // a preset
JsonParseOptions options;
options.max_depth = 64;                                    // resource limits
auto doc3 = parse_document(json, options);
```

### Error Handling
```cpp
try {
    auto doc = parse_document(invalid_json);
} catch (const std::runtime_error& e) {
    // Parse errors
}

try {
    auto value = doc.at("/nonexistent/path");
} catch (const JsonPointerNotFoundException& e) {
    // Path not found
}
```

## Format Presets

- **`compact`** - Minimal bandwidth, storage efficiency
- **`pretty`** - General-purpose readable formatting (default)
- **`config`** - Configuration files with aligned values
- **`api`** - API responses with consistent structure  
- **`debug`** - Maximum readability for development

Each preset can be customized with additional options like `--indent`, `--max-width`, `--inline-arrays`, etc.

## JSON Pointer Support

JSOM provides comprehensive RFC 6901 JSON Pointer support with advanced optimizations and performance enhancements.

### RFC 6901 Compliance

Full compliance with the RFC 6901 JSON Pointer standard:
- **Standard escape sequences**: Proper handling of `~0` (for `~`) and `~1` (for `/`)
- **Array index validation**: Strict validation of array indices (no leading zeros except "0")
- **Path resolution**: Correct navigation through nested objects and arrays
- **Error handling**: Comprehensive exception hierarchy with specific error types

### Core Navigation Operations

```cpp
// Navigate to a path (throws JsonPointerNotFoundException if not found)
auto value = doc.at("/users/0/name");

// Safe navigation (returns nullptr if not found)
auto* safe_value = doc.find("/users/0/name");

// Check path existence
bool exists = doc.exists("/users/0/profile/email");
```

### Path Modification Operations

```cpp
// Set value at path (creates intermediate objects/arrays as needed)
doc.set_at("/users/0/profile/active", JsonDocument(true));

// Remove value at path
bool removed = doc.remove_at("/users/0/age");

// Extract value (remove and return)
auto extracted = doc.extract_at("/config/temp_setting");
```

### Batch Operations

Optimized for processing multiple paths efficiently:

```cpp
// Get multiple values in one operation
std::vector<std::string> paths = {"/users/0/name", "/users/1/name", "/config/host"};
auto results = doc.at_multiple(paths);

// Check multiple paths exist
auto existence_checks = doc.exists_multiple(paths);
```

### Path Introspection

```cpp
// List all available paths
auto all_paths = doc.list_paths();

// List paths with depth limit
auto shallow_paths = doc.list_paths(2);

// Find paths matching pattern
auto user_emails = doc.find_paths("/users/*/email");

// Count total available paths
size_t path_count = doc.count_paths();
```

### Performance Optimizations

JSOM includes advanced caching for high-performance path operations:

- **Multi-level caching**: LRU cache for exact paths, prefix cache for related operations
- **Prefix optimization**: Intelligent caching of intermediate path segments
- **Batch optimization**: Sorted processing for maximum cache reuse

```cpp
// Pre-warm cache for known access patterns
std::vector<std::string> likely_paths = {"/users/0/name", "/users/0/profile"};
doc.warm_path_cache(likely_paths);

// Precompute paths for complex documents
doc.precompute_paths(3); // Precompute up to depth 3

// Get cache performance statistics
auto stats = doc.get_path_cache_stats();
```

### Command Line Interface

Complete CLI support for all JSON Pointer operations:

```bash
# Navigation operations
jsom pointer get "/users/0/name" data.json
jsom pointer exists "/config/database/host" config.json
jsom pointer find "/users/*/email" data.json

# Modification operations  
jsom pointer set "/users/0/active" true data.json
jsom pointer remove "/users/0/temp_field" data.json
jsom pointer extract "/config/deprecated" data.json

# Batch and introspection operations
jsom pointer bulk-get "/users/0/name,/users/1/name,/config/port" data.json
jsom pointer list --max-depth=3 --include-values data.json
jsom pointer benchmark "/users/0/name,/config/database/host" data.json
```

### Error Handling

Comprehensive exception hierarchy for robust error handling:

```cpp
try {
    auto value = doc.at("/nonexistent/path");
} catch (const JsonPointerNotFoundException& e) {
    // Path does not exist
    std::cout << "Path not found: " << e.get_pointer() << std::endl;
} catch (const JsonPointerTypeException& e) {
    // Type mismatch (e.g., trying to index into a string)
    std::cout << "Type error at: " << e.get_pointer() << std::endl;
} catch (const InvalidJsonPointerException& e) {
    // Malformed pointer syntax
    std::cout << "Invalid pointer: " << e.get_pointer() << std::endl;
}
```

## String Escape Handling

JSOM handles JSON string escape sequences as follows:

### Standard Escapes (Converted)
These escape sequences are converted to their actual characters:
- `\"` → `"` (quotation mark)
- `\\` → `\` (backslash)
- `\/` → `/` (forward slash)
- `\n` → newline character
- `\r` → carriage return
- `\t` → tab character
- `\b` → backspace
- `\f` → form feed

### Unicode Escapes (Configurable)
Unicode escape sequences can be handled in two ways:

#### Default Behavior (Preserves Escapes)
- `\uXXXX` → kept as literal `\uXXXX` (not converted to UTF-8 characters)
- Maintains exact **round-trip fidelity** - parsing and re-serializing produces identical output

```cpp
// Default: Unicode escapes are preserved
auto doc = parse_document(R"({"letter": "\u0041", "emoji": "\uD83D\uDE00"})");
std::string letter = doc["letter"].as<std::string>();  // Contains "\\u0041", not "A"
std::string serialized = doc.to_json();               // Preserves \u0041 in output
```

#### Unicode Conversion Option
- `\uXXXX` → converted to actual UTF-8 characters
- Supports surrogate pairs for full Unicode support (emojis, etc.)

```cpp
// Convert Unicode escapes to UTF-8
auto doc = parse_document(R"({"letter": "\u0041", "emoji": "\uD83D\uDE00"})", ParsePresets::Unicode);
std::string letter = doc["letter"].as<std::string>();  // Contains "A"
std::string emoji = doc["emoji"].as<std::string>();    // Contains " "

// Custom parse options
JsonParseOptions options;
options.convert_unicode_escapes = true;
auto custom_doc = parse_document(json_string, options);
```

Regular UTF-8 characters in JSON strings work normally without escaping in both modes.

## Comment-Tolerant Parsing

JSOM supports optional `//` line comments and `/* */` block comments for parsing configuration files, JSONC, and other commented JSON formats. Comments are **disabled by default** for strict JSON compliance.

```cpp
// Enable via preset
auto doc = parse_document(json, ParsePresets::Comments);

// Or via options
JsonParseOptions opts;
opts.allow_comments = true;
auto doc = parse_document(R"({
    // Database configuration
    "host": "localhost",  /* default host */
    "port": 5432
})", opts);
```

CLI support:
```bash
./build/jsom format --comments config.jsonc
./build/jsom validate --comments config.jsonc
```

## Lexical rules (always enforced)

Three of the RFC 8259 lexical rules are not configurable — they are what makes a
document valid JSON, so they are enforced however the parser is called:

1. a raw control character (U+0000–U+001F) inside a string is rejected (§7 requires it
   escaped) — `Unescaped control character in string`
2. `\u` must be followed by exactly four hex digits — `Invalid hex digit in unicode escape`
3. an escape outside `" \ / b f n r t u` is rejected — `Invalid escape sequence: \U`
   (an escape's backslash is never dropped, so no accepted document is altered)
4. whitespace is exactly space, tab, LF and CR (§2) — formfeed and vertical tab are not
   whitespace (`std::isspace()` is locale-dependent, so the set is spelled out)

Cost: +1.5% on string-heavy parsing, nothing measurable elsewhere (measured; see
"Lexical rules" in `OPTIMIZATIONS.md`).

## Number validation (opt-in)

Numbers are stored lazily — JSOM keeps the original text so round trips are byte-exact
and nothing is converted until you ask for a value. That also means the number grammar
is not enforced during the scan, so `-01`, `1.0.`, `2.e+3`, `0e+` and `[-]` parse.
RFC 8259 §9 allows a parser to accept non-JSON forms, so the lenient default is a
policy, not an accident; when the input is not yours, enforce the other one:

```cpp
JsonParseOptions options;
options.validate_numbers = true;                  // rejects "Invalid number: 01"
auto doc = parse_document(json, options);

auto strict = parse_document(json, ParsePresets::Validate);  // the same thing, as a preset
```

The same switch on the command line:

```bash
jsom validate --validation=numbers data.json   # also enforce the number grammar
jsom format   --validation=numbers data.json   # ...while formatting
```

Cost, measured on a Release build (see "Number validation" in `OPTIMIZATIONS.md`):
**+6%** parsing 2,000 short numbers, **+12%** for 17-digit numbers with exponents,
**+0.5%** on a realistic mixed payload. The default path pays ~1–2% on number-heavy
input for the branch.

Validation does **not** force conversion: `1.500` and `1e10` still serialize back
byte-for-byte, and access stays lazy.

## Resource Limits (RFC 8259 §9)

RFC 8259 §9 says an implementation *may* set limits on the size of texts it accepts
and on the maximum depth of nesting. JSOM sets the depth limit deliberately.

The reason is that recursion cannot fail gracefully: an unbounded recursive-descent
parser exhausts the C++ stack and dies with SIGSEGV — not an exception, so nothing can
catch it, and on a 1 MB worker-thread stack about 3 KB of `[` is enough. Measured
2026-09-16 (gcc, x86-64), parsing without a depth bound:

| input | 8 MB main-thread stack | 1 MB worker-thread stack |
|---|---|---|
| 20,000 nested arrays | parses | SIGSEGV at ~3,000 levels |
| 24,000 nested arrays | SIGSEGV | SIGSEGV |
| 100,000 `[`, closers not needed | SIGSEGV | SIGSEGV |

So the bound is taken on the way *in*, and every traversal honours it: parsing,
serialization (compact and pretty), comparison and path listing all refuse a document
deeper than the limit, with

```text
std::runtime_error: Maximum nesting depth exceeded (limit 256)
```

### Choosing your limit

The default is 256, which fits in a 1 MB thread stack with ~7x headroom. The cost is
per nesting level and was measured; the worst case is an unoptimised debug build:

| path | -O2 | -O0 |
|---|---|---|
| parse | 291 B | 323 B |
| compare | 64 B | 323 B |
| serialize (compact) | 533 B | 565 B |

Budget ~0.6 KB per level (roughly double under AddressSanitizer) and pick from the
smallest stack you run on:

```cpp
JsonParseOptions options;
options.max_depth = 64;                    // e.g. a small dedicated worker thread
auto doc = parse_document(json, options);  // 64 levels ≈ 40 KB, worst case
```

The guard itself costs one comparison per container: measured at ~1.7% on a 200-deep
document (70.4 vs 69.2 ns per level) and within noise on realistic shapes — see
"Depth guard" in `OPTIMIZATIONS.md`.

**One asymmetry worth knowing:** `max_depth` tunes *parsing*. The traversal bound
(serialize, compare, path listing) is the fixed compile-time constant, so raising
`max_depth` above `limits::MAX_NESTING_DEPTH` lets you parse documents that those
operations will then refuse to serialize or compare. Lowering it is the supported
direction; if you need deep documents end to end, raise the constant itself and accept
the stack cost that implies.

No file the RFC 8259 conformance suite requires us to *accept* nests deeper than 3
levels, so 256 is far more generous than real data. The deepest case the suite
exercises at all — `i_structure_500_nested_arrays.json`, where the suite explicitly
abstains — is rejected by policy; raise `max_depth` if you want it.

### What is not bounded

The depth limit bounds *recursion*. It does not bound heap or CPU: JSOM still
allocates in proportion to the text it accepts, because a library cannot know how much
memory its caller intends to spend. Set that bound at your trust boundary (cap request
bodies, or check the size before parsing) — RFC 8259 §9 permits it there too.

## Testing

```bash
# Run all tests
./build/jsom_tests

# Run specific test categories
./build/jsom_tests --gtest_filter="ParseDocumentTest.*"
./build/jsom_tests --gtest_filter="JsonPointerTest.*"

# Performance regression tests
./build/jsom_tests --gtest_filter="PerformanceRegressionTest.*"
```

## Benchmarking

JSOM provides two benchmarking options:

### Quick Benchmarking
Built into the CLI for simple parse/serialize timing:
```bash
# Time parsing and serialization of a file (from project root)
./build/jsom benchmark large.json
echo '{"test": 123}' | ./build/jsom benchmark
```

### Comprehensive Benchmarking
Professional benchmark suite comparing with nlohmann/json:
```bash
# Run all benchmarks with statistical analysis
./build/jsom_benchmarks

# Specific benchmark categories
./build/jsom_benchmarks --benchmark_filter="Parse.*"
./build/jsom_benchmarks --benchmark_filter="NumberHeavy.*"

# Export results to JSON
./build/jsom_benchmarks --benchmark_format=json --benchmark_out=results.json
```

## Architecture

JSOM uses a modern C++17 architecture:
- **`std::variant`** for type-safe JSON value storage
- **`LazyNumber`** class for deferred number parsing with format preservation
- **`FastParser`** with direct construction to eliminate allocation overhead
- **`PathCache`** with LRU eviction and prefix optimization
- **`JsonFormatter`** with intelligent layout algorithms

See `FORMATTING.md` for detailed documentation of the formatting system.

## Contributing

1. Ensure all tests pass: `cmake --build build --target run_tests`
2. Verify performance: `cmake --build build --target run_benchmarks`
3. Check code style: `cmake --build build --target format && cmake --build build --target tidy`
4. Add tests for new functionality
5. Update documentation as needed

## License

This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or distribute this software, either in source code form or as a compiled binary, for any purpose, commercial or non-commercial, and by any means.

See the `LICENSE` file for complete details.
