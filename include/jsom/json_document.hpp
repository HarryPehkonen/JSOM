#pragma once

#include "constants.hpp"
#include "core_types.hpp"
#include <array>
#include <cstdio>
#include <initializer_list>
#include <iomanip>
#include <map>
#include <memory>
#include <sstream>
#include <variant>
#include <vector>

namespace jsom {

// Forward declarations for path functionality
class NavigationEngine;
struct NavigationResult;

// Forward declaration for PathCache - actual include happens after JsonDocument declaration
class PathCache;

// Forward declaration for formatting
struct JsonFormatOptions;
class JsonFormatter;

class JsonDocument;

using JsonStorage = std::variant<std::monostate,                      // null
                                 bool,                                // boolean
                                 LazyNumber,                          // number with lazy evaluation
                                 std::string,                         // string
                                 std::map<std::string, JsonDocument>, // object
                                 std::vector<JsonDocument>            // array
                                 >;

class JsonDocument {
    friend class DocumentBuilder;
    friend class NavigationEngine;
    friend class JsonFormatter;
    friend class FastParser;

private:
    JsonType type_;
    JsonStorage storage_;

    // Path cache for this document instance (managed manually to avoid forward declaration issues)
    mutable PathCache* path_cache_;

    void validate_type(JsonType expected) const {
        if (type_ != expected) {
            throw TypeException("Invalid type access - expected " + type_name(expected)
                                + " but got " + type_name(type_));
        }
    }

    static auto type_name(JsonType type) -> std::string {
        switch (type) {
        case JsonType::Null:
            return "null";
        case JsonType::Boolean:
            return "boolean";
        case JsonType::Number:
            return "number";
        case JsonType::String:
            return "string";
        case JsonType::Object:
            return "object";
        case JsonType::Array:
            return "array";
        }
        return "unknown";
    }

public:
    JsonDocument() : type_(JsonType::Null), storage_(std::monostate{}), path_cache_(nullptr) {}

    // Destructor
    ~JsonDocument();

    // Copy constructor
    JsonDocument(const JsonDocument& other)
        : type_(other.type_), storage_(other.storage_), path_cache_(nullptr) {
        // path_cache_ is left as nullptr - will be created lazily if needed
    }

    // Move constructor
    JsonDocument(JsonDocument&& other) noexcept
        : type_(other.type_), storage_(std::move(other.storage_)), path_cache_(other.path_cache_) {
        other.path_cache_ = nullptr; // Transfer ownership
    }

    // Copy assignment
    // Copy assignment - defined in implementation file
    auto operator=(const JsonDocument& other) -> JsonDocument&;

    // Move assignment - defined in implementation file
    auto operator=(JsonDocument&& other) noexcept -> JsonDocument&;

    explicit JsonDocument(bool value)
        : type_(JsonType::Boolean), storage_(value), path_cache_(nullptr) {}

    explicit JsonDocument(int value)
        : type_(JsonType::Number), storage_(LazyNumber(value)), path_cache_(nullptr) {}

    explicit JsonDocument(double value)
        : type_(JsonType::Number), storage_(LazyNumber(value)), path_cache_(nullptr) {}

    explicit JsonDocument(std::string value)
        : type_(JsonType::String), storage_(std::move(value)), path_cache_(nullptr) {}

    explicit JsonDocument(const char* value)
        : type_(JsonType::String), storage_(std::string(value)), path_cache_(nullptr) {}

    JsonDocument(std::initializer_list<std::pair<const std::string, JsonDocument>> init)
        : type_(JsonType::Object), storage_(std::map<std::string, JsonDocument>(init)),
          path_cache_(nullptr) {}

    JsonDocument(std::initializer_list<JsonDocument> init)
        : type_(JsonType::Array), storage_(std::vector<JsonDocument>(init)), path_cache_(nullptr) {}

    static auto from_lazy_number(const std::string& repr) -> JsonDocument {
        JsonDocument doc;
        doc.type_ = JsonType::Number;
        doc.storage_ = LazyNumber(repr);
        return doc;
    }

    auto type() const -> JsonType { return type_; }

    auto is_null() const -> bool { return type_ == JsonType::Null; }
    auto is_bool() const -> bool { return type_ == JsonType::Boolean; }
    auto is_number() const -> bool { return type_ == JsonType::Number; }
    auto is_string() const -> bool { return type_ == JsonType::String; }
    auto is_object() const -> bool { return type_ == JsonType::Object; }
    auto is_array() const -> bool { return type_ == JsonType::Array; }

