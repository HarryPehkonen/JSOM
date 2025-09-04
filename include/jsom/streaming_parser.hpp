#pragma once

#include "parse_events.hpp"
#include "path_node.hpp"
#include <cctype>
#include <stack>
#include <string>

namespace jsom {

enum class ParseState : std::uint8_t {
    Start,
    InString,
    InStringEscape,
    InNumber,
    InLiteral,
    ExpectingColon,
    ExpectingCommaOrEnd,
    Complete,
    Error
};

struct ParseContext {
    PathNode* node;
    ContainerType type;
    bool expecting_key;
    std::size_t array_index;

    // NOLINTNEXTLINE(readability-identifier-length)
    ParseContext(PathNode* n, ContainerType t, bool key = false, std::size_t idx = 0)
        : node(n), type(t), expecting_key(key), array_index(idx) {}
};

class StreamingParser {
private:
    ParseState state_;
    ParseEvents events_;
    PathManager path_manager_;
    std::stack<ParseContext> context_stack_;

    std::string value_buffer_;
    std::string literal_buffer_;
    std::size_t position_;

    PathNode* current_path_node_;

public:
    StreamingParser() { reset(); }

    void set_events(const ParseEvents& events) { events_ = events; }

    void reset() {
        state_ = ParseState::Start;
        while (!context_stack_.empty()) {
            context_stack_.pop();
        }
        value_buffer_.clear();
        literal_buffer_.clear();
        position_ = 0;
        path_manager_.reset();
        current_path_node_ = path_manager_.get_root();
    }

    // NOLINTNEXTLINE(readability-identifier-length)
    void feed_character(char c) {
        position_++;

        try {
            switch (state_) {
            case ParseState::Start:
                handle_start(c);
                break;
            case ParseState::InString:
                handle_in_string(c);
                break;
            case ParseState::InStringEscape:
                handle_string_escape(c);
                break;
            case ParseState::InNumber:
                handle_in_number(c);
                break;
            case ParseState::InLiteral:
                handle_in_literal(c);
                break;
            case ParseState::ExpectingColon:
                handle_expecting_colon(c);
                break;
            case ParseState::ExpectingCommaOrEnd:
                handle_expecting_comma_or_end(c);
                break;
            case ParseState::Complete:
            case ParseState::Error:
                if (std::isspace(c) == 0) {
                    emit_error("Unexpected character after complete JSON");
                }
                break;
            }
        } catch (const std::exception& e) {
            emit_error(e.what());
        }
    }

    void parse_string(const std::string& json) {
        // NOLINTNEXTLINE(readability-identifier-length)
        for (char c : json) {
            feed_character(c);
            if (state_ == ParseState::Error) {
                break;
            }
        }
    }

    void end_input() {
        if (state_ == ParseState::InNumber) {
            complete_number();
        } else if (state_ == ParseState::InLiteral) {
            complete_literal();
        } else if (state_ != ParseState::Complete && state_ != ParseState::Start) {
            emit_error("Unexpected end of input");
        }
    }

private:
    // NOLINTNEXTLINE(readability-identifier-length)
    void handle_start(char c) {
        if (std::isspace(c) != 0) {
            return;
        }

        switch (c) {
        case '"':
            start_string();
            break;
        case '{':
            start_object();
            break;
        case '[':
            start_array();
            break;
        case 't':
        case 'f':
        case 'n':
            start_literal(c);
            break;
        case '-':
            start_number(c);
            break;
        default:
            if (std::isdigit(c) != 0) {
                start_number(c);
            } else {
                emit_error("Unexpected character");
            }
            break;
        }
    }

    // NOLINTNEXTLINE(readability-identifier-length)
    [[nodiscard]] static auto is_number_start_char(char c) -> bool {
        return c == '-' || (std::isdigit(c) != 0);
    }

    // NOLINTNEXTLINE(readability-identifier-length)
    void handle_in_string(char c) {
        if (c == '"') {
            complete_string();
        } else if (c == '\\') {
            state_ = ParseState::InStringEscape;
        } else {
            value_buffer_ += c;
        }
    }

    // NOLINTNEXTLINE(readability-identifier-length)
    void handle_string_escape(char c) {
        switch (c) {
        case '"':
            value_buffer_ += '"';
            break;
        case '\\':
            value_buffer_ += '\\';
            break;
        case '/':
            value_buffer_ += '/';
            break;
        case 'b':
            value_buffer_ += '\b';
            break;
        case 'f':
            value_buffer_ += '\f';
            break;
        case 'n':
            value_buffer_ += '\n';
            break;
        case 'r':
            value_buffer_ += '\r';
            break;
        case 't':
            value_buffer_ += '\t';
            break;
        case 'u':
            emit_error("Unicode escape sequences not yet implemented");
            return;
        default:
            emit_error("Invalid escape sequence");
            return;
        }
        state_ = ParseState::InString;
    }

    // NOLINTNEXTLINE(readability-identifier-length)
    void handle_in_number(char c) {
        if ((std::isdigit(c) != 0) || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
            value_buffer_ += c;
        } else {
            complete_number();
            handle_post_value(c);
        }
    }

