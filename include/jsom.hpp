#pragma once

// JSOM - JavaScript Object Model for C++
// A streaming JSON parser with JSON Pointer tracking and lazy evaluation
// https://github.com/user/jsom

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace jsom {

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

struct PathNode;
class JsonValue;
class IAllocator;
class StandardAllocator;
class ArenaAllocator;
class StreamingParser;

// ============================================================================
// PATH NODE - Bidirectional JSON Pointer path tracking
// ============================================================================

/**
 * Represents a single component in a JSON Pointer path with bidirectional navigation.
 * Forms a tree structure for efficient path reconstruction and JSON generation.
 */
struct PathNode {
    enum ComponentType : std::uint8_t { ObjectKey, ArrayIndex };

    ComponentType type;
    std::string_view component; // Points to key name or array index as string

    // Bidirectional navigation for JSON reconstruction
    PathNode* parent = nullptr;       // For upward traversal (JSON Pointer generation)
    PathNode* first_child = nullptr;  // For downward traversal (JSON reconstruction)
    PathNode* next_sibling = nullptr; // For iterating children

    // Constructors
    PathNode() = default;
    PathNode(ComponentType node_type, std::string_view comp, PathNode* parent_node = nullptr)
        : type(node_type), component(comp), parent(parent_node) {}

    // Navigation helpers
    auto add_child(ComponentType child_type, std::string_view child_component) -> PathNode* {
        // Create new child node
        auto* child = new PathNode(child_type, child_component, this);

        // Link into sibling chain
        if (first_child == nullptr) {
            first_child = child;
        } else {
            // Find last sibling and append
            PathNode* last_sibling = first_child;
            while (last_sibling->next_sibling != nullptr) {
                last_sibling = last_sibling->next_sibling;
            }
            last_sibling->next_sibling = child;
        }

        return child;
    }

    [[nodiscard]] auto find_child(std::string_view child_component) const -> PathNode* {
        for (PathNode* child = first_child; child != nullptr; child = child->next_sibling) {
            if (child->component == child_component) {
                return child;
            }
        }
        return nullptr;
    }

    // JSON Pointer generation
    // NOLINTBEGIN(readability-function-size)
    [[nodiscard]] auto generate_json_pointer() const -> std::string {
        if (is_root()) {
            return ""; // Root has empty pointer
        }

        // Build path components by walking up the tree
        std::vector<std::string_view> components;
        for (const PathNode* node = this; node != nullptr && !node->is_root();
             node = node->parent) {
            components.push_back(node->component);
        }

        // Reverse to get correct order (root to leaf)
        std::reverse(components.begin(), components.end());

        // Build JSON Pointer string with proper escaping
        std::ostringstream pointer;
        for (const auto& component : components) {
            pointer << '/';

            // RFC 6901 escaping: ~ becomes ~0, / becomes ~1
            for (char character : component) {
                if (character == '~') {
                    pointer << "~0";
                } else if (character == '/') {
                    pointer << "~1";
                } else {
                    pointer << character;
                }
            }
        }

        return pointer.str();
    }
    // NOLINTEND(readability-function-size)

    // Utility methods
    [[nodiscard]] auto is_root() const -> bool { return parent == nullptr; }

    [[nodiscard]] auto depth() const -> size_t {
        size_t level = 0;
        for (const PathNode* node = parent; node != nullptr; node = node->parent) {
            ++level;
        }
        return level;
    }
};

// ============================================================================
// JSON VALUE - Lazy-evaluated JSON values with type information
// ============================================================================

/**
 * Represents a JSON value with lazy evaluation and type information.
 * Supports all JSON types with on-demand parsing and proper string unescaping.
 */
class JsonValue {
public:
    enum JsonType : std::uint8_t { Null, Bool, Number, String, Object, Array, Unparsed };

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
    explicit JsonValue(std::string_view raw_data, PathNode* path = nullptr)
        : raw_data_(raw_data), path_node_(path) {}

    // Raw data access (no parsing required)
    [[nodiscard]] auto raw() const -> std::string_view { return raw_data_; }
    [[nodiscard]] auto path() const -> PathNode* { return path_node_; }

    // Type queries
    [[nodiscard]] auto type() const -> JsonType {
        if (type_ == Unparsed) {
            type_ = parse_type();
        }
        return type_;
    }

    [[nodiscard]] auto is_null() const -> bool { return type() == Null; }
    [[nodiscard]] auto is_bool() const -> bool { return type() == Bool; }
    [[nodiscard]] auto is_number() const -> bool { return type() == Number; }
    [[nodiscard]] auto is_string() const -> bool { return type() == String; }
    [[nodiscard]] auto is_object() const -> bool { return type() == Object; }
    [[nodiscard]] auto is_array() const -> bool { return type() == Array; }

    // Typed getters (triggers parsing if needed)
    [[nodiscard]] auto get_bool() const -> bool {
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

    [[nodiscard]] auto get_number() const -> double {
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

    [[nodiscard]] auto get_string() const -> std::string { // Handles unescaping
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

    [[nodiscard]] static auto get_object() -> const std::map<std::string, JsonValue>& {
        throw std::runtime_error("Object parsing not yet implemented");
    }

    [[nodiscard]] static auto get_array() -> const std::vector<JsonValue>& {
        throw std::runtime_error("Array parsing not yet implemented");
    }

    // String escape handling utilities (public for testing)
    static constexpr char NOT_SIMPLE_ESCAPE = '\0';

    [[nodiscard]] static auto handle_simple_escape(char escape_char) -> char {
        switch (escape_char) {
        case '"':
            return '"';
        case '\\':
            return '\\';
        case '/':
            return '/';
        case 'b':
            return '\b';
        case 'f':
            return '\f';
        case 'n':
            return '\n';
        case 'r':
            return '\r';
        case 't':
            return '\t';
        default:
            return NOT_SIMPLE_ESCAPE;
        }
    }

    [[nodiscard]] static auto unescape_string(std::string_view escaped) -> std::string;

    // JSON reconstruction
    [[nodiscard]] auto to_json() const -> std::string {
        switch (type()) {
        case Null:
            return std::string(NULL_LITERAL);
        case Bool:
            return get_bool() ? std::string(TRUE_LITERAL) : std::string(FALSE_LITERAL);
        case Number:
            return std::to_string(get_number());
        case String:
            return escape_for_json(get_string());
        case Object:
        case Array:
        case Unparsed:
            // For now, return raw JSON for objects/arrays/unparsed
            // Full reconstruction would require parsing the structure
            return std::string(raw_data_);
        }
        return "";
    }

    [[nodiscard]] auto get_json_pointer() const -> std::string {
        return (path_node_ != nullptr) ? path_node_->generate_json_pointer() : "";
    }

private:
    // JSON literal constants
    static constexpr std::string_view NULL_LITERAL = "null";
    static constexpr std::string_view TRUE_LITERAL = "true";
    static constexpr std::string_view FALSE_LITERAL = "false";

    // String parsing constants
    static constexpr size_t MIN_STRING_LENGTH = 2;  // Opening and closing quotes
    static constexpr size_t QUOTE_OFFSET = 1;       // Offset to skip opening quote
    static constexpr size_t UNICODE_HEX_LENGTH = 4; // Length of unicode hex digits

    // Utility function for trimming whitespace
    [[nodiscard]] static auto trim_whitespace(std::string_view data) -> std::string_view {
        if (data.empty()) {
            return data;
        }

        size_t start = 0;
        size_t end = data.size();

        while (start < end && (std::isspace(data[start]) != 0)) {
            ++start;
        }
        while (end > start && (std::isspace(data[end - 1]) != 0)) {
            --end;
        }

        return data.substr(start, end - start);
    }

    // Internal parsing methods
    // NOLINTBEGIN(readability-function-size)
    [[nodiscard]] auto parse_type() const -> JsonType {
        if (raw_data_.empty()) {
            return Null;
        }

        std::string_view trimmed = trim_whitespace(raw_data_);
        if (trimmed.empty()) {
            return Null;
        }

        char first_char = trimmed.front();

        // Check first character to determine type
        if (first_char == 'n' && trimmed.substr(0, NULL_LITERAL.length()) == NULL_LITERAL) {
            return Null;
        }
        if (first_char == 't' || first_char == 'f') {
            return Bool;
        }
        if (first_char == '"') {
            return String;
        }
        if (first_char == '{') {
            return Object;
        }
        if (first_char == '[') {
            return Array;
        }
        if (first_char == '-' || (std::isdigit(first_char) != 0)) {
            return Number;
        }

        return Unparsed;
    }
    // NOLINTEND(readability-function-size)

    [[nodiscard]] auto parse_bool() const -> bool {
        std::string_view trimmed = trim_whitespace(raw_data_);

        if (trimmed == TRUE_LITERAL) {
            return true;
        }
        if (trimmed == FALSE_LITERAL) {
            return false;
        }

        throw std::runtime_error("Invalid boolean value");
    }

    [[nodiscard]] auto parse_number() const -> double {
        std::string_view trimmed = trim_whitespace(raw_data_);

        double result = 0.0;
        auto [ptr, ec] = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), result);

        if (ec != std::errc{}) {
            throw std::runtime_error("Invalid number format");
        }

        return result;
    }

    [[nodiscard]] auto parse_string() const -> std::string { // Handles escape sequences
        if (raw_data_.size() < MIN_STRING_LENGTH || raw_data_.front() != '"'
            || raw_data_.back() != '"') {
            throw std::runtime_error("Invalid string format");
        }

        // Extract content between quotes
        std::string_view content
            = raw_data_.substr(QUOTE_OFFSET, raw_data_.size() - MIN_STRING_LENGTH);
        return unescape_string(content);
    }

    [[nodiscard]] static auto escape_for_json(const std::string& value) -> std::string {
        std::ostringstream json;
        json << '"';
        // Re-escape the string for JSON output
        for (char character : value) {
            switch (character) {
            case '"':
                json << "\\\"";
                break;
            case '\\':
                json << "\\\\";
                break;
            case '\n':
                json << "\\n";
                break;
            case '\r':
                json << "\\r";
                break;
            case '\t':
                json << "\\t";
                break;
            default:
                json << character;
                break;
            }
        }
        json << '"';
        return json.str();
    }

    // Unicode escape result structure
    struct UnicodeResult {
        std::string text; // The unescaped unicode content
        size_t length;    // Number of input characters consumed
    };

    // Helper function for processing unicode escape sequences
    // NOLINTBEGIN(readability-function-size)
    [[nodiscard]] static auto unescape_unicode(std::string_view sequence) -> UnicodeResult {
        // sequence starts with 'u' followed by potential hex digits
        if (sequence.empty() || sequence[0] != 'u') {
            // Invalid input - shouldn't happen in normal usage
            return {"u", 1};
        }

        // Check if we have enough characters for a complete unicode escape (u + 4 hex digits)
        static constexpr size_t UNICODE_ESCAPE_LENGTH = 5; // 'u' + 4 hex digits
        constexpr size_t REQUIRED_LENGTH = UNICODE_ESCAPE_LENGTH;
        if (sequence.length() >= REQUIRED_LENGTH) {
            // Extract the hex part and validate it
            std::string_view hex_part = sequence.substr(1, UNICODE_HEX_LENGTH);
            if (is_valid_hex_sequence(hex_part)) {
                // Convert hex string to codepoint
                static constexpr uint8_t HEX_DIGIT_A_OFFSET = 10; // 'A' = 10 in hex
                static constexpr uint8_t HEX_BITS_PER_DIGIT = 4;  // Each hex digit = 4 bits

                uint32_t codepoint = 0;
                for (char hex_char : hex_part) {
                    codepoint <<= HEX_BITS_PER_DIGIT;
                    if (hex_char >= '0' && hex_char <= '9') {
                        codepoint |= static_cast<uint32_t>(hex_char - '0');
                    } else if (hex_char >= 'A' && hex_char <= 'F') {
                        codepoint |= static_cast<uint32_t>(hex_char - 'A' + HEX_DIGIT_A_OFFSET);
                    } else if (hex_char >= 'a' && hex_char <= 'f') {
                        codepoint |= static_cast<uint32_t>(hex_char - 'a' + HEX_DIGIT_A_OFFSET);
                    }
                }

                // Convert codepoint to UTF-8
                std::string utf8_result = codepoint_to_utf8(codepoint);
                return {utf8_result, REQUIRED_LENGTH};
            }
        }

        // Incomplete or invalid unicode escape - fall back to treating it as literal
        // Return just 'u' and let the caller handle the backslash separately
        return {"u", 1};
    }
    // NOLINTEND(readability-function-size)

    // Helper function to validate if a string contains valid hex digits
    [[nodiscard]] static auto is_valid_hex_sequence(std::string_view hex_chars) -> bool {
        if (hex_chars.length() != UNICODE_HEX_LENGTH) {
            return false;
        }

        return std::all_of(hex_chars.cbegin(), hex_chars.cend(),
                           // NOLINTNEXTLINE(readability-identifier-length)
                           [](unsigned char c) { return isxdigit(c); });
    }

    // Helper function to convert Unicode codepoint to UTF-8 byte sequence
    // NOLINTBEGIN(readability-function-size)
    [[nodiscard]] static auto codepoint_to_utf8(uint32_t codepoint) -> std::string {
        // UTF-8 encoding constants
        static constexpr uint32_t UTF8_1_BYTE_MAX = 0x7F;     // ASCII range
        static constexpr uint32_t UTF8_2_BYTE_MAX = 0x7FF;    // 2-byte UTF-8 max
        static constexpr uint32_t UTF8_3_BYTE_MAX = 0xFFFF;   // 3-byte UTF-8 max
        static constexpr uint32_t UTF8_4_BYTE_MAX = 0x10FFFF; // Unicode max codepoint

        static constexpr uint8_t UTF8_2_BYTE_PREFIX = 0xC0; // 110xxxxx
        static constexpr uint8_t UTF8_3_BYTE_PREFIX = 0xE0; // 1110xxxx
        static constexpr uint8_t UTF8_4_BYTE_PREFIX = 0xF0; // 11110xxx
        static constexpr uint8_t UTF8_CONTINUATION = 0x80;  // 10xxxxxx
        static constexpr uint8_t UTF8_6_BIT_MASK = 0x3F;    // 111111

        // Bit shift constants
        static constexpr uint8_t UTF8_2_BYTE_SHIFT = 6;
        static constexpr uint8_t UTF8_3_BYTE_SHIFT = 12;
        static constexpr uint8_t UTF8_4_BYTE_SHIFT = 18;

        std::string result;

        if (codepoint <= UTF8_1_BYTE_MAX) {
            // ASCII range: 0xxxxxxx
            result += static_cast<char>(codepoint);
        } else if (codepoint <= UTF8_2_BYTE_MAX) {
            // 2-byte UTF-8: 110xxxxx 10xxxxxx
            result += static_cast<char>(UTF8_2_BYTE_PREFIX | (codepoint >> UTF8_2_BYTE_SHIFT));
            result += static_cast<char>(UTF8_CONTINUATION | (codepoint & UTF8_6_BIT_MASK));
        } else if (codepoint <= UTF8_3_BYTE_MAX) {
            // 3-byte UTF-8: 1110xxxx 10xxxxxx 10xxxxxx
            result += static_cast<char>(UTF8_3_BYTE_PREFIX | (codepoint >> UTF8_3_BYTE_SHIFT));
            result += static_cast<char>(UTF8_CONTINUATION
                                        | ((codepoint >> UTF8_2_BYTE_SHIFT) & UTF8_6_BIT_MASK));
            result += static_cast<char>(UTF8_CONTINUATION | (codepoint & UTF8_6_BIT_MASK));
        } else if (codepoint <= UTF8_4_BYTE_MAX) {
            // 4-byte UTF-8: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            result += static_cast<char>(UTF8_4_BYTE_PREFIX | (codepoint >> UTF8_4_BYTE_SHIFT));
            result += static_cast<char>(UTF8_CONTINUATION
                                        | ((codepoint >> UTF8_3_BYTE_SHIFT) & UTF8_6_BIT_MASK));
            result += static_cast<char>(UTF8_CONTINUATION
                                        | ((codepoint >> UTF8_2_BYTE_SHIFT) & UTF8_6_BIT_MASK));
            result += static_cast<char>(UTF8_CONTINUATION | (codepoint & UTF8_6_BIT_MASK));
        } else {
            // Invalid codepoint - return replacement character (U+FFFD)
            result += "\xEF\xBF\xBD"; // UTF-8 encoding of U+FFFD
        }

        return result;
    }
    // NOLINTEND(readability-function-size)
};

// Implementation of unescape_string (defined outside class for readability)
// NOLINTBEGIN(readability-function-size)
inline auto JsonValue::unescape_string(std::string_view escaped) -> std::string {
    std::string result;
    result.reserve(escaped.size());

    for (size_t i = 0; i < escaped.size(); ++i) {
        if (escaped[i] == '\\' && i + 1 < escaped.size()) {
            char next = escaped[i + 1];
            char simple_escape = handle_simple_escape(next);

            if (simple_escape != NOT_SIMPLE_ESCAPE) {
                result += simple_escape;
            } else if (next == 'u') {
                static constexpr size_t UNICODE_ESCAPE_LENGTH = 5; // 'u' + 4 hex digits
                auto unicode_result = unescape_unicode(escaped.substr(i + 1));
                if (unicode_result.length == UNICODE_ESCAPE_LENGTH) {
                    // Valid unicode sequence converted to UTF-8
                    result += unicode_result.text;
                } else {
                    // Invalid/incomplete sequence - add backslash + result
                    result += '\\';
                    result += unicode_result.text;
                }
                i += unicode_result.length - 1; // -1 because loop will increment i
            } else {
                // Unknown escape - add backslash + character
                result += '\\';
                result += next;
            }
            ++i; // Skip the escaped character
        } else {
            result += escaped[i];
        }
    }

    return result;
}
// NOLINTEND(readability-function-size)

// ============================================================================
// ALLOCATORS - Flexible memory management strategies
// ============================================================================

/**
 * Abstract allocator interface for flexible allocation strategies
 */
class IAllocator {
public:
    virtual ~IAllocator() = default;
    virtual auto allocate(size_t size, size_t alignment = alignof(std::max_align_t)) -> void* = 0;
    virtual void deallocate(void* ptr, size_t size) = 0;
    [[nodiscard]] virtual auto clone() const -> std::unique_ptr<IAllocator> = 0;
};

/**
 * Standard allocator wrapper - baseline implementation
 */
class StandardAllocator : public IAllocator {
public:
    auto allocate(size_t size, size_t /*alignment*/) -> void* override {
        void* ptr = std::malloc(size);
        if (ptr == nullptr) {
            throw std::bad_alloc();
        }
        return ptr;
    }

    void deallocate(void* ptr, size_t /*size*/) override { std::free(ptr); }

    [[nodiscard]] auto clone() const -> std::unique_ptr<IAllocator> override {
        return std::make_unique<StandardAllocator>();
    }
};

/**
 * Arena allocator for high-performance batch allocation
 * Ideal for PathNodes during parsing - allocate fast, deallocate all at once
 */
class ArenaAllocator : public IAllocator {
private:
    struct Chunk {
        std::unique_ptr<char[]> data; // NOLINT(modernize-avoid-c-arrays) - dynamic sizing required
        size_t size;
        size_t used = 0;

	// NOLINTBEGIN(modernize-avoid-c-arrays)
        explicit Chunk(size_t chunk_size)
            : data(std::make_unique<char[]>(chunk_size)), size(chunk_size) {
        }
	// NOLINTEND(modernize-avoid-c-arrays)

        // NOLINTNEXTLINE(bugprone-easily-swappable-parameters) - names are descriptive
        [[nodiscard]] auto has_space(size_t requested_size, size_t alignment) const -> bool {
            size_t aligned_used = ArenaAllocator::align_size(used, alignment);
            return aligned_used + requested_size <= size;
        }

        auto allocate_from_chunk(size_t requested_size, size_t alignment) -> void* {
            if (!has_space(requested_size, alignment)) {
                return nullptr;
            }

            used = align_size(used, alignment);
            void* ptr = data.get() + used;
            used += requested_size;

            return ptr;
        }
    };

    std::vector<Chunk> chunks_;
    size_t default_chunk_size_;
    size_t total_allocated_ = 0;

public:
    static constexpr size_t DEFAULT_CHUNK_SIZE = static_cast<size_t>(64 * 1024); // 64KB chunks

    explicit ArenaAllocator(size_t chunk_size = DEFAULT_CHUNK_SIZE)
        : default_chunk_size_(chunk_size) {
        constexpr size_t TYPICAL_CHUNK_COUNT = 8;
        chunks_.reserve(TYPICAL_CHUNK_COUNT); // Reserve space for typical usage
    }

    ~ArenaAllocator() override = default;

    // Non-copyable but movable
    ArenaAllocator(const ArenaAllocator&) = delete;
    auto operator=(const ArenaAllocator&) -> ArenaAllocator& = delete;
    ArenaAllocator(ArenaAllocator&&) = default;
    auto operator=(ArenaAllocator&&) -> ArenaAllocator& = default;

    auto allocate(size_t size, size_t alignment = alignof(std::max_align_t)) -> void* override {
        // Try to allocate from existing chunks
        for (auto& chunk : chunks_) {
            if (void* ptr = chunk.allocate_from_chunk(size, alignment)) {
                total_allocated_ += size;
                return ptr;
            }
        }

        // Need a new chunk
        size_t chunk_size = std::max(default_chunk_size_, size + alignment);
        Chunk& new_chunk = add_chunk(chunk_size);

        void* ptr = new_chunk.allocate_from_chunk(size, alignment);
        if (ptr == nullptr) {
            throw std::bad_alloc();
        }

        total_allocated_ += size;
        return ptr;
    }

    void deallocate(void* /*ptr*/, size_t /*size*/) override {} // No-op for arena

    [[nodiscard]] auto clone() const -> std::unique_ptr<IAllocator> override {
        return std::make_unique<ArenaAllocator>(default_chunk_size_);
    }

    // Arena-specific methods
    void reset() { // Deallocate all memory at once
        chunks_.clear();
        total_allocated_ = 0;
    }

    [[nodiscard]] auto total_allocated() const -> size_t { return total_allocated_; }
    [[nodiscard]] auto chunk_count() const -> size_t { return chunks_.size(); }

private:
    auto add_chunk(size_t min_size) -> Chunk& {
        size_t chunk_size = std::max(default_chunk_size_, min_size);
        chunks_.emplace_back(chunk_size);
        return chunks_.back();
    }

    static auto align_size(size_t size, size_t alignment) -> size_t {
        return (size + alignment - 1) & ~(alignment - 1);
    }
};

/**
 * Allocator-aware deleter for use with unique_ptr
 */
template <typename T> class AllocatorDeleter {
private:
    IAllocator* allocator_;

public:
    explicit AllocatorDeleter(IAllocator* alloc) : allocator_(alloc) {}

    void operator()(T* ptr) {
        if (ptr != nullptr && allocator_ != nullptr) {
            ptr->~T();
            allocator_->deallocate(ptr, sizeof(T));
        }
    }
};

/**
 * Convenience alias for allocator-aware unique_ptr
 */
template <typename T> using AllocatorUniquePtr = std::unique_ptr<T, AllocatorDeleter<T>>;

/**
 * Factory function for creating objects with custom allocators
 */
template <typename T, typename... Args>
auto make_allocated(IAllocator& allocator, Args&&... args) -> AllocatorUniquePtr<T> {
    void* ptr = allocator.allocate(sizeof(T), alignof(T));
    T* typed_ptr = new (ptr) T(std::forward<Args>(args)...);
    return AllocatorUniquePtr<T>(typed_ptr, AllocatorDeleter<T>(&allocator));
}

// ============================================================================
// STREAMING PARSER - Character-by-character JSON parsing with JSON Pointer tracking
// ============================================================================

/**
 * Event-driven callbacks for streaming JSON parsing
 */
struct ParseEvents {
    std::function<void(const std::string&, const JsonValue&)> on_value;
    std::function<void(const std::string&)> on_enter_object;
    std::function<void(const std::string&)> on_enter_array;
    std::function<void(const std::string&)> on_exit_container;
    std::function<void(size_t position, const std::string&)> on_error;
};

/**
 * Streaming JSON parser with JSON Pointer tracking
 * Character-by-character parsing with minimal memory overhead
 */
class StreamingParser {
private:
    std::unique_ptr<IAllocator> allocator_;
    PathNode* root_node_ = nullptr;
    PathNode* current_node_ = nullptr;

    // Parser state
    enum State : std::uint8_t { 
        Start, 
        InObject, 
        InObjectKey,     // Parsing object key string
        InObjectColon,   // Expecting ':' after key
        InObjectValue,   // Parsing object value
        InArray, 
        InArrayValue,    // Parsing array element
        InString, 
        InNumber, 
        InLiteral 
    };
    State current_state_ = Start;

    // Event callbacks
    ParseEvents events_;

    // Parsing buffer
    std::string input_buffer_;
    size_t current_position_ = 0;

    // Value parsing state
    std::string current_value_buffer_;
    size_t value_start_position_ = 0;
    
    // Object parsing state
    std::string current_object_key_;

public:
    explicit StreamingParser(std::unique_ptr<IAllocator> allocator = nullptr)
        : allocator_(std::move(allocator)) {
        initialize_allocator();

        // Create root node
        root_node_ = reinterpret_cast<PathNode*>(
            allocator_->allocate(sizeof(PathNode), alignof(PathNode)));
        new (root_node_) PathNode();
        current_node_ = root_node_;
    }

    ~StreamingParser() { reset(); }

    // Non-copyable but movable
    StreamingParser(const StreamingParser&) = delete;
    auto operator=(const StreamingParser&) -> StreamingParser& = delete;
    StreamingParser(StreamingParser&&) = default;
    auto operator=(StreamingParser&&) -> StreamingParser& = default;

    // Configuration
    void set_events(const ParseEvents& events) { events_ = events; }

    // Parsing interface
    void parse_string(const std::string& json) {
        input_buffer_ = json;
        current_position_ = 0;

        for (char character : json) {
            try {
                process_character(character);
            } catch (const std::exception& e) {
                if (events_.on_error) {
                    events_.on_error(current_position_, e.what());
                }
                return;
            }
            ++current_position_;
        }

        // Signal end of input
        try {
            process_character(std::nullopt);
        } catch (const std::exception& e) {
            if (events_.on_error) {
                events_.on_error(current_position_, e.what());
            }
        }
    }

    void feed_character(char character) {
        try {
            process_character(character);
            ++current_position_;
        } catch (const std::exception& e) {
            if (events_.on_error) {
                events_.on_error(current_position_, e.what());
            }
        }
    }

    void end_input() {
        try {
            process_character(std::nullopt);
        } catch (const std::exception& e) {
            if (events_.on_error) {
                events_.on_error(current_position_, e.what());
            }
        }
    }

    void reset() {
        if (allocator_) {
            // Arena allocator can reset all memory at once
            if (auto* arena = dynamic_cast<ArenaAllocator*>(allocator_.get())) {
                arena->reset();
            }
            // Note: StandardAllocator would need individual deallocations
        }

        root_node_ = nullptr;
        current_node_ = nullptr;
        current_state_ = Start;
        input_buffer_.clear();
        current_position_ = 0;
        current_value_buffer_.clear();
        value_start_position_ = 0;
    }

    // Path iteration (for accessing all discovered paths)
    class PathIterator {
        // TODO: Implement iterator over all paths
    };

    static auto begin() -> PathIterator {
        // TODO: Implement
        return PathIterator{};
    }

    static auto end() -> PathIterator {
        // TODO: Implement
        return PathIterator{};
    }

    // Statistics
    [[nodiscard]] static auto total_paths() -> size_t {
        // TODO: Implement - traverse tree and count nodes
        return 0;
    }

    [[nodiscard]] auto memory_usage() const -> size_t {
        if (auto* arena = dynamic_cast<ArenaAllocator*>(allocator_.get())) {
            return arena->total_allocated();
        }
        return 0; // Can't track StandardAllocator usage easily
    }

private:
    void initialize_allocator() {
        if (!allocator_) {
            allocator_ = std::make_unique<ArenaAllocator>();
        }
    }

    void process_character(std::optional<char> input) {
        if (!input.has_value()) {
            handle_eof();
            return;
        }

        char character = input.value();

        switch (current_state_) {
        case Start:
            handle_start_state(character);
            break;
        case InString:
            handle_string_state(character);
            break;
        case InNumber:
            handle_number_state(character);
            break;
        case InLiteral:
            handle_literal_state(character);
            break;
        case InObject:
            handle_object_state(character);
            break;
        case InObjectKey:
            handle_object_key_state(character); // Keys are strings with special completion
            break;
        case InObjectColon:
            handle_object_colon_state(character);
            break;
        case InObjectValue:
            handle_object_value_state(character);
            break;
        case InArray:
            handle_array_state(character);
            break;
        case InArrayValue:
            handle_array_value_state(character);
            break;
        }
    }

    void handle_eof() {
        if (current_state_ == InNumber || current_state_ == InLiteral) {
            complete_value_parsing();
            current_state_ = Start;
        }
        // Note: If we're in InString, InObjectKey, or container states at EOF, 
        // that's an error (unclosed structures). For now, we'll just ignore it,
        // but we could emit an error
    }

    void handle_start_state(char character) {
        if (std::isspace(character) != 0) {
            return; // Skip whitespace
        }

        if (character == '"') {
            start_value_with_state(character, InString);
        } else if (character == '{') {
            current_state_ = InObject;
            if (events_.on_enter_object) {
                events_.on_enter_object("");
            }
        } else if (character == '[') {
            current_state_ = InArray;
            if (events_.on_enter_array) {
                events_.on_enter_array("");
            }
        } else if (character == '-' || (std::isdigit(character) != 0)) {
            start_value_with_state(character, InNumber);
        } else if (std::isalpha(character) != 0) {
            start_value_with_state(character, InLiteral);
        }
    }

    void handle_string_state(char character) {
        current_value_buffer_ += character;
        if (character == '"' && !is_escaped()) {
            complete_value_and_restart();
        }
    }

    void handle_number_state(char character) {
        if (is_number_character(character)) {
            current_value_buffer_ += character;
        } else {
            complete_value_and_restart();
            process_character(character); // Re-process current character
        }
    }

    void handle_literal_state(char character) {
        if (std::isalpha(character) != 0) {
            current_value_buffer_ += character;
        } else {
            complete_value_and_restart();
            process_character(character); // Re-process current character
        }
    }

    void handle_object_state(char character) {
        if (std::isspace(character) != 0) {
            return; // Skip whitespace in objects
        }

        if (character == '}') {
            // Check if this is an empty object or closing after key-value pairs
            if (current_node_ && current_node_->first_child != nullptr) {
                // Object with content - emit object representation and return to parent
                complete_populated_object();
            } else {
                // Empty object - emit the object value and return to Start state
                complete_empty_container("{}");
            }
        } else if (character == '"') {
            // Start parsing object key
            start_value_with_state(character, InObjectKey);
        } else if (character == ',') {
            // Continue to next key-value pair (ignore for now)
            // TODO: Handle multiple key-value pairs
        } else {
            // Invalid character in object context
            // For now, ignore (could emit error)
        }
    }

    void handle_array_state(char character) {
        if (std::isspace(character) != 0) {
            return; // Skip whitespace in arrays  
        }

        if (character == ']') {
            // Empty array - emit the array value and return to Start state
            complete_empty_container("[]");
        } else {
            // TODO: Handle array elements
            // For now, just ignore non-closing characters
        }
    }

    void handle_object_key_state(char character) {
        current_value_buffer_ += character;
        if (character == '"' && !is_escaped()) {
            // Object key completed - store it and transition to colon state
            complete_object_key();
            current_state_ = InObjectColon;
        }
    }

    void handle_object_colon_state(char character) {
        if (std::isspace(character) != 0) {
            return; // Skip whitespace before/after colon
        }

        if (character == ':') {
            // Found colon, transition to object value parsing
            current_state_ = InObjectValue;
        } else {
            // Invalid character - expected colon
            // For now, ignore (could emit error)
        }
    }

    void handle_object_value_state(char character) {
        if (std::isspace(character) != 0) {
            return; // Skip whitespace before value
        }

        // Delegate to start state logic for value parsing
        if (character == '"') {
            start_value_with_state(character, InString);
        } else if (character == '-' || (std::isdigit(character) != 0)) {
            start_value_with_state(character, InNumber);
        } else if (std::isalpha(character) != 0) {
            start_value_with_state(character, InLiteral);
        } else if (character == '{') {
            current_state_ = InObject;
            if (events_.on_enter_object) {
                std::string pointer = current_node_ ? current_node_->generate_json_pointer() : "";
                events_.on_enter_object(pointer);
            }
        } else if (character == '[') {
            current_state_ = InArray;
            if (events_.on_enter_array) {
                std::string pointer = current_node_ ? current_node_->generate_json_pointer() : "";
                events_.on_enter_array(pointer);
            }
        }
    }

    void handle_array_value_state(char character) {
        // Similar to handle_object_value_state but for array elements
        if (std::isspace(character) != 0) {
            return; // Skip whitespace before value
        }

        // Delegate to start state logic for value parsing
        if (character == '"') {
            start_value_with_state(character, InString);
        } else if (character == '-' || (std::isdigit(character) != 0)) {
            start_value_with_state(character, InNumber);
        } else if (std::isalpha(character) != 0) {
            start_value_with_state(character, InLiteral);
        } else if (character == '{') {
            current_state_ = InObject;
            if (events_.on_enter_object) {
                std::string pointer = current_node_ ? current_node_->generate_json_pointer() : "";
                events_.on_enter_object(pointer);
            }
        } else if (character == '[') {
            current_state_ = InArray;
            if (events_.on_enter_array) {
                std::string pointer = current_node_ ? current_node_->generate_json_pointer() : "";
                events_.on_enter_array(pointer);
            }
        }
    }

    void complete_object_key() {
        // Extract the key string (remove surrounding quotes)
        if (current_value_buffer_.size() >= 2 && 
            current_value_buffer_.front() == '"' && 
            current_value_buffer_.back() == '"') {
            current_object_key_ = current_value_buffer_.substr(1, current_value_buffer_.size() - 2);
            // Unescape the key if needed
            current_object_key_ = JsonValue::unescape_string(current_object_key_);
        } else {
            current_object_key_ = current_value_buffer_; // Fallback
        }
        
        // Clear the value buffer for the upcoming value
        current_value_buffer_.clear();
    }

    void complete_populated_object() {
        // For now, emit a simple object representation
        // TODO: Generate proper JSON representation from PathNode children
        std::string pointer = current_node_ ? current_node_->generate_json_pointer() : "";
        JsonValue value("{...}", current_node_); // Placeholder representation
        emit_value(pointer, value);
        current_state_ = Start;
        
        // TODO: Handle proper parent context restoration for nested objects
    }

    void complete_empty_container(const std::string& container_json) {
        // Set up the value buffer as if we parsed the container
        current_value_buffer_ = container_json;
        value_start_position_ = 0;
        
        // Use the standard value parsing completion
        complete_value_parsing();
        current_state_ = Start;
        
        // TODO: When we implement nested structures, we may need to exit object/array
        // and restore parent state instead of going to Start
    }

    void start_value_with_state(char character, State new_state) {
        start_value_parsing();
        current_value_buffer_ += character;
        current_state_ = new_state;
    }

    void complete_value_and_restart() {
        complete_value_parsing();
        
        // Determine next state based on context
        if (!current_object_key_.empty()) {
            // We just completed an object value - expect } or ,
            current_state_ = InObject;
            current_object_key_.clear(); // Clear for next key-value pair
        } else {
            current_state_ = Start;
        }
    }

    [[nodiscard]] static auto is_number_character(char character) -> bool {
        return std::isdigit(character) != 0 || character == '.' || character == 'e'
               || character == 'E' || character == '+' || character == '-';
    }

    void start_value_parsing() {
        current_value_buffer_.clear();
        value_start_position_ = current_position_;
    }

    void complete_value_parsing() {
        // Handle object key context
        PathNode* value_node = current_node_;
        if (!current_object_key_.empty()) {
            // Create a new PathNode for this object key
            value_node = current_node_->add_child(PathNode::ObjectKey, current_object_key_);
        }
        
        // Generate JSON Pointer for current context
        std::string pointer = (value_node != nullptr) ? value_node->generate_json_pointer() : "";

        // For streaming, we need to store the value in input_buffer_ to keep it valid
        if (input_buffer_.empty()) {
            // Streaming mode - copy buffer to input_buffer_ to make it persistent
            input_buffer_ = current_value_buffer_;
            value_start_position_ = 0;
        }

        // Create JsonValue using substring of input_buffer_ (now always populated)
        size_t value_length
            = input_buffer_.empty() ? 0 : (input_buffer_.size() - value_start_position_);
        std::string_view value_data
            = std::string_view(input_buffer_).substr(value_start_position_, value_length);
        JsonValue value(value_data, value_node);

        // Emit the value
        emit_value(pointer, value);

        // Clear the buffer for next value
        current_value_buffer_.clear();
    }

    [[nodiscard]] auto is_escaped() const -> bool {
        // Check if current position has escaped character
        // Look for odd number of preceding backslashes
        if (current_value_buffer_.size() < 2) {
            return false;
        }

        size_t backslash_count = 0;
        for (size_t i = current_value_buffer_.size() - 2; i > 0; --i) {
            if (current_value_buffer_[i] == '\\') {
                ++backslash_count;
            } else {
                break;
            }
        }

        // Odd number of backslashes means the quote is escaped
        return (backslash_count % 2) == 1;
    }

    void emit_value(const std::string& pointer, const JsonValue& value) const {
        if (events_.on_value) {
            events_.on_value(pointer, value);
        }
    }
};

} // namespace jsom
