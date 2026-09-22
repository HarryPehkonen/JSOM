# JSOM JSON Formatting Guide

JSOM provides an advanced JSON serialization system with intelligent formatting controls that go far beyond simple "compact" and "pretty" printing. This document provides a comprehensive guide to all available formatting options with detailed examples.

## Overview

JSOM's formatting system centers around the `JsonFormatOptions` structure, which provides fine-grained control over JSON output formatting. The library includes smart inlining logic that automatically determines when arrays and objects should stay on single lines versus being formatted across multiple lines.

## Core Formatting Options

Every option lives in `JsonFormatOptions`. The defaults below are generated from the
struct's initialisers, so they cannot drift from the code (`tools/check_docs.py` fails if
they do).

<!-- BEGIN GENERATED: options — tools/check_docs.py --write -->
| option | type | default |
|---|---|---|
| `indent_size` | std::optional<int> | 2 |
| `sort_keys` | bool | off |
| `max_inline_array_size` | int | 10 |
| `max_inline_object_size` | int | 3 |
| `max_inline_string_length` | int | 40 |
| `max_line_width` | int | 120 |
| `align_values` | bool | off |
| `colon_spacing` | int | 1 |
| `bracket_spacing` | bool | off |
| `quote_keys` | bool | on |
| `trailing_comma` | bool | off |
| `escape_unicode` | bool | off |
| `intelligent_wrapping` | bool | off |
| `max_depth` | int | 256 |
<!-- END GENERATED: options -->

What each option does:

- `indent_size` — spaces per level; `none` writes everything on one line
- `sort_keys` — sort object keys alphabetically
- `max_inline_array_size`, `max_inline_object_size`, `max_inline_string_length` — how small a
  container (and its strings) must be to stay on one line. A container that HOLDS a container
  goes multiline regardless, and the inner container then follows its own rule
- `max_line_width` — characters per line before wrapping; 0 disables the limit
- `align_values` — pad keys so the values line up in a column (multiline objects)
- `colon_spacing` — spaces around `:` (0, 1 or 2)
- `bracket_spacing` — padding inside `[ ]` and `{ }`
- `quote_keys` — quote object keys; `false` emits non-standard JSON
- `trailing_comma` — trailing commas in multiline output; non-standard JSON
- `escape_unicode` — write non-ASCII as `\uXXXX`, escaping the CODEPOINT (an astral character
  becomes a surrogate pair). Text that is not valid UTF-8 is passed through byte for byte: no
  `\uXXXX` decodes back to an invalid byte, so escaping it would change the value. Control
  characters are escaped whatever this says, because a raw control character is not valid JSON
- `intelligent_wrapping` — pack multiple simple values per line instead of one per line
- `max_depth` — nesting accepted while formatting; deeper input is rejected with
  "Maximum formatting depth exceeded"

## Predefined Format Presets

JSOM includes five carefully designed presets for common use cases:

### 1. Compact
```cpp
jsom::FormatPresets::Compact
```
- **Use case**: Minimal bandwidth, storage efficiency
- **Characteristics**: No whitespace, everything on one line

### 2. Pretty
```cpp
jsom::FormatPresets::Pretty
```
- **Use case**: General-purpose readable formatting
- **Characteristics**: Smart inlining with balanced readability

### 3. Config
```cpp
jsom::FormatPresets::Config
```
- **Use case**: Configuration files, settings
- **Characteristics**: Conservative inlining, sorted keys

### 4. Api
```cpp
jsom::FormatPresets::Api
```
- **Use case**: API responses, data interchange
- **Characteristics**: Balanced compactness and readability

### 5. Debug
```cpp
jsom::FormatPresets::Debug
```
- **Use case**: Debugging, development, maximum readability
- **Characteristics**: Every element on separate line, unicode escaping

The numbers behind those presets come from `FormatPresets` itself:

