#pragma once

#include <cstddef>
#include <memory>
#include <vector>

namespace jsom {

/**
 * Abstract allocator interface for flexible allocation strategies
 */
class IAllocator {
public:
    virtual ~IAllocator() = default;
    virtual auto allocate(size_t size, size_t alignment = alignof(std::max_align_t)) -> void* = 0;
    virtual void deallocate(void* ptr, size_t size) = 0;
    virtual auto clone() const -> std::unique_ptr<IAllocator> = 0;
};

/**
 * Standard allocator wrapper - baseline implementation
 */
class StandardAllocator : public IAllocator {
public:
    auto allocate(size_t size, size_t /*alignment*/) -> void* override;
    void deallocate(void* ptr, size_t /*size*/) override;
    auto clone() const -> std::unique_ptr<IAllocator> override;
};

/**
 * Arena allocator for high-performance batch allocation
 * Ideal for PathNodes during parsing - allocate fast, deallocate all at once
 */
class ArenaAllocator : public IAllocator {
private:
    struct Chunk {
        std::unique_ptr<char[]> data;
        size_t size;
        size_t used = 0;

        explicit Chunk(size_t chunk_size);
        auto has_space(size_t requested_size, size_t alignment) const -> bool;
        auto allocate_from_chunk(size_t size, size_t alignment) -> void*;
    };

    std::vector<Chunk> chunks_;
    size_t default_chunk_size_;
    size_t total_allocated_ = 0;

public:
    static constexpr size_t DEFAULT_CHUNK_SIZE = 64 * 1024; // 64KB chunks

    explicit ArenaAllocator(size_t chunk_size = DEFAULT_CHUNK_SIZE);
    ~ArenaAllocator() override = default;

    // Non-copyable but movable
    ArenaAllocator(const ArenaAllocator&) = delete;
    auto operator=(const ArenaAllocator&) -> ArenaAllocator& = delete;
    ArenaAllocator(ArenaAllocator&&) = default;
    auto operator=(ArenaAllocator&&) -> ArenaAllocator& = default;

    auto allocate(size_t size, size_t alignment = alignof(std::max_align_t)) -> void* override;
    void deallocate(void* /*ptr*/, size_t /*size*/) override {} // No-op for arena
    auto clone() const -> std::unique_ptr<IAllocator> override;

    // Arena-specific methods
    void reset(); // Deallocate all memory at once
    auto total_allocated() const -> size_t { return total_allocated_; }
    auto chunk_count() const -> size_t { return chunks_.size(); }

private:
    auto add_chunk(size_t min_size) -> Chunk&;
    static auto align_size(size_t size, size_t alignment) -> size_t;
};

/**
 * Allocator-aware deleter for use with unique_ptr
 */
template <typename T> class AllocatorDeleter {
private:
    IAllocator* allocator_;

public:
    explicit AllocatorDeleter(IAllocator* alloc) : allocator_(alloc) {}

    void operator()(T* ptr) {
        if (ptr && allocator_) {
            ptr->~T();
            allocator_->deallocate(ptr, sizeof(T));
        }
    }
};

/**
 * Convenience alias for allocator-aware unique_ptr
 */
template <typename T> using AllocatorUniquePtr = std::unique_ptr<T, AllocatorDeleter<T>>;

/**
 * Factory function for creating objects with custom allocators
 */
template <typename T, typename... Args>
auto make_allocated(IAllocator& allocator, Args&&... args) -> AllocatorUniquePtr<T> {
    void* ptr = allocator.allocate(sizeof(T), alignof(T));
    T* typed_ptr = new (ptr) T(std::forward<Args>(args)...);
    return AllocatorUniquePtr<T>(typed_ptr, AllocatorDeleter<T>(&allocator));
}

} // namespace jsom