    template <typename T> auto as() const -> T {
        if constexpr (std::is_same_v<T, bool>) {
            validate_type(JsonType::Boolean);
            return std::get<bool>(storage_);
        } else if constexpr (std::is_same_v<T, int>) {
            validate_type(JsonType::Number);
            return std::get<LazyNumber>(storage_).as_int();
        } else if constexpr (std::is_same_v<T, long long>) {
            validate_type(JsonType::Number);
            return std::get<LazyNumber>(storage_).as_long_long();
        } else if constexpr (std::is_same_v<T, double>) {
            validate_type(JsonType::Number);
            return std::get<LazyNumber>(storage_).as_double();
        } else if constexpr (std::is_same_v<T, std::string>) {
            validate_type(JsonType::String);
            return std::get<std::string>(storage_);
        } else if constexpr (std::is_same_v<T, std::map<std::string, JsonDocument>>) {
            validate_type(JsonType::Object);
            return std::get<std::map<std::string, JsonDocument>>(storage_);
        } else if constexpr (std::is_same_v<T, std::vector<JsonDocument>>) {
            validate_type(JsonType::Array);
            return std::get<std::vector<JsonDocument>>(storage_);
        } else {
            static_assert(std::is_same_v<T, void>, "Unsupported type for as<T>()");
        }
    }

    template <typename T> auto try_as() const -> std::optional<T> {
        try {
            return as<T>();
        } catch (const TypeException&) {
            return std::nullopt;
        }
    }

    auto operator[](const std::string& key) -> JsonDocument& {
        validate_type(JsonType::Object);
        auto& obj = std::get<std::map<std::string, JsonDocument>>(storage_);
        // NOLINTNEXTLINE(readability-identifier-length)
        auto it = obj.find(key);
        if (it == obj.end()) {
            throw std::out_of_range("Key '" + key + "' not found in object");
        }
        return it->second;
    }

    auto operator[](const std::string& key) const -> const JsonDocument& {
        validate_type(JsonType::Object);
        const auto& obj = std::get<std::map<std::string, JsonDocument>>(storage_);
        // NOLINTNEXTLINE(readability-identifier-length)
        auto it = obj.find(key);
        if (it == obj.end()) {
            throw std::out_of_range("Key '" + key + "' not found in object");
        }
        return it->second;
    }

    auto operator[](std::size_t index) -> JsonDocument& {
        validate_type(JsonType::Array);
        auto& arr = std::get<std::vector<JsonDocument>>(storage_);
        if (index >= arr.size()) {
            throw std::out_of_range("Array index " + std::to_string(index) + " out of range");
        }
        return arr[index];
    }

    auto operator[](std::size_t index) const -> const JsonDocument& {
        validate_type(JsonType::Array);
        const auto& arr = std::get<std::vector<JsonDocument>>(storage_);
        if (index >= arr.size()) {
            throw std::out_of_range("Array index " + std::to_string(index) + " out of range");
        }
        return arr[index];
    }

    void set(const std::string& key, const JsonDocument& value) {
        validate_type(JsonType::Object);
        std::get<std::map<std::string, JsonDocument>>(storage_)[key] = value;
    }

    void set(std::size_t index, const JsonDocument& value) {
        validate_type(JsonType::Array);
        auto& arr = std::get<std::vector<JsonDocument>>(storage_);
        if (index >= arr.size()) {
            arr.resize(index + 1);
        }
        arr[index] = value;
    }

    void set(const std::string& key, JsonDocument&& value) {
        validate_type(JsonType::Object);
        std::get<std::map<std::string, JsonDocument>>(storage_)[key] = std::move(value);
    }

    void set(std::string&& key, JsonDocument&& value) {
        validate_type(JsonType::Object);
        std::get<std::map<std::string, JsonDocument>>(storage_).emplace(std::move(key),
                                                                        std::move(value));
    }

    auto to_json() const -> std::string {
        std::string result;
        result.reserve(
            parser_constants::JSON_DOCUMENT_INITIAL_SIZE); // Pre-allocate reasonable size
        serialize_compact_to_string(result);
        return result;
    }

    auto to_json(bool pretty) const -> std::string {
        std::ostringstream oss;
        serialize_to(oss, pretty, 0);
        return oss.str();
    }

    // Advanced formatting with full options control
    auto to_json(const JsonFormatOptions& options) const -> std::string;

    // JSON Pointer support (RFC 6901)
    // These methods activate path functionality lazily - zero cost if not used

    // Get the JSON Pointer path for this node
    static auto get_json_pointer() -> std::string;
    static auto get_path() -> std::string { return get_json_pointer(); } // Alias

    // Navigate to path (const version)
    auto at(const std::string& json_pointer) const -> const JsonDocument&;

    // Navigate to path (non-const version)
    auto at(const std::string& json_pointer) -> JsonDocument&;

    // Safe navigation (returns nullptr if not found)
    auto find(const std::string& json_pointer) const -> const JsonDocument*;
    auto find(const std::string& json_pointer) -> JsonDocument*;

