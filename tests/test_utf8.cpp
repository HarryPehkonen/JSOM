#include "jsom.hpp"
#include <gtest/gtest.h>

// Test constants
constexpr std::uint32_t TEST_EMOJI_CODEPOINT = 0x1F600;

class UTF8Test : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(UTF8Test, IsValidUTF8Start) {
    EXPECT_TRUE(jsom::utf8::is_valid_utf8_start(0x41)); // ASCII 'A'
    EXPECT_TRUE(jsom::utf8::is_valid_utf8_start(0xC2)); // 2-byte start
    EXPECT_TRUE(jsom::utf8::is_valid_utf8_start(0xE0)); // 3-byte start
    EXPECT_TRUE(jsom::utf8::is_valid_utf8_start(0xF0)); // 4-byte start

    EXPECT_FALSE(jsom::utf8::is_valid_utf8_start(0x80)); // Continuation byte
    EXPECT_FALSE(jsom::utf8::is_valid_utf8_start(0xBF)); // Continuation byte
}

TEST_F(UTF8Test, IsUTF8Continuation) {
    EXPECT_TRUE(jsom::utf8::is_utf8_continuation(0x80)); // 10000000
    EXPECT_TRUE(jsom::utf8::is_utf8_continuation(0xBF)); // 10111111

    EXPECT_FALSE(jsom::utf8::is_utf8_continuation(0x41)); // ASCII
    EXPECT_FALSE(jsom::utf8::is_utf8_continuation(0xC2)); // Start byte
}

TEST_F(UTF8Test, UTF8SequenceLength) {
    EXPECT_EQ(jsom::utf8::utf8_sequence_length(0x41), 1); // ASCII
    EXPECT_EQ(jsom::utf8::utf8_sequence_length(0xC2), 2); // 2-byte
    EXPECT_EQ(jsom::utf8::utf8_sequence_length(0xE0), 3); // 3-byte
    EXPECT_EQ(jsom::utf8::utf8_sequence_length(0xF0), 4); // 4-byte
    EXPECT_EQ(jsom::utf8::utf8_sequence_length(0x80), 0); // Invalid
}

TEST_F(UTF8Test, IsValidHexString) {
    EXPECT_TRUE(jsom::utf8::is_valid_hex_string("0123456789"));
    EXPECT_TRUE(jsom::utf8::is_valid_hex_string("ABCDEF"));
    EXPECT_TRUE(jsom::utf8::is_valid_hex_string("abcdef"));
    EXPECT_TRUE(jsom::utf8::is_valid_hex_string("0041"));
    EXPECT_TRUE(jsom::utf8::is_valid_hex_string("FFFF"));
    EXPECT_TRUE(jsom::utf8::is_valid_hex_string(""));     // Empty string is valid
    
    EXPECT_FALSE(jsom::utf8::is_valid_hex_string("G"));    // Invalid hex char
    EXPECT_FALSE(jsom::utf8::is_valid_hex_string("0x41"));  // Has 'x'
    EXPECT_FALSE(jsom::utf8::is_valid_hex_string("123G"));  // Mixed valid/invalid
}

TEST_F(UTF8Test, HexToUint32) {
    EXPECT_EQ(jsom::utf8::hex_to_uint32("0000"), 0);
    EXPECT_EQ(jsom::utf8::hex_to_uint32("0041"), 65);    // 'A'
    EXPECT_EQ(jsom::utf8::hex_to_uint32("2713"), 10003); // Check mark
    EXPECT_EQ(jsom::utf8::hex_to_uint32("FFFF"), 65535);
    EXPECT_EQ(jsom::utf8::hex_to_uint32("ffff"), 65535); // Lower case
}

TEST_F(UTF8Test, UnicodeToUTF8) {
    EXPECT_EQ(jsom::utf8::unicode_to_utf8(0x41), "A");              // ASCII
    EXPECT_EQ(jsom::utf8::unicode_to_utf8(0xC6), "\xC3\x86");       // 2-byte (Æ)
    EXPECT_EQ(jsom::utf8::unicode_to_utf8(0x2713), "\xE2\x9C\x93"); // 3-byte (✓)

    std::string emoji = jsom::utf8::unicode_to_utf8(TEST_EMOJI_CODEPOINT); // 4-byte (😀)
    EXPECT_EQ(emoji.size(), 4);
    EXPECT_EQ(emoji, "\xF0\x9F\x98\x80");
}