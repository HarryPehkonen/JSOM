#include "jsom.hpp"
#include <gtest/gtest.h>

namespace {
constexpr int TEST_INT_VALUE = 42;
constexpr double TEST_DOUBLE_VALUE = 3.14;
constexpr int TEST_AGE_VALUE = 30;
constexpr int TEST_OBJECT_VALUE = 123;
constexpr double TEST_PRECISION = 0.001;
} // namespace

class BatchParserTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test basic JsonDocument construction
TEST_F(BatchParserTest, JsonDocumentConstruction) {
    // Test null value
    jsom::JsonDocument null_val;
    EXPECT_TRUE(null_val.is_null());
    EXPECT_FALSE(null_val.is_bool());

    // Test boolean values
    jsom::JsonDocument bool_true(true);
    jsom::JsonDocument bool_false(false);
    EXPECT_TRUE(bool_true.is_bool());
    EXPECT_TRUE(bool_false.is_bool());
    EXPECT_EQ(bool_true.as<bool>(), true);
    EXPECT_EQ(bool_false.as<bool>(), false);

    // Test numeric values
    jsom::JsonDocument int_val(TEST_INT_VALUE);
    jsom::JsonDocument double_val(TEST_DOUBLE_VALUE);
    EXPECT_TRUE(int_val.is_number());
    EXPECT_TRUE(double_val.is_number());
    EXPECT_EQ(int_val.as<int>(), TEST_INT_VALUE);
    EXPECT_NEAR(double_val.as<double>(), TEST_DOUBLE_VALUE, TEST_PRECISION);

    // Test string values
    jsom::JsonDocument str_val("hello");
    jsom::JsonDocument string_val(std::string("world"));
    EXPECT_TRUE(str_val.is_string());
    EXPECT_TRUE(string_val.is_string());
    EXPECT_EQ(str_val.as<std::string>(), "hello");
    EXPECT_EQ(string_val.as<std::string>(), "world");
}

// Test initializer list construction
TEST_F(BatchParserTest, InitializerListConstruction) {
    // Test object construction
    jsom::JsonDocument obj = {{"name", jsom::JsonDocument("Alice")},
                              {"age", jsom::JsonDocument(TEST_AGE_VALUE)},
                              {"active", jsom::JsonDocument(true)}};

    EXPECT_TRUE(obj.is_object());

    // Test array construction
    jsom::JsonDocument arr = {jsom::JsonDocument(1), jsom::JsonDocument(2), jsom::JsonDocument(3)};

    EXPECT_TRUE(arr.is_array());
}

// Test copy and move semantics
TEST_F(BatchParserTest, CopyMoveSemantics) {
    jsom::JsonDocument original("test");

    // Test copy constructor
    jsom::JsonDocument copied = original;
    EXPECT_TRUE(copied.is_string());
    EXPECT_EQ(copied.as<std::string>(), "test");
    EXPECT_EQ(original.as<std::string>(), "test"); // Original should remain intact

    // Test move constructor
    jsom::JsonDocument moved = std::move(copied);
    EXPECT_TRUE(moved.is_string());
    EXPECT_EQ(moved.as<std::string>(), "test");
    // copied is now in moved-from state
}

// Test type conversion exceptions
TEST_F(BatchParserTest, TypeConversionExceptions) {
    jsom::JsonDocument str_val("hello");

    // Should throw when trying to convert string to number
    EXPECT_THROW(str_val.as<int>(), jsom::TypeException);
    EXPECT_THROW(str_val.as<double>(), jsom::TypeException);
    EXPECT_THROW(str_val.as<bool>(), jsom::TypeException);
}

// Test basic serialization
TEST_F(BatchParserTest, BasicSerialization) {
    // Test primitive serialization
    EXPECT_EQ(jsom::JsonDocument().to_json(), "null");
    EXPECT_EQ(jsom::JsonDocument(true).to_json(), "true");
    EXPECT_EQ(jsom::JsonDocument(false).to_json(), "false");
    EXPECT_EQ(jsom::JsonDocument(42).to_json(), "42");
    EXPECT_EQ(jsom::JsonDocument("hello").to_json(), "\"hello\"");

    // Test simple object serialization
    jsom::JsonDocument simple_obj
        = {{"name", jsom::JsonDocument("test")}, {"value", jsom::JsonDocument(TEST_OBJECT_VALUE)}};

    std::string json = simple_obj.to_json();
    EXPECT_FALSE(json.empty());
    // Check that the JSON contains the expected content
    EXPECT_TRUE(json.find("name") != std::string::npos);
    EXPECT_TRUE(json.find("test") != std::string::npos);
    EXPECT_TRUE(json.find("value") != std::string::npos);
    EXPECT_TRUE(json.find(std::to_string(TEST_OBJECT_VALUE)) != std::string::npos);

    // Test simple array serialization
    jsom::JsonDocument simple_arr
        = {jsom::JsonDocument(1), jsom::JsonDocument(2), jsom::JsonDocument(3)};

    std::string arr_json = simple_arr.to_json();
    EXPECT_EQ(arr_json, "[1,2,3]");
}

