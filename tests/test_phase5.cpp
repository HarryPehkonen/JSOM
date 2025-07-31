#include "jsom.hpp"
#include <gtest/gtest.h>
#include <vector>

class Phase5Test : public ::testing::Test {
protected:
    void SetUp() override {
        parser = std::make_unique<jsom::StreamingParser>();
        values_.clear();
        errors_.clear();
        object_enters_.clear();
        array_enters_.clear();
        container_exits_.clear();

        // Set up event callbacks
        jsom::ParseEvents events;
        events.on_value = [this](const jsom::JsonEvent& value) {
            values_.emplace_back(value.type(), value.raw_value(), value.path());
        };
        events.on_error = [this](const jsom::ParseError& error) {
            errors_.emplace_back(error.position, error.message, error.json_pointer);
        };
        events.on_enter_object = [this](const std::string& key) { object_enters_.push_back(key); };
        events.on_enter_array = [this]() { array_enters_.emplace_back("array"); };
        events.on_exit_container = [this]() { container_exits_.emplace_back("exit"); };

        parser->set_events(events);
    }

    void parse(const std::string& json) {
        parser->parse_string(json);
        parser->end_input();
    }

    std::unique_ptr<jsom::StreamingParser> parser;
    std::vector<std::tuple<jsom::JsonType, std::string, std::string>> values_;
    std::vector<std::tuple<std::size_t, std::string, std::string>> errors_;
    std::vector<std::string> object_enters_;
    std::vector<std::string> array_enters_;
    std::vector<std::string> container_exits_;
};

// Object content tests
TEST_F(Phase5Test, SimpleObjectWithStringValue) {
    parse(R"({"key": "value"})");

    EXPECT_TRUE(errors_.empty());
    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::String);
    EXPECT_EQ(std::get<1>(values_[0]), "value");

    ASSERT_EQ(object_enters_.size(), 1);
    EXPECT_EQ(object_enters_[0], ""); // Root object
    ASSERT_EQ(container_exits_.size(), 1);
}

TEST_F(Phase5Test, ObjectWithMultipleKeyValues) {
    parse(R"({"name": "John", "age": 30, "active": true})");

    EXPECT_TRUE(errors_.empty());
    ASSERT_EQ(values_.size(), 3);

    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::String);
    EXPECT_EQ(std::get<1>(values_[0]), "John");

    EXPECT_EQ(std::get<0>(values_[1]), jsom::JsonType::Number);
    EXPECT_EQ(std::get<1>(values_[1]), "30");

    EXPECT_EQ(std::get<0>(values_[2]), jsom::JsonType::Boolean);
    EXPECT_EQ(std::get<1>(values_[2]), "true");

    ASSERT_EQ(object_enters_.size(), 1);
    ASSERT_EQ(container_exits_.size(), 1);
}

TEST_F(Phase5Test, ObjectWithVariousValueTypes) {
    parse(R"({"str": "hello", "num": 42, "bool": false, "null": null})");

    EXPECT_TRUE(errors_.empty());
    ASSERT_EQ(values_.size(), 4);

    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::String);
    EXPECT_EQ(std::get<1>(values_[0]), "hello");

    EXPECT_EQ(std::get<0>(values_[1]), jsom::JsonType::Number);
    EXPECT_EQ(std::get<1>(values_[1]), "42");

    EXPECT_EQ(std::get<0>(values_[2]), jsom::JsonType::Boolean);
    EXPECT_EQ(std::get<1>(values_[2]), "false");

    EXPECT_EQ(std::get<0>(values_[3]), jsom::JsonType::Null);
    EXPECT_EQ(std::get<1>(values_[3]), "null");
}

// Array content tests
TEST_F(Phase5Test, ArrayWithMixedValues) {
    parse(R"([1, "two", true, null, -3.14])");

    EXPECT_TRUE(errors_.empty());
    ASSERT_EQ(values_.size(), 5);

    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::Number);
    EXPECT_EQ(std::get<1>(values_[0]), "1");

    EXPECT_EQ(std::get<0>(values_[1]), jsom::JsonType::String);
    EXPECT_EQ(std::get<1>(values_[1]), "two");

    EXPECT_EQ(std::get<0>(values_[2]), jsom::JsonType::Boolean);
    EXPECT_EQ(std::get<1>(values_[2]), "true");

    EXPECT_EQ(std::get<0>(values_[3]), jsom::JsonType::Null);
    EXPECT_EQ(std::get<1>(values_[3]), "null");

    EXPECT_EQ(std::get<0>(values_[4]), jsom::JsonType::Number);
    EXPECT_EQ(std::get<1>(values_[4]), "-3.14");

    ASSERT_EQ(array_enters_.size(), 1);
    ASSERT_EQ(container_exits_.size(), 1);
}

