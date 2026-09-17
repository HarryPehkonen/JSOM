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

    // NOLINTNEXTLINE(google-explicit-constructor)
    JsonDocument(bool value)
        : type_(JsonType::Boolean), storage_(value), path_cache_(nullptr) {}

    // NOLINTNEXTLINE(google-explicit-constructor)
    JsonDocument(int value)
        : type_(JsonType::Number), storage_(LazyNumber(value)), path_cache_(nullptr) {}

    // NOLINTNEXTLINE(google-explicit-constructor)
    JsonDocument(double value)
        : type_(JsonType::Number), storage_(LazyNumber(value)), path_cache_(nullptr) {}

    // NOLINTNEXTLINE(google-explicit-constructor)
    JsonDocument(const std::string& value)
        : type_(JsonType::String), storage_(value), path_cache_(nullptr) {}

    // NOLINTNEXTLINE(google-explicit-constructor)
    JsonDocument(const char* value)
        : type_(JsonType::String), storage_(std::string(value)), path_cache_(nullptr) {}

    // Prevent nullptr from calling const char* overload (would be UB via std::string(nullptr))
    // NOLINTNEXTLINE(google-explicit-constructor)
    JsonDocument(std::nullptr_t)
        : type_(JsonType::Null), storage_(std::monostate{}), path_cache_(nullptr) {}

    JsonDocument(std::initializer_list<std::pair<const std::string, JsonDocument>> init)
        : type_(JsonType::Object), storage_(std::map<std::string, JsonDocument>(init)),
          path_cache_(nullptr) {}

    // Direct container constructors - efficient when you already have JsonDocument containers
    explicit JsonDocument(std::map<std::string, JsonDocument> obj)
        : type_(JsonType::Object), storage_(std::move(obj)), path_cache_(nullptr) {}

    explicit JsonDocument(std::vector<JsonDocument> arr)
        : type_(JsonType::Array), storage_(std::move(arr)), path_cache_(nullptr) {}

    static auto from_lazy_number(const std::string& repr) -> JsonDocument {
        JsonDocument doc;
        doc.type_ = JsonType::Number;
        doc.storage_ = LazyNumber(repr);
        return doc;
    }

    // Template factory methods for converting containers to JsonDocument
    // Zero-overhead - converter is inlined by compiler
    template <typename T, typename Converter>
    static auto from_map(const std::map<std::string, T>& map, Converter converter) -> JsonDocument {
        std::map<std::string, JsonDocument> result;
        for (const auto& [key, value] : map) {
            result.emplace(key, converter(value));
        }
        return JsonDocument(std::move(result));
    }

    template <typename T, typename Converter>
    static auto from_vector(const std::vector<T>& vec, Converter converter) -> JsonDocument {
        std::vector<JsonDocument> result;
        result.reserve(vec.size());
        for (const auto& value : vec) {
            result.push_back(converter(value));
        }
        return JsonDocument(std::move(result));
    }

    // Convenience overloads for types that JsonDocument already accepts
    // Automatically converts int, double, bool, std::string without explicit converter
    template <typename T>
    static auto from_map(const std::map<std::string, T>& map) -> JsonDocument {
        return from_map(map, [](const T& v) { return JsonDocument(v); });
    }

    template <typename T> static auto from_vector(const std::vector<T>& vec) -> JsonDocument {
        return from_vector(vec, [](const T& v) { return JsonDocument(v); });
    }

    auto type() const -> JsonType { return type_; }

    auto is_null() const -> bool { return type_ == JsonType::Null; }
    auto is_bool() const -> bool { return type_ == JsonType::Boolean; }
    auto is_number() const -> bool { return type_ == JsonType::Number; }
    auto is_string() const -> bool { return type_ == JsonType::String; }

    /// Moves the stored string out of this document, leaving it null.
    /// Parser fast path (object keys, OPTIMIZATIONS.md #3): avoids the copy
    /// that as<std::string>() would make. Only valid on a String document.
    auto take_string() -> std::string {
        validate_type(JsonType::String);
        std::string out = std::move(std::get<std::string>(storage_));
        storage_ = std::monostate{};
        type_ = JsonType::Null;
        invalidate_cache();
        return out;
    }
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

    auto as_array() const -> const std::vector<JsonDocument>& {
        validate_type(JsonType::Array);
        return std::get<std::vector<JsonDocument>>(storage_);
    }

    auto as_object() const -> const std::map<std::string, JsonDocument>& {
        validate_type(JsonType::Object);
        return std::get<std::map<std::string, JsonDocument>>(storage_);
    }

    // Array iteration (range-for support)
    using iterator = std::vector<JsonDocument>::iterator;
    using const_iterator = std::vector<JsonDocument>::const_iterator;

    auto begin() -> iterator {
        validate_type(JsonType::Array);
        return std::get<std::vector<JsonDocument>>(storage_).begin();
    }

    auto end() -> iterator {
        validate_type(JsonType::Array);
        return std::get<std::vector<JsonDocument>>(storage_).end();
    }

    auto begin() const -> const_iterator {
        validate_type(JsonType::Array);
        return std::get<std::vector<JsonDocument>>(storage_).begin();
    }

    auto end() const -> const_iterator {
        validate_type(JsonType::Array);
        return std::get<std::vector<JsonDocument>>(storage_).end();
    }

    // Object iteration via items() (structured binding support)
    auto items() -> std::map<std::string, JsonDocument>& {
        validate_type(JsonType::Object);
        return std::get<std::map<std::string, JsonDocument>>(storage_);
    }

    auto items() const -> const std::map<std::string, JsonDocument>& {
        validate_type(JsonType::Object);
        return std::get<std::map<std::string, JsonDocument>>(storage_);
    }

    auto keys() const -> std::vector<std::string> {
        validate_type(JsonType::Object);
        const auto& obj = std::get<std::map<std::string, JsonDocument>>(storage_);
        std::vector<std::string> result;
        result.reserve(obj.size());
        for (const auto& entry : obj) {
            result.push_back(entry.first);
        }
        return result;
    }

    auto size() const -> std::size_t {
        if (type_ == JsonType::Array) {
            return std::get<std::vector<JsonDocument>>(storage_).size();
        }
        if (type_ == JsonType::Object) {
            return std::get<std::map<std::string, JsonDocument>>(storage_).size();
        }
        throw TypeException("size() requires array or object, got " + type_name(type_));
    }

    auto empty() const -> bool {
        if (type_ == JsonType::Null) {
            return true;
        }
        if (type_ == JsonType::Array) {
            return std::get<std::vector<JsonDocument>>(storage_).empty();
        }
        if (type_ == JsonType::Object) {
            return std::get<std::map<std::string, JsonDocument>>(storage_).empty();
        }
        throw TypeException("empty() requires null, array, or object, got " + type_name(type_));
    }

    auto contains(const std::string& key) const -> bool {
        validate_type(JsonType::Object);
        const auto& obj = std::get<std::map<std::string, JsonDocument>>(storage_);
        return obj.find(key) != obj.end();
    }

    // Mutation methods: set(), push_back()
    //
    // These invalidate the path cache (both this document's and the global epoch)
    // so that any ancestor's cache will detect the change and re-navigate.
    //
    // References and pointers obtained from operator[], at(), as_array(), or
    // as_object() follow standard C++ container rules: any mutation that causes
    // reallocation (push_back, set with resize on arrays) invalidates them.
    // Prefer set_at()/remove_at() from the root document for safe structural
    // changes when using JSON Pointer navigation.

    void push_back(const JsonDocument& value) {
        validate_type(JsonType::Array);
        std::get<std::vector<JsonDocument>>(storage_).push_back(value);
        invalidate_cache();
    }

    void push_back(JsonDocument&& value) {
        validate_type(JsonType::Array);
        std::get<std::vector<JsonDocument>>(storage_).push_back(std::move(value));
        invalidate_cache();
    }

    static auto make_array() -> JsonDocument {
        return JsonDocument(std::vector<JsonDocument>{});
    }

    static auto make_object() -> JsonDocument {
        return JsonDocument(std::map<std::string, JsonDocument>{});
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
        invalidate_cache();
    }

    void set(std::size_t index, const JsonDocument& value) {
        validate_type(JsonType::Array);
        auto& arr = std::get<std::vector<JsonDocument>>(storage_);
        if (index >= arr.size()) {
            arr.resize(index + 1);
        }
        arr[index] = value;
        invalidate_cache();
    }

    // rvalue overload (OPTIMIZATIONS.md #1): array parsing was deep-copying
    // every element because only the const& overload existed. Moving the
    // temporary in makes deeply nested documents O(depth) instead of
    // O(depth^2). See test DeepNestingParseIsLinear.
    void set(std::size_t index, JsonDocument&& value) {
        validate_type(JsonType::Array);
        auto& arr = std::get<std::vector<JsonDocument>>(storage_);
        if (index >= arr.size()) {
            arr.resize(index + 1);
        }
        arr[index] = std::move(value);
        invalidate_cache();
    }

    void set(const std::string& key, JsonDocument&& value) {
        validate_type(JsonType::Object);
        // insert_or_assign (not operator[] =): no null-document default
        // construction before the move-assign (OPTIMIZATIONS.md #6).
        std::get<std::map<std::string, JsonDocument>>(storage_).insert_or_assign(
            key, std::move(value));
        invalidate_cache();
    }

    void set(std::string&& key, JsonDocument&& value) {
        validate_type(JsonType::Object);
        std::get<std::map<std::string, JsonDocument>>(storage_).insert_or_assign(
            std::move(key), std::move(value));
        invalidate_cache();
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
    // Invalidate path cache after structural mutations
    void invalidate_cache();
    /// One check for every traversal in this class: refuse to recurse past the
    /// documented nesting limit rather than overflowing the stack. `level` is the
    /// node's own level, counting the root as 1 — the same convention the parser uses,
    /// so a document that parsed is a document that serializes and compares.
    static void check_traversal_depth(int level) {
        if (level > limits::MAX_NESTING_DEPTH) {
            throw_depth_error();
        }
    }

    /// Slow path, kept out of the inline check so a traversal costs one compare.
    [[noreturn]] static void throw_depth_error() {
        throw std::runtime_error(std::string(limits::MAX_NESTING_DEPTH_MESSAGE) + " (limit "
                                 + std::to_string(limits::MAX_NESTING_DEPTH) + ")");
    }

    /// Equality with an explicit nesting level. It cannot delegate to
    /// std::vector/std::map's own operator==, which has no depth budget: that is what
    /// used to overflow the stack (measured 2026-09-16: comparing two 20,000-deep
    /// documents kills a 1 MB stack, with no parsing involved at all). Level counts
    /// the root as 1, the same convention the parser uses.
    [[nodiscard]] auto equals_at(const JsonDocument& other, int level) const -> bool {
        check_traversal_depth(level);
        if (type_ != other.type_) {
            return false;
        }
        switch (type_) {
        case JsonType::Null:
            return true;
        case JsonType::Boolean:
            return std::get<bool>(storage_) == std::get<bool>(other.storage_);
        case JsonType::Number:
            return std::get<LazyNumber>(storage_) == std::get<LazyNumber>(other.storage_);
        case JsonType::String:
            return std::get<std::string>(storage_) == std::get<std::string>(other.storage_);
        case JsonType::Array: {
            const auto& lhs = std::get<std::vector<JsonDocument>>(storage_);
            const auto& rhs = std::get<std::vector<JsonDocument>>(other.storage_);
            if (lhs.size() != rhs.size()) {
                return false;
            }
            for (size_t i = 0; i < lhs.size(); ++i) {
                if (!lhs[i].equals_at(rhs[i], level + 1)) {
                    return false;
                }
            }
            return true;
        }
        case JsonType::Object: {
            const auto& lhs = std::get<std::map<std::string, JsonDocument>>(storage_);
            const auto& rhs = std::get<std::map<std::string, JsonDocument>>(other.storage_);
            if (lhs.size() != rhs.size()) {
                return false;
            }
            auto lhs_it = lhs.begin();
            auto rhs_it = rhs.begin();
            for (; lhs_it != lhs.end(); ++lhs_it, ++rhs_it) {
                if (lhs_it->first != rhs_it->first
                    || !lhs_it->second.equals_at(rhs_it->second, level + 1)) {
                    return false;
                }
            }
            return true;
        }
        }
        return false;
    }

    /// Ordering with an explicit nesting level, for the same reason as equals_at.
    [[nodiscard]] auto less_at(const JsonDocument& other, int level) const -> bool {
        check_traversal_depth(level);
        if (type_ != other.type_) {
            return static_cast<uint8_t>(type_) < static_cast<uint8_t>(other.type_);
        }
        switch (type_) {
        case JsonType::Null:
            return false; // null == null, never less
        case JsonType::Boolean:
            return !std::get<bool>(storage_) && std::get<bool>(other.storage_); // false < true
        case JsonType::Number:
            return std::get<LazyNumber>(storage_) < std::get<LazyNumber>(other.storage_);
        case JsonType::String:
            return std::get<std::string>(storage_) < std::get<std::string>(other.storage_);
        case JsonType::Array: {
            const auto& lhs = std::get<std::vector<JsonDocument>>(storage_);
            const auto& rhs = std::get<std::vector<JsonDocument>>(other.storage_);
            const size_t common = lhs.size() < rhs.size() ? lhs.size() : rhs.size();
            for (size_t i = 0; i < common; ++i) {
                if (!lhs[i].equals_at(rhs[i], level + 1)) {
                    return lhs[i].less_at(rhs[i], level + 1);
                }
            }
            return lhs.size() < rhs.size();
        }
        case JsonType::Object: {
            const auto& lhs = std::get<std::map<std::string, JsonDocument>>(storage_);
            const auto& rhs = std::get<std::map<std::string, JsonDocument>>(other.storage_);
            auto lhs_it = lhs.begin();
            auto rhs_it = rhs.begin();
            while (lhs_it != lhs.end() && rhs_it != rhs.end()) {
                if (lhs_it->first != rhs_it->first) {
                    return lhs_it->first < rhs_it->first;
                }
                if (!lhs_it->second.equals_at(rhs_it->second, level + 1)) {
                    return lhs_it->second.less_at(rhs_it->second, level + 1);
                }
                ++lhs_it;
                ++rhs_it;
            }
            return lhs.size() < rhs.size();
        }
        }
        return false;
    }

    // Highly optimized string-based serialization
    // NOLINTBEGIN(readability-function-size)
    void serialize_compact_to_string(std::string& out, int level = 1) const {
        check_traversal_depth(level);
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
            serialize_object_compact_to_string(out, level);
            break;
        case JsonType::Array:
            serialize_array_compact_to_string(out, level);
            break;
        }
    }
    // NOLINTEND(readability-function-size)

    // Optimized compact serialization (no pretty printing overhead)
    void serialize_compact(std::ostream& out, int level = 1) const {
        check_traversal_depth(level);
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
            serialize_object_compact(out, level);
            break;
        case JsonType::Array:
            serialize_array_compact(out, level);
            break;
        }
    }

    void serialize_object_to(std::ostream& out, bool pretty, int indent, int level) const {
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
            value.serialize_value(out, pretty, indent + 1, level + 1);
            first = false;
        }
        if (pretty && !obj.empty()) {
            out << '\n' << std::string(static_cast<size_t>(indent * 2), ' ');
        }
        out << '}';
    }

    void serialize_object_compact_to_string(std::string& out, int level) const {
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
            value.serialize_compact_to_string(out, level + 1);
            first = false;
        }
        out += '}';
    }

    void serialize_array_compact_to_string(std::string& out, int level) const {
        const auto& arr = std::get<std::vector<JsonDocument>>(storage_);
        out += '[';
        bool first = true;
        for (const auto& value : arr) {
            if (!first) {
                out += ',';
            }
            value.serialize_compact_to_string(out, level + 1);
            first = false;
        }
        out += ']';
    }

    void serialize_object_compact(std::ostream& out, int level) const {
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
            value.serialize_compact(out, level + 1);
            first = false;
        }
        out << '}';
    }

    void serialize_array_compact(std::ostream& out, int level) const {
        const auto& arr = std::get<std::vector<JsonDocument>>(storage_);
        out << '[';
        bool first = true;
        for (const auto& value : arr) {
            if (!first) {
                out << ',';
            }
            value.serialize_compact(out, level + 1);
            first = false;
        }
        out << ']';
    }

    void serialize_array_to(std::ostream& out, bool pretty, int indent, int level) const {
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
            value.serialize_value(out, pretty, indent + 1, level + 1);
            first = false;
        }
        if (pretty && !arr.empty()) {
            out << '\n' << std::string(static_cast<size_t>(indent * 2), ' ');
        }
        out << ']';
    }

    /// Pretty/indented serialization with an explicit nesting level: bounded by
    /// limits::MAX_NESTING_DEPTH like every other traversal.
    void serialize_value(std::ostream& out, bool pretty, int indent, int level) const {
        check_traversal_depth(level);
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
            serialize_object_to(out, pretty, indent, level);
            break;
        case JsonType::Array:
            serialize_array_to(out, pretty, indent, level);
            break;
        }
    }

