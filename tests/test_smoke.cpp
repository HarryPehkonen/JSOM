#include "jsom.hpp"
#include <gtest/gtest.h>

// Test constants
constexpr std::size_t TEST_ALLOCATION_SIZE = 64;

class JSOMSmokeTest : public ::testing::Test {
protected:
    void SetUp() override { parser = std::make_unique<jsom::StreamingParser>(); }

    std::unique_ptr<jsom::StreamingParser> parser;
};

TEST_F(JSOMSmokeTest, ParserCanBeCreatedAndDestroyed) {
    EXPECT_NE(parser, nullptr);

    jsom::ParseEvents events;
    parser->set_events(events);

    parser->reset();
}

TEST_F(JSOMSmokeTest, AllocatorsWork) {
    auto standard_alloc = std::make_unique<jsom::StandardAllocator>();
    auto arena_alloc = std::make_unique<jsom::ArenaAllocator>();

    void* ptr1 = standard_alloc->allocate(TEST_ALLOCATION_SIZE);
    EXPECT_NE(ptr1, nullptr);
    standard_alloc->deallocate(ptr1, TEST_ALLOCATION_SIZE);

    void* ptr2 = arena_alloc->allocate(TEST_ALLOCATION_SIZE);
    EXPECT_NE(ptr2, nullptr);
    arena_alloc->reset();
}

TEST_F(JSOMSmokeTest, JsonValueCreation) {
    auto alloc = std::make_unique<jsom::ArenaAllocator>();

    auto* root = static_cast<jsom::PathNode*>(alloc->allocate(sizeof(jsom::PathNode)));
    new (root) jsom::PathNode(nullptr, "");

    jsom::JsonValue value(jsom::JsonType::String, "test", root);

    EXPECT_EQ(value.type(), jsom::JsonType::String);
    EXPECT_EQ(value.raw_value(), "test");
    EXPECT_EQ(value.path(), "");

    root->~PathNode();
    alloc->deallocate(root, sizeof(jsom::PathNode));
}

TEST_F(JSOMSmokeTest, PathNodeJsonPointerGeneration) {
    auto alloc = std::make_unique<jsom::ArenaAllocator>();

    auto* root = static_cast<jsom::PathNode*>(alloc->allocate(sizeof(jsom::PathNode)));
    new (root) jsom::PathNode(nullptr, "");

    auto* child = static_cast<jsom::PathNode*>(alloc->allocate(sizeof(jsom::PathNode)));
    new (child) jsom::PathNode(root, "key");

    EXPECT_EQ(root->generate_json_pointer(), "");
    EXPECT_EQ(child->generate_json_pointer(), "/key");

    child->~PathNode();
    alloc->deallocate(child, sizeof(jsom::PathNode));
    root->~PathNode();
    alloc->deallocate(root, sizeof(jsom::PathNode));
}

TEST_F(JSOMSmokeTest, PathNodeEscaping) {
    auto alloc = std::make_unique<jsom::ArenaAllocator>();

    auto* root = static_cast<jsom::PathNode*>(alloc->allocate(sizeof(jsom::PathNode)));
    new (root) jsom::PathNode(nullptr, "");

    auto* child = static_cast<jsom::PathNode*>(alloc->allocate(sizeof(jsom::PathNode)));
    new (child) jsom::PathNode(root, "key~with/special");

    EXPECT_EQ(child->generate_json_pointer(), "/key~0with~1special");

    child->~PathNode();
    alloc->deallocate(child, sizeof(jsom::PathNode));
    root->~PathNode();
    alloc->deallocate(root, sizeof(jsom::PathNode));
}