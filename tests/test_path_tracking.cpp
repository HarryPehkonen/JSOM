#include "jsom.hpp"
#include <gtest/gtest.h>
#include <vector>

class PathTrackingTest : public ::testing::Test {
protected:
    void SetUp() override {
        parser = std::make_unique<jsom::StreamingParser>();
        values_.clear();
        errors_.clear();

        // Set up event callbacks
        jsom::ParseEvents events;
        events.on_value = [this](const jsom::JsonEvent& value) {
            values_.emplace_back(value.type(), value.raw_value(), value.path());
        };
        events.on_error = [this](const jsom::ParseError& error) {
            errors_.emplace_back(error.position, error.message, error.json_pointer);
        };

        parser->set_events(events);
    }

    void parse(const std::string& json) {
        parser->parse_string(json);
        parser->end_input();
    }

    std::unique_ptr<jsom::StreamingParser> parser;
    std::vector<std::tuple<jsom::JsonType, std::string, std::string>> values_;
    std::vector<std::tuple<std::size_t, std::string, std::string>> errors_;
};

// Basic path tests
TEST_F(PathTrackingTest, SimpleValue) {
    parse("42");

    EXPECT_TRUE(errors_.empty());
    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::Number);
    EXPECT_EQ(std::get<1>(values_[0]), "42");
    EXPECT_EQ(std::get<2>(values_[0]), ""); // Root level has empty path
}

TEST_F(PathTrackingTest, SimpleObjectValue) {
    parse(R"({"foo": "bar"})");

    EXPECT_TRUE(errors_.empty());
    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::String);
    EXPECT_EQ(std::get<1>(values_[0]), "bar");
    EXPECT_EQ(std::get<2>(values_[0]), "/foo");
}

TEST_F(PathTrackingTest, SimpleArrayValues) {
    parse(R"([1, 2, 3])");

    EXPECT_TRUE(errors_.empty());
    ASSERT_EQ(values_.size(), 3);

    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::Number);
    EXPECT_EQ(std::get<1>(values_[0]), "1");
    EXPECT_EQ(std::get<2>(values_[0]), "/0");

    EXPECT_EQ(std::get<0>(values_[1]), jsom::JsonType::Number);
    EXPECT_EQ(std::get<1>(values_[1]), "2");
    EXPECT_EQ(std::get<2>(values_[1]), "/1");

    EXPECT_EQ(std::get<0>(values_[2]), jsom::JsonType::Number);
    EXPECT_EQ(std::get<1>(values_[2]), "3");
    EXPECT_EQ(std::get<2>(values_[2]), "/2");
}

TEST_F(PathTrackingTest, NestedObjectAndArray) {
    parse(R"({"data": [{"id": 1}, {"id": 2}]})");

    EXPECT_TRUE(errors_.empty());
    ASSERT_EQ(values_.size(), 2);

    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::Number);
    EXPECT_EQ(std::get<1>(values_[0]), "1");
    EXPECT_EQ(std::get<2>(values_[0]), "/data/0/id");

    EXPECT_EQ(std::get<0>(values_[1]), jsom::JsonType::Number);
    EXPECT_EQ(std::get<1>(values_[1]), "2");
    EXPECT_EQ(std::get<2>(values_[1]), "/data/1/id");
}

// Path escaping tests
TEST_F(PathTrackingTest, EscapingSpecialCharacters) {
    parse(R"({"a~b": "value1", "c/d": "value2"})");

    EXPECT_TRUE(errors_.empty());
    ASSERT_EQ(values_.size(), 2);

    EXPECT_EQ(std::get<1>(values_[0]), "value1");
    EXPECT_EQ(std::get<2>(values_[0]), "/a~0b"); // ~ escaped to ~0

    EXPECT_EQ(std::get<1>(values_[1]), "value2");
    EXPECT_EQ(std::get<2>(values_[1]), "/c~1d"); // / escaped to ~1
}

TEST_F(PathTrackingTest, EscapingBothCharacters) {
    parse(R"({"a~/b": "value"})");

    EXPECT_TRUE(errors_.empty());
    ASSERT_EQ(values_.size(), 1);

    EXPECT_EQ(std::get<1>(values_[0]), "value");
    EXPECT_EQ(std::get<2>(values_[0]), "/a~0~1b"); // ~/ escaped to ~0~1
}

// Error path tracking
TEST_F(PathTrackingTest, ErrorWithPath) {
    parse(R"({"key": invalid})");

    ASSERT_GE(errors_.size(), 1);
    // The first error should include a path context
    EXPECT_FALSE(std::get<2>(errors_[0]).empty());
    EXPECT_EQ(std::get<2>(errors_[0]), "/key");
}