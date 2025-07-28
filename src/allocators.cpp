#include "jsom/allocators.hpp"
#include <algorithm>
#include <cstdlib>

namespace jsom {

// StandardAllocator implementation
auto StandardAllocator::allocate(size_t size, size_t /*alignment*/) -> void* {
    void* ptr = std::malloc(size);
    if (!ptr) {
        throw std::bad_alloc();
    }
    return ptr;
}

void StandardAllocator::deallocate(void* ptr, size_t /*size*/) { std::free(ptr); }

auto StandardAllocator::clone() const -> std::unique_ptr<IAllocator> {
    return std::make_unique<StandardAllocator>();
}

// ArenaAllocator::Chunk implementation
ArenaAllocator::Chunk::Chunk(size_t chunk_size)
    : data(std::make_unique<char[]>(chunk_size)), size(chunk_size) {}

auto ArenaAllocator::Chunk::has_space(size_t requested_size, size_t alignment) const -> bool {
    size_t aligned_used = ArenaAllocator::align_size(used, alignment);
    return aligned_used + requested_size <= size;
}

auto ArenaAllocator::Chunk::allocate_from_chunk(size_t requested_size, size_t alignment) -> void* {
    if (!has_space(requested_size, alignment)) {
        return nullptr;
    }

    used = align_size(used, alignment);
    void* ptr = data.get() + used;
    used += requested_size;

    return ptr;
}

// ArenaAllocator implementation
ArenaAllocator::ArenaAllocator(size_t chunk_size) : default_chunk_size_(chunk_size) {
    chunks_.reserve(8); // Reserve space for typical usage
}

auto ArenaAllocator::allocate(size_t size, size_t alignment) -> void* {
    // Try to allocate from existing chunks
    for (auto& chunk : chunks_) {
        if (void* ptr = chunk.allocate_from_chunk(size, alignment)) {
            total_allocated_ += size;
            return ptr;
        }
    }

    // Need a new chunk
    size_t chunk_size = std::max(default_chunk_size_, size + alignment);
    Chunk& new_chunk = add_chunk(chunk_size);

    void* ptr = new_chunk.allocate_from_chunk(size, alignment);
    if (!ptr) {
        throw std::bad_alloc();
    }

    total_allocated_ += size;
    return ptr;
}

auto ArenaAllocator::clone() const -> std::unique_ptr<IAllocator> {
    return std::make_unique<ArenaAllocator>(default_chunk_size_);
}

void ArenaAllocator::reset() {
    chunks_.clear();
    total_allocated_ = 0;
}

auto ArenaAllocator::add_chunk(size_t min_size) -> ArenaAllocator::Chunk& {
    size_t chunk_size = std::max(default_chunk_size_, min_size);
    chunks_.emplace_back(chunk_size);
    return chunks_.back();
}

auto ArenaAllocator::align_size(size_t size, size_t alignment) -> size_t {
    return (size + alignment - 1) & ~(alignment - 1);
}

} // namespace jsom