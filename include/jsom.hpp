#pragma once

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace jsom {

namespace utf8 {
// UTF-8 byte type markers and masks
constexpr std::uint8_t ASCII_MASK = 0x80;
constexpr std::uint8_t TWO_BYTE_MASK = 0xE0;
constexpr std::uint8_t TWO_BYTE_MARKER = 0xC0;
constexpr std::uint8_t THREE_BYTE_MASK = 0xF0;
constexpr std::uint8_t THREE_BYTE_MARKER = 0xE0;
constexpr std::uint8_t FOUR_BYTE_MASK = 0xF8;
constexpr std::uint8_t FOUR_BYTE_MARKER = 0xF0;
constexpr std::uint8_t CONTINUATION_MASK = 0xC0;
constexpr std::uint8_t CONTINUATION_MARKER = 0x80;
constexpr std::uint8_t DATA_BITS_MASK = 0x3F;

// Unicode codepoint ranges
constexpr std::uint32_t ASCII_MAX = 0x7F;
constexpr std::uint32_t TWO_BYTE_MAX = 0x7FF;
constexpr std::uint32_t THREE_BYTE_MAX = 0xFFFF;
constexpr std::uint32_t UNICODE_MAX = 0x10FFFF;

// Bit shift amounts for UTF-8 encoding
constexpr int SHIFT_6_BITS = 6;
constexpr int SHIFT_12_BITS = 12;
constexpr int SHIFT_18_BITS = 18;

// Hex conversion
constexpr int HEX_LETTER_OFFSET = 10;

// NOLINTNEXTLINE(readability-identifier-length)
inline auto is_valid_utf8_start(unsigned char c) -> bool {
    return (c & ASCII_MASK) == 0 ||                      // ASCII (0xxxxxxx)
           (c & TWO_BYTE_MASK) == TWO_BYTE_MARKER ||     // 2-byte (110xxxxx)
           (c & THREE_BYTE_MASK) == THREE_BYTE_MARKER || // 3-byte (1110xxxx)
           (c & FOUR_BYTE_MASK) == FOUR_BYTE_MARKER;     // 4-byte (11110xxx)
}

// NOLINTNEXTLINE(readability-identifier-length)
inline auto is_utf8_continuation(unsigned char c) -> bool {
    return (c & CONTINUATION_MASK) == CONTINUATION_MARKER; // 10xxxxxx
}

// NOLINTNEXTLINE(readability-identifier-length)
inline auto utf8_sequence_length(unsigned char c) -> int {
    if ((c & ASCII_MASK) == 0) {
        return 1; // ASCII
    }
    if ((c & TWO_BYTE_MASK) == TWO_BYTE_MARKER) {
        return 2; // 2-byte
    }
    if ((c & THREE_BYTE_MASK) == THREE_BYTE_MARKER) {
        return 3; // 3-byte
    }
    if ((c & FOUR_BYTE_MASK) == FOUR_BYTE_MARKER) {
        return 4; // 4-byte
    }
    return 0; // Invalid
}

inline auto is_valid_hex_string(const std::string& hex_chars) -> bool {
    return std::all_of(hex_chars.cbegin(), hex_chars.cend(),
                       // NOLINTNEXTLINE(readability-identifier-length)
                       [](unsigned char c) { return std::isxdigit(c) != 0; });
}

inline auto hex_to_uint32(const std::string& hex) -> std::uint32_t {
    std::uint32_t result = 0;
    // NOLINTNEXTLINE(readability-identifier-length)
    for (char c : hex) {
        result <<= 4;
        if (c >= '0' && c <= '9') {
            result |= (c - '0');
        } else if (c >= 'A' && c <= 'F') {
            result |= (c - 'A' + HEX_LETTER_OFFSET);
        } else if (c >= 'a' && c <= 'f') {
            result |= (c - 'a' + HEX_LETTER_OFFSET);
        }
        // Note: This function assumes valid hex input
        // Use is_valid_hex_string() to validate before calling
    }
    return result;
}

inline auto unicode_to_utf8(std::uint32_t codepoint) -> std::string {
    std::string result;

    if (codepoint <= ASCII_MAX) {
        result += static_cast<char>(codepoint);
    } else if (codepoint <= TWO_BYTE_MAX) {
        result += static_cast<char>(TWO_BYTE_MARKER | (codepoint >> SHIFT_6_BITS));
        result += static_cast<char>(CONTINUATION_MARKER | (codepoint & DATA_BITS_MASK));
    } else if (codepoint <= THREE_BYTE_MAX) {
        result += static_cast<char>(THREE_BYTE_MARKER | (codepoint >> SHIFT_12_BITS));
        result += static_cast<char>(CONTINUATION_MARKER
                                    | ((codepoint >> SHIFT_6_BITS) & DATA_BITS_MASK));
        result += static_cast<char>(CONTINUATION_MARKER | (codepoint & DATA_BITS_MASK));
    } else if (codepoint <= UNICODE_MAX) {
        result += static_cast<char>(FOUR_BYTE_MARKER | (codepoint >> SHIFT_18_BITS));
        result += static_cast<char>(CONTINUATION_MARKER
                                    | ((codepoint >> SHIFT_12_BITS) & DATA_BITS_MASK));
        result += static_cast<char>(CONTINUATION_MARKER
                                    | ((codepoint >> SHIFT_6_BITS) & DATA_BITS_MASK));
        result += static_cast<char>(CONTINUATION_MARKER | (codepoint & DATA_BITS_MASK));
    }

    return result;
}
} // namespace utf8

// Default allocator block size
constexpr std::size_t DEFAULT_ARENA_BLOCK_SIZE = 4096;

// Maximum literal length (for "false" which is 5 characters)
constexpr std::size_t MAX_LITERAL_LENGTH = 5;

// ASCII control character threshold (characters below 0x20 must be escaped)
constexpr std::uint8_t ASCII_CONTROL_THRESHOLD = 0x20;

class JsonEvent;
struct PathNode;
struct ParseError;

enum class JsonType : std::uint8_t { Null, Boolean, Number, String, Object, Array };

enum class ContainerType : std::uint8_t { Object, Array };

enum class ParseState : std::uint8_t {
    Start,
    InString,
    InNumber,
    InLiteral,
    InObject,
    InArray,
    Error,
    InUnicodeEscape
};


struct ParseEvents {
    std::function<void(const JsonEvent&)> on_value;
    std::function<void(const std::string&)> on_enter_object;
    std::function<void()> on_enter_array;
    std::function<void()> on_exit_container;
    std::function<void(const ParseError&)> on_error;
};

struct ParseError {
    std::size_t position;
    std::string message;
    std::string json_pointer;
};

class IAllocator {
public:
    virtual ~IAllocator() = default;
    virtual auto allocate(std::size_t size) -> void* = 0;
    virtual void deallocate(void* ptr, std::size_t size) = 0;
    virtual void reset() = 0;
};

class StandardAllocator : public IAllocator {
public:
    auto allocate(std::size_t size) -> void* override { return std::malloc(size); }

    void deallocate(void* ptr, std::size_t size) override {
        (void)size;
        std::free(ptr);
    }

    void reset() override {}
};

class ArenaAllocator : public IAllocator {
private:
    struct Block {
        // NOLINTNEXTLINE(modernize-avoid-c-arrays) - Raw memory for performance
        std::unique_ptr<char[]> data;
        std::size_t size;
        std::size_t used;
    };

    std::vector<Block> blocks_;
    std::size_t block_size_;

public:
    explicit ArenaAllocator(std::size_t block_size = DEFAULT_ARENA_BLOCK_SIZE)
        : block_size_(block_size) {}

    auto allocate(std::size_t size) -> void* override {
        if (blocks_.empty() || blocks_.back().used + size > blocks_.back().size) {
            std::size_t alloc_size = std::max(size, block_size_);
            blocks_.emplace_back();
            // NOLINTNEXTLINE(modernize-avoid-c-arrays) - Raw memory for performance
            blocks_.back().data = std::make_unique<char[]>(alloc_size);
            blocks_.back().size = alloc_size;
            blocks_.back().used = 0;
        }

        Block& current = blocks_.back();
        void* ptr = current.data.get() + current.used;
        current.used += size;
        return ptr;
    }

    void deallocate(void* ptr, std::size_t size) override {
        (void)ptr;
        (void)size;
    }

    void reset() override { blocks_.clear(); }
};

struct PathNode {
    PathNode* parent;
    std::vector<PathNode*> children;
    std::string component;

    PathNode(PathNode* parent_node, std::string path_component)
        : parent(parent_node), component(std::move(path_component)) {}

    [[nodiscard]] auto generate_json_pointer() const -> std::string {
        if (parent == nullptr) {
            return "";
        }

        std::vector<std::string> components;
        const PathNode* current = this;
        while (current != nullptr && !current->component.empty()) {
            std::string escaped = current->component;
            std::size_t pos = 0;
            while ((pos = escaped.find('~', pos)) != std::string::npos) {
                escaped.replace(pos, 1, "~0");
                pos += 2;
            }
            pos = 0;
            while ((pos = escaped.find('/', pos)) != std::string::npos) {
                escaped.replace(pos, 1, "~1");
                pos += 2;
            }
            components.push_back(escaped);
            current = current->parent;
        }

        std::string result;
        for (auto it = components.rbegin(); it != components.rend(); ++it) {
            result += "/" + *it;
        }
        return result;
    }
};

struct ParseContext {
    PathNode* node;
    ContainerType type;
    bool expecting_key;
    bool expecting_colon{false};
    std::size_t array_index{0};
    std::string key; // Object key for this context level

    ParseContext(PathNode* path_node, ContainerType container_type)
        : node(path_node), type(container_type),
          expecting_key(container_type == ContainerType::Object) {}
};

class JsonEvent {
private:
    JsonType type_;
    std::string raw_value_;
    PathNode* path_node_;
    std::string path_override_;

public:
    JsonEvent(JsonType type, std::string value, PathNode* node)
        : type_(type), raw_value_(std::move(value)), path_node_(node) {}

    JsonEvent(JsonType type, std::string value, std::string path)
        : type_(type), raw_value_(std::move(value)), path_node_(nullptr),
          path_override_(std::move(path)) {}

    [[nodiscard]] auto type() const -> JsonType { return type_; }
    [[nodiscard]] auto raw_value() const -> const std::string& { return raw_value_; }
    [[nodiscard]] auto path() const -> std::string {
        if (!path_override_.empty()) {
            return path_override_;
        }
        return path_node_ != nullptr ? path_node_->generate_json_pointer() : "";
    }

    [[nodiscard]] auto as_bool() const -> bool { return raw_value_ == "true"; }

    [[nodiscard]] auto as_string() const -> const std::string& { return raw_value_; }

    [[nodiscard]] auto as_double() const -> double { return std::stod(raw_value_); }

    [[nodiscard]] auto as_int() const -> int { return std::stoi(raw_value_); }
};

class StreamingParser {
private:
    ParseState state_{ParseState::Start};
    std::vector<ParseContext> context_stack_;
    std::string value_buffer_;
    ParseEvents events_;
    std::unique_ptr<IAllocator> allocator_;
    PathNode* root_node_;
    std::size_t position_{0};
    bool in_escape_{false};
    std::string unicode_buffer_;
    std::string current_key_;
    mutable std::string cached_json_pointer_;             // Cached JSON Pointer path
    mutable std::size_t last_context_size_{0};            // Track when to invalidate cache
    mutable std::string last_current_key_;                // Track key changes
    mutable std::vector<std::size_t> last_array_indices_; // Track array index changes
    bool has_pushed_back_char_{false};
    char pushed_back_char_;

public:
    explicit StreamingParser(std::unique_ptr<IAllocator> alloc
                             = std::make_unique<StandardAllocator>())
        : allocator_(std::move(alloc)) {
        root_node_ = static_cast<PathNode*>(allocator_->allocate(sizeof(PathNode)));
        new (root_node_) PathNode(nullptr, "");
    }

    ~StreamingParser() {
        if (root_node_ != nullptr) {
            root_node_->~PathNode();
            allocator_->deallocate(root_node_, sizeof(PathNode));
        }
    }

    // NOLINTNEXTLINE(readability-identifier-length)
    void feed_character(char c) {
        if (has_pushed_back_char_) {
            // Process the pushed back character first
            char pushed_char = pushed_back_char_;
            has_pushed_back_char_ = false;
            process_character_in_state(pushed_char);
        }
        ++position_;
        process_character_in_state(c);
    }

private:
    // NOLINTNEXTLINE(readability-identifier-length)
    void process_character_in_state(char c) {
        switch (state_) {
        case ParseState::Start:
            handle_start_state(c);
            break;
        case ParseState::InLiteral:
            handle_literal_state(c);
            break;
        case ParseState::InNumber:
            handle_number_state(c);
            break;
        case ParseState::InString:
            handle_string_state(c);
            break;
        case ParseState::InObject:
            handle_object_state(c);
            break;
        case ParseState::InArray:
            handle_array_state(c);
            break;
        case ParseState::Error:
            handle_error_state(c);
            break;
        case ParseState::InUnicodeEscape:
            handle_unicode_escape_state(c);
            break;
        }
    }

    // NOLINTBEGIN(readability-function-size)
    // NOLINTNEXTLINE(readability-identifier-length)
    void handle_start_state(char c) {
        // Skip whitespace
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            return;
        }

        if (c == '"') {
            // Start of string
            state_ = ParseState::InString;
            value_buffer_.clear();
        } else if (c == 't' || c == 'f' || c == 'n') {
            // Start of literal (true, false, null)
            state_ = ParseState::InLiteral;
            value_buffer_.clear();
            value_buffer_.push_back(c);
        } else if (c == '-' || (c >= '0' && c <= '9')) {
            // Start of number
            state_ = ParseState::InNumber;
            value_buffer_.clear();
            value_buffer_.push_back(c);
        } else if (c == '{') {
            // Start of object
            handle_object_opening();
        } else if (c == '[') {
            // Start of array
            handle_array_opening();
        } else if (c == '}') {
            // End of object
            handle_object_closing();
        } else if (c == ']') {
            // End of array
            handle_array_closing();
        } else if (c == ':') {
            // Colon separator
            handle_colon_separator();
        } else {
            emit_error("Unexpected character in start state");
        }
    }
    // NOLINTEND(readability-function-size)

    // NOLINTNEXTLINE(readability-identifier-length)
    void handle_literal_state(char c) {
        value_buffer_.push_back(c);

        // Check if we have a complete literal
        if (value_buffer_ == "true" || value_buffer_ == "false" || value_buffer_ == "null") {
            complete_literal_value();
        } else if (value_buffer_.size() > MAX_LITERAL_LENGTH || !is_valid_literal_prefix()) {
            // Invalid literal - too long or invalid prefix
            emit_error("Invalid literal");
            state_ = ParseState::Start;
            value_buffer_.clear();
        }
    }

    // NOLINTNEXTLINE(readability-identifier-length)
    void handle_number_state(char c) {
        if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
            // Valid number characters (digits and scientific notation characters)
            value_buffer_.push_back(c);
        } else {
            // End of number - emit value and push back the terminating character
            complete_number_value();
            // Push back the terminating character to be processed in the correct state
            push_back_character(c);
        }
    }

    [[nodiscard]] auto is_valid_literal_prefix() const -> bool {
        return value_buffer_ == "t" || value_buffer_ == "tr" || value_buffer_ == "tru"
               || value_buffer_ == "f" || value_buffer_ == "fa" || value_buffer_ == "fal"
               || value_buffer_ == "fals" || value_buffer_ == "n" || value_buffer_ == "nu"
               || value_buffer_ == "nul";
    }

    void complete_literal_value() {
        JsonType type;
        if (value_buffer_ == "true" || value_buffer_ == "false") {
            type = JsonType::Boolean;
        } else if (value_buffer_ == "null") {
            type = JsonType::Null;
        } else {
            emit_error("Internal error: invalid literal completed");
            return;
        }

        emit_value(type, value_buffer_);
        update_object_state_after_value();
        return_to_container_or_start();
        value_buffer_.clear();
    }

    void complete_number_value() {
        emit_value(JsonType::Number, value_buffer_);
        update_object_state_after_value();
        return_to_container_or_start();
        value_buffer_.clear();
    }

    // NOLINTNEXTLINE(readability-identifier-length)
    void handle_string_state(char c) {
        if (in_escape_) {
            handle_escape_sequence(c);
        } else if (c == '\\') {
            // Start escape sequence
            in_escape_ = true;
        } else if (c == '"') {
            // End of string
            complete_string_value();
        } else if (static_cast<unsigned char>(c) < ASCII_CONTROL_THRESHOLD) {
            // Control characters must be escaped in JSON strings
            emit_error("Unescaped control character in string");
            state_ = ParseState::Error;
            value_buffer_.clear();
            in_escape_ = false;
        } else {
            // Regular character
            value_buffer_.push_back(c);
        }
    }

    // NOLINTNEXTLINE(readability-identifier-length)
    void handle_escape_sequence(char c) {
        if (c == 'u') {
            // Start Unicode escape sequence
            handle_unicode_escape();
            return;
        }

        char escaped_char = get_escaped_character(c);
        if (escaped_char == '\0') {
            // Invalid escape sequence
            emit_error("Invalid escape sequence");
            state_ = ParseState::Error;
            value_buffer_.clear();
            in_escape_ = false;
            return;
        }

        value_buffer_.push_back(escaped_char);
        in_escape_ = false;
    }

    // NOLINTNEXTLINE(readability-identifier-length)
    [[nodiscard]] static auto get_escaped_character(char c) -> char {
        switch (c) {
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
            return '\0'; // Invalid escape
        }
    }

    void handle_unicode_escape() {
        // Start collecting 4 hex digits
        state_ = ParseState::InUnicodeEscape;
        unicode_buffer_.clear();
        in_escape_ = false;
    }

    // NOLINTNEXTLINE(readability-identifier-length)
    void handle_unicode_escape_state(char c) {
        if (std::isxdigit(static_cast<unsigned char>(c)) != 0) {
            unicode_buffer_.push_back(c);

            if (unicode_buffer_.size() == 4) {
                // We have 4 hex digits, convert to UTF-8
                complete_unicode_escape();
            }
        } else {
            emit_error("Invalid Unicode escape sequence: expected hex digit");
            state_ = ParseState::Error;
            unicode_buffer_.clear();
        }
    }

    void complete_unicode_escape() {
        // Convert 4 hex digits to Unicode codepoint
        std::uint32_t codepoint = utf8::hex_to_uint32(unicode_buffer_);

        // Convert codepoint to UTF-8 string and add to value buffer
        std::string utf8_sequence = utf8::unicode_to_utf8(codepoint);
        value_buffer_ += utf8_sequence;

        // Return to string parsing state
        state_ = ParseState::InString;
        unicode_buffer_.clear();
    }

    // NOLINTNEXTLINE(readability-identifier-length)
    void handle_error_state(char c) {
        // In error state, consume characters until we find a recovery point
        // For string errors, we skip until we find a quote or whitespace
        if (c == '"' || c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            state_ = ParseState::Start;
        }
        // Otherwise, just consume and ignore the character
    }

    void complete_string_value() {
        // Check if this string is an object key
        if (!context_stack_.empty() && context_stack_.back().type == ContainerType::Object
            && context_stack_.back().expecting_key) {

            // This is an object key - store it and expect colon next
            current_key_ = value_buffer_;
            context_stack_.back().expecting_key = false;
            context_stack_.back().expecting_colon = true;
        } else {
            // This is a regular string value
            emit_value(JsonType::String, value_buffer_);
            update_object_state_after_value();
        }

        return_to_container_or_start();
        value_buffer_.clear();
        in_escape_ = false;
    }

    void emit_value(JsonType type, const std::string& value) {
        if (events_.on_value) {
            // Create JsonEvent with current JSON Pointer path
            std::string json_pointer = generate_current_json_pointer();
            JsonEvent json_event(type, value, json_pointer);
            events_.on_value(json_event);
        }
    }

    void emit_error(const std::string& message) {
        if (events_.on_error) {
            std::string json_pointer = generate_current_json_pointer();
            events_.on_error({position_, message, json_pointer});
        }
    }

    [[nodiscard]] static auto escape_json_pointer_component(const std::string& component)
        -> std::string {
        std::string escaped = component;
        std::size_t pos = 0;
        // Escape ~ first (~ -> ~0)
        while ((pos = escaped.find('~', pos)) != std::string::npos) {
            escaped.replace(pos, 1, "~0");
            pos += 2;
        }
        pos = 0;
        // Then escape / (/ -> ~1)
        while ((pos = escaped.find('/', pos)) != std::string::npos) {
            escaped.replace(pos, 1, "~1");
            pos += 2;
        }
        return escaped;
    }

    // Fast escape check - most keys don't need escaping
    [[nodiscard]] static auto needs_escaping(const std::string& component) -> bool {
        return component.find('~') != std::string::npos || component.find('/') != std::string::npos;
    }


    [[nodiscard]] auto generate_current_json_pointer() const -> std::string {
        // Build path from context stack, incorporating array indices dynamically
        std::string result;
        
        // Build path from context stack - this gives us the correct array index handling
        for (std::size_t i = 0; i < context_stack_.size(); ++i) {
            const auto& context = context_stack_[i];
            
            if (!context.key.empty()) {
                // This context represents a value accessed by object key
                result += '/';
                if (needs_escaping(context.key)) {
                    result += escape_json_pointer_component(context.key);
                } else {
                    result += context.key;
                }
            }
            
            // If this context is an array element (not the root array), add the index
            if (i > 0 && context_stack_[i - 1].type == ContainerType::Array) {
                result += '/';
                result += std::to_string(context_stack_[i - 1].array_index);
            }
        }
        
        // Add current key if we're parsing a value in an object
        if (!context_stack_.empty() && context_stack_.back().type == ContainerType::Object && !current_key_.empty()) {
            result += '/';
            if (needs_escaping(current_key_)) {
                result += escape_json_pointer_component(current_key_);
            } else {
                result += current_key_;
            }
        }
        
        // If we're parsing a value in an array, add the current array index
        if (!context_stack_.empty() && context_stack_.back().type == ContainerType::Array) {
            result += '/';
            result += std::to_string(context_stack_.back().array_index);
        }
        
        return result;
    }

    [[nodiscard]] auto is_cache_valid() const -> bool {
        if (cached_json_pointer_.empty()) {
            return false;
        }

        if (last_context_size_ != context_stack_.size()) {
            return false;
        }

        if (last_current_key_ != current_key_) {
            return false;
        }

        // Check if array indices have changed
        if (last_array_indices_.size() != context_stack_.size()) {
            return false;
        }

        for (std::size_t i = 0; i < context_stack_.size(); ++i) {
            if (context_stack_[i].type == ContainerType::Array) {
                if (i >= last_array_indices_.size()
                    || last_array_indices_[i] != context_stack_[i].array_index) {
                    return false;
                }
            }
        }

        return true;
    }

    void update_cache_state() const {
        last_context_size_ = context_stack_.size();
        last_current_key_ = current_key_;

        // Update array indices snapshot
        last_array_indices_.clear();
        last_array_indices_.reserve(context_stack_.size());
        for (const auto& context : context_stack_) {
            last_array_indices_.push_back(context.array_index);
        }
    }

    // NOLINTBEGIN(readability-function-size)
    [[nodiscard]] auto build_json_pointer_from_context() const -> std::string {
        std::string result;

        // Build path from context stack
        for (std::size_t i = 0; i < context_stack_.size(); ++i) {
            const auto& context = context_stack_[i];

            if (!context.key.empty()) {
                // This context represents a value accessed by object key
                std::string escaped_key = escape_json_pointer_component(context.key);
                result += "/" + escaped_key;
            }

            // If this context is an array element (not the root array), add the index
            if (i > 0 && context_stack_[i - 1].type == ContainerType::Array) {
                result += "/" + std::to_string(context_stack_[i - 1].array_index);
            }
        }

        // Add current key if we're parsing a value in an object
        if (!context_stack_.empty() && context_stack_.back().type == ContainerType::Object
            && !current_key_.empty()) {
            std::string escaped_key = escape_json_pointer_component(current_key_);
            result += "/" + escaped_key;
        }

        // If we're parsing a value in an array, add the current array index
        if (!context_stack_.empty() && context_stack_.back().type == ContainerType::Array) {
            result += "/" + std::to_string(context_stack_.back().array_index);
        }

        return result;
    }
    // NOLINTEND(readability-function-size)

    void return_to_container_or_start() {
        if (context_stack_.empty()) {
            state_ = ParseState::Start;
        } else {
            ParseContext& current = context_stack_.back();
            state_ = (current.type == ContainerType::Object) ? ParseState::InObject
                                                             : ParseState::InArray;
        }
    }

    void update_object_state_after_value() {
        // If we just emitted a value in an object, expect next key (after comma)
        if (!context_stack_.empty() && context_stack_.back().type == ContainerType::Object) {
            context_stack_.back().expecting_key = true;
            current_key_.clear(); // Clear the stored key
        }
    }

    // NOLINTNEXTLINE(readability-identifier-length)
    void push_back_character(char c) {
        if (has_pushed_back_char_) {
            // Can only push back one character at a time
            emit_error("Internal error: multiple character push-back");
            return;
        }
        pushed_back_char_ = c;
        has_pushed_back_char_ = true;
    }

    void handle_object_opening() {
        // Create new ParseContext for object
        ParseContext context(root_node_, ContainerType::Object);

        // Store the current key in the new context if we're inside an object
        if (!context_stack_.empty() && context_stack_.back().type == ContainerType::Object
            && !current_key_.empty()) {
            context.key = current_key_;
        }

        context_stack_.push_back(context);

        // Invalidate JSON pointer cache due to context change (still needed for compatibility)
        cached_json_pointer_.clear();

        // Emit on_enter_object event
        if (events_.on_enter_object) {
            // Pass empty string for now to maintain compatibility
            events_.on_enter_object("");
        }

        // Transition to InObject state
        state_ = ParseState::InObject;
    }

    void handle_array_opening() {
        // Create new ParseContext for array
        ParseContext context(root_node_, ContainerType::Array);

        // Store the current key in the new context if we're inside an object
        if (!context_stack_.empty() && context_stack_.back().type == ContainerType::Object
            && !current_key_.empty()) {
            context.key = current_key_;
        }

        context_stack_.push_back(context);

        // Invalidate JSON pointer cache due to context change (still needed for compatibility)
        cached_json_pointer_.clear();

        // Emit on_enter_array event
        if (events_.on_enter_array) {
            events_.on_enter_array();
        }

        // Transition to InArray state
        state_ = ParseState::InArray;
    }

    void handle_object_closing() {
        if (context_stack_.empty()) {
            emit_error("Unexpected '}': no object to close");
            return;
        }

        ParseContext& current = context_stack_.back();
        if (current.type != ContainerType::Object) {
            emit_error("Unexpected '}': expected ']' to close array");
            context_stack_.clear(); // Clear to prevent end_input error
            state_ = ParseState::Error;
            return;
        }

        // Pop the object context
        context_stack_.pop_back();

        // Emit on_exit_container event
        if (events_.on_exit_container) {
            events_.on_exit_container();
        }

        // Return to appropriate state
        return_to_container_or_start();
    }

    void handle_array_closing() {
        if (context_stack_.empty()) {
            emit_error("Unexpected ']': no array to close");
            return;
        }

        ParseContext& current = context_stack_.back();
        if (current.type != ContainerType::Array) {
            emit_error("Unexpected ']': expected '}' to close object");
            context_stack_.clear(); // Clear to prevent end_input error
            state_ = ParseState::Error;
            return;
        }

        // Pop the array context
        context_stack_.pop_back();

        // Emit on_exit_container event
        if (events_.on_exit_container) {
            events_.on_exit_container();
        }

        // Return to appropriate state
        return_to_container_or_start();
    }

    void handle_colon_separator() {
        if (context_stack_.empty() || context_stack_.back().type != ContainerType::Object) {
            emit_error("Unexpected ':' outside of object");
            state_ = ParseState::Error;
            return;
        }

        if (context_stack_.back().expecting_key || !context_stack_.back().expecting_colon) {
            emit_error("Expected key before ':'");
            state_ = ParseState::Error;
            return;
        }

        // Colon found after key, now expect value
        context_stack_.back().expecting_colon = false;
    }

    // NOLINTBEGIN(readability-function-size)
    // NOLINTNEXTLINE(readability-identifier-length)
    void handle_object_state(char c) {
        // Skip whitespace
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            return;
        }

        if (c == '}') {
            // End of object - clear any pending expectations
            if (!context_stack_.empty()) {
                context_stack_.back().expecting_key = false;
                context_stack_.back().expecting_colon = false;
            }
            handle_object_closing();
        } else if (c == ',') {
            // Comma separator - expect next key
            if (!context_stack_.empty()) {
                context_stack_.back().expecting_key = true;
            }
            return;
        } else if (c == ':') {
            // Colon separator - expect value after key
            handle_colon_separator();
        } else {
            // Check if we're expecting a colon and got something else
            if (!context_stack_.empty() && context_stack_.back().expecting_colon) {
                emit_error("Expected ':' after object key");
                state_ = ParseState::Error;
                return;
            }

            // Parse key or value based on current state
            if (!context_stack_.empty() && context_stack_.back().expecting_key) {
                // Must be a string key
                if (c == '"') {
                    handle_start_state(c);
                } else {
                    emit_error("Object key must be a string");
                    state_ = ParseState::Error;
                }
            } else {
                // Parse value
                handle_start_state(c);
            }
        }
    }
    // NOLINTEND(readability-function-size)

    // NOLINTNEXTLINE(readability-identifier-length)
    void handle_array_state(char c) {
        // Skip whitespace
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            return;
        }

        if (c == ']') {
            // End of array
            handle_array_closing();
        } else if (c == ',') {
            // Comma separator - increment array index for next element
            if (!context_stack_.empty() && context_stack_.back().type == ContainerType::Array) {
                context_stack_.back().array_index++;
            }
            return;
        } else {
            // Parse array element value
            handle_start_state(c);
        }
    }

