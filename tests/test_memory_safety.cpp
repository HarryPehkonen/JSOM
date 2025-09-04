#include <gtest/gtest.h>
#include <jsom/jsom.hpp>

using namespace jsom;

TEST(MemorySafetyTest, RAIICompliance) {
    {
        auto doc = parse_document(R"({
            "large_object": {
                "data": [1, 2, 3, 4, 5],
                "metadata": {
                    "size": 5,
                    "type": "array"
                }
            }
        })");

        EXPECT_TRUE(doc.is_object());
    }
}

TEST(MemorySafetyTest, CopySemantics) {
    auto doc1 = parse_document(R"({"number": 123.456})");
    auto doc2 = doc1;
    auto doc3 = std::move(doc1);

    auto value2 = doc2["number"].as<double>();
    auto value3 = doc3["number"].as<double>();

    EXPECT_EQ(value2, 123.456);
    EXPECT_EQ(value3, 123.456);
}

TEST(MemorySafetyTest, NestedStructureSafety) {
    auto doc = parse_document(R"({
        "level1": {
            "level2": {
                "level3": {
                    "level4": {
                        "value": "deep_value"
                    }
                }
            }
        }
    })");

    EXPECT_EQ(doc["level1"]["level2"]["level3"]["level4"]["value"].as<std::string>(), "deep_value");
}

TEST(MemorySafetyTest, LargeArraySafety) {
    std::ostringstream oss;
    oss << "[";
    // NOLINTNEXTLINE(readability-magic-numbers)
    for (int i = 0; i < 1000; ++i) {
        if (i > 0) {
            oss << ",";
        }
        oss << i;
    }
    oss << "]";

    auto doc = parse_document(oss.str());
    EXPECT_TRUE(doc.is_array());
    EXPECT_EQ(doc[999].as<int>(), 999);
}

TEST(MemorySafetyTest, StringEscapeSafety) {
    auto doc = parse_document(R"({"text": "Hello\nWorld\t\"Quote\""})");
    auto text = doc["text"].as<std::string>();

    EXPECT_TRUE(text.find('\n') != std::string::npos);
    EXPECT_TRUE(text.find('\t') != std::string::npos);
    EXPECT_TRUE(text.find('"') != std::string::npos);
}
