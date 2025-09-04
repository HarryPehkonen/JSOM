#pragma once

#include "constants.hpp"
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace jsom {

// Forward declaration
class JsonDocument;

// Exception hierarchy for JSON Pointer operations
class JsonPointerException : public std::runtime_error {
public:
    JsonPointerException(const std::string& pointer, const std::string& message)
        : std::runtime_error("JSON Pointer '" + pointer + "': " + message), pointer_(pointer) {}

    [[nodiscard]] auto get_pointer() const -> const std::string& { return pointer_; }

private:
    std::string pointer_;
};

class InvalidJsonPointerException : public JsonPointerException {
public:
    InvalidJsonPointerException(const std::string& pointer, const std::string& reason)
        : JsonPointerException(pointer, "Invalid JSON Pointer - " + reason) {}
};

class JsonPointerNotFoundException : public JsonPointerException {
public:
    JsonPointerNotFoundException(const std::string& pointer)
        : JsonPointerException(pointer, "Path not found") {}
};

class JsonPointerTypeException : public JsonPointerException {
public:
    JsonPointerTypeException(const std::string& pointer, const std::string& expected,
                             const std::string& actual)
        : JsonPointerException(pointer,
                               "Type mismatch - expected " + expected + " but got " + actual) {}
};

// RFC 6901 JSON Pointer utilities
class JsonPointer {
public:
    // Parse JSON Pointer into segments
    static auto parse(const std::string& pointer) -> std::vector<std::string> {
        if (pointer.empty()) {
            return {}; // Root pointer
        }

        if (pointer[0] != '/') {
            throw InvalidJsonPointerException(pointer, "must start with '/'");
        }

        std::vector<std::string> segments;
        std::string current_segment;

        for (size_t i = 1; i < pointer.length(); ++i) {
            // NOLINTNEXTLINE(readability-identifier-length)
            char c = pointer[i];
            if (c == '/') {
                segments.push_back(unescape_segment(current_segment));
                current_segment.clear();
            } else {
                current_segment += c;
            }
        }

        if (!current_segment.empty() || pointer.back() == '/') {
            segments.push_back(unescape_segment(current_segment));
        }

        return segments;
    }

    // Build JSON Pointer from segments
    static auto build(const std::vector<std::string>& segments) -> std::string {
        if (segments.empty()) {
            return ""; // Root pointer
        }

        std::string result;
        result.reserve(segments.size() * pointer_constants::SEGMENT_RESERVE_MULTIPLIER); // Estimate

        for (const auto& segment : segments) {
            result += "/";
            result += escape_segment(segment);
        }

        return result;
    }

    // Escape segment according to RFC 6901
    static auto escape_segment(const std::string& segment) -> std::string {
        std::string result;
        result.reserve(segment.length()
                       + (segment.length() / pointer_constants::ESCAPE_RESERVE_DIVISOR));

        // NOLINTNEXTLINE(readability-identifier-length)
        for (char c : segment) {
            if (c == '~') {
                result += "~0";
            } else if (c == '/') {
                result += "~1";
            } else {
                result += c;
            }
        }

        return result;
    }

    // Unescape segment according to RFC 6901
    static auto unescape_segment(const std::string& segment) -> std::string {
        std::string result;
        result.reserve(segment.length());

        for (size_t i = 0; i < segment.length(); ++i) {
            if (segment[i] == '~' && i + 1 < segment.length()) {
                if (segment[i + 1] == '0') {
                    result += '~';
                    ++i; // Skip next character
                } else if (segment[i + 1] == '1') {
                    result += '/';
                    ++i; // Skip next character
                } else {
                    throw InvalidJsonPointerException(
                        segment, "invalid escape sequence ~" + std::string(1, segment[i + 1]));
                }
            } else {
                result += segment[i];
            }
        }

        return result;
    }

    // Validate JSON Pointer format
    static auto is_valid(const std::string& pointer) -> bool {
        try {
            parse(pointer);
            return true;
        } catch (const JsonPointerException&) {
            return false;
        }
    }

    // Validate and throw if invalid
    static void validate(const std::string& pointer) {
        parse(pointer); // Will throw if invalid
    }

    // Check if pointer is array index
    static auto is_array_index(const std::string& segment) -> bool {
        if (segment.empty()) {
            return false;
        }
        if (segment == "0") {
            return true;
        }
        if (segment[0] == '0') {
            return false; // Leading zeros not allowed
        }

        return std::all_of(segment.begin(), segment.end(),
                           // NOLINTNEXTLINE(readability-identifier-length)
                           [](char c) { return c >= '0' && c <= '9'; });
    }

    // Convert segment to array index
    static auto to_array_index(const std::string& segment) -> size_t {
        if (!is_array_index(segment)) {
            throw InvalidJsonPointerException(segment, "not a valid array index");
        }

        try {
            return std::stoull(segment);
        } catch (const std::exception&) {
            throw InvalidJsonPointerException(segment, "array index out of range");
        }
    }

    // Get parent pointer
    static auto get_parent(const std::string& pointer) -> std::string {
        if (pointer.empty()) {
            throw InvalidJsonPointerException(pointer, "root has no parent");
        }

        size_t last_slash = pointer.find_last_of('/');
        if (last_slash == 0) {
            return ""; // Parent is root
        }

        return pointer.substr(0, last_slash);
    }

    // Get last segment
    static auto get_last_segment(const std::string& pointer) -> std::string {
        if (pointer.empty()) {
            throw InvalidJsonPointerException(pointer, "root has no segment");
        }

        size_t last_slash = pointer.find_last_of('/');
        return unescape_segment(pointer.substr(last_slash + 1));
    }

    // Check if one pointer is prefix of another
    static auto is_prefix(const std::string& prefix, const std::string& pointer) -> bool {
        if (prefix.empty()) {
            return true; // Root is prefix of everything
        }
        if (pointer.empty()) {
            return false; // Nothing is prefix of root except root
        }

        return pointer.substr(0, prefix.length()) == prefix
               && (pointer.length() == prefix.length() || pointer[prefix.length()] == '/');
    }

    // Get relative pointer (remove prefix)
    static auto make_relative(const std::string& prefix, const std::string& pointer)
        -> std::string {
        if (!is_prefix(prefix, pointer)) {
            throw InvalidJsonPointerException(pointer, "'" + prefix + "' is not a prefix");
        }

        if (prefix.empty()) {
            return pointer;
        }
        if (pointer.length() == prefix.length()) {
            return "";
        }

        return pointer.substr(prefix.length());
    }

    // Join two pointers
    static auto join(const std::string& base, const std::string& relative) -> std::string {
        if (relative.empty()) {
            return base;
        }
        if (base.empty()) {
            return relative;
        }

        return base + relative;
    }
};

} // namespace jsom
