// jsom::ParseError — a typed exception for parse/format failures.
//
// Compatibility promise: ParseError derives from std::runtime_error, and every what()
// string is byte-for-byte what the old generic std::runtime_error used to say. Existing
// callers that catch std::runtime_error (or std::exception) around parse_document() /
// to_json() keep working unmodified; new callers can switch on code() instead of matching
// what() substrings.

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <type_traits>

#include "jsom/jsom.hpp"
#include "jsom/json_pointer.hpp"
#include "jsom/parse_error.hpp"

using namespace jsom;

namespace {

/// Builds a JsonDocument nested `depth` levels deep without going through the parser, so
/// the JsonDocument-side depth guard (compare/serialize) can be exercised on its own —
/// same technique as tests/test_nesting_limits.cpp.
auto nest_programmatically(int depth) -> JsonDocument {
    auto doc = JsonDocument{0};
    for (int level = 1; level < depth; ++level) {
        auto outer = JsonDocument::make_array();
        outer.push_back(std::move(doc));
        doc = std::move(outer);
    }
    return doc;
}

auto nested_arrays(int depth) -> std::string {
    const auto n = static_cast<size_t>(depth);
    return std::string(n, '[') + std::string(n, ']');
}

} // namespace

// ============================================================
// 1. One (input, expected code) case per enumerator (RED: ParseErrorCode/ParseError
//    do not exist until parse_error.hpp is implemented).
// ============================================================

TEST(ParseErrorCodeTest, NestingDepthExceededFromParsing) {
    JsonParseOptions options;
    options.max_depth = 1;
    try {
        (void)parse_document(nested_arrays(2), options);
        FAIL() << "expected a ParseError";
    } catch (const ParseError& error) {
        EXPECT_EQ(error.code(), ParseErrorCode::NestingDepthExceeded);
    }
}

TEST(ParseErrorCodeTest, NestingDepthExceededFromSerialization) {
    const auto doc = nest_programmatically(limits::MAX_NESTING_DEPTH + 2);
    try {
        (void)doc.to_json();
        FAIL() << "expected a ParseError";
    } catch (const ParseError& error) {
        EXPECT_EQ(error.code(), ParseErrorCode::NestingDepthExceeded);
    }
}

TEST(ParseErrorCodeTest, NestingDepthExceededFromFormatting) {
    JsonFormatOptions options = FormatPresets::Compact;
    options.max_depth = 1;
    const auto doc = nest_programmatically(3);
    try {
        (void)JsonFormatter{options}.format(doc);
        FAIL() << "expected a ParseError";
    } catch (const ParseError& error) {
        EXPECT_EQ(error.code(), ParseErrorCode::NestingDepthExceeded);
    }
}

TEST(ParseErrorCodeTest, InvalidNumber) {
    try {
        (void)parse_document("[01]");
        FAIL() << "expected a ParseError";
    } catch (const ParseError& error) {
        EXPECT_EQ(error.code(), ParseErrorCode::InvalidNumber);
    }
}

TEST(ParseErrorCodeTest, InvalidEscape) {
    try {
        (void)parse_document(R"(["\q"])");
        FAIL() << "expected a ParseError";
    } catch (const ParseError& error) {
        EXPECT_EQ(error.code(), ParseErrorCode::InvalidEscape);
    }
}

TEST(ParseErrorCodeTest, InvalidUnicodeEscape) {
    try {
        (void)parse_document(R"(["\u1"])");
        FAIL() << "expected a ParseError";
    } catch (const ParseError& error) {
        EXPECT_EQ(error.code(), ParseErrorCode::InvalidUnicodeEscape);
    }
}

TEST(ParseErrorCodeTest, RawControlCharacter) {
    try {
        (void)parse_document("[\"a\tb\"]");
        FAIL() << "expected a ParseError";
    } catch (const ParseError& error) {
        EXPECT_EQ(error.code(), ParseErrorCode::RawControlCharacter);
    }
}

TEST(ParseErrorCodeTest, UnterminatedString) {
    try {
        (void)parse_document(R"("unterminated)");
        FAIL() << "expected a ParseError";
    } catch (const ParseError& error) {
        EXPECT_EQ(error.code(), ParseErrorCode::UnterminatedString);
    }
}

TEST(ParseErrorCodeTest, UnterminatedComment) {
    try {
        (void)parse_document(R"({ /* unterminated comment "x": 1 })", ParsePresets::Comments);
        FAIL() << "expected a ParseError";
    } catch (const ParseError& error) {
        EXPECT_EQ(error.code(), ParseErrorCode::UnterminatedComment);
    }
}

TEST(ParseErrorCodeTest, InvalidLiteral) {
    try {
        (void)parse_document("tru");
        FAIL() << "expected a ParseError";
    } catch (const ParseError& error) {
        EXPECT_EQ(error.code(), ParseErrorCode::InvalidLiteral);
    }
}

TEST(ParseErrorCodeTest, ExpectedToken) {
    try {
        (void)parse_document(R"({"a" 1})");
        FAIL() << "expected a ParseError";
    } catch (const ParseError& error) {
        EXPECT_EQ(error.code(), ParseErrorCode::ExpectedToken);
    }
}

TEST(ParseErrorCodeTest, EmptyInput) {
    try {
        (void)parse_document("");
        FAIL() << "expected a ParseError";
    } catch (const ParseError& error) {
        EXPECT_EQ(error.code(), ParseErrorCode::EmptyInput);
    }
}

TEST(ParseErrorCodeTest, TrailingContent) {
    try {
        (void)parse_document("[1]x");
        FAIL() << "expected a ParseError";
    } catch (const ParseError& error) {
        EXPECT_EQ(error.code(), ParseErrorCode::TrailingContent);
    }
}

