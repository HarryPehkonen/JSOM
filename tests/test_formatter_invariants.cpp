// INVARIANTS of the formatting engine.
//
// The formatting engine (include/jsom/json_formatter.hpp) had 62 of its 328 executable
// lines covered when this file was written: the tests serialized through
// JsonDocument::to_json() almost exclusively, and the layout engine — presets, inlining,
// wrapping, alignment, escaping — was never exercised. These tests assert the properties
// that must hold for ANY option set, so they catch a regression in a code path whose exact
// byte-for-byte output nobody wants to pin down:
//
//   1. formatting never changes what a document MEANS: re-parsing the output gives an
//      equal document (unless the caller asked for non-standard JSON on purpose);
//   2. formatting is idempotent: format(format(x)) == format(x);
//   3. the output of every preset is valid JSON that parses.
#include <gtest/gtest.h>
#include <jsom/jsom.hpp>
#include <jsom/json_formatter.hpp>
#include <string>
#include <utility>
#include <vector>

namespace {

struct Document {
    const char* name;
    const char* json;
};

// A corpus that exercises the shape of the formatter, not just simple scalars: empty
// containers, containers of containers, the escape table, preserved \uXXXX sequences,
// non-ASCII text, number fidelity, a key longer than any sane width, and both a wide flat
// array and a wide array of objects (the inlining/wrapping decision points).
const std::vector<Document>& corpus() {
    static const std::vector<Document> docs = {
        {"null", "null"},
        {"true", "true"},
        {"false", "false"},
        {"integer", "42"},
        {"fraction-keeps-trailing-zeros", "1.500"},
        {"exponent", "1.5e10"},
        {"negative-zero", "-0.0"},
        {"huge-integer", "123456789012345678901234567890"},
        {"negative", "-17.25"},
        {"empty-string", "\"\""},
        {"plain-string", "\"hello world\""},
        {"escaped-newline", "\"line\\nbreak\""},
        {"escaped-tab", "\"tab\\there\""},
        {"escaped-quote", "\"say \\\"hi\\\"\""},
        {"escaped-backslash", "\"back\\\\slash\""},
        {"escaped-slash", "\"a\\/b\""},
        {"preserved-unicode-escape", "\"\\u4e2d\\u6587\""},
        {"preserved-unicode-escape-upper", "\"\\u00E4\""},
        {"non-ascii-text", "\"häh, miä ja 😀\""},
        {"unicode-key-and-value", "{\"ключ\":\"значение\"}"},
        {"empty-object", "{}"},
        {"empty-array", "[]"},
        {"nested-empties", "{\"a\":[],\"b\":{},\"c\":[[],{},[{}]]}"},
        {"scalar-object", "{\"a\":1,\"b\":\"x\",\"c\":true,\"d\":null}"},
        {"short-array", "[1,2,3]"},
        {"wide-scalar-array", "[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20]"},
        {"wide-string-array",
         "[\"alpha\",\"bravo\",\"charlie\",\"delta\",\"echo\",\"foxtrot\",\"golf\",\"hotel\","
         "\"india\",\"juliet\",\"kilo\",\"lima\"]"},
        {"array-of-objects", "[{\"a\":1},{\"b\":2},{\"c\":3}]"},
        {"array-of-arrays", "[[1,2],[3,4],[]]"},
        {"object-in-object", "{\"outer\":{\"inner\":{\"leaf\":[1,2,{\"deep\":true}]}}}"},
        {"long-key", "{\"a_very_long_key_name_that_goes_on_and_on\":1,\"k\":2}"},
        {"mixed-width-keys", "{\"id\":1,\"description\":\"a longer value here\",\"n\":2}"},
        {"whitespace-heavy-input", "  { \"a\" : [ 1 , 2 ] , \"b\" : { }  }  "},
        {"deep-nesting", "[[[[[[[[[[[[[[[[[[[[1]]]]]]]]]]]]]]]]]]]]"},
        {"long-string-value",
         "\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
         "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
         "aaaa\""},
    };
    return docs;
}

const std::vector<std::pair<const char*, jsom::JsonFormatOptions>>& presets() {
    static const std::vector<std::pair<const char*, jsom::JsonFormatOptions>> all = {
        {"Compact", jsom::FormatPresets::Compact}, {"Pretty", jsom::FormatPresets::Pretty},
        {"Config", jsom::FormatPresets::Config},   {"Api", jsom::FormatPresets::Api},
        {"Debug", jsom::FormatPresets::Debug},
    };
    return all;
}

auto format_with(const jsom::JsonFormatOptions& options, const std::string& json) -> std::string {
    const auto doc = jsom::parse_document(json);
    return jsom::JsonFormatter{options}.format(doc);
}

/// escape_unicode writes non-ASCII text as \uXXXX escapes. A reader that follows RFC 8259
/// decodes those escapes; JSOM's DEFAULT parse mode deliberately keeps them exactly as
/// written (that is what makes round-trip fidelity possible), so the round trip is checked
/// with unicode conversion enabled — the option's contract — and the SAME parse options are
/// used on both sides so the comparison is like for like.
auto parse_options_for(const jsom::JsonFormatOptions& format) -> jsom::JsonParseOptions {
    jsom::JsonParseOptions options;
    options.convert_unicode_escapes = format.escape_unicode;
    return options;
}

} // namespace

