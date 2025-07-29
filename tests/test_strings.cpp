#include "jsom.hpp"
#include <gtest/gtest.h>
#include <vector>

class StringTest : public ::testing::Test {
protected:
    void SetUp() override {
        parser = std::make_unique<jsom::StreamingParser>();
        values_.clear();
        errors_.clear();

        // Set up event callbacks
        jsom::ParseEvents events;
        events.on_value = [this](const jsom::JsonValue& value) {
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

// Basic string parsing tests
TEST_F(StringTest, ParseEmptyString) {
    parse("\"\"");

    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::String);
    EXPECT_EQ(std::get<1>(values_[0]), "");
    EXPECT_TRUE(errors_.empty());
}

TEST_F(StringTest, ParseSimpleString) {
    parse("\"hello\"");

    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::String);
    EXPECT_EQ(std::get<1>(values_[0]), "hello");
    EXPECT_TRUE(errors_.empty());
}

TEST_F(StringTest, ParseStringWithSpaces) {
    parse("\"hello world\"");

    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::String);
    EXPECT_EQ(std::get<1>(values_[0]), "hello world");
    EXPECT_TRUE(errors_.empty());
}

// Standard escape sequence tests
TEST_F(StringTest, ParseStringWithQuoteEscape) {
    parse(R"("hello \"world\"")");

    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::String);
    EXPECT_EQ(std::get<1>(values_[0]), "hello \"world\"");
    EXPECT_TRUE(errors_.empty());
}

TEST_F(StringTest, ParseStringWithBackslashEscape) {
    parse(R"("path\\to\\file")");

    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::String);
    EXPECT_EQ(std::get<1>(values_[0]), "path\\to\\file");
    EXPECT_TRUE(errors_.empty());
}

TEST_F(StringTest, ParseStringWithSlashEscape) {
    parse(R"("http:\/\/example.com")");

    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::String);
    EXPECT_EQ(std::get<1>(values_[0]), "http://example.com");
    EXPECT_TRUE(errors_.empty());
}

TEST_F(StringTest, ParseStringWithControlEscapes) {
    parse(R"("line1\nline2\tindented\r\nwindows")");

    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::String);
    EXPECT_EQ(std::get<1>(values_[0]), "line1\nline2\tindented\r\nwindows");
    EXPECT_TRUE(errors_.empty());
}

TEST_F(StringTest, ParseStringWithBackspaceFormfeed) {
    parse(R"("text\bwith\fcontrol")");

    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::String);
    EXPECT_EQ(std::get<1>(values_[0]), "text\bwith\fcontrol");
    EXPECT_TRUE(errors_.empty());
}

// Error handling tests
TEST_F(StringTest, UnterminatedString) {
    parse("\"unterminated");

    EXPECT_TRUE(values_.empty());
    ASSERT_EQ(errors_.size(), 1);
    EXPECT_EQ(std::get<1>(errors_[0]), "Unterminated string at end of input");
}

TEST_F(StringTest, InvalidEscapeSequence) {
    parse(R"("invalid\xescape")");

    EXPECT_TRUE(values_.empty());
    ASSERT_EQ(errors_.size(), 1);
    EXPECT_EQ(std::get<1>(errors_[0]), "Invalid escape sequence");
}

TEST_F(StringTest, UnescapedControlCharacter) {
    std::string json = "\"control\x01char\""; // Control character (ASCII 1)
    parse(json);

    EXPECT_TRUE(values_.empty());
    ASSERT_EQ(errors_.size(), 1);
    EXPECT_EQ(std::get<1>(errors_[0]), "Unescaped control character in string");
}

// Unicode escape tests
TEST_F(StringTest, UnicodeEscapeBasic) {
    parse(R"("\u0041")"); // 'A'

    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::String);
    EXPECT_EQ(std::get<1>(values_[0]), "A");
    EXPECT_TRUE(errors_.empty());
}

TEST_F(StringTest, UnicodeEscapeCheckmark) {
    parse(R"("\u2713")"); // ✓

    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::String);
    EXPECT_EQ(std::get<1>(values_[0]), "✓");
    EXPECT_TRUE(errors_.empty());
}

TEST_F(StringTest, UnicodeEscapeWithText) {
    parse(R"("Hello \u0041 World")"); // "Hello A World"

    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::String);
    EXPECT_EQ(std::get<1>(values_[0]), "Hello A World");
    EXPECT_TRUE(errors_.empty());
}

TEST_F(StringTest, InvalidUnicodeEscapeNonHex) {
    parse(R"("\u00GZ")");

    EXPECT_TRUE(values_.empty());
    ASSERT_EQ(errors_.size(), 1);
    EXPECT_EQ(std::get<1>(errors_[0]), "Invalid Unicode escape sequence: expected hex digit");
}

TEST_F(StringTest, IncompleteUnicodeEscape) {
    parse("\"\\u004"); // Only 3 hex digits, unterminated string

    EXPECT_TRUE(values_.empty());
    ASSERT_EQ(errors_.size(), 1);
    EXPECT_EQ(std::get<1>(errors_[0]), "Incomplete Unicode escape sequence at end of input");
}

TEST_F(StringTest, UnicodeEscapeInvalidHexInMiddle) {
    parse(R"("\u00G4")"); // Non-hex digit in 3rd position

    EXPECT_TRUE(values_.empty());
    ASSERT_EQ(errors_.size(), 1);
    EXPECT_EQ(std::get<1>(errors_[0]), "Invalid Unicode escape sequence: expected hex digit");
}

// Multiple strings test
TEST_F(StringTest, MultipleStrings) {
    parse(R"("first" "second" "third")");

    ASSERT_EQ(values_.size(), 3);

    EXPECT_EQ(std::get<0>(values_[0]), jsom::JsonType::String);
    EXPECT_EQ(std::get<1>(values_[0]), "first");

    EXPECT_EQ(std::get<0>(values_[1]), jsom::JsonType::String);
    EXPECT_EQ(std::get<1>(values_[1]), "second");

    EXPECT_EQ(std::get<0>(values_[2]), jsom::JsonType::String);
    EXPECT_EQ(std::get<1>(values_[2]), "third");

    EXPECT_TRUE(errors_.empty());
}