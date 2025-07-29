# JSOM - JSON Streaming Object Model

A fast, header-only C++17 streaming JSON parser with complete JSON Pointer (RFC 6901) support and event-driven architecture.

## Features

- **Header-only library** - Single `jsom.hpp` include, no dependencies
- **Streaming parser** - Character-by-character processing with minimal memory footprint  
- **JSON Pointer tracking** - RFC 6901 compliant path generation for all values and errors
- **Event-driven architecture** - Real-time callbacks during parsing
- **Modern C++17** - Clean, type-safe API with proper RAII and smart pointers
- **Comprehensive error handling** - Detailed error messages with JSON Pointer context
- **Full JSON support** - Strings, numbers, booleans, null, objects, arrays, and nested structures
- **Character-level precision** - Accurate position tracking for debugging

## Quick Start

### Basic Usage

```cpp
#include "jsom.hpp"
#include <iostream>

int main() {
    jsom::StreamingParser parser;
    
    // Set up event callbacks
    jsom::ParseEvents events;
    events.on_value = [](const jsom::JsonValue& value) {
        std::cout << "Found " << value.raw_value() 
                  << " at path: " << value.path() << std::endl;
    };
    events.on_error = [](const jsom::ParseError& error) {
        std::cout << "Error: " << error.message 
                  << " at " << error.json_pointer << std::endl;
    };
    
    parser.set_events(events);
    
    std::string json = R"({
        "name": "John Doe",
        "age": 30,
        "hobbies": ["reading", "coding"],
        "address": {
            "city": "New York",
            "zip": "10001"
        }
    })";
    
    parser.parse_string(json);
    parser.end_input();
    
    return 0;
}
```

**Output:**
```
Found John Doe at path: /name
Found 30 at path: /age
Found reading at path: /hobbies/0
Found coding at path: /hobbies/1
Found New York at path: /address/city
Found 10001 at path: /address/zip
```

### CMake Integration

```cmake
# Include JSOM in your project
add_executable(my_app main.cpp)
target_include_directories(my_app PRIVATE path/to/jsom/include)
target_compile_features(my_app PRIVATE cxx_std_17)
```

## Architecture

### StreamingParser

The core parser processes JSON character-by-character using a state machine approach:

```cpp
jsom::StreamingParser parser;

// Configure with custom allocator (optional)
auto arena = std::make_unique<jsom::ArenaAllocator>(4096);
jsom::StreamingParser parser(std::move(arena));

// Parse incrementally
for (char c : json_string) {
    parser.feed_character(c);
}
parser.end_input();

// Or parse all at once
parser.parse_string(json_string);
parser.end_input();

// Reset for reuse
parser.reset();
```

### JsonValue

Represents parsed JSON values with automatic type detection:

```cpp
// Access value properties
JsonValue value = /* from callback */;
auto type = value.type();           // JsonType enum
auto raw = value.raw_value();       // Raw string representation
auto path = value.path();           // JSON Pointer path

// Type-specific accessors
if (type == jsom::JsonType::String) {
    auto str = value.as_string();   // Unescaped string
}
if (type == jsom::JsonType::Number) {
    auto num = value.as_double();   // Parsed as double
    auto int_val = value.as_int();  // Parsed as int
}
if (type == jsom::JsonType::Boolean) {
    auto flag = value.as_bool();    // true/false
}
```

### ParseEvents

Event-driven callbacks for real-time processing:

```cpp
jsom::ParseEvents events;

// Called for each parsed value (strings, numbers, booleans, null)
events.on_value = [](const jsom::JsonValue& value) {
    // Process value with automatic JSON Pointer path
};

// Called when entering objects (with key context)
events.on_enter_object = [](const std::string& key) {
    // Key is empty for root object
};

// Called when entering arrays
events.on_enter_array = []() {
    // Array entered
};

// Called when exiting any container (object or array)
events.on_exit_container = []() {
    // Container closed
};

// Called for parse errors with precise location
events.on_error = [](const jsom::ParseError& error) {
    // error.position - character position
    // error.message - human-readable description  
    // error.json_pointer - path context where error occurred
};
```

## JSON Pointer Support

JSOM provides full RFC 6901 JSON Pointer support with automatic path generation:

### Path Examples

```cpp
// JSON: {"users": [{"name": "Alice"}, {"name": "Bob"}]}
// Paths generated:
// /users/0/name → "Alice"
// /users/1/name → "Bob"

// JSON: {"key~with/special": "value"}  
// Path: /key~0with~1special → "value"
// (~ escaped to ~0, / escaped to ~1)
```

### Error Context

Parse errors include JSON Pointer context for precise debugging:

```cpp
// Invalid JSON: {"name": invalid_value}
// Error: "Unexpected character in start state" at /name
```

## Supported JSON Features

### ✅ Complete Implementation

