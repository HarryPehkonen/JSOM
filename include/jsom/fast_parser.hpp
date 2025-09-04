#pragma once

#include "constants.hpp"
#include "json_document.hpp"
#include "json_parse_options.hpp"
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

namespace jsom {

class FastParser {
private:
    const char* data_;
    size_t size_;
    size_t pos_;
    JsonParseOptions options_;

    // Pre-allocated buffers to avoid reallocations
    std::string string_buffer_;
    std::string number_buffer_;

    void skip_whitespace() {
        while (pos_ < size_ && (std::isspace(data_[pos_]) != 0)) {
            ++pos_;
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
            return c - 'A' + 10;
        }
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
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
                throw std::runtime_error("Invalid hex digit in Unicode escape: " + std::string(1, c));
            }
            codepoint = (codepoint << 4) | static_cast<uint16_t>(hex_val);
        }
        return codepoint;
    }

    void append_utf8(std::string& str, uint32_t codepoint) {
        if (codepoint <= 0x7F) {
            // 1-byte UTF-8
            str += static_cast<char>(codepoint);
        } else if (codepoint <= 0x7FF) {
            // 2-byte UTF-8
            str += static_cast<char>(0xC0 | (codepoint >> 6));
            str += static_cast<char>(0x80 | (codepoint & 0x3F));
        } else if (codepoint <= 0xFFFF) {
            // 3-byte UTF-8
            str += static_cast<char>(0xE0 | (codepoint >> 12));
            str += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            str += static_cast<char>(0x80 | (codepoint & 0x3F));
        } else if (codepoint <= 0x10FFFF) {
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

        // Fast scan for end quote or escape
        while (pos_ < size_) {
            // NOLINTNEXTLINE(readability-identifier-length)
            char c = data_[pos_];
            if (c == '"') {
                // Bulk append everything we've scanned
                string_buffer_.append(current, data_ + pos_ - current);
                ++pos_; // Skip closing quote
                return JsonDocument(std::move(string_buffer_));
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
                        
                        // Check for surrogate pairs (high surrogate: 0xD800-0xDBFF)
                        if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                            // High surrogate - look for low surrogate
                            if (pos_ + 1 < size_ && data_[pos_] == '\\' && data_[pos_ + 1] == 'u') {
                                pos_ += 2; // Skip \u
                                uint16_t low_surrogate = parse_unicode_escape();
                                if (low_surrogate >= 0xDC00 && low_surrogate <= 0xDFFF) {
                                    // Valid surrogate pair - convert to full codepoint
                                    uint32_t full_codepoint = 0x10000 + 
                                        ((static_cast<uint32_t>(codepoint) & 0x3FF) << 10) +
                                        (static_cast<uint32_t>(low_surrogate) & 0x3FF);
                                    append_utf8(string_buffer_, full_codepoint);
                                } else {
                                    throw std::runtime_error("Invalid low surrogate pair");
                                }
                            } else {
                                throw std::runtime_error("Incomplete surrogate pair");
                            }
                        } else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) {
                            throw std::runtime_error("Unexpected low surrogate");
                        } else {
                            // Regular codepoint
                            append_utf8(string_buffer_, codepoint);
                        }
                    } else {
                        // Preserve Unicode escape as-is (current behavior)
                        if (pos_ + parser_constants::UNICODE_ESCAPE_LENGTH > size_) {
                            break;
                        }
                        string_buffer_ += "\\u";
                        for (int i = 0; i < parser_constants::UNICODE_ESCAPE_LENGTH; ++i) {
                            string_buffer_ += advance();
                        }
                    }
                    break;
                }
                default:
                    string_buffer_ += escaped;
                    break;
                }
                current = data_ + pos_;
            } else {
                ++pos_;
            }
        }

        throw std::runtime_error("Unterminated string");
    }
    // NOLINTEND(readability-function-size)

    // Fast number parsing with bulk scanning
    auto parse_number() -> JsonDocument {
        number_buffer_.clear();
        number_buffer_.reserve(parser_constants::NUMBER_BUFFER_SIZE);

        const char* start = data_ + pos_;

        // Fast scan for number end
        while (pos_ < size_) {
            // NOLINTNEXTLINE(readability-identifier-length)
            char c = data_[pos_];
            if ((std::isdigit(c) != 0) || c == '.' || c == 'e' || c == 'E' || c == '+'
                || c == '-') {
                ++pos_;
            } else {
                break;
            }
        }

        // Bulk copy the number
        number_buffer_.assign(start, data_ + pos_ - start);
        return JsonDocument::from_lazy_number(number_buffer_);
    }

    // Fast literal parsing
    auto parse_literal() -> JsonDocument {
        // Determine literal type by first character
        char first = peek();
        if (first == 't') {
            if (pos_ + parser_constants::TRUE_LENGTH <= size_
                && std::memcmp(data_ + pos_, parser_constants::LITERAL_TRUE.c_str(),
                               parser_constants::TRUE_LENGTH)
                       == 0) {
                pos_ += parser_constants::TRUE_LENGTH;
                return JsonDocument(true);
            }
        } else if (first == 'f') {
            if (pos_ + parser_constants::FALSE_LENGTH <= size_
                && std::memcmp(data_ + pos_, parser_constants::LITERAL_FALSE.c_str(),
                               parser_constants::FALSE_LENGTH)
                       == 0) {
                pos_ += parser_constants::FALSE_LENGTH;
                return JsonDocument(false);
            }
        } else if (first == 'n') {
            if (pos_ + parser_constants::NULL_LENGTH <= size_
                && std::memcmp(data_ + pos_, parser_constants::LITERAL_NULL.c_str(),
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
        expect('{');
        skip_whitespace();

        // Create the final object immediately
        JsonDocument result(std::initializer_list<std::pair<const std::string, JsonDocument>>{});

        if (peek() == '}') {
            advance();
            return result;
        }

        while (true) {
            skip_whitespace();

            // Parse key
            if (peek() != '"') {
                throw std::runtime_error("Expected string key in object");
            }
            auto key_doc = parse_string();
            auto key = key_doc.as<std::string>();

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

        return result;
    }
    // NOLINTEND(readability-function-size)

    // Fast array parsing with direct building
    // NOLINTBEGIN(readability-function-size)
    auto parse_array() -> JsonDocument {
        expect('[');
        skip_whitespace();

        // Create the final array immediately
        JsonDocument result(std::initializer_list<JsonDocument>{});

        if (peek() == ']') {
            advance();
            return result;
        }

        // Use array index for direct insertion - eliminates intermediate vector
        size_t index = 0;

        while (true) {
            skip_whitespace();

            // Set directly using index - no intermediate storage!
            result.set(index++, parse_value());

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

// Optimized parse function that replaces the slow streaming parser
inline auto parse_document_fast(const std::string& json) -> JsonDocument {
    FastParser parser;
    return parser.parse(json);
}

} // namespace jsom
