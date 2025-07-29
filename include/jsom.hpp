#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
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

class JsonValue;
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
    std::function<void(const JsonValue&)> on_value;
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
    std::size_t array_index{0};

    ParseContext(PathNode* path_node, ContainerType container_type)
        : node(path_node), type(container_type),
          expecting_key(container_type == ContainerType::Object) {}
};

class JsonValue {
private:
    JsonType type_;
    std::string raw_value_;
    PathNode* path_node_;

public:
    JsonValue(JsonType type, std::string value, PathNode* node)
        : type_(type), raw_value_(std::move(value)), path_node_(node) {}

    [[nodiscard]] auto type() const -> JsonType { return type_; }
    [[nodiscard]] auto raw_value() const -> const std::string& { return raw_value_; }
    [[nodiscard]] auto path() const -> std::string {
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
        } else {
            emit_error("Unexpected character in start state");
        }
    }

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
            // End of number - emit value and handle the terminating character
            complete_number_value();
            // Process the terminating character in the appropriate state
            process_character_in_state(c);
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
        return_to_container_or_start();
        value_buffer_.clear();
    }

    void complete_number_value() {
        emit_value(JsonType::Number, value_buffer_);
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
        emit_value(JsonType::String, value_buffer_);
        return_to_container_or_start();
        value_buffer_.clear();
        in_escape_ = false;
    }

    void emit_value(JsonType type, const std::string& value) {
        if (events_.on_value) {
            // For now, use root_node as path (Phase 6 will implement proper path tracking)
            JsonValue json_value(type, value, root_node_);
            events_.on_value(json_value);
        }
    }

    void emit_error(const std::string& message) {
        if (events_.on_error) {
            events_.on_error({position_, message, ""});
        }
    }

    void return_to_container_or_start() {
        if (context_stack_.empty()) {
            state_ = ParseState::Start;
        } else {
            ParseContext& current = context_stack_.back();
            state_ = (current.type == ContainerType::Object) ? ParseState::InObject : ParseState::InArray;
        }
    }

    void handle_object_opening() {
        // Create new ParseContext for object
        ParseContext context(root_node_, ContainerType::Object);
        context_stack_.push_back(context);
        
        // Emit on_enter_object event
        if (events_.on_enter_object) {
            events_.on_enter_object("");  // No key for root object
        }
        
        // Transition to InObject state
        state_ = ParseState::InObject;
    }

    void handle_array_opening() {
        // Create new ParseContext for array
        ParseContext context(root_node_, ContainerType::Array);
        context_stack_.push_back(context);
        
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

    // NOLINTNEXTLINE(readability-identifier-length)
    void handle_object_state(char c) {
        // Skip whitespace
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            return;
        }

        if (c == '}') {
            // End of object
            handle_object_closing();
        } else if (c == ',') {
            // Comma separator - just skip for now (Phase 5 will handle properly)
            return;
        } else {
            // For now, treat any other character as start of value
            // This will be expanded in Phase 5 for key-value parsing
            handle_start_state(c);
        }
    }

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
            // Comma separator - just skip for now (Phase 5 will handle properly)
            return;
        } else {
            // For now, treat any other character as start of value
            // This will be expanded in Phase 5 for element parsing
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

    void end_input() {
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
            if (events_.on_error) {
                events_.on_error({position_, "Unexpected end of input: unclosed containers", ""});
            }
        }
    }

    void set_events(const ParseEvents& events) { events_ = events; }

    void reset() {
        state_ = ParseState::Start;
        context_stack_.clear();
        value_buffer_.clear();
        position_ = 0;
        in_escape_ = false;
        unicode_buffer_.clear();
        allocator_->reset();

        root_node_ = static_cast<PathNode*>(allocator_->allocate(sizeof(PathNode)));
        new (root_node_) PathNode(nullptr, "");
    }
};

} // namespace jsom