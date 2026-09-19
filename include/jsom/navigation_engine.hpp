#pragma once

#include "json_document.hpp"
#include "json_pointer.hpp"
#include <string>
#include <vector>

namespace jsom {

/// JSON Pointer navigation over a document.
///
/// Navigation is pure reading: it caches nothing and modifies nothing, so a const
/// document can be navigated from several threads at once (pinned by the `tsan` gate).
///
/// This used to be wrapped in a three-level path cache owned by every JsonDocument.
/// Measured 2026-09-19 (tools/cache_probe.cpp, -O3 -march=native, 100 KB document), the
/// cache was a net loss — 1.07x slower on a shared-prefix sweep, 17.97x slower reading
/// every path once, break-even on repeated shallow lookups, and only a win (1.6x) for
/// repeating one DEEP path — while making const access mutate hidden state (a data race)
/// and retaining ~72 KB for a 1000-record document. If a caller needs the deep-repeat
/// case, holding on to the pointer from the first lookup is all it takes.
class NavigationEngine {
public:
    /// Navigates a pointer from a const document. nullptr if the path is absent.
    [[nodiscard]] static auto find(const JsonDocument* root, const std::string& json_pointer)
        -> const JsonDocument* {
        if (json_pointer.empty()) {
            return root;
        }

        const auto segments = JsonPointer::parse(json_pointer);
        const JsonDocument* current = root;

        for (const auto& segment : segments) {
            current = navigate_single_step(current, segment);
            if (current == nullptr) {
                return nullptr;
            }
        }

        return current;
    }

    /// Mutable navigation. The single const_cast lives here, once, so no caller needs
    /// its own: a pointer obtained from a non-const document may be modified.
    [[nodiscard]] static auto find(JsonDocument* root, const std::string& json_pointer)
        -> JsonDocument* {
        return const_cast<JsonDocument*>(
            find(static_cast<const JsonDocument*>(root), json_pointer));
    }

    /// True if the pointer resolves to a value.
    static auto exists(const JsonDocument* root, const std::string& json_pointer) -> bool {
        try {
            return find(root, json_pointer) != nullptr;
        } catch (const JsonPointerException&) {
            return false; // Malformed pointer, or not found
        }
    }

    /// Navigates several pointers, results in the same order as `paths`.
    /// Returns nullptr per missing path; a malformed pointer throws.
    static auto find_multiple(const JsonDocument* root, const std::vector<std::string>& paths)
        -> std::vector<const JsonDocument*> {
        std::vector<const JsonDocument*> results;
        results.reserve(paths.size());
        for (const auto& path : paths) {
            results.push_back(find(root, path));
        }
        return results;
    }

    /// Every path in the document, depth-first, in document order.
    static auto enumerate_paths(const JsonDocument& root, int max_depth = -1,
                                const std::string& prefix = "") -> std::vector<std::string> {
        std::vector<std::string> paths;
        enumerate_paths_recursive(root, prefix, paths, 0, max_depth);
        return paths;
    }

private:
    /// One segment: object key or array index. nullptr if it does not resolve.
    static auto navigate_single_step(const JsonDocument* current, const std::string& segment)
        -> const JsonDocument* {
        if (current == nullptr) {
            return nullptr;
        }

        try {
            if (current->is_object()) {
                const auto& obj = std::get<std::map<std::string, JsonDocument>>(current->storage_);
                // NOLINTNEXTLINE(readability-identifier-length)
                const auto it = obj.find(segment);
                if (it != obj.end()) {
                    return &it->second;
                }
                return nullptr; // Key not found
            }
            if (current->is_array()) {
                if (!JsonPointer::is_array_index(segment)) {
                    return nullptr; // Not an index
                }
                const size_t index = JsonPointer::to_array_index(segment);
                const auto& arr = std::get<std::vector<JsonDocument>>(current->storage_);
                if (index >= arr.size()) {
                    return nullptr; // Out of bounds
                }
                return &arr[index];
            }
            return nullptr; // Cannot navigate into a primitive
        } catch (const std::exception&) {
            return nullptr;
        }
    }

    static void enumerate_paths_recursive(const JsonDocument& node, const std::string& current_path,
                                          std::vector<std::string>& paths, int current_depth,
                                          int max_depth) {
        // Hard safety bound: `max_depth` is a caller filter ("stop descending here"), so
        // without this a document deeper than the nesting limit would recurse unbounded —
        // the same stack overflow the parser refuses.
        if (current_depth > limits::MAX_NESTING_DEPTH) {
            throw std::runtime_error(std::string(limits::MAX_NESTING_DEPTH_MESSAGE) + " (limit "
                                     + std::to_string(limits::MAX_NESTING_DEPTH) + ")");
        }

        paths.push_back(current_path);

        if (max_depth >= 0 && current_depth >= max_depth) {
            return;
        }

        if (node.is_object()) {
            const auto& obj = std::get<std::map<std::string, JsonDocument>>(node.storage_);
            for (const auto& [key, value] : obj) {
                const std::string child_path
                    = current_path + "/" + JsonPointer::escape_segment(key);
                enumerate_paths_recursive(value, child_path, paths, current_depth + 1, max_depth);
            }
        } else if (node.is_array()) {
            const auto& arr = std::get<std::vector<JsonDocument>>(node.storage_);
            for (size_t i = 0; i < arr.size(); ++i) {
                const std::string child_path = current_path + "/" + std::to_string(i);
                enumerate_paths_recursive(arr[i], child_path, paths, current_depth + 1, max_depth);
            }
        }
    }
};

} // namespace jsom
