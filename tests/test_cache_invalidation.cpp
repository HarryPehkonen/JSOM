#include "jsom/fast_parser.hpp"
#include "jsom/json_document.hpp"
#include "jsom/json_pointer.hpp"
#include <gtest/gtest.h>

using namespace jsom;

TEST(CacheInvalidationTest, ArraySetWithResizeInvalidatesCache) {
    // Parse a document with an array
    auto doc = FastParser().parse(R"({"items": [1, 2, 3]})");

    // Navigate to populate the cache
    EXPECT_EQ(doc.at("/items/0").as<int>(), 1);
    EXPECT_EQ(doc.at("/items/2").as<int>(), 3);

    // Mutate via set_at which correctly invalidates the root cache
    constexpr int FAR_INDEX_VAL = 42;
    doc.set_at("/items/100", JsonDocument(FAR_INDEX_VAL));

    // Navigate again - cache was invalidated by set_at, must not crash
    EXPECT_EQ(doc.at("/items/0").as<int>(), 1);
    EXPECT_EQ(doc.at("/items/100").as<int>(), FAR_INDEX_VAL);
}

TEST(CacheInvalidationTest, ObjectSetInvalidatesCache) {
    auto doc = FastParser().parse(R"({"a": 1, "b": 2})");

    // Populate cache
    EXPECT_EQ(doc.at("/a").as<int>(), 1);

    // set() on the root document invalidates its own cache
    doc.set("c", JsonDocument(3));

    // Must see the new value through navigation
    EXPECT_EQ(doc.at("/c").as<int>(), 3);
    // Old values still accessible
    EXPECT_EQ(doc.at("/a").as<int>(), 1);
}

TEST(CacheInvalidationTest, PushBackInvalidatesCache) {
    auto doc = FastParser().parse(R"({"items": [10, 20]})");

    // Populate cache
    EXPECT_EQ(doc.at("/items/1").as<int>(), 20);

    // push_back via set_at with "-" appends to array
    doc.set_at("/items/2", JsonDocument(30));

    // Navigate to the new element
    EXPECT_EQ(doc.at("/items/2").as<int>(), 30);
    // Old elements still accessible
    EXPECT_EQ(doc.at("/items/0").as<int>(), 10);
}

TEST(CacheInvalidationTest, DirectSetOnRootInvalidatesOwnCache) {
    // Direct set() on the root document invalidates its own cache
    auto doc = FastParser().parse(R"({"x": 1, "y": 2})");

    // Populate cache
    EXPECT_EQ(doc.at("/x").as<int>(), 1);

    // Overwrite existing key
    doc.set("x", JsonDocument(99));

    // Cache was invalidated, must see new value
    EXPECT_EQ(doc.at("/x").as<int>(), 99);
}

TEST(CacheInvalidationTest, DirectPushBackOnRootArrayInvalidatesCache) {
    auto doc = FastParser().parse(R"([10, 20])");

    // Populate cache
    EXPECT_EQ(doc.at("/0").as<int>(), 10);
    EXPECT_EQ(doc.at("/1").as<int>(), 20);

    // push_back on root array
    doc.push_back(JsonDocument(30));

    // Cache was invalidated, must see all elements correctly
    EXPECT_EQ(doc.at("/0").as<int>(), 10);
    EXPECT_EQ(doc.at("/1").as<int>(), 20);
    EXPECT_EQ(doc.at("/2").as<int>(), 30);
}

TEST(CacheInvalidationTest, NestedMutationViaSetAt) {
    auto doc = FastParser().parse(R"({
        "users": [
            {"name": "Alice", "score": 100},
            {"name": "Bob", "score": 200}
        ]
    })");

    // Warm cache with deep paths
    doc.precompute_paths();
    EXPECT_EQ(doc.at("/users/0/name").as<std::string>(), "Alice");
    EXPECT_EQ(doc.at("/users/1/score").as<int>(), 200);

    // Mutate a nested value via set_at (correctly invalidates root cache)
    doc.set_at("/users/0/score", JsonDocument(150));

    // Re-navigate - cache was invalidated, values correct
    EXPECT_EQ(doc.at("/users/0/score").as<int>(), 150);
    EXPECT_EQ(doc.at("/users/1/score").as<int>(), 200);
}

TEST(CacheInvalidationTest, ChildPushBackInvalidatesRootCache) {
    // This is the critical scenario: root cache holds pointers into a child
    // array's vector. push_back on the child can reallocate that vector,
    // making the root's cached pointers dangling. The global mutation epoch
    // causes the root's cache to discard stale entries.
    auto doc = FastParser().parse(R"({"items": [1, 2, 3]})");

    // Populate root cache with pointers into the items vector
    EXPECT_EQ(doc.at("/items/0").as<int>(), 1);
    EXPECT_EQ(doc.at("/items/2").as<int>(), 3);

    // Mutate child array directly — may reallocate the vector
    doc["items"].push_back(JsonDocument(4));

    // Root cache must detect the epoch change and re-navigate safely
    EXPECT_EQ(doc.at("/items/0").as<int>(), 1);
    EXPECT_EQ(doc.at("/items/2").as<int>(), 3);
    EXPECT_EQ(doc.at("/items/3").as<int>(), 4);
}

TEST(CacheInvalidationTest, ChildSetInvalidatesRootCache) {
    auto doc = FastParser().parse(R"({"data": {"x": 1}})");

    // Populate root cache
    EXPECT_EQ(doc.at("/data/x").as<int>(), 1);

    // Mutate child object directly
    doc["data"].set("x", JsonDocument(99));

    // Root cache must detect the epoch change
    EXPECT_EQ(doc.at("/data/x").as<int>(), 99);
}
