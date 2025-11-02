#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace jsom {

// CLI Constants
namespace cli_constants {

// Display formatting
constexpr int SEPARATOR_LINE_WIDTH = 60;        // Width of separator lines (===, ---)
constexpr int BENCHMARK_PATH_COLUMN_WIDTH = 40; // Path column width in benchmark output
constexpr int BENCHMARK_TIME_COLUMN_WIDTH = 10; // Time column width in benchmark output
constexpr int BENCHMARK_PRECISION = 1;          // Decimal places for benchmark timing

// Command argument positions
constexpr int COMMAND_ARG_INDEX = 1;        // argv[1] is the command
constexpr int FIRST_OPTION_INDEX = 2;       // argv[2] is first option
constexpr int MINIMUM_VALIDATE_ARGS = 3;    // Minimum args for validate command
constexpr int MINIMUM_POINTER_ARGS = 3;     // Minimum args for pointer command
constexpr int POINTER_SUBCOMMAND_INDEX = 2; // Index of pointer subcommand
constexpr int POINTER_PATH_INDEX = 3;       // Index of pointer path argument
constexpr int POINTER_VALUE_INDEX = 4;      // Index of pointer value argument (for set)
constexpr int MINIMUM_ARGC = 2;             // Minimum argc for any valid command
constexpr int MINIMUM_GET_ARGS = 4;         // Minimum args for pointer get
constexpr int MINIMUM_SET_ARGS = 5;         // Minimum args for pointer set

// Benchmark constants
constexpr int BENCHMARK_ITERATIONS = 1000;   // Number of iterations for benchmarking
constexpr double BENCHMARK_DIVISOR = 1000.0; // Divisor to get average time

// Colon spacing limits
constexpr int MIN_COLON_SPACING = 0;
constexpr int MAX_COLON_SPACING = 2;

// Examples for help text
constexpr int EXAMPLE_MAX_DEPTH = 3;     // Example max-depth value
constexpr int EXAMPLE_INDENT = 4;        // Example indent value
constexpr int EXAMPLE_INLINE_ARRAYS = 5; // Example inline-arrays value
constexpr int EXAMPLE_MAX_WIDTH = 80;    // Example max-width value

// Error codes
constexpr int ERROR_CODE_GENERAL = 1;
constexpr int ERROR_CODE_PATH_NOT_FOUND = 2;
} // namespace cli_constants

// JSON Format Option Defaults
namespace format_defaults {
constexpr int DEFAULT_INDENT_SIZE = 2;
constexpr int DEFAULT_MAX_INLINE_ARRAY_SIZE = 10;
constexpr int DEFAULT_MAX_INLINE_OBJECT_SIZE = 3;
constexpr int DEFAULT_MAX_INLINE_STRING_LENGTH = 40;
constexpr int DEFAULT_MAX_LINE_WIDTH = 120;
constexpr int DEFAULT_COLON_SPACING = 1;
constexpr int DEFAULT_MAX_DEPTH = 100;

// Preset-specific values
constexpr int PRETTY_INLINE_ARRAY_SIZE = 8;
constexpr int PRETTY_MAX_LINE_WIDTH = 100;

constexpr int CONFIG_INLINE_ARRAY_SIZE = 5;
constexpr int CONFIG_INLINE_OBJECT_SIZE = 1;
constexpr int CONFIG_MAX_LINE_WIDTH = 100;

constexpr int API_INLINE_ARRAY_SIZE = 15;
constexpr int API_INLINE_OBJECT_SIZE = 4;
constexpr int API_MAX_INLINE_STRING_LENGTH = 50;
constexpr int API_MAX_LINE_WIDTH = 120;

constexpr int DEBUG_INDENT_SIZE = 4;
constexpr int DEBUG_INLINE_ARRAY_SIZE = 1;
constexpr int DEBUG_INLINE_OBJECT_SIZE = 0;
constexpr int DEBUG_MAX_LINE_WIDTH = 80;
} // namespace format_defaults

