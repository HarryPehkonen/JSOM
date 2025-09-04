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
};

/**
 * Implementation of parsing presets.
 */
inline const JsonParseOptions ParsePresets::Default = {
    false  // convert_unicode_escapes
};

inline const JsonParseOptions ParsePresets::Unicode = {
    true   // convert_unicode_escapes
};

} // namespace jsom