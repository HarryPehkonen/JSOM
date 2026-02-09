#include <gtest/gtest.h>
#include "jsom/json_document.hpp"

using namespace jsom;

// ============================================================
// Equality Tests
// ============================================================

TEST(ComparisonTest, NullEquality) {
    JsonDocument a;
    JsonDocument b;
    EXPECT_EQ(a, b);
    EXPECT_EQ(a, nullptr);
}

TEST(ComparisonTest, BoolEquality) {
    EXPECT_EQ(JsonDocument(true), JsonDocument(true));
    EXPECT_EQ(JsonDocument(false), JsonDocument(false));
    EXPECT_NE(JsonDocument(true), JsonDocument(false));
}

TEST(ComparisonTest, NumberEquality) {
    EXPECT_EQ(JsonDocument(42), JsonDocument(42));
    EXPECT_EQ(JsonDocument(3.14), JsonDocument(3.14));
    EXPECT_NE(JsonDocument(1), JsonDocument(2));
    // int and double with same value
    EXPECT_EQ(JsonDocument(42), JsonDocument(42.0));
}

TEST(ComparisonTest, StringEquality) {
    EXPECT_EQ(JsonDocument("hello"), JsonDocument("hello"));
    EXPECT_NE(JsonDocument("hello"), JsonDocument("world"));
}

TEST(ComparisonTest, ArrayEquality) {
    auto arr1 = JsonDocument(std::vector<JsonDocument>{1, 2, 3});
    auto arr2 = JsonDocument(std::vector<JsonDocument>{1, 2, 3});
    auto arr3 = JsonDocument(std::vector<JsonDocument>{1, 2, 4});
    EXPECT_EQ(arr1, arr2);
    EXPECT_NE(arr1, arr3);
}

TEST(ComparisonTest, ObjectEquality) {
    // NOLINTNEXTLINE(readability-magic-numbers)
    JsonDocument obj1{{"name", "Alice"}, {"age", 30}};
    // NOLINTNEXTLINE(readability-magic-numbers)
    JsonDocument obj2{{"name", "Alice"}, {"age", 30}};
    // NOLINTNEXTLINE(readability-magic-numbers)
    JsonDocument obj3{{"name", "Bob"}, {"age", 30}};
    EXPECT_EQ(obj1, obj2);
    EXPECT_NE(obj1, obj3);
}

TEST(ComparisonTest, DifferentTypesNotEqual) {
    EXPECT_NE(JsonDocument(1), JsonDocument("1"));
    EXPECT_NE(JsonDocument(true), JsonDocument(1));
    EXPECT_NE(JsonDocument(), JsonDocument(false));
    EXPECT_NE(JsonDocument(0), JsonDocument(""));
}

TEST(ComparisonTest, DeepStructuralEquality) {
    // NOLINTNEXTLINE(readability-magic-numbers)
    JsonDocument a{{"data", JsonDocument(std::vector<JsonDocument>{1, 2, 3})},
                   {"nested", JsonDocument{{"x", 10}}}};
    // NOLINTNEXTLINE(readability-magic-numbers)
    JsonDocument b{{"data", JsonDocument(std::vector<JsonDocument>{1, 2, 3})},
                   {"nested", JsonDocument{{"x", 10}}}};
    EXPECT_EQ(a, b);
}

// ============================================================
// Ordering Tests
// ============================================================

TEST(ComparisonTest, CrossTypeOrdering) {
    // Null < Bool < Number < String < Object < Array (matches JsonType enum order)
    JsonDocument null_val;
    JsonDocument bool_val = false;
    JsonDocument num_val = 0;
    JsonDocument str_val = "";
    auto obj_val = JsonDocument::make_object();
    auto arr_val = JsonDocument::make_array();

    EXPECT_LT(null_val, bool_val);
    EXPECT_LT(bool_val, num_val);
    EXPECT_LT(num_val, str_val);
    EXPECT_LT(str_val, obj_val);
    EXPECT_LT(obj_val, arr_val);
}

TEST(ComparisonTest, BoolOrdering) {
    EXPECT_LT(JsonDocument(false), JsonDocument(true));
    EXPECT_FALSE(JsonDocument(true) < JsonDocument(false));
    EXPECT_FALSE(JsonDocument(true) < JsonDocument(true));
}

TEST(ComparisonTest, NumberOrdering) {
    EXPECT_LT(JsonDocument(1), JsonDocument(2));
    EXPECT_LT(JsonDocument(-1), JsonDocument(0));
    EXPECT_LT(JsonDocument(1.5), JsonDocument(2.5));
    EXPECT_GT(JsonDocument(10), JsonDocument(5));
}

TEST(ComparisonTest, StringOrdering) {
    EXPECT_LT(JsonDocument("abc"), JsonDocument("abd"));
    EXPECT_LT(JsonDocument("a"), JsonDocument("b"));
    EXPECT_GT(JsonDocument("z"), JsonDocument("a"));
}

TEST(ComparisonTest, ArrayOrdering) {
    auto arr1 = JsonDocument(std::vector<JsonDocument>{1, 2});
    auto arr2 = JsonDocument(std::vector<JsonDocument>{1, 3});
    auto arr3 = JsonDocument(std::vector<JsonDocument>{1, 2, 3});
    EXPECT_LT(arr1, arr2);
    EXPECT_LT(arr1, arr3); // shorter prefix < longer
}

TEST(ComparisonTest, LessOrEqualGreaterOrEqual) {
    EXPECT_LE(JsonDocument(1), JsonDocument(1));
    EXPECT_LE(JsonDocument(1), JsonDocument(2));
    EXPECT_GE(JsonDocument(2), JsonDocument(2));
    EXPECT_GE(JsonDocument(2), JsonDocument(1));
}

TEST(ComparisonTest, ImplicitConversionInComparison) {
    // Implicit construction allows comparing directly with primitives
    JsonDocument doc = 42;
    EXPECT_EQ(doc, 42);
    EXPECT_NE(doc, 43);
    EXPECT_LT(doc, 100);
    EXPECT_GT(doc, 0);

    JsonDocument str = "hello";
    EXPECT_EQ(str, "hello");
    EXPECT_NE(str, "world");
}