// Nested structure tests
TEST_F(Phase5Test, NestedObjectsAndArrays) {
    parse(R"({"data": [{"id": 1}, {"id": 2}]})");

    EXPECT_TRUE(errors_.empty());
    ASSERT_EQ(values_.size(), 2);

    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::Number);
    EXPECT_EQ(std::get<1>(values_[0]), "1");

    EXPECT_EQ(std::get<0>(values_[1]), jsom::JsonType::Number);
    EXPECT_EQ(std::get<1>(values_[1]), "2");

    // Should have root object + 2 nested objects
    ASSERT_EQ(object_enters_.size(), 3);
    ASSERT_EQ(array_enters_.size(), 1);
    ASSERT_EQ(container_exits_.size(), 4); // 2 nested objects + 1 array + 1 root object
}

TEST_F(Phase5Test, ArrayOfArrays) {
    parse(R"([[1, 2], [3, 4, 5]])");

    EXPECT_TRUE(errors_.empty());
    ASSERT_EQ(values_.size(), 5);

    EXPECT_EQ(std::get<1>(values_[0]), "1");
    EXPECT_EQ(std::get<1>(values_[1]), "2");
    EXPECT_EQ(std::get<1>(values_[2]), "3");
    EXPECT_EQ(std::get<1>(values_[3]), "4");
    EXPECT_EQ(std::get<1>(values_[4]), "5");

    ASSERT_EQ(array_enters_.size(), 3); // Root array + 2 nested arrays
    ASSERT_EQ(container_exits_.size(), 3);
}

// Error handling tests
TEST_F(Phase5Test, ObjectKeyMustBeString) {
    parse(R"({42: "value"})");

    // Debug: Print errors
    for (const auto& error : errors_) {
        std::cout << "Error: " << std::get<1>(error) << "\n";
    }

    EXPECT_TRUE(values_.empty());
    ASSERT_EQ(errors_.size(), 1);
    EXPECT_EQ(std::get<1>(errors_[0]), "Object key must be a string");
}

TEST_F(Phase5Test, MissingColonInObject) {
    parse(R"({"key" "value"})");

    EXPECT_TRUE(values_.empty());
    ASSERT_GE(errors_.size(), 1);
    EXPECT_EQ(std::get<1>(errors_[0]), "Expected ':' after object key");
}

TEST_F(Phase5Test, UnexpectedColonInArray) {
    parse(R"([1: 2])");

    // Parser should detect the error but may continue parsing
    ASSERT_GE(errors_.size(), 1);
    EXPECT_EQ(std::get<1>(errors_[0]), "Unexpected ':' outside of object");

    // Note: Parser continues and parses values despite error (acceptable behavior)
    EXPECT_GE(values_.size(), 1);
}

TEST_F(Phase5Test, ExpectedKeyBeforeColon) {
    parse(R"({: "value"})");

    EXPECT_TRUE(values_.empty());
    ASSERT_GE(errors_.size(), 1);
    EXPECT_EQ(std::get<1>(errors_[0]), "Expected key before ':'");
}

// Edge cases
TEST_F(Phase5Test, EmptyStringKey) {
    parse(R"({"": "value"})");

    EXPECT_TRUE(errors_.empty());
    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<1>(values_[0]), "value");
}

TEST_F(Phase5Test, ObjectWithNestedEmptyContainers) {
    parse(R"({"empty_obj": {}, "empty_arr": []})");

    EXPECT_TRUE(errors_.empty());
    EXPECT_TRUE(values_.empty()); // No actual values, just containers

    ASSERT_EQ(object_enters_.size(), 2);   // Root + 1 nested object (empty_obj)
    ASSERT_EQ(array_enters_.size(), 1);    // 1 nested array (empty_arr)
    ASSERT_EQ(container_exits_.size(), 3); // 1 nested object + 1 nested array + 1 root object
}

TEST_F(Phase5Test, SpecialCaseFromPlan) {
    // Test for the bug described in DECISION-004: ensure value is not contaminated by closing brace
    parse(R"({"key":"value"})");

    EXPECT_TRUE(errors_.empty());
    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::String);
    EXPECT_EQ(std::get<1>(values_[0]), "value"); // Must be exactly "value", not contaminated
}
