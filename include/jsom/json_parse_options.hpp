#pragma once

namespace jsom {

/**
 * Configuration options for JSON parsing behavior.
 *
 * This structure provides control over how JSON is parsed,
 * including Unicode escape sequence handling.
 */
struct JsonParseOptions {
    /// Convert Unicode escape sequences (\uXXXX) to actual UTF-8 characters
    /// When false (default): Preserves \uXXXX as literal strings for round-trip fidelity
    /// When true: Converts \uXXXX sequences to their corresponding UTF-8 characters
    bool convert_unicode_escapes = false;

    /// Allow C/C++-style comments in JSON input
    /// When false (default): Strict JSON parsing, comments are syntax errors
    /// When true: Skips // line comments and /* block comments */
    bool allow_comments = false;
};

/**
 * Predefined parse presets for common use cases.
 */
class ParsePresets {
public:
    /// Default parsing - preserves Unicode escapes as-is for round-trip fidelity
    static const JsonParseOptions Default;

    /// Unicode parsing - converts Unicode escapes to actual UTF-8 characters
    static const JsonParseOptions Unicode;

    /// Comment-tolerant parsing - allows // and /* */ comments
    static const JsonParseOptions Comments;
};

/**
 * Implementation of parsing presets.
 */
inline const JsonParseOptions ParsePresets::Default = {
    false, // convert_unicode_escapes
    false  // allow_comments
};

inline const JsonParseOptions ParsePresets::Unicode = {
    true, // convert_unicode_escapes
    false // allow_comments
};

inline const JsonParseOptions ParsePresets::Comments = {
    false, // convert_unicode_escapes
    true   // allow_comments
};

} // namespace jsom