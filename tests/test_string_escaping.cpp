#include <gtest/gtest.h>
#include "jsom.hpp"

namespace jsom {

class StringEscapingTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// SIMPLE ESCAPE SEQUENCE TESTS
// ============================================================================

TEST_F(StringEscapingTest, HandleSimpleEscape_AllValidEscapes) {
    // Test all valid simple escape sequences
    EXPECT_EQ(JsonValue::handle_simple_escape('"'), '"');
    EXPECT_EQ(JsonValue::handle_simple_escape('\\'), '\\');
    EXPECT_EQ(JsonValue::handle_simple_escape('/'), '/');
    EXPECT_EQ(JsonValue::handle_simple_escape('b'), '\b');
    EXPECT_EQ(JsonValue::handle_simple_escape('f'), '\f');
    EXPECT_EQ(JsonValue::handle_simple_escape('n'), '\n');
    EXPECT_EQ(JsonValue::handle_simple_escape('r'), '\r');
    EXPECT_EQ(JsonValue::handle_simple_escape('t'), '\t');
}

TEST_F(StringEscapingTest, HandleSimpleEscape_InvalidEscapes) {
    // Test that invalid escape characters return NOT_SIMPLE_ESCAPE
    EXPECT_EQ(JsonValue::handle_simple_escape('u'), JsonValue::NOT_SIMPLE_ESCAPE);
    EXPECT_EQ(JsonValue::handle_simple_escape('x'), JsonValue::NOT_SIMPLE_ESCAPE);
    EXPECT_EQ(JsonValue::handle_simple_escape('a'), JsonValue::NOT_SIMPLE_ESCAPE);
    EXPECT_EQ(JsonValue::handle_simple_escape('z'), JsonValue::NOT_SIMPLE_ESCAPE);
    EXPECT_EQ(JsonValue::handle_simple_escape('1'), JsonValue::NOT_SIMPLE_ESCAPE);
    EXPECT_EQ(JsonValue::handle_simple_escape(' '), JsonValue::NOT_SIMPLE_ESCAPE);
}

// ============================================================================
// UNESCAPE STRING TESTS - SIMPLE ESCAPES
// ============================================================================

TEST_F(StringEscapingTest, UnescapeString_SimpleEscapes) {
    // Test all simple escape sequences in actual strings
    EXPECT_EQ(JsonValue::unescape_string("\\\""), "\"");
    EXPECT_EQ(JsonValue::unescape_string("\\\\"), "\\");
    EXPECT_EQ(JsonValue::unescape_string("\\/"), "/");
    EXPECT_EQ(JsonValue::unescape_string("\\b"), "\b");
    EXPECT_EQ(JsonValue::unescape_string("\\f"), "\f");
    EXPECT_EQ(JsonValue::unescape_string("\\n"), "\n");
    EXPECT_EQ(JsonValue::unescape_string("\\r"), "\r");
    EXPECT_EQ(JsonValue::unescape_string("\\t"), "\t");
}

TEST_F(StringEscapingTest, UnescapeString_MultipleEscapes) {
    // Test multiple escape sequences in one string
    EXPECT_EQ(JsonValue::unescape_string("Hello\\nWorld\\t!"), "Hello\nWorld\t!");
    EXPECT_EQ(JsonValue::unescape_string("\\\"Quoted\\\""), "\"Quoted\"");
    EXPECT_EQ(JsonValue::unescape_string("Path\\\\to\\\\file"), "Path\\to\\file");
    EXPECT_EQ(JsonValue::unescape_string("Line1\\r\\nLine2"), "Line1\r\nLine2");
}

TEST_F(StringEscapingTest, UnescapeString_MixedContent) {
    // Test mixing escape sequences with regular text
    EXPECT_EQ(JsonValue::unescape_string("Say \\\"Hello\\\" to the world!"), 
              "Say \"Hello\" to the world!");
    EXPECT_EQ(JsonValue::unescape_string("Tab:\\tNewline:\\nEnd"), 
              "Tab:\tNewline:\nEnd");
}

// ============================================================================
// UNESCAPE STRING TESTS - UNICODE (CURRENT BEHAVIOR)
// ============================================================================

TEST_F(StringEscapingTest, UnescapeString_UnicodeToUtf8) {
    // Test Unicode escapes are converted to UTF-8
    EXPECT_EQ(JsonValue::unescape_string("\\u0041"), "A");
    EXPECT_EQ(JsonValue::unescape_string("\\u00e9"), "é");
    EXPECT_EQ(JsonValue::unescape_string("\\u0048\\u0065\\u006C\\u006C\\u006F"), "Hello");
}

TEST_F(StringEscapingTest, UnescapeString_MixedUnicodeAndSimple) {
    // Test mixing Unicode escapes with simple escapes
    EXPECT_EQ(JsonValue::unescape_string("Hello \\u0041\\nWorld"), 
              "Hello A\nWorld");
    EXPECT_EQ(JsonValue::unescape_string("\\\"\\u00e9\\\""), 
              "\"é\"");
}

TEST_F(StringEscapingTest, UnescapeString_InvalidUnicode) {
    // Test invalid/incomplete Unicode escapes
    EXPECT_EQ(JsonValue::unescape_string("\\u004"), "\\u004");    // Too short
    EXPECT_EQ(JsonValue::unescape_string("\\u"), "\\u");          // Just \\u
    EXPECT_EQ(JsonValue::unescape_string("\\u00GH"), "\\u00GH");  // Invalid hex
    EXPECT_EQ(JsonValue::unescape_string("\\u00g1"), "\\u00g1");  // Mixed case invalid
}

// ============================================================================
// EDGE CASES AND ERROR HANDLING
// ============================================================================

TEST_F(StringEscapingTest, UnescapeString_EdgeCases) {
    // Empty string
    EXPECT_EQ(JsonValue::unescape_string(""), "");
    
    // No escape sequences
    EXPECT_EQ(JsonValue::unescape_string("Hello World"), "Hello World");
    
    // Single backslash at end (incomplete escape)
    EXPECT_EQ(JsonValue::unescape_string("Hello\\"), "Hello\\");
    
    // Unknown escape sequences
    EXPECT_EQ(JsonValue::unescape_string("\\x"), "\\x");  // Unknown escape
    EXPECT_EQ(JsonValue::unescape_string("\\z"), "\\z");  // Unknown escape
}

TEST_F(StringEscapingTest, UnescapeString_BackslashHandling) {
    // Test various backslash scenarios
    EXPECT_EQ(JsonValue::unescape_string("\\"), "\\");           // Single backslash
    EXPECT_EQ(JsonValue::unescape_string("\\\\\\\\"), "\\\\");   // Four backslashes -> two
    EXPECT_EQ(JsonValue::unescape_string("a\\b"), "a\b");        // Backspace escape sequence
    EXPECT_EQ(JsonValue::unescape_string("a\\z"), "a\\z");      // Unknown escape preserves both
    EXPECT_EQ(JsonValue::unescape_string("\\\\n"), "\\n");      // Escaped backslash + n
}

TEST_F(StringEscapingTest, UnescapeString_LongStrings) {
    // Test performance with longer strings
    std::string input = "Start\\n";
    for (int i = 0; i < 100; ++i) {
        input += "Line " + std::to_string(i) + "\\t";
    }
    input += "End\\n";
    
    std::string expected = "Start\n";
    for (int i = 0; i < 100; ++i) {
        expected += "Line " + std::to_string(i) + "\t";
    }
    expected += "End\n";
    
    EXPECT_EQ(JsonValue::unescape_string(input), expected);
}

// ============================================================================
// UTF-8 CONVERSION TESTS
// ============================================================================

TEST_F(StringEscapingTest, Utf8Conversion_AsciiRange) {
    // Test ASCII range (U+0000 to U+007F) - 1-byte UTF-8
    EXPECT_EQ(JsonValue::unescape_string("\\u0000"), std::string(1, '\0'));  // Null character
    EXPECT_EQ(JsonValue::unescape_string("\\u0020"), " ");   // Space
    EXPECT_EQ(JsonValue::unescape_string("\\u0041"), "A");   // Latin A
    EXPECT_EQ(JsonValue::unescape_string("\\u007F"), "\x7F"); // DEL character
}

TEST_F(StringEscapingTest, Utf8Conversion_TwoByteRange) {
    // Test 2-byte UTF-8 range (U+0080 to U+07FF)
    EXPECT_EQ(JsonValue::unescape_string("\\u00A9"), "©");    // Copyright symbol
    EXPECT_EQ(JsonValue::unescape_string("\\u00e9"), "é");    // Latin small letter e with acute
    EXPECT_EQ(JsonValue::unescape_string("\\u00C0"), "À");    // Latin capital letter A with grave
    EXPECT_EQ(JsonValue::unescape_string("\\u07FF"), "\xDF\xBF"); // Last 2-byte codepoint
}

TEST_F(StringEscapingTest, Utf8Conversion_ThreeByteRange) {
    // Test 3-byte UTF-8 range (U+0800 to U+FFFF)
    EXPECT_EQ(JsonValue::unescape_string("\\u2603"), "☃");    // Snowman
    EXPECT_EQ(JsonValue::unescape_string("\\u20AC"), "€");    // Euro sign
    EXPECT_EQ(JsonValue::unescape_string("\\u4E2D"), "中");   // Chinese character (middle)
    EXPECT_EQ(JsonValue::unescape_string("\\uD83D"), "\xED\xA0\xBD"); // High surrogate (incomplete)
}

TEST_F(StringEscapingTest, Utf8Conversion_CaseInsensitive) {
    // Test both uppercase and lowercase hex digits
    EXPECT_EQ(JsonValue::unescape_string("\\u00e9"), "é");    // Lowercase
    EXPECT_EQ(JsonValue::unescape_string("\\u00E9"), "é");    // Uppercase
    EXPECT_EQ(JsonValue::unescape_string("\\u00Aa"), "\xC2\xAA"); // Mixed case
    EXPECT_EQ(JsonValue::unescape_string("\\u00aA"), "\xC2\xAA"); // Mixed case reverse
}

TEST_F(StringEscapingTest, Utf8Conversion_SpecialCharacters) {
    // Test common special characters
    EXPECT_EQ(JsonValue::unescape_string("\\u0022"), "\"");   // Quotation mark
    EXPECT_EQ(JsonValue::unescape_string("\\u005C"), "\\");  // Backslash
    EXPECT_EQ(JsonValue::unescape_string("\\u002F"), "/");   // Solidus
    EXPECT_EQ(JsonValue::unescape_string("\\u000A"), "\n");  // Line feed
    EXPECT_EQ(JsonValue::unescape_string("\\u000D"), "\r");  // Carriage return
    EXPECT_EQ(JsonValue::unescape_string("\\u0009"), "\t");  // Tab
}

TEST_F(StringEscapingTest, Utf8Conversion_ComplexStrings) {
    // Test complex strings with multiple Unicode escapes
    EXPECT_EQ(JsonValue::unescape_string("\\u0048\\u0065\\u006C\\u006C\\u006F"), "Hello");
    EXPECT_EQ(JsonValue::unescape_string("\\u4E16\\u754C"), "世界");  // "World" in Chinese
    EXPECT_EQ(JsonValue::unescape_string("Caf\\u00e9"), "Café");     // Mixed ASCII and Unicode
    EXPECT_EQ(JsonValue::unescape_string("\\u03B1\\u03B2\\u03B3"), "αβγ"); // Greek letters
}

} // namespace jsom