<!-- BEGIN GENERATED: presets — tools/check_docs.py --write -->
| preset | indent | inline arrays | inline objects | line width | sort keys | align values | colon spacing | bracket spacing | escape unicode | intelligent wrap |
|---|---|---|---|---|---|---|---|---|---|---|
| Compact | none | 10 | 3 | 0 (no limit) | off | off | 1 | off | off | off |
| Pretty | 2 | 8 | 3 | 100 | off | on | 1 | off | off | on |
| Config | 2 | 5 | 1 | 100 | on | on | 1 | off | off | off |
| Api | 2 | 15 | 4 | 120 | off | off | 1 | on | off | on |
| Debug | 4 | 1 | 0 | 80 | on | on | 1 | on | on | off |
<!-- END GENERATED: presets -->

## Formatting Examples

Let's use this sample JSON data for all examples:

<!-- BEGIN GENERATED: sample — tools/check_docs.py --write -->
```json
{
  "user": "john_doe",
  "profile": {
    "name": "John Doe",
    "age": 30,
    "email": "john@example.com"
  },
  "settings": {
    "theme": "dark",
    "notifications": true
  },
  "tags": ["developer", "javascript", "json"],
  "scores": [95, 87, 92, 88, 91, 89, 94, 86, 93, 90, 85, 96],
  "metadata": {
    "created": "2024-01-15",
    "updated": "2024-01-20"
  }
}
```
<!-- END GENERATED: sample -->

### Compact Format

```cpp
doc.to_json(jsom::FormatPresets::Compact)
```

**Output:**
<!-- BEGIN GENERATED: example-compact — tools/check_docs.py --write -->
```json
{"metadata": {"created": "2024-01-15", "updated": "2024-01-20"}, "profile": {"age": 30, "email": "john@example.com", "name": "John Doe"}, "scores": [95, 87, 92, 88, 91, 89, 94, 86, 93, 90, 85, 96], "settings": {"notifications": true, "theme": "dark"}, "tags": ["developer", "javascript", "json"], "user": "john_doe"}
```
<!-- END GENERATED: example-compact -->

### Pretty Format (Smart Inlining)

```cpp
doc.to_json(jsom::FormatPresets::Pretty)
```

**Output:**
<!-- BEGIN GENERATED: example-pretty — tools/check_docs.py --write -->
```json
{
  "metadata": {"created": "2024-01-15", "updated": "2024-01-20"},
  "profile" : {"age": 30, "email": "john@example.com", "name": "John Doe"},
  "scores"  : [
    95, 87, 92, 88, 91, 89, 94, 86, 93, 90, 85, 96
  ],
  "settings": {"notifications": true, "theme": "dark"},
  "tags"    : ["developer", "javascript", "json"],
  "user"    : "john_doe"
}
```
<!-- END GENERATED: example-pretty -->

**Note:** the small objects (`metadata`, `settings`, `profile`) stay inline while the twelve-element
`scores` array wraps across lines. The per-preset inline limits are in the table above — this
page does not restate them, so they cannot disagree with the code.

### Config Format (Conservative)

```cpp
doc.to_json(jsom::FormatPresets::Config)
```

**Output:**
<!-- BEGIN GENERATED: example-config — tools/check_docs.py --write -->
```json
{
  "metadata": {
    "created": "2024-01-15",
    "updated": "2024-01-20"
  },
  "profile" : {
    "age"  : 30,
    "email": "john@example.com",
    "name" : "John Doe"
  },
  "scores"  : [
    95,
    87,
    92,
    88,
    91,
    89,
    94,
    86,
    93,
    90,
    85,
    96
  ],
  "settings": {
    "notifications": true,
    "theme"        : "dark"
  },
  "tags"    : ["developer", "javascript", "json"],
  "user"    : "john_doe"
}
```
<!-- END GENERATED: example-config -->

**Note:** a smaller inline budget than Pretty for both arrays and objects — conservative inlining —
with keys sorted alphabetically.

### API Format (Balanced)

```cpp
doc.to_json(jsom::FormatPresets::Api)
```

**Output:**
<!-- BEGIN GENERATED: example-api — tools/check_docs.py --write -->
```json
{
  "metadata": { "created": "2024-01-15", "updated": "2024-01-20" },
  "profile": { "age": 30, "email": "john@example.com", "name": "John Doe" },
  "scores": [ 95, 87, 92, 88, 91, 89, 94, 86, 93, 90, 85, 96 ],
  "settings": { "notifications": true, "theme": "dark" },
  "tags": [ "developer", "javascript", "json" ],
  "user": "john_doe"
}
```
<!-- END GENERATED: example-api -->

