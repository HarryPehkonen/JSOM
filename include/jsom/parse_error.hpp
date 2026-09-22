#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace jsom {

/// One enumerator per distinguishable parse/format failure. Grouped by cause rather than
/// by throw site: several sites in the parser share a code (e.g. every malformed `\uXXXX`
/// shape is InvalidUnicodeEscape) when a caller has no reason to tell them apart.
enum class ParseErrorCode : uint8_t {
    NestingDepthExceeded, ///< a container, or a traversal of one, is nested past the limit
    InvalidNumber,        ///< RFC 8259 §6 number grammar rejected the token
    InvalidEscape,        ///< `\` followed by something other than a recognised escape
    InvalidUnicodeEscape, ///< a malformed, incomplete, or unpaired `\uXXXX` escape
    RawControlCharacter,  ///< an unescaped control character (U+0000..U+001F) in a string
    UnterminatedString,   ///< a string's closing `"` was never found
    UnterminatedComment,  ///< a `/* ... */` comment's closing `*/` was never found
    InvalidLiteral,       ///< `true`/`false`/`null` misspelled or truncated
    ExpectedToken,        ///< a structural character (`{}[]:,"`) was expected but missing
    EmptyInput,           ///< the input has no value at all
    TrailingContent,      ///< non-whitespace text follows a complete JSON value
};

/// Thrown for every parse and format failure in JSOM. Derives from std::runtime_error (not
/// std::invalid_argument) so existing code that catches std::runtime_error around
/// parse_document() / to_json() keeps working unmodified; code() lets new callers switch
/// on the failure kind instead of matching what() substrings.
class ParseError : public std::runtime_error {
public:
    ParseError(ParseErrorCode code, const std::string& message)
        : std::runtime_error(message), code_(code) {}

    [[nodiscard]] auto code() const noexcept -> ParseErrorCode { return code_; }

private:
    ParseErrorCode code_;
};

} // namespace jsom
