#include <gtest/gtest.h>
#include "jsom/json_document.hpp"

using namespace jsom;

TEST(IterationTest, RangeForArray) {
    auto arr = JsonDocument(std::vector<JsonDocument>{1, 2, 3});
    int sum = 0;
    for (const auto& elem : arr) {
        sum += elem.as<int>();
    }
    EXPECT_EQ(sum, 6);
}

TEST(IterationTest, RangeForArrayConst) {
    const auto arr = JsonDocument(std::vector<JsonDocument>{10, 20, 30});
    int sum = 0;
    for (const auto& elem : arr) {
        sum += elem.as<int>();
    }
    // NOLINTNEXTLINE(readability-magic-numbers)
    EXPECT_EQ(sum, 60);
}

TEST(IterationTest, RangeForEmptyArray) {
    auto arr = JsonDocument::make_array();
    int count = 0;
    for (const auto& elem : arr) {
        (void)elem;
        ++count;
    }
    EXPECT_EQ(count, 0);
}

TEST(IterationTest, MutableIteration) {
    auto arr = JsonDocument(std::vector<JsonDocument>{1, 2, 3});
    for (auto& elem : arr) {
        // Replace each element with its value * 10
        // NOLINTNEXTLINE(readability-magic-numbers)
        elem = JsonDocument(elem.as<int>() * 10);
    }
    // NOLINTNEXTLINE(readability-magic-numbers)
    EXPECT_EQ(arr[0].as<int>(), 10);
    // NOLINTNEXTLINE(readability-magic-numbers)
    EXPECT_EQ(arr[1].as<int>(), 20);
    // NOLINTNEXTLINE(readability-magic-numbers)
    EXPECT_EQ(arr[2].as<int>(), 30);
}

TEST(IterationTest, BeginEndOnNonArrayThrows) {
    JsonDocument obj{{"k", 1}};
    EXPECT_THROW(obj.begin(), TypeException);
    EXPECT_THROW(obj.end(), TypeException);

    JsonDocument num = 42;
    EXPECT_THROW(num.begin(), TypeException);
}

TEST(IterationTest, ItemsStructuredBinding) {
    // NOLINTNEXTLINE(readability-magic-numbers)
    JsonDocument obj{{"name", "Alice"}, {"age", 30}};
    std::map<std::string, std::string> collected;
    for (const auto& [key, value] : obj.items()) {
        collected[key] = value.to_json();
    }
    EXPECT_EQ(collected.size(), 2);
    EXPECT_EQ(collected["name"], "\"Alice\"");
    EXPECT_EQ(collected["age"], "30");
}

TEST(IterationTest, ItemsConst) {
    // NOLINTNEXTLINE(readability-magic-numbers)
    const JsonDocument obj{{"x", 10}, {"y", 20}};
    int sum = 0;
    for (const auto& [key, value] : obj.items()) {
        sum += value.as<int>();
    }
    // NOLINTNEXTLINE(readability-magic-numbers)
    EXPECT_EQ(sum, 30);
}

TEST(IterationTest, ItemsMutable) {
    JsonDocument obj{{"a", 1}, {"b", 2}};
    for (auto& [key, value] : obj.items()) {
        // NOLINTNEXTLINE(readability-magic-numbers)
        value = JsonDocument(value.as<int>() * 100);
    }
    // NOLINTNEXTLINE(readability-magic-numbers)
    EXPECT_EQ(obj["a"].as<int>(), 100);
    // NOLINTNEXTLINE(readability-magic-numbers)
    EXPECT_EQ(obj["b"].as<int>(), 200);
}

TEST(IterationTest, ItemsOnNonObjectThrows) {
    auto arr = JsonDocument(std::vector<JsonDocument>{1, 2});
    EXPECT_THROW(arr.items(), TypeException);

    JsonDocument num = 42;
    EXPECT_THROW(num.items(), TypeException);
}

TEST(IterationTest, ItemsEmptyObject) {
    auto obj = JsonDocument::make_object();
    int count = 0;
    for (const auto& [key, value] : obj.items()) {
        (void)key;
        (void)value;
        ++count;
    }
    EXPECT_EQ(count, 0);
}

TEST(IterationTest, Keys) {
    JsonDocument obj{{"c", 3}, {"a", 1}, {"b", 2}};
    auto key_list = obj.keys();
    // std::map is sorted, so keys should be in order
    EXPECT_EQ(key_list.size(), 3);
    EXPECT_EQ(key_list[0], "a");
    EXPECT_EQ(key_list[1], "b");
    EXPECT_EQ(key_list[2], "c");
}

TEST(IterationTest, KeysEmpty) {
    auto obj = JsonDocument::make_object();
    auto key_list = obj.keys();
    EXPECT_TRUE(key_list.empty());
}

TEST(IterationTest, KeysOnNonObjectThrows) {
    auto arr = JsonDocument(std::vector<JsonDocument>{1});
    EXPECT_THROW(arr.keys(), TypeException);
}