public:
    void parse_string(const std::string& json) {
        // NOLINTNEXTLINE(readability-identifier-length)
        for (char c : json) {
            feed_character(c);
        }
    }

    // NOLINTBEGIN(readability-function-size)
    void end_input() {
        // Process any pushed back character first
        if (has_pushed_back_char_) {
            char pushed_char = pushed_back_char_;
            has_pushed_back_char_ = false;
            process_character_in_state(pushed_char);
        }

        // Complete any pending value
        if (state_ == ParseState::InNumber) {
            complete_number_value();
        } else if (state_ == ParseState::InLiteral) {
            // Check if we have a complete literal
            if (value_buffer_ == "true" || value_buffer_ == "false" || value_buffer_ == "null") {
                complete_literal_value();
            } else if (!value_buffer_.empty()) {
                // For incomplete literals at end of input, treat as invalid literal
                emit_error("Invalid literal");
            }
        } else if (state_ == ParseState::InString) {
            emit_error("Unterminated string at end of input");
        } else if (state_ == ParseState::InUnicodeEscape) {
            emit_error("Incomplete Unicode escape sequence at end of input");
        } else if (state_ == ParseState::Error) {
            // Already in error state, no additional error needed
        }

        if (!context_stack_.empty()) {
            emit_error("Unexpected end of input: unclosed containers");
        }
    }
    // NOLINTEND(readability-function-size)

    void set_events(const ParseEvents& events) { events_ = events; }

    void reset() {
        state_ = ParseState::Start;
        context_stack_.clear();
        value_buffer_.clear();
        position_ = 0;
        in_escape_ = false;
        unicode_buffer_.clear();
        current_key_.clear();
        has_pushed_back_char_ = false;
        allocator_->reset();

        root_node_ = static_cast<PathNode*>(allocator_->allocate(sizeof(PathNode)));
        new (root_node_) PathNode(nullptr, "");
    }
};

