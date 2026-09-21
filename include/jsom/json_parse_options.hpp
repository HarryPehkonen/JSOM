#pragma once

#include "constants.hpp"

namespace jsom {

/**
 * Configuration options for JSON parsing behavior.
 *
 * This structure provides control over how JSON is parsed,
 * including Unicode escape sequence handling and resource limits.
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

    /// Deepest nesting accepted. Input nested deeper is rejected with
    /// "Maximum nesting depth exceeded" rather than exhausting the C++ stack —
    /// a stack overflow cannot be caught, so this bound is what keeps parsing a
    /// total function on hostile input (RFC 8259 §9 explicitly permits it).
    ///
    /// Lower it for a small-stack thread; raise it only with the stack to match
    /// (~0.6 KB per level, ≈2x under sanitizers). The default is safe in a 1 MB
    /// thread stack: see the calibration table in README "Resource limits".
    /// Note the asymmetry: traversals (serialize, compare, path listing) are bounded by
    /// limits::MAX_NESTING_DEPTH, so raising max_depth past that lets you parse documents
    /// those operations will then refuse. Lowering is the supported direction.
    int max_depth = limits::MAX_NESTING_DEPTH;

    /// Accept number forms that are NOT valid JSON — `01`, `1.`, `-.5`, `1eE2`, `1+2`.
    ///
    /// Off by default: RFC 8259 §6 is enforced while scanning, so an input that is not
    /// JSON is not reported as JSON. On, it is the documented extension mode of §9 —
    /// useful for hand-edited configs and producers you cannot change (spreadsheets,
    /// internal tools). Only the number grammar relaxes; escapes, control characters,
    /// whitespace and structure are rejected in both modes.
    ///
    /// This costs nothing to leave off. Enforcing the grammar happens inside the scan the
    /// parser already performs, with no second pass over the text, and measures 0.92x-0.98x
    /// against not enforcing it (OPTIMIZATIONS.md, "Number grammar").
    bool allow_loose_numbers = false;
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

    /// Loose preset - accepts the number extensions (allow_loose_numbers).
    /// Everything else — escapes, control characters, whitespace, structure — stays
    /// strict, because those rules are not optional in JSOM.
    static const JsonParseOptions Loose;
};

/**
 * Implementation of parsing presets.
 */
inline const JsonParseOptions ParsePresets::Default = {
    false,                     // convert_unicode_escapes
    false,                     // allow_comments
    limits::MAX_NESTING_DEPTH, // max_depth
    false                      // allow_loose_numbers
};

inline const JsonParseOptions ParsePresets::Unicode = {
    true,                      // convert_unicode_escapes
    false,                     // allow_comments
    limits::MAX_NESTING_DEPTH, // max_depth
    false                      // allow_loose_numbers
};

inline const JsonParseOptions ParsePresets::Comments = {
    false,                     // convert_unicode_escapes
    true,                      // allow_comments
    limits::MAX_NESTING_DEPTH, // max_depth
    false                      // allow_loose_numbers
};

inline const JsonParseOptions ParsePresets::Loose = {
    false,                     // convert_unicode_escapes
    false,                     // allow_comments
    limits::MAX_NESTING_DEPTH, // max_depth
    true                       // allow_loose_numbers
};

} // namespace jsom