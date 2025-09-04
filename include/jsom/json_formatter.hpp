#pragma once

#include "constants.hpp"
#include "json_document.hpp"
#include "json_format_options.hpp"
#include <algorithm>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace jsom {

/**
 * Advanced JSON formatter with intelligent inlining and customizable formatting options.
 */
class JsonFormatter {
public:
    explicit JsonFormatter(const JsonFormatOptions& options = FormatPresets::Compact)
        : options_(options) {}

    /**
     * Format a JsonDocument to string using the configured options.
     */
    [[nodiscard]] auto format(const JsonDocument& doc) const -> std::string {
        std::ostringstream oss;
        format_value(oss, doc, 0);
        return oss.str();
    }

private:
    const JsonFormatOptions& options_;

    void format_value(std::ostringstream& oss, const JsonDocument& doc, int depth) const {
        if (depth > options_.max_depth) {
            throw std::runtime_error("Maximum formatting depth exceeded");
        }

        switch (doc.type()) {
        case JsonType::Null:
            oss << "null";
            break;
        case JsonType::Boolean:
            oss << (doc.as<bool>() ? "true" : "false");
            break;
        case JsonType::Number:
            oss << std::get<LazyNumber>(doc.storage_).as_string();
            break;
        case JsonType::String:
            format_string(oss, doc.as<std::string>());
            break;
        case JsonType::Array:
            format_array(oss, doc, depth);
            break;
        case JsonType::Object:
            format_object(oss, doc, depth);
            break;
        }
    }

    // NOLINTBEGIN(readability-function-size)
    void format_string(std::ostringstream& oss, const std::string& str) const {
        oss << '"';
        
        for (size_t i = 0; i < str.length(); ++i) {
            char c = str[i];
            
            // Check for preserved Unicode escape sequences (\uXXXX)
            if (c == '\\' && i + 5 <= str.length() && str[i + 1] == 'u') {
                bool is_valid_unicode_escape = true;
                for (int j = 2; j < 6; ++j) {
                    char hex_char = str[i + j];
                    if (!((hex_char >= '0' && hex_char <= '9') ||
                          (hex_char >= 'A' && hex_char <= 'F') ||
                          (hex_char >= 'a' && hex_char <= 'f'))) {
                        is_valid_unicode_escape = false;
                        break;
                    }
                }
                
                if (is_valid_unicode_escape) {
                    // Preserve the Unicode escape sequence as-is
                    oss << str.substr(i, 6);  // \uXXXX
                    i += 5; // Skip the next 5 characters (we'll increment by 1 in the loop)
                    continue;
                }
            }
            
            // Handle regular escape sequences and characters
            switch (c) {
            case '"':
                oss << "\\\"";
                break;
            case '\\':
                oss << "\\\\";
                break;
            case '\b':
                oss << "\\b";
                break;
            case '\f':
                oss << "\\f";
                break;
            case '\n':
                oss << "\\n";
                break;
            case '\r':
                oss << "\\r";
                break;
            case '\t':
                oss << "\\t";
                break;
            default:
                if (options_.escape_unicode
                    && (c < character_constants::MIN_CONTROL_CHAR
                        || c > character_constants::MAX_ASCII_CHAR)) {
                    oss << "\\u" << std::hex << std::setfill('0')
                        << std::setw(character_constants::HEX_WIDTH)
                        << static_cast<unsigned char>(c);
                } else {
                    oss << c;
                }
                break;
            }
        }
        oss << '"';
    }
    // NOLINTEND(readability-function-size)

    struct ArrayFormatStrategy {
        bool should_inline;
        bool use_intelligent_wrapping;
    };

    struct ArrayLineFormatter {
        std::string current_line;
        size_t available_width;
        std::string line_prefix;
        bool is_multiline_mode;
        bool first_element{true};

        ArrayLineFormatter(size_t width, std::string prefix, bool multiline)
            : available_width(width), line_prefix(std::move(prefix)), is_multiline_mode(multiline) {
            current_line.reserve(width);
        }

        [[nodiscard]] auto would_exceed_width(const std::string& element_str) const -> bool {
            return (current_line.length() + element_str.length()) > available_width;
        }

        void add_element(const std::string& element_str) {
            current_line += element_str;
            first_element = false;
        }

        void output_current_line(std::ostringstream& oss) const {
            if (!current_line.empty()) {
                oss << current_line;
            }
        }