// Property 1: what goes in comes out. Formatting is a layout decision, so re-parsing the
// output must give a document equal to the original, for every preset and every shape.
TEST(FormatterInvariantsTest, EveryPresetPreservesTheDocument) {
    for (const auto& [preset_name, options] : presets()) {
        for (const auto& doc : corpus()) {
            const auto parse_options = parse_options_for(options);
            const auto original = jsom::parse_document(doc.json, parse_options);
            const std::string formatted = jsom::JsonFormatter{options}.format(original);
            jsom::JsonDocument reparsed;
            ASSERT_NO_THROW(reparsed = jsom::parse_document(formatted, parse_options))
                << preset_name << " / " << doc.name << " produced unparseable output:\n"
                << formatted;
            EXPECT_EQ(reparsed, original)
                << preset_name << " / " << doc.name
                << " changed the document.\nInput:  " << doc.json << "\nOutput: " << formatted;
        }
    }
}

// Property 2: formatting a formatted document changes nothing. This is what lets a tool
// reformat a file repeatedly (editors, diffs, pre-commit hooks) without churn.
TEST(FormatterInvariantsTest, EveryPresetIsIdempotent) {
    for (const auto& [preset_name, options] : presets()) {
        for (const auto& doc : corpus()) {
            const auto parse_options = parse_options_for(options);
            const auto original = jsom::parse_document(doc.json, parse_options);
            const std::string once = jsom::JsonFormatter{options}.format(original);
            const std::string twice
                = jsom::JsonFormatter{options}.format(jsom::parse_document(once, parse_options));
            EXPECT_EQ(twice, once)
                << preset_name << " / " << doc.name << " is not idempotent.\nFirst:  " << once
                << "\nSecond: " << twice;
        }
    }
}

// Property 3: every preset emits valid JSON. Round-tripping (property 1) proves this for
// the corpus, so this test only adds the cheapest possible check over the same inputs on
// the JSON mini-language the presets are meant for.
TEST(FormatterInvariantsTest, EveryPresetEmitsValidJson) {
    for (const auto& [preset_name, options] : presets()) {
        for (const auto& doc : corpus()) {
            const std::string formatted = format_with(options, doc.json);
            EXPECT_NO_THROW((void)jsom::parse_document(formatted))
                << preset_name << " / " << doc.name << ":\n"
                << formatted;
        }
    }
}

// The formatter is not only reachable by constructing one: JsonDocument::to_json(options)
// routes here, which is how most users meet it. Asserted so the two entry points cannot
// drift apart (a preset applied through to_json must mean the same as through JsonFormatter).
TEST(FormatterInvariantsTest, ToJsonWithOptionsGoesThroughTheFormatter) {
    for (const char* json : {R"({"a":1,"b":[1,2,3],"c":{"d":"x"}})", R"([1,2,3,4,5,6,7,8,9,10])"}) {
        const auto doc = jsom::parse_document(json);
        for (const auto& [preset_name, options] : presets()) {
            EXPECT_EQ(doc.to_json(options), jsom::JsonFormatter{options}.format(doc))
                << "to_json(options) and JsonFormatter disagree for " << preset_name;
        }
    }
}

// Compact is "one line, no whitespace": that is its whole purpose, and a stray newline or
// indentation in it is a bug rather than a style choice.
TEST(FormatterInvariantsTest, CompactNeverEmitsNewlines) {
    for (const auto& doc : corpus()) {
        const std::string out = format_with(jsom::FormatPresets::Compact, doc.json);
        EXPECT_EQ(out.find('\n'), std::string::npos)
            << "Compact emitted a newline for " << doc.name << ":\n"
            << out;
    }
}

// The corollary of Compact: the presets that indent must actually indent non-trivial
// documents, or "pretty" is a lie. Skipped for documents that legitimately fit on one line.
TEST(FormatterInvariantsTest, IndentingPresetsUseMoreThanOneLineForStructuredDocuments) {
    for (const char* json : {"{\"outer\":{\"inner\":{\"leaf\":[1,2,{\"deep\":true}]}}}",
                             "{\"a\":{\"b\":{\"c\":1}}}", "[{\"a\":1},{\"b\":2}]"}) {
        const auto doc = jsom::parse_document(json);
        for (const auto& [preset_name, options] : presets()) {
            if (!options.indent_size.has_value()) {
                continue; // Compact: one line by design
            }
            const std::string out = jsom::JsonFormatter{options}.format(doc);
            EXPECT_NE(out.find('\n'), std::string::npos)
                << preset_name << " kept " << json << " on one line:\n"
                << out;
        }
    }
}
