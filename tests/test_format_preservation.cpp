#include <gtest/gtest.h>
#include <jsom/jsom.hpp>

using namespace jsom;

TEST(FormatPreservationTest, DecimalPreservation) {
    auto doc = parse_document(R"({"price": "1.0"})");
    std::string output = doc.to_json();

    EXPECT_TRUE(output.find("1.0") != std::string::npos);
    EXPECT_TRUE(output.find("1.0") != std::string::npos && output.find('1') == output.find("1.0"));
}

TEST(FormatPreservationTest, ScientificNotation) {
    auto doc = parse_document(R"({"large": "1e10", "small": "1e-5"})");
    std::string output = doc.to_json();

    EXPECT_TRUE(output.find("1e10") != std::string::npos);
    EXPECT_TRUE(output.find("1e-5") != std::string::npos);
}

TEST(FormatPreservationTest, IntegerFormatPreservation) {
    auto doc = parse_document(R"({"int": 42, "float": 42.0})");
    std::string output = doc.to_json();

    EXPECT_TRUE(output.find("\"int\":42") != std::string::npos);
    EXPECT_TRUE(output.find("\"float\":42.0") != std::string::npos);
}

TEST(FormatPreservationTest, MixedFormats) {
    auto doc = parse_document(R"({
        "integer": "123",
        "decimal": "123.456",
        "scientific": "1.23e2",
        "negative": "-456.789"
    })");

    std::string output = doc.to_json();

    EXPECT_TRUE(output.find("123") != std::string::npos);
    EXPECT_TRUE(output.find("123.456") != std::string::npos);
    EXPECT_TRUE(output.find("1.23e2") != std::string::npos);
    EXPECT_TRUE(output.find("-456.789") != std::string::npos);
}

TEST(FormatPreservationTest, AccessPreservesFormat) {
    auto doc = parse_document(R"({"value": 1.0})");

    auto numeric_value = doc["value"].as<double>();
    EXPECT_EQ(numeric_value, 1.0);

    std::string output = doc.to_json();
    EXPECT_TRUE(output.find("1.0") != std::string::npos);
}

TEST(FormatPreservationTest, ModifiedValueUseComputedFormat) {
    auto doc = parse_document(R"({"value": "1.0"})");

    // NOLINTNEXTLINE(readability-magic-numbers)
    JsonDocument modified{{"value", JsonDocument(2.5)}};
    std::string output = modified.to_json();

    EXPECT_TRUE(output.find("2.5") != std::string::npos);
}

TEST(FormatPreservationTest, ZeroFormats) {
    auto doc = parse_document(R"({
        "zero1": 0,
        "zero2": 0.0,
        "zero3": 0.00
    })");

    std::string output = doc.to_json();

    size_t zero_pos = output.find("\"zero1\":0");
    size_t zero_decimal_pos = output.find("\"zero2\":0.0");
    size_t zero_double_decimal_pos = output.find("\"zero3\":0.00");

    EXPECT_NE(zero_pos, std::string::npos);
    EXPECT_NE(zero_decimal_pos, std::string::npos);
    EXPECT_NE(zero_double_decimal_pos, std::string::npos);
}