        void start_new_line(std::ostringstream& oss) {
            if (is_multiline_mode) {
                if (!current_line.empty()) {
                    oss << "\n" << line_prefix;
                }
            } else {
                if (!current_line.empty()) {
                    oss << ", ";
                }
            }
            current_line.clear();
        }

        [[nodiscard]] auto is_empty() const -> bool { return current_line.empty(); }
    };

    [[nodiscard]] auto format_element_to_string(const JsonDocument& element, int depth) const
        -> std::string {
        std::ostringstream element_stream;
        format_value(element_stream, element, depth);
        return element_stream.str();
    }

    [[nodiscard]] auto format_element_with_separator(const JsonDocument& element, int depth,
                                                     bool needs_separator) const -> std::string {
        std::ostringstream element_stream;
        if (needs_separator) {
            element_stream << ", ";
        }
        format_value(element_stream, element, depth);
        return element_stream.str();
    }

    void format_array_without_width_limit(std::ostringstream& oss,
                                          const std::vector<JsonDocument>& arr, int depth) const {
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i > 0) {
                oss << ", ";
            }
            format_value(oss, arr[i], depth + 1);
        }
    }

    void format_array_elements_with_wrapping(std::ostringstream& oss,
                                             const std::vector<JsonDocument>& arr, int depth,
                                             bool is_multiline_mode) const {
        if (options_.max_line_width <= 0 || arr.empty()) {
            format_array_without_width_limit(oss, arr, depth);
            return;
        }

        std::string line_prefix
            = options_.indent_size.has_value() && is_multiline_mode ? indent(depth + 1) : "";

        size_t available_width;
        if (is_multiline_mode) {
            available_width = options_.max_line_width - line_prefix.length();
        } else {
            size_t prefix_length
                = options_.indent_size.has_value() ? indent(depth + 1).length() : 0;
            available_width = options_.max_line_width - prefix_length;
        }

        ArrayLineFormatter formatter(available_width, line_prefix, is_multiline_mode);

        for (const auto& element : arr) {
            bool needs_separator = !formatter.first_element;
            std::string element_str
                = format_element_with_separator(element, depth + 1, needs_separator);

            if (needs_separator && formatter.would_exceed_width(element_str)
                && !formatter.is_empty()) {
                formatter.output_current_line(oss);
                formatter.start_new_line(oss);

                std::string clean_element = format_element_to_string(element, depth + 1);
                formatter.add_element(clean_element);
            } else {
                formatter.add_element(element_str);
            }
        }

        formatter.output_current_line(oss);
    }

    void format_empty_array(std::ostringstream& oss) const {
        if (options_.bracket_spacing) {
            oss << "[ ]";
        } else {
            oss << "[]";
        }
    }

    [[nodiscard]] auto determine_array_format_strategy(const std::vector<JsonDocument>& arr,
                                                       int depth) const -> ArrayFormatStrategy {
        bool should_inline = should_inline_array(arr);
        bool use_intelligent_wrapping = false;

        // Check if we should use intelligent wrapping instead of traditional inline/multiline
        if (options_.intelligent_wrapping && contains_only_simple_values(arr)
            && static_cast<int>(arr.size()) > options_.max_inline_array_size) {
            should_inline = false;           // Use multiline structure
            use_intelligent_wrapping = true; // But with intelligent wrapping
        }

        // Apply width checking for all modes when max_line_width is set
        if (options_.max_line_width > 0 && should_inline) {
            bool fits_on_line = check_array_fits_on_line(arr, depth);
            if (!fits_on_line) {
                should_inline = false; // Force multiline if it doesn't fit
            }
        }

        return {should_inline, use_intelligent_wrapping};
    }

    void add_opening_bracket_spacing(std::ostringstream& oss, bool should_inline) const {
        if (options_.bracket_spacing && should_inline) {
            oss << " ";
        }
    }

    void add_closing_bracket_spacing(std::ostringstream& oss, bool should_inline) const {
        if (options_.bracket_spacing && should_inline) {
            oss << " ";
        }
    }

    void format_inline_array(std::ostringstream& oss, const std::vector<JsonDocument>& arr,
                             int depth, bool /* use_intelligent_wrapping */) const {
        if (options_.max_line_width > 0) {
            format_array_with_intelligent_wrapping(oss, arr, depth);
        } else {
            // Standard inline format without width constraints
            for (size_t i = 0; i < arr.size(); ++i) {
                if (i > 0) {
                    oss << ", ";
                }
                format_value(oss, arr[i], depth + 1);
            }
        }
    }

    void format_multiline_array(std::ostringstream& oss, const std::vector<JsonDocument>& arr,
                                int depth, bool use_intelligent_wrapping) const {
        if (use_intelligent_wrapping) {
            format_intelligent_multiline_array(oss, arr, depth);
        } else {
            format_traditional_multiline_array(oss, arr, depth);
        }
    }

    void format_intelligent_multiline_array(std::ostringstream& oss,
                                            const std::vector<JsonDocument>& arr, int depth) const {
        // Intelligent wrapping: multiple elements per line within width constraints
        if (options_.indent_size.has_value()) {
            oss << "\n" << indent(depth + 1);
        }
        format_array_with_intelligent_multiline_wrapping(oss, arr, depth);
        if (options_.indent_size.has_value()) {
            oss << "\n" << indent(depth);
        }
    }

    void format_traditional_multiline_array(std::ostringstream& oss,
                                            const std::vector<JsonDocument>& arr, int depth) const {
        // Traditional multiline: one element per line
        for (size_t i = 0; i < arr.size(); ++i) {
            if (options_.indent_size.has_value()) {
                oss << "\n" << indent(depth + 1);
            }
            format_value(oss, arr[i], depth + 1);
            if ((i < arr.size() - 1) || (options_.trailing_comma)) {
                oss << ",";
            }
        }
        if (options_.indent_size.has_value()) {
            oss << "\n" << indent(depth);
        }
    }

