#pragma once

#include "jsom/allocators.hpp"
#include "jsom/json_value.hpp"
#include "jsom/path_node.hpp"
#include <functional>
#include <memory>

namespace jsom {

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
    enum State { Start, InObject, InArray, InString, InNumber, InLiteral };
    State current_state_ = Start;

    // Event callbacks
    ParseEvents events_;

    // Parsing buffer
    std::string input_buffer_;
    size_t current_position_ = 0;

public:
    explicit StreamingParser(std::unique_ptr<IAllocator> allocator = nullptr);
    ~StreamingParser();

    // Non-copyable but movable
    StreamingParser(const StreamingParser&) = delete;
    auto operator=(const StreamingParser&) -> StreamingParser& = delete;
    StreamingParser(StreamingParser&&) = default;
    auto operator=(StreamingParser&&) -> StreamingParser& = default;

    // Configuration
    void set_events(const ParseEvents& events) { events_ = events; }

    // Parsing interface
    void parse_string(const std::string& json);
    void feed_character(char ch);
    void reset();

    // Path iteration (for accessing all discovered paths)
    class PathIterator {
        // TODO: Implement iterator over all paths
    };

    auto begin() -> PathIterator;
    auto end() -> PathIterator;

    // Statistics
    auto total_paths() const -> size_t;
    auto memory_usage() const -> size_t;

private:
    void initialize_allocator();
    void process_character(char ch);
    void emit_value(const std::string& pointer, const JsonValue& value);
};

} // namespace jsom