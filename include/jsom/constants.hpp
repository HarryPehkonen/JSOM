#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace jsom {

// ================================
// Resource limits (RFC 8259 §9)
// ================================
// "An implementation may set limits on the size of texts that it accepts. An
// implementation may set limits on the maximum depth of nesting. …"
//
// JSOM's recursion is bounded by these constants. The alternative is a stack
// overflow, which cannot be caught with try/catch and kills the whole process, so
// no amount of downstream error handling can compensate: the bound has to be taken
// on the way in, and every traversal has to honour it. See README "Resource limits"
// and CONFORMANCE.md Finding 1.
namespace limits {

// Deepest nesting any JSOM operation will accept or traverse. Chosen from
// MEASUREMENT, not taste — stack cost per nesting level, measured 2026-09-16 with
// gcc on x86-64 (worst case is the unoptimised debug build):
//
//     path                  -O2     -O0
//     parse                 291 B   323 B
//     compare               64 B    323 B
//     serialize (compact)   533 B   565 B
//
// At 256 levels the worst case is ~145 KB, so every operation fits in a 1 MB thread
// stack with ~7x headroom and in 512 KB with ~3.5x. Raising this raises the stack
// requirement proportionally (~0.6 KB per level, roughly double under
// AddressSanitizer). Nothing in the RFC 8259 conformance suite nests deeper than
// 500, and no file the suite requires us to ACCEPT nests deeper than 3.
constexpr int MAX_NESTING_DEPTH = 256;

// One message for every depth guard, so callers can match on a single substring
// regardless of which path refused the document.
inline constexpr std::string_view MAX_NESTING_DEPTH_MESSAGE = "Maximum nesting depth exceeded";

} // namespace limits

// CLI Constants
namespace cli_constants {

// Display formatting
constexpr int SEPARATOR_LINE_WIDTH = 60;        // Width of separator lines (===, ---)
constexpr int BENCHMARK_PATH_COLUMN_WIDTH = 40; // Path column width in benchmark output
constexpr int BENCHMARK_TIME_COLUMN_WIDTH = 10; // Time column width in benchmark output
constexpr int BENCHMARK_PRECISION = 1;          // Decimal places for benchmark timing

// Command argument positions
constexpr int FIRST_OPTION_INDEX = 2; // argv[2] is first option

// Benchmark constants
constexpr int BENCHMARK_ITERATIONS = 1000;   // Number of iterations for benchmarking
constexpr double BENCHMARK_DIVISOR = 1000.0; // Divisor to get average time

// Colon spacing limits

// Examples for help text

// Error codes
} // namespace cli_constants

// JSON Format Option Defaults
namespace format_defaults {
constexpr int DEFAULT_INDENT_SIZE = 2;
constexpr int DEFAULT_MAX_INLINE_ARRAY_SIZE = 10;
constexpr int DEFAULT_MAX_INLINE_OBJECT_SIZE = 3;
constexpr int DEFAULT_MAX_INLINE_STRING_LENGTH = 40;
constexpr int DEFAULT_MAX_LINE_WIDTH = 120;
constexpr int DEFAULT_COLON_SPACING = 1;
// Printing depth shares the nesting limit: a document JSOM accepts must be a
// document JSOM can print. (This guard predates the parse-time limit and used to be
// 100 — it refused documents the parser happily accepted, which is exactly the kind
// of inconsistency that makes a limit feel arbitrary.)
constexpr int DEFAULT_MAX_DEPTH = limits::MAX_NESTING_DEPTH;

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
constexpr int JSON_DOCUMENT_INITIAL_SIZE = 1024; // Initial JsonDocument string size

// Literal string views (string_view: no static-init allocation, no throw risk)
inline constexpr std::string_view LITERAL_TRUE = "true";
inline constexpr std::string_view LITERAL_FALSE = "false";
inline constexpr std::string_view LITERAL_NULL = "null";
constexpr int TRUE_LENGTH = 4;           // "true"
constexpr int FALSE_LENGTH = 5;          // "false"
constexpr int NULL_LENGTH = 4;           // "null"
constexpr int UNICODE_ESCAPE_LENGTH = 4; // \uXXXX
} // namespace parser_constants

// Path Cache Configuration

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
