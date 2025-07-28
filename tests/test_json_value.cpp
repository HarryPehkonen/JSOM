#include <gtest/gtest.h>
#include "jsom.hpp"

namespace jsom {

class JsonValueTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(JsonValueTest, DefaultConstruction) {
    JsonValue value;
    EXPECT_EQ(value.raw(), "");
    EXPECT_EQ(value.path(), nullptr);
}

TEST_F(JsonValueTest, ConstructionWithRawData) {
    JsonValue value("\"hello world\"");
    EXPECT_EQ(value.raw(), "\"hello world\"");
    EXPECT_EQ(value.type(), JsonValue::String);
}

TEST_F(JsonValueTest, BooleanValues) {
    JsonValue true_value("true");
    JsonValue false_value("false");
    
    EXPECT_EQ(true_value.type(), JsonValue::Bool);
    EXPECT_EQ(false_value.type(), JsonValue::Bool);
    EXPECT_TRUE(true_value.get_bool());
    EXPECT_FALSE(false_value.get_bool());
}

TEST_F(JsonValueTest, NumberValues) {
    JsonValue int_value("42");
    JsonValue float_value("3.14159");
    JsonValue negative_value("-17.5");
    
    EXPECT_EQ(int_value.type(), JsonValue::Number);
    EXPECT_EQ(float_value.type(), JsonValue::Number);
    EXPECT_EQ(negative_value.type(), JsonValue::Number);
    
    EXPECT_DOUBLE_EQ(int_value.get_number(), 42.0);
    EXPECT_NEAR(float_value.get_number(), 3.14159, 1e-5);
    EXPECT_DOUBLE_EQ(negative_value.get_number(), -17.5);
}

TEST_F(JsonValueTest, StringValues) {
    JsonValue simple_string("\"hello\"");
    JsonValue escaped_string("\"hello\\nworld\\t!\"");
    
    EXPECT_EQ(simple_string.type(), JsonValue::String);
    EXPECT_EQ(escaped_string.type(), JsonValue::String);
    
    EXPECT_EQ(simple_string.get_string(), "hello");
    EXPECT_EQ(escaped_string.get_string(), "hello\nworld\t!");
}

TEST_F(JsonValueTest, StringEscapeSequences) {
    JsonValue quotes("\"He said \\\"Hello\\\"\"");
    JsonValue backslash("\"path\\\\to\\\\file\"");
    
    EXPECT_EQ(quotes.get_string(), "He said \"Hello\"");
    EXPECT_EQ(backslash.get_string(), "path\\to\\file");
}

TEST_F(JsonValueTest, NullValue) {
    JsonValue null_value("null");
    EXPECT_EQ(null_value.type(), JsonValue::Null);
    EXPECT_TRUE(null_value.is_null());
}

TEST_F(JsonValueTest, ObjectAndArrayDetection) {
    JsonValue object_value("{}");
    JsonValue array_value("[]");
    
    EXPECT_EQ(object_value.type(), JsonValue::Object);
    EXPECT_EQ(array_value.type(), JsonValue::Array);
    EXPECT_TRUE(object_value.is_object());
    EXPECT_TRUE(array_value.is_array());
}

TEST_F(JsonValueTest, JsonReconstruction) {
    JsonValue string_value("\"test\"");
    JsonValue number_value("42");
    JsonValue bool_value("true");
    JsonValue null_value("null");
    
    EXPECT_EQ(string_value.to_json(), "\"test\"");
    EXPECT_EQ(number_value.to_json(), "42.000000");  // std::to_string format
    EXPECT_EQ(bool_value.to_json(), "true");
    EXPECT_EQ(null_value.to_json(), "null");
}

TEST_F(JsonValueTest, JsonPointerIntegration) {
    PathNode root;
    PathNode* child = root.add_child(PathNode::ObjectKey, "test");
    
    JsonValue value("\"data\"", child);
    EXPECT_EQ(value.get_json_pointer(), "/test");
}

TEST_F(JsonValueTest, WhitespaceHandling) {
    JsonValue padded_number("  42  ");
    JsonValue padded_bool("  true  ");
    
    EXPECT_EQ(padded_number.type(), JsonValue::Number);
    EXPECT_EQ(padded_bool.type(), JsonValue::Bool);
    EXPECT_DOUBLE_EQ(padded_number.get_number(), 42.0);
    EXPECT_TRUE(padded_bool.get_bool());
}

TEST_F(JsonValueTest, TrimWhitespaceIndirect) {
    // Test various whitespace scenarios to verify trim_whitespace works correctly
    JsonValue tab_spaces("\t  true  \n");
    JsonValue mixed_whitespace("\r\n  false \t\r");
    JsonValue only_whitespace("   \t\n\r   ");
    JsonValue empty_value("");
    JsonValue leading_only("   42");
    JsonValue trailing_only("3.14   ");
    
    EXPECT_EQ(tab_spaces.type(), JsonValue::Bool);
    EXPECT_TRUE(tab_spaces.get_bool());
    
    EXPECT_EQ(mixed_whitespace.type(), JsonValue::Bool);
    EXPECT_FALSE(mixed_whitespace.get_bool());
    
    EXPECT_EQ(only_whitespace.type(), JsonValue::Null);
    EXPECT_EQ(empty_value.type(), JsonValue::Null);
    
    EXPECT_EQ(leading_only.type(), JsonValue::Number);
    EXPECT_DOUBLE_EQ(leading_only.get_number(), 42.0);
    
    EXPECT_EQ(trailing_only.type(), JsonValue::Number);
    EXPECT_NEAR(trailing_only.get_number(), 3.14, 1e-6);
}

TEST_F(JsonValueTest, UnicodeEscapeHandling) {
    // Test unicode escape sequences (converted to UTF-8)
    JsonValue unicode_basic("\"\\u0041\"");  // Should convert to 'A'
    JsonValue unicode_lowercase("\"\\u00e9\"");  // Should convert to 'é'
    JsonValue unicode_uppercase("\"\\u00E9\"");  // Should convert to 'é'
    JsonValue unicode_complex("\"Hello \\u0041\\u00e9\"");  // Multiple escapes
    
    EXPECT_EQ(unicode_basic.type(), JsonValue::String);
    EXPECT_EQ(unicode_basic.get_string(), "A");  // Converted to UTF-8
    
    EXPECT_EQ(unicode_lowercase.type(), JsonValue::String);
    EXPECT_EQ(unicode_lowercase.get_string(), "é");
    
    EXPECT_EQ(unicode_uppercase.type(), JsonValue::String);
    EXPECT_EQ(unicode_uppercase.get_string(), "é");
    
    EXPECT_EQ(unicode_complex.type(), JsonValue::String);
    EXPECT_EQ(unicode_complex.get_string(), "Hello Aé");
}

TEST_F(JsonValueTest, InvalidUnicodeEscapeHandling) {
    // Test malformed unicode escape sequences
    JsonValue incomplete_unicode("\"\\u004\"");      // Too short
    JsonValue incomplete_end("\"\\u\"");            // Just \u
    JsonValue invalid_hex("\"\\u00GH\"");           // Invalid hex chars
    JsonValue mixed_case("\"\\u00Aa\"");            // Mixed case (valid)
    
    EXPECT_EQ(incomplete_unicode.type(), JsonValue::String);
    EXPECT_EQ(incomplete_unicode.get_string(), "\\u004");  // Preserved with trailing chars
    
    EXPECT_EQ(incomplete_end.type(), JsonValue::String);
    EXPECT_EQ(incomplete_end.get_string(), "\\u");  // Just the incomplete escape
    
    EXPECT_EQ(invalid_hex.type(), JsonValue::String);
    EXPECT_EQ(invalid_hex.get_string(), "\\u00GH");  // Preserved with invalid chars
    
    EXPECT_EQ(mixed_case.type(), JsonValue::String);
    EXPECT_EQ(mixed_case.get_string(), "\xC2\xAA");  // Valid hex sequence converted to UTF-8
}

} // namespace jsom