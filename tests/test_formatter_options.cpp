// BEHAVIOUR of the formatting options, one test per documented switch.
//
// The invariants file asserts what must hold for any option set; this file asserts what
// each option actually DOES, so a change in behaviour is a failing test rather than a
// surprise in someone's config file. Assertions read the option values they depend on
// instead of hard-coding constants, so tuning a preset does not break them.
#include <algorithm>
#include <gtest/gtest.h>
#include <jsom/jsom.hpp>
#include <jsom/json_formatter.hpp>
#include <string>
#include <vector>

namespace {

auto format(const std::string& json, jsom::JsonFormatOptions options) -> std::string {
    return jsom::JsonFormatter{options}.format(jsom::parse_document(json));
}

/// Options that force every container onto multiple lines, so layout is explicit.
auto multiline_options(int indent = 4) -> jsom::JsonFormatOptions {
    jsom::JsonFormatOptions options;
    options.indent_size = indent;
    options.max_inline_array_size = 0;
    options.max_inline_object_size = 0;
    options.intelligent_wrapping = false;
    options.max_line_width = 0;
    return options;
}

auto count_lines(const std::string& text) -> std::size_t {
    return static_cast<std::size_t>(std::count(text.begin(), text.end(), '\n')) + 1;
}

auto line_starting_with(const std::string& text, const std::string& needle) -> std::string {
    std::size_t pos = 0;
    while (pos <= text.size()) {
        const std::size_t end = text.find('\n', pos);
        const std::string line = text.substr(pos, end == std::string::npos ? end : end - pos);
        if (line.find(needle) != std::string::npos) {
            return line;
        }
        if (end == std::string::npos) {
            break;
        }
        pos = end + 1;
    }
    return {};
}

} // namespace

// ---------------------------------------------------------------- basics

TEST(FormatterOptionsTest, CompactKeepsEverythingOnOneLine) {
    const std::string out
        = format(R"({"a":1,"b":[1,2,3],"c":{"d":true}})", jsom::FormatPresets::Compact);
    EXPECT_EQ(out.find('\n'), std::string::npos) << out;
    EXPECT_EQ(jsom::parse_document(out),
              jsom::parse_document(R"({"a":1,"b":[1,2,3],"c":{"d":true}})"));
}

TEST(FormatterOptionsTest, IndentSizeIsAppliedPerDepthLevel) {
    const std::string out = format(R"({"a":{"b":1}})", multiline_options(4));
    EXPECT_EQ(out, "{\n    \"a\": {\n        \"b\": 1\n    }\n}") << out;
}

TEST(FormatterOptionsTest, IndentNoneMeansCompactEvenWithNestedContainers) {
    jsom::JsonFormatOptions options;
    options.indent_size = std::nullopt;
    const std::string out = format(R"({"a":{"b":[1,2]}})", options);
    EXPECT_EQ(out.find('\n'), std::string::npos) << out;
}

TEST(FormatterOptionsTest, ColonSpacingControlsWhitespaceAfterColon) {
    for (const auto& [spacing, expected] : std::vector<std::pair<int, std::string>>{
             {0, R"({"a":1})"}, {1, R"({"a": 1})"}, {2, R"({"a" : 1})"}}) {
        jsom::JsonFormatOptions options = jsom::FormatPresets::Compact;
        options.colon_spacing = spacing;
        EXPECT_EQ(format(R"({"a":1})", options), expected) << "colon_spacing=" << spacing;
    }
}

TEST(FormatterOptionsTest, BracketSpacingAppliesToInlineAndEmptyContainers) {
    jsom::JsonFormatOptions options = jsom::FormatPresets::Compact;
    options.bracket_spacing = true;
    EXPECT_EQ(format(R"({"a":[1,2]})", options), R"({ "a": [ 1, 2 ] })");
    EXPECT_EQ(format(R"([1,2])", options), "[ 1, 2 ]");
    EXPECT_EQ(format("[[]]", options), "[ [ ] ]");
    EXPECT_EQ(format("{}", options), "{ }");
    EXPECT_EQ(format("[]", options), "[ ]");

    options.bracket_spacing = false;
    EXPECT_EQ(format("{}", options), "{}");
    EXPECT_EQ(format("[]", options), "[]");
}

