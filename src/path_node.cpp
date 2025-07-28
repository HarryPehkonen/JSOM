#include "jsom/path_node.hpp"
#include <algorithm>
#include <sstream>
#include <vector>

namespace jsom {

PathNode::PathNode(ComponentType node_type, std::string_view comp, PathNode* parent_node)
    : type(node_type), component(comp), parent(parent_node) {}

auto PathNode::add_child(ComponentType child_type, std::string_view child_component) -> PathNode* {
    // Create new child node
    auto* child = new PathNode(child_type, child_component, this);

    // Link into sibling chain
    if (first_child == nullptr) {
        first_child = child;
    } else {
        // Find last sibling and append
        PathNode* last_sibling = first_child;
        while (last_sibling->next_sibling != nullptr) {
            last_sibling = last_sibling->next_sibling;
        }
        last_sibling->next_sibling = child;
    }

    return child;
}

auto PathNode::find_child(std::string_view child_component) const -> PathNode* {
    for (PathNode* child = first_child; child != nullptr; child = child->next_sibling) {
        if (child->component == child_component) {
            return child;
        }
    }
    return nullptr;
}

auto PathNode::generate_json_pointer() const -> std::string {
    if (is_root()) {
        return ""; // Root has empty pointer
    }

    // Build path components by walking up the tree
    std::vector<std::string_view> components;
    for (const PathNode* node = this; node != nullptr && !node->is_root(); node = node->parent) {
        components.push_back(node->component);
    }

    // Reverse to get correct order (root to leaf)
    std::reverse(components.begin(), components.end());

    // Build JSON Pointer string with proper escaping
    std::ostringstream pointer;
    for (const auto& component : components) {
        pointer << '/';

        // RFC 6901 escaping: ~ becomes ~0, / becomes ~1
        for (char ch : component) {
            if (ch == '~') {
                pointer << "~0";
            } else if (ch == '/') {
                pointer << "~1";
            } else {
                pointer << ch;
            }
        }
    }

    return pointer.str();
}

auto PathNode::depth() const -> size_t {
    size_t level = 0;
    for (const PathNode* node = parent; node != nullptr; node = node->parent) {
        ++level;
    }
    return level;
}

} // namespace jsom