// ============================================================================
// BATCH PARSER - DOM-STYLE JSON PROCESSING WITH INTELLIGENT SERIALIZATION
// ============================================================================

// Forward declarations
class JsonDocument;
class BatchParser;
class SmartJsonFormatter;

// JSON formatting options for intelligent serialization
struct JsonFormatOptions {
    bool pretty = false;    // Enable pretty printing
    int indent_size = 2;    // Spaces per indent level
    bool sort_keys = false; // Sort object keys alphabetically

    // Smart compacting controls
    int max_inline_array_size = 10;    // Arrays with <= elements stay on one line
    int max_inline_object_size = 3;    // Objects with <= properties stay on one line
    int max_inline_string_length = 40; // Strings <= length don't break structure

    // Special handling
    bool quote_keys = true;      // Quote object keys (JSON standard)
    bool trailing_comma = false; // Add trailing commas (non-standard)
    bool escape_unicode = false; // Escape non-ASCII as \uXXXX
    int max_depth = 100;         // Maximum nesting depth
};

// Predefined format presets
namespace FormatPresets {
inline const JsonFormatOptions Compact{}; // Default compact format

inline const JsonFormatOptions Pretty{
    .pretty = true, .indent_size = 2, .max_inline_array_size = 10, .max_inline_object_size = 3};

inline const JsonFormatOptions Config{.pretty = true,
                                      .indent_size = 2,
                                      .sort_keys = true,
                                      .max_inline_array_size = 5,
                                      .max_inline_object_size = 1};

inline const JsonFormatOptions Api{
    .pretty = true, .indent_size = 2, .max_inline_array_size = 20, .max_inline_object_size = 5};

inline const JsonFormatOptions Debug{.pretty = true,
                                     .indent_size = 4,
                                     .sort_keys = true,
                                     .max_inline_array_size = 1,
                                     .max_inline_object_size = 0,
                                     .escape_unicode = true};
} // namespace FormatPresets

