#include <chrono>
#include <gtest/gtest.h>
#include <jsom/jsom.hpp>

using namespace jsom;

class PerformanceRegressionTest : public ::testing::Test {
protected:
    // Multiplier to convert integer indices to varied decimal numbers for testing
    // This creates numbers like: 0.0, 1.5, 3.0, 4.5, 6.0... to test decimal parsing performance
    static constexpr double NUMBER_VARIATION_MULTIPLIER = 1.5;

    static auto create_number_heavy_json(size_t count) -> std::string {
        std::ostringstream oss;
        oss << "{\"numbers\":[";
        for (size_t i = 0; i < count; ++i) {
            if (i > 0) {
                oss << ",";
            }
            oss << (static_cast<double>(i)
                    * NUMBER_VARIATION_MULTIPLIER); // Creates varied decimal numbers
        }
        oss << "]}";
        return oss.str();
    }

    template <typename Func> static auto measure_time_ms(Func&& func) -> double {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        // NOLINTNEXTLINE(readability-magic-numbers)
        return static_cast<double>(duration.count()) / 1000.0;
    }
};

TEST_F(PerformanceRegressionTest, ParseOnlyPerformance) {
    // NOLINTNEXTLINE(readability-magic-numbers)
    std::string json = create_number_heavy_json(1000);

    double time_ms = measure_time_ms([&]() { auto doc = parse_document(json); });

    EXPECT_LT(time_ms, 100.0);
}

TEST_F(PerformanceRegressionTest, ParseSerializePerformance) {
    // NOLINTNEXTLINE(readability-magic-numbers)
    std::string json = create_number_heavy_json(100);

    double time_ms = measure_time_ms([&]() {
        auto doc = parse_document(json);
        [[maybe_unused]] auto output = doc.to_json();
    });

    EXPECT_LT(time_ms, 50.0);
}

TEST_F(PerformanceRegressionTest, NumberAccessPerformance) {
    // NOLINTNEXTLINE(readability-magic-numbers)
    auto doc = parse_document(create_number_heavy_json(100));

    double time_ms = measure_time_ms([&]() {
        // NOLINTNEXTLINE(readability-magic-numbers)
        for (size_t i = 0; i < 100; ++i) {
            auto value = doc["numbers"][i].as<double>();
            (void)value;
        }
    });

    EXPECT_LT(time_ms, 10.0);
}

TEST_F(PerformanceRegressionTest, RepeatedAccessCaching) {
    auto doc = parse_document(R"({"value": 123.456})");

    // Measure multiple iterations to get measurable timing
    constexpr int iterations = 10000;

    double first_access = measure_time_ms([&]() {
        for (int i = 0; i < iterations; ++i) {
            auto value = doc["value"].as<double>();
            (void)value;
        }
    });

    double second_access = measure_time_ms([&]() {
        for (int i = 0; i < iterations; ++i) {
            auto value = doc["value"].as<double>();
            (void)value;
        }
    });

    // Second access should be faster due to caching, but allow for some variance
    // Since both should be cached after first iteration, expect them to be similar
    EXPECT_GT(first_access, 0.0);                 // Ensure we actually measured something
    EXPECT_GT(second_access, 0.0);                // Ensure we actually measured something
    EXPECT_LT(second_access, first_access * 1.5); // Allow 50% variance for timing noise
}

TEST_F(PerformanceRegressionTest, DeepNestingParseIsLinear) {
    // Regression (OPTIMIZATIONS.md #1): array elements used to be deep-COPIED
    // into the result (no rvalue set() overload existed), making deeply nested
    // documents O(depth^2). Depth 3000 previously took ~200ms+; with the move
    // overload the same document parses in ~1ms. The generous 100ms ceiling
    // separates the two by two orders of magnitude while staying immune to
    // machine and -O0 variance.
    std::string json;
    for (int i = 0; i < 3000; ++i) {
        json += '[';
    }
    json += "42";
    for (int i = 0; i < 3000; ++i) {
        json += ']';
    }

    double time_ms = measure_time_ms([&]() {
        auto doc = parse_document(json);
        (void)doc;
    });

    EXPECT_LT(time_ms, 100.0);
}
