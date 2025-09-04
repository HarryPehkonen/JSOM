#include "benchmark_utils.hpp"
#include <benchmark/benchmark.h>
#include <jsom/jsom.hpp>
#include <nlohmann/json.hpp>

// JSOM Parse-Only Benchmarks (compatible with Phase 2)
static void BM_JSOM_ParseSmall(benchmark::State& state) {
    auto json = benchmark_utils::get_small_json();
    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        auto doc = jsom::parse_document(json);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
                            * static_cast<int64_t>(json.size()));
}
BENCHMARK(BM_JSOM_ParseSmall);

static void BM_JSOM_ParseMedium(benchmark::State& state) {
    auto json = benchmark_utils::get_medium_json();
    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        auto doc = jsom::parse_document(json);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
                            * static_cast<int64_t>(json.size()));
}
BENCHMARK(BM_JSOM_ParseMedium);

static void BM_JSOM_ParseLarge(benchmark::State& state) {
    auto json = benchmark_utils::get_large_json();
    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        auto doc = jsom::parse_document(json);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
                            * static_cast<int64_t>(json.size()));
}
BENCHMARK(BM_JSOM_ParseLarge);

// Temporarily disabled deep nested test due to JSON generation issue
// static void BM_JSOM_ParseDeepNested(benchmark::State& state) {
//     auto json = benchmark_utils::get_deep_nested_json();
//     for (auto _ : state) {
//         auto doc = jsom::parse_document(json);
//         benchmark::DoNotOptimize(doc);
//     }
//     state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
//     static_cast<int64_t>(json.size()));
// }
// BENCHMARK(BM_JSOM_ParseDeepNested);

static void BM_JSOM_ParseNumberHeavy(benchmark::State& state) {
    auto json = benchmark_utils::get_number_heavy_json();
    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        auto doc = jsom::parse_document(json);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
                            * static_cast<int64_t>(json.size()));
}
BENCHMARK(BM_JSOM_ParseNumberHeavy);

// nlohmann::json comparison benchmarks
static void BM_Nlohmann_ParseSmall(benchmark::State& state) {
    auto json = benchmark_utils::get_small_json();
    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        auto doc = nlohmann::json::parse(json);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
                            * static_cast<int64_t>(json.size()));
}
BENCHMARK(BM_Nlohmann_ParseSmall);

static void BM_Nlohmann_ParseMedium(benchmark::State& state) {
    auto json = benchmark_utils::get_medium_json();
    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        auto doc = nlohmann::json::parse(json);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
                            * static_cast<int64_t>(json.size()));
}
BENCHMARK(BM_Nlohmann_ParseMedium);

static void BM_Nlohmann_ParseLarge(benchmark::State& state) {
    auto json = benchmark_utils::get_large_json();
    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        auto doc = nlohmann::json::parse(json);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
                            * static_cast<int64_t>(json.size()));
}
BENCHMARK(BM_Nlohmann_ParseLarge);

// Temporarily disabled deep nested test due to JSON generation issue
// static void BM_Nlohmann_ParseDeepNested(benchmark::State& state) {
//     auto json = benchmark_utils::get_deep_nested_json();
//     for (auto _ : state) {
//         auto doc = nlohmann::json::parse(json);
//         benchmark::DoNotOptimize(doc);
//     }
//     state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
//     static_cast<int64_t>(json.size()));
// }
// BENCHMARK(BM_Nlohmann_ParseDeepNested);

static void BM_Nlohmann_ParseNumberHeavy(benchmark::State& state) {
    auto json = benchmark_utils::get_number_heavy_json();
    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        auto doc = nlohmann::json::parse(json);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
                            * static_cast<int64_t>(json.size()));
}
BENCHMARK(BM_Nlohmann_ParseNumberHeavy);
