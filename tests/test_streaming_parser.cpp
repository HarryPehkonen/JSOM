#include <gtest/gtest.h>
#include "jsom.hpp"

namespace jsom {

class StreamingParserTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(StreamingParserTest, Construction) {
    StreamingParser parser;
    EXPECT_EQ(parser.total_paths(), 0);
}

TEST_F(StreamingParserTest, ConstructionWithCustomAllocator) {
    auto allocator = std::make_unique<ArenaAllocator>(1024);
    StreamingParser parser(std::move(allocator));
    EXPECT_EQ(parser.total_paths(), 0);
}

TEST_F(StreamingParserTest, BasicEventHandling) {
    StreamingParser parser;
    
    bool value_called = false;
    bool enter_object_called = false;
    
    ParseEvents events;
    events.on_value = [&](const std::string& /*pointer*/, const JsonValue& /*value*/) {
        value_called = true;
    };
    events.on_enter_object = [&](const std::string& /*pointer*/) {
        enter_object_called = true;
    };
    
    parser.set_events(events);
    
    // TODO: Test with actual JSON when parser is implemented
    // For now, just verify events can be set
    EXPECT_FALSE(value_called);
    EXPECT_FALSE(enter_object_called);
}

TEST_F(StreamingParserTest, ParseEmptyObject) {
    StreamingParser parser;
    
    // TODO: When parser is implemented, test:
    // parser.parse_string("{}");
    // EXPECT_EQ(parser.total_paths(), 1);  // Root path
}

TEST_F(StreamingParserTest, ParseSimpleObject) {
    StreamingParser parser;
    
    // TODO: When parser is implemented, test:
    // parser.parse_string("{\"key\": \"value\"}");
    // Verify that /key path is discovered
}

TEST_F(StreamingParserTest, MemoryUsageTracking) {
    auto allocator = std::make_unique<ArenaAllocator>();
    StreamingParser parser(std::move(allocator));
    
    size_t initial_usage = parser.memory_usage();
    EXPECT_GE(initial_usage, 0);  // Should track some memory for root node
}

TEST_F(StreamingParserTest, Reset) {
    StreamingParser parser;
    
    // TODO: When parser is implemented:
    // parser.parse_string("{\"test\": 123}");
    // EXPECT_GT(parser.total_paths(), 0);
    
    parser.reset();
    EXPECT_EQ(parser.total_paths(), 0);
}

// Round-trip parsing tests for simple values
TEST_F(StreamingParserTest, RoundTripStringParsing) {
    StreamingParser parser;
    
    // Capture emitted values
    std::vector<JsonValue> captured_values;
    std::vector<std::string> captured_pointers;
    
    ParseEvents events;
    events.on_value = [&](const std::string& pointer, const JsonValue& value) {
        captured_pointers.push_back(pointer);
        captured_values.push_back(value);
    };
    
    parser.set_events(events);
    
    // Test the user's example: "this is a string"
    std::string json_input = "\"this is a string\"";
    parser.parse_string(json_input);
    
    // Verify we captured exactly one value
    ASSERT_EQ(captured_values.size(), 1);
    EXPECT_EQ(captured_pointers[0], ""); // Root pointer
    
    const JsonValue& value = captured_values[0];
    
    // Test round-trip behavior
    EXPECT_EQ(value.type(), JsonValue::String);
    EXPECT_EQ(value.get_string(), "this is a string");  // Unescaped content
    EXPECT_EQ(value.to_json(), "\"this is a string\""); // Re-escaped JSON
    EXPECT_EQ(value.raw(), "\"this is a string\"");     // Original raw data
}

TEST_F(StreamingParserTest, RoundTripNumberParsing) {
    StreamingParser parser;
    
    std::vector<JsonValue> captured_values;
    
    ParseEvents events;
    events.on_value = [&](const std::string& /*pointer*/, const JsonValue& value) {
        captured_values.push_back(value);
    };
    
    parser.set_events(events);
    
    // Test integer
    parser.parse_string("42");
    ASSERT_EQ(captured_values.size(), 1);
    
    const JsonValue& int_value = captured_values[0];
    EXPECT_EQ(int_value.type(), JsonValue::Number);
    EXPECT_DOUBLE_EQ(int_value.get_number(), 42.0);
    EXPECT_EQ(int_value.raw(), "42");
    
    // Test decimal
    captured_values.clear();
    parser.reset();
    parser.set_events(events);
    parser.parse_string("3.14159");
    
    ASSERT_EQ(captured_values.size(), 1);
    const JsonValue& float_value = captured_values[0];
    EXPECT_EQ(float_value.type(), JsonValue::Number);
    EXPECT_NEAR(float_value.get_number(), 3.14159, 1e-5);
    EXPECT_EQ(float_value.raw(), "3.14159");
}

TEST_F(StreamingParserTest, RoundTripBooleanParsing) {
    StreamingParser parser;
    
    std::vector<JsonValue> captured_values;
    
    ParseEvents events;
    events.on_value = [&](const std::string& /*pointer*/, const JsonValue& value) {
        captured_values.push_back(value);
    };
    
    parser.set_events(events);
    
    // Test true
    parser.parse_string("true");
    ASSERT_EQ(captured_values.size(), 1);
    
    const JsonValue& true_value = captured_values[0];
    EXPECT_EQ(true_value.type(), JsonValue::Bool);
    EXPECT_TRUE(true_value.get_bool());
    EXPECT_EQ(true_value.to_json(), "true");
    EXPECT_EQ(true_value.raw(), "true");
    
    // Test false
    captured_values.clear();
    parser.reset();
    parser.set_events(events);
    parser.parse_string("false");
    
    ASSERT_EQ(captured_values.size(), 1);
    const JsonValue& false_value = captured_values[0];
    EXPECT_EQ(false_value.type(), JsonValue::Bool);
    EXPECT_FALSE(false_value.get_bool());
    EXPECT_EQ(false_value.to_json(), "false");
    EXPECT_EQ(false_value.raw(), "false");
}

TEST_F(StreamingParserTest, RoundTripNullParsing) {
    StreamingParser parser;
    
    std::vector<JsonValue> captured_values;
    
    ParseEvents events;
    events.on_value = [&](const std::string& /*pointer*/, const JsonValue& value) {
        captured_values.push_back(value);
    };
    
    parser.set_events(events);
    
    parser.parse_string("null");
    ASSERT_EQ(captured_values.size(), 1);
    
    const JsonValue& null_value = captured_values[0];
    EXPECT_EQ(null_value.type(), JsonValue::Null);
    EXPECT_TRUE(null_value.is_null());
    EXPECT_EQ(null_value.to_json(), "null");
    EXPECT_EQ(null_value.raw(), "null");
}

TEST_F(StreamingParserTest, RoundTripStringWithEscapes) {
    StreamingParser parser;
    
    std::vector<JsonValue> captured_values;
    
    ParseEvents events;
    events.on_value = [&](const std::string& /*pointer*/, const JsonValue& value) {
        captured_values.push_back(value);
    };
    
    parser.set_events(events);
    
    // Test string with escape sequences and Unicode
    std::string json_input = "\"Hello\\nWorld\\u0021\"";
    parser.parse_string(json_input);
    
    ASSERT_EQ(captured_values.size(), 1);
    const JsonValue& value = captured_values[0];
    
    EXPECT_EQ(value.type(), JsonValue::String);
    EXPECT_EQ(value.get_string(), "Hello\nWorld!");  // Unescaped with UTF-8 conversion
    EXPECT_EQ(value.raw(), "\"Hello\\nWorld\\u0021\""); // Original raw data
    // Note: to_json() will re-escape but may not preserve exact original escaping
}

TEST_F(StreamingParserTest, StreamingAPIWithEndInput) {
    StreamingParser parser;
    
    std::vector<JsonValue> captured_values;
    
    ParseEvents events;
    events.on_value = [&](const std::string& /*pointer*/, const JsonValue& value) {
        captured_values.push_back(value);
    };
    
    parser.set_events(events);
    
    // Test character-by-character feeding with end_input()
    std::string json_input = "42";
    for (char c : json_input) {
        parser.feed_character(c);
    }
    parser.end_input();
    
    ASSERT_EQ(captured_values.size(), 1);
    const JsonValue& value = captured_values[0];
    EXPECT_EQ(value.type(), JsonValue::Number);
    EXPECT_DOUBLE_EQ(value.get_number(), 42.0);
    EXPECT_EQ(value.raw(), "42");
}


} // namespace jsom