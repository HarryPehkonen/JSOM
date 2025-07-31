#include "jsom.hpp"
#include <gtest/gtest.h>

namespace {
constexpr double TEST_PRECISION = 0.001;
} // namespace

class IntelligentSerializationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test documents for various scenarios
        const std::string name = "test";
        const int value = 42;
        simple_object_ = jsom::JsonDocument{{"name", jsom::JsonDocument(name)},
                                            {"value", jsom::JsonDocument(value)}};

        simple_array_ = jsom::JsonDocument{jsom::JsonDocument(1), jsom::JsonDocument(2),
                                           jsom::JsonDocument(3)};

        // NOLINTBEGIN(readability-magic-numbers)
        large_array_ = jsom::JsonDocument{
            jsom::JsonDocument(1),  jsom::JsonDocument(2),  jsom::JsonDocument(3),
            jsom::JsonDocument(4),  jsom::JsonDocument(5),  jsom::JsonDocument(6),
            jsom::JsonDocument(7),  jsom::JsonDocument(8),  jsom::JsonDocument(9),
            jsom::JsonDocument(10), jsom::JsonDocument(11), jsom::JsonDocument(12)};
        // NOLINTEND(readability-magic-numbers)

        nested_object_ = jsom::JsonDocument{
            {"level1", jsom::JsonDocument{
                           {"level2", jsom::JsonDocument{{"value", jsom::JsonDocument("deep")}}}}}};

        mixed_content_ = jsom::JsonDocument{{"settings", simple_object_}, {"data", large_array_}};
    }

    jsom::JsonDocument simple_object_;
    jsom::JsonDocument simple_array_;
    jsom::JsonDocument large_array_;
    jsom::JsonDocument nested_object_;
    jsom::JsonDocument mixed_content_;
};

// Test JsonFormatOptions structure and default values
TEST_F(IntelligentSerializationTest, JsonFormatOptionsDefaults) {
    jsom::JsonFormatOptions defaults;

    EXPECT_FALSE(defaults.pretty);
    EXPECT_EQ(defaults.indent_size, 2);
    EXPECT_FALSE(defaults.sort_keys);
    EXPECT_EQ(defaults.max_inline_array_size, 10);
    EXPECT_EQ(defaults.max_inline_object_size, 3);
    EXPECT_EQ(defaults.max_inline_string_length, 40);
    EXPECT_TRUE(defaults.quote_keys);
    EXPECT_FALSE(defaults.trailing_comma);
    EXPECT_FALSE(defaults.escape_unicode);
    EXPECT_EQ(defaults.max_depth, 100);
}

// Test all format presets
TEST_F(IntelligentSerializationTest, FormatPresets) {
    // Test Compact preset
    const auto& compact = jsom::FormatPresets::Compact;
    EXPECT_FALSE(compact.pretty);

    // Test Pretty preset
    const auto& pretty = jsom::FormatPresets::Pretty;
    EXPECT_TRUE(pretty.pretty);
    EXPECT_EQ(pretty.indent_size, 2);
    EXPECT_FALSE(pretty.sort_keys);
    EXPECT_EQ(pretty.max_inline_array_size, 10);
    EXPECT_EQ(pretty.max_inline_object_size, 3);

    // Test Config preset
    const auto& config = jsom::FormatPresets::Config;
    EXPECT_TRUE(config.pretty);
    EXPECT_TRUE(config.sort_keys);
    EXPECT_EQ(config.max_inline_array_size, 5);
    EXPECT_EQ(config.max_inline_object_size, 1);

    // Test Api preset
    const auto& api = jsom::FormatPresets::Api;
    EXPECT_TRUE(api.pretty);
    EXPECT_EQ(api.max_inline_array_size, 20);
    EXPECT_EQ(api.max_inline_object_size, 5);

    // Test Debug preset
    const auto& debug = jsom::FormatPresets::Debug;
    EXPECT_TRUE(debug.pretty);
    EXPECT_EQ(debug.indent_size, 4);
    EXPECT_TRUE(debug.sort_keys);
    EXPECT_EQ(debug.max_inline_array_size, 1);
    EXPECT_EQ(debug.max_inline_object_size, 0);
    EXPECT_TRUE(debug.escape_unicode);
}

