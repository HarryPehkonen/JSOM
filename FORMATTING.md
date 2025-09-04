# JSOM JSON Formatting Guide

JSOM provides an advanced JSON serialization system with intelligent formatting controls that go far beyond simple "compact" and "pretty" printing. This document provides a comprehensive guide to all available formatting options with detailed examples.

## Overview

JSOM's formatting system centers around the `JsonFormatOptions` structure, which provides fine-grained control over JSON output formatting. The library includes smart inlining logic that automatically determines when arrays and objects should stay on single lines versus being formatted across multiple lines.

## Core Formatting Options

### Basic Controls

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `pretty` | bool | false | Enable pretty printing with indentation and spacing |
| `indent_size` | int | 2 | Number of spaces per indentation level |
| `sort_keys` | bool | false | Sort object keys alphabetically |

### Smart Inlining Controls

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `max_inline_array_size` | int | 10 | Arrays with ≤ N elements stay on one line |
| `max_inline_object_size` | int | 3 | Objects with ≤ N properties stay on one line |
| `max_inline_string_length` | int | 40 | Strings ≤ length don't break structure |

### Advanced Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `quote_keys` | bool | true | Quote object keys (JSON standard) |
| `trailing_comma` | bool | false | Add trailing commas (non-standard) |
| `escape_unicode` | bool | false | Escape non-ASCII characters as \uXXXX |
| `max_depth` | int | 100 | Maximum nesting depth allowed |

## Predefined Format Presets

JSOM includes five carefully designed presets for common use cases:

### 1. Compact
```cpp
jsom::FormatPresets::Compact
```
- **Use case**: Minimal bandwidth, storage efficiency
- **Characteristics**: No whitespace, everything on one line
- **Settings**: `pretty = false`

### 2. Pretty
```cpp
jsom::FormatPresets::Pretty
```
- **Use case**: General-purpose readable formatting
- **Characteristics**: Smart inlining with balanced readability
- **Settings**: `pretty = true`, `indent_size = 2`, `max_inline_array_size = 10`, `max_inline_object_size = 3`

### 3. Config
```cpp
jsom::FormatPresets::Config
```
- **Use case**: Configuration files, settings
- **Characteristics**: Conservative inlining, sorted keys
- **Settings**: `pretty = true`, `sort_keys = true`, `max_inline_array_size = 5`, `max_inline_object_size = 1`

### 4. Api
```cpp
jsom::FormatPresets::Api
```
- **Use case**: API responses, data interchange
- **Characteristics**: Balanced compactness and readability
- **Settings**: `pretty = true`, `max_inline_array_size = 20`, `max_inline_object_size = 5`

### 5. Debug
```cpp
jsom::FormatPresets::Debug
```
- **Use case**: Debugging, development, maximum readability
- **Characteristics**: Every element on separate line, unicode escaping
- **Settings**: `pretty = true`, `indent_size = 4`, `sort_keys = true`, `max_inline_array_size = 1`, `max_inline_object_size = 0`, `escape_unicode = true`

## Formatting Examples

Let's use this sample JSON data for all examples:

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

### Compact Format

```cpp
doc.to_json(jsom::FormatPresets::Compact)
```

**Output:**
```json
{"metadata":{"created":"2024-01-15","updated":"2024-01-20"},"profile":{"age":30,"email":"john@example.com","name":"John Doe"},"scores":[95,87,92,88,91,89,94,86,93,90,85,96],"settings":{"notifications":true,"theme":"dark"},"tags":["developer","javascript","json"],"user":"john_doe"}
```

### Pretty Format (Smart Inlining)

```cpp
doc.to_json(jsom::FormatPresets::Pretty)
```

**Output:**
```json
{
  "metadata": {"created": "2024-01-15", "updated": "2024-01-20"},
  "profile": {
    "age": 30,
    "email": "john@example.com",
    "name": "John Doe"
  },
  "scores": [95, 87, 92, 88, 91, 89, 94, 86, 93, 90, 85, 96],
  "settings": {"notifications": true, "theme": "dark"},
  "tags": ["developer", "javascript", "json"],
  "user": "john_doe"
}
```

