// TDD for JsonFormatter::available_line_width(): the ONE place that turns
// (max_line_width, indent_size, depth) into the number of characters left on a line, the
// computation the 3.1.2 crash fix clamped in two copies (include/jsom/json_formatter.hpp,
// commit 1c7131a). The formatter's layout refactor pulls it out into a single named,
// directly-testable method instead of an inline block repeated at each call site — written
// RED first, against a method that does not exist yet.
#include <cstddef>
#include <gtest/gtest.h>
#include <jsom/jsom.hpp>
#include <jsom/json_formatter.hpp>
#include <limits>

using jsom::JsonFormatOptions;
using jsom::JsonFormatter;

namespace {

auto options_with(int indent_size, int max_line_width) -> JsonFormatOptions {
    JsonFormatOptions options;
    options.indent_size = indent_size;
    options.max_line_width = max_line_width;
    return options;
}

} // namespace

// The prefix (indent_size * depth) exactly consumes the line: nothing is left for content,
// and that is a valid, well-defined answer — not a wraparound.
TEST(AvailableLineWidthTest, PrefixExactlyConsumingTheWidthYieldsZero) {
    // depth 5, indent 4 -> prefix 20, max_line_width 20.
    // JsonFormatter stores its options by const reference (not a copy), so the options
    // object must outlive the formatter — kept as a named local, not a temporary handed
    // straight to the constructor.
    const JsonFormatOptions options = options_with(/*indent_size=*/4, /*max_line_width=*/20);
    const JsonFormatter formatter{options};
    EXPECT_EQ(formatter.available_line_width(5), 0U);
}

// The exact crash shape from 3.1.2: a prefix LONGER than max_line_width must clamp to zero,
// not underflow the unsigned subtraction (which used to wrap to ~2^64 and blow up
// std::string::reserve()).
TEST(AvailableLineWidthTest, PrefixLongerThanWidthClampsToZeroRatherThanUnderflowing) {
    // depth 100, indent 4 -> prefix 400, far past max_line_width 20.
    const JsonFormatOptions options = options_with(/*indent_size=*/4, /*max_line_width=*/20);
    const JsonFormatter formatter{options};
    EXPECT_EQ(formatter.available_line_width(100), 0U);
}

// max_line_width <= 0 means "no limit" everywhere else in this file's option surface
// (FORMATTING.md, FormatterOptionsTest.MaxLineWidthZeroMeansNoLimit); the width helper must
// agree, rather than callers having to special-case "unlimited" around it.
TEST(AvailableLineWidthTest, WidthUnlimitedWhenMaxLineWidthIsNotPositive) {
    const JsonFormatOptions zero_options = options_with(/*indent_size=*/4, /*max_line_width=*/0);
    const JsonFormatter zero{zero_options};
    EXPECT_EQ(zero.available_line_width(50), std::numeric_limits<std::size_t>::max());

    const JsonFormatOptions negative_options
        = options_with(/*indent_size=*/4, /*max_line_width=*/-1);
    const JsonFormatter negative{negative_options};
    EXPECT_EQ(negative.available_line_width(50), std::numeric_limits<std::size_t>::max());
}

// depth 0 (an empty/top-level container has nothing to indent into) means an empty prefix,
// so the full width is available — the degenerate case a container with no nesting hits.
TEST(AvailableLineWidthTest, DepthZeroHasNoPrefixSoTheFullWidthIsAvailable) {
    const JsonFormatOptions options = options_with(/*indent_size=*/4, /*max_line_width=*/80);
    const JsonFormatter formatter{options};
    EXPECT_EQ(formatter.available_line_width(0), 80U);
}

// Compact-shaped options (no indentation at all) never lose width to a prefix, regardless
// of depth.
TEST(AvailableLineWidthTest, NoIndentSizeMeansNoPrefixAtAnyDepth) {
    JsonFormatOptions options;
    options.indent_size = std::nullopt;
    options.max_line_width = 80;
    const JsonFormatter formatter{options};
    EXPECT_EQ(formatter.available_line_width(500), 80U);
}