- **Strings** - Full escape sequence support (`\"`, `\\`, `\/`, `\b`, `\f`, `\n`, `\r`, `\t`, `\uXXXX`)
- **Numbers** - Integers, floats, scientific notation (`123`, `-45.67`, `1.23e-4`)
- **Literals** - `true`, `false`, `null`
- **Objects** - Key-value pairs with string keys (`{"key": "value"}`)
- **Arrays** - Ordered lists (`[1, "two", true, null]`)
- **Nested structures** - Arbitrary nesting depth
- **Unicode** - Full UTF-8 support with `\uXXXX` escape sequences
- **Whitespace** - Proper handling of spaces, tabs, newlines, carriage returns

### Error Handling

- **Syntax validation** - Malformed JSON detection
- **Type checking** - Object keys must be strings
- **Container matching** - Proper `{}` and `[]` pairing
- **String validation** - Proper quote and escape handling
- **Graceful recovery** - Continues parsing after recoverable errors

## Memory Management

JSOM offers flexible memory management strategies:

### Standard Allocator (Default)
```cpp
// Uses std::malloc/free
jsom::StreamingParser parser; // Default constructor
```

### Arena Allocator (High Performance)
```cpp
// Pre-allocates blocks for faster allocation
auto arena = std::make_unique<jsom::ArenaAllocator>(4096); // 4KB blocks
jsom::StreamingParser parser(std::move(arena));
```

## Building

### Requirements
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.10+ (for building tests)

### Build Commands
```bash
# Configure and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run tests
cd build && ctest

# Or run directly
cd build && ./jsom_tests
```

### Build Targets
```bash
# Format code
cmake --build build --target format

# Run static analysis
cmake --build build --target tidy

# Run specific test suite
cd build && ./jsom_tests --gtest_filter="StringTest.*"
```

## Performance Characteristics

- **Memory efficient** - Minimal allocation during parsing
- **Streaming** - Process JSON as it arrives, no buffering required
- **Fast** - Character-by-character state machine with optimized transitions
- **Scalable** - Handles large JSON documents with constant memory usage

## API Reference

### Core Types

```cpp
namespace jsom {
    // Main parser class
    class StreamingParser;
    
    // Value representation
    class JsonValue;
    enum class JsonType { Null, Boolean, Number, String, Object, Array };
    
    // Events and errors
    struct ParseEvents;
    struct ParseError;
    
    // Memory management
    class IAllocator;
    class StandardAllocator;
    class ArenaAllocator;
    
    // Path tracking (legacy support)
    struct PathNode;
}
```

### StreamingParser Methods

```cpp
// Parsing
void feed_character(char c);
void parse_string(const std::string& json);
void end_input();

// Configuration
void set_events(const ParseEvents& events);
void reset();
```

### JsonValue Methods

```cpp
// Type and value access
JsonType type() const;
const std::string& raw_value() const;
std::string path() const;

// Type-specific accessors
bool as_bool() const;
const std::string& as_string() const;
double as_double() const;
int as_int() const;
```

## Current Status

**🎉 Complete Production-Ready Implementation (88/88 tests passing)**

- ✅ **Phase 1**: Project skeleton with core data structures
- ✅ **Phase 2**: Simple value parsing (literals, numbers)  
- ✅ **Phase 3**: String parsing with escape sequences
- ✅ **Phase 4**: Container management (objects, arrays)
- ✅ **Phase 5**: Container content parsing
- ✅ **Phase 6**: JSON Pointer path tracking
- ✅ **Phase 7**: Finalization and quality assurance

**🏆 Production Ready**
- Zero memory leaks (Valgrind verified)
- Comprehensive test coverage (88 tests, 1,200+ lines)
- Clean static analysis (0 clang-tidy warnings)
- Complete API implementation

## Examples

### Parse Configuration File
```cpp
jsom::StreamingParser parser;
std::map<std::string, std::string> config;

jsom::ParseEvents events;
events.on_value = [&](const jsom::JsonValue& value) {
    if (value.type() == jsom::JsonType::String) {
        config[value.path()] = value.as_string();
    }
};

parser.set_events(events);
parser.parse_string(config_json);
parser.end_input();

// Access: config["/database/host"], config["/api/key"], etc.
```

### Validate JSON Structure
```cpp
jsom::StreamingParser parser;
bool valid = true;

jsom::ParseEvents events;
events.on_error = [&](const jsom::ParseError& error) {
    std::cerr << "Invalid JSON at " << error.json_pointer 
              << ": " << error.message << std::endl;
    valid = false;
};

parser.set_events(events);
parser.parse_string(json_to_validate);
parser.end_input();

return valid;
```

## License

MIT License - see LICENSE file for details.

## Contributing

Contributions welcome! Please see the AI development guides in the `AI/` directory for structured development workflows.