**Note:** Small objects like `metadata` and `settings` stay inline because they have ≤ 3 properties. The `scores` array stays inline because it has ≤ 10 elements. The `profile` object becomes multiline because it exceeds the 3-property limit.

### Config Format (Conservative)

```cpp
doc.to_json(jsom::FormatPresets::Config)
```

**Output:**
```json
{
  "metadata": {
    "created": "2024-01-15",
    "updated": "2024-01-20"
  },
  "profile": {
    "age": 30,
    "email": "john@example.com",
    "name": "John Doe"
  },
  "scores": [95, 87, 92, 88, 91],
  "settings": {
    "notifications": true,
    "theme": "dark"
  },
  "tags": ["developer", "javascript", "json"],
  "user": "john_doe"
}
```

**Note:** More conservative inlining - only objects with 1 property stay inline, and arrays with ≤ 5 elements. Keys are sorted alphabetically.

### API Format (Balanced)

```cpp
doc.to_json(jsom::FormatPresets::Api)
```

**Output:**
```json
{
  "metadata": {"created": "2024-01-15", "updated": "2024-01-20"},
  "profile": {"age": 30, "email": "john@example.com", "name": "John Doe"},
  "scores": [95, 87, 92, 88, 91, 89, 94, 86, 93, 90, 85, 96],
  "settings": {"notifications": true, "theme": "dark"},
  "tags": ["developer", "javascript", "json"],
  "user": "john_doe"
}
```

**Note:** More aggressive inlining - objects with ≤ 5 properties and arrays with ≤ 20 elements stay inline, optimizing for API response compactness while maintaining readability.

### Debug Format (Maximum Readability)

```cpp
doc.to_json(jsom::FormatPresets::Debug)
```

**Output:**
```json
{
    "metadata": {
        "created": "2024-01-15",
        "updated": "2024-01-20"
    },
    "profile": {
        "age": 30,
        "email": "john@example.com",
        "name": "John Doe"
    },
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
    "settings": {
        "notifications": true,
        "theme": "dark"
    },
    "tags": [
        "developer",
        "javascript",
        "json"
    ],
    "user": "john_doe"
}
```

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
```json
{
        "metadata": {"created": "2024-01-15", "updated": "2024-01-20"},
        "profile": {
                "age": 30,
                "email": "john@example.com",
                "name": "John Doe"
        },
        "scores": [
                95, 87, 92,
                88, 91, 89,
                94, 86, 93,
                90, 85, 96
        ],
        "settings": {"notifications": true, "theme": "dark"},
        "tags": ["developer", "javascript", "json"],
        "user": "john_doe"
}
```

### Example 2: Ultra-Compact Arrays

```cpp
jsom::JsonFormatOptions custom;
custom.pretty = true;
custom.max_inline_array_size = 50;  // Keep large arrays inline
custom.max_inline_object_size = 1;   // Break objects early

doc.to_json(custom)
```

**Output:**
```json
{
  "metadata": {
    "created": "2024-01-15",
    "updated": "2024-01-20"
  },
  "profile": {
    "age": 30,
    "email": "john@example.com",
    "name": "John Doe"
  },
  "scores": [95, 87, 92, 88, 91, 89, 94, 86, 93, 90, 85, 96],
  "settings": {
    "notifications": true,
    "theme": "dark"
  },
  "tags": ["developer", "javascript", "json"],
  "user": "john_doe"
}
```

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
```json
{
    "metadata": {
        "created": "2024-01-15",
        "updated": "2024-01-20"
    },
    "profile": {
        "age": 30,
        "email": "john@example.com",
        "name": "John Doe"
    },
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
    "settings": {
        "notifications": true,
        "theme": "dark"
    },
    "tags": [
        "developer",
        "javascript",
        "json"
    ],
    "user": "john_doe"
}
```

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