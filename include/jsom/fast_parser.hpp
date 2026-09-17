#pragma once

#include "constants.hpp"
#include "json_document.hpp"
#include "json_parse_options.hpp"
#include <cctype>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace jsom {

class FastParser {
private:
    const char* data_;
    size_t size_;
    size_t pos_;
    JsonParseOptions options_;

    /// The slow path, deliberately kept out of the inline check: the guard on the hot
    /// path is then one compare and a branch, and nothing to do with the error string
    /// is set up on the way through.
    [[noreturn]] static void throw_depth_error(int limit) {
        throw std::runtime_error(std::string(limits::MAX_NESTING_DEPTH_MESSAGE) + " (limit "
                                 + std::to_string(limit) + ")");
    }

    /// Current nesting level (1 = outermost container). Bounded so no input can exhaust
    /// the C++ stack: see limits::MAX_NESTING_DEPTH for the calibration, and the
    /// guard_cost200 measurement in OPTIMIZATIONS.md for why this is a member rather
    /// than a threaded argument (a member is ~1.5%; passing the level down the mutual
    /// recursion cost ~18% on a 200-deep document).
    ///
    /// INVARIANT: every normal return from parse_object()/parse_array() decrements it,
    /// and parse() resets it. A throw abandons the whole parse, so a counter left high
    /// by an exception cannot leak into the next document.
    int depth_ = 0;

    // Pre-allocated buffers to avoid reallocations
    std::string string_buffer_;
    std::string number_buffer_;

