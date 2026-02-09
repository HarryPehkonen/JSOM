#pragma once

#include "constants.hpp"
#include "json_pointer.hpp"
#include <algorithm>
#include <chrono>
#include <deque>
#include <memory>
#include <unordered_map>
#include <vector>

namespace jsom {

// Forward declaration
class JsonDocument;

// Cache entry for path operations
struct PathCacheEntry {
    JsonDocument* document;
    std::chrono::steady_clock::time_point last_access;
    size_t access_count;

    PathCacheEntry()
        : document(nullptr), last_access(std::chrono::steady_clock::now()), access_count(0) {}

    PathCacheEntry(JsonDocument* doc)
        : document(doc), last_access(std::chrono::steady_clock::now()), access_count(1) {}

    void update_access() {
        last_access = std::chrono::steady_clock::now();
        ++access_count;
    }
};

// Multi-level path cache with prefix optimization.
//
// SAFETY: The cache stores raw JsonDocument* pointers obtained during navigation.
// These pointers become dangling if the underlying storage is reallocated (e.g. by
// vector::push_back or vector::resize on an array). A global mutation epoch detects
// this: any JsonDocument mutation increments the epoch, and cache lookups that find
// an epoch mismatch discard stale entries and re-navigate. This may cause false-positive
// cache misses when unrelated documents are mutated, but prevents use-after-free.
class PathCache {
private:
    // Level 1: Exact path cache (LRU)
    mutable std::unordered_map<std::string, PathCacheEntry> exact_cache_;
    mutable std::deque<std::string> exact_lru_order_;

    // Level 2: Prefix cache (no size limit, but pruned by age)
    mutable std::unordered_map<std::string, PathCacheEntry> prefix_cache_;

    // Level 3: Recent prefixes for locality optimization
    mutable std::vector<std::string> recent_prefixes_;

    // Epoch at which this cache's entries were last validated
    mutable uint64_t cached_epoch_ = 0;

    // Global mutation epoch — incremented by any JsonDocument mutation.
    // Not thread-safe; JSOM documents are not intended for concurrent access.
    static inline uint64_t s_mutation_epoch_ = 0;

    // Configuration
    static constexpr size_t MAX_EXACT_CACHE_SIZE = cache_constants::MAX_EXACT_CACHE_SIZE;
    static constexpr size_t MAX_PREFIX_CACHE_SIZE = cache_constants::MAX_PREFIX_CACHE_SIZE;
    static constexpr size_t MAX_RECENT_PREFIXES = cache_constants::MAX_RECENT_PREFIXES;
    static constexpr auto MAX_PREFIX_AGE
        = std::chrono::minutes(cache_constants::MAX_PREFIX_AGE_MINUTES);

    // If the global epoch has advanced past our cached epoch, all entries are
    // potentially stale. Clear everything and resync.
    void check_epoch() const {
        if (cached_epoch_ != s_mutation_epoch_) {
            clear();
            cached_epoch_ = s_mutation_epoch_;
        }
    }

public:
    PathCache() : cached_epoch_(s_mutation_epoch_) {
        exact_cache_.reserve(MAX_EXACT_CACHE_SIZE);
        prefix_cache_.reserve(MAX_PREFIX_CACHE_SIZE);
        recent_prefixes_.reserve(MAX_RECENT_PREFIXES);
    }

    // Called by JsonDocument::invalidate_cache() on every structural mutation.
    static void notify_mutation() { ++s_mutation_epoch_; }

    // Get exact path from cache
    auto get_exact(const std::string& path) const -> JsonDocument* {
        check_epoch();
        // NOLINTNEXTLINE(readability-identifier-length)
        auto it = exact_cache_.find(path);
        if (it != exact_cache_.end()) {
            // Update LRU order
            auto lru_it = std::find(exact_lru_order_.begin(), exact_lru_order_.end(), path);
            if (lru_it != exact_lru_order_.end()) {
                exact_lru_order_.erase(lru_it);
            }
            exact_lru_order_.push_back(path);

            it->second.update_access();
            return it->second.document;
        }
        return nullptr;
    }

    // Cache exact path
    void put_exact(const std::string& path, JsonDocument* doc) const {
        check_epoch();
        if (exact_cache_.size() >= MAX_EXACT_CACHE_SIZE) {
            evict_exact_lru();
        }

        exact_cache_[path] = PathCacheEntry(doc);
        exact_lru_order_.push_back(path);
    }

    // Find best cached prefix
    // NOLINTBEGIN(readability-function-size)
    auto find_best_prefix(const std::string& path) const -> std::pair<JsonDocument*, std::string> {
        check_epoch();
        // First try recent prefixes for locality optimization
        for (const auto& prefix : recent_prefixes_) {
            if (JsonPointer::is_prefix(prefix, path)) {
                // NOLINTNEXTLINE(readability-identifier-length)
                auto it = prefix_cache_.find(prefix);
                if (it != prefix_cache_.end()) {
                    it->second.update_access();
                    return {it->second.document, JsonPointer::make_relative(prefix, path)};
                }
            }
        }

        // Try all cached prefixes to find the longest match
        std::string best_prefix;
        JsonDocument* best_node = nullptr;

        for (const auto& [cached_prefix, entry] : prefix_cache_) {
            if (JsonPointer::is_prefix(cached_prefix, path)
                && cached_prefix.length() > best_prefix.length()) {
                best_prefix = cached_prefix;
                best_node = entry.document;
            }
        }

        if (best_node != nullptr) {
            // Update recent prefixes for locality
            update_recent_prefixes(best_prefix);
            // Update access time
            prefix_cache_[best_prefix].update_access();
            return {best_node, JsonPointer::make_relative(best_prefix, path)};
        }

        return {nullptr, path}; // No prefix found, start from root
    }
    // NOLINTEND(readability-function-size)

