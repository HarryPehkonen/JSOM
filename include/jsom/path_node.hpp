#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace jsom {

enum class ContainerType : std::uint8_t { Object, Array };

class PathNode {
private:
    PathNode* parent_;
    std::string key_;
    std::size_t array_index_;
    ContainerType container_type_;

    mutable std::string cached_pointer_;
    mutable bool pointer_cached_;

    std::unordered_map<std::string, std::unique_ptr<PathNode>> object_children_;
    std::vector<std::unique_ptr<PathNode>> array_children_;

public:
    PathNode()
        : parent_(nullptr), array_index_(0), container_type_(ContainerType::Object),
          pointer_cached_(false) {}

    PathNode(PathNode* parent, std::string key)
        : parent_(parent), key_(std::move(key)), array_index_(0),
          container_type_(ContainerType::Object), pointer_cached_(false) {}

    PathNode(PathNode* parent, std::size_t index)
        : parent_(parent), array_index_(index), container_type_(ContainerType::Array),
          pointer_cached_(false) {}

    auto get_json_pointer() const -> std::string {
        if (pointer_cached_) {
            return cached_pointer_;
        }

        std::vector<std::string> segments;
        const PathNode* current = this;

        while ((current != nullptr) && (current->parent_ != nullptr)) {
            if (current->container_type_ == ContainerType::Object) {
                segments.push_back(escape_json_pointer(current->key_));
            } else {
                segments.push_back(std::to_string(current->array_index_));
            }
            current = current->parent_;
        }

        if (segments.empty()) {
            cached_pointer_ = "";
        } else {
            std::string result;
            for (auto it = segments.rbegin(); it != segments.rend(); ++it) {
                result += "/" + *it;
            }
            cached_pointer_ = result;
        }

        pointer_cached_ = true;
        return cached_pointer_;
    }

    auto get_object_child(const std::string& key) -> PathNode* {
        // NOLINTNEXTLINE(readability-identifier-length)
        auto it = object_children_.find(key);
        if (it != object_children_.end()) {
            return it->second.get();
        }

        auto new_child = std::make_unique<PathNode>(this, key);
        PathNode* child_ptr = new_child.get();
        object_children_[key] = std::move(new_child);
        return child_ptr;
    }

    auto get_array_child(std::size_t index) -> PathNode* {
        if (index >= array_children_.size()) {
            array_children_.resize(index + 1);
        }

        if (!array_children_[index]) {
            array_children_[index] = std::make_unique<PathNode>(this, index);
        }

        return array_children_[index].get();
    }

    void invalidate_pointer_cache() {
        pointer_cached_ = false;
        cached_pointer_.clear();

        for (auto& [key, child] : object_children_) {
            child->invalidate_pointer_cache();
        }

        for (auto& child : array_children_) {
            if (child) {
                child->invalidate_pointer_cache();
            }
        }
    }

private:
    static auto escape_json_pointer(const std::string& str) -> std::string {
        std::string result;
        result.reserve(str.length() + (str.length() / 4));

        // NOLINTNEXTLINE(readability-identifier-length)
        for (char c : str) {
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
};

class PathManager {
private:
    std::unique_ptr<PathNode> root_;

public:
    PathManager() : root_(std::make_unique<PathNode>()) {}

    auto get_root() -> PathNode* { return root_.get(); }

    void reset() { root_ = std::make_unique<PathNode>(); }
};

} // namespace jsom
