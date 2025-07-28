#pragma once

#include <string>
#include <string_view>

namespace jsom {

/**
 * Represents a single component in a JSON Pointer path with bidirectional navigation.
 * Forms a tree structure for efficient path reconstruction and JSON generation.
 */
struct PathNode {
    enum ComponentType { ObjectKey, ArrayIndex };

    ComponentType type;
    std::string_view component; // Points to key name or array index as string

    // Bidirectional navigation for JSON reconstruction
    PathNode* parent = nullptr;       // For upward traversal (JSON Pointer generation)
    PathNode* first_child = nullptr;  // For downward traversal (JSON reconstruction)
    PathNode* next_sibling = nullptr; // For iterating children

    // Constructors
    PathNode() = default;
    PathNode(ComponentType node_type, std::string_view comp, PathNode* parent_node = nullptr);

    // Navigation helpers
    auto add_child(ComponentType child_type, std::string_view child_component) -> PathNode*;
    auto find_child(std::string_view child_component) const -> PathNode*;

    // JSON Pointer generation
    auto generate_json_pointer() const -> std::string;

    // Utility methods
    auto is_root() const -> bool { return parent == nullptr; }
    auto depth() const -> size_t;
};

} // namespace jsom