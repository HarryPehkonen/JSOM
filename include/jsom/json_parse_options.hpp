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

    /// Validate numbers against the RFC 8259 grammar while scanning, rejecting `-01`,
    /// `1.0.`, `2.e+3`, `0e+`, `[-]` and friends with "Invalid number: …".
    ///
    /// Off by default, deliberately, and it is a policy rather than an oversight: with
    /// validation off JSOM accepts those forms as *extensions* (RFC 8259 §9 allows a
    /// parser to accept non-JSON forms), which is what the conformance suite's 25
    /// `n_number_*` disagreements are. Speed is the reason the default is off — see the
    /// measurement in OPTIMIZATIONS.md.
    ///
    /// Turn it on when the document came from somewhere you do not control, or when the
    /// JSON is going to be compared, keyed or re-serialized by a third party — a
    /// malformed number that survives the scan is a number whose *text* JSOM faithfully
    /// preserves, which is not the same as a number that is valid JSON.
    ///
    /// Validation does NOT imply eager conversion: LazyNumber still stores the original
    /// text, so round trips stay byte-identical and number access stays lazy.
    bool validate_numbers = false;
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

    /// Validate preset - also enforces the RFC 8259 number grammar while scanning.
    /// The lexical rules (escapes, control characters, whitespace) are always enforced,
    /// so this preset is about the one remaining leniency: numbers.
    static const JsonParseOptions Validate;
};

/**
 * Implementation of parsing presets.
 */
inline const JsonParseOptions ParsePresets::Default = {
    false,                     // convert_unicode_escapes
    false,                     // allow_comments
    limits::MAX_NESTING_DEPTH, // max_depth
    false                      // validate_numbers
};

inline const JsonParseOptions ParsePresets::Unicode = {
    true,                      // convert_unicode_escapes
    false,                     // allow_comments
    limits::MAX_NESTING_DEPTH, // max_depth
    false                      // validate_numbers
};

inline const JsonParseOptions ParsePresets::Comments = {
    false,                     // convert_unicode_escapes
    true,                      // allow_comments
    limits::MAX_NESTING_DEPTH, // max_depth
    false                      // validate_numbers
};

inline const JsonParseOptions ParsePresets::Validate = {
    false,                     // convert_unicode_escapes
    false,                     // allow_comments
    limits::MAX_NESTING_DEPTH, // max_depth
    true                       // validate_numbers
};

} // namespace jsom