// Lexical conformance — the RFC 8259 rules the suite requires, enforced ALWAYS.
//
// Decision (Harri, 2026-09-16): these are not a setting. Whatever `validation` is set
// to, JSOM rejects what the spec says is invalid:
//
//   1. a raw control character (U+0000..U+001F) inside a string — §7
//   2. `\u` not followed by exactly four hex digits — §7, `unicode = "u" 4HEXDIG`
//   3. an escape character outside `" \ / b f n r t u` — §7, `escape = ...`
//   4. formfeed (or vertical tab) used as whitespace — §2, `ws = space / tab / LF / CR`
//
// Rule 3 also removes the old "backslash dropped" behaviour by construction: the path
// that used to append the character and lose its backslash is now a rejection, so no
// accepted document is ever altered. There is nothing left to preserve.
//
// Costs are measured, not assumed: these sit on the always-hot string path, see the A/B
// in OPTIMIZATIONS.md.
#include <gtest/gtest.h>

#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "jsom/jsom.hpp"

using namespace jsom;

namespace {

/// The conformance-suite files JSOM used to accept, byte for byte
/// (nst/JSONTestSuite `n_string_*` and `n_structure_whitespace_formfeed`).
struct Case {
    const char* name;
    std::string json;
};

std::vector<Case> spec_violations() {
    const std::string nul(1, '\0');
    return {
        // 1. raw control characters inside a string
        {"n_string_unescaped_ctrl_char", std::string("[\"a") + nul + "a\"]"},
        {"n_string_unescaped_newline", "[\"new\nline\"]"},
        {"n_string_unescaped_tab", "[\"\t\"]"},
        {"n_string_backslash_00", std::string("[\"\\") + nul + "\"]"},
        {"n_string_escaped_ctrl_char_tab", "[\"\\\t\"]"},
        // 2. `\u` without four hex digits
        {"n_string_invalid_unicode_escape", R"(["\uqqqq"])"},
        {"n_string_1_surrogate_then_escape_u", R"(["\uD800\u"])"},
        {"n_string_1_surrogate_then_escape_u1", R"(["\uD800\u1"])"},
        {"n_string_incomplete_surrogate_escape_invalid", R"(["\uD800\uD800\x"])"},
        {"n_string_invalid-utf-8-in-escape", std::string("[\"\\u") + "\xe5" + "\"]"},
        // 3. escape characters outside the spec's set
        {"n_string_invalid_backslash_esc", R"(["\a"])"},
        {"n_string_unicode_CapitalU", R"("\UA66D")"},
        {"n_string_escaped_emoji", std::string("[\"\\") + "\xf0\x9f\x8c\x80" + "\"]"},
        {"n_string_invalid_utf8_after_escape", std::string("[\"\\") + "\xe5" + "\"]"},
        // 4. whitespace outside space / tab / LF / CR
        {"n_structure_whitespace_formfeed", "[\x0c]"},
    };
}

/// Shapes that MUST keep working — the conformance suite's must-accept set is the real
/// contract, and a validation rule that rejects valid JSON is worse than the bug.
std::vector<Case> must_still_parse() {
    const std::string nul_escape = R"(["\u0000"])";
    return {
        {"escaped tab", R"(["\t"])"},
        {"escaped newline", R"(["\n"])"},
        {"escaped backslash", R"(["\\"])"},
        {"escaped quote", R"(["\""])"},
        {"escaped solidus", R"(["\/"])"},
        {"escaped NUL (legal form)", nul_escape},
        {"valid \\u escape", R"(["\u4e2d"])"},
        {"surrogate pair", R"(["\uD83D\uDE00"])"},
        {"uppercase hex", R"(["\u00E9"])"},
        {"raw UTF-8 in a string", "[\"\xe6\x97\xa5\xe6\x9c\xac\"]"},
        {"tab and newline as whitespace", "[\t1,\n2,\r3\t]"},
        {"spaces around everything", "  {  \"a\"  :  [  1  ]  }  "},
        {"a backslash that came from an escaped backslash", R"(["\\u0041"])"},
    };
}

} // namespace

TEST(LexicalConformanceTest, SpecViolationsAreRejectedInTheDefaultConfiguration) {
    for (const auto& c : spec_violations()) {
        EXPECT_THROW((void)parse_document(c.json), std::runtime_error)
            << c.name << " must be rejected (its bytes: " << c.json.size() << ")";
    }
}

TEST(LexicalConformanceTest, MustAcceptShapesBrakeNothing) {
    for (const auto& c : must_still_parse()) {
        EXPECT_NO_THROW((void)parse_document(c.json)) << c.name;
    }
}

TEST(LexicalConformanceTest, RawWhitespaceControlCharacters) {
    // Tabs, newlines and CR are the only control characters allowed OUTSIDE strings.
    EXPECT_NO_THROW((void)parse_document("[\t\n\r 1]"));
    EXPECT_THROW((void)parse_document("[\x0b1]"), std::runtime_error); // vertical tab
    EXPECT_THROW((void)parse_document("[\x0c1]"), std::runtime_error); // formfeed
    EXPECT_THROW((void)parse_document("[\x001]"), std::runtime_error); // NUL
}

TEST(LexicalConformanceTest, NothingIsDroppedFromAnAcceptedDocument) {
    // The old default path lost the backslash of an unrecognised escape (`"\U0041"` was
    // stored as `U0041`). Rule 3 rejects that input, so the interesting property is now
    // that every ACCEPTED document keeps what it read: an escaped backslash survives as
    // a backslash, and a serialization round trip returns an equal document.
    const auto doc = parse_document(R"("\\u0041")");
    ASSERT_TRUE(doc.is_string());
    EXPECT_EQ(doc.as<std::string>(), "\\u0041"); // backslash kept

    for (const auto& c : must_still_parse()) {
        const auto parsed = parse_document(c.json);
        const auto reparsed = parse_document(parsed.to_json());
        EXPECT_EQ(reparsed, parsed) << "round trip changed identity for " << c.name;
    }
}

TEST(LexicalConformanceTest, TheSuiteCorpusItselfAgrees) {
    // Same rules, but read from the vendored suite so the test cannot drift from the
    // corpus. Only the files JSOM used to accept are listed — the rest already passed.
    // The corpus lives in the SOURCE tree, so the path is baked in at configure time:
    // a relative path made this test depend on the working directory, which passed from
    // the project root and failed under `ctest` (which runs from the build directory).
    const std::string dir
        = std::string{JSOM_SOURCE_DIR} + "/third_party/json_test_suite/test_parsing/";
    for (const char* name :
         {"n_string_unescaped_ctrl_char.json", "n_string_unescaped_newline.json",
          "n_string_invalid_unicode_escape.json", "n_string_invalid_backslash_esc.json",
          "n_string_unicode_CapitalU.json", "n_structure_whitespace_formfeed.json"}) {
        std::ifstream in(dir + name, std::ios::binary);
        ASSERT_TRUE(in) << "missing suite file " << (dir + name);
        const std::string text{std::istreambuf_iterator<char>{in},
                               std::istreambuf_iterator<char>{}};
        EXPECT_THROW((void)parse_document(text), std::runtime_error)
            << name << " (suite requires rejection)";
    }
}
