#pragma once

#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace jsom {

// Forward declaration
struct PathNode;

/**
 * Represents a JSON value with lazy evaluation and type information.
 * Supports all JSON types with on-demand parsing and proper string unescaping.
 */
class JsonValue {
public:
    enum JsonType { Null, Bool, Number, String, Object, Array, Unparsed };

    using ParsedValue = std::variant<std::nullptr_t,                   // Null
                                     bool,                             // Bool
                                     double,                           // Number
                                     std::string,                      // String (unescaped)
                                     std::map<std::string, JsonValue>, // Object
                                     std::vector<JsonValue>            // Array
                                     >;

private:
    std::string_view raw_data_;        // Always available from input buffer
    mutable JsonType type_ = Unparsed; // Lazy type determination
    mutable ParsedValue parsed_value_; // Parsed on-demand
    PathNode* path_node_ = nullptr;    // Associated path for JSON Pointer

public:
    // Constructors
    JsonValue() = default;
    explicit JsonValue(std::string_view raw_data, PathNode* path = nullptr);

    // Raw data access (no parsing required)
    auto raw() const -> std::string_view { return raw_data_; }
    auto path() const -> PathNode* { return path_node_; }

    // Type queries
    auto type() const -> JsonType;
    auto is_null() const -> bool { return type() == Null; }
    auto is_bool() const -> bool { return type() == Bool; }
    auto is_number() const -> bool { return type() == Number; }
    auto is_string() const -> bool { return type() == String; }
    auto is_object() const -> bool { return type() == Object; }
    auto is_array() const -> bool { return type() == Array; }

    // Typed getters (triggers parsing if needed)
    auto get_bool() const -> bool;
    auto get_number() const -> double;
    auto get_string() const -> std::string; // Handles unescaping
    auto get_object() const -> const std::map<std::string, JsonValue>&;
    auto get_array() const -> const std::vector<JsonValue>&;

    // JSON reconstruction
    auto to_json() const -> std::string;
    auto get_json_pointer() const -> std::string;

private:
    // Internal parsing methods
    auto parse_type() const -> JsonType;
    auto parse_bool() const -> bool;
    auto parse_number() const -> double;
    auto parse_string() const -> std::string; // Handles escape sequences
    auto unescape_string(std::string_view escaped) const -> std::string;
};

} // namespace jsom