    /// RFC 8259 §2: `ws = *( %x20 / %x09 / %x0A / %x0D )`. std::isspace() is
    /// locale-dependent and also accepts formfeed and vertical tab, which the spec does
    /// not allow as whitespace — `[\f]` is an n_ file in the conformance suite.
    [[nodiscard]] static constexpr auto is_json_whitespace(char c) -> bool {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    /// RFC 8259 §7 hex digits for `\uXXXX`. hex_to_int() returns -1 for a non-hex digit
    /// (it does not throw), so the check is explicit here.
    void expect_unicode_escape_digits() {
        if (pos_ + parser_constants::UNICODE_ESCAPE_LENGTH > size_) {
            throw std::runtime_error("Incomplete unicode escape");
        }
        for (int i = 0; i < parser_constants::UNICODE_ESCAPE_LENGTH; ++i) {
            if (hex_to_int(data_[pos_ + static_cast<size_t>(i)]) < 0) {
                throw std::runtime_error("Invalid hex digit in unicode escape");
            }
        }
    }

    /// §7 `escape = ...` — anything else is a syntax error. The old default branch
    /// appended the character and DROPPED its backslash, silently altering the document;
    /// rejecting is both conformant and lossless.
    [[noreturn]] static void throw_invalid_escape(char escaped) {
        static constexpr std::string_view HEX_DIGITS = "0123456789abcdef";
        std::string shown;
        if (static_cast<unsigned char>(escaped) >= character_constants::MIN_CONTROL_CHAR) {
            shown = std::string("\\") + escaped;
        } else {
            shown = std::string("\\x");
            shown += HEX_DIGITS[static_cast<unsigned char>(escaped) >> 4];
            shown += HEX_DIGITS[static_cast<unsigned char>(escaped) & 0x0F];
        }
        throw std::runtime_error("Invalid escape sequence: " + shown);
    }

    void skip_whitespace() {
        while (pos_ < size_) {
            if (is_json_whitespace(data_[pos_])) {
                ++pos_;
            } else if (options_.allow_comments && pos_ + 1 < size_ && data_[pos_] == '/') {
                if (data_[pos_ + 1] == '/') {
                    // Line comment: skip to end of line
                    pos_ += 2;
                    while (pos_ < size_ && data_[pos_] != '\n') {
                        ++pos_;
                    }
                } else if (data_[pos_ + 1] == '*') {
                    // Block comment: skip to */
                    pos_ += 2;
                    while (pos_ + 1 < size_
                           && !(data_[pos_] == '*' && data_[pos_ + 1] == '/')) {
                        ++pos_;
                    }
                    if (pos_ + 1 < size_) {
                        pos_ += 2; // skip */
                    } else {
                        throw std::runtime_error("Unterminated block comment");
                    }
                } else {
                    break;
                }
            } else {
                break;
            }
        }
    }

    [[nodiscard]] auto peek() const -> char { return pos_ < size_ ? data_[pos_] : '\0'; }

    auto advance() -> char { return pos_ < size_ ? data_[pos_++] : '\0'; }

    void expect(char expected) {
        // NOLINTNEXTLINE(readability-identifier-length)
        char c = advance();
        if (c != expected) {
            throw std::runtime_error("Expected '" + std::string(1, expected) + "' but got '"
                                     + std::string(1, c) + "'");
        }
    }

    // Unicode escape sequence conversion helpers
    static auto hex_to_int(char c) -> int {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'A' && c <= 'F') {
            return c - 'A' + unicode_constants::HEX_LETTER_OFFSET;
        }
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + unicode_constants::HEX_LETTER_OFFSET;
        }
        return -1; // Invalid hex digit
    }

    auto parse_unicode_escape() -> uint16_t {
        if (pos_ + parser_constants::UNICODE_ESCAPE_LENGTH > size_) {
            throw std::runtime_error("Incomplete Unicode escape sequence");
        }

        uint16_t codepoint = 0;
        for (int i = 0; i < parser_constants::UNICODE_ESCAPE_LENGTH; ++i) {
            char c = advance();
            int hex_val = hex_to_int(c);
            if (hex_val == -1) {
                throw std::runtime_error("Invalid hex digit in Unicode escape: "
                                         + std::string(1, c));
            }
            codepoint = (codepoint << 4) | static_cast<uint16_t>(hex_val);
        }
        return codepoint;
    }

    void append_utf8(std::string& str, uint32_t codepoint) {
        if (codepoint <= unicode_constants::UTF8_1_BYTE_MAX) {
            // 1-byte UTF-8
            str += static_cast<char>(codepoint);
        } else if (codepoint <= unicode_constants::UTF8_2_BYTE_MAX) {
            // 2-byte UTF-8
            str += static_cast<char>(0xC0 | (codepoint >> 6));
            str += static_cast<char>(0x80 | (codepoint & 0x3F));
        } else if (codepoint <= unicode_constants::UTF8_3_BYTE_MAX) {
            // 3-byte UTF-8
            str += static_cast<char>(0xE0 | (codepoint >> 12));
            str += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            str += static_cast<char>(0x80 | (codepoint & 0x3F));
        } else if (codepoint <= unicode_constants::UTF8_MAX_CODEPOINT) {
            // 4-byte UTF-8
            str += static_cast<char>(0xF0 | (codepoint >> 18));
            str += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
            str += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            str += static_cast<char>(0x80 | (codepoint & 0x3F));
        } else {
            throw std::runtime_error("Invalid Unicode codepoint");
        }
    }

    // Fast string parsing with bulk operations
    // NOLINTBEGIN(readability-function-size)
    auto parse_string() -> JsonDocument {
        expect('"');
        string_buffer_.clear();
        string_buffer_.reserve(
            parser_constants::STRING_BUFFER_INITIAL_SIZE); // Pre-allocate reasonable size

        const char* start = data_ + pos_;
        const char* current = start;

        // Fast scan for end quote, an escape, or a raw control character
        while (pos_ < size_) {
            // NOLINTNEXTLINE(readability-identifier-length)
            char c = data_[pos_];
            // §7: control characters (U+0000..U+001F) MUST be escaped inside a string.
            if (static_cast<unsigned char>(c) < character_constants::MIN_CONTROL_CHAR) {
                throw std::runtime_error("Unescaped control character in string");
            }
            if (c == '"') {
                // Bulk append everything we've scanned
                string_buffer_.append(current, data_ + pos_ - current);
                ++pos_; // Skip closing quote
                return JsonDocument(string_buffer_);
            }
            if (c == '\\') {
                // Append everything up to escape
                string_buffer_.append(current, data_ + pos_ - current);
                ++pos_; // Skip backslash

                if (pos_ >= size_) {
                    break;
                }
                char escaped = advance();
                switch (escaped) {
                case '"':
                    string_buffer_ += '"';
                    break;
                case '\\':
                    string_buffer_ += '\\';
                    break;
                case '/':
                    string_buffer_ += '/';
                    break;
                case 'b':
                    string_buffer_ += '\b';
                    break;
                case 'f':
                    string_buffer_ += '\f';
                    break;
                case 'n':
                    string_buffer_ += '\n';
                    break;
                case 'r':
                    string_buffer_ += '\r';
                    break;
                case 't':
                    string_buffer_ += '\t';
                    break;
                case 'u': {
                    if (options_.convert_unicode_escapes) {
                        // Convert Unicode escape to UTF-8
                        uint16_t codepoint = parse_unicode_escape();

                        // Check for surrogate pairs (high surrogate)
                        if (codepoint >= unicode_constants::HIGH_SURROGATE_START
                            && codepoint <= unicode_constants::HIGH_SURROGATE_END) {
                            // High surrogate - look for low surrogate
                            if (pos_ + 1 < size_ && data_[pos_] == '\\' && data_[pos_ + 1] == 'u') {
                                pos_ += 2; // Skip \u
                                uint16_t low_surrogate = parse_unicode_escape();
                                if (low_surrogate >= unicode_constants::LOW_SURROGATE_START
                                    && low_surrogate <= unicode_constants::LOW_SURROGATE_END) {
                                    // Valid surrogate pair - convert to full codepoint
                                    uint32_t full_codepoint
                                        = unicode_constants::SURROGATE_OFFSET
                                          + ((static_cast<uint32_t>(codepoint)
                                              & unicode_constants::SURROGATE_MASK)
                                             << 10)
                                          + (static_cast<uint32_t>(low_surrogate)
                                             & unicode_constants::SURROGATE_MASK);
                                    append_utf8(string_buffer_, full_codepoint);
                                } else {
                                    throw std::runtime_error("Invalid low surrogate pair");
                                }
                            } else {
                                throw std::runtime_error("Incomplete surrogate pair");
                            }
                        } else if (codepoint >= unicode_constants::LOW_SURROGATE_START
                                   && codepoint <= unicode_constants::LOW_SURROGATE_END) {
                            throw std::runtime_error("Unexpected low surrogate");
                        } else {
                            // Regular codepoint
                            append_utf8(string_buffer_, codepoint);
                        }
                    } else {
                        // Preserve Unicode escape as-is (round-trip fidelity). The four
                        // digits are validated first: preserving `\uqqqq` as text is how
                        // it used to slip through.
                        expect_unicode_escape_digits();
                        string_buffer_ += "\\u";
                        for (int i = 0; i < parser_constants::UNICODE_ESCAPE_LENGTH; ++i) {
                            string_buffer_ += advance();
                        }
                    }
                    break;
                }
                default:
                    throw_invalid_escape(escaped);
                }
                current = data_ + pos_;
            } else {
                ++pos_;
            }
        }

        throw std::runtime_error("Unterminated string");
    }
    // NOLINTEND(readability-function-size)

    [[nodiscard]] static constexpr auto is_digit(char c) -> bool { return c >= '0' && c <= '9'; }
    [[nodiscard]] static constexpr auto is_digit_1_to_9(char c) -> bool { return c >= '1' && c <= '9'; }

    /// RFC 8259 §6 number grammar, applied to the text the scan already collected.
    /// Only used when JsonParseOptions::validate_numbers is set, because numbers are
    /// otherwise stored lazily and never inspected (which is why `-01`, `1.0.` and
    /// `2.e+3` were all accepted — see CONFORMANCE.md Finding 2).
    ///
    ///     number = [ minus ] int [ frac ] [ exp ]
    ///     int    = zero / ( digit1-9 *DIGIT )
    ///     frac   = "." 1*DIGIT
    ///     exp    = ("e" / "E") ["+" / "-"] 1*DIGIT
    [[nodiscard]] static auto is_valid_number(std::string_view text) -> bool {
        size_t i = 0;
        const size_t size = text.size();

        if (i < size && text[i] == '-') {
            ++i;
        }

        // int: leading zeros are what `-01`, `012` get wrong; `-` alone dies here.
        if (i >= size) {
            return false;
        }
        if (text[i] == '0') {
            ++i;
            if (i < size && is_digit(text[i])) {
                return false;
            }
        } else if (is_digit_1_to_9(text[i])) {
            while (i < size && is_digit(text[i])) {
                ++i;
            }
        } else {
            return false; // covers ".123", "-.123", "+1", "e5"
        }

        // frac: the dot must be followed by at least one digit ("1." and "2.e3").
        if (i < size && text[i] == '.') {
            ++i;
            const size_t frac_start = i;
            while (i < size && is_digit(text[i])) {
                ++i;
            }
            if (i == frac_start) {
                return false;
            }
        }

        // exp: same requirement after the sign ("0e", "0e+", "1.0e-", "1eE2").
        if (i < size && (text[i] == 'e' || text[i] == 'E')) {
            ++i;
            if (i < size && (text[i] == '+' || text[i] == '-')) {
                ++i;
            }
            const size_t exp_start = i;
            while (i < size && is_digit(text[i])) {
                ++i;
            }
            if (i == exp_start) {
                return false;
            }
        }

        return i == size; // anything left over ("0.1.2", "1+2", "0e+-1") is a rejection
    }

    // Fast number parsing with bulk scanning
    auto parse_number() -> JsonDocument {
        number_buffer_.clear();
        number_buffer_.reserve(parser_constants::NUMBER_BUFFER_SIZE);

        const char* start = data_ + pos_;

        // Fast scan for number end (direct comparisons — std::isdigit is a
        // locale-table call per character; OPTIMIZATIONS.md #4)
        while (pos_ < size_) {
            // NOLINTNEXTLINE(readability-identifier-length)
            char c = data_[pos_];
            if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+'
                || c == '-') {
                ++pos_;
            } else {
                break;
            }
        }

        // Bulk copy the number
        number_buffer_.assign(start, data_ + pos_ - start);

        // Opt-in grammar check (JsonParseOptions::validate_numbers). The text is still
        // stored lazily either way, so this costs one pass over the collected digits and
        // buys nothing to the caller who only re-serializes what it parsed.
        if (options_.validate_numbers && !is_valid_number(number_buffer_)) {
            throw std::runtime_error("Invalid number: " + number_buffer_);
        }

        return JsonDocument::from_lazy_number(number_buffer_);
    }

    // Fast literal parsing
    auto parse_literal() -> JsonDocument {
        // Determine literal type by first character
        char first = peek();
        if (first == 't') {
            if (pos_ + parser_constants::TRUE_LENGTH <= size_
                && std::memcmp(data_ + pos_, parser_constants::LITERAL_TRUE.data(),
                               parser_constants::TRUE_LENGTH)
                       == 0) {
                pos_ += parser_constants::TRUE_LENGTH;
                return JsonDocument(true);
            }
        } else if (first == 'f') {
            if (pos_ + parser_constants::FALSE_LENGTH <= size_
                && std::memcmp(data_ + pos_, parser_constants::LITERAL_FALSE.data(),
                               parser_constants::FALSE_LENGTH)
                       == 0) {
                pos_ += parser_constants::FALSE_LENGTH;
                return JsonDocument(false);
            }
        } else if (first == 'n') {
            if (pos_ + parser_constants::NULL_LENGTH <= size_
                && std::memcmp(data_ + pos_, parser_constants::LITERAL_NULL.data(),
                               parser_constants::NULL_LENGTH)
                       == 0) {
                pos_ += parser_constants::TRUE_LENGTH;
                return {};
            }
        }

        throw std::runtime_error("Invalid literal");
    }

    // Fast object parsing with direct building
    // NOLINTBEGIN(readability-function-size)
    auto parse_object() -> JsonDocument {
        // Bounded on the way IN: a stack overflow cannot be caught, so the check has to
        // happen before the recursion, not after it.
        if (++depth_ > options_.max_depth) {
            throw_depth_error(options_.max_depth);
        }
        expect('{');
        skip_whitespace();

        // Create the final object immediately
        JsonDocument result(std::initializer_list<std::pair<const std::string, JsonDocument>>{});

        if (peek() == '}') {
            advance();
            --depth_;
            return result;
        }

        while (true) {
            skip_whitespace();

            // Parse key
            if (peek() != '"') {
                throw std::runtime_error("Expected string key in object");
            }
            auto key_doc = parse_string();
            auto key = key_doc.take_string();  // move out — no copy (OPTIMIZATIONS.md #3)

            skip_whitespace();
            expect(':');
            skip_whitespace();

            // Use move-optimized set method - eliminates intermediate vector!
            result.set(std::move(key), parse_value());

            skip_whitespace();
            // NOLINTNEXTLINE(readability-identifier-length)
            char c = advance();
            if (c == '}') {
                break;
            }
            if (c != ',') {
                throw std::runtime_error("Expected ',' or '}' in object");
            }
        }

        --depth_;
        return result;
    }
    // NOLINTEND(readability-function-size)

    // Fast array parsing with direct building
    // NOLINTBEGIN(readability-function-size)
    auto parse_array() -> JsonDocument {
        if (++depth_ > options_.max_depth) {
            throw_depth_error(options_.max_depth);
        }
        expect('[');
        skip_whitespace();

        // Create the final array immediately
        JsonDocument result(std::vector<JsonDocument>{});

        if (peek() == ']') {
            advance();
            --depth_;
            return result;
        }

        while (true) {
            skip_whitespace();

            // Append in place (OPTIMIZATIONS.md #1b): push_back move-constructs
            // the element; set(index) resized (default-constructing a null
            // document) then move-assigned — one extra construction per element.
            // A/B measured: -9.5% numbers, -11.5% strings, -16.1% deep nesting.
            result.push_back(parse_value());

            skip_whitespace();
            // NOLINTNEXTLINE(readability-identifier-length)
            char c = advance();
            if (c == ']') {
                break;
            }
            if (c != ',') {
                throw std::runtime_error("Expected ',' or ']' in array");
            }
        }

        --depth_;
        return result;
    }
    // NOLINTEND(readability-function-size)

    // NOLINTBEGIN(readability-function-size)
    auto parse_value() -> JsonDocument {
        skip_whitespace();
        // NOLINTNEXTLINE(readability-identifier-length)
        char c = peek();

        switch (c) {
        case '"':
            return parse_string();
        case '{':
            return parse_object();
        case '[':
            return parse_array();
        case 't':
        case 'f':
        case 'n':
            return parse_literal();
        case '-':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            return parse_number();
        default:
            throw std::runtime_error("Unexpected character: " + std::string(1, c));
        }
    }
    // NOLINTEND(readability-function-size)

public:
    explicit FastParser(const JsonParseOptions& options = {}) : options_(options) {}

    auto parse(const std::string& json) -> JsonDocument {
        data_ = json.data();
        size_ = json.size();
        pos_ = 0;
        depth_ = 0; // a parser instance may be reused for another document

        // Pre-allocate buffers
        string_buffer_.reserve(parser_constants::STRING_BUFFER_PARSE_SIZE);
        number_buffer_.reserve(parser_constants::NUMBER_BUFFER_PARSE_SIZE);

        skip_whitespace();
        if (pos_ >= size_) {
            throw std::runtime_error("Empty JSON input");
        }

        auto result = parse_value();


        skip_whitespace();
        if (pos_ < size_) {
            throw std::runtime_error("Unexpected characters after JSON");
        }

        return result;
    }
};

// Parse function: the only parser (the legacy event-based streaming parser was removed
// on 2026-09-16; see parse_document.hpp)
inline auto parse_document_fast(const std::string& json) -> JsonDocument {
    FastParser parser;
    return parser.parse(json);
}

} // namespace jsom
