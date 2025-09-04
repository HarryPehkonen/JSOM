#include <gtest/gtest.h>
#include <jsom/json_document.hpp>

using namespace jsom;

constexpr int TEST_PERSON_AGE = 30;

TEST(JsonDocumentTest, ConstructNull) {
    JsonDocument doc;
    EXPECT_TRUE(doc.is_null());
    EXPECT_EQ(doc.type(), JsonType::Null);
}

TEST(JsonDocumentTest, ConstructBoolean) {
    JsonDocument doc(true);
    EXPECT_TRUE(doc.is_bool());
    EXPECT_EQ(doc.as<bool>(), true);
}

TEST(JsonDocumentTest, ConstructNumber) {
    // NOLINTNEXTLINE(readability-magic-numbers)
    JsonDocument doc(42);
    EXPECT_TRUE(doc.is_number());
    EXPECT_EQ(doc.as<int>(), 42);
    EXPECT_EQ(doc.as<double>(), 42.0);
}

TEST(JsonDocumentTest, ConstructString) {
    JsonDocument doc("hello");
    EXPECT_TRUE(doc.is_string());
    EXPECT_EQ(doc.as<std::string>(), "hello");
}

TEST(JsonDocumentTest, ConstructObject) {
    // NOLINTNEXTLINE(readability-magic-numbers)
    JsonDocument doc{{"name", JsonDocument("John")}, {"age", JsonDocument(30)}};
    EXPECT_TRUE(doc.is_object());
    EXPECT_EQ(doc["name"].as<std::string>(), "John");
    EXPECT_EQ(doc["age"].as<int>(), 30);
}

TEST(JsonDocumentTest, ConstructArray) {
    JsonDocument doc{JsonDocument(1), JsonDocument(2), JsonDocument(3)};
    EXPECT_TRUE(doc.is_array());
    EXPECT_EQ(doc[0].as<int>(), 1);
    EXPECT_EQ(doc[1].as<int>(), 2);
    EXPECT_EQ(doc[2].as<int>(), 3);
}

TEST(JsonDocumentTest, TypeValidation) {
    JsonDocument doc("hello");
    EXPECT_THROW(doc.as<int>(), TypeException);
    EXPECT_THROW(doc["key"], TypeException);
    EXPECT_THROW(doc[0], TypeException);
}

TEST(JsonDocumentTest, OptionalAccess) {
    // NOLINTNEXTLINE(readability-magic-numbers)
    JsonDocument doc(42);
    auto maybe_int = doc.try_as<int>();
    EXPECT_TRUE(maybe_int.has_value());
    if (maybe_int.has_value()) {
        EXPECT_EQ(*maybe_int, 42);
    }

    auto maybe_string = doc.try_as<std::string>();
    EXPECT_FALSE(maybe_string.has_value());
}

TEST(JsonDocumentTest, Serialization) {
    JsonDocument doc{{"name", JsonDocument("John")},
                     {"age", JsonDocument(TEST_PERSON_AGE)},
                     {"active", JsonDocument(true)}};

    std::string json = doc.to_json();
    EXPECT_TRUE(json.find("\"name\":\"John\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"age\":30") != std::string::npos);
    EXPECT_TRUE(json.find("\"active\":true") != std::string::npos);
}

TEST(JsonDocumentTest, LazyNumberCreation) {
    JsonDocument doc = JsonDocument::from_lazy_number("1.0");
    EXPECT_TRUE(doc.is_number());
    EXPECT_EQ(doc.as<double>(), 1.0);

    std::string serialized = doc.to_json();
    EXPECT_EQ(serialized, "1.0");
}