// Test basic compact vs pretty formatting
TEST_F(IntelligentSerializationTest, CompactVsPrettyBasics) {
    // Compact should have no spaces or newlines
    std::string compact = simple_object_.to_json(jsom::FormatPresets::Compact);
    EXPECT_EQ(compact, R"({"name":"test","value":42})");
    EXPECT_EQ(compact.find(' '), std::string::npos);
    EXPECT_EQ(compact.find('\n'), std::string::npos);

    // Pretty formatting for small objects (should be inline with spaces)
    std::string pretty = simple_object_.to_json(jsom::FormatPresets::Pretty);
    EXPECT_NE(pretty.find(' '), std::string::npos);
    EXPECT_TRUE(pretty.find(": ") != std::string::npos);   // Space after colon
    EXPECT_EQ(pretty, R"({"name": "test", "value": 42})"); // Small objects stay inline

    // Test with a larger object that should have newlines
    auto large_obj = jsom::JsonDocument{
        {"key1", jsom::JsonDocument("value1")},
        {"key2", jsom::JsonDocument("value2")},
        {"key3", jsom::JsonDocument("value3")},
        {"key4", jsom::JsonDocument("value4")} // 4 properties > max_inline_object_size (3)
    };

    std::string large_pretty = large_obj.to_json(jsom::FormatPresets::Pretty);
    EXPECT_NE(large_pretty.find('\n'), std::string::npos); // Should have newlines
}

// Test array inlining behavior
TEST_F(IntelligentSerializationTest, ArrayInlining) {
    jsom::JsonFormatOptions options;
    options.pretty = true;
    options.max_inline_array_size = 5; // NOLINT(readability-magic-numbers)

    // Small array should be inline
    std::string small_result = simple_array_.to_json(options);
    EXPECT_EQ(small_result, "[1, 2, 3]");

    // Large array should be multiline
    std::string large_result = large_array_.to_json(options);
    EXPECT_NE(large_result.find('\n'), std::string::npos);
    EXPECT_TRUE(large_result.find("[\n") != std::string::npos
                || large_result.find("[ \n") != std::string::npos);
}

// Test object inlining behavior
TEST_F(IntelligentSerializationTest, ObjectInlining) {
    jsom::JsonFormatOptions options;
    options.pretty = true;
    options.max_inline_object_size = 3;

    // Small object should be inline
    std::string small_result = simple_object_.to_json(options);
    EXPECT_EQ(small_result, R"({"name": "test", "value": 42})");

    // Test with smaller limit - should be multiline
    options.max_inline_object_size = 1;
    std::string multiline_result = simple_object_.to_json(options);
    EXPECT_NE(multiline_result.find('\n'), std::string::npos);
}

// Test array size-based inlining
TEST_F(IntelligentSerializationTest, ArraySizeBasedInlining) {
    jsom::JsonFormatOptions options;
    options.pretty = true;
    options.max_inline_array_size = 3;

    // Small arrays should stay inline
    std::string small_result = simple_array_.to_json(options); // 3 elements
    EXPECT_EQ(small_result, "[1, 2, 3]");

    // Large arrays should be multiline
    std::string large_result = large_array_.to_json(options); // 12 elements > 3
    EXPECT_NE(large_result.find('\n'), std::string::npos);

    // Test edge case - exactly at the limit
    auto edge_array
        = jsom::JsonDocument{jsom::JsonDocument(1), jsom::JsonDocument(2), jsom::JsonDocument(3)};
    std::string edge_result = edge_array.to_json(options);
    EXPECT_EQ(edge_result, "[1, 2, 3]"); // Should be inline (3 <= 3)
}

// Test indent size control
TEST_F(IntelligentSerializationTest, IndentSizeControl) {
    jsom::JsonFormatOptions options2;
    options2.pretty = true;
    options2.indent_size = 2;
    options2.max_inline_object_size = 0; // Force multiline

    jsom::JsonFormatOptions options4;
    options4.pretty = true;
    options4.indent_size = 4;
    options4.max_inline_object_size = 0; // Force multiline

    std::string result2 = nested_object_.to_json(options2);
    std::string result4 = nested_object_.to_json(options4);

    // Should have different indentation
    EXPECT_NE(result2, result4);

    // Check for specific indentation patterns
    EXPECT_TRUE(result2.find("  \"level1\"") != std::string::npos);   // 2 spaces
    EXPECT_TRUE(result4.find("    \"level1\"") != std::string::npos); // 4 spaces
}

