#pragma once

#include "core_types.hpp"
#include "json_document.hpp"
#include <functional>
#include <string>

namespace jsom {

struct ParseError {
    std::size_t position;
    std::string message;
    std::string json_pointer;

    ParseError(std::size_t pos, std::string msg, const std::string& ptr = "")
        : position(pos), message(std::move(msg)), json_pointer(ptr) {}
};

class ParseEvents {
public:
    std::function<void(const JsonDocument&, const std::string&)> on_value;
    std::function<void(const std::string&)> on_enter_object;
    std::function<void(const std::string&)> on_enter_array;
    std::function<void(const std::string&)> on_exit_container;
    std::function<void(const ParseError&)> on_error;

    ParseEvents() = default;

    void emit_value(const JsonDocument& value, const std::string& path) const {
        if (on_value) {
            on_value(value, path);
        }
    }

    void emit_enter_object(const std::string& path) const {
        if (on_enter_object) {
            on_enter_object(path);
        }
    }

    void emit_enter_array(const std::string& path) const {
        if (on_enter_array) {
            on_enter_array(path);
        }
    }

    void emit_exit_container(const std::string& path) const {
        if (on_exit_container) {
            on_exit_container(path);
        }
    }

    void emit_error(const ParseError& error) const {
        if (on_error) {
            on_error(error);
        }
    }
};

} // namespace jsom