// BatchParser JsonType enum - extends the existing JsonType concept
enum class BatchJsonType : std::uint8_t { Null, Boolean, Number, String, Object, Array };

// Exception hierarchy for BatchParser
class JsonException : public std::exception {
protected:
    std::string message_;
    std::string json_pointer_;

public:
    JsonException(std::string message, std::string pointer = "")
        : message_(std::move(message)), json_pointer_(std::move(pointer)) {}

    [[nodiscard]] auto what() const noexcept -> const char* override { return message_.c_str(); }
    [[nodiscard]] auto json_pointer() const noexcept -> const std::string& { return json_pointer_; }
};

class ParseException : public JsonException {
public:
    ParseException(const std::string& message, const std::string& pointer = "")
        : JsonException("Parse error: " + message, pointer) {}
};

class TypeException : public JsonException {
public:
    TypeException(const std::string& message, const std::string& pointer = "")
        : JsonException("Type error: " + message, pointer) {}
};

// Forward declaration for friend class
class BatchParser;

// JsonDocument - The core DOM-style JSON value class
class JsonDocument {
    friend class BatchParser;
    friend class SmartJsonFormatter;

private:
    BatchJsonType type_{BatchJsonType::Null};

    union Storage {
        bool boolean;
        double number;
        std::string* string;
        std::map<std::string, JsonDocument>* object;
        std::vector<JsonDocument>* array;

