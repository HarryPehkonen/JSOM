// Navigation always observes the CURRENT document: every lookup re-reads the model, so a
// mutation is visible immediately and nothing can go stale.
//
// These started as path-cache-invalidation tests. The cache is gone (measured a net loss —
// see navigation_engine.hpp and OPTIMIZATIONS.md), so what is left is the property that
// actually matters to a caller, and it is the reason a future cache must prove itself
// against these tests before it earns a place again.
#include "jsom/fast_parser.hpp"
#include "jsom/json_document.hpp"
#include "jsom/json_pointer.hpp"
#include <gtest/gtest.h>

using namespace jsom;

TEST(NavigationFreshnessTest, ArrayGrowthIsVisibleThroughTheRoot) {
    auto doc = FastParser().parse(R"({"items": [1, 2, 3]})");

    EXPECT_EQ(doc.at("/items/0").as<int>(), 1);
    EXPECT_EQ(doc.at("/items/2").as<int>(), 3);

    constexpr int FAR_INDEX_VAL = 42;
    doc.set_at("/items/100", JsonDocument(FAR_INDEX_VAL));

    EXPECT_EQ(doc.at("/items/0").as<int>(), 1);
    EXPECT_EQ(doc.at("/items/100").as<int>(), FAR_INDEX_VAL);
}

TEST(NavigationFreshnessTest, ObjectInsertionIsVisible) {
    auto doc = FastParser().parse(R"({"a": 1, "b": 2})");

    EXPECT_EQ(doc.at("/a").as<int>(), 1);

    doc.set("c", JsonDocument(3));

    EXPECT_EQ(doc.at("/c").as<int>(), 3);
    EXPECT_EQ(doc.at("/a").as<int>(), 1);
}

TEST(NavigationFreshnessTest, PushBackIsVisible) {
    auto doc = FastParser().parse(R"({"items": [10, 20]})");

    EXPECT_EQ(doc.at("/items/1").as<int>(), 20);

    doc.set_at("/items/2", JsonDocument(30));

    EXPECT_EQ(doc.at("/items/2").as<int>(), 30);
    EXPECT_EQ(doc.at("/items/0").as<int>(), 10);
}

TEST(NavigationFreshnessTest, OverwritingAKeyIsVisible) {
    auto doc = FastParser().parse(R"({"x": 1, "y": 2})");

    EXPECT_EQ(doc.at("/x").as<int>(), 1);

    doc.set("x", JsonDocument(99));

    EXPECT_EQ(doc.at("/x").as<int>(), 99);
}

TEST(NavigationFreshnessTest, PushBackOnTheRootArrayIsVisible) {
    auto doc = FastParser().parse(R"([10, 20])");

    EXPECT_EQ(doc.at("/0").as<int>(), 10);
    EXPECT_EQ(doc.at("/1").as<int>(), 20);

    doc.push_back(JsonDocument(30));

    EXPECT_EQ(doc.at("/0").as<int>(), 10);
    EXPECT_EQ(doc.at("/1").as<int>(), 20);
    EXPECT_EQ(doc.at("/2").as<int>(), 30);
}

TEST(NavigationFreshnessTest, NestedMutationIsVisibleThroughDeepPaths) {
    auto doc = FastParser().parse(R"({
        "users": [
            {"name": "Alice", "score": 100},
            {"name": "Bob", "score": 200}
        ]
    })");

    EXPECT_EQ(doc.at("/users/0/name").as<std::string>(), "Alice");
    EXPECT_EQ(doc.at("/users/1/score").as<int>(), 200);

    doc.set_at("/users/0/score", JsonDocument(150));

    EXPECT_EQ(doc.at("/users/0/score").as<int>(), 150);
    EXPECT_EQ(doc.at("/users/1/score").as<int>(), 200);
}

TEST(NavigationFreshnessTest, GrowthOfAChildVectorIsVisibleThroughTheRoot) {
    // The case a caching layer gets wrong: the root's view of a child array whose
    // std::vector reallocates on growth.
    auto doc = FastParser().parse(R"({"items": [1, 2, 3]})");

    EXPECT_EQ(doc.at("/items/0").as<int>(), 1);
    EXPECT_EQ(doc.at("/items/2").as<int>(), 3);

    doc["items"].push_back(JsonDocument(4));

    EXPECT_EQ(doc.at("/items/0").as<int>(), 1);
    EXPECT_EQ(doc.at("/items/2").as<int>(), 3);
    EXPECT_EQ(doc.at("/items/3").as<int>(), 4);
}

TEST(NavigationFreshnessTest, MutationOfAChildObjectIsVisibleThroughTheRoot) {
    auto doc = FastParser().parse(R"({"data": {"x": 1}})");

    EXPECT_EQ(doc.at("/data/x").as<int>(), 1);

    doc["data"].set("x", JsonDocument(99));

    EXPECT_EQ(doc.at("/data/x").as<int>(), 99);
}

TEST(NavigationFreshnessTest, PathsAreEnumeratedFromTheCurrentState) {
    auto doc = FastParser().parse(R"({"items": [1, 2]})");

    EXPECT_EQ(doc.list_paths().size(), 4); // "", "/items", "/items/0", "/items/1"

    doc.set_at("/items/2", JsonDocument(3));

    EXPECT_EQ(doc.list_paths().size(), 5);
    EXPECT_EQ(doc.count_paths(), 5);
}
