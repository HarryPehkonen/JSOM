#include <gtest/gtest.h>
#include "jsom.hpp"

namespace jsom {

class AllocatorTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(AllocatorTest, StandardAllocator) {
    StandardAllocator allocator;
    
    void* ptr1 = allocator.allocate(100, alignof(std::max_align_t));
    EXPECT_NE(ptr1, nullptr);
    
    void* ptr2 = allocator.allocate(200, alignof(std::max_align_t));
    EXPECT_NE(ptr2, nullptr);
    EXPECT_NE(ptr1, ptr2);
    
    allocator.deallocate(ptr1, 100);
    allocator.deallocate(ptr2, 200);
}

TEST_F(AllocatorTest, ArenaAllocator) {
    ArenaAllocator arena(1024);  // 1KB chunks
    
    EXPECT_EQ(arena.total_allocated(), 0);
    EXPECT_EQ(arena.chunk_count(), 0);
    
    void* ptr1 = arena.allocate(100);
    EXPECT_NE(ptr1, nullptr);
    EXPECT_EQ(arena.total_allocated(), 100);
    EXPECT_EQ(arena.chunk_count(), 1);
    
    void* ptr2 = arena.allocate(200);
    EXPECT_NE(ptr2, nullptr);
    EXPECT_EQ(arena.total_allocated(), 300);
    EXPECT_EQ(arena.chunk_count(), 1);  // Should fit in same chunk
}

TEST_F(AllocatorTest, ArenaLargeAllocation) {
    ArenaAllocator arena(512);  // Small chunks
    
    // Allocate more than chunk size
    void* large_ptr = arena.allocate(1000);
    EXPECT_NE(large_ptr, nullptr);
    EXPECT_EQ(arena.total_allocated(), 1000);
    EXPECT_EQ(arena.chunk_count(), 1);
}

TEST_F(AllocatorTest, ArenaReset) {
    ArenaAllocator arena;
    
    arena.allocate(100);
    arena.allocate(200);
    EXPECT_GT(arena.total_allocated(), 0);
    
    arena.reset();
    EXPECT_EQ(arena.total_allocated(), 0);
    EXPECT_EQ(arena.chunk_count(), 0);
}

TEST_F(AllocatorTest, ArenaAlignment) {
    ArenaAllocator arena;
    
    // Test various alignments
    void* ptr1 = arena.allocate(1, 1);
    void* ptr2 = arena.allocate(1, 4);
    void* ptr3 = arena.allocate(1, 8);
    
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr2) % 4, 0);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr3) % 8, 0);
}

TEST_F(AllocatorTest, AllocatorCloning) {
    StandardAllocator standard;
    ArenaAllocator arena(2048);
    
    auto standard_clone = standard.clone();
    auto arena_clone = arena.clone();
    
    EXPECT_NE(standard_clone.get(), nullptr);
    EXPECT_NE(arena_clone.get(), nullptr);
    
    // Test that clones work
    void* ptr1 = standard_clone->allocate(100);
    void* ptr2 = arena_clone->allocate(100);
    
    EXPECT_NE(ptr1, nullptr);
    EXPECT_NE(ptr2, nullptr);
    
    standard_clone->deallocate(ptr1, 100);
    // Arena doesn't need explicit deallocation
}

struct TestStruct {
    int value;
    explicit TestStruct(int v) : value(v) {}
};

TEST_F(AllocatorTest, MakeAllocated) {
    ArenaAllocator arena;
    
    auto obj = make_allocated<TestStruct>(arena, 42);
    EXPECT_NE(obj.get(), nullptr);
    EXPECT_EQ(obj->value, 42);
    
    // Object should be automatically destroyed when unique_ptr goes out of scope
}

TEST_F(AllocatorTest, AllocatorDeleter) {
    StandardAllocator allocator;
    
    {
        auto obj = make_allocated<TestStruct>(allocator, 123);
        EXPECT_EQ(obj->value, 123);
    }
    // TestStruct destructor and deallocator should be called here
}

} // namespace jsom