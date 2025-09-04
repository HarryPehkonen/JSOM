#include <gtest/gtest.h>
#include <jsom/jsom.hpp>

using namespace jsom;
using namespace jsom::literals;

TEST(APICompatibilityTest, ParseDocument) {
    auto doc = parse_document(R"({"name": "John", "age": 30})");
    EXPECT_TRUE(doc.is_object());
}

TEST(APICompatibilityTest, TypeChecking) {
    auto doc = parse_document(R"({"name": "John", "age": 30, "active": true, "data": null})");

    EXPECT_TRUE(doc["name"].is_string());
    EXPECT_TRUE(doc["age"].is_number());
    EXPECT_TRUE(doc["active"].is_bool());
    EXPECT_TRUE(doc["data"].is_null());

    EXPECT_EQ(doc.type(), JsonType::Object);
}

TEST(APICompatibilityTest, ValueAccess) {
    auto doc = parse_document(R"({
        "name": "John",
        "age": 30,
        "price": 75000.50,
        "active": true
    })");

    auto name = doc["name"].as<std::string>();
    auto age = doc["age"].as<int>();
    auto price = doc["price"].as<double>();
    auto active = doc["active"].as<bool>();

    EXPECT_EQ(name, "John");
    EXPECT_EQ(age, 30);
    EXPECT_EQ(price, 75000.50);
    EXPECT_EQ(active, true);
}

TEST(APICompatibilityTest, ContainerAccess) {
    auto doc = parse_document(R"({
        "data": [
            {"name": "Item1", "price": {"amount": 10.50}},
            {"name": "Item2", "price": {"amount": 20.75}}
        ],
        "pagination": {"page": 1, "total": 100}
    })");

    auto first_item = doc["data"][0];
    auto item_name = doc["data"][0]["name"].as<std::string>();
    auto item_price = doc["data"][0]["price"]["amount"].as<double>();
    auto page = doc["pagination"]["page"].as<int>();

    EXPECT_EQ(item_name, "Item1");
    EXPECT_EQ(item_price, 10.50);
    EXPECT_EQ(page, 1);
}

TEST(APICompatibilityTest, Construction) {
    JsonDocument doc{{"name", JsonDocument("John Doe")},
                     // NOLINTNEXTLINE(readability-magic-numbers)
                     {"age", JsonDocument(30)},
                     // NOLINTNEXTLINE(readability-magic-numbers)
                     {"salary", JsonDocument(75000.50)},
                     {"active", JsonDocument(true)},
                     {"tags", JsonDocument{JsonDocument("developer"), JsonDocument("senior")}},
                     {"address", JsonDocument{{"street", JsonDocument("123 Main St")},
                                              // NOLINTNEXTLINE(readability-magic-numbers)
                                              {"zip", JsonDocument(12345)}}}};

    EXPECT_EQ(doc["name"].as<std::string>(), "John Doe");
    EXPECT_EQ(doc["age"].as<int>(), 30);
    EXPECT_EQ(doc["salary"].as<double>(), 75000.50);
    EXPECT_EQ(doc["active"].as<bool>(), true);
    EXPECT_EQ(doc["tags"][0].as<std::string>(), "developer");
    EXPECT_EQ(doc["address"]["street"].as<std::string>(), "123 Main St");
}

TEST(APICompatibilityTest, Serialization) {
    auto doc = parse_document(R"({"name": "John", "age": 30})");
    auto output = doc.to_json();

    EXPECT_TRUE(output.find("\"name\":\"John\"") != std::string::npos);
    EXPECT_TRUE(output.find("\"age\":30") != std::string::npos);
}

TEST(APICompatibilityTest, ErrorHandling) {
    auto doc = parse_document(R"({"value": "not_a_number"})");

    EXPECT_THROW(doc["value"].as<double>(), TypeException);
    EXPECT_THROW(doc["missing_key"], std::out_of_range);
}

TEST(APICompatibilityTest, OptionalAccess) {
    auto doc = parse_document(R"({"age": 30})");

    auto maybe_age = doc["age"].try_as<int>();
    EXPECT_TRUE(maybe_age.has_value());
    if (maybe_age.has_value()) {
        EXPECT_EQ(*maybe_age, 30);
    }
}

TEST(APICompatibilityTest, LiteralOperator) {
    // Test object creation with _jsom literal
    auto obj = R"({"name": "Alice", "age": 25, "active": true})"_jsom;
    EXPECT_TRUE(obj.is_object());
    EXPECT_EQ(obj["name"].as<std::string>(), "Alice");
    EXPECT_EQ(obj["age"].as<int>(), 25);
    EXPECT_EQ(obj["active"].as<bool>(), true);

    // Test array creation
    auto arr = R"([1, 2, 3, "test"])"_jsom;
    EXPECT_TRUE(arr.is_array());
    EXPECT_EQ(arr[0].as<int>(), 1);
    EXPECT_EQ(arr[3].as<std::string>(), "test");

    // Test simple values
    auto str_val = R"("hello")"_jsom;
    auto num_val = R"(42)"_jsom;
    auto bool_val = R"(true)"_jsom;
    auto null_val = R"(null)"_jsom;

    EXPECT_TRUE(str_val.is_string());
    EXPECT_EQ(str_val.as<std::string>(), "hello");

    EXPECT_TRUE(num_val.is_number());
    EXPECT_EQ(num_val.as<int>(), 42);

    EXPECT_TRUE(bool_val.is_bool());
    EXPECT_EQ(bool_val.as<bool>(), true);

    EXPECT_TRUE(null_val.is_null());

    // Test that it's equivalent to parse_document
    auto regular = parse_document(R"({"test": "value"})");
    auto literal = R"({"test": "value"})"_jsom;

    EXPECT_EQ(regular["test"].as<std::string>(), literal["test"].as<std::string>());
    EXPECT_EQ(regular.to_json(), literal.to_json());

    // Test error handling - invalid JSON should throw
    EXPECT_THROW(R"({invalid json})"_jsom, std::runtime_error);
}