TEST(FormatterOptionsTest, EmptyContainersAreNeverBrokenAcrossLines) {
    const std::string out = format(R"({"a":[],"b":{}})", multiline_options(4));
    EXPECT_EQ(out, "{\n    \"a\": [],\n    \"b\": {}\n}") << out;
}

// ---------------------------------------------------------------- inlining

TEST(FormatterOptionsTest, ArrayInlinesUpToTheSizeLimitAndWrapsAboveIt) {
    jsom::JsonFormatOptions options;
    options.indent_size = 2;
    options.max_line_width = 0;
    options.intelligent_wrapping = false;
    options.max_inline_array_size = 3;

    const std::string at_limit = format("[1,2,3]", options);
    EXPECT_EQ(at_limit, "[1, 2, 3]") << at_limit;

    const std::string above_limit = format("[1,2,3,4]", options);
    EXPECT_EQ(above_limit, "[\n  1,\n  2,\n  3,\n  4\n]") << above_limit;
}

TEST(FormatterOptionsTest, ObjectInlinesUpToTheSizeLimitAndWrapsAboveIt) {
    jsom::JsonFormatOptions options;
    options.indent_size = 2;
    options.max_line_width = 0;
    options.max_inline_object_size = 2;

    EXPECT_EQ(format(R"({"a":1,"b":2})", options), R"({"a": 1, "b": 2})");
    EXPECT_EQ(format(R"({"a":1,"b":2,"c":3})", options),
              "{\n  \"a\": 1,\n  \"b\": 2,\n  \"c\": 3\n}");
}

TEST(FormatterOptionsTest, AContainerHoldingAContainerGoesMultiline) {
    jsom::JsonFormatOptions options;
    options.indent_size = 2;
    options.max_line_width = 0;
    options.max_inline_array_size = 10;
    options.max_inline_object_size = 10;

    // One element, but that element is a container: the OUTER container is broken up even
    // though it is well under the size limit. The decision is per container, not inherited:
    // the inner one holds only a simple value and follows its own rule, so it stays inline.
    EXPECT_EQ(format("[[1]]", options), "[\n  [1]\n]");
    EXPECT_EQ(format(R"({"a":{"b":1}})", options), "{\n  \"a\": {\"b\": 1}\n}");

    // With the inline limits at zero the inner container is multiline as well.
    options.max_inline_array_size = 0;
    options.max_inline_object_size = 0;
    EXPECT_EQ(format("[[1]]", options), "[\n  [\n    1\n  ]\n]");
}

TEST(FormatterOptionsTest, MaxLineWidthForcesMultilineWhenInlineWouldNotFit) {
    jsom::JsonFormatOptions options;
    options.indent_size = 2;
    options.max_inline_array_size = 10;
    options.intelligent_wrapping = false;
    options.max_line_width = 40;
    EXPECT_EQ(format("[1,2,3]", options), "[1, 2, 3]") << "a fitting array stays inline";

    options.max_line_width = 4;
    EXPECT_EQ(format("[1,2,3]", options), "[\n  1,\n  2,\n  3\n]") << "a too-wide array wraps";
}

TEST(FormatterOptionsTest, MaxLineWidthZeroMeansNoLimit) {
    jsom::JsonFormatOptions options = jsom::FormatPresets::Compact;
    options.max_line_width = 0;
    const std::string wide = "[" + std::string(400, '1') + "]";
    EXPECT_EQ(format(wide, options).find('\n'), std::string::npos);
}

