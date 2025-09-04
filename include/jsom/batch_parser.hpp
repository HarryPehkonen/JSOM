#pragma once

#include "fast_parser.hpp"
#include "json_document.hpp"
#include "streaming_parser.hpp"
#include <stdexcept>

namespace jsom {

class DocumentBuilder {
private:
    JsonDocument root_;
    std::stack<JsonDocument*> container_stack_;
    bool has_root_{false};

    auto get_current_container() -> JsonDocument* {
        if (container_stack_.empty()) {
            return &root_;
        }
        return container_stack_.top();
    }

public:
    DocumentBuilder() = default;

    void on_value(const JsonDocument& value, const std::string& path) {
        if (path.empty()) {
            root_ = value;
            has_root_ = true;
        } else {
            set_value_at_path(value, path);
        }
    }

    void on_enter_object(const std::string& path) {
        JsonDocument obj(std::initializer_list<std::pair<const std::string, JsonDocument>>{});
        if (path.empty()) {
            root_ = obj;
            has_root_ = true;
        } else {
            set_value_at_path(obj, path);
        }
    }

    void on_enter_array(const std::string& path) {
        JsonDocument arr(std::initializer_list<JsonDocument>{});
        if (path.empty()) {
            root_ = arr;
            has_root_ = true;
        } else {
            set_value_at_path(arr, path);
        }
    }

    void on_exit_container(const std::string& /*unused*/) {}

    static void on_error(const ParseError& error) {
        throw std::runtime_error("Parse error at position " + std::to_string(error.position)
                                 + " (path: " + error.json_pointer + "): " + error.message);
    }

    auto get_document() const -> JsonDocument {
        if (!has_root_) {
            throw std::runtime_error("No document parsed");
        }
        return root_;
    }

    void reset() {
        root_ = JsonDocument();
        while (!container_stack_.empty()) {
            container_stack_.pop();
        }
        has_root_ = false;
    }

private:
    // NOLINTBEGIN(readability-function-size)
    void set_value_at_path(const JsonDocument& value, const std::string& path) {
        if (!has_root_) {
            throw std::runtime_error("Cannot set value - no root container");
        }

        std::vector<std::string> segments = parse_json_pointer(path);
        JsonDocument* current = &root_;

        for (size_t i = 0; i < segments.size(); ++i) {
            const std::string& segment = segments[i];

            if (i == segments.size() - 1) {
                if (current->is_object()) {
                    current->set(segment, value);
                } else if (current->is_array()) {
                    size_t index = std::stoull(segment);
                    current->set(index, value);
                }
            } else {
                if (current->is_object()) {
                    auto& obj_map
                        = std::get<std::map<std::string, JsonDocument>>(current->storage_);
                    if (obj_map.find(segment) == obj_map.end()) {
                        if (i + 1 < segments.size() && is_numeric(segments[i + 1])) {
                            current->set(segment,
                                         JsonDocument(std::initializer_list<JsonDocument>{}));
                        } else {
                            current->set(
                                segment,
                                JsonDocument(std::initializer_list<
                                             std::pair<const std::string, JsonDocument>>{}));
                        }
                    }
                    current = &obj_map[segment];
                } else if (current->is_array()) {
                    size_t index = std::stoull(segment);
                    auto& arr = std::get<std::vector<JsonDocument>>(current->storage_);
                    if (index >= arr.size()) {
                        arr.resize(index + 1);
                    }
                    current = &arr[index];
                }
            }
        }
    }
    // NOLINTEND(readability-function-size)

    static auto parse_json_pointer(const std::string& path) -> std::vector<std::string> {
        std::vector<std::string> segments;
        if (path.empty() || path[0] != '/') {
            return segments;
        }

        std::string current;
        for (size_t i = 1; i < path.length(); ++i) {
            if (path[i] == '/') {
                segments.push_back(unescape_json_pointer(current));
                current.clear();
            } else {
                current += path[i];
            }
        }
        if (!current.empty()) {
            segments.push_back(unescape_json_pointer(current));
        }

        return segments;
    }

    static auto unescape_json_pointer(const std::string& str) -> std::string {
        std::string result;
        for (size_t i = 0; i < str.length(); ++i) {
            if (str[i] == '~' && i + 1 < str.length()) {
                if (str[i + 1] == '0') {
                    result += '~';
                    ++i;
                } else if (str[i + 1] == '1') {
                    result += '/';
                    ++i;
                } else {
                    result += str[i];
                }
            } else {
                result += str[i];
            }
        }
        return result;
    }

    static auto is_numeric(const std::string& str) -> bool {
        if (str.empty()) {
            return false;
        }
        // NOLINTNEXTLINE(readability-identifier-length)
        return std::all_of(str.begin(), str.end(), [](char c) { return std::isdigit(c) != 0; });
    }
};

class SimpleDocumentBuilder {
private:
    JsonDocument result_;
    bool has_result_{false};

public:
    SimpleDocumentBuilder() = default;

    void on_value(const JsonDocument& value, const std::string& path) {
        if (path.empty()) {
            result_ = value;
            has_result_ = true;
        }
    }

    void on_enter_object(const std::string& path) {
        if (path.empty()) {
            result_
                = JsonDocument(std::initializer_list<std::pair<const std::string, JsonDocument>>{});
            has_result_ = true;
        }
    }

    void on_enter_array(const std::string& path) {
        if (path.empty()) {
            result_ = JsonDocument(std::initializer_list<JsonDocument>{});
            has_result_ = true;
        }
    }

    void on_exit_container(const std::string& /*unused*/) {}

    static void on_error(const ParseError& error) {
        throw std::runtime_error("Parse error at position " + std::to_string(error.position) + ": "
                                 + error.message);
    }

    auto get_result() const -> JsonDocument {
        if (!has_result_) {
            throw std::runtime_error("No document parsed");
        }
        return result_;
    }
};

inline auto parse_document(const std::string& json) -> JsonDocument {
    // Use fast parser by default for better performance
    FastParser parser;
    return parser.parse(json);
}

inline auto parse_document(const std::string& json, const JsonParseOptions& options) -> JsonDocument {
    // Use fast parser with options for better performance
    FastParser parser(options);
    return parser.parse(json);
}

// Legacy streaming parser (for compatibility/debugging)
inline auto parse_document_streaming(const std::string& json) -> JsonDocument {
    DocumentBuilder builder;
    StreamingParser parser;

    ParseEvents events;
    events.on_value = [&builder](const JsonDocument& value, const std::string& path) {
        builder.on_value(value, path);
    };
    events.on_enter_object = [&builder](const std::string& path) { builder.on_enter_object(path); };
    events.on_enter_array = [&builder](const std::string& path) { builder.on_enter_array(path); };
    events.on_exit_container
        = [&builder](const std::string& path) { builder.on_exit_container(path); };
    events.on_error
        = [&builder](const ParseError& error) { jsom::DocumentBuilder::on_error(error); };

    parser.set_events(events);
    parser.parse_string(json);
    parser.end_input();

    return builder.get_document();
}

} // namespace jsom