    // Cache prefix
    void put_prefix(const std::string& prefix, JsonDocument* doc) const {
        if (prefix_cache_.size() >= MAX_PREFIX_CACHE_SIZE) {
            prune_old_prefixes();
        }

        prefix_cache_[prefix] = PathCacheEntry(doc);
        update_recent_prefixes(prefix);
    }

    // Cache intermediate steps during navigation
    void cache_intermediate_steps(
        const std::vector<std::pair<std::string, JsonDocument*>>& steps) const {
        for (const auto& [path, doc] : steps) {
            put_prefix(path, doc);
        }
    }

    // Clear all caches
    void clear() const {
        exact_cache_.clear();
        exact_lru_order_.clear();
        prefix_cache_.clear();
        recent_prefixes_.clear();
    }

    // Get cache statistics
    struct CacheStats {
        size_t exact_cache_size;
        size_t prefix_cache_size;
        size_t total_entries;
        size_t memory_usage_estimate;
        double avg_prefix_length;
    };

    auto get_stats() const -> CacheStats {
        CacheStats stats;
        stats.exact_cache_size = exact_cache_.size();
        stats.prefix_cache_size = prefix_cache_.size();
        stats.total_entries = exact_cache_.size() + prefix_cache_.size();

        // Rough memory usage estimate
        stats.memory_usage_estimate = 0;
        for (const auto& [path, _] : exact_cache_) {
            stats.memory_usage_estimate += path.length() + sizeof(PathCacheEntry);
        }
        for (const auto& [path, _] : prefix_cache_) {
            stats.memory_usage_estimate += path.length() + sizeof(PathCacheEntry);
        }

        // Average prefix length
        if (!prefix_cache_.empty()) {
            size_t total_length = 0;
            for (const auto& [path, _] : prefix_cache_) {
                total_length += path.length();
            }
            stats.avg_prefix_length
                = static_cast<double>(total_length) / static_cast<double>(prefix_cache_.size());
        } else {
            stats.avg_prefix_length = 0.0;
        }

        return stats;
    }

private:
    // Evict least recently used exact cache entry
    void evict_exact_lru() const {
        if (!exact_lru_order_.empty()) {
            const std::string& oldest = exact_lru_order_.front();
            exact_cache_.erase(oldest);
            exact_lru_order_.pop_front();
        }
    }

    // Prune old prefix cache entries
    // NOLINTBEGIN(readability-function-size)
    void prune_old_prefixes() const {
        auto now = std::chrono::steady_clock::now();
        auto cutoff = now - MAX_PREFIX_AGE;

        for (auto it = prefix_cache_.begin(); it != prefix_cache_.end();) {
            if (it->second.last_access < cutoff) {
                it = prefix_cache_.erase(it);
            } else {
                ++it;
            }
        }

        // If still too large, remove least accessed entries
        if (prefix_cache_.size() >= MAX_PREFIX_CACHE_SIZE) {
            std::vector<std::pair<std::string, size_t>> access_counts;
            access_counts.reserve(prefix_cache_.size());
            for (const auto& [path, entry] : prefix_cache_) {
                access_counts.emplace_back(path, entry.access_count);
            }

            // Sort by access count (ascending)
            std::sort(access_counts.begin(), access_counts.end(),
                      // NOLINTNEXTLINE(readability-identifier-length)
                      // NOLINTNEXTLINE(readability-identifier-length)
                      [](const auto& a, const auto& b) { return a.second < b.second; });

            // Remove least accessed half
            size_t to_remove = prefix_cache_.size() / cache_constants::CACHE_EVICTION_HALF_DIVISOR;
            for (size_t i = 0; i < to_remove && i < access_counts.size(); ++i) {
                prefix_cache_.erase(access_counts[i].first);
            }
        }
    }
    // NOLINTEND(readability-function-size)

    // Update recent prefixes for locality optimization
    void update_recent_prefixes(const std::string& prefix) const {
        // Remove if already present
        // NOLINTNEXTLINE(readability-identifier-length)
        auto it = std::find(recent_prefixes_.begin(), recent_prefixes_.end(), prefix);
        if (it != recent_prefixes_.end()) {
            recent_prefixes_.erase(it);
        }

        // Add to end
        recent_prefixes_.push_back(prefix);

        // Maintain size limit
        if (recent_prefixes_.size() > MAX_RECENT_PREFIXES) {
            recent_prefixes_.erase(recent_prefixes_.begin());
        }
    }
};

} // namespace jsom