TEST(FormatterOptionsTest, IntelligentWrappingPacksSeveralElementsPerLine) {
    jsom::JsonFormatOptions options;
    options.indent_size = 2;
    options.max_inline_array_size = 3;
    options.intelligent_wrapping = true;
    options.max_line_width = 40;

    const std::string json = "[1,2,3,4,5,6,7,8,9,10,11,12]";
    const std::string out = format(json, options);

    EXPECT_GT(count_lines(out), 2) << "expected wrapping:\n" << out;
    EXPECT_LT(count_lines(out), 13) << "expected packing, not one element per line:\n" << out;
    EXPECT_EQ(jsom::parse_document(out), jsom::parse_document(json))
        << "wrapped output must still parse:\n"
        << out;
}

// ---------------------------------------------------------------- object layout

TEST(FormatterOptionsTest, AlignValuesPutsColonsInOneColumn) {
    jsom::JsonFormatOptions options = multiline_options(4);
    options.align_values = true;

    const std::string out = format(R"({"id":1,"description":"x","n":2})", options);
    const auto colon_column = [](const std::string& line) {
        return line.rfind(" : ", line.find(':') + 1) != std::string::npos ? line.find(':')
                                                                          : line.find(':');
    };
    const std::size_t id_colon = colon_column(line_starting_with(out, "\"id\""));
    const std::size_t description_colon = colon_column(line_starting_with(out, "\"description\""));
    const std::size_t n_colon = colon_column(line_starting_with(out, "\"n\""));
    EXPECT_EQ(id_colon, description_colon) << out;
    EXPECT_EQ(id_colon, n_colon) << out;
}

TEST(FormatterOptionsTest, WithoutAlignValuesColonsFollowTheirKeys) {
    // Keys come out sorted (std::map storage), so "description" is the first member and
    // "id" the last — and only a non-final member carries a comma.
    const std::string out = format(R"({"id":1,"description":"x"})", multiline_options(2));
    EXPECT_EQ(out, "{\n  \"description\": \"x\",\n  \"id\": 1\n}") << out;
}

TEST(FormatterOptionsTest, KeysAreSortedEvenWhenSortKeysIsOff) {
    // Object storage is a std::map, so key order is a property of the container rather
    // than of the formatter. Asserted so a future container change cannot silently
    // reorder existing users' output (and so sort_keys' effect stays understood).
    jsom::JsonFormatOptions options = jsom::FormatPresets::Compact;
    options.sort_keys = false;
    EXPECT_EQ(format(R"({"z":1,"a":2,"m":3})", options), R"({"a": 2, "m": 3, "z": 1})");

    options.sort_keys = true;
    EXPECT_EQ(format(R"({"z":1,"a":2,"m":3})", options), R"({"a": 2, "m": 3, "z": 1})");
}

TEST(FormatterOptionsTest, QuoteKeysFalseEmitsBareKeys) {
    jsom::JsonFormatOptions options = jsom::FormatPresets::Compact;
    options.quote_keys = false;
    // Non-standard output on purpose: the point is that the keys are written unquoted.
    EXPECT_EQ(format(R"({"a":1})", options), "{a: 1}");
}

TEST(FormatterOptionsTest, TrailingCommaAddsACommaAfterTheLastElement) {
    jsom::JsonFormatOptions options = multiline_options(2);
    options.trailing_comma = true;
    EXPECT_EQ(format("[1,2]", options), "[\n  1,\n  2,\n]");
    EXPECT_EQ(format(R"({"a":1,"b":2})", options), "{\n  \"a\": 1,\n  \"b\": 2,\n}");
}

// ---------------------------------------------------------------- values

TEST(FormatterOptionsTest, NumbersAreWrittenExactlyAsParsed) {
    // LazyNumber keeps the source text; the formatter must not re-render numbers.
    for (const char* number : {"1.500", "1e10", "1E+10", "-0.0", "0.0000000001",
                               "123456789012345678901234567890", "-17.25"}) {
        EXPECT_EQ(format(number, jsom::FormatPresets::Compact), number)
            << "number " << number << " was reformatted";
    }
}

