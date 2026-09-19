// Performance regressions, without the flakiness.
//
// These tests exist to catch an ORDER-OF-MAGNITUDE regression (a hot path that suddenly
// does per-element work it used to avoid), not to measure the library. Two rules keep them
// from failing for reasons that have nothing to do with the code:
//
//   1. Sample with min-of-N (`measure_best_ms`), never a single wall-clock reading. The
//      minimum is the least-interfered sample, so machine load cannot inflate it.
//   2. Compare against a ceiling with two orders of magnitude of headroom, never two timings
//      against each other. The old `RepeatedAccessCaching` test did exactly that
//      (`EXPECT_LT(second_access, first_access * 1.5)`) and failed for real in a full CI run
//      while the machine was busy building: a ratio of two single samples is noise.
//
// Real performance measurement lives in tools/perf_probe.cpp, interleaved A/B, on the shapes
// OPTIMIZATIONS.md tracks. Numbers from these tests should never be quoted as performance.
#include <algorithm>
#include <chrono>
#include <gtest/gtest.h>
#include <jsom/jsom.hpp>
#include <vector>

using namespace jsom;

namespace {

/// Multiplier to convert integer indices to varied decimal numbers, so number parsing sees
/// values like 0.0, 1.5, 3.0, 4.5 ... rather than a run of integers.
constexpr double NUMBER_VARIATION_MULTIPLIER = 1.5;

auto create_number_heavy_json(std::size_t count) -> std::string {
    std::ostringstream oss;
    oss << "{\"numbers\":[";
    for (std::size_t i = 0; i < count; ++i) {
        if (i > 0) {
            oss << ",";
        }
        oss << (static_cast<double>(i) * NUMBER_VARIATION_MULTIPLIER);
    }
    oss << "]}";
    return oss.str();
}

/// Milliseconds for one call of `func`.
template <typename Func> auto measure_once_ms(Func&& func) -> double {
    const auto start = std::chrono::steady_clock::now();
    func();
    const auto end = std::chrono::steady_clock::now();
    return static_cast<double>(
               std::chrono::duration_cast<std::chrono::microseconds>(end - start).count())
           / 1000.0;
}

/// Best of `samples` runs: the least-interfered measurement, which is what a load spike
/// cannot distort. Three samples is enough to reject an outlier on a shared machine.
template <typename Func> auto measure_best_ms(Func&& func, int samples = 3) -> double {
    std::vector<double> timings;
    timings.reserve(static_cast<std::size_t>(samples));
    for (int i = 0; i < samples; ++i) {
        timings.push_back(measure_once_ms(func));
    }
    return *std::min_element(timings.begin(), timings.end());
}

} // namespace

TEST(PerformanceRegressionTest, ParseOnlyPerformance) {
    const std::string json = create_number_heavy_json(1000);
    const double best_ms = measure_best_ms([&]() { auto doc = parse_document(json); });
    EXPECT_LT(best_ms, 100.0); // measured ~1 ms: a 100x margin
}

TEST(PerformanceRegressionTest, ParseSerializePerformance) {
    const std::string json = create_number_heavy_json(100);
    const double best_ms = measure_best_ms([&]() {
        auto doc = parse_document(json);
        [[maybe_unused]] auto output = doc.to_json();
    });
    EXPECT_LT(best_ms, 50.0); // measured well under 1 ms
}

TEST(PerformanceRegressionTest, NumberAccessPerformance) {
    auto doc = parse_document(create_number_heavy_json(100));
    const double best_ms = measure_best_ms([&]() {
        for (std::size_t i = 0; i < 100; ++i) {
            (void)doc["numbers"][i].as<double>();
        }
    });
    EXPECT_LT(best_ms, 50.0); // measured in microseconds; the ceiling only catches O(n) work
}

TEST(PerformanceRegressionTest, RepeatedAccessIsCorrectAndStaysCheap) {
    // 10,000 accesses of the same member. The old version of this test compared the second
    // batch's wall-clock against the first batch's and demanded it be within 1.5x — which
    // flakes on a loaded machine and measures the scheduler as much as the library. What is
    // asserted now: every access returns the right value, and the batch stays far below a
    // ceiling that only a pathological change (per-access parsing, an accidental copy of the
    // document) could cross.
    auto doc = parse_document(R"({"value": 123.456})");

    constexpr int iterations = 10000;
    const double best_ms = measure_best_ms([&]() {
        for (int i = 0; i < iterations; ++i) {
            const double value = doc["value"].as<double>();
            EXPECT_DOUBLE_EQ(value, 123.456);
        }
    });
    EXPECT_LT(best_ms, 250.0); // 25 us per access; measured ~0.1-1 us per access
}

TEST(PerformanceRegressionTest, DeepNestingParseIsLinear) {
    // Regression (OPTIMIZATIONS.md #1): array elements used to be deep-COPIED into the
    // result (no rvalue set() overload existed), making deeply nested documents O(depth^2).
    // Depth 3000 previously took ~200ms+; with the move overload it parses in ~1ms. The
    // generous ceiling separates the two by two orders of magnitude.
    //
    // 3000 levels is far past the default nesting limit (limits::MAX_NESTING_DEPTH), so the
    // limit is raised on purpose: this test is about the asymptotic cost of depth, and a test
    // thread's stack has room for 3000 levels (~1 MB measured).
    JsonParseOptions options;
    options.max_depth = 3000;

    std::string json;
    json.reserve(6001);
    for (int i = 0; i < 3000; ++i) {
        json += '[';
    }
    json += "42";
    for (int i = 0; i < 3000; ++i) {
        json += ']';
    }

    const double best_ms = measure_best_ms([&]() {
        auto doc = parse_document(json, options);
        (void)doc;
    });
    EXPECT_LT(best_ms, 100.0);
}
