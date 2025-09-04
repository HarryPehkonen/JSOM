#include "benchmark_utils.hpp"
#include <benchmark/benchmark.h>
#include <jsom/jsom.hpp>
#include <nlohmann/json.hpp>

// Format Preservation Benchmarks

// Test that parse-serialize preserves original number formats
static void BM_JSOM_FormatPreservation_ParseSerialize(benchmark::State& state) {
    const auto* json = R"({
        "integer": 42,
        "decimal": 123.456,
        "scientific": 1.23e10,
        "negative": -456.789,
        "zero": 0,
        "zero_decimal": 0.0
    })";

    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        auto doc = jsom::parse_document(json);
        auto output = doc.to_json();

        // Verify format preservation (in real benchmarks, this would be validation)
        benchmark::DoNotOptimize(output);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
                            * static_cast<int64_t>(strlen(json)));
}
BENCHMARK(BM_JSOM_FormatPreservation_ParseSerialize);

// Test lazy evaluation with format preservation under access
static void BM_JSOM_FormatPreservation_WithAccess(benchmark::State& state) {
    const auto* json = R"({
        "metrics": [
            {"value": 1.0, "count": 100},
            {"value": 2.5, "count": 200},
            {"value": 3.14159, "count": 300}
        ]
    })";

    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        auto doc = jsom::parse_document(json);

        // Access some values (triggers lazy evaluation)
        auto value0 = doc["metrics"][0]["value"].as<double>();
        auto count1 = doc["metrics"][1]["count"].as<int>();

        // Serialize should still preserve format for non-accessed values
        auto output = doc.to_json();

        benchmark::DoNotOptimize(value0);
        benchmark::DoNotOptimize(count1);
        benchmark::DoNotOptimize(output);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
                            * static_cast<int64_t>(strlen(json)));
}
BENCHMARK(BM_JSOM_FormatPreservation_WithAccess);

// Compare with nlohmann format handling
static void BM_Nlohmann_FormatHandling_ParseSerialize(benchmark::State& state) {
    const auto* json = R"({
        "integer": 42,
        "decimal": 123.456,
        "scientific": 1.23e10,
        "negative": -456.789,
        "zero": 0,
        "zero_decimal": 0.0
    })";

    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        auto doc = nlohmann::json::parse(json);
        auto output = doc.dump();

        benchmark::DoNotOptimize(output);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
                            * static_cast<int64_t>(strlen(json)));
}
BENCHMARK(BM_Nlohmann_FormatHandling_ParseSerialize);

// Scientific notation handling
static void BM_JSOM_ScientificNotation(benchmark::State& state) {
    const auto* json = R"({
        "large": 1.23e10,
        "small": 4.56e-8,
        "coefficient": 6.02e23,
        "planck": 6.626e-34
    })";

    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        auto doc = jsom::parse_document(json);

        // Access some scientific notation values
        auto large = doc["large"].as<double>();
        auto small = doc["small"].as<double>();

        auto output = doc.to_json();

        benchmark::DoNotOptimize(large);
        benchmark::DoNotOptimize(small);
        benchmark::DoNotOptimize(output);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
                            * static_cast<int64_t>(strlen(json)));
}
BENCHMARK(BM_JSOM_ScientificNotation);

// Large precision numbers
static void BM_JSOM_HighPrecisionNumbers(benchmark::State& state) {
    const auto* json = R"({
        "pi": 3.141592653589793,
        "e": 2.718281828459045,
        "phi": 1.618033988749895,
        "sqrt2": 1.414213562373095
    })";

    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        auto doc = jsom::parse_document(json);

        // Access high precision values
        // NOLINTNEXTLINE(readability-identifier-length)
        auto pi = doc["pi"].as<double>();
        // NOLINTNEXTLINE(readability-identifier-length)
        auto e = doc["e"].as<double>();

        auto output = doc.to_json();

        benchmark::DoNotOptimize(pi);
        benchmark::DoNotOptimize(e);
        benchmark::DoNotOptimize(output);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
                            * static_cast<int64_t>(strlen(json)));
}
BENCHMARK(BM_JSOM_HighPrecisionNumbers);