TEST(FormatterOptionsTest, PreservedUnicodeEscapesAreEmittedVerbatim) {
    // Fidelity parsing keeps \uXXXX exactly as written; the formatter keeps that text.
    EXPECT_EQ(format(R"("\u4e2d\u6587")", jsom::FormatPresets::Compact), R"("\u4e2d\u6587")");
    EXPECT_EQ(format(R"("\u0041")", jsom::FormatPresets::Compact), R"("\u0041")");
    // A backslash that only LOOKS like a unicode escape is escaped as a backslash, so the
    // text survives. Built in memory: such a string cannot come from the parser, which
    // requires four hex digits after \u.
    const jsom::JsonDocument lookalike{std::string{R"(a\uZZZZb)"}};
    const std::string lookalike_out
        = jsom::JsonFormatter{jsom::FormatPresets::Compact}.format(lookalike);
    EXPECT_EQ(lookalike_out, R"("a\\uZZZZb")") << lookalike_out;
    EXPECT_EQ(jsom::parse_document(lookalike_out), lookalike);
}

TEST(FormatterOptionsTest, ControlCharactersAreAlwaysEscaped) {
    // A document can hold a raw control character (built in memory, since the parser
    // rejects raw control characters in input). Emitting it raw would produce invalid
    // JSON, so the escape is not optional and not tied to escape_unicode.
    const jsom::JsonDocument doc{std::string{"tab\there\nnewline\x01"
                                             "ctrl"}};
    const std::string out = jsom::JsonFormatter{jsom::FormatPresets::Compact}.format(doc);

    EXPECT_EQ(out, R"("tab\there\nnewline\u0001ctrl")") << out;

    // Read back with a DECODING reader: byte for byte.
    jsom::JsonParseOptions decoding;
    decoding.convert_unicode_escapes = true;
    EXPECT_EQ(jsom::parse_document(out, decoding), doc)
        << "escaped control characters must read back byte for byte";

    // A fidelity reader keeps the escape as text (documented JSOM behaviour, shared with
    // the serializer), which is why the exact round trip needs conversion enabled.
    const auto fidelity = jsom::parse_document(out, jsom::JsonParseOptions{});
    EXPECT_NE(fidelity, doc);
    EXPECT_EQ(fidelity.as<std::string>(), std::string{"tab\there\nnewline\\u0001ctrl"});
}

TEST(FormatterOptionsTest, EscapeUnicodeWritesCodepointsNotBytes) {
    jsom::JsonFormatOptions options = jsom::FormatPresets::Compact;
    options.escape_unicode = true;

    // Two-byte, three-byte and four-byte (astral, needs a surrogate pair) characters.
    EXPECT_EQ(format(R"("ä")", options), R"("\u00e4")");
    EXPECT_EQ(format(R"("中")", options), R"("\u4e2d")");
    EXPECT_EQ(format(R"("😀")", options), R"("\ud83d\ude00")");
    EXPECT_EQ(format(R"("aä中😀b")", options), R"("a\u00e4\u4e2d\ud83d\ude00b")");

    // ASCII is untouched, and the round trip is exact with a decoding reader.
    EXPECT_EQ(format(R"("plain")", options), R"("plain")");
    jsom::JsonParseOptions decoding;
    decoding.convert_unicode_escapes = true;
    EXPECT_EQ(jsom::parse_document(format(R"("häh miä 😀")", options), decoding),
              jsom::parse_document(R"("häh miä 😀")"));
}

TEST(FormatterOptionsTest, EscapeUnicodeLeavesTheStreamInDecimal) {
    // std::hex is sticky on a stream: if the escape path forgot to restore it, a later
    // number would be written in hex. Formatting a string first, then a number, checks it.
    jsom::JsonFormatOptions options = jsom::FormatPresets::Compact;
    options.escape_unicode = true;
    EXPECT_EQ(format(R"(["ä",255])", options), R"(["\u00e4", 255])");
}

