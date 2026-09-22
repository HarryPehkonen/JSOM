#include "jsom/json_document.hpp"
#include "jsom/json_pointer.hpp"
#include "jsom/navigation_engine.hpp"
#include <memory>

namespace jsom {

// Destructor implementation (the storage variant is all there is to release)
JsonDocument::~JsonDocument() = default;

// Copy assignment operator
auto JsonDocument::operator=(const JsonDocument& other) -> JsonDocument& {
    if (this != &other) {
        type_ = other.type_;
        storage_ = other.storage_;
    }
    return *this;
}

// Move assignment operator
auto JsonDocument::operator=(JsonDocument&& other) noexcept -> JsonDocument& {
    if (this != &other) {
        type_ = other.type_;
        storage_ = std::move(other.storage_);
    }
    return *this;
}

// JSON Pointer implementation for JsonDocument
auto JsonDocument::get_json_pointer() -> std::string {
    // This would require parent tracking during document construction
    // For now, we'll throw since this requires structural changes
    //
    // Plain std::runtime_error, not ParseError: this is neither a parse nor a format
    // failure, just an unimplemented API surface.
    throw std::runtime_error(
        "get_json_pointer() requires parent tracking - not implemented in current architecture");
}

/// The reference returned here points INTO the document's own storage: it stays valid
/// until the document is modified. Treat any mutation (set, push_back, remove_at, ...) as
/// invalidating every reference and pointer obtained earlier — for arrays specifically,
/// the storage is a std::vector and a growth reallocates.
auto JsonDocument::at(const std::string& json_pointer) const -> const JsonDocument& {
    const JsonDocument* target = NavigationEngine::find(this, json_pointer);
    if (target == nullptr) {
        throw JsonPointerNotFoundException(json_pointer);
    }
    return *target;
}

auto JsonDocument::at(const std::string& json_pointer) -> JsonDocument& {
    JsonDocument* target = NavigationEngine::find(this, json_pointer);
    if (target == nullptr) {
        throw JsonPointerNotFoundException(json_pointer);
    }
    return *target;
}

auto JsonDocument::find(const std::string& json_pointer) const -> const JsonDocument* {
    try {
        return NavigationEngine::find(this, json_pointer);
    } catch (const JsonPointerException&) {
        return nullptr;
    }
}

auto JsonDocument::find(const std::string& json_pointer) -> JsonDocument* {
    try {
        return NavigationEngine::find(this, json_pointer);
    } catch (const JsonPointerException&) {
        return nullptr;
    }
}

auto JsonDocument::exists(const std::string& json_pointer) const -> bool {
    return NavigationEngine::exists(this, json_pointer);
}

void JsonDocument::set_at(const std::string& json_pointer, const JsonDocument& value) {
    set_at(json_pointer, JsonDocument(value));
}

void JsonDocument::set_at(const std::string& json_pointer, JsonDocument&& value) {
    if (json_pointer.empty()) {
        // Setting root
        *this = std::move(value);
        return;
    }

    // Parse the pointer
    auto segments = JsonPointer::parse(json_pointer);
    if (segments.empty()) {
        *this = std::move(value);
        return;
    }

    // Navigate to parent and set the final segment
    std::string parent_path = JsonPointer::get_parent(json_pointer);
    std::string final_segment = JsonPointer::get_last_segment(json_pointer);

    JsonDocument* parent = this;
    if (!parent_path.empty()) {
        parent = &at(parent_path);
    }

    // Set the value based on parent type
    if (JsonPointer::is_append(final_segment)) {
        // Append sentinel "-" (RFC 6902): only valid on arrays.
        if (!parent->is_array()) {
            throw JsonPointerTypeException(json_pointer, "array",
                                           parent->is_object()   ? "object"
                                           : parent->is_null()   ? "null"
                                           : parent->is_bool()   ? "boolean"
                                           : parent->is_number() ? "number"
                                                                 : "string");
        }
        parent->push_back(value);
    } else if (parent->is_object()) {
        parent->set(final_segment, value);
    } else if (parent->is_array()) {
        if (!JsonPointer::is_array_index(final_segment)) {
            throw JsonPointerTypeException(json_pointer, "array", "object");
        }
        size_t index = JsonPointer::to_array_index(final_segment);
        parent->set(index, value);
    } else {
        throw JsonPointerTypeException(json_pointer, "object or array",
                                       parent->is_null()     ? "null"
                                       : parent->is_bool()   ? "boolean"
                                       : parent->is_number() ? "number"
                                                             : "string");
    }
}

auto JsonDocument::remove_at(const std::string& json_pointer) -> bool {
    if (json_pointer.empty()) {
        // Cannot remove root
        return false;
    }

    try {
        std::string parent_path = JsonPointer::get_parent(json_pointer);
        std::string final_segment = JsonPointer::get_last_segment(json_pointer);

        JsonDocument* parent = this;
        if (!parent_path.empty()) {
            parent = &at(parent_path);
        }

        if (parent->is_object()) {
            auto& obj = std::get<std::map<std::string, JsonDocument>>(parent->storage_);
            auto it = obj.find(final_segment);
            if (it != obj.end()) {
                obj.erase(it);
                return true;
            }
        } else if (parent->is_array()) {
            if (!JsonPointer::is_array_index(final_segment)) {
                return false;
            }
            size_t index = JsonPointer::to_array_index(final_segment);
            auto& arr = std::get<std::vector<JsonDocument>>(parent->storage_);
            if (index < arr.size()) {
                // erase needs an iterator difference_type; index < size() (checked
                // above) so the explicit narrowing is safe and well-defined.
                arr.erase(arr.begin() + static_cast<std::ptrdiff_t>(index));
                return true;
            }
        }

        return false;
    } catch (const JsonPointerException&) {
        return false;
    }
}

auto JsonDocument::extract_at(const std::string& json_pointer) -> JsonDocument {
    JsonDocument result = at(json_pointer);
    if (!remove_at(json_pointer)) {
        throw JsonPointerNotFoundException(json_pointer);
    }
    return result;
}

auto JsonDocument::at_multiple(const std::vector<std::string>& paths) const
    -> std::vector<const JsonDocument*> {
    return NavigationEngine::find_multiple(this, paths);
}

auto JsonDocument::at_multiple(const std::vector<std::string>& paths)
    -> std::vector<JsonDocument*> {
    const auto found = NavigationEngine::find_multiple(this, paths);
    std::vector<JsonDocument*> results;
    results.reserve(found.size());
    for (const auto* item : found) {
        results.push_back(const_cast<JsonDocument*>(item));
    }
    return results;
}

auto JsonDocument::exists_multiple(const std::vector<std::string>& paths) const
    -> std::vector<bool> {
    std::vector<bool> results;
    results.reserve(paths.size());

    for (const auto& path : paths) {
        results.push_back(exists(path));
    }

    return results;
}

auto JsonDocument::list_paths(int max_depth) const -> std::vector<std::string> {
    return NavigationEngine::enumerate_paths(*this, max_depth);
}

auto JsonDocument::find_paths(const std::string& pattern) const -> std::vector<std::string> {
    auto all_paths = list_paths();
    std::vector<std::string> matching_paths;

    // Simple pattern matching (could be enhanced with regex or glob patterns)
    for (const auto& path : all_paths) {
        if (path.find(pattern) != std::string::npos) {
            matching_paths.push_back(path);
        }
    }

    return matching_paths;
}

auto JsonDocument::count_paths() const -> size_t { return list_paths().size(); }

} // namespace jsom