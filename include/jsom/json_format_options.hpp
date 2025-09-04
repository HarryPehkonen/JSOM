#pragma once

#include "constants.hpp"
#include <optional>
#include <string>

namespace jsom {

/**
 * Configuration options for JSON formatting and pretty printing.
 *
 * This structure provides fine-grained control over JSON output formatting,
 * including intelligent inlining logic for arrays and objects.
 */
struct JsonFormatOptions {
    // Basic formatting controls
    std::optional<int> indent_size
        = format_defaults::DEFAULT_INDENT_SIZE; ///< Number of spaces per indentation level (nullopt
                                                ///< = compact mode)
    bool sort_keys = false;                     ///< Sort object keys alphabetically

    // Smart inlining controls
    int max_inline_array_size
        = format_defaults::DEFAULT_MAX_INLINE_ARRAY_SIZE; ///< Arrays with ≤ N elements stay on one
                                                          ///< line
    int max_inline_object_size
        = format_defaults::DEFAULT_MAX_INLINE_OBJECT_SIZE; ///< Objects with ≤ N properties stay on
                                                           ///< one line
    int max_inline_string_length
        = format_defaults::DEFAULT_MAX_INLINE_STRING_LENGTH; ///< Strings ≤ length don't break
                                                             ///< structure

    // Line width and alignment controls
    int max_line_width
        = format_defaults::DEFAULT_MAX_LINE_WIDTH; ///< Maximum characters per line (0 = no limit)
    bool align_values = false;                     ///< Align object values at same column

    // Enhanced spacing controls
    int colon_spacing
        = format_defaults::DEFAULT_COLON_SPACING; ///< Spaces around colons (0, 1, or 2)
    bool bracket_spacing = false;                 ///< Add spacing inside brackets/braces

    // Advanced options
    bool quote_keys = true;      ///< Quote object keys (JSON standard)
    bool trailing_comma = false; ///< Add trailing commas (non-standard)
    bool escape_unicode = false; ///< Escape non-ASCII characters as \uXXXX
    bool intelligent_wrapping
        = false; ///< Use intelligent line wrapping for arrays of simple values
    int max_depth = format_defaults::DEFAULT_MAX_DEPTH; ///< Maximum nesting depth allowed
};

/**
 * Predefined format presets for common use cases.
 */
class FormatPresets {
public:
    /// Minimal bandwidth, storage efficiency - no whitespace, everything on one line
    static const JsonFormatOptions Compact;

    /// General-purpose readable formatting with smart inlining and balanced readability
    static const JsonFormatOptions Pretty;

    /// Configuration files, settings - conservative inlining, sorted keys
    static const JsonFormatOptions Config;

    /// API responses, data interchange - balanced compactness and readability
    static const JsonFormatOptions Api;