    // Check if path exists
    auto exists(const std::string& json_pointer) const -> bool;
    auto has_path(const std::string& json_pointer) const -> bool { return exists(json_pointer); }

    // Path manipulation (modify document structure)
    void set_at(const std::string& json_pointer, const JsonDocument& value);
    void set_at(const std::string& json_pointer, JsonDocument&& value);
    auto remove_at(const std::string& json_pointer) -> bool;
    auto extract_at(const std::string& json_pointer) -> JsonDocument; // Remove and return

    // Batch operations for efficiency
    auto at_multiple(const std::vector<std::string>& paths) const
        -> std::vector<const JsonDocument*>;
    auto at_multiple(const std::vector<std::string>& paths) -> std::vector<JsonDocument*>;
    auto exists_multiple(const std::vector<std::string>& paths) const -> std::vector<bool>;

    // Path introspection
    auto list_paths(int max_depth = -1) const -> std::vector<std::string>;
    auto find_paths(const std::string& pattern) const -> std::vector<std::string>;
    auto count_paths() const -> size_t;

    // Performance tuning for path operations
    void precompute_paths(int max_depth = cache_constants::DEFAULT_PRECOMPUTE_DEPTH) const;
    void warm_path_cache(const std::vector<std::string>& likely_paths) const;
    void clear_path_cache() const;

    // Path cache statistics
    struct PathCacheStats {
        size_t exact_cache_size;
        size_t prefix_cache_size;
        size_t total_entries;
        size_t memory_usage_estimate;
        double avg_prefix_length;
    };

    auto get_path_cache_stats() const -> PathCacheStats;

private:
    // Get or create path cache for this document
    auto get_path_cache() const -> PathCache&;
    // Highly optimized string-based serialization
    // NOLINTBEGIN(readability-function-size)
    void serialize_compact_to_string(std::string& out) const {
        switch (type_) {
        case JsonType::Null:
            out += "null";
            break;
        case JsonType::Boolean:
            out += std::get<bool>(storage_) ? "true" : "false";
            break;
        case JsonType::Number: {
            const auto& num = std::get<LazyNumber>(storage_);
            if (num.has_original_repr()) {
                out += num.get_original_repr();
            } else {
                // Fallback to stream-based (rare case)
                std::ostringstream oss;
                num.serialize(oss);
                out += oss.str();
            }
            break;
        }
        case JsonType::String:
            out += '"';
            escape_string_to_string(out, std::get<std::string>(storage_));
            out += '"';
            break;
        case JsonType::Object:
            serialize_object_compact_to_string(out);
            break;
        case JsonType::Array:
            serialize_array_compact_to_string(out);
            break;
        }
    }
    // NOLINTEND(readability-function-size)

    // Optimized compact serialization (no pretty printing overhead)
    void serialize_compact(std::ostream& out) const {
        switch (type_) {
        case JsonType::Null:
            out << "null";
            break;
        case JsonType::Boolean:
            out << (std::get<bool>(storage_) ? "true" : "false");
            break;
        case JsonType::Number:
            std::get<LazyNumber>(storage_).serialize(out);
            break;
        case JsonType::String:
            out << '"';
            escape_string(out, std::get<std::string>(storage_));
            out << '"';
            break;
        case JsonType::Object:
            serialize_object_compact(out);
            break;
        case JsonType::Array:
            serialize_array_compact(out);
            break;
        }
    }

    void serialize_object_to(std::ostream& out, bool pretty, int indent) const {
        const auto& obj = std::get<std::map<std::string, JsonDocument>>(storage_);
        out << '{';
        bool first = true;
        for (const auto& [key, value] : obj) {
            if (!first) {
                out << ',';
            }
            if (pretty) {
                out << '\n' << std::string(static_cast<size_t>((indent + 1) * 2), ' ');
            }
            out << '"';
            escape_string(out, key);
            out << "\":";
            if (pretty) {
                out << ' ';
            }
            value.serialize_to(out, pretty, indent + 1);
            first = false;
        }
        if (pretty && !obj.empty()) {
            out << '\n' << std::string(static_cast<size_t>(indent * 2), ' ');
        }
        out << '}';
    }

    void serialize_object_compact_to_string(std::string& out) const {
        const auto& obj = std::get<std::map<std::string, JsonDocument>>(storage_);
        out += '{';
        bool first = true;
        for (const auto& [key, value] : obj) {
            if (!first) {
                out += ',';
            }
            out += '"';
            escape_string_to_string(out, key);
            out += "\":";
            value.serialize_compact_to_string(out);
            first = false;
        }
        out += '}';
    }