// Parser Buffer Sizes
namespace parser_constants {
constexpr int STRING_BUFFER_INITIAL_SIZE = 64;   // Initial string buffer size
constexpr int STRING_BUFFER_PARSE_SIZE = 1024;   // String buffer size for parsing
constexpr int NUMBER_BUFFER_SIZE = 32;           // Number buffer size
constexpr int NUMBER_BUFFER_PARSE_SIZE = 64;     // Number buffer size for parsing
constexpr int ARRAY_INITIAL_CAPACITY = 16;       // Initial array capacity
constexpr int JSON_DOCUMENT_INITIAL_SIZE = 1024; // Initial JsonDocument string size

// Literal string lengths
const std::string LITERAL_TRUE = "true";
const std::string LITERAL_FALSE = "false";
const std::string LITERAL_NULL = "null";
constexpr int TRUE_LENGTH = 4;           // "true"
constexpr int FALSE_LENGTH = 5;          // "false"
constexpr int NULL_LENGTH = 4;           // "null"
constexpr int UNICODE_ESCAPE_LENGTH = 4; // \uXXXX
} // namespace parser_constants

// Path Cache Configuration
namespace cache_constants {
constexpr size_t MAX_EXACT_CACHE_SIZE = 1000;
constexpr size_t MAX_PREFIX_CACHE_SIZE = 5000;
constexpr size_t MAX_RECENT_PREFIXES = 50;
constexpr int MAX_PREFIX_AGE_MINUTES = 10;
constexpr int DEFAULT_PRECOMPUTE_DEPTH = 5;
constexpr size_t CACHE_EVICTION_HALF_DIVISOR = 2; // Remove half when evicting
} // namespace cache_constants

// Unicode and Character Constants
namespace character_constants {
constexpr unsigned char MIN_CONTROL_CHAR = 0x20; // Minimum printable ASCII
constexpr unsigned char MAX_ASCII_CHAR = 126;    // Maximum basic ASCII
constexpr int HEX_WIDTH = 4;                     // Width for hex formatting (\uXXXX)
constexpr int INDENT_MULTIPLIER = 2;             // Spaces per indent level in basic formatting
constexpr int UNICODE_BUFFER_SIZE = 7;           // Buffer size for "\\uXXXX" + null terminator
} // namespace character_constants

// Unicode Encoding Constants (RFC 3629 UTF-8, RFC 2781 UTF-16)
namespace unicode_constants {
// UTF-8 codepoint boundaries (RFC 3629)
constexpr uint32_t UTF8_1_BYTE_MAX = 0x7F;        // 127 - ASCII range
constexpr uint32_t UTF8_2_BYTE_MAX = 0x7FF;       // 2047
constexpr uint32_t UTF8_3_BYTE_MAX = 0xFFFF;      // 65535 - BMP (Basic Multilingual Plane)
constexpr uint32_t UTF8_MAX_CODEPOINT = 0x10FFFF; // Maximum valid Unicode codepoint

// UTF-16 surrogate pair ranges (RFC 2781)
constexpr uint16_t HIGH_SURROGATE_START = 0xD800;
constexpr uint16_t HIGH_SURROGATE_END = 0xDBFF;
constexpr uint16_t LOW_SURROGATE_START = 0xDC00;
constexpr uint16_t LOW_SURROGATE_END = 0xDFFF;

// Surrogate pair conversion constants
constexpr uint32_t SURROGATE_OFFSET = 0x10000;
constexpr uint32_t SURROGATE_MASK = 0x3FF; // 10-bit mask for surrogate data

// Hex digit conversion
constexpr int HEX_LETTER_OFFSET = 10; // A=10, B=11, ..., F=15
} // namespace unicode_constants

// JSON Pointer Constants
namespace pointer_constants {
constexpr int SEGMENT_RESERVE_MULTIPLIER = 10; // segments.size() * 10 for reserve
constexpr int ESCAPE_RESERVE_DIVISOR = 4;      // length / 4 for escape reserve
} // namespace pointer_constants

} // namespace jsom