    /// Debugging, development - maximum readability, every element on separate line
    static const JsonFormatOptions Debug;
};

/**
 * Implementation of formatting presets.
 */
inline const JsonFormatOptions FormatPresets::Compact = {
    std::nullopt,                                      // indent_size (compact mode)
    false,                                             // sort_keys
    format_defaults::DEFAULT_MAX_INLINE_ARRAY_SIZE,    // max_inline_array_size
    format_defaults::DEFAULT_MAX_INLINE_OBJECT_SIZE,   // max_inline_object_size
    format_defaults::DEFAULT_MAX_INLINE_STRING_LENGTH, // max_inline_string_length
    0,                                                 // max_line_width (no limit)
    false,                                             // align_values
    format_defaults::DEFAULT_COLON_SPACING,            // colon_spacing
    false,                                             // bracket_spacing
    true,                                              // quote_keys
    false,                                             // trailing_comma
    false,                                             // escape_unicode
    false,                                             // intelligent_wrapping
    format_defaults::DEFAULT_MAX_DEPTH                 // max_depth
};

inline const JsonFormatOptions FormatPresets::Pretty = {
    format_defaults::DEFAULT_INDENT_SIZE,      // indent_size (2 spaces)
    false,                                     // sort_keys
    format_defaults::PRETTY_INLINE_ARRAY_SIZE, // max_inline_array_size (slightly reduced for better
                                               // readability)
    format_defaults::DEFAULT_MAX_INLINE_OBJECT_SIZE,   // max_inline_object_size
    format_defaults::DEFAULT_MAX_INLINE_STRING_LENGTH, // max_inline_string_length
    format_defaults::PRETTY_MAX_LINE_WIDTH, // max_line_width (reduced to 100 for better editor
                                            // compatibility)
    true,                                   // align_values (enable for better readability)
    format_defaults::DEFAULT_COLON_SPACING, // colon_spacing
    false,                                  // bracket_spacing
    true,                                   // quote_keys
    false,                                  // trailing_comma
    false,                                  // escape_unicode
    true,                              // intelligent_wrapping (enable for arrays of simple values)
    format_defaults::DEFAULT_MAX_DEPTH // max_depth
};

inline const JsonFormatOptions FormatPresets::Config = {
    format_defaults::DEFAULT_INDENT_SIZE,              // indent_size (2 spaces)
    true,                                              // sort_keys
    format_defaults::CONFIG_INLINE_ARRAY_SIZE,         // max_inline_array_size
    format_defaults::CONFIG_INLINE_OBJECT_SIZE,        // max_inline_object_size
    format_defaults::DEFAULT_MAX_INLINE_STRING_LENGTH, // max_inline_string_length
    format_defaults::CONFIG_MAX_LINE_WIDTH,            // max_line_width
    true,                                              // align_values
    format_defaults::DEFAULT_COLON_SPACING,            // colon_spacing
    false,                                             // bracket_spacing
    true,                                              // quote_keys
    false,                                             // trailing_comma
    false,                                             // escape_unicode
    false,                             // intelligent_wrapping (keep traditional for config files)
    format_defaults::DEFAULT_MAX_DEPTH // max_depth
};

inline const JsonFormatOptions FormatPresets::Api = {
    format_defaults::DEFAULT_INDENT_SIZE,          // indent_size (2 spaces)
    false,                                         // sort_keys
    format_defaults::API_INLINE_ARRAY_SIZE,        // max_inline_array_size (slightly reduced for
                                                   // consistency)
    format_defaults::API_INLINE_OBJECT_SIZE,       // max_inline_object_size (slightly reduced)
    format_defaults::API_MAX_INLINE_STRING_LENGTH, // max_inline_string_length (increased for API
                                                   // data like URLs)
    format_defaults::API_MAX_LINE_WIDTH, // max_line_width (reduced for better editor compatibility)
    false,                               // align_values (keep false for compactness in APIs)
    format_defaults::DEFAULT_COLON_SPACING, // colon_spacing
    true,  // bracket_spacing (add for better readability in API responses)
    true,  // quote_keys
    false, // trailing_comma
    false, // escape_unicode
    true,  // intelligent_wrapping (great for API arrays of IDs, numbers)
    format_defaults::DEFAULT_MAX_DEPTH // max_depth
};

inline const JsonFormatOptions FormatPresets::Debug = {
    format_defaults::DEBUG_INDENT_SIZE,                // indent_size (4 spaces)
    true,                                              // sort_keys
    format_defaults::DEBUG_INLINE_ARRAY_SIZE,          // max_inline_array_size
    format_defaults::DEBUG_INLINE_OBJECT_SIZE,         // max_inline_object_size
    format_defaults::DEFAULT_MAX_INLINE_STRING_LENGTH, // max_inline_string_length
    format_defaults::DEBUG_MAX_LINE_WIDTH,             // max_line_width
    true,                                              // align_values
    format_defaults::DEFAULT_COLON_SPACING,            // colon_spacing
    true,                                              // bracket_spacing
    true,                                              // quote_keys
    false,                                             // trailing_comma
    true,                                              // escape_unicode
    false, // intelligent_wrapping (keep each element separate for debugging)
    format_defaults::DEFAULT_MAX_DEPTH // max_depth
};

} // namespace jsom