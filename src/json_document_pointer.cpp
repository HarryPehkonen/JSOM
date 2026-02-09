#include "jsom/json_document.hpp"
#include "jsom/json_pointer.hpp"
#include "jsom/path_cache.hpp"
#include "jsom/navigation_engine.hpp"
#include <memory>

namespace jsom {

// Destructor implementation
JsonDocument::~JsonDocument() {
    delete path_cache_;
}

// Copy assignment operator
auto JsonDocument::operator=(const JsonDocument& other) -> JsonDocument& {
    if (this != &other) {
        type_ = other.type_;
        storage_ = other.storage_;
        delete path_cache_; // Clear cache on copy - will be recreated if needed
        path_cache_ = nullptr;
    }
    return *this;
}

// Move assignment operator
auto JsonDocument::operator=(JsonDocument&& other) noexcept -> JsonDocument& {
    if (this != &other) {
        type_ = other.type_;
        storage_ = std::move(other.storage_);
        delete path_cache_;
        path_cache_ = other.path_cache_;
        other.path_cache_ = nullptr;
    }
    return *this;
}

// Get or create path cache for this document instance
auto JsonDocument::get_path_cache() const -> PathCache& {
    if (path_cache_ == nullptr) {
        path_cache_ = new PathCache();
    }
    return *path_cache_;
}

// JSON Pointer implementation for JsonDocument
auto JsonDocument::get_json_pointer() -> std::string {
    // This would require parent tracking during document construction
    // For now, we'll throw since this requires structural changes
    throw std::runtime_error("get_json_pointer() requires parent tracking - not implemented in current architecture");
}

auto JsonDocument::at(const std::string& json_pointer) const -> const JsonDocument& {
    auto& cache = this->get_path_cache();
    auto result = NavigationEngine::navigate_with_cache(
        const_cast<JsonDocument*>(this), json_pointer, cache);
    
    if (result.target == nullptr) {
        throw JsonPointerNotFoundException(json_pointer);
    }
    
    return *result.target;
}

auto JsonDocument::at(const std::string& json_pointer) -> JsonDocument& {
    auto& cache = this->get_path_cache();
    auto result = NavigationEngine::navigate_with_cache(this, json_pointer, cache);
    
    if (result.target == nullptr) {
        throw JsonPointerNotFoundException(json_pointer);
    }
    
    return *result.target;
}

auto JsonDocument::find(const std::string& json_pointer) const -> const JsonDocument* {
    try {
        auto& cache = this->get_path_cache();
        auto result = NavigationEngine::navigate_with_cache(
            const_cast<JsonDocument*>(this), json_pointer, cache);
        return result.target;
    } catch (const JsonPointerException&) {
        return nullptr;
    }
}

auto JsonDocument::find(const std::string& json_pointer) -> JsonDocument* {
    try {
        auto& cache = this->get_path_cache();
        auto result = NavigationEngine::navigate_with_cache(this, json_pointer, cache);
        return result.target;
    } catch (const JsonPointerException&) {
        return nullptr;
    }
}

auto JsonDocument::exists(const std::string& json_pointer) const -> bool {
    auto& cache = this->get_path_cache();
    return NavigationEngine::exists(const_cast<JsonDocument*>(this), json_pointer, cache);
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
    if (parent->is_object()) {
        parent->set(final_segment, value);
    } else if (parent->is_array()) {
        if (!JsonPointer::is_array_index(final_segment)) {
            throw JsonPointerTypeException(json_pointer, "array", "object");
        }
        size_t index = JsonPointer::to_array_index(final_segment);
        parent->set(index, value);
    } else {
        throw JsonPointerTypeException(json_pointer, "object or array", 
                                     parent->is_null() ? "null" : 
                                     parent->is_bool() ? "boolean" :
                                     parent->is_number() ? "number" : "string");
    }
    
    // Clear path cache since structure changed
    if (path_cache_ != nullptr) { path_cache_->clear(); }
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
                if (path_cache_ != nullptr) { path_cache_->clear(); }
                return true;
            }
        } else if (parent->is_array()) {
            if (!JsonPointer::is_array_index(final_segment)) {
                return false;
            }
            size_t index = JsonPointer::to_array_index(final_segment);
            auto& arr = std::get<std::vector<JsonDocument>>(parent->storage_);
            if (index < arr.size()) {
                arr.erase(arr.begin() + index);
                if (path_cache_ != nullptr) { path_cache_->clear(); }
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

auto JsonDocument::at_multiple(const std::vector<std::string>& paths) const -> std::vector<const JsonDocument*> {
    auto& cache = this->get_path_cache();
    auto results = NavigationEngine::navigate_multiple(
        const_cast<JsonDocument*>(this), paths, cache);
    
    std::vector<const JsonDocument*> const_results;
    const_results.reserve(results.size());
    
    for (auto* result : results) {
        const_results.push_back(result);
    }
    
    return const_results;
}

auto JsonDocument::at_multiple(const std::vector<std::string>& paths) -> std::vector<JsonDocument*> {
    auto& cache = this->get_path_cache();
    return NavigationEngine::navigate_multiple(this, paths, cache);
}

auto JsonDocument::exists_multiple(const std::vector<std::string>& paths) const -> std::vector<bool> {
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

auto JsonDocument::count_paths() const -> size_t {
    return list_paths().size();
}

void JsonDocument::precompute_paths(int max_depth) const {
    auto paths = NavigationEngine::enumerate_paths(*this, max_depth);
    auto& cache = this->get_path_cache();
    
    // Pre-populate cache with all paths
    for (const auto& path : paths) {
        try {
            auto result = NavigationEngine::navigate_with_cache(
                const_cast<JsonDocument*>(this), path, cache);
        } catch (const JsonPointerException&) {
            // Ignore navigation failures during precomputation
        }
    }
}

void JsonDocument::warm_path_cache(const std::vector<std::string>& likely_paths) const {
    auto& cache = this->get_path_cache();
    
    for (const auto& path : likely_paths) {
        try {
            auto result = NavigationEngine::navigate_with_cache(
                const_cast<JsonDocument*>(this), path, cache);
        } catch (const JsonPointerException&) {
            // Ignore failures during cache warming
        }
    }
}

void JsonDocument::clear_path_cache() const {
    if (path_cache_ != nullptr) {
        path_cache_->clear();
    }
}

void JsonDocument::invalidate_cache() {
    // Always notify the global epoch, even if this document has no cache.
    // A parent document's cache may hold pointers into our storage that
    // become dangling after reallocation (e.g. vector::push_back).
    PathCache::notify_mutation();
    if (path_cache_ != nullptr) {
        path_cache_->clear();
    }
}

auto JsonDocument::get_path_cache_stats() const -> JsonDocument::PathCacheStats {
    auto& cache = this->get_path_cache();
    auto stats = cache.get_stats();
    
    PathCacheStats result;
    result.exact_cache_size = stats.exact_cache_size;
    result.prefix_cache_size = stats.prefix_cache_size;
    result.total_entries = stats.total_entries;
    result.memory_usage_estimate = stats.memory_usage_estimate;
    result.avg_prefix_length = stats.avg_prefix_length;
    
    return result;
}

}