public:
    void format_array(std::ostringstream& oss, const JsonDocument& doc, int depth) const {
        const auto& arr = doc.as<std::vector<JsonDocument>>();

        if (arr.empty()) {
            format_empty_array(oss);
            return;
        }

        auto format_strategy = determine_array_format_strategy(arr, depth);

        oss << "[";
        add_opening_bracket_spacing(oss, format_strategy.should_inline);

        if (format_strategy.should_inline) {
            format_inline_array(oss, arr, depth, format_strategy.use_intelligent_wrapping);
        } else {
            format_multiline_array(oss, arr, depth, format_strategy.use_intelligent_wrapping);
        }

        add_closing_bracket_spacing(oss, format_strategy.should_inline);
        oss << "]";
    }

    void format_empty_object(std::ostringstream& oss) const {
        if (options_.bracket_spacing) {
            oss << "{ }";
        } else {
            oss << "{}";
        }
    }

    [[nodiscard]] auto prepare_object_keys(const std::map<std::string, JsonDocument>& obj) const
        -> std::vector<std::string> {
        std::vector<std::string> keys;
        keys.reserve(obj.size());
        for (const auto& pair : obj) {
            keys.push_back(pair.first);
        }

        if (options_.sort_keys) {
            std::sort(keys.begin(), keys.end());
        }

        return keys;
    }

    [[nodiscard]] auto calculate_max_key_width(const std::vector<std::string>& keys,
                                               bool should_inline) const -> size_t {
        size_t max_key_width = 0;
        if (options_.align_values && !should_inline) {
            for (const auto& key : keys) {
                // Account for quotes around key
                size_t key_length
                    = key.length()
                      + (options_.quote_keys ? character_constants::INDENT_MULTIPLIER : 0);
                max_key_width = std::max(max_key_width, key_length);
            }
        }
        return max_key_width;
    }

    void format_inline_object(std::ostringstream& oss,
                              const std::map<std::string, JsonDocument>& obj,
                              const std::vector<std::string>& keys, int depth) const {
        // Inline format
        for (size_t i = 0; i < keys.size(); ++i) {
            if (i > 0) {
                oss << ", ";
            }
            format_key(oss, keys[i]);
            format_colon_spacing(oss);
            format_value(oss, obj.at(keys[i]), depth + 1);
        }
    }

    void format_multiline_object(std::ostringstream& oss,
                                 const std::map<std::string, JsonDocument>& obj, int depth,
                                 const std::vector<std::string>& keys, size_t max_key_width) const {
        // Multiline format
        for (size_t i = 0; i < keys.size(); ++i) {
            if (options_.indent_size.has_value()) {
                oss << "\n" << indent(depth + 1);
            }

            // Format key with potential alignment
            std::ostringstream key_stream;
            format_key(key_stream, keys[i]);
            std::string formatted_key = key_stream.str();
            oss << formatted_key;

            // Add padding for alignment
            if (options_.align_values && max_key_width > formatted_key.length()) {
                oss << std::string(max_key_width - formatted_key.length(), ' ');
            }

            format_colon_spacing(oss);
            format_value(oss, obj.at(keys[i]), depth + 1);

            if ((i < keys.size() - 1) || (options_.trailing_comma)) {
                oss << ",";
            }
        }
        if (options_.indent_size.has_value()) {
            oss << "\n" << indent(depth);
        }
    }

    void format_object(std::ostringstream& oss, const JsonDocument& doc, int depth) const {
        const auto& obj = doc.as<std::map<std::string, JsonDocument>>();

        if (obj.empty()) {
            format_empty_object(oss);
            return;
        }

        bool should_inline = should_inline_object(obj);
        auto keys = prepare_object_keys(obj);
        auto max_key_width = calculate_max_key_width(keys, should_inline);

        oss << "{";
        add_opening_bracket_spacing(oss, should_inline);

        if (should_inline) {
            format_inline_object(oss, obj, keys, depth);
        } else {
            format_multiline_object(oss, obj, depth, keys, max_key_width);
        }

        add_closing_bracket_spacing(oss, should_inline);
        oss << "}";
    }

    void format_key(std::ostringstream& oss, const std::string& key) const {
        if (options_.quote_keys) {
            format_string(oss, key);
        } else {
            oss << key;
        }
    }

    void format_colon_spacing(std::ostringstream& oss) const {
        if (options_.colon_spacing == 0) {
            oss << ":";
        } else if (options_.colon_spacing == 1) {
            oss << ": ";
        } else {
            oss << " : ";
        }
    }

    [[nodiscard]] auto check_array_fits_on_line(const std::vector<JsonDocument>& arr,
                                                int depth) const -> bool {
        if (options_.max_line_width <= 0) {
            return true; // No width limit
        }

        // Calculate the total length if we format inline
        size_t total_length = 1; // Opening bracket
        if (options_.bracket_spacing) {
            total_length += character_constants::INDENT_MULTIPLIER; // " ]"
        }

        for (size_t i = 0; i < arr.size(); ++i) {
            if (i > 0) {
                total_length += character_constants::INDENT_MULTIPLIER; // ", "
            }

            std::ostringstream element_stream;
            format_value(element_stream, arr[i], depth + 1);
            total_length += element_stream.str().length();

            // Early exit if we already exceed the limit
            if (total_length > static_cast<size_t>(options_.max_line_width)) {
                return false;
            }
        }

        return total_length <= static_cast<size_t>(options_.max_line_width);
    }

    void format_array_with_intelligent_multiline_wrapping(std::ostringstream& oss,
                                                          const std::vector<JsonDocument>& arr,
                                                          int depth) const {
        format_array_elements_with_wrapping(oss, arr, depth, true);
    }

    void format_array_with_intelligent_wrapping(std::ostringstream& oss,
                                                const std::vector<JsonDocument>& arr,
                                                int depth) const {
        format_array_elements_with_wrapping(oss, arr, depth, false);
    }

    [[nodiscard]] static auto contains_only_simple_values(const std::vector<JsonDocument>& arr)
        -> bool {
        return std::all_of(arr.begin(), arr.end(), [](const auto& item) {
            return item.type() != JsonType::Array && item.type() != JsonType::Object;
        });
    }

    [[nodiscard]] auto should_inline_array(const std::vector<JsonDocument>& arr) const -> bool {
        if (!options_.indent_size.has_value()) {
            return true;
        }

        // Content check first - arrays containing nested containers become multiline
        if (!contains_only_simple_values(arr)) {
            return false;
        }

        // Traditional size check - intelligent wrapping will be handled separately
        if (static_cast<int>(arr.size()) > options_.max_inline_array_size) {
            return false;
        }

        return true;
    }

    [[nodiscard]] auto should_inline_object(const std::map<std::string, JsonDocument>& obj) const
        -> bool {
        if (!options_.indent_size.has_value()) {
            return true;
        }

        // Size check
        if (static_cast<int>(obj.size()) > options_.max_inline_object_size) {
            return false;
        }

        // Content check - objects containing nested containers become multiline
        return std::all_of(obj.begin(), obj.end(), [](const auto& pair) {
            return pair.second.type() != JsonType::Array && pair.second.type() != JsonType::Object;
        });
    }

    [[nodiscard]] auto indent(int depth) const -> std::string {
        return std::string(static_cast<size_t>(depth * options_.indent_size.value_or(0)), ' ');
    }
};

} // namespace jsom
