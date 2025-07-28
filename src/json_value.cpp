#include "jsom/json_value.hpp"
#include "jsom/path_node.hpp"
#include <charconv>
#include <sstream>
#include <stdexcept>

namespace jsom {

JsonValue::JsonValue(std::string_view raw_data, PathNode* path)
    : raw_data_(raw_data), path_node_(path) {}

auto JsonValue::type() const -> JsonType {
    if (type_ == Unparsed) {
        type_ = parse_type();
    }
    return type_;
}

auto JsonValue::get_bool() const -> bool {
    if (type() != Bool) {
        throw std::runtime_error("JsonValue is not a boolean");
    }

    if (std::holds_alternative<bool>(parsed_value_)) {
        return std::get<bool>(parsed_value_);
    }

    bool result = parse_bool();
    parsed_value_ = result;
    return result;
}

auto JsonValue::get_number() const -> double {
    if (type() != Number) {
        throw std::runtime_error("JsonValue is not a number");
    }

    if (std::holds_alternative<double>(parsed_value_)) {
        return std::get<double>(parsed_value_);
    }

    double result = parse_number();
    parsed_value_ = result;
    return result;
}

auto JsonValue::get_string() const -> std::string {
    if (type() != String) {
        throw std::runtime_error("JsonValue is not a string");
    }

    if (std::holds_alternative<std::string>(parsed_value_)) {
        return std::get<std::string>(parsed_value_);
    }

    std::string result = parse_string();
    parsed_value_ = result;
    return result;
}

auto JsonValue::to_json() const -> std::string {
    switch (type()) {
    case Null:
        return "null";
    case Bool:
        return get_bool() ? "true" : "false";
    case Number:
        return std::to_string(get_number());
    case String: {
        std::ostringstream json;
        json << '"';
        // Re-escape the string for JSON output
        for (char ch : get_string()) {
            if (ch == '"') {
                json << "\\\"";
            } else if (ch == '\\') {
                json << "\\\\";
            } else if (ch == '\n') {
                json << "\\n";
            } else if (ch == '\r') {
                json << "\\r";
            } else if (ch == '\t') {
                json << "\\t";
            } else {
                json << ch;
            }
        }
        json << '"';
        return json.str();
    }
    case Object:
    case Array:
        // For now, return raw JSON for objects/arrays
        // Full reconstruction would require parsing the structure
        return std::string(raw_data_);
    case Unparsed:
        return std::string(raw_data_);
    }
    return "";
}

auto JsonValue::get_json_pointer() const -> std::string {
    return path_node_ ? path_node_->generate_json_pointer() : "";
}

auto JsonValue::parse_type() const -> JsonType {
    if (raw_data_.empty()) {
        return Null;
    }

    // Trim whitespace
    size_t start = 0;
    size_t end = raw_data_.size();
    while (start < end && std::isspace(raw_data_[start])) {
        ++start;
    }
    while (end > start && std::isspace(raw_data_[end - 1])) {
        --end;
    }

    if (start >= end) {
        return Null;
    }

    char first_char = raw_data_[start];

    // Check first character to determine type
    if (first_char == 'n' && raw_data_.substr(start, 4) == "null") {
        return Null;
    } else if (first_char == 't' || first_char == 'f') {
        return Bool;
    } else if (first_char == '"') {
        return String;
    } else if (first_char == '{') {
        return Object;
    } else if (first_char == '[') {
        return Array;
    } else if (first_char == '-' || std::isdigit(first_char)) {
        return Number;
    }

    return Unparsed;
}

auto JsonValue::parse_bool() const -> bool {
    std::string_view trimmed = raw_data_;

    // Simple trim
    while (!trimmed.empty() && std::isspace(trimmed.front())) {
        trimmed.remove_prefix(1);
    }
    while (!trimmed.empty() && std::isspace(trimmed.back())) {
        trimmed.remove_suffix(1);
    }

    if (trimmed == "true") {
        return true;
    } else if (trimmed == "false") {
        return false;
    }

    throw std::runtime_error("Invalid boolean value");
}

auto JsonValue::parse_number() const -> double {
    std::string_view trimmed = raw_data_;

    // Simple trim
    while (!trimmed.empty() && std::isspace(trimmed.front())) {
        trimmed.remove_prefix(1);
    }
    while (!trimmed.empty() && std::isspace(trimmed.back())) {
        trimmed.remove_suffix(1);
    }

    double result = 0.0;
    auto [ptr, ec] = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), result);

    if (ec != std::errc{}) {
        throw std::runtime_error("Invalid number format");
    }

    return result;
}

auto JsonValue::parse_string() const -> std::string {
    if (raw_data_.size() < 2 || raw_data_.front() != '"' || raw_data_.back() != '"') {
        throw std::runtime_error("Invalid string format");
    }

    // Extract content between quotes
    std::string_view content = raw_data_.substr(1, raw_data_.size() - 2);
    return unescape_string(content);
}

auto JsonValue::unescape_string(std::string_view escaped) const -> std::string {
    std::string result;
    result.reserve(escaped.size());

    for (size_t i = 0; i < escaped.size(); ++i) {
        if (escaped[i] == '\\' && i + 1 < escaped.size()) {
            char next = escaped[i + 1];
            switch (next) {
            case '"':
                result += '"';
                break;
            case '\\':
                result += '\\';
                break;
            case '/':
                result += '/';
                break;
            case 'b':
                result += '\b';
                break;
            case 'f':
                result += '\f';
                break;
            case 'n':
                result += '\n';
                break;
            case 'r':
                result += '\r';
                break;
            case 't':
                result += '\t';
                break;
            case 'u':
                // Unicode escape - simplified implementation
                if (i + 5 < escaped.size()) {
                    // For now, just include the raw unicode escape
                    result += "\\u";
                    result.append(escaped.substr(i + 2, 4));
                    i += 4;
                } else {
                    result += '\\';
                    result += next;
                }
                break;
            default:
                result += '\\';
                result += next;
                break;
            }
            ++i; // Skip the escaped character
        } else {
            result += escaped[i];
        }
    }

    return result;
}

auto JsonValue::get_object() const -> const std::map<std::string, JsonValue>& {
    throw std::runtime_error("Object parsing not yet implemented");
}

auto JsonValue::get_array() const -> const std::vector<JsonValue>& {
    throw std::runtime_error("Array parsing not yet implemented");
}

} // namespace jsom