        Storage() : boolean(false) {}
        ~Storage() {} // Handled by JsonDocument destructor
    } storage_;

    void destroy_storage() {
        switch (type_) {
        case BatchJsonType::String:
            delete storage_.string;
            break;
        case BatchJsonType::Object:
            delete storage_.object;
            break;
        case BatchJsonType::Array:
            delete storage_.array;
            break;
        default:
            // Primitive types don't need cleanup
            break;
        }
    }

public:
    // Default constructor - creates null value
    JsonDocument() = default;

    // Constructors for each JSON type
    explicit JsonDocument(bool value) : type_(BatchJsonType::Boolean) { storage_.boolean = value; }

    explicit JsonDocument(int value) : type_(BatchJsonType::Number) {
        storage_.number = static_cast<double>(value);
    }

    explicit JsonDocument(double value) : type_(BatchJsonType::Number) { storage_.number = value; }

    explicit JsonDocument(const char* value) : type_(BatchJsonType::String) {
        storage_.string = new std::string(value);
    }

    explicit JsonDocument(std::string value) : type_(BatchJsonType::String) {
        storage_.string = new std::string(std::move(value));
    }

    // Initializer list constructor for objects
    JsonDocument(std::initializer_list<std::pair<std::string, JsonDocument>> init)
        : type_(BatchJsonType::Object) {
        storage_.object = new std::map<std::string, JsonDocument>();
        for (const auto& pair : init) {
            (*storage_.object)[pair.first] = pair.second;
        }
    }

    // Initializer list constructor for arrays
    JsonDocument(std::initializer_list<JsonDocument> init) : type_(BatchJsonType::Array) {
        storage_.array = new std::vector<JsonDocument>(init);
    }

    // Destructor
    ~JsonDocument() { destroy_storage(); }

    // Copy constructor
    JsonDocument(const JsonDocument& other) : type_(other.type_) {
        switch (type_) {
        case BatchJsonType::Null:
            break;
        case BatchJsonType::Boolean:
            storage_.boolean = other.storage_.boolean;
            break;
        case BatchJsonType::Number:
            storage_.number = other.storage_.number;
            break;
        case BatchJsonType::String:
            storage_.string = new std::string(*other.storage_.string);
            break;
        case BatchJsonType::Object:
            storage_.object = new std::map<std::string, JsonDocument>(*other.storage_.object);
            break;
        case BatchJsonType::Array:
            storage_.array = new std::vector<JsonDocument>(*other.storage_.array);
            break;
        }
    }

    // Move constructor
    JsonDocument(JsonDocument&& other) noexcept : type_(other.type_), storage_(other.storage_) {
        other.type_ = BatchJsonType::Null;
    }

    // Copy assignment
    auto operator=(const JsonDocument& other) -> JsonDocument& {
        if (this != &other) {
            destroy_storage();
            type_ = other.type_;

            switch (type_) {
            case BatchJsonType::Null:
                break;
            case BatchJsonType::Boolean:
                storage_.boolean = other.storage_.boolean;
                break;
            case BatchJsonType::Number:
                storage_.number = other.storage_.number;
                break;
            case BatchJsonType::String:
                storage_.string = new std::string(*other.storage_.string);
                break;
            case BatchJsonType::Object:
                storage_.object = new std::map<std::string, JsonDocument>(*other.storage_.object);
                break;
            case BatchJsonType::Array:
                storage_.array = new std::vector<JsonDocument>(*other.storage_.array);
                break;
            }
        }
        return *this;
    }

    // Move assignment
    auto operator=(JsonDocument&& other) noexcept -> JsonDocument& {
        if (this != &other) {
            destroy_storage();
            type_ = other.type_;
            storage_ = other.storage_;
            other.type_ = BatchJsonType::Null;
        }
        return *this;
    }

    // Type checking methods
    [[nodiscard]] auto type() const noexcept -> BatchJsonType { return type_; }
    [[nodiscard]] auto is_null() const noexcept -> bool { return type_ == BatchJsonType::Null; }
    [[nodiscard]] auto is_bool() const noexcept -> bool { return type_ == BatchJsonType::Boolean; }
    [[nodiscard]] auto is_number() const noexcept -> bool { return type_ == BatchJsonType::Number; }
    [[nodiscard]] auto is_string() const noexcept -> bool { return type_ == BatchJsonType::String; }
    [[nodiscard]] auto is_object() const noexcept -> bool { return type_ == BatchJsonType::Object; }
    [[nodiscard]] auto is_array() const noexcept -> bool { return type_ == BatchJsonType::Array; }

    // Type conversion methods - will implement template specializations
    template <typename T> auto as() const -> T;

    // Container access methods
    // Object access
    auto operator[](const std::string& key) -> JsonDocument& {
        if (type_ != BatchJsonType::Object) {
            throw TypeException("Cannot access non-object with string key");
        }
        return (*storage_.object)[key];
    }

    auto operator[](const std::string& key) const -> const JsonDocument& {
        if (type_ != BatchJsonType::Object) {
            throw TypeException("Cannot access non-object with string key");
        }

        // NOLINTNEXTLINE(readability-identifier-length)
        auto it = storage_.object->find(key);
        if (it == storage_.object->end()) {
            throw std::out_of_range("Key not found: " + key);
        }
        return it->second;
    }

    // Array access
    auto operator[](std::size_t index) -> JsonDocument& {
        if (type_ != BatchJsonType::Array) {
            throw TypeException("Cannot access non-array with numeric index");
        }
        if (index >= storage_.array->size()) {
            throw std::out_of_range("Array index out of range");
        }
        return (*storage_.array)[index];
    }

    auto operator[](std::size_t index) const -> const JsonDocument& {
        if (type_ != BatchJsonType::Array) {
            throw TypeException("Cannot access non-array with numeric index");
        }
        if (index >= storage_.array->size()) {
            throw std::out_of_range("Array index out of range");
        }
        return (*storage_.array)[index];
    }

    // Safe access with bounds checking
    auto at(const std::string& key) -> JsonDocument& {
        if (type_ != BatchJsonType::Object) {
            throw TypeException("Cannot access non-object with string key");
        }
        return storage_.object->at(key);
    }

    [[nodiscard]] auto at(const std::string& key) const -> const JsonDocument& {
        if (type_ != BatchJsonType::Object) {
            throw TypeException("Cannot access non-object with string key");
        }
        return storage_.object->at(key);
    }

    auto at(std::size_t index) -> JsonDocument& {
        if (type_ != BatchJsonType::Array) {
            throw TypeException("Cannot access non-array with numeric index");
        }
        return storage_.array->at(index);
    }

    [[nodiscard]] auto at(std::size_t index) const -> const JsonDocument& {
        if (type_ != BatchJsonType::Array) {
            throw TypeException("Cannot access non-array with numeric index");
        }
        return storage_.array->at(index);
    }

