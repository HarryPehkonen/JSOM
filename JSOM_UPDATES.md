# JSOM Library Updates for Computo Integration

> **Status (June 2026): All proposals below have been implemented.** Mutable
> containers (`set`, `push_back`, `make_array`, `make_object`), `size()`/`empty()`,
> iteration (`begin`/`end`, `items()`, `keys()`), the full comparison operator set,
> `contains()`, implicit construction from primitives, and comment-tolerant parsing
> are all part of the library -- see README.md for usage. This document is kept as a
> historical record of the migration requirements.

This document proposes additions to the JSOM library that would enable replacing nlohmann/json in the Computo project. The proposals are ordered by priority based on the number of call sites in Computo that depend on the missing functionality.

## Context

Computo is a JSON-native data transformation engine. It parses JSON scripts, evaluates them, and produces JSON results. The JSON library serves as both the **input parser** and the **runtime value type** -- every intermediate and final result is a JSON value. JSOM already covers parsing, JSON Pointer navigation, and serialization well. The gaps are in **value construction**, **mutation**, **introspection**, and **comparison**.

---

## 1. Mutable Array and Object Operations (Critical -- ~90 call sites)

Computo builds result values incrementally. Every array operator (`map`, `filter`, `reduce`, `find`, `append`, `cons`, `cdr`, `zip`, `sort`, `reverse`, `unique`, `uniqueSorted`) and every object operator (`obj`, `keys`, `values`, `pick`, `omit`, `merge`, `objFromPairs`) constructs its result by starting with an empty container and adding elements one at a time.

### Needed API

```cpp
// Array mutation
void JsonDocument::push_back(JsonDocument value);
void JsonDocument::emplace_back(Args&&... args);  // optional but nice

// Object mutation
void JsonDocument::set(const std::string& key, JsonDocument value);
// or: JsonDocument& JsonDocument::operator[](const std::string& key);
//     with assignment support

// Factory functions for empty containers
static JsonDocument JsonDocument::make_array();
static JsonDocument JsonDocument::make_object();
```

### Current Computo Pattern

```cpp
// This pattern appears in nearly every operator implementation
auto result = nlohmann::json::array();
for (const auto& item : input_array) {
    auto transformed = evaluate(transform_expr, ctx);
    result.push_back(transformed);
}
return result;
```

```cpp
// Object construction
auto result = nlohmann::json::object();
for (size_t i = 0; i < args.size(); i += 2) {
    auto key = evaluate(args[i], ctx);
    auto value = evaluate(args[i + 1], ctx);
    result[key.get<std::string>()] = value;
}
return result;
```

### Notes

This is the single largest blocker. Without mutable containers, every operator would need to collect elements into `std::vector<JsonDocument>` and then construct the document from the vector at the end. That is workable but much less ergonomic and would require rewriting the core pattern used in ~15 operator files.

---

## 2. Size and Emptiness Queries (Critical -- ~135 call sites)

Computo checks `.size()` and `.empty()` constantly for argument validation, loop bounds, and as the implementation of the `count` operator.

### Needed API

```cpp
std::size_t JsonDocument::size() const;   // elements in array, keys in object, chars in string
bool JsonDocument::empty() const;         // size() == 0
```

### Current Computo Pattern

```cpp
// Argument count validation (every operator does this)
if (args.size() < 2) {
    throw InvalidArgumentException("operator requires at least 2 arguments");
}

// The count operator
auto op_count = [](const json& args, ExecutionContext& ctx) {
    auto array_data = extract_array_data(args[0], "count", path);
    return EvaluationResult(json(array_data.size()));
};

// Empty checks
if (array_data.empty()) {
    return EvaluationResult(initial_value);
}
```

---

## 3. Iteration Protocol (Critical -- ~50 call sites)

Computo iterates over arrays with range-for and over objects with key-value iteration.

### Needed API

```cpp
// Array iteration (range-for support)
auto JsonDocument::begin() const -> const_iterator;
auto JsonDocument::end() const -> const_iterator;

// Object key-value iteration
// Option A: items() returning iterable of pairs
auto JsonDocument::items() const -> ItemsRange;
// where each element yields .key() -> const std::string&
//                       and .value() -> const JsonDocument&

// Option B: keys() + operator[]
auto JsonDocument::keys() const -> std::vector<std::string>;
// combined with existing operator[] for access
```

### Current Computo Pattern

```cpp
// Array iteration (very common)
for (const auto& item : array_data) {
    auto result = evaluate_lambda(lambda, {item}, ctx);
    // ...
}

// Object iteration
for (const auto& [key, value] : obj.items()) {
    result.push_back(key);
}
```

### Notes

Array iteration is the more critical of the two. Object iteration appears in ~5 locations but array iteration is used in every array operator plus sort utilities and functional operators.

---

## 4. Equality and Comparison Operators (High -- ~40 call sites)

Computo's `==` and `!=` operators delegate to JSON value equality. The `sort` operator uses `<` and `>` for ordering. The entire test suite uses `EXPECT_EQ(result, expected)` which requires `operator==`.

