#include <gtest/gtest.h>
#include <jsom/jsom.hpp>

using namespace jsom;

class UnicodeEscapeTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(UnicodeEscapeTest, DefaultBehaviorPreservesEscapes) {
    // Default behavior should preserve Unicode escapes as-is
    auto doc = parse_document(R"({"letter": "\u0041", "emoji": "\uD83D\uDE00"})");
    
    EXPECT_EQ(doc["letter"].as<std::string>(), "\\u0041");
    EXPECT_EQ(doc["emoji"].as<std::string>(), "\\uD83D\\uDE00");
}

TEST_F(UnicodeEscapeTest, ConvertBasicUnicodeEscapes) {
    JsonParseOptions options;
    options.convert_unicode_escapes = true;
    
    // Test basic ASCII character
    auto doc = parse_document(R"({"letter": "\u0041"})", options);
    EXPECT_EQ(doc["letter"].as<std::string>(), "A");
    
    // Test non-ASCII character
    doc = parse_document(R"({"euro": "\u20AC"})", options);
    EXPECT_EQ(doc["euro"].as<std::string>(), "€");
}

TEST_F(UnicodeEscapeTest, ConvertUnicodeEscapesWithSurrogatePairs) {
    JsonParseOptions options;
    options.convert_unicode_escapes = true;
    
    // Test emoji using surrogate pairs
    auto doc = parse_document(R"({"emoji": "\uD83D\uDE00"})", options);
    EXPECT_EQ(doc["emoji"].as<std::string>(), "😀");
    
    // Test another emoji
    doc = parse_document(R"({"heart": "\uD83D\uDC96"})", options);
    EXPECT_EQ(doc["heart"].as<std::string>(), "💖");
}

TEST_F(UnicodeEscapeTest, MixedUnicodeAndRegularEscapes) {
    JsonParseOptions options;
    options.convert_unicode_escapes = true;
    
    auto doc = parse_document(R"({"text": "Hello\n\u0041\tWorld\u0021"})", options);
    EXPECT_EQ(doc["text"].as<std::string>(), "Hello\nA\tWorld!");
}

TEST_F(UnicodeEscapeTest, InvalidUnicodeEscapes) {
    JsonParseOptions options;
    options.convert_unicode_escapes = true;
    
    // Invalid hex digit
    EXPECT_THROW(parse_document(R"({"bad": "\u004G"})", options), std::runtime_error);
    
    // Incomplete escape
    EXPECT_THROW(parse_document(R"({"bad": "\u004"})", options), std::runtime_error);
    
    // Invalid surrogate (low without high)
    EXPECT_THROW(parse_document(R"({"bad": "\uDE00"})", options), std::runtime_error);
    
    // Incomplete surrogate pair
    EXPECT_THROW(parse_document(R"({"bad": "\uD83D"})", options), std::runtime_error);
    
    // Invalid low surrogate
    EXPECT_THROW(parse_document(R"({"bad": "\uD83D\u0041"})", options), std::runtime_error);
}

TEST_F(UnicodeEscapeTest, ArrayOfUnicodeStrings) {
    JsonParseOptions options;
    options.convert_unicode_escapes = true;
    
    auto doc = parse_document(R"(["A", "\u0042", "\u0043", "\uD83D\uDE00"])", options);
    EXPECT_EQ(doc[0].as<std::string>(), "A");
    EXPECT_EQ(doc[1].as<std::string>(), "B");
    EXPECT_EQ(doc[2].as<std::string>(), "C");
    EXPECT_EQ(doc[3].as<std::string>(), "😀");
}

TEST_F(UnicodeEscapeTest, NestedObjectsWithUnicode) {
    JsonParseOptions options;
    options.convert_unicode_escapes = true;
    
    auto doc = parse_document(R"({
        "user": {
            "name": "\u4E2D\u6587",
            "emoji": "\uD83D\uDE04"
        }
    })", options);
    
    EXPECT_EQ(doc["user"]["name"].as<std::string>(), "中文");
    EXPECT_EQ(doc["user"]["emoji"].as<std::string>(), "😄");
}

TEST_F(UnicodeEscapeTest, ParsePresetsWork) {
    // Test Default preset (preserves escapes)
    auto doc = parse_document(R"({"test": "\u0041"})", ParsePresets::Default);
    EXPECT_EQ(doc["test"].as<std::string>(), "\\u0041");
    
    // Test Unicode preset (converts escapes)
    doc = parse_document(R"({"test": "\u0041"})", ParsePresets::Unicode);
    EXPECT_EQ(doc["test"].as<std::string>(), "A");
}

TEST_F(UnicodeEscapeTest, LiteralOperatorUsesDefaultBehavior) {
    // The _jsom literal should use default behavior (preserve escapes)
    using namespace jsom::literals;
    
    auto doc = R"({"test": "\u0041"})"_jsom;
    EXPECT_EQ(doc["test"].as<std::string>(), "\\u0041");
}

TEST_F(UnicodeEscapeTest, RoundTripCompatibility) {
    // Test that default behavior maintains round-trip compatibility
    std::string original = R"({"unicode": "\u0041\u20AC\uD83D\uDE00"})";
    
    auto doc = parse_document(original);
    std::string serialized = doc.to_json();
    
    // The original Unicode escapes should be preserved in serialization
    EXPECT_TRUE(serialized.find("\\u0041") != std::string::npos);
    EXPECT_TRUE(serialized.find("\\u20AC") != std::string::npos);
    EXPECT_TRUE(serialized.find("\\uD83D") != std::string::npos);
    EXPECT_TRUE(serialized.find("\\uDE00") != std::string::npos);
}