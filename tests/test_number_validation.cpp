// Number validation — the opt-in strictness switch (CONFORMANCE.md Finding 2).
//
// 25 of JSOM's 42 conformance disagreements are malformed numbers, and the reason is
// structural rather than sloppy: numbers are stored lazily (LazyNumber keeps the
// original text), so the number grammar is never enforced during the scan and `-01`,
// `1.0.`, `2.e+3`, `0e+`, `[-]` are all accepted as documents. Turn something like that
// on by default and every caller pays for validation they may not want; leave it off
// and the documented contract is "we accept these as extensions" rather than "we never
// looked" (RFC 8259 §9 permits a parser to accept non-JSON forms, so lenient is
// defensible — but it has to be a decision, which is what this switch makes it).
//
// The switch changes VALIDATION ONLY. LazyNumber still stores the original text, so
// round-trip fidelity and the laziness of conversion are untouched — proved below.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "jsom/jsom.hpp"

using namespace jsom;

namespace {

/// The conformance suite's number cases JSOM currently accepts, verbatim
/// (nst/JSONTestSuite: `n_number_*` plus `n_array_just_minus`).
const std::vector<std::string>& malformed_numbers() {
    static const std::vector<std::string> cases = {
        "[-]",     "[0.1.2]", "[-01]",  "[0.3e+]", "[0.3e]",  "[0E+]",  "[0E]",
        "[0.e1]",  "[0e+]",   "[0e]",   "[1.0e+]", "[1.0e-]", "[1.0e]", "[1eE2]",
        "[2.e+3]", "[2.e-3]", "[2.e3]", "[-2.]",   "[9.e+]",  "[1+2]",  "[0e+-1]",
        "[-012]",  "[-.123]", "[1.]",   "[012]",
    };
    return cases;
}

/// Numbers the grammar does allow, including every shape the malformed list gets wrong.
const std::vector<std::string>& valid_numbers() {
    static const std::vector<std::string> cases = {
        "[0]",      "[-0]",       "[1]",   "[-1]",   "[12345]", "[0.5]",
        "[-0.5]",   "[1.0]",      "[1e0]", "[1E0]",  "[1e+0]",  "[1e-0]",
        "[1.5e10]", "[-1.5E-10]", "[0e0]", "[0e+0]", "[1e10]",  "[123.456e-789]",
    };
    return cases;
}

auto strict_numbers() -> JsonParseOptions {
    JsonParseOptions options;
    options.validate_numbers = true;
    return options;
}

/// Every case must be rejected *because of the number*, with the documented error.
void expect_number_rejection(const std::string& json) {
    try {
        const auto doc = parse_document(json, strict_numbers());
        ADD_FAILURE() << "expected a number rejection for " << json << ", got " << doc.to_json();
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find("Invalid number"), std::string::npos)
            << "unexpected error message for " << json << ": " << error.what();
    }
}

} // namespace

TEST(NumberValidationTest, LenientByDefaultIsDeliberate) {
    // Today's behaviour, pinned so it stays a decision rather than an accident: the
    // default accepts these as extensions, at full speed.
    for (const auto& json : malformed_numbers()) {
        EXPECT_NO_THROW((void)parse_document(json, ParsePresets::Default)) << json;
    }
    EXPECT_FALSE(ParsePresets::Default.validate_numbers);
}

TEST(NumberValidationTest, StrictModeRejectsEverySuiteNumberCase) {
    for (const auto& json : malformed_numbers()) {
        expect_number_rejection(json);
    }
}

TEST(NumberValidationTest, StrictModeAcceptsTheGrammar) {
    for (const auto& json : valid_numbers()) {
        EXPECT_NO_THROW((void)parse_document(json, strict_numbers())) << json;
    }
}

TEST(NumberValidationTest, StrictModeRejectsInsideContainers) {
    expect_number_rejection(R"({"a": 01})");
    expect_number_rejection(R"({"a": [1.0., 2]})");
    expect_number_rejection(R"([1, 2.e3])");
    expect_number_rejection("{\"a\":-}");
    EXPECT_NO_THROW((void)parse_document(R"({"a": [1.5e-3, -0.25, 0]})", strict_numbers()));
}

TEST(NumberValidationTest, StrictModeStillRejectsTrailingGarbage) {
    EXPECT_THROW((void)parse_document("[1]x", strict_numbers()), std::runtime_error);
    EXPECT_THROW((void)parse_document("[1,]", strict_numbers()), std::runtime_error);
}

TEST(NumberValidationTest, StrictModeKeepsTheOriginalText) {
    // Validation must not force conversion: the document still carries the number bytes
    // that came in, so a round trip reproduces them exactly (the LazyNumber promise).
    for (const auto& json : {"1.500", "1e10", "1E+10", "-0.0", "0.000", "-0e-0"}) {
        const auto doc = parse_document(json, strict_numbers());
        EXPECT_EQ(doc.to_json(), json) << json;
    }
    // Inside a container the *numbers* survive verbatim; insignificant whitespace does
    // not (compact serialization), which is long-standing behaviour, not a strictness
    // side effect.
    const auto doc = parse_document(R"({"a": 1.500, "b": [1e10, 0.0]})", strict_numbers());
    EXPECT_EQ(doc.to_json(), R"({"a":1.500,"b":[1e10,0.0]})");
}

TEST(NumberValidationTest, TheStrictPresetCarriesTheSwitch) {
    EXPECT_TRUE(ParsePresets::Validate.validate_numbers);
    EXPECT_EQ(ParsePresets::Validate.max_depth, limits::MAX_NESTING_DEPTH);
    EXPECT_EQ(ParsePresets::Validate.convert_unicode_escapes,
              ParsePresets::Default.convert_unicode_escapes);
    EXPECT_NO_THROW((void)parse_document("[1.5, 2e3]", ParsePresets::Validate));
    EXPECT_THROW((void)parse_document("[1.5, 2eE3]", ParsePresets::Validate), std::runtime_error);
}

TEST(NumberValidationTest, TheSwitchIsIndependentOfTheOtherOptions) {
    JsonParseOptions options;
    options.validate_numbers = true;
    options.convert_unicode_escapes = true;
    options.allow_comments = true;
    options.max_depth = 8;
    const std::string nine_deep = std::string(9, '[') + std::string(9, ']');
    EXPECT_NO_THROW((void)parse_document("[1, /* c */ 2]", options));
    EXPECT_THROW((void)parse_document("[01]", options), std::runtime_error);
    EXPECT_THROW((void)parse_document(nine_deep, options), std::runtime_error);
}
