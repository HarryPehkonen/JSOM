#include <gtest/gtest.h>
#include <jsom/jsom.hpp>
#include <map>
#include <vector>

using namespace jsom;

// Test Option 2: Direct container constructors
TEST(ErgonomicConstructionTest, DirectMapConstructor) {
    std::map<std::string, JsonDocument> obj_map;
    obj_map["name"] = JsonDocument("Alice");
    obj_map["age"] = JsonDocument(30);
    obj_map["active"] = JsonDocument(true);

    JsonDocument doc(std::move(obj_map));

    ASSERT_TRUE(doc.is_object());
    EXPECT_EQ(doc["name"].as<std::string>(), "Alice");
    EXPECT_EQ(doc["age"].as<int>(), 30);
    EXPECT_EQ(doc["active"].as<bool>(), true);
}

TEST(ErgonomicConstructionTest, DirectVectorConstructor) {
    std::vector<JsonDocument> arr_vec;
    arr_vec.push_back(JsonDocument(1));
    arr_vec.push_back(JsonDocument(2));
    arr_vec.push_back(JsonDocument(3));

    JsonDocument doc(std::move(arr_vec));

    ASSERT_TRUE(doc.is_array());
    EXPECT_EQ(doc[0].as<int>(), 1);
    EXPECT_EQ(doc[1].as<int>(), 2);
    EXPECT_EQ(doc[2].as<int>(), 3);
}

TEST(ErgonomicConstructionTest, DirectConstructorRoundTrip) {
    std::map<std::string, JsonDocument> obj_map;
    obj_map["key1"] = JsonDocument("value1");
    obj_map["key2"] = JsonDocument(42);

    JsonDocument doc(std::move(obj_map));
    std::string json = doc.to_json();

    auto parsed = parse_document(json);
    EXPECT_EQ(parsed["key1"].as<std::string>(), "value1");
    EXPECT_EQ(parsed["key2"].as<int>(), 42);
}

// Test Option 1: Factory methods with explicit converter
TEST(ErgonomicConstructionTest, FromMapWithConverter) {
    std::map<std::string, size_t> frequency_map;
    frequency_map["apple"] = 5;
    frequency_map["banana"] = 3;
    frequency_map["cherry"] = 7;

    auto doc = JsonDocument::from_map(frequency_map, [](size_t v) {
        return JsonDocument(static_cast<int>(v));
    });

    ASSERT_TRUE(doc.is_object());
    EXPECT_EQ(doc["apple"].as<int>(), 5);
    EXPECT_EQ(doc["banana"].as<int>(), 3);
    EXPECT_EQ(doc["cherry"].as<int>(), 7);
}

TEST(ErgonomicConstructionTest, FromVectorWithConverter) {
    std::vector<size_t> sizes = {100, 200, 300};

    auto doc = JsonDocument::from_vector(sizes, [](size_t v) {
        return JsonDocument(static_cast<int>(v));
    });

    ASSERT_TRUE(doc.is_array());
    EXPECT_EQ(doc[0].as<int>(), 100);
    EXPECT_EQ(doc[1].as<int>(), 200);
    EXPECT_EQ(doc[2].as<int>(), 300);
}

// Test Option 1: Factory methods with automatic conversion
TEST(ErgonomicConstructionTest, FromMapAutoConvertInt) {
    std::map<std::string, int> scores;
    scores["alice"] = 95;
    scores["bob"] = 87;
    scores["charlie"] = 92;

    auto doc = JsonDocument::from_map(scores);

    ASSERT_TRUE(doc.is_object());
    EXPECT_EQ(doc["alice"].as<int>(), 95);
    EXPECT_EQ(doc["bob"].as<int>(), 87);
    EXPECT_EQ(doc["charlie"].as<int>(), 92);
}

TEST(ErgonomicConstructionTest, FromMapAutoConvertDouble) {
    std::map<std::string, double> measurements;
    measurements["temperature"] = 23.5;
    measurements["humidity"] = 65.2;
    measurements["pressure"] = 1013.25;

    auto doc = JsonDocument::from_map(measurements);

    ASSERT_TRUE(doc.is_object());
    EXPECT_DOUBLE_EQ(doc["temperature"].as<double>(), 23.5);
    EXPECT_DOUBLE_EQ(doc["humidity"].as<double>(), 65.2);
    EXPECT_DOUBLE_EQ(doc["pressure"].as<double>(), 1013.25);
}

TEST(ErgonomicConstructionTest, FromMapAutoConvertString) {
    std::map<std::string, std::string> labels;
    labels["title"] = "My Document";
    labels["author"] = "John Doe";
    labels["version"] = "1.0.0";

    auto doc = JsonDocument::from_map(labels);

    ASSERT_TRUE(doc.is_object());
    EXPECT_EQ(doc["title"].as<std::string>(), "My Document");
    EXPECT_EQ(doc["author"].as<std::string>(), "John Doe");
    EXPECT_EQ(doc["version"].as<std::string>(), "1.0.0");
}

TEST(ErgonomicConstructionTest, FromMapAutoConvertBool) {
    std::map<std::string, bool> flags;
    flags["enabled"] = true;
    flags["debug"] = false;
    flags["verbose"] = true;

    auto doc = JsonDocument::from_map(flags);

    ASSERT_TRUE(doc.is_object());
    EXPECT_EQ(doc["enabled"].as<bool>(), true);
    EXPECT_EQ(doc["debug"].as<bool>(), false);
    EXPECT_EQ(doc["verbose"].as<bool>(), true);
}