**Note:** a larger inline budget than Pretty, so more arrays and objects stay on one line — aimed at
API responses, where compactness and readability are both wanted.

### Debug Format (Maximum Readability)

```cpp
doc.to_json(jsom::FormatPresets::Debug)
```

**Output:**
<!-- BEGIN GENERATED: example-debug — tools/check_docs.py --write -->
```json
{
    "metadata": {
        "created": "2024-01-15",
        "updated": "2024-01-20"
    },
    "profile" : {
        "age"  : 30,
        "email": "john@example.com",
        "name" : "John Doe"
    },
    "scores"  : [
        95,
        87,
        92,
        88,
        91,
        89,
        94,
        86,
        93,
        90,
        85,
        96
    ],
    "settings": {
        "notifications": true,
        "theme"        : "dark"
    },
    "tags"    : [
        "developer",
        "javascript",
        "json"
    ],
    "user"    : "john_doe"
}
```
<!-- END GENERATED: example-debug -->

**Note:** 4-space indentation, every array element and object property on separate lines, keys sorted alphabetically.

## Custom Formatting Examples

### Example 1: Custom Indentation

```cpp
jsom::JsonFormatOptions custom;
custom.pretty = true;
custom.indent_size = 8;  // 8-space indentation
custom.max_inline_object_size = 2;
custom.max_inline_array_size = 3;

doc.to_json(custom)
```

**Output:**
<!-- BEGIN GENERATED: example-indent — tools/check_docs.py --write -->
```json
{
    "metadata": {"created": "2024-01-15", "updated": "2024-01-20"},
    "profile": {"age": 30, "email": "john@example.com", "name": "John Doe"},
    "scores": [
        95,
        87,
        92,
        88,
        91,
        89,
        94,
        86,
        93,
        90,
        85,
        96
    ],
    "settings": {"notifications": true, "theme": "dark"},
    "tags": ["developer", "javascript", "json"],
    "user": "john_doe"
}
```
<!-- END GENERATED: example-indent -->

### Example 2: Ultra-Compact Arrays

```cpp
jsom::JsonFormatOptions custom;
custom.pretty = true;
custom.max_inline_array_size = 50;  // Keep large arrays inline
custom.max_inline_object_size = 1;   // Break objects early

doc.to_json(custom)
```

**Output:**
<!-- BEGIN GENERATED: example-ultra-compact — tools/check_docs.py --write -->
```json
{
  "metadata": {
    "created": "2024-01-15",
    "updated": "2024-01-20"
  },
  "profile" : {
    "age"  : 30,
    "email": "john@example.com",
    "name" : "John Doe"
  },
  "scores"  : [95, 87, 92, 88, 91, 89, 94, 86, 93, 90, 85, 96],
  "settings": {
    "notifications": true,
    "theme"        : "dark"
  },
  "tags"    : ["developer", "javascript", "json"],
  "user"    : "john_doe"
}
```
<!-- END GENERATED: example-ultra-compact -->

### Example 3: Extreme Verbosity

```cpp
jsom::JsonFormatOptions custom;
custom.pretty = true;
custom.indent_size = 4;
custom.max_inline_array_size = 0;   // No inline arrays
custom.max_inline_object_size = 0;  // No inline objects
custom.sort_keys = true;

doc.to_json(custom)
```

**Output:**
<!-- BEGIN GENERATED: example-verbose — tools/check_docs.py --write -->
```json
{
  "metadata": {
    "created": "2024-01-15",
    "updated": "2024-01-20"
  },
  "profile" : {
    "age"  : 30,
    "email": "john@example.com",
    "name" : "John Doe"
  },
  "scores"  : [
    95, 87, 92, 88, 91, 89, 94, 86, 93, 90, 85, 96
  ],
  "settings": {
    "notifications": true,
    "theme"        : "dark"
  },
  "tags"    : [
    "developer", "javascript", "json"
  ],
  "user"    : "john_doe"
}
```
<!-- END GENERATED: example-verbose -->

