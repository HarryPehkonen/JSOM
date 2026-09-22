// WIDENED property net for the formatter's layout policy (documents x options).
//
// This file exists to be in place BEFORE include/jsom/json_formatter.hpp is refactored
// into a single layout decision point (see the "one decision point" work tracked against
// determine_array_format_strategy / format_array_elements_with_wrapping /
// check_array_fits_on_line). tests/test_formatter_invariants.cpp and
// tests/test_formatter_options.cpp already assert these properties over a modest corpus; this file
// widens BOTH axes so a refactor that changes behaviour anywhere in the layout surface has
// somewhere to fail:
//
//   documents: deep nesting (the parser's own maximum, and the depth at which indentation
//     first outgrows the line width), empty/single-element/nested containers, very long
//     keys and strings, a wide flat array, unicode text, and the fuzz corpus already
//     archived under fuzz/regressions/ (skipping any file that is itself a REJECTION
//     regression rather than a formatter input — read_json_if_valid() below).
//   options: all 5 presets, plus one explicit-override variant per option named in the
//     brief (indent_size, max_line_width, max_inline_array_size, max_inline_object_size,
//     align_values, colon_spacing, bracket_spacing, intelligent_wrapping, trailing_comma,
//     escape_unicode).
//
// All of this passes on the pre-refactor tree (the width-underflow clamp landed in 3.1.2,
// commit 1c7131a) — this file only makes the safety net wider, it does not change behaviour.
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <jsom/jsom.hpp>
#include <jsom/json_formatter.hpp>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using jsom::FormatPresets;
using jsom::JsonFormatOptions;

struct Document {
    std::string name;
    std::string json;
    // false when JsonDocument::operator==() cannot be trusted on THIS document — see
    // "deep-nesting-at-parser-max" below. Every other property (no throw, reparses,
    // idempotent-as-text, no raw control byte) still runs.
    bool comparable = true;
};

struct OptionVariant {
    std::string name;
    JsonFormatOptions options;
    // false for a variant whose OWN documented behaviour is to emit non-standard JSON
    // (trailing_comma=true — FORMATTING.md calls it out explicitly), so re-parsing its
    // output is expected to fail rather than round-trip. Formatting itself must still
    // never throw for these, which is why they stay in option_variants() and in
    // FormattingNeverThrows / OutputHasNoRawControlByteOtherThanTheLineBreak — only the
    // round-trip-shaped properties (reparse-equals, idempotence) skip them.
    bool round_trippable = true;
};

auto nested_arrays(int depth, const std::string& leaf) -> std::string {
    return std::string(static_cast<std::size_t>(depth), '[') + leaf
           + std::string(static_cast<std::size_t>(depth), ']');
}

