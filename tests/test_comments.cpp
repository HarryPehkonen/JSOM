#include <gtest/gtest.h>
#include "jsom/fast_parser.hpp"

using namespace jsom;

TEST(CommentTest, LineComment) {
    FastParser parser(ParsePresets::Comments);
    auto doc = parser.parse(R"({
        // this is a comment
        "name": "Alice"
    })");
    EXPECT_EQ(doc["name"].as<std::string>(), "Alice");
}

TEST(CommentTest, LineCommentAfterValue) {
    FastParser parser(ParsePresets::Comments);
    auto doc = parser.parse(R"({
        "name": "Alice", // inline comment
        "age": 30
    })");
    EXPECT_EQ(doc["name"].as<std::string>(), "Alice");
    EXPECT_EQ(doc["age"].as<int>(), 30);
}

TEST(CommentTest, BlockComment) {
    FastParser parser(ParsePresets::Comments);
    auto doc = parser.parse(R"({
        /* block comment */
        "value": 42
    })");
    EXPECT_EQ(doc["value"].as<int>(), 42);
}

TEST(CommentTest, BlockCommentMultiline) {
    FastParser parser(ParsePresets::Comments);
    auto doc = parser.parse(R"({
        /*
         * Multi-line
         * block comment
         */
        "data": true
    })");
    EXPECT_EQ(doc["data"].as<bool>(), true);
}

TEST(CommentTest, NestedObjectWithComments) {
    FastParser parser(ParsePresets::Comments);
    auto doc = parser.parse(R"({
        // Configuration
        "server": {
            "host": "localhost", /* default host */
            "port": 8080 // default port
        }
    })");
    EXPECT_EQ(doc["server"]["host"].as<std::string>(), "localhost");
    // NOLINTNEXTLINE(readability-magic-numbers)
    EXPECT_EQ(doc["server"]["port"].as<int>(), 8080);
}

TEST(CommentTest, ArrayWithComments) {
    FastParser parser(ParsePresets::Comments);
    auto doc = parser.parse(R"([
        1, // first
        2, /* second */
        3  // third
    ])");
    EXPECT_EQ(doc[0].as<int>(), 1);
    EXPECT_EQ(doc[1].as<int>(), 2);
    EXPECT_EQ(doc[2].as<int>(), 3);
}

TEST(CommentTest, CommentsDisabledByDefault) {
    FastParser parser; // default options
    EXPECT_THROW(parser.parse(R"({
        // comment
        "name": "Alice"
    })"),
                 std::runtime_error);
}

TEST(CommentTest, UnterminatedBlockCommentThrows) {
    FastParser parser(ParsePresets::Comments);
    EXPECT_THROW(parser.parse(R"({ /* unterminated comment "x": 1 })"), std::runtime_error);
}

TEST(CommentTest, CommentOnlyInput) {
    FastParser parser(ParsePresets::Comments);
    // Comment followed by valid JSON
    auto doc = parser.parse("// leading comment\n42");
    EXPECT_EQ(doc.as<int>(), 42);
}

TEST(CommentTest, BlockCommentBeforeValue) {
    FastParser parser(ParsePresets::Comments);
    auto doc = parser.parse("/* comment */ \"hello\"");
    EXPECT_EQ(doc.as<std::string>(), "hello");
}