## Smart Inlining Logic

JSOM's formatter uses intelligent rules to determine when containers should be inlined:

### Array Inlining Rules

1. **Size Check**: Arrays with more than `max_inline_array_size` elements become multiline
2. **Content Check**: Arrays containing nested objects or arrays become multiline regardless of size
3. **Empty Arrays**: Always inline (`[]`)
4. **Simple Arrays**: Arrays with only primitive values (numbers, strings, booleans, null) can be inlined if under size limit

### Object Inlining Rules

1. **Size Check**: Objects with more than `max_inline_object_size` properties become multiline
2. **Content Check**: Objects containing nested containers become multiline regardless of size  
3. **Empty Objects**: Always inline (`{}`)
4. **Simple Objects**: Objects with only primitive values can be inlined if under size limit

### Example: Nested Container Behavior

```cpp
// This array contains objects, so it becomes multiline even with max_inline_array_size = 10
auto nested = jsom::JsonDocument{
    jsom::JsonDocument{{"id", jsom::JsonDocument(1)}, {"name", jsom::JsonDocument("item1")}},
    jsom::JsonDocument{{"id", jsom::JsonDocument(2)}, {"name", jsom::JsonDocument("item2")}}
};

nested.to_json(jsom::FormatPresets::Pretty)
```

**Output:**
```json
[
  {"id": 1, "name": "item1"},
  {"id": 2, "name": "item2"}
]
```

## CLI Usage

The JSOM command-line tool provides access to all formatting options:

### Using Presets
```bash
# Compact formatting
jsom format --preset=compact data.json

# Pretty formatting  
jsom format --preset=pretty data.json

# Configuration file formatting
jsom format --preset=config data.json

# API response formatting
jsom format --preset=api data.json

# Debug formatting
jsom format --preset=debug data.json
```

### Custom Options
```bash
# Custom indentation
jsom format --indent=4 --preset=pretty data.json

# Custom inlining limits
jsom format --inline-arrays=5 --inline-objects=2 --preset=pretty data.json

# Combining options
jsom format --preset=pretty --indent=8 --inline-arrays=20 data.json
```

### Processing Multiple Files
```bash
# Format all JSON files in directory
jsom format --preset=pretty *.json

# Process stdin
echo '{"test": [1,2,3]}' | jsom format --preset=debug
```

## Programming Interface

### Basic Usage

```cpp
#include "jsom.hpp"

// Parse JSON
auto doc = jsom::parse_document(json_string);

// Use preset
std::string formatted = doc.to_json(jsom::FormatPresets::Pretty);

// Use custom options
jsom::JsonFormatOptions opts;
opts.pretty = true;
opts.indent_size = 4;
opts.max_inline_array_size = 5;
std::string custom_formatted = doc.to_json(opts);
```

### Method Overloads

```cpp
// Default (compact)
std::string compact = doc.to_json();

// Bool parameter (pretty vs compact)
std::string pretty = doc.to_json(true);
std::string compact2 = doc.to_json(false);

// Full options control
std::string custom = doc.to_json(my_options);
```

## Best Practices

### For Configuration Files
- Use `FormatPresets::Config` or similar conservative settings
- Enable `sort_keys` for consistent ordering
- Use smaller inline limits for better readability
- Consider 4-space indentation for better visibility

### For API Responses
- Use `FormatPresets::Api` or `FormatPresets::Pretty`
- Balance readability with bandwidth considerations
- Higher inline limits reduce line count while maintaining structure

### For Debugging
- Use `FormatPresets::Debug` for maximum clarity
- Every element on its own line makes diff tools more effective
- Enable `escape_unicode` if dealing with international text

### For Storage/Transmission
- Use `FormatPresets::Compact` for minimal size
- No whitespace reduces bandwidth and storage requirements
- Consider gzip compression for additional savings

## Performance Considerations

- **Compact formatting** is fastest (minimal processing)
- **Smart inlining** adds computational overhead but improves readability
- **Sorting keys** has performance impact on large objects
- **Unicode escaping** is most expensive option

The intelligent formatting system provides excellent balance between human readability and machine efficiency, making JSOM suitable for both development and production use cases.