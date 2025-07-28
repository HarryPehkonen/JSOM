#include "jsom/streaming_parser.hpp"
#include <stdexcept>

namespace jsom {

StreamingParser::StreamingParser(std::unique_ptr<IAllocator> allocator)
    : allocator_(std::move(allocator)) {
    initialize_allocator();

    // Create root node
    root_node_
        = reinterpret_cast<PathNode*>(allocator_->allocate(sizeof(PathNode), alignof(PathNode)));
    new (root_node_) PathNode();
    current_node_ = root_node_;
}

StreamingParser::~StreamingParser() { reset(); }

void StreamingParser::parse_string(const std::string& json) {
    input_buffer_ = json;
    current_position_ = 0;

    for (char ch : json) {
        try {
            process_character(ch);
        } catch (const std::exception& e) {
            if (events_.on_error) {
                events_.on_error(current_position_, e.what());
            }
            return;
        }
        ++current_position_;
    }
}

void StreamingParser::feed_character(char ch) {
    try {
        process_character(ch);
        ++current_position_;
    } catch (const std::exception& e) {
        if (events_.on_error) {
            events_.on_error(current_position_, e.what());
        }
    }
}

void StreamingParser::reset() {
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
}

auto StreamingParser::begin() -> PathIterator {
    // TODO: Implement
    return PathIterator{};
}

auto StreamingParser::end() -> PathIterator {
    // TODO: Implement
    return PathIterator{};
}

auto StreamingParser::total_paths() const -> size_t {
    // TODO: Implement - traverse tree and count nodes
    return 0;
}

auto StreamingParser::memory_usage() const -> size_t {
    if (auto* arena = dynamic_cast<ArenaAllocator*>(allocator_.get())) {
        return arena->total_allocated();
    }
    return 0; // Can't track StandardAllocator usage easily
}

void StreamingParser::initialize_allocator() {
    if (!allocator_) {
        allocator_ = std::make_unique<ArenaAllocator>();
    }
}

void StreamingParser::process_character(char ch) {
    // TODO: Implement full state machine
    // This is a placeholder that demonstrates the structure

    switch (current_state_) {
    case Start:
        if (ch == '{') {
            current_state_ = InObject;
            if (events_.on_enter_object) {
                events_.on_enter_object("");
            }
        } else if (ch == '[') {
            current_state_ = InArray;
            if (events_.on_enter_array) {
                events_.on_enter_array("");
            }
        }
        break;

    case InObject:
    case InArray:
    case InString:
    case InNumber:
    case InLiteral:
        // TODO: Implement state transitions
        break;
    }
}

void StreamingParser::emit_value(const std::string& pointer, const JsonValue& value) {
    if (events_.on_value) {
        events_.on_value(pointer, value);
    }
}

} // namespace jsom