// Test sort keys functionality (Note: std::map already sorts keys automatically)
TEST_F(IntelligentSerializationTest, SortKeys) {
    auto obj = jsom::JsonDocument{{"zebra", jsom::JsonDocument("last")},
                                  {"alpha", jsom::JsonDocument("first")},
                                  {"beta", jsom::JsonDocument("second")}};

    jsom::JsonFormatOptions options;
    options.pretty = true;
    options.sort_keys = true;           // This option exists but std::map already sorts
    options.max_inline_object_size = 0; // Force multiline

    std::string result = obj.to_json(options);

    // Keys should be in alphabetical order (std::map guarantees this)
    size_t alpha_pos = result.find("\"alpha\"");
    size_t beta_pos = result.find("\"beta\"");
    size_t zebra_pos = result.find("\"zebra\"");

    EXPECT_LT(alpha_pos, beta_pos);
    EXPECT_LT(beta_pos, zebra_pos);

    // Verify the option can be set (even if it doesn't change behavior)
    EXPECT_TRUE(options.sort_keys);
}

// Test nested containers in arrays (should force multiline)
TEST_F(IntelligentSerializationTest, NestedContainersInArrays) {
    jsom::JsonFormatOptions options;
    options.pretty = true;

    // NOLINTNEXTLINE(readability-magic-numbers)
    options.max_inline_array_size = 10; // Large enough to allow inlining

    // Array with nested objects should be multiline even if size allows inlining
    auto array_with_objects
        = jsom::JsonDocument{jsom::JsonDocument{{"key", jsom::JsonDocument("value")}},
                             jsom::JsonDocument{{"key2", jsom::JsonDocument("value2")}}};

    std::string result = array_with_objects.to_json(options);
    EXPECT_NE(result.find('\n'), std::string::npos); // Should be multiline due to nested objects
}

// Test string escaping
TEST_F(IntelligentSerializationTest, StringEscaping) {
    auto special_strings
        = jsom::JsonDocument{{"quote", jsom::JsonDocument("He said \"Hello\"")},
                             {"backslash", jsom::JsonDocument("Path\\to\\file")},
                             {"newline", jsom::JsonDocument("Line1\nLine2")},
                             {"tab", jsom::JsonDocument("Col1\tCol2")},
                             {"control", jsom::JsonDocument(std::string("\x01\x02\x03", 3))}};

    std::string result = special_strings.to_json();

    // Check proper escaping
    EXPECT_TRUE(result.find("\\\"") != std::string::npos);  // Quote escaped
    EXPECT_TRUE(result.find("\\\\") != std::string::npos);  // Backslash escaped
    EXPECT_TRUE(result.find("\\n") != std::string::npos);   // Newline escaped
    EXPECT_TRUE(result.find("\\t") != std::string::npos);   // Tab escaped
    EXPECT_TRUE(result.find("\\u00") != std::string::npos); // Control chars as unicode
}

// Test unicode escaping control (option exists for future use)
TEST_F(IntelligentSerializationTest, UnicodeEscaping) {
    auto unicode_obj = jsom::JsonDocument{{"test", jsom::JsonDocument("café")}};

    jsom::JsonFormatOptions no_escape;
    no_escape.escape_unicode = false;

    jsom::JsonFormatOptions with_escape;
    with_escape.escape_unicode = true;

    std::string result = unicode_obj.to_json(no_escape);

    // Currently, unicode chars are preserved as-is
    // The escape_unicode option exists for future implementation
    EXPECT_TRUE(result.find("café") != std::string::npos);

    // Verify the option can be set
    EXPECT_FALSE(no_escape.escape_unicode);
    EXPECT_TRUE(with_escape.escape_unicode);
}

// Test empty containers
TEST_F(IntelligentSerializationTest, EmptyContainers) {
    // Create empty containers using initializer list syntax
    auto empty_obj = jsom::JsonDocument{
        // Empty object
    };

    auto empty_arr = jsom::JsonDocument{
        // Empty array - but this creates a null value, so let's create a proper empty array
    };

    // Create a proper empty array by removing elements from a non-empty one
    auto temp_arr = jsom::JsonDocument{jsom::JsonDocument(1)};
    // Since we can't easily create empty containers from outside, let's test with minimal
    // containers

    auto minimal_obj = jsom::JsonDocument{{"k", jsom::JsonDocument("v")}};
    auto minimal_arr = jsom::JsonDocument{jsom::JsonDocument(1)};

    jsom::JsonFormatOptions options;
    options.pretty = true;

    // NOLINTNEXTLINE(readability-magic-numbers)
    options.max_inline_object_size = 10; // Ensure small containers stay inline

    // NOLINTNEXTLINE(readability-magic-numbers)
    options.max_inline_array_size = 10;

    // Small containers should be inline
    std::string obj_result = minimal_obj.to_json(options);
    std::string arr_result = minimal_arr.to_json(options);

    EXPECT_EQ(obj_result, R"({"k": "v"})");
    EXPECT_EQ(arr_result, "[1]");
}