TEST(FormatterOptionsTest, InvalidUtf8IsLeftAloneSoTheValueCannotChange) {
    // Not valid UTF-8: a lone continuation byte and a truncated sequence. No \uXXXX decodes
    // back to an invalid byte — escaping 0x80 as \u0080 yields TWO bytes (C2 80) and changes
    // the value, which is a reformat silently rewriting data. So the byte passes through
    // untouched: a string that was not valid UTF-8 stays exactly as it was, and the output is
    // no more invalid than the input. (Validating input UTF-8 is separate work: JSOM does no
    // UTF-8 validation when parsing.) Measured 2026-09-21: the previous byte-wise escaping
    // round-tripped "a\xc3" back as "a\xc3\x83".
    jsom::JsonFormatOptions options = jsom::FormatPresets::Compact;
    options.escape_unicode = true;
    const std::vector<std::string> malformed = {
        std::string{"a\x80"
                    "b"}, // lone continuation byte
        std::string{"a\xE2\x82"
                    "b"}, // truncated 3-byte sequence
    };
    for (const auto& bad : malformed) {
        const jsom::JsonDocument doc{bad};
        const std::string out = jsom::JsonFormatter{options}.format(doc);
        EXPECT_NO_THROW((void)jsom::parse_document(out)) << "output was: " << out;
        EXPECT_EQ(out, "\"" + bad + "\"") << "the bytes were rewritten: " << out;
        EXPECT_EQ(jsom::parse_document(out).as<std::string>(), bad) << "the value changed";
    }
}

// ---------------------------------------------------------------- limits

TEST(FormatterOptionsTest, MaxDepthThrowsInsteadOfRecursingForever) {
    jsom::JsonFormatOptions options = jsom::FormatPresets::Compact;
    options.max_depth = 2;

    const auto shallow = jsom::parse_document("[[1]]"); // depth 2
    EXPECT_NO_THROW((void)jsom::JsonFormatter{options}.format(shallow));

    const auto deep = jsom::parse_document("[[[1]]]"); // depth 3 > 2
    EXPECT_THROW((void)jsom::JsonFormatter{options}.format(deep), std::runtime_error);
}

TEST(FormatterOptionsTest, DeeplyNestedDocumentsFormatWithoutRecursionProblems) {
    std::string json;
    for (int i = 0; i < 100; ++i) {
        json += '[';
    }
    json += "1";
    for (int i = 0; i < 100; ++i) {
        json += ']';
    }
    const std::string out = format(json, jsom::FormatPresets::Compact);
    EXPECT_EQ(jsom::parse_document(out), jsom::parse_document(json));
}

TEST(FormatterOptionsTest, IndentationDeeperThanTheLineWidthDoesNotThrow) {
    // The indent prefix grows with depth, so past some depth it is LONGER than
    // max_line_width. Subtracting it from an unsigned width wrapped to ~2^64 and the
    // message buffer's reserve() threw std::length_error out of the formatter, which a
    // caller cannot be expected to catch: a 320-byte document from the nightly campaign
    // killed the process this way (fuzz/regressions/deep-nesting-exceeds-line-width.json).
    // With no room left on the line, every element takes its own line.
    const std::string deep
        = std::string(60, '[') + "[1,2,3,4,5,6,7,8,9,10,11,12]" + std::string(60, ']');
    const auto doc = jsom::parse_document(deep);

    for (const auto& preset : {jsom::FormatPresets::Pretty, jsom::FormatPresets::Debug,
                               jsom::FormatPresets::Config, jsom::FormatPresets::Api}) {
        std::string out;
        ASSERT_NO_THROW(out = jsom::JsonFormatter{preset}.format(doc));
        // ...and the output must still be the same document.
        EXPECT_EQ(jsom::parse_document(out), doc) << "preset produced unreadable output";
    }
}