TEST(ErgonomicConstructionTest, FromVectorAutoConvertInt) {
    std::vector<int> numbers = {10, 20, 30, 40, 50};

    auto doc = JsonDocument::from_vector(numbers);

    ASSERT_TRUE(doc.is_array());
    EXPECT_EQ(doc[0].as<int>(), 10);
    EXPECT_EQ(doc[2].as<int>(), 30);
    EXPECT_EQ(doc[4].as<int>(), 50);
}

TEST(ErgonomicConstructionTest, FromVectorAutoConvertString) {
    std::vector<std::string> names = {"Alice", "Bob", "Charlie"};

    auto doc = JsonDocument::from_vector(names);

    ASSERT_TRUE(doc.is_array());
    EXPECT_EQ(doc[0].as<std::string>(), "Alice");
    EXPECT_EQ(doc[1].as<std::string>(), "Bob");
    EXPECT_EQ(doc[2].as<std::string>(), "Charlie");
}

// Complex use case: nested structures
TEST(ErgonomicConstructionTest, ComplexNestedConstruction) {
    // Simulate NameAnalyzer use case
    std::map<std::string, size_t> frequency_map;
    frequency_map["function"] = 42;
    frequency_map["variable"] = 38;
    frequency_map["class"] = 15;

    std::map<std::string, double> averages;
    averages["avg_length"] = 12.5;
    averages["avg_complexity"] = 3.7;

    // Build nested structure using new factories
    std::map<std::string, JsonDocument> analysis;
    analysis["frequencies"] = JsonDocument::from_map(frequency_map, [](size_t v) {
        return JsonDocument(static_cast<int>(v));
    });
    analysis["averages"] = JsonDocument::from_map(averages);

    JsonDocument doc(std::move(analysis));

    ASSERT_TRUE(doc.is_object());
    ASSERT_TRUE(doc["frequencies"].is_object());
    ASSERT_TRUE(doc["averages"].is_object());
    EXPECT_EQ(doc["frequencies"]["function"].as<int>(), 42);
    EXPECT_DOUBLE_EQ(doc["averages"]["avg_length"].as<double>(), 12.5);
}

// Edge cases
TEST(ErgonomicConstructionTest, EmptyMapFactory) {
    std::map<std::string, int> empty_map;
    auto doc = JsonDocument::from_map(empty_map);

    ASSERT_TRUE(doc.is_object());
    EXPECT_EQ(doc.to_json(), "{}");
}

TEST(ErgonomicConstructionTest, EmptyVectorFactory) {
    std::vector<int> empty_vec;
    auto doc = JsonDocument::from_vector(empty_vec);

    ASSERT_TRUE(doc.is_array());
    EXPECT_EQ(doc.to_json(), "[]");
}

TEST(ErgonomicConstructionTest, EmptyDirectMapConstructor) {
    std::map<std::string, JsonDocument> empty_map;
    JsonDocument doc(std::move(empty_map));

    ASSERT_TRUE(doc.is_object());
    EXPECT_EQ(doc.to_json(), "{}");
}

TEST(ErgonomicConstructionTest, EmptyDirectVectorConstructor) {
    std::vector<JsonDocument> empty_vec;
    JsonDocument doc(std::move(empty_vec));

    ASSERT_TRUE(doc.is_array());
    EXPECT_EQ(doc.to_json(), "[]");
}

// Serialization tests
TEST(ErgonomicConstructionTest, FactoryMethodRoundTrip) {
    std::map<std::string, int> original_data;
    original_data["x"] = 10;
    original_data["y"] = 20;
    original_data["z"] = 30;

    auto doc = JsonDocument::from_map(original_data);
    std::string json = doc.to_json();

    auto parsed = parse_document(json);
    EXPECT_EQ(parsed["x"].as<int>(), 10);
    EXPECT_EQ(parsed["y"].as<int>(), 20);
    EXPECT_EQ(parsed["z"].as<int>(), 30);
}

// Performance characteristic: move semantics
TEST(ErgonomicConstructionTest, MoveSemantics) {
    std::map<std::string, JsonDocument> large_map;
    for (int i = 0; i < 1000; ++i) {
        large_map["key" + std::to_string(i)] = JsonDocument(i);
    }

    // Should move, not copy
    JsonDocument doc(std::move(large_map));

    ASSERT_TRUE(doc.is_object());
    EXPECT_EQ(doc["key0"].as<int>(), 0);
    EXPECT_EQ(doc["key999"].as<int>(), 999);
}

// Custom converter with complex logic
TEST(ErgonomicConstructionTest, ComplexConverter) {
    std::map<std::string, int> raw_data;
    raw_data["small"] = 5;
    raw_data["medium"] = 50;
    raw_data["large"] = 500;

    auto doc = JsonDocument::from_map(raw_data, [](int v) {
        // Complex conversion logic
        if (v < 10) {
            return JsonDocument("small");
        } else if (v < 100) {
            return JsonDocument("medium");
        } else {
            return JsonDocument("large");
        }
    });

    ASSERT_TRUE(doc.is_object());
    EXPECT_EQ(doc["small"].as<std::string>(), "small");
    EXPECT_EQ(doc["medium"].as<std::string>(), "medium");
    EXPECT_EQ(doc["large"].as<std::string>(), "large");
}
