// The JSOM JSONFuzz adapter (fuzz/jsonfuzz_jsom.hpp) held to the SUT contract.
//
// The adapter is the bridge that lets JSONFuzz's structure-aware generator, mutators
// and oracles drive JSOM. These tests pin the contract the fuzzer relies on: parse
// maps ParseConfig onto JsonParseOptions and reports rejection as an outcome (never a
// throw); serialize() round-trips; every pointer from pointers() resolves through
// get(); RFC 6901 escaping works in both directions; and the two opt-in parse options
// (comments, unicode escapes) plus the strict number grammar are honoured.

#include "jsonfuzz_jsom.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

namespace {

using jsonfuzz::ParseConfig;

TEST(JsonomSutTest, ParsesValidJsonAndReportsRejectionForInvalid) {
    jsom_fuzz::JsonomSut sut;
    const ParseConfig cfg;
    EXPECT_TRUE(sut.parse(R"({"a":1,"b":[true,null]})", cfg).accepted);
    EXPECT_FALSE(sut.parse(R"({"a":)", cfg).accepted);
    EXPECT_FALSE(sut.parse(R"([1,2,])", cfg).accepted);
    EXPECT_FALSE(sut.parse(R"("unterminated)", cfg).accepted);
}

TEST(JsonomSutTest, RejectionIsAnOutcomeNotAThrow) {
    jsom_fuzz::JsonomSut sut;
    const ParseConfig cfg;
    const jsonfuzz::ParseResult r = sut.parse(R"({"a":)", cfg);
    EXPECT_FALSE(r.accepted);
    EXPECT_FALSE(r.error.empty());
}

TEST(JsonomSutTest, SerializeRoundTrips) {
    jsom_fuzz::JsonomSut sut;
    const ParseConfig cfg;
    const std::string text = R"({"a":1,"b":[true,null,"x"],"c":-0.5e3})";
    ASSERT_TRUE(sut.parse(text, cfg).accepted);
    const std::string once = sut.serialize();
    ASSERT_TRUE(sut.parse(once, cfg).accepted);
    EXPECT_EQ(sut.serialize(), once);
}

TEST(JsonomSutTest, EveryPointerResolvesThroughGet) {
    jsom_fuzz::JsonomSut sut;
    const ParseConfig cfg;
    ASSERT_TRUE(sut.parse(R"({"a":{"b":1},"c":[10,20]})", cfg).accepted);
    const auto ptrs = sut.pointers(8);
    ASSERT_FALSE(ptrs.empty());
    for (const auto& p : ptrs) {
        EXPECT_TRUE(sut.get(p).has_value()) << "pointer " << p << " did not resolve";
    }
    EXPECT_EQ(sut.get("/a/b"), std::optional<std::string>("1"));
    EXPECT_EQ(sut.get("/c/1"), std::optional<std::string>("20"));
    EXPECT_EQ(sut.get("/nope"), std::nullopt);
}

TEST(JsonomSutTest, PointerEscapingResolvesSlashAndTilde) {
    jsom_fuzz::JsonomSut sut;
    const ParseConfig cfg;
    ASSERT_TRUE(sut.parse(R"({"/":1,"~":2})", cfg).accepted);
    EXPECT_EQ(sut.get("/~1"), std::optional<std::string>("1"));
    EXPECT_EQ(sut.get("/~0"), std::optional<std::string>("2"));
}

TEST(JsonomSutTest, PointerEnumerationEscapesKeysSoThePointerRoundTrips) {
    jsom_fuzz::JsonomSut sut;
    const ParseConfig cfg;
    ASSERT_TRUE(sut.parse(R"({"a/b":1,"c~d":2,"plain":3})", cfg).accepted);
    const auto ptrs = sut.pointers(8);
    EXPECT_NE(std::find(ptrs.begin(), ptrs.end(), "/a~1b"), ptrs.end());
    EXPECT_NE(std::find(ptrs.begin(), ptrs.end(), "/c~0d"), ptrs.end());
    EXPECT_EQ(std::find(ptrs.begin(), ptrs.end(), "/a/b"), ptrs.end());
    EXPECT_EQ(sut.get("/a~1b"), std::optional<std::string>("1"));
    EXPECT_EQ(sut.get("/c~0d"), std::optional<std::string>("2"));
}

TEST(JsonomSutTest, CommentsAcceptedOnlyWhenAllowCommentsIsOn) {
    jsom_fuzz::JsonomSut sut;
    ParseConfig cfg;
    const std::string text = R"({"a":1 /* comment */})";
    EXPECT_FALSE(sut.parse(text, cfg).accepted);
    cfg.allow_comments = true;
    EXPECT_TRUE(sut.parse(text, cfg).accepted);
}

TEST(JsonomSutTest, UnicodeEscapesConvertedOnlyWhenUnicodeEscapesIsOn) {
    jsom_fuzz::JsonomSut sut;
    ParseConfig cfg;
    const std::string text = R"({"a":"\u00e4"})";
    ASSERT_TRUE(sut.parse(text, cfg).accepted);
    // Off: the escape is preserved literally for round-trip fidelity (the backslash is
    // itself escaped when the value is serialized, so the JSON text is "\\u00e4").
    EXPECT_EQ(sut.get("/a"), std::optional<std::string>("\"\\\\u00e4\""));
    cfg.unicode_escapes = true;
    ASSERT_TRUE(sut.parse(text, cfg).accepted);
    // On: the escape is converted to the UTF-8 character.
    EXPECT_EQ(sut.get("/a"), std::optional<std::string>("\"\xc3\xa4\""));
}

TEST(JsonomSutTest, StrictNumbersRejectsLeadingZero) {
    jsom_fuzz::JsonomSut sut;
    ParseConfig cfg;
    const std::string text = R"({"a":-01})";
    // Lazy default: accepted as an extension.
    EXPECT_TRUE(sut.parse(text, cfg).accepted);
    cfg.strict_numbers = true;
    EXPECT_FALSE(sut.parse(text, cfg).accepted);
}

} // namespace
