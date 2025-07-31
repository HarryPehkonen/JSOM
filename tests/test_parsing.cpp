#include "jsom.hpp"
#include <gtest/gtest.h>
#include <vector>

class ParseTest : public ::testing::Test {
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

// Literal parsing tests
TEST_F(ParseTest, ParseTrue) {
    parse("true");

    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::Boolean);
    EXPECT_EQ(std::get<1>(values_[0]), "true");
    EXPECT_TRUE(errors_.empty());
}

TEST_F(ParseTest, ParseFalse) {
    parse("false");

    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::Boolean);
    EXPECT_EQ(std::get<1>(values_[0]), "false");
    EXPECT_TRUE(errors_.empty());
}

TEST_F(ParseTest, ParseNull) {
    parse("null");

    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::Null);
    EXPECT_EQ(std::get<1>(values_[0]), "null");
    EXPECT_TRUE(errors_.empty());
}

TEST_F(ParseTest, ParseTrueWithWhitespace) {
    parse("  true  ");

    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::Boolean);
    EXPECT_EQ(std::get<1>(values_[0]), "true");
    EXPECT_TRUE(errors_.empty());
}

TEST_F(ParseTest, ParseFalseWithNewlines) {
    parse("\n\tfalse\r\n");

    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::Boolean);
    EXPECT_EQ(std::get<1>(values_[0]), "false");
    EXPECT_TRUE(errors_.empty());
}

// Number parsing tests
TEST_F(ParseTest, ParsePositiveInteger) {
    parse("42");

    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::Number);
    EXPECT_EQ(std::get<1>(values_[0]), "42");
    EXPECT_TRUE(errors_.empty());
}

TEST_F(ParseTest, ParseNegativeInteger) {
    parse("-123");

    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::Number);
    EXPECT_EQ(std::get<1>(values_[0]), "-123");
    EXPECT_TRUE(errors_.empty());
}

TEST_F(ParseTest, ParseZero) {
    parse("0");

    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::Number);
    EXPECT_EQ(std::get<1>(values_[0]), "0");
    EXPECT_TRUE(errors_.empty());
}

TEST_F(ParseTest, ParseFloatingPoint) {
    parse("3.14159");

    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::Number);
    EXPECT_EQ(std::get<1>(values_[0]), "3.14159");
    EXPECT_TRUE(errors_.empty());
}

TEST_F(ParseTest, ParseNegativeFloat) {
    parse("-2.5");

    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::Number);
    EXPECT_EQ(std::get<1>(values_[0]), "-2.5");
    EXPECT_TRUE(errors_.empty());
}

TEST_F(ParseTest, ParseScientificNotation) {
    parse("1.23e10");

    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::Number);
    EXPECT_EQ(std::get<1>(values_[0]), "1.23e10");
    EXPECT_TRUE(errors_.empty());
}

TEST_F(ParseTest, ParseScientificNotationNegativeExponent) {
    parse("1.5E-3");

    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::Number);
    EXPECT_EQ(std::get<1>(values_[0]), "1.5E-3");
    EXPECT_TRUE(errors_.empty());
}

// Error handling tests
TEST_F(ParseTest, InvalidLiteralTru) {
    parse("tru");

    EXPECT_TRUE(values_.empty());
    ASSERT_EQ(errors_.size(), 1);
    EXPECT_GT(std::get<0>(errors_[0]), 0); // Position should be > 0
    EXPECT_EQ(std::get<1>(errors_[0]), "Invalid literal");
}

TEST_F(ParseTest, InvalidLiteralFalsy) {
    parse("falsy");

    EXPECT_TRUE(values_.empty());
    ASSERT_EQ(errors_.size(), 1);
    EXPECT_GT(std::get<0>(errors_[0]), 0);
    EXPECT_EQ(std::get<1>(errors_[0]), "Invalid literal");
}

TEST_F(ParseTest, InvalidLiteralNul) {
    parse("nul");

    EXPECT_TRUE(values_.empty());
    ASSERT_EQ(errors_.size(), 1);
    EXPECT_GT(std::get<0>(errors_[0]), 0);
    EXPECT_EQ(std::get<1>(errors_[0]), "Invalid literal");
}

TEST_F(ParseTest, UnexpectedCharacter) {
    parse("x");

    EXPECT_TRUE(values_.empty());
    ASSERT_EQ(errors_.size(), 1);
    EXPECT_GT(std::get<0>(errors_[0]), 0);
    EXPECT_EQ(std::get<1>(errors_[0]), "Unexpected character in start state");
}

// Multiple values (separated by whitespace - containers not implemented yet)
TEST_F(ParseTest, MultipleValues) {
    parse("true false null 42");

    ASSERT_EQ(values_.size(), 4);

    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::Boolean);
    EXPECT_EQ(std::get<1>(values_[0]), "true");

    EXPECT_EQ(std::get<0>(values_[1]), jsom::JsonType::Boolean);
    EXPECT_EQ(std::get<1>(values_[1]), "false");

    EXPECT_EQ(std::get<0>(values_[2]), jsom::JsonType::Null);
    EXPECT_EQ(std::get<1>(values_[2]), "null");

    EXPECT_EQ(std::get<0>(values_[3]), jsom::JsonType::Number);
    EXPECT_EQ(std::get<1>(values_[3]), "42");

    EXPECT_TRUE(errors_.empty());
}