    void serialize_array_compact_to_string(std::string& out) const {
        const auto& arr = std::get<std::vector<JsonDocument>>(storage_);
        out += '[';
        bool first = true;
        for (const auto& value : arr) {
            if (!first) {
                out += ',';
            }
            value.serialize_compact_to_string(out);
            first = false;
        }
        out += ']';
    }

    void serialize_object_compact(std::ostream& out) const {
        const auto& obj = std::get<std::map<std::string, JsonDocument>>(storage_);
        out << '{';
        bool first = true;
        for (const auto& [key, value] : obj) {
            if (!first) {
                out << ',';
            }
            out << '"';
            escape_string(out, key);
            out << "\":";
            value.serialize_compact(out);
            first = false;
        }
        out << '}';
    }

    void serialize_array_compact(std::ostream& out) const {
        const auto& arr = std::get<std::vector<JsonDocument>>(storage_);
        out << '[';
        bool first = true;
        for (const auto& value : arr) {
            if (!first) {
                out << ',';
            }
            value.serialize_compact(out);
            first = false;
        }
        out << ']';
    }

    void serialize_array_to(std::ostream& out, bool pretty, int indent) const {
        const auto& arr = std::get<std::vector<JsonDocument>>(storage_);
        out << '[';
        bool first = true;
        for (const auto& value : arr) {
            if (!first) {
                out << ',';
            }
            if (pretty) {
                out << '\n' << std::string(static_cast<size_t>((indent + 1) * 2), ' ');
            }
            value.serialize_to(out, pretty, indent + 1);
            first = false;
        }
        if (pretty && !arr.empty()) {
            out << '\n' << std::string(static_cast<size_t>(indent * 2), ' ');
        }
        out << ']';
    }

public:
    void serialize_to(std::ostream& out, bool pretty, int indent = 0) const {
        switch (type_) {
        case JsonType::Null:
            out << "null";
            break;
        case JsonType::Boolean:
            out << (std::get<bool>(storage_) ? "true" : "false");
            break;
        case JsonType::Number:
            std::get<LazyNumber>(storage_).serialize(out);
            break;
        case JsonType::String:
            out << '"';
            escape_string(out, std::get<std::string>(storage_));
            out << '"';
            break;
        case JsonType::Object:
            serialize_object_to(out, pretty, indent);
            break;
        case JsonType::Array:
            serialize_array_to(out, pretty, indent);
            break;
        }
    }

    // NOLINTBEGIN(readability-function-size)
    static void escape_string_to_string(std::string& out, const std::string& str) {
        // Fast path: check if string needs escaping
        bool needs_escaping = false;
        // NOLINTNEXTLINE(readability-identifier-length)
        for (char c : str) {
            // NOLINTNEXTLINE(readability-magic-numbers)
            if (c == '"' || c == '\\'
                || static_cast<unsigned char>(c) < character_constants::MIN_CONTROL_CHAR) {
                needs_escaping = true;
                break;
            }
        }

        if (!needs_escaping) {
            // Fast path: append directly
            out += str;
            return;
        }

        // Slow path: escape character by character
        // NOLINTNEXTLINE(readability-identifier-length)
        for (char c : str) {
            switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                // NOLINTNEXTLINE(readability-magic-numbers)
                if (static_cast<unsigned char>(c) < character_constants::MIN_CONTROL_CHAR) {
                    std::array<char, character_constants::UNICODE_BUFFER_SIZE> buf{};
                    std::sprintf(buf.data(), "\\u%04x", static_cast<unsigned>(c));
                    out += buf.data();
                } else {
                    out += c;
                }
                break;
            }
        }
    }
    // NOLINTEND(readability-function-size)

    // NOLINTBEGIN(readability-function-size)
    static void escape_string(std::ostream& out, const std::string& str) {
        // Fast path: check if string needs escaping
        bool needs_escaping = false;
        // NOLINTNEXTLINE(readability-identifier-length)
        for (char c : str) {
            if (c == '"' || c == '\\'
                || static_cast<unsigned char>(c) < character_constants::MIN_CONTROL_CHAR) {
                needs_escaping = true;
                break;
            }
        }

        if (!needs_escaping) {
            // Fast path: output directly
            out << str;
            return;
        }

        // Slow path: escape character by character
        // NOLINTNEXTLINE(readability-identifier-length)
        for (char c : str) {
            switch (c) {
            case '"':
                out << "\\\"";
                break;
            case '\\':
                out << "\\\\";
                break;
            case '\b':
                out << "\\b";
                break;
            case '\f':
                out << "\\f";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                if (static_cast<unsigned char>(c) < character_constants::MIN_CONTROL_CHAR) {
                    out << "\\u" << std::hex << std::setfill('0') << std::setw(4)
                        << static_cast<unsigned>(c);
                } else {
                    out << c;
                }
                break;
            }
        }
    }
    // NOLINTEND(readability-function-size)
};

} // namespace jsom
