// The LOOSE option: number forms that are not valid JSON, accepted as extensions.
//
// RFC 8259 section 9 lets a parser accept non-JSON forms, so `allow_loose_numbers` is a
// deliberate, documented leniency -- useful when the producer is Microsoft Excel, a
// hand-edited config, or an internal tool you cannot change. It is OFF by default: an
// input that is not JSON should not be reported as JSON unless you asked for tolerance.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "jsom/jsom.hpp"

using namespace jsom;

namespace {

const std::vector<std::string>& loose_only_numbers() {
    static const std::vector<std::string> cases = {
        "[01]", "[-01]",  "[012]",  "[1.]",  "[-.123]", "[1.0.]",  "[2.e3]",
        "[0e]", "[1eE2]", "[01.5]", "[1+2]", "[-]",     "[0e+-1]",
    };
    return cases;
}

auto loose_numbers() -> JsonParseOptions {
    JsonParseOptions options;
    options.allow_loose_numbers = true;
    return options;
}

} // namespace

TEST(NumberLooseTest, TheOptionIsOffByDefault) {
    EXPECT_FALSE(JsonParseOptions{}.allow_loose_numbers);
    EXPECT_FALSE(ParsePresets::Default.allow_loose_numbers);
    // The named presets all inherit the strict default; only Loose turns it off.
    EXPECT_FALSE(ParsePresets::Unicode.allow_loose_numbers);
    EXPECT_FALSE(ParsePresets::Comments.allow_loose_numbers);
}

TEST(NumberLooseTest, LooseModeAcceptsTheExtensions) {
    for (const auto& json : loose_only_numbers()) {
        EXPECT_NO_THROW((void)parse_document(json, loose_numbers())) << json;
    }
    EXPECT_NO_THROW((void)parse_document(R"({"a": [01, 1., -]})", loose_numbers()));
}

TEST(NumberLooseTest, LooseModeStillKeepsTheOriginalText) {
    // Looseness is about acceptance, never about rewriting: whatever came in is what the
    // document holds, so the bytes survive a round trip (the LazyNumber promise).
    for (const auto& json : {"01", "1.", "-.123", "1eE2"}) {
        const auto doc = parse_document(json, loose_numbers());
        EXPECT_EQ(doc.to_json(), json) << json;
    }
}

TEST(NumberLooseTest, LooseModeRejectsStructureThatIsNotANumberProblem) {
    // The leniency is narrow on purpose: it relaxes the number grammar and nothing else.
    EXPECT_THROW((void)parse_document("[1,]", loose_numbers()), std::runtime_error);
    EXPECT_THROW((void)parse_document("[1]x", loose_numbers()), std::runtime_error);
    EXPECT_THROW((void)parse_document("{\"a\":}", loose_numbers()), std::runtime_error);
    EXPECT_THROW((void)parse_document("tru", loose_numbers()), std::runtime_error);
    EXPECT_THROW((void)parse_document("[\"a\tb\"]", loose_numbers()), std::runtime_error);
    EXPECT_THROW((void)parse_document(R"("unterminated)", loose_numbers()), std::runtime_error);
}

TEST(NumberLooseTest, TheExtensionDoesNotCoverNonNumberTokens) {
    // `+1`, `.5`, `0x1F`, `1_000` are not numbers with a lenient grammar — they are other
    // characters where a value was expected. Looseness covers the number grammar only, so
    // both modes reject them (RFC 8259 has no notion of them at all).
    for (const auto& json : {"[+1]", "[.5]", "[0x1F]", "[1_000]"}) {
        EXPECT_THROW((void)parse_document(json, loose_numbers()), std::runtime_error) << json;
        EXPECT_THROW((void)parse_document(json), std::runtime_error) << json;
    }
}

TEST(NumberLooseTest, TheLoosePresetCarriesTheOption) {
    EXPECT_TRUE(ParsePresets::Loose.allow_loose_numbers);
    EXPECT_EQ(ParsePresets::Loose.max_depth, ParsePresets::Default.max_depth);
    EXPECT_EQ(ParsePresets::Loose.convert_unicode_escapes,
              ParsePresets::Default.convert_unicode_escapes);
    EXPECT_NO_THROW((void)parse_document("[01, 1.5e2]", ParsePresets::Loose));
    EXPECT_THROW((void)parse_document("[01, 1.5e2]", ParsePresets::Default), std::runtime_error);
}

TEST(NumberLooseTest, LooseCombinesWithTheOtherOptions) {
    JsonParseOptions options = loose_numbers();
    options.allow_comments = true;
    options.convert_unicode_escapes = true;
    options.max_depth = 8;
    const std::string nine_deep = std::string(9, '[') + std::string(9, ']');
    EXPECT_NO_THROW((void)parse_document("[01, /* c */ 2]", options));
    EXPECT_THROW((void)parse_document(nine_deep, options), std::runtime_error);
}