// ============================================================
// 2. what() text is unchanged, character for character, for at least one input per
//    throw site. Copied from the current (pre-ParseError) exact strings.
// ============================================================

TEST(ParseErrorMessageTest, MessagesAreUnchanged) {
    JsonParseOptions depth_options;
    depth_options.max_depth = 1;
    try {
        (void)parse_document(nested_arrays(2), depth_options);
        FAIL() << "expected a ParseError";
    } catch (const ParseError& error) {
        EXPECT_STREQ(error.what(), "Maximum nesting depth exceeded (limit 1)");
    }

    const auto deep_doc = nest_programmatically(limits::MAX_NESTING_DEPTH + 2);
    try {
        (void)deep_doc.to_json();
        FAIL() << "expected a ParseError";
    } catch (const ParseError& error) {
        EXPECT_EQ(std::string(error.what()), "Maximum nesting depth exceeded (limit "
                                                 + std::to_string(limits::MAX_NESTING_DEPTH) + ")");
    }

    JsonFormatOptions format_options = FormatPresets::Compact;
    format_options.max_depth = 1;
    try {
        (void)JsonFormatter{format_options}.format(nest_programmatically(3));
        FAIL() << "expected a ParseError";
    } catch (const ParseError& error) {
        EXPECT_STREQ(error.what(), "Maximum formatting depth exceeded");
    }

    try {
        (void)parse_document("[01]");
        FAIL() << "expected a ParseError";
    } catch (const ParseError& error) {
        EXPECT_STREQ(error.what(), "Invalid number: 01");
    }

    try {
        (void)parse_document(R"(["\q"])");
        FAIL() << "expected a ParseError";
    } catch (const ParseError& error) {
        EXPECT_STREQ(error.what(), "Invalid escape sequence: \\q");
    }

    try {
        (void)parse_document(R"(["\u1"])");
        FAIL() << "expected a ParseError";
    } catch (const ParseError& error) {
        EXPECT_STREQ(error.what(), "Incomplete unicode escape");
    }

    try {
        (void)parse_document("[\"a\tb\"]");
        FAIL() << "expected a ParseError";
    } catch (const ParseError& error) {
        EXPECT_STREQ(error.what(), "Unescaped control character in string");
    }

    try {
        (void)parse_document(R"("unterminated)");
        FAIL() << "expected a ParseError";
    } catch (const ParseError& error) {
        EXPECT_STREQ(error.what(), "Unterminated string");
    }

    try {
        (void)parse_document(R"({ /* unterminated comment "x": 1 })", ParsePresets::Comments);
        FAIL() << "expected a ParseError";
    } catch (const ParseError& error) {
        EXPECT_STREQ(error.what(), "Unterminated block comment");
    }

    try {
        (void)parse_document("tru");
        FAIL() << "expected a ParseError";
    } catch (const ParseError& error) {
        EXPECT_STREQ(error.what(), "Invalid literal");
    }

    try {
        (void)parse_document(R"({"a" 1})");
        FAIL() << "expected a ParseError";
    } catch (const ParseError& error) {
        EXPECT_STREQ(error.what(), "Expected ':' but got '1'");
    }

    try {
        (void)parse_document("");
        FAIL() << "expected a ParseError";
    } catch (const ParseError& error) {
        EXPECT_STREQ(error.what(), "Empty JSON input");
    }

    try {
        (void)parse_document("[1]x");
        FAIL() << "expected a ParseError";
    } catch (const ParseError& error) {
        EXPECT_STREQ(error.what(), "Unexpected characters after JSON");
    }
}

// ============================================================
// 3. The compatibility promise: ParseError is still catchable as std::runtime_error and
//    std::exception, so existing callers built before ParseError existed keep working.
// ============================================================

TEST(ParseErrorCompatibilityTest, CaughtAsRuntimeError) {
    EXPECT_THROW((void)parse_document("[01]"), std::runtime_error);
}

TEST(ParseErrorCompatibilityTest, CaughtAsException) {
    EXPECT_THROW((void)parse_document("[01]"), std::exception);
}

TEST(ParseErrorCompatibilityTest, RuntimeErrorCatchSeesTheSameMessage) {
    try {
        (void)parse_document("[01]");
        FAIL() << "expected an exception";
    } catch (const std::runtime_error& error) {
        EXPECT_STREQ(error.what(), "Invalid number: 01");
    }
}

// ============================================================
// 4. The JSON Pointer exception hierarchy is untouched by this change: still its own
//    types, still its own messages.
// ============================================================

TEST(ParseErrorCompatibilityTest, JsonPointerExceptionsAreUnchanged) {
    JsonDocument doc = JsonDocument::make_object();
    doc.set("a", JsonDocument(1));

    try {
        (void)doc.at("/missing");
        FAIL() << "expected JsonPointerNotFoundException";
    } catch (const JsonPointerNotFoundException& error) {
        EXPECT_STREQ(error.what(), "JSON Pointer '/missing': Path not found");
    }

    try {
        JsonPointer::to_array_index("not-a-number");
        FAIL() << "expected InvalidJsonPointerException";
    } catch (const InvalidJsonPointerException& error) {
        EXPECT_STREQ(error.what(),
                     "JSON Pointer 'not-a-number': Invalid JSON Pointer - not a valid array index");
    }

    // Pointer exceptions are not ParseError, and vice versa: two disjoint hierarchies.
    EXPECT_THROW((void)doc.at("/missing"), std::runtime_error);
    static_assert(!std::is_base_of_v<ParseError, JsonPointerNotFoundException>);
    static_assert(!std::is_base_of_v<JsonPointerException, ParseError>);
}
