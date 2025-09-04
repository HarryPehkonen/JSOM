#pragma once

#include "json_document.hpp"
#include "json_pointer.hpp"
#include "path_cache.hpp"
#include <string>
#include <vector>

namespace jsom {

// Navigation result with intermediate caching information
struct NavigationResult {
    JsonDocument* target{nullptr};
    std::vector<std::pair<std::string, JsonDocument*>> intermediate_nodes;
    bool cache_hit{false};
    size_t steps_navigated{0};

    NavigationResult() = default;
};

// Core navigation engine with prefix optimization
class NavigationEngine {
public:
    // Navigate to JSON Pointer with caching
    static auto navigate_with_cache(JsonDocument* root, const std::string& json_pointer,
                                    PathCache& cache) -> NavigationResult {

        // Validate pointer
        JsonPointer::validate(json_pointer);

        NavigationResult result;

        // Try exact cache first
        if (auto* cached = cache.get_exact(json_pointer)) {
            result.target = cached;
            result.cache_hit = true;
            result.steps_navigated = 0;
            return result;
        }

        // Find best cached prefix
        auto [start_node, remaining_path] = cache.find_best_prefix(json_pointer);
        if (start_node == nullptr) {
            start_node = root;
        }

        // Navigate remaining path
        result = navigate_and_cache_intermediate(start_node, remaining_path, json_pointer, cache);

        // Cache final result
        if (result.target != nullptr) {
            cache.put_exact(json_pointer, result.target);
        }

        return result;
    }

    // Navigate without caching (for internal use)
    static auto navigate_simple(JsonDocument* root, const std::string& json_pointer)
        -> JsonDocument* {
        if (json_pointer.empty()) {
            return root;
        }

        auto segments = JsonPointer::parse(json_pointer);
        JsonDocument* current = root;

        for (const auto& segment : segments) {
            current = navigate_single_step(current, segment);
            if (current == nullptr) {
                return nullptr;
            }
        }

        return current;
    }

    // Check if path exists
    static auto exists(JsonDocument* root, const std::string& json_pointer, PathCache& cache)
        -> bool {
        try {
            auto result = navigate_with_cache(root, json_pointer, cache);
            return result.target != nullptr;
        } catch (const JsonPointerException&) {
            return false; // Path not found
        }
    }

    // Batch navigation for multiple paths (optimized)
    // NOLINTBEGIN(readability-function-size)
    static auto navigate_multiple(JsonDocument* root, const std::vector<std::string>& paths,
                                  PathCache& cache) -> std::vector<JsonDocument*> {

        std::vector<JsonDocument*> results;
        results.reserve(paths.size());

        // Sort paths to maximize prefix reuse potential
        std::vector<std::pair<std::string, size_t>> sorted_paths;
        sorted_paths.reserve(paths.size());
        for (size_t i = 0; i < paths.size(); ++i) {
            sorted_paths.emplace_back(paths[i], i);
        }

        std::sort(sorted_paths.begin(), sorted_paths.end(),
                  // NOLINTNEXTLINE(readability-identifier-length)
                  [](const auto& a, const auto& b) { return a.first < b.first; });

        // Navigate in sorted order
        std::vector<JsonDocument*> sorted_results;
        sorted_results.reserve(paths.size());

        for (const auto& [path, _] : sorted_paths) {
            auto result = navigate_with_cache(root, path, cache);
            sorted_results.push_back(result.target);
        }

        // Restore original order
        results.resize(paths.size());
        for (size_t i = 0; i < sorted_paths.size(); ++i) {
            size_t original_index = sorted_paths[i].second;
            results[original_index] = sorted_results[i];
        }

        return results;
    }
    // NOLINTEND(readability-function-size)

private:
    // Navigate remaining path and cache intermediate steps
    // NOLINTBEGIN(readability-function-size)
    static auto navigate_and_cache_intermediate(JsonDocument* start_node,
                                                const std::string& remaining_path,
                                                const std::string& full_path, PathCache& cache)
        -> NavigationResult {

        NavigationResult result;
        result.target = start_node;

        if (remaining_path.empty()) {
            return result; // Already at target
        }

        auto segments = JsonPointer::parse(remaining_path);
        JsonDocument* current = start_node;

        // Calculate base path (prefix that was already cached)
        std::string base_path;
        if (remaining_path == full_path) {
            // No prefix was cached, start from root
            base_path = "";
        } else {
            // Some prefix was cached
            base_path = full_path.substr(0, full_path.length() - remaining_path.length());
            if (!base_path.empty() && base_path.back() == '/') {
                base_path.pop_back(); // Remove trailing slash
            }
        }

        std::string current_path = base_path;

        for (size_t i = 0; i < segments.size(); ++i) {
            const auto& segment = segments[i];

            // Build current path
            current_path += "/" + JsonPointer::escape_segment(segment);

            // Navigate one step
            current = navigate_single_step(current, segment);
            if (current == nullptr) {
                throw JsonPointerNotFoundException(current_path);
            }

            // Cache intermediate step (but not the final result - that's handled by caller)
            if (i < segments.size() - 1) {
                result.intermediate_nodes.emplace_back(current_path, current);
            }

            result.steps_navigated++;
        }

        // Cache all intermediate steps
        cache.cache_intermediate_steps(result.intermediate_nodes);

        result.target = current;
        return result;
    }
    // NOLINTEND(readability-function-size)

