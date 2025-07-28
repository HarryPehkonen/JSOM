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
    return (c & ASCII_MASK) == 0 ||                              // ASCII (0xxxxxxx)
           (c & TWO_BYTE_MASK) == TWO_BYTE_MARKER ||             // 2-byte (110xxxxx)
           (c & THREE_BYTE_MASK) == THREE_BYTE_MARKER ||         // 3-byte (1110xxxx)
           (c & FOUR_BYTE_MASK) == FOUR_BYTE_MARKER;             // 4-byte (11110xxx)
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
        result += static_cast<char>(CONTINUATION_MARKER | ((codepoint >> SHIFT_6_BITS) & DATA_BITS_MASK));
        result += static_cast<char>(CONTINUATION_MARKER | (codepoint & DATA_BITS_MASK));
    } else if (codepoint <= UNICODE_MAX) {
        result += static_cast<char>(FOUR_BYTE_MARKER | (codepoint >> SHIFT_18_BITS));
        result += static_cast<char>(CONTINUATION_MARKER | ((codepoint >> SHIFT_12_BITS) & DATA_BITS_MASK));
        result += static_cast<char>(CONTINUATION_MARKER | ((codepoint >> SHIFT_6_BITS) & DATA_BITS_MASK));
        result += static_cast<char>(CONTINUATION_MARKER | (codepoint & DATA_BITS_MASK));
    }

    return result;
}
} // namespace utf8

// Default allocator block size
constexpr std::size_t DEFAULT_ARENA_BLOCK_SIZE = 4096;

class JsonValue;
struct PathNode;
struct ParseError;

enum class JsonType : std::uint8_t { Null, Boolean, Number, String, Object, Array };

enum class ContainerType : std::uint8_t { Object, Array };

enum class ParseState : std::uint8_t { Start, InString, InNumber, InLiteral, InObject, InArray };

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
    explicit ArenaAllocator(std::size_t block_size = DEFAULT_ARENA_BLOCK_SIZE) : block_size_(block_size) {}

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
    static void process_character_in_state(char c) {
        // Character processing logic will be implemented in Phase 2
        // This method eliminates the switch with identical branches warning
        (void)c; // Suppress unused parameter warning
    }

public:

    void parse_string(const std::string& json) {
        // NOLINTNEXTLINE(readability-identifier-length)
        for (char c : json) {
            feed_character(c);
        }
    }

    void end_input() {
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
        allocator_->reset();

        root_node_ = static_cast<PathNode*>(allocator_->allocate(sizeof(PathNode)));
        new (root_node_) PathNode(nullptr, "");
    }
};

} // namespace jsom