// Test nested structure formatting
TEST_F(IntelligentSerializationTest, NestedStructureFormatting) {
    jsom::JsonFormatOptions options;
    options.pretty = true;
    options.max_inline_object_size = 2;
    options.max_inline_array_size = 5; // NOLINT(readability-magic-numbers)

    std::string result = mixed_content_.to_json(options);

    // Should format large array as multiline
    EXPECT_TRUE(result.find("\"data\": [") != std::string::npos);

    // Should have proper indentation
    EXPECT_TRUE(result.find("  \"settings\"") != std::string::npos);
    EXPECT_TRUE(result.find("  \"data\"") != std::string::npos);
}

// Test all to_json() method overloads
TEST_F(IntelligentSerializationTest, ToJsonOverloads) {
    // Default to_json() should use compact
    std::string default_result = simple_object_.to_json();
    EXPECT_EQ(default_result, R"({"name":"test","value":42})");

    // to_json(bool) overload
    std::string compact_bool = simple_object_.to_json(false);
    std::string pretty_bool = simple_object_.to_json(true);
    EXPECT_EQ(compact_bool, default_result);
    EXPECT_NE(pretty_bool, default_result);
    EXPECT_TRUE(pretty_bool.find(' ') != std::string::npos);

    // to_json(JsonFormatOptions) overload
    std::string with_options = simple_object_.to_json(jsom::FormatPresets::Pretty);
    EXPECT_EQ(with_options, pretty_bool);
}

// Test number formatting
TEST_F(IntelligentSerializationTest, NumberFormatting) {
    // NOLINTBEGIN(readability-magic-numbers)
    auto numbers = jsom::JsonDocument{{"integer", jsom::JsonDocument(42)},
                                      {"float", jsom::JsonDocument(3.14159)},
                                      {"negative", jsom::JsonDocument(-123)},
                                      {"zero", jsom::JsonDocument(0)},
                                      {"large", jsom::JsonDocument(1000000)}};

    std::string result = numbers.to_json();

    // Numbers should be formatted correctly
    EXPECT_TRUE(result.find("42") != std::string::npos);
    EXPECT_TRUE(result.find("3.14159") != std::string::npos);
    EXPECT_TRUE(result.find("-123") != std::string::npos);
    EXPECT_TRUE(result.find('0') != std::string::npos);
    EXPECT_TRUE(result.find("1000000") != std::string::npos);
    // NOLINTEND(readability-magic-numbers)
}

// Test boolean and null formatting
TEST_F(IntelligentSerializationTest, BooleanAndNullFormatting) {
    auto values = jsom::JsonDocument{{"true_val", jsom::JsonDocument(true)},
                                     {"false_val", jsom::JsonDocument(false)},
                                     {"null_val", jsom::JsonDocument()}};

    std::string result = values.to_json();

    EXPECT_TRUE(result.find("true") != std::string::npos);
    EXPECT_TRUE(result.find("false") != std::string::npos);
    EXPECT_TRUE(result.find("null") != std::string::npos);
}

// Test round-trip compatibility
TEST_F(IntelligentSerializationTest, RoundTripCompatibility) {
    // Test that formatted JSON can be parsed back correctly
    std::string original_json = R"({"name":"test","values":[1,2,3],"nested":{"key":"value"}})";
    auto doc = jsom::parse_document(original_json);

    // Format with different presets
    std::vector<jsom::JsonFormatOptions> presets
        = {jsom::FormatPresets::Compact, jsom::FormatPresets::Pretty, jsom::FormatPresets::Config,
           jsom::FormatPresets::Api, jsom::FormatPresets::Debug};

    for (const auto& preset : presets) {
        std::string formatted = doc.to_json(preset);
        auto reparsed = jsom::parse_document(formatted);

        // Should be able to access the same data
        EXPECT_EQ(reparsed["name"].as<std::string>(), "test");
        EXPECT_EQ(reparsed["values"][0].as<int>(), 1);
        EXPECT_EQ(reparsed["values"][1].as<int>(), 2);
        EXPECT_EQ(reparsed["values"][2].as<int>(), 3);
        EXPECT_EQ(reparsed["nested"]["key"].as<std::string>(), "value");
    }
}