// Test container access methods
TEST_F(BatchParserTest, ContainerAccess) {
    // Test object access
    jsom::JsonDocument obj = {{"name", jsom::JsonDocument("Alice")},
                              {"age", jsom::JsonDocument(TEST_AGE_VALUE)},
                              {"active", jsom::JsonDocument(true)}};

    // Test bracket operator for objects
    EXPECT_EQ(obj["name"].as<std::string>(), "Alice");
    EXPECT_EQ(obj["age"].as<int>(), TEST_AGE_VALUE);
    EXPECT_TRUE(obj["active"].as<bool>());

    // Test at() method for objects
    EXPECT_EQ(obj.at("name").as<std::string>(), "Alice");
    EXPECT_THROW(obj.at("nonexistent"), std::out_of_range);

    // Test array access
    jsom::JsonDocument arr
        = {jsom::JsonDocument("first"), jsom::JsonDocument("second"), jsom::JsonDocument("third")};

    // Test bracket operator for arrays
    EXPECT_EQ(arr[0].as<std::string>(), "first");
    EXPECT_EQ(arr[1].as<std::string>(), "second");
    EXPECT_EQ(arr[2].as<std::string>(), "third");

    // Test at() method for arrays
    EXPECT_EQ(arr.at(0).as<std::string>(), "first");
    EXPECT_THROW(arr.at(5), std::out_of_range);

    // Test size and empty
    EXPECT_EQ(obj.size(), 3);
    EXPECT_FALSE(obj.empty());
    EXPECT_EQ(arr.size(), 3);
    EXPECT_FALSE(arr.empty());

    // Test type exceptions
    jsom::JsonDocument num(TEST_INT_VALUE);
    EXPECT_THROW(num["key"], jsom::TypeException);
    EXPECT_THROW(num[0], jsom::TypeException);
}

// Test recursive container access
TEST_F(BatchParserTest, RecursiveContainerAccess) {
    // Test nested structures
    jsom::JsonDocument nested = {
        {"users",
         jsom::JsonDocument(
             {jsom::JsonDocument({{"name", jsom::JsonDocument("Alice")},
                                  {"skills", jsom::JsonDocument({jsom::JsonDocument("C++"),
                                                                 jsom::JsonDocument("Python")})}}),
              jsom::JsonDocument(
                  {{"name", jsom::JsonDocument("Bob")},
                   {"skills", jsom::JsonDocument({jsom::JsonDocument("JavaScript"),
                                                  jsom::JsonDocument("TypeScript")})}})})},
        {"count", jsom::JsonDocument(2)}};

    // Test deep access
    EXPECT_EQ(nested["users"][0]["name"].as<std::string>(), "Alice");
    EXPECT_EQ(nested["users"][0]["skills"][1].as<std::string>(), "Python");
    EXPECT_EQ(nested["users"][1]["name"].as<std::string>(), "Bob");
    EXPECT_EQ(nested["users"][1]["skills"][0].as<std::string>(), "JavaScript");
    EXPECT_EQ(nested["count"].as<int>(), 2);
}

// Test simple object parsing (converted from debug test)
TEST_F(BatchParserTest, SimpleObjectParsing) {
    jsom::JsonDocument obj = jsom::parse_document(R"({"key": "value"})");

    EXPECT_TRUE(obj.is_object());
    EXPECT_EQ(obj.size(), 1);
    EXPECT_EQ(obj["key"].as<std::string>(), "value");
    EXPECT_EQ(obj.to_json(), R"({"key":"value"})");
}

// Test simple array parsing (converted from debug test)
TEST_F(BatchParserTest, SimpleArrayParsing) {
    jsom::JsonDocument arr = jsom::parse_document("[1, 2, 3]");

    EXPECT_TRUE(arr.is_array());
    EXPECT_EQ(arr.size(), 3);
    EXPECT_EQ(arr[0].as<int>(), 1);
    EXPECT_EQ(arr[1].as<int>(), 2);
    EXPECT_EQ(arr[2].as<int>(), 3);
    EXPECT_EQ(arr.to_json(), "[1,2,3]");
}