    // NOLINTNEXTLINE(readability-identifier-length)
    void handle_in_literal(char c) {
        if (std::isalpha(c) != 0) {
            literal_buffer_ += c;
        } else {
            complete_literal();
            handle_post_value(c);
        }
    }

    // NOLINTNEXTLINE(readability-identifier-length)
    void handle_expecting_colon(char c) {
        if (std::isspace(c) != 0) {
            return;
        }

        if (c == ':') {
            state_ = ParseState::Start;
        } else {
            emit_error("Expected ':'");
        }
    }

    // NOLINTNEXTLINE(readability-identifier-length)
    void handle_object_comma_or_end(char c) {
        auto& ctx = context_stack_.top();
        if (c == '}') {
            exit_container();
        } else if (c == ',') {
            ctx.expecting_key = true;
            state_ = ParseState::Start;
        } else {
            emit_error("Expected ',' or '}'");
        }
    }

    // NOLINTNEXTLINE(readability-identifier-length)
    void handle_array_comma_or_end(char c) {
        auto& ctx = context_stack_.top();
        if (c == ']') {
            exit_container();
        } else if (c == ',') {
            ctx.array_index++;
            current_path_node_ = ctx.node->get_array_child(ctx.array_index);
            state_ = ParseState::Start;
        } else {
            emit_error("Expected ',' or ']'");
        }
    }

    // NOLINTNEXTLINE(readability-identifier-length)
    void handle_expecting_comma_or_end(char c) {
        if (std::isspace(c) != 0) {
            return;
        }

        if (context_stack_.empty()) {
            if (std::isspace(c) == 0) {
                emit_error("Unexpected character after complete JSON");
            }
            return;
        }

        auto& ctx = context_stack_.top();
        if (ctx.type == ContainerType::Object) {
            handle_object_comma_or_end(c);
        } else { // Array
            handle_array_comma_or_end(c);
        }
    }

    // NOLINTNEXTLINE(readability-identifier-length)
    void handle_post_value(char c) {
        if (context_stack_.empty()) {
            state_ = ParseState::Complete;
            if (c != '\0' && (std::isspace(c) == 0)) {
                emit_error("Unexpected character after complete JSON");
            }
        } else {
            state_ = ParseState::ExpectingCommaOrEnd;
            if (c != '\0') {
                handle_expecting_comma_or_end(c);
            }
        }
    }

    void start_string() {
        value_buffer_.clear();
        state_ = ParseState::InString;
    }

    void complete_string() {
        JsonDocument value(value_buffer_);

        if (!context_stack_.empty() && context_stack_.top().type == ContainerType::Object
            && context_stack_.top().expecting_key) {

            auto& ctx = context_stack_.top();
            current_path_node_ = ctx.node->get_object_child(value_buffer_);
            ctx.expecting_key = false;
            state_ = ParseState::ExpectingColon;
        } else {
            emit_value(value);
            handle_post_value('\0');
        }
    }

    // NOLINTNEXTLINE(readability-identifier-length)
    void start_number(char c) {
        value_buffer_.clear();
        value_buffer_ += c;
        state_ = ParseState::InNumber;
    }

    void complete_number() {
        JsonDocument value = JsonDocument::from_lazy_number(value_buffer_);
        emit_value(value);
    }

    // NOLINTNEXTLINE(readability-identifier-length)
    void start_literal(char c) {
        literal_buffer_.clear();
        literal_buffer_ += c;
        state_ = ParseState::InLiteral;
    }

    void complete_literal() {
        JsonDocument value;

        if (literal_buffer_ == "true") {
            value = JsonDocument(true);
        } else if (literal_buffer_ == "false") {
            value = JsonDocument(false);
        } else if (literal_buffer_ == "null") {
            value = JsonDocument();
        } else {
            emit_error("Invalid literal: " + literal_buffer_);
            return;
        }

        emit_value(value);
    }

    void start_object() {
        events_.emit_enter_object(current_path_node_->get_json_pointer());
        context_stack_.emplace(current_path_node_, ContainerType::Object, true);
        state_ = ParseState::Start;
    }

    void start_array() {
        events_.emit_enter_array(current_path_node_->get_json_pointer());
        context_stack_.emplace(current_path_node_, ContainerType::Array, false, 0);
        current_path_node_ = context_stack_.top().node->get_array_child(0);
        state_ = ParseState::Start;
    }

    void exit_container() {
        if (context_stack_.empty()) {
            emit_error("Unexpected container close");
            return;
        }

        std::string path = context_stack_.top().node->get_json_pointer();
        events_.emit_exit_container(path);
        context_stack_.pop();

        if (context_stack_.empty()) {
            current_path_node_ = path_manager_.get_root();
            state_ = ParseState::Complete;
        } else {
            current_path_node_ = context_stack_.top().node;
            state_ = ParseState::ExpectingCommaOrEnd;
        }
    }

    void emit_value(const JsonDocument& value) {
        events_.emit_value(value, current_path_node_->get_json_pointer());
    }

    void emit_error(const std::string& message) {
        state_ = ParseState::Error;
        std::string path
            = (current_path_node_ != nullptr) ? current_path_node_->get_json_pointer() : "";
        events_.emit_error(ParseError(position_, message, path));
    }
};

} // namespace jsom
