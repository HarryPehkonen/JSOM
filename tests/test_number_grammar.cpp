// RFC 8259 section 6 is enforced BY DEFAULT in 4.0.0. The malformed forms below are the
// conformance suite's n_number_* cases (plus n_array_just_minus), which used to be
// accepted as extensions and are now rejected with "Invalid number: ...".
//
// The grammar check happens INSIDE the scan (no second pass), which measured 0.92x-0.98x
// -- i.e. slightly FASTER than the old permissive scan. See OPTIMIZATIONS.md.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "jsom/jsom.hpp"

using namespace jsom;

namespace {

/// Malformed numbers the suite requires rejecting, each inside an array so the number is
/// not the only thing the parser has to get right.
const std::vector<std::string>& malformed_numbers() {
    static const std::vector<std::string> cases = {
        "[-]",     "[0.1.2]", "[-01]",  "[0.3e+]", "[0.3e]",  "[0E+]",  "[0E]",
        "[0.e1]",  "[0e+]",   "[0e]",   "[1.0e+]", "[1.0e-]", "[1.0e]", "[1eE2]",
        "[2.e+3]", "[2.e-3]", "[2.e3]", "[-2.]",   "[9.e+]",  "[1+2]",  "[0e+-1]",
        "[-012]",  "[-.123]", "[1.]",   "[012]",   "[01]",    "[--1]",  "[1.2.3]",
    };
    return cases;
}

/// Numbers the grammar allows, including every shape the malformed list gets wrong.
const std::vector<std::string>& valid_numbers() {
    static const std::vector<std::string> cases = {
        "[0]",      "[-0]",       "[1]",   "[-1]",   "[12345]", "[0.5]",
        "[-0.5]",   "[1.0]",      "[1e0]", "[1E0]",  "[1e+0]",  "[1e-0]",
        "[1.5e10]", "[-1.5E-10]", "[0e0]", "[0e+0]", "[1e10]",  "[123.456e-789]",
    };
    return cases;
}

/// Rejection must come from the number grammar, with the documented message.
void expect_number_rejection(const std::string& json) {
    try {
        const auto doc = parse_document(json);
        ADD_FAILURE() << "expected rejection of " << json << ", got " << doc.to_json();
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find("Invalid number"), std::string::npos)
            << "wrong error for " << json << ": " << error.what();
    }
}

} // namespace

TEST(NumberGrammarTest, DefaultRejectsEverySuiteNumberCase) {
    for (const auto& json : malformed_numbers()) {
        expect_number_rejection(json);
    }
}

TEST(NumberGrammarTest, DefaultAcceptsTheGrammar) {
    for (const auto& json : valid_numbers()) {
        EXPECT_NO_THROW((void)parse_document(json)) << json;
    }
}

TEST(NumberGrammarTest, RejectionNamesTheOffendingText) {
    // The message should quote the run of number-ish characters, not just the character
    // that ended it, so a caller can see which token was rejected.
    try {
        (void)parse_document("[01]");
        ADD_FAILURE() << "expected rejection";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find("01"), std::string::npos) << error.what();
    }
}

TEST(NumberGrammarTest, DefaultRejectsInsideContainers) {
    expect_number_rejection(R"({"a": 01})");
    expect_number_rejection(R"({"a": [1.0., 2]})");
    expect_number_rejection(R"([1, 2.e3])");
    expect_number_rejection("{\"a\":-}");
    EXPECT_NO_THROW((void)parse_document(R"({"a": [1.5e-3, -0.25, 0]})"));
}

TEST(NumberGrammarTest, DefaultStillRejectsTrailingGarbage) {
    EXPECT_THROW((void)parse_document("[1]x"), std::runtime_error);
    EXPECT_THROW((void)parse_document("[1,]"), std::runtime_error);
    EXPECT_THROW((void)parse_document("{\"a\":}"), std::runtime_error);
}

TEST(NumberGrammarTest, DefaultKeepsTheOriginalText) {
    // Enforcing the grammar must not force conversion: the document still carries the
    // number bytes that came in, so a round trip reproduces them exactly.
    for (const auto& json : {"1.500", "1e10", "1E+10", "-0.0", "0.000", "-0e-0"}) {
        const auto doc = parse_document(json);
        EXPECT_EQ(doc.to_json(), json) << json;
    }
    const auto doc = parse_document(R"({"a": 1.500, "b": [1e10, 0.0]})");
    EXPECT_EQ(doc.to_json(), R"({"a":1.500,"b":[1e10,0.0]})");
}

TEST(NumberGrammarTest, TheGrammarIsIndependentOfTheOtherOptions) {
    JsonParseOptions options;
    options.convert_unicode_escapes = true;
    options.allow_comments = true;
    options.max_depth = 8;
    const std::string nine_deep = std::string(9, '[') + std::string(9, ']');
    EXPECT_NO_THROW((void)parse_document("[1, /* c */ 2]", options));
    EXPECT_THROW((void)parse_document("[01]", options), std::runtime_error);
    EXPECT_THROW((void)parse_document(nine_deep, options), std::runtime_error);
}