public:
    void serialize_to(std::ostream& out, bool pretty, int indent = 0) const {
        serialize_value(out, pretty, indent, 1);
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

    // Comparison operators
    friend auto operator==(const JsonDocument& lhs, const JsonDocument& rhs) -> bool;
    friend auto operator<(const JsonDocument& lhs, const JsonDocument& rhs) -> bool;
};

// NOLINTBEGIN(readability-function-size)
inline auto operator==(const JsonDocument& lhs, const JsonDocument& rhs) -> bool {
    return lhs.equals_at(rhs, 1);
}

inline auto operator!=(const JsonDocument& lhs, const JsonDocument& rhs) -> bool {
    return !(lhs == rhs);
}

inline auto operator<(const JsonDocument& lhs, const JsonDocument& rhs) -> bool {
    return lhs.less_at(rhs, 1);
}
// NOLINTEND(readability-function-size)

inline auto operator>(const JsonDocument& lhs, const JsonDocument& rhs) -> bool {
    return rhs < lhs;
}

inline auto operator<=(const JsonDocument& lhs, const JsonDocument& rhs) -> bool {
    return !(rhs < lhs);
}

inline auto operator>=(const JsonDocument& lhs, const JsonDocument& rhs) -> bool {
    return !(lhs < rhs);
}

} // namespace jsom