    // Container size
    [[nodiscard]] auto size() const -> std::size_t {
        switch (type_) {
        case BatchJsonType::Object:
            return storage_.object->size();
        case BatchJsonType::Array:
            return storage_.array->size();
        case BatchJsonType::String:
            return storage_.string->size();
        default:
            throw TypeException("Size not supported for this type");
        }
    }

    [[nodiscard]] auto empty() const -> bool {
        switch (type_) {
        case BatchJsonType::Object:
            return storage_.object->empty();
        case BatchJsonType::Array:
            return storage_.array->empty();
        case BatchJsonType::String:
            return storage_.string->empty();
        default:
            return false; // Primitives are never "empty"
        }
    }

private:
    // Helper methods for DocumentBuilder (friend class access)
    void init_as_object() {
        destroy_storage();
        type_ = BatchJsonType::Object;
        storage_.object = new std::map<std::string, JsonDocument>();
    }

    void init_as_array() {
        destroy_storage();
        type_ = BatchJsonType::Array;
        storage_.array = new std::vector<JsonDocument>();
    }

    void add_to_object(const std::string& key, JsonDocument value) {
        if (type_ == BatchJsonType::Object) {
            (*storage_.object)[key] = std::move(value);
        }
    }

    void add_to_array(JsonDocument value) {
        if (type_ == BatchJsonType::Array) {
            storage_.array->push_back(std::move(value));
        }
    }

    auto get_object_ref(const std::string& key) -> JsonDocument* {
        if (type_ == BatchJsonType::Object) {

            // NOLINTNEXTLINE(readability-identifier-length)
            auto it = storage_.object->find(key);
            return (it != storage_.object->end()) ? &it->second : nullptr;
        }
        return nullptr;
    }

    auto get_array_back() -> JsonDocument* {
        if (type_ == BatchJsonType::Array && !storage_.array->empty()) {
            return &storage_.array->back();
        }
        return nullptr;
    }

public:
    // Basic serialization - compact format (implemented after FormatPresets)
    [[nodiscard]] auto to_json() const -> std::string;

    // Intelligent JSON serialization with format options (implemented after SmartJsonFormatter)
    [[nodiscard]] auto to_json(const JsonFormatOptions& options) const -> std::string;

    // Convenient pretty printing (implemented after FormatPresets)
    [[nodiscard]] auto to_json(bool pretty) const -> std::string;

private:
    void write_primitive_to_stream(std::ostream& out) const {
        switch (type_) {
        case BatchJsonType::Null:
            out << "null";
            break;
        case BatchJsonType::Boolean:
            out << (storage_.boolean ? "true" : "false");
            break;
        case BatchJsonType::Number:
            out << storage_.number;
            break;
        case BatchJsonType::String:
            out << '"' << *storage_.string << '"'; // TODO: Proper escaping
            break;
        default:
            break;
        }
    }

    void write_object_to_stream(std::ostream& out) const {
        out << '{';
        bool first = true;
        for (const auto& pair : *storage_.object) {
            const auto& key = pair.first;
            const auto& value = pair.second;
            if (!first) {
                out << ',';
            }
            out << '"' << key << "\":";
            value.write_json_compact(out);
            first = false;
        }
        out << '}';
    }

    void write_array_to_stream(std::ostream& out) const {
        out << '[';
        bool first_elem = true;
        for (const auto& value : *storage_.array) {
            if (!first_elem) {
                out << ',';
            }
            value.write_json_compact(out);
            first_elem = false;
        }
        out << ']';
    }

    void write_json_compact(std::ostream& out) const {
        switch (type_) {
        case BatchJsonType::Null:
        case BatchJsonType::Boolean:
        case BatchJsonType::Number:
        case BatchJsonType::String:
            write_primitive_to_stream(out);
            break;
        case BatchJsonType::Object:
            write_object_to_stream(out);
            break;
        case BatchJsonType::Array:
            write_array_to_stream(out);
            break;
        }
    }
};

// Template specializations for type conversion
template <> inline auto JsonDocument::as<bool>() const -> bool {
    if (type_ != BatchJsonType::Boolean) {
        throw TypeException("Expected boolean, got " + std::to_string(static_cast<int>(type_)));
    }
    return storage_.boolean;
}

template <> inline auto JsonDocument::as<int>() const -> int {
    if (type_ != BatchJsonType::Number) {
        throw TypeException("Expected number, got " + std::to_string(static_cast<int>(type_)));
    }
    return static_cast<int>(storage_.number);
}

template <> inline auto JsonDocument::as<double>() const -> double {
    if (type_ != BatchJsonType::Number) {
        throw TypeException("Expected number, got " + std::to_string(static_cast<int>(type_)));
    }
    return storage_.number;
}

template <> inline auto JsonDocument::as<std::string>() const -> std::string {
    if (type_ != BatchJsonType::String) {
        throw TypeException("Expected string, got " + std::to_string(static_cast<int>(type_)));
    }
    return *storage_.string;
}

// Smart JSON formatter with intelligent pretty printing
class SmartJsonFormatter {
private:
    std::ostream& out_;
    const JsonFormatOptions& options_;
    int current_indent_level_{0};
    bool need_comma_{false};

public:
    SmartJsonFormatter(std::ostream& out, const JsonFormatOptions& options)
        : out_(out), options_(options) {}

    void write(const JsonDocument& doc);

private:
    void write_indent();
    void write_newline();
    void write_value(const JsonDocument& value);
    void write_string(const std::string& str);
    void write_object(const JsonDocument& obj);
    void write_array(const JsonDocument& arr);
    void write_inline_object(const JsonDocument& obj);
    void write_inline_array(const JsonDocument& arr);

    [[nodiscard]] auto should_inline_array(const JsonDocument& arr) const -> bool;
    [[nodiscard]] auto should_inline_object(const JsonDocument& obj) const -> bool;

    // Helper methods for write_value
    void write_number(double num);

    // Helper methods for write_string
    void write_escaped_char(char c);            // NOLINT(readability-identifier-length)
    auto write_standard_escape(char c) -> bool; // NOLINT(readability-identifier-length)
    void write_unicode_escape(unsigned char c); // NOLINT(readability-identifier-length)

    static constexpr unsigned char CONTROL_CHAR_THRESHOLD = 0x20;
};

// SmartJsonFormatter implementation
inline void SmartJsonFormatter::write(const JsonDocument& doc) { write_value(doc); }

inline void SmartJsonFormatter::write_indent() {
    if (options_.pretty) {
        for (int i = 0; i < current_indent_level_ * options_.indent_size; ++i) {
            out_ << ' ';
        }
    }
}

inline void SmartJsonFormatter::write_newline() {
    if (options_.pretty) {
        out_ << '\n';
    }
}

inline void SmartJsonFormatter::write_value(const JsonDocument& value) {
    switch (value.type()) {
    case BatchJsonType::Null:
        out_ << "null";
        break;
    case BatchJsonType::Boolean:
        out_ << (value.as<bool>() ? "true" : "false");
        break;
    case BatchJsonType::Number:
        write_number(value.as<double>());
        break;
    case BatchJsonType::String:
        write_string(value.as<std::string>());
        break;
    case BatchJsonType::Object:
        write_object(value);
        break;
    case BatchJsonType::Array:
        write_array(value);
        break;
    }
}

inline void SmartJsonFormatter::write_string(const std::string& str) {
    out_ << '"';
    for (char c : str) { // NOLINT(readability-identifier-length)
        write_escaped_char(c);
    }
    out_ << '"';
}

inline void SmartJsonFormatter::write_object(const JsonDocument& obj) {
    if (should_inline_object(obj)) {
        write_inline_object(obj);
    } else {
        out_ << '{';
        if (!obj.empty()) {
            current_indent_level_++;
            bool first = true;

            for (const auto& pair : *obj.storage_.object) {
                if (!first) {
                    out_ << ',';
                }
                write_newline();
                write_indent();
                write_string(pair.first);
                out_ << (options_.pretty ? ": " : ":");
                write_value(pair.second);
                first = false;
            }

            current_indent_level_--;
            write_newline();
            write_indent();
        }
        out_ << '}';
    }
}

inline void SmartJsonFormatter::write_array(const JsonDocument& arr) {
    if (should_inline_array(arr)) {
        write_inline_array(arr);
    } else {
        out_ << '[';
        if (!arr.empty()) {
            current_indent_level_++;
            bool first = true;

            for (const auto& value : *arr.storage_.array) {
                if (!first) {
                    out_ << ',';
                }
                write_newline();
                write_indent();
                write_value(value);
                first = false;
            }

            current_indent_level_--;
            write_newline();
            write_indent();
        }
        out_ << ']';
    }
}

