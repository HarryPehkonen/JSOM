// The UTF-8 decoder (include/jsom/utf8.hpp) is the piece the formatter needs to escape
// text as \uXXXX codepoints instead of bytes: escaping bytes produces "\u00c3\u00a4" for
// "ä", which reads back as two different characters. These tests pin the decoder's
// accept/reject boundary, including the sequences a naive implementation gets wrong.
#include <gtest/gtest.h>
#include <jsom/utf8.hpp>
#include <string>

namespace {

auto decode_at(const std::string& text, std::size_t index = 0) -> std::pair<std::size_t, uint32_t> {
    uint32_t codepoint = 0;
    const std::size_t consumed = jsom::utf8::decode(text, index, codepoint);
    return {consumed, codepoint};
}

} // namespace

TEST(Utf8Test, DecodesEachSequenceLength) {
    // 1 byte (ASCII), 2 bytes (U+00E4), 3 bytes (U+4E2D), 4 bytes (U+1F600).
    EXPECT_EQ(decode_at("a"), (std::pair<std::size_t, uint32_t>{1, 0x61}));
    EXPECT_EQ(decode_at("\xC3\xA4"), (std::pair<std::size_t, uint32_t>{2, 0xE4}));
    EXPECT_EQ(decode_at("\xE4\xB8\xAD"), (std::pair<std::size_t, uint32_t>{3, 0x4E2D}));
    EXPECT_EQ(decode_at("\xF0\x9F\x98\x80"), (std::pair<std::size_t, uint32_t>{4, 0x1F600}));
}

TEST(Utf8Test, DecodesFromTheMiddleOfAString) {
    EXPECT_EQ(decode_at("ab\xE4\xB8\xADz", 2), (std::pair<std::size_t, uint32_t>{3, 0x4E2D}));
}

TEST(Utf8Test, RejectsStrayAndMalformedContinuationBytes) {
    EXPECT_EQ(decode_at("\x80").first, 0u);         // stray continuation byte
    EXPECT_EQ(decode_at("\xBF").first, 0u);         // stray continuation byte
    EXPECT_EQ(decode_at("\xC3\xC3").first, 0u);     // continuation byte missing
    EXPECT_EQ(decode_at("\xE4\xB8z").first, 0u);    // truncated sequence
    EXPECT_EQ(decode_at("\xF0\x9F\x98").first, 0u); // truncated 4-byte sequence
}

TEST(Utf8Test, RejectsOverlongEncodings) {
    // "<0x80" encoded in two bytes, "a" encoded in three bytes: valid bytes, invalid UTF-8.
    EXPECT_EQ(decode_at("\xC0\x80").first, 0u);
    EXPECT_EQ(decode_at("\xC1\xBF").first, 0u);
    EXPECT_EQ(decode_at("\xE0\x80\x80").first, 0u);
    EXPECT_EQ(decode_at("\xF0\x80\x80\x80").first, 0u);
}

TEST(Utf8Test, RejectsSurrogateHalvesAndOutOfRangeValues) {
    // UTF-8 must never encode U+D800..U+DFFF, and U+10FFFF is the ceiling.
    EXPECT_EQ(decode_at("\xED\xA0\x80").first, 0u);     // U+D800
    EXPECT_EQ(decode_at("\xED\xBF\xBF").first, 0u);     // U+DFFF
    EXPECT_EQ(decode_at("\xF4\x90\x80\x80").first, 0u); // U+110000
    EXPECT_EQ(decode_at("\xF7\xBF\xBF\xBF").first, 0u); // beyond the 4-byte range
}

TEST(Utf8Test, RejectsAnEmptyOrExhaustedString) {
    EXPECT_EQ(decode_at("").first, 0u);
    EXPECT_EQ(decode_at("a", 1).first, 0u);
    EXPECT_EQ(decode_at("ab", 5).first, 0u);
}

TEST(Utf8Test, EncodeIsTheInverseOfDecode) {
    for (const uint32_t codepoint : {0x41u, 0x7Fu, 0x80u, 0xE4u, 0x7FFu, 0x800u, 0x4E2Du, 0xFFFFu,
                                     0x10000u, 0x1F600u, 0x10FFFFu}) {
        std::string encoded;
        jsom::utf8::encode(encoded, codepoint);
        uint32_t decoded = 0;
        const std::size_t consumed = jsom::utf8::decode(encoded, 0, decoded);
        EXPECT_EQ(consumed, encoded.size()) << "codepoint U+" << std::hex << codepoint;
        EXPECT_EQ(decoded, codepoint) << "codepoint U+" << std::hex << codepoint;
    }
}

TEST(Utf8Test, IsContinuationMatchesTheUtf8BitPattern) {
    EXPECT_TRUE(jsom::utf8::is_continuation(0x80));
    EXPECT_TRUE(jsom::utf8::is_continuation(0xBF));
    EXPECT_FALSE(jsom::utf8::is_continuation(0x7F));
    EXPECT_FALSE(jsom::utf8::is_continuation(0xC0));
    EXPECT_FALSE(jsom::utf8::is_continuation(0xFF));
}
