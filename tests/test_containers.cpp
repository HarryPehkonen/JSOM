#include "jsom.hpp"
#include <gtest/gtest.h>
#include <vector>

class ContainerTest : public ::testing::Test {
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

// Basic container tests
TEST_F(ContainerTest, ParseEmptyObject) {
    parse("{}");

    EXPECT_TRUE(values_.empty());
    EXPECT_TRUE(errors_.empty());
    ASSERT_EQ(object_enters_.size(), 1);
    EXPECT_EQ(object_enters_[0], "");
    ASSERT_EQ(container_exits_.size(), 1);
    EXPECT_EQ(container_exits_[0], "exit");
    EXPECT_TRUE(array_enters_.empty());
}

TEST_F(ContainerTest, ParseEmptyArray) {
    parse("[]");

    EXPECT_TRUE(values_.empty());
    EXPECT_TRUE(errors_.empty());
    ASSERT_EQ(array_enters_.size(), 1);
    EXPECT_EQ(array_enters_[0], "array");
    ASSERT_EQ(container_exits_.size(), 1);
    EXPECT_EQ(container_exits_[0], "exit");
    EXPECT_TRUE(object_enters_.empty());
}

// Nested empty containers
TEST_F(ContainerTest, ParseNestedEmptyArrays) {
    parse("[[]]");

    EXPECT_TRUE(values_.empty());
    EXPECT_TRUE(errors_.empty());
    ASSERT_EQ(array_enters_.size(), 2);
    EXPECT_EQ(array_enters_[0], "array");
    EXPECT_EQ(array_enters_[1], "array");
    ASSERT_EQ(container_exits_.size(), 2);
    EXPECT_EQ(container_exits_[0], "exit");
    EXPECT_EQ(container_exits_[1], "exit");
    EXPECT_TRUE(object_enters_.empty());
}

TEST_F(ContainerTest, ParseNestedEmptyObjects) {
    // This test should actually use valid JSON syntax
    parse(R"({"nested": {}})");

    EXPECT_TRUE(values_.empty());
    EXPECT_TRUE(errors_.empty());
    ASSERT_EQ(object_enters_.size(), 2);
    EXPECT_EQ(object_enters_[0], ""); // Root object
    EXPECT_EQ(object_enters_[1], ""); // Nested object
    ASSERT_EQ(container_exits_.size(), 2);
    EXPECT_TRUE(array_enters_.empty());
}

TEST_F(ContainerTest, ParseMixedNestedContainers) {
    parse("[{}]");

    EXPECT_TRUE(values_.empty());
    EXPECT_TRUE(errors_.empty());
    ASSERT_EQ(array_enters_.size(), 1);
    EXPECT_EQ(array_enters_[0], "array");
    ASSERT_EQ(object_enters_.size(), 1);
    EXPECT_EQ(object_enters_[0], "");
    ASSERT_EQ(container_exits_.size(), 2);
    EXPECT_EQ(container_exits_[0], "exit"); // Object closes first
    EXPECT_EQ(container_exits_[1], "exit"); // Array closes second
}

TEST_F(ContainerTest, ParseComplexNesting) {
    parse("[{}, [[]]]");

    EXPECT_TRUE(values_.empty());
    EXPECT_TRUE(errors_.empty());

    // Events should fire in order: array, object, object_exit, array, array, array_exit,
    // array_exit, array_exit
    ASSERT_EQ(array_enters_.size(), 3);
    ASSERT_EQ(object_enters_.size(), 1);
    ASSERT_EQ(container_exits_.size(), 4);
}

// Error handling tests
TEST_F(ContainerTest, UnexpectedObjectClose) {
    parse("}");

    EXPECT_TRUE(values_.empty());
    ASSERT_EQ(errors_.size(), 1);
    EXPECT_EQ(std::get<1>(errors_[0]), "Unexpected '}': no object to close");
    EXPECT_TRUE(object_enters_.empty());
    EXPECT_TRUE(container_exits_.empty());
}

TEST_F(ContainerTest, UnexpectedArrayClose) {
    parse("]");

    EXPECT_TRUE(values_.empty());
    ASSERT_EQ(errors_.size(), 1);
    EXPECT_EQ(std::get<1>(errors_[0]), "Unexpected ']': no array to close");
    EXPECT_TRUE(array_enters_.empty());
    EXPECT_TRUE(container_exits_.empty());
}

TEST_F(ContainerTest, MismatchedContainerObjectArray) {
    parse("{]");

    EXPECT_TRUE(values_.empty());
    ASSERT_GE(errors_.size(), 1);
    // The first error should be about expecting a string key (since ] is not a valid key)
    EXPECT_EQ(std::get<1>(errors_[0]), "Object key must be a string");
    ASSERT_EQ(object_enters_.size(), 1);
    EXPECT_TRUE(array_enters_.empty());
    EXPECT_TRUE(container_exits_.empty()); // Error prevents exit
}

TEST_F(ContainerTest, MismatchedContainerArrayObject) {
    parse("[}");

    EXPECT_TRUE(values_.empty());
    ASSERT_EQ(errors_.size(), 1);
    EXPECT_EQ(std::get<1>(errors_[0]), "Unexpected '}': expected ']' to close array");
    ASSERT_EQ(array_enters_.size(), 1);
    EXPECT_TRUE(object_enters_.empty());
    EXPECT_TRUE(container_exits_.empty()); // Error prevents exit
}

TEST_F(ContainerTest, UnterminatedObject) {
    parse("{");

    EXPECT_TRUE(values_.empty());
    ASSERT_EQ(errors_.size(), 1);
    EXPECT_EQ(std::get<1>(errors_[0]), "Unexpected end of input: unclosed containers");
    ASSERT_EQ(object_enters_.size(), 1);
    EXPECT_TRUE(container_exits_.empty());
}

TEST_F(ContainerTest, UnterminatedArray) {
    parse("[");

    EXPECT_TRUE(values_.empty());
    ASSERT_EQ(errors_.size(), 1);
    EXPECT_EQ(std::get<1>(errors_[0]), "Unexpected end of input: unclosed containers");
    ASSERT_EQ(array_enters_.size(), 1);
    EXPECT_TRUE(container_exits_.empty());
}

// Container with simple values
TEST_F(ContainerTest, ArrayWithValues) {
    parse("[true, false, null]");

    EXPECT_TRUE(errors_.empty());
    ASSERT_EQ(values_.size(), 3);
    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::Boolean);
    EXPECT_EQ(std::get<1>(values_[0]), "true");
    EXPECT_EQ(std::get<0>(values_[1]), jsom::JsonType::Boolean);
    EXPECT_EQ(std::get<1>(values_[1]), "false");
    EXPECT_EQ(std::get<0>(values_[2]), jsom::JsonType::Null);
    EXPECT_EQ(std::get<1>(values_[2]), "null");

    ASSERT_EQ(array_enters_.size(), 1);
    ASSERT_EQ(container_exits_.size(), 1);
}

TEST_F(ContainerTest, ArrayWithStringsAndNumbers) {
    parse(R"(["hello", 42, "world", -3.14])");

    // Debug: Print errors
    for (const auto& error : errors_) {
        std::cout << "Error: " << std::get<1>(error) << "\n";
    }

    EXPECT_TRUE(errors_.empty());
    ASSERT_EQ(values_.size(), 4);
    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::String);
    EXPECT_EQ(std::get<1>(values_[0]), "hello");
    EXPECT_EQ(std::get<0>(values_[1]), jsom::JsonType::Number);
    EXPECT_EQ(std::get<1>(values_[1]), "42");
    EXPECT_EQ(std::get<0>(values_[2]), jsom::JsonType::String);
    EXPECT_EQ(std::get<1>(values_[2]), "world");
    EXPECT_EQ(std::get<0>(values_[3]), jsom::JsonType::Number);
    EXPECT_EQ(std::get<1>(values_[3]), "-3.14");
}