inline void SmartJsonFormatter::write_inline_object(const JsonDocument& obj) {
    out_ << '{';
    bool first = true;
    for (const auto& pair : *obj.storage_.object) {
        if (!first) {
            out_ << (options_.pretty ? ", " : ",");
        }
        write_string(pair.first);
        out_ << (options_.pretty ? ": " : ":");
        write_value(pair.second);
        first = false;
    }
    out_ << '}';
}

inline void SmartJsonFormatter::write_inline_array(const JsonDocument& arr) {
    out_ << '[';
    bool first = true;
    for (const auto& value : *arr.storage_.array) {
        if (!first) {
            out_ << (options_.pretty ? ", " : ",");
        }
        write_value(value);
        first = false;
    }
    out_ << ']';
}

inline auto SmartJsonFormatter::should_inline_array(const JsonDocument& arr) const -> bool {
    if (!options_.pretty) {
        return true; // Everything inline in compact mode
    }

    if (arr.empty()) {
        return true; // Empty arrays are always inline
    }

    if (arr.size() > static_cast<size_t>(options_.max_inline_array_size)) {
        return false; // Array is too long
    }

    // Check if all elements are simple (no nested containers)
    return std::all_of(
        arr.storage_.array->begin(), arr.storage_.array->end(),
        [](const JsonDocument& value) { return !value.is_object() && !value.is_array(); });
}

inline auto SmartJsonFormatter::should_inline_object(const JsonDocument& obj) const -> bool {
    if (!options_.pretty) {
        return true; // Everything inline in compact mode
    }

    if (obj.empty()) {
        return true; // Empty objects are always inline
    }

    if (obj.size() > static_cast<size_t>(options_.max_inline_object_size)) {
        return false;
    }

    // Check if all values are simple (no nested containers)
    return std::all_of(
        obj.storage_.object->begin(), obj.storage_.object->end(),
        [](const auto& pair) { return !pair.second.is_object() && !pair.second.is_array(); });
}

inline void SmartJsonFormatter::write_number(double num) {
    // Check if it's actually an integer
    auto int_val = static_cast<long long>(num);
    if (static_cast<double>(int_val) == num && num >= LLONG_MIN && num <= LLONG_MAX) {
        out_ << static_cast<long long>(num);
    } else {
        // Format floating point numbers without scientific notation
        std::ostringstream temp;
        temp << std::fixed << num;
        std::string str = temp.str();

        // Remove trailing zeros after decimal point
        if (str.find('.') != std::string::npos) {
            str.erase(str.find_last_not_of('0') + 1);
            if (str.back() == '.') {
                str.pop_back();
            }
        }
        out_ << str;
    }
}

// NOLINTNEXTLINE(readability-identifier-length)
inline void SmartJsonFormatter::write_escaped_char(char c) {
    if (write_standard_escape(c)) {
        return;
    }

    // NOLINTNEXTLINE(readability-identifier-length)
    auto uc = static_cast<unsigned char>(c);
    if (uc < CONTROL_CHAR_THRESHOLD) {
        write_unicode_escape(uc);
    } else {
        out_ << c;
    }
}

// NOLINTNEXTLINE(readability-identifier-length)
inline auto SmartJsonFormatter::write_standard_escape(char c) -> bool {
    switch (c) {
    case '"':
        out_ << "\\\"";
        return true;
    case '\\':
        out_ << "\\\\";
        return true;
    case '\b':
        out_ << "\\b";
        return true;
    case '\f':
        out_ << "\\f";
        return true;
    case '\n':
        out_ << "\\n";
        return true;
    case '\r':
        out_ << "\\r";
        return true;
    case '\t':
        out_ << "\\t";
        return true;
    default:
        return false;
    }
}

// NOLINTNEXTLINE(readability-identifier-length)
inline void SmartJsonFormatter::write_unicode_escape(unsigned char c) {
    out_ << "\\u" << std::hex << std::setfill('0') << std::setw(4) << static_cast<unsigned int>(c)
         << std::dec;
}

// JsonDocument method implementations (defined after SmartJsonFormatter)
inline auto JsonDocument::to_json(const JsonFormatOptions& options) const -> std::string {
    std::ostringstream stream;
    SmartJsonFormatter formatter(stream, options);
    formatter.write(*this);
    return stream.str();
}

inline auto JsonDocument::to_json() const -> std::string { return to_json(FormatPresets::Compact); }

inline auto JsonDocument::to_json(bool pretty) const -> std::string {
    return pretty ? to_json(FormatPresets::Pretty) : to_json(FormatPresets::Compact);
}

// BatchParser class - integrates with JSOM StreamingParser
class BatchParser {
private:
    // Document builder that converts JSOM streaming events to JsonDocument
    class DocumentBuilder {
    private:
        // Container stack entry - tracks current container and next key/index
        struct ContainerFrame {
            JsonDocument* container;
            std::string next_key;      // For objects: the key for the next value
            std::size_t next_index{0}; // For arrays: the index for the next value
            bool is_array;

            ContainerFrame(JsonDocument* cont, bool array) : container(cont), is_array(array) {}
        };

        JsonDocument root_;
        bool has_error_{false};
        std::string error_message_;
        bool root_initialized_{false};
        std::vector<ContainerFrame> container_stack_; // Stack-based navigation

    public:
        void reset_builder_state() {
            root_ = JsonDocument();
            has_error_ = false;
            error_message_.clear();
            root_initialized_ = false;
            container_stack_.clear();
        }

        void configure_parser_events(ParseEvents& events) {
            events.on_value
                = [this](const jsom::JsonEvent& value) { this->handle_value_optimized(value); };

            events.on_enter_object = [this](const std::string& key) {
                if (key.empty() && !root_initialized_) {
                    // Root object starting
                    root_.init_as_object();
                    root_initialized_ = true;
                }
            };

            events.on_enter_array = [this]() {
                if (!root_initialized_) {
                    // Root array starting
                    root_.init_as_array();
                    root_initialized_ = true;
                }
            };

            events.on_exit_container = [this]() {
                // Container exit - no action needed
            };

            events.on_error = [this](const ParseError& error) { this->handle_error(error); };
        }

        auto execute_parsing(const std::string& json) -> JsonDocument {
            StreamingParser parser;
            ParseEvents events;

            configure_parser_events(events);
            parser.set_events(events);

            try {
                parser.parse_string(json);
                parser.end_input();

                if (has_error_) {
                    throw ParseException(error_message_);
                }

                return std::move(root_);
            } catch (const std::exception& e) {
                throw ParseException("JSON parsing failed: " + std::string(e.what()));
            }
        }

        auto build_from_json(const std::string& json) -> JsonDocument {
            reset_builder_state();
            return execute_parsing(json);
        }

    private:
        void handle_value(const jsom::JsonEvent& jsom_event) {
            // Convert JSOM JsonEvent to JsonDocument
            JsonDocument value;
            switch (jsom_event.type()) {
            case jsom::JsonType::Null:
                value = JsonDocument(); // null
                break;
            case jsom::JsonType::Boolean:
                value = JsonDocument(jsom_event.as_bool());
                break;
            case jsom::JsonType::Number:
                value = JsonDocument(jsom_event.as_double());
                break;
            case jsom::JsonType::String:
                value = JsonDocument(jsom_event.as_string());
                break;
            default:
                return;
            }

            std::string path = jsom_event.path();

            if (path.empty() || path == "/") {
                // Root value
                root_ = std::move(value);
                root_initialized_ = true;
            } else {
                // Nested value - ensure path exists and set value
                set_value_at_path(path, std::move(value));
            }
        }

        // Optimized value handler with reduced path parsing
        void handle_value_optimized(const jsom::JsonEvent& jsom_event) {
            // Convert JSOM JsonEvent to JsonDocument
            JsonDocument value;
            switch (jsom_event.type()) {
            case jsom::JsonType::Null:
                value = JsonDocument(); // null
                break;
            case jsom::JsonType::Boolean:
                value = JsonDocument(jsom_event.as_bool());
                break;
            case jsom::JsonType::Number:
                value = JsonDocument(jsom_event.as_double());
                break;
            case jsom::JsonType::String:
                value = JsonDocument(jsom_event.as_string());
                break;
            default:
                return;
            }

            std::string path = jsom_event.path();

            if (path.empty() || path == "/") {
                // Root value
                root_ = std::move(value);
                root_initialized_ = true;
            } else {
                // Use optimized path parsing that avoids full vector creation
                set_value_at_path_optimized(path, std::move(value));
            }
        }

