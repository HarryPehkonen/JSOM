#include <gtest/gtest.h>
#include <jsom/batch_parser.hpp>

using namespace jsom;

TEST(ParseDocumentTest, ParseSimpleString) {
    auto doc = parse_document(R"("hello")");
    EXPECT_TRUE(doc.is_string());
    EXPECT_EQ(doc.as<std::string>(), "hello");
}

TEST(ParseDocumentTest, ParseNumber) {
    auto doc = parse_document("42");
    EXPECT_TRUE(doc.is_number());
    EXPECT_EQ(doc.as<int>(), 42);
}

TEST(ParseDocumentTest, ParseBoolean) {
    auto doc = parse_document("true");
    EXPECT_TRUE(doc.is_bool());
    EXPECT_EQ(doc.as<bool>(), true);
}

TEST(ParseDocumentTest, ParseNull) {
    auto doc = parse_document("null");
    EXPECT_TRUE(doc.is_null());
}

TEST(ParseDocumentTest, ParseSimpleObject) {
    auto doc = parse_document(R"({"name": "John", "age": 30})");
    EXPECT_TRUE(doc.is_object());
    EXPECT_EQ(doc["name"].as<std::string>(), "John");
    EXPECT_EQ(doc["age"].as<int>(), 30);
}

TEST(ParseDocumentTest, ParseSimpleArray) {
    auto doc = parse_document("[1, 2, 3]");
    EXPECT_TRUE(doc.is_array());
    EXPECT_EQ(doc[0].as<int>(), 1);
    EXPECT_EQ(doc[1].as<int>(), 2);
    EXPECT_EQ(doc[2].as<int>(), 3);
}

TEST(ParseDocumentTest, ParseNestedStructure) {
    auto doc = parse_document(R"({
        "user": {
            "name": "John",
            "details": {
                "age": 30,
                "scores": [85, 92, 78]
            }
        }
    })");

    EXPECT_TRUE(doc.is_object());
    EXPECT_EQ(doc["user"]["name"].as<std::string>(), "John");
    EXPECT_EQ(doc["user"]["details"]["age"].as<int>(), 30);
    EXPECT_EQ(doc["user"]["details"]["scores"][0].as<int>(), 85);
    EXPECT_EQ(doc["user"]["details"]["scores"][1].as<int>(), 92);
    EXPECT_EQ(doc["user"]["details"]["scores"][2].as<int>(), 78);
}

TEST(ParseDocumentTest, NumberFormatPreservation) {
    auto doc = parse_document(R"({"price": "1.0", "scientific": "1e10"})");

    std::string serialized = doc.to_json();
    EXPECT_TRUE(serialized.find("1.0") != std::string::npos);
    EXPECT_TRUE(serialized.find("1e10") != std::string::npos);
}

TEST(ParseDocumentTest, ErrorHandling) {
    EXPECT_THROW(parse_document("{invalid json}"), std::runtime_error);
    EXPECT_THROW(parse_document("[1, 2,]"), std::runtime_error);
}