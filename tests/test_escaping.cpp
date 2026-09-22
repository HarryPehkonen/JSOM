// One escaping routine, not three.
//
// Measured 2026-09-21: JSOM carried THREE implementations of "escape a string for JSON" —
// two private statics in json_document.hpp (one per sink: std::string and std::ostream) and
// one inside the formatter. The two in json_document.hpp had drifted: the string version
// built `\uXXXX` by hand, the ostream version formatted it with stream manipulators
// (`<< std::hex << std::setfill('0') << std::setw(4)`), which leaves the CALLER's stream in
// hex mode with a fill and width armed. That is the same technique that shipped the
// `\u000<byte>` bug in the formatter's escape_unicode.

#include <jsom/jsom.hpp>

#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

namespace {

using jsom::FormatPresets;
using jsom::JsonDocument;
using jsom::ParsePresets;

// Control characters outside the named escape set, the named ones, quotes, backslash, a
// high byte and non-ASCII UTF-8, plus a string long enough to exercise the fast path.
std::vector<std::string> corpus() {
    return {
        std::string("plain ascii, nothing to escape"),
        std::string("quote \" and backslash \\"),
        std::string("named escapes: \b\f\n\r\t"),
        std::string("control \x01 and \x1f and \x00 NUL"),
        std::string("DEL \x7f and high byte \xc3"),
        "non-ascii: \u00e4 \u20ac \U0001f600",
        std::string(300, 'a') + "\x02" + std::string(300, 'b'),
    };
}

TEST(EscapingTest, EscapingNeverTouchesTheCallersStreamState) {
    // RED before the fix: escape_string left std::hex and std::setw(4) armed on the stream,
    // so anything the caller printed afterwards came out wrong (42 as "2a").
    const JsonDocument doc(std::string("control \x01 here"));
    std::ostringstream out;
    const auto flags_before = out.flags();
    const auto fill_before = out.fill();
    const auto precision_before = out.precision();

    doc.serialize_to(out, false);
    out << 42; // the caller keeps using the stream

    EXPECT_EQ(out.flags(), flags_before) << "the library changed the caller's stream flags";
    EXPECT_EQ(out.fill(), fill_before) << "the library changed the caller's fill character";
    EXPECT_EQ(out.precision(), precision_before) << "the library changed the caller's precision";
    EXPECT_NE(out.str().find("42"), std::string::npos)
        << "the caller's 42 came out as: "
        << out.str().substr(out.str().size() > 8 ? out.str().size() - 8 : 0);
}

TEST(EscapingTest, TheTwoSerializersEscapeTheSameWay) {
    // Compact goes through the serializer's escaper, the presets go through the formatter's.
    // Two implementations of one rule are exactly how they drift apart.
    for (const auto& value : corpus()) {
        const JsonDocument doc(value);
        const std::string compact = doc.to_json();
        const std::string pretty = doc.to_json(FormatPresets::Pretty);
        EXPECT_EQ(compact, pretty) << "escaping differs between the serializer and the formatter";
    }
}

TEST(EscapingTest, EscapedOutputReparsesToTheSameString) {
    // Whatever the escaping, the value has to come back. \uXXXX is decoded by
    // ParsePresets::Unicode; raw UTF-8 bytes are legal JSON and need no decoding.
    for (const auto& value : corpus()) {
        const JsonDocument doc(value);
        for (const auto& preset :
             {FormatPresets::Compact, FormatPresets::Pretty, FormatPresets::Debug}) {
            const std::string text = doc.to_json(preset);
            const auto reparsed = jsom::parse_document(text, ParsePresets::Unicode);
            EXPECT_EQ(reparsed.as<std::string>(), value)
                << "round trip changed the value: " << text;
        }
    }
}

TEST(EscapingTest, EscapeUnicodeEscapesCodepointsNotBytes) {
    // The bug this pins: escaping UTF-8 BYTES gives \u00c3\u00a4 for "ä", which is a
    // different string (two characters) that happens to look similar.
    const JsonDocument doc(std::string("\u00e4") + "\U0001f600");
    const std::string escaped = doc.to_json(FormatPresets::Debug);

    EXPECT_NE(escaped.find("\\u00e4"), std::string::npos) << escaped;
    EXPECT_NE(escaped.find("\\ud83d\\ude00"), std::string::npos) << escaped; // surrogate pair
    EXPECT_EQ(escaped.find("\\u00c3"), std::string::npos) << "escaped the UTF-8 bytes: " << escaped;
    EXPECT_EQ(jsom::parse_document(escaped, ParsePresets::Unicode).as<std::string>(),
              doc.as<std::string>());
}

TEST(EscapingTest, ControlCharactersAreAlwaysEscaped) {
    // A raw control character is not valid JSON, so no preset may emit one — reachable
    // through in-memory documents, since the parser refuses them in input.
    for (const auto& value : corpus()) {
        const JsonDocument doc(value);
        for (const auto& preset :
             {FormatPresets::Compact, FormatPresets::Pretty, FormatPresets::Config,
              FormatPresets::Api, FormatPresets::Debug}) {
            const std::string text = doc.to_json(preset);
            for (unsigned char c : text) {
                EXPECT_GE(c, jsom::character_constants::MIN_CONTROL_CHAR)
                    << "raw control character in output: " << text;
            }
        }
    }
}

} // namespace