        // Optimized version that does minimal parsing compared to the original
        void set_value_at_path_optimized(const std::string& path, JsonDocument value) {
            // For simple paths, avoid full parsing and use direct navigation
            if (is_simple_path(path)) {
                set_simple_value(path, std::move(value));
            } else {
                // Fall back to original method for complex paths
                set_value_at_path(path, std::move(value));
            }
        }

        // Check if path is simple enough for direct handling
        static auto is_simple_path(const std::string& path) -> bool {
            // Simple path: single level like "/key" or "/0"
            if (path.length() < 2 || path[0] != '/') {
                return false;
            }

            // Count slashes - simple path has exactly one
            size_t slash_count = 0;
            for (char c : path) { // NOLINT(readability-identifier-length)
                if (c == '/') {
                    ++slash_count;
                }
            }
            return slash_count == 1;
        }

        // Handle simple single-level paths without full parsing
        void set_simple_value(const std::string& path, JsonDocument value) {
            // Extract key (everything after first '/')
            std::string key = path.substr(1);

            // Ensure root is initialized
            if (!root_initialized_) {
                if (is_numeric(key)) {
                    root_.init_as_array();
                } else {
                    root_.init_as_object();
                }
                root_initialized_ = true;
            }

            // Set value directly in root
            if (root_.is_object()) {
                root_.add_to_object(key, std::move(value));
            } else if (root_.is_array()) {
                size_t index = std::stoull(key);
                extend_array_to_size(root_, index + 1);
                (*root_.storage_.array)[index] = std::move(value);
            }
        }

        // Helper method to initialize root container
        void initialize_root_container(bool is_array) {
            if (is_array) {
                root_.init_as_array();
            } else {
                root_.init_as_object();
            }
            root_initialized_ = true;
            container_stack_.emplace_back(&root_, is_array);
        }

        // Helper method to place container in array parent
        static auto place_container_in_array(ContainerFrame& parent_frame,
                                             JsonDocument&& new_container) -> JsonDocument* {
            extend_array_to_size(*parent_frame.container, parent_frame.next_index + 1);
            (*parent_frame.container->storage_.array)[parent_frame.next_index]
                = std::move(new_container);
            JsonDocument* result
                = &(*parent_frame.container->storage_.array)[parent_frame.next_index];
            parent_frame.next_index++;
            return result;
        }

        // Helper method to place container in object parent
        static auto place_container_in_object(ContainerFrame& parent_frame,
                                              JsonDocument&& new_container) -> JsonDocument* {
            if (!parent_frame.next_key.empty()) {
                parent_frame.container->add_to_object(parent_frame.next_key,
                                                      std::move(new_container));
                JsonDocument* result
                    = parent_frame.container->get_object_ref(parent_frame.next_key);
                parent_frame.next_key.clear();
                return result;
            }
            return nullptr;
        }

        // Generic helper for entering containers
        void handle_enter_container(bool is_array) {
            if (!root_initialized_) {
                initialize_root_container(is_array);
            } else if (!container_stack_.empty()) {
                // Create new nested container
                JsonDocument new_container;
                if (is_array) {
                    new_container.init_as_array();
                } else {
                    new_container.init_as_object();
                }

                auto& current_frame = container_stack_.back();
                JsonDocument* new_container_ptr = nullptr;

                if (current_frame.is_array) {
                    new_container_ptr
                        = place_container_in_array(current_frame, std::move(new_container));
                } else {
                    new_container_ptr
                        = place_container_in_object(current_frame, std::move(new_container));
                }

                if (new_container_ptr != nullptr) {
                    container_stack_.emplace_back(new_container_ptr, is_array);
                }
            }
        }

        void handle_enter_object() { handle_enter_container(false); }

        void handle_enter_array() { handle_enter_container(true); }

        void handle_exit_container() {
            if (!container_stack_.empty()) {
                container_stack_.pop_back();
            }
        }

        // Helper method to extract just the final key/index from a JSON Pointer path
        static auto extract_final_key_from_path(const std::string& path) -> std::string {
            if (path.empty() || path == "/") {
                return "";
            }

            // Find the last '/' and return everything after it
            size_t last_slash = path.find_last_of('/');
            if (last_slash != std::string::npos && last_slash + 1 < path.length()) {
                return path.substr(last_slash + 1);
            }

            return path; // Fallback for paths without '/'
        }

        void ensure_root_initialized(const std::vector<std::string>& components) {
            if (!root_initialized_) {
                // Determine if root should be object or array based on first component
                if (is_numeric(components[0])) {
                    root_.init_as_array();
                } else {
                    root_.init_as_object();
                }
                root_initialized_ = true;
            }
        }

        auto navigate_to_parent(const std::vector<std::string>& components) -> JsonDocument* {
            JsonDocument* current = &root_;
            for (size_t i = 0; i < components.size() - 1; ++i) {
                current = ensure_container_exists(current, components[i], components[i + 1]);
            }
            return current;
        }

        static void assign_final_value(JsonDocument* parent, const std::string& final_key,
                                       JsonDocument value) {
            if (parent->is_object()) {
                parent->add_to_object(final_key, std::move(value));
            } else if (parent->is_array()) {
                // Extend array as needed
                size_t index = std::stoull(final_key);
                extend_array_to_size(*parent, index + 1);
                (*parent->storage_.array)[index] = std::move(value);
            }
        }

        void set_value_at_path(const std::string& path, JsonDocument value) {
            std::vector<std::string> components = parse_json_pointer(path);
            if (components.empty()) {
                return;
            }

            ensure_root_initialized(components);
            JsonDocument* parent = navigate_to_parent(components);
            assign_final_value(parent, components.back(), std::move(value));
        }

        static auto parse_json_pointer(const std::string& path) -> std::vector<std::string> {
            std::vector<std::string> components;
            if (path.empty() || path == "/") {
                return components;
            }

            size_t start = (path[0] == '/') ? 1 : 0;
            size_t pos = start;

            while (pos < path.length()) {
                size_t next_slash = path.find('/', pos);
                if (next_slash == std::string::npos) {
                    next_slash = path.length();
                }

                if (next_slash > pos) {
                    components.push_back(path.substr(pos, next_slash - pos));
                }
                pos = next_slash + 1;
            }

            return components;
        }

        static auto is_numeric(const std::string& str) -> bool {
            return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit);
        }

        static auto create_container_for_key(const std::string& next_key) -> JsonDocument {
            JsonDocument new_container;
            if (is_numeric(next_key)) {
                new_container.init_as_array();
            } else {
                new_container.init_as_object();
            }
            return new_container;
        }

        // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
        static auto ensure_object_child_exists(JsonDocument* parent, const std::string& key,
                                               const std::string& next_key) -> JsonDocument* {
            auto* existing = parent->get_object_ref(key);
            if (existing == nullptr) {
                JsonDocument new_container = create_container_for_key(next_key);
                parent->add_to_object(key, std::move(new_container));
                existing = parent->get_object_ref(key);
            }
            return existing;
        }

        // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
        static auto ensure_array_element_exists(JsonDocument* parent, const std::string& key,
                                                const std::string& next_key) -> JsonDocument* {
            size_t index = std::stoull(key);
            extend_array_to_size(*parent, index + 1);

            JsonDocument& element = (*parent->storage_.array)[index];
            if (element.is_null()) {
                // Initialize based on next key
                if (is_numeric(next_key)) {
                    element.init_as_array();
                } else {
                    element.init_as_object();
                }
            }
            return &element;
        }

        static auto ensure_container_exists(JsonDocument* parent, const std::string& key,
                                            const std::string& next_key) -> JsonDocument* {
            if (parent->is_object()) {
                return ensure_object_child_exists(parent, key, next_key);
            }
            if (parent->is_array()) {
                return ensure_array_element_exists(parent, key, next_key);
            }
            return nullptr;
        }

        static void extend_array_to_size(JsonDocument& arr, size_t required_size) {
            if (arr.is_array() && arr.storage_.array->size() < required_size) {
                arr.storage_.array->resize(required_size);
            }
        }

        void handle_error(const ParseError& error) {
            has_error_ = true;
            error_message_ = error.message;
        }
    };

public:
    // Main parsing interface
    static auto parse(const std::string& json) -> JsonDocument {
        DocumentBuilder builder;
        return builder.build_from_json(json);
    }
};

// Convenience functions
inline auto parse_document(const std::string& json) -> JsonDocument {
    return BatchParser::parse(json);
}

} // namespace jsom