auto read_file(const std::string& path) -> std::string {
    std::ifstream in(path, std::ios::binary);
    return std::string{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

/// Some files under fuzz/regressions/ pin a PARSE rejection (malformed input the parser
/// must reject), not a document the formatter ever sees. Those are skipped here rather
/// than asserted on, since re-parsing them is exactly what the parser's own regression
/// tests already cover.
auto read_json_if_valid(const std::string& path) -> std::pair<bool, std::string> {
    const std::string text = read_file(path);
    try {
        (void)jsom::parse_document(text);
        return {true, text};
    } catch (const std::exception&) {
        return {false, text};
    }
}

auto documents() -> const std::vector<Document>& {
    static const std::vector<Document> docs = [] {
        std::vector<Document> v = {
            {"empty-array", "[]"},
            {"empty-object", "{}"},
            {"single-element-array", "[1]"},
            {"single-element-object", "{\"a\":1}"},
            {"container-of-containers-array", "[[1,2],[3,4],[5,6]]"},
            {"container-of-containers-object", "{\"a\":{\"b\":{\"c\":1}}}"},
            {"array-of-objects", "[{\"x\":1},{\"y\":2},{\"z\":3}]"},
            {"very-long-key", "{\"" + std::string(300, 'k') + "\":1,\"b\":2}"},
            {"very-long-string-value", "{\"a\":\"" + std::string(500, 'x') + "\"}"},
            {"unicode-mixed", "[\"h\xc3\xa9llo\",\"\xe4\xb8\xad\xe6\x96\x87\",\"\xf0\x9f\x98\x80"
                              "\xf0\x9f\x8e\x89\",\"\xd8\xb9\xd8\xb1\xd8\xa8\xd9\x8a\"]"},
            // The parser's own documented maximum (jsom::limits::MAX_NESTING_DEPTH). Formatting
            // must not throw at a depth the parser itself accepts.
            //
            // NOT marked comparable: this is a genuine PRE-EXISTING bug found by widening the
            // net, orthogonal to the formatter (reported separately, not fixed here — out of
            // scope for the layout-policy refactor). JsonDocument::equals_at() checks the LEAF
            // value at level+1 relative to its deepest container (json_document.hpp ~line 513),
            // so a document with exactly MAX_NESTING_DEPTH levels of array nesting is checked
            // one level past the very constant it is checked against, and
            // check_traversal_depth() throws NestingDepthExceeded from inside operator==. The
            // parser accepts this document (it IS the documented maximum); comparing it to
            // itself already throws, with no formatting involved. Confirmed with gdb: 257 calls
            // to equals_at for 256 levels of array nesting.
            {"deep-nesting-at-parser-max", nested_arrays(jsom::limits::MAX_NESTING_DEPTH, "1"),
             /*comparable=*/false},
            // Deep enough that (depth+1)*indent_size exceeds every preset's max_line_width
            // (see FormatterOptionsTest.IndentationDeeperThanTheLineWidthDoesNotThrow for the
            // per-preset arithmetic) — the exact shape the 3.1.2 crash fix targets.
            {"deep-nesting-past-width-threshold",
             nested_arrays(70, "[1,2,3,4,5,6,7,8,9,10,11,12]")},
        };

        {
            std::string wide = "[";
            for (int i = 0; i < 200; ++i) {
                if (i > 0) {
                    wide += ",";
                }
                wide += std::to_string(i);
            }
            wide += "]";
            v.push_back({"wide-flat-array-of-numbers", wide});
        }

        const std::string fuzz_dir = std::string{JSOM_SOURCE_DIR} + "/fuzz/regressions/";
        for (const char* name :
             {"round-trip-malformed-escape.json", "deep-nesting-exceeds-line-width.json"}) {
            const auto [is_valid, text] = read_json_if_valid(fuzz_dir + name);
            if (is_valid) {
                v.push_back({std::string{"fuzz-regression-"} + name, text});
            }
        }
        return v;
    }();
    return docs;
}

/// All 5 presets, plus one explicit-override variant per option named in the brief. Each
/// override starts from a preset whose default is the OTHER value, so the variant actually
/// exercises the option rather than restating the preset's default.
auto option_variants() -> const std::vector<OptionVariant>& {
    static const std::vector<OptionVariant> variants = [] {
        std::vector<OptionVariant> v = {
            {"Compact", FormatPresets::Compact, true}, {"Pretty", FormatPresets::Pretty, true},
            {"Config", FormatPresets::Config, true},   {"Api", FormatPresets::Api, true},
            {"Debug", FormatPresets::Debug, true},
        };

        auto add = [&v](std::string name, JsonFormatOptions options, bool round_trippable = true) {
            v.push_back({std::move(name), std::move(options), round_trippable});
        };

        JsonFormatOptions o;

        o = FormatPresets::Pretty;
        o.indent_size = 0;
        add("Pretty+indent_size=0", o);
        o = FormatPresets::Pretty;
        o.indent_size = 10;
        add("Pretty+indent_size=10", o);

        o = FormatPresets::Pretty;
        o.max_line_width = 1;
        add("Pretty+max_line_width=1", o);
        o = FormatPresets::Debug;
        o.max_line_width = 0;
        add("Debug+max_line_width=0(unlimited)", o);

        o = FormatPresets::Pretty;
        o.max_inline_array_size = 0;
        add("Pretty+max_inline_array_size=0", o);
        o = FormatPresets::Config;
        o.max_inline_array_size = 1000;
        add("Config+max_inline_array_size=1000", o);

        o = FormatPresets::Pretty;
        o.max_inline_object_size = 0;
        add("Pretty+max_inline_object_size=0", o);
        o = FormatPresets::Api;
        o.max_inline_object_size = 1000;
        add("Api+max_inline_object_size=1000", o);

        o = FormatPresets::Compact;
        o.indent_size = 2;
        o.align_values = true;
        add("Compact+align_values=true", o);

        o = FormatPresets::Debug;
        o.colon_spacing = 0;
        add("Debug+colon_spacing=0", o);
        o = FormatPresets::Compact;
        o.colon_spacing = 2;
        add("Compact+colon_spacing=2", o);

        o = FormatPresets::Pretty;
        o.bracket_spacing = true;
        add("Pretty+bracket_spacing=true", o);
        o = FormatPresets::Api;
        o.bracket_spacing = false;
        add("Api+bracket_spacing=false", o);

        o = FormatPresets::Pretty;
        o.intelligent_wrapping = false;
        add("Pretty+intelligent_wrapping=false", o);
        o = FormatPresets::Config;
        o.intelligent_wrapping = true;
        add("Config+intelligent_wrapping=true", o);

        o = FormatPresets::Pretty;
        o.trailing_comma = true;
        add("Pretty+trailing_comma=true", o, /*round_trippable=*/false);

        o = FormatPresets::Compact;
        o.escape_unicode = true;
        add("Compact+escape_unicode=true", o);
        o = FormatPresets::Debug;
        o.escape_unicode = false;
        add("Debug+escape_unicode=false", o);

        return v;
    }();
    return variants;
}

auto parse_options_for(const JsonFormatOptions& format_options) -> jsom::JsonParseOptions {
    jsom::JsonParseOptions options;
    options.convert_unicode_escapes = format_options.escape_unicode;
    return options;
}

/// Parses `json` under `options`, or returns nullopt for the rare corpus entry that is not
/// valid input under these particular parse options (none currently, kept defensive since
/// the corpus mixes hand-written and fuzz-derived documents).
auto try_parse(const std::string& json, const jsom::JsonParseOptions& options)
    -> std::pair<bool, jsom::JsonDocument> {
    try {
        return {true, jsom::parse_document(json, options)};
    } catch (const std::exception&) {
        return {false, jsom::JsonDocument{}};
    }
}

} // namespace

TEST(FormatterLayoutMatrixTest, FormattingNeverThrows) {
    for (const auto& variant : option_variants()) {
        for (const auto& doc : documents()) {
            const auto [ok, parsed] = try_parse(doc.json, parse_options_for(variant.options));
            if (!ok) {
                continue;
            }
            EXPECT_NO_THROW((void)jsom::JsonFormatter{variant.options}.format(parsed))
                << variant.name << " / " << doc.name;
        }
    }
}

TEST(FormatterLayoutMatrixTest, EveryVariantPreservesTheDocument) {
    for (const auto& variant : option_variants()) {
        if (!variant.round_trippable) {
            continue; // e.g. trailing_comma=true: non-standard JSON by design, doesn't reparse
        }
        for (const auto& doc : documents()) {
            const auto parse_options = parse_options_for(variant.options);
            const auto [ok, original] = try_parse(doc.json, parse_options);
            if (!ok) {
                continue;
            }
            const std::string formatted = jsom::JsonFormatter{variant.options}.format(original);
            jsom::JsonDocument reparsed;
            ASSERT_NO_THROW(reparsed = jsom::parse_document(formatted, parse_options))
                << variant.name << " / " << doc.name << " produced unparseable output:\n"
                << formatted;
            if (doc.comparable) {
                EXPECT_EQ(reparsed, original)
                    << variant.name << " / " << doc.name << " changed the document";
            }
        }
    }
}

TEST(FormatterLayoutMatrixTest, EveryVariantIsIdempotent) {
    for (const auto& variant : option_variants()) {
        if (!variant.round_trippable) {
            continue; // its own formatted output isn't guaranteed to re-parse (see above)
        }
        for (const auto& doc : documents()) {
            const auto parse_options = parse_options_for(variant.options);
            const auto [ok, original] = try_parse(doc.json, parse_options);
            if (!ok) {
                continue;
            }
            const std::string once = jsom::JsonFormatter{variant.options}.format(original);
            const std::string twice = jsom::JsonFormatter{variant.options}.format(
                jsom::parse_document(once, parse_options));
            EXPECT_EQ(twice, once) << variant.name << " / " << doc.name << " is not idempotent";
        }
    }
}

// A raw control byte in the output (other than the '\n' the formatter itself uses as a line
// break) can only mean one thing: a bug let it leak past format_string()'s escaping, since
// every documented option keeps control characters escaped regardless of escape_unicode
// (see FormatterOptionsTest.ControlCharactersAreAlwaysEscaped).
TEST(FormatterLayoutMatrixTest, OutputHasNoRawControlByteOtherThanTheLineBreak) {
    for (const auto& variant : option_variants()) {
        for (const auto& doc : documents()) {
            const auto [ok, parsed] = try_parse(doc.json, parse_options_for(variant.options));
            if (!ok) {
                continue;
            }
            const std::string out = jsom::JsonFormatter{variant.options}.format(parsed);
            for (unsigned char c : out) {
                if (c == '\n') {
                    continue;
                }
                EXPECT_FALSE(c < 0x20 || c == 0x7F)
                    << variant.name << " / " << doc.name << " emitted raw byte "
                    << static_cast<int>(c) << " in:\n"
                    << out;
            }
        }
    }
}

// max_line_width = 0 must behave as "no limit", not as "limit of zero": the width check in
// the layout decision has to be skipped entirely, not merely produce a degenerate width of
// zero that then forces everything onto its own line.
TEST(FormatterLayoutMatrixTest, MaxLineWidthZeroKeepsWideContentInline) {
    std::string json = "[";
    for (int i = 0; i < 5; ++i) {
        if (i > 0) {
            json += ",";
        }
        json += "\"" + std::string(50, static_cast<char>('a' + i)) + "\"";
    }
    json += "]";

    JsonFormatOptions options = FormatPresets::Pretty;
    options.max_line_width = 0;
    const auto doc = jsom::parse_document(json);
    const std::string out = jsom::JsonFormatter{options}.format(doc);
    EXPECT_EQ(out.find('\n'), std::string::npos) << out;
}