### Needed API

```cpp
bool operator==(const JsonDocument& a, const JsonDocument& b);
bool operator!=(const JsonDocument& a, const JsonDocument& b);
bool operator<(const JsonDocument& a, const JsonDocument& b);
bool operator>(const JsonDocument& a, const JsonDocument& b);
bool operator<=(const JsonDocument& a, const JsonDocument& b);
bool operator>=(const JsonDocument& a, const JsonDocument& b);
```

### Semantics

Deep structural equality: two documents are equal if they have the same type and the same contents recursively. For ordering, a consistent total order is needed (e.g., null < bool < number < string < array < object, with values compared within type).

### Notes

Without `operator==`, Computo's 200+ test assertions would all need to be rewritten to serialize both sides and compare strings, which is fragile and loses type fidelity.

---

## 5. Object Key Membership Test (Medium -- ~10 call sites)

Computo checks whether an object contains a specific key.

### Needed API

```cpp
bool JsonDocument::contains(const std::string& key) const;
```

### Current Computo Pattern

```cpp
// Detecting array wrapper format
if (expr.is_object() && expr.size() == 1 && expr.contains(ctx.array_key)) {
    return expr[ctx.array_key];
}

// Checking result structure
EXPECT_TRUE(result.contains("array"));
```

### Notes

JSOM has `.exists()` but that takes a JSON Pointer path string (`"/key"`), not a bare key name. A `contains("key")` convenience method that checks for a direct child key of an object (without requiring the leading `/`) would match the common usage pattern.

---

## 6. Implicit Construction from Primitive Types (Medium -- ~30 call sites)

Computo constructs JSON values from C++ primitives frequently, often implicitly.

### Needed API

```cpp
// These constructors exist but implicit conversion is also useful:
JsonDocument(int value);
JsonDocument(double value);
JsonDocument(bool value);
JsonDocument(const char* value);
JsonDocument(const std::string& value);
JsonDocument(std::nullptr_t);

// Implicit conversion enables patterns like:
JsonDocument result = 42;          // from int
JsonDocument result = 3.14;        // from double
JsonDocument result = true;        // from bool
JsonDocument result = "hello";     // from string
JsonDocument result = nullptr;     // null
```

### Current Computo Pattern

```cpp
// nlohmann::json supports implicit construction everywhere
return EvaluationResult(json(array_data.size()));  // int → json
return EvaluationResult(json(true));               // bool → json
return EvaluationResult(json(nullptr));             // null → json

// In tests
EXPECT_EQ(result, json(10));
EXPECT_EQ(result, json("hello"));
EXPECT_EQ(result, json(true));
```

### Notes

JSOM already has explicit constructors for these types. The question is whether implicit conversion is supported or whether every construction site needs an explicit `JsonDocument(...)` wrapper. Explicit construction is fine -- it just means more call sites to update.

---

## 7. JSONC / Comment Support in Parsing (Low -- 2 call sites)

Computo supports a `--comments` flag that enables JSON files with `//` and `/* */` comments.

### Needed API

```cpp
// A parse option to ignore comments
JsonParseOptions options;
options.allow_comments = true;  // or similar
auto doc = parse_document(content, options);
```

### Current Computo Pattern

```cpp
// In repl.cpp
nlohmann::json::parse(content, nullptr, allow_exceptions, ignore_comments);
```

### Notes

This is low priority. Only two call sites use it (REPL and CLI file loading). If JSOM doesn't support comments, Computo could strip comments with a preprocessing step before parsing.

---

## 8. Stream Output (Low -- ~9 call sites)

Computo uses `.dump()` for serialization with optional indentation.

### Current Status

JSOM already has `.to_json()` with format presets. This is covered. The only note is that Computo uses `.dump()` with no arguments for compact output and `.dump(2)` for indented output. The mapping would be:

```cpp
// nlohmann                          // JSOM equivalent
value.dump()                         value.to_json(FormatPresets::Compact)
value.dump(2)                        value.to_json(FormatPresets::Pretty)
```

This is already supported. No changes needed.

---

## Summary

| Priority | Feature | Computo Call Sites | Effort Estimate |
|----------|---------|-------------------|-----------------|
| **Critical** | Mutable arrays/objects (`push_back`, `set`, `make_array`, `make_object`) | ~90 | Medium-high |
| **Critical** | `size()` and `empty()` | ~135 | Low |
| **Critical** | Iteration (range-for arrays, key-value objects) | ~50 | Medium |
| **High** | Equality and comparison operators | ~40 | Medium |
| **Medium** | `contains(key)` for objects | ~10 | Low |
| **Medium** | Implicit/explicit construction from primitives | ~30 | Low (may already work) |
| **Low** | Comment-tolerant parsing | 2 | Low |
| **None** | Serialization with formatting | ~9 | Already supported |

The three critical items (mutable containers, size/empty, iteration) account for ~275 call sites and would unblock the vast majority of the migration. Adding equality operators unblocks the test suite. The remaining items are smaller conveniences.