    // Navigate a single step (one segment)
    // NOLINTBEGIN(readability-function-size)
    static auto navigate_single_step(JsonDocument* current, const std::string& segment)
        -> JsonDocument* {
        if (current == nullptr) {
            return nullptr;
        }

        try {
            if (current->is_object()) {
                // Object access
                auto& obj = std::get<std::map<std::string, JsonDocument>>(current->storage_);
                // NOLINTNEXTLINE(readability-identifier-length)
                auto it = obj.find(segment);
                if (it != obj.end()) {
                    return &it->second;
                }
                return nullptr; // Key not found
            }
            if (current->is_array()) {
                // Array access
                if (!JsonPointer::is_array_index(segment)) {
                    return nullptr; // Invalid array index
                }

                size_t index = JsonPointer::to_array_index(segment);
                auto& arr = std::get<std::vector<JsonDocument>>(current->storage_);

                if (index >= arr.size()) {
                    return nullptr; // Index out of bounds
                }

                return &arr[index];

            } // Cannot navigate into primitive types
            return nullptr;

        } catch (const std::exception&) {
            return nullptr; // Navigation failed
        }
    }
    // NOLINTEND(readability-function-size)

public:
    // Utility: Enumerate all paths in a document
    static auto enumerate_paths(const JsonDocument& root, int max_depth = -1,
                                const std::string& prefix = "") -> std::vector<std::string> {

        std::vector<std::string> paths;
        enumerate_paths_recursive(root, prefix, paths, 0, max_depth);
        return paths;
    }

private:
    static void enumerate_paths_recursive(const JsonDocument& node, const std::string& current_path,
                                          std::vector<std::string>& paths, int current_depth,
                                          int max_depth) {

        // Add current path
        paths.push_back(current_path);

        // Check depth limit
        if (max_depth >= 0 && current_depth >= max_depth) {
            return;
        }

        if (node.is_object()) {
            const auto& obj = std::get<std::map<std::string, JsonDocument>>(node.storage_);
            for (const auto& [key, value] : obj) {
                std::string child_path = current_path + "/" + JsonPointer::escape_segment(key);
                enumerate_paths_recursive(value, child_path, paths, current_depth + 1, max_depth);
            }
        } else if (node.is_array()) {
            const auto& arr = std::get<std::vector<JsonDocument>>(node.storage_);
            for (size_t i = 0; i < arr.size(); ++i) {
                std::string child_path = current_path + "/" + std::to_string(i);
                enumerate_paths_recursive(arr[i], child_path, paths, current_depth + 1, max_depth);
            }
        }
    }
};

} // namespace jsom