// Test multi-property object parsing
TEST_F(BatchParserTest, MultiPropertyObject) {
    jsom::JsonDocument obj = jsom::parse_document(R"({"name": "Alice", "age": 30})");

    EXPECT_TRUE(obj.is_object());
    EXPECT_EQ(obj.size(), 2);
    EXPECT_EQ(obj["name"].as<std::string>(), "Alice");
    EXPECT_EQ(obj["age"].as<int>(), TEST_AGE_VALUE);
}

// Test nested array in object
TEST_F(BatchParserTest, NestedArrayInObject) {
    jsom::JsonDocument nested = jsom::parse_document(R"({"numbers": [1, 2, 3]})");

    EXPECT_TRUE(nested.is_object());
    EXPECT_EQ(nested.size(), 1);

    auto numbers = nested["numbers"];

    EXPECT_TRUE(numbers.is_array());
    if (numbers.is_array()) {
        EXPECT_EQ(numbers.size(), 3);
        EXPECT_EQ(numbers[0].as<int>(), 1);
        EXPECT_EQ(numbers[1].as<int>(), 2);
        EXPECT_EQ(numbers[2].as<int>(), 3);
    }
}

// Test nested object in array
TEST_F(BatchParserTest, NestedObjectInArray) {
    jsom::JsonDocument nested = jsom::parse_document(R"([{"name": "Alice"}, {"name": "Bob"}])");

    EXPECT_TRUE(nested.is_array());
    EXPECT_EQ(nested.size(), 2);

    const auto& first_user = nested[0];
    EXPECT_TRUE(first_user.is_object());
    EXPECT_EQ(first_user["name"].as<std::string>(), "Alice");

    const auto& second_user = nested[1];
    EXPECT_TRUE(second_user.is_object());
    EXPECT_EQ(second_user["name"].as<std::string>(), "Bob");
}

// Test basic JSON parsing functionality
TEST_F(BatchParserTest, BasicJsonParsing) {
    // Test parsing primitive values
    jsom::JsonDocument null_doc = jsom::parse_document("null");
    EXPECT_TRUE(null_doc.is_null());

    jsom::JsonDocument bool_doc = jsom::parse_document("true");
    EXPECT_TRUE(bool_doc.is_bool());
    EXPECT_TRUE(bool_doc.as<bool>());

    jsom::JsonDocument false_doc = jsom::parse_document("false");
    EXPECT_TRUE(false_doc.is_bool());
    EXPECT_FALSE(false_doc.as<bool>());

    jsom::JsonDocument num_doc = jsom::parse_document("42");
    EXPECT_TRUE(num_doc.is_number());
    EXPECT_EQ(num_doc.as<int>(), 42);

    jsom::JsonDocument str_doc = jsom::parse_document("\"hello\"");
    EXPECT_TRUE(str_doc.is_string());
    EXPECT_EQ(str_doc.as<std::string>(), "hello");
}

// Test complex JSON parsing
TEST_F(BatchParserTest, ComplexJsonParsing) {
    // Test object parsing
    jsom::JsonDocument obj = jsom::parse_document(R"({"name": "Alice", "age": 30})");
    EXPECT_TRUE(obj.is_object());
    EXPECT_EQ(obj["name"].as<std::string>(), "Alice");
    EXPECT_EQ(obj["age"].as<int>(), TEST_AGE_VALUE);

    // Test array parsing
    jsom::JsonDocument arr = jsom::parse_document("[1, 2, 3]");
    EXPECT_TRUE(arr.is_array());
    EXPECT_EQ(arr[0].as<int>(), 1);
    EXPECT_EQ(arr[1].as<int>(), 2);
    EXPECT_EQ(arr[2].as<int>(), 3);

    // Test nested structures
    jsom::JsonDocument nested = jsom::parse_document(R"({
        "users": [
            {"name": "Alice", "active": true},
            {"name": "Bob", "active": false}
        ],
        "count": 2
    })");

    EXPECT_TRUE(nested.is_object());
    EXPECT_EQ(nested["users"][0]["name"].as<std::string>(), "Alice");
    EXPECT_TRUE(nested["users"][0]["active"].as<bool>());
    EXPECT_EQ(nested["users"][1]["name"].as<std::string>(), "Bob");
    EXPECT_FALSE(nested["users"][1]["active"].as<bool>());
    EXPECT_EQ(nested["count"].as<int>(), 2);
}