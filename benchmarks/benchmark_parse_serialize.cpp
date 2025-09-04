#include "benchmark_utils.hpp"
#include <benchmark/benchmark.h>
#include <jsom/jsom.hpp>
#include <nlohmann/json.hpp>

// JSOM Parse-Serialize Benchmarks (Primary Phase 3 Optimization Target)
static void BM_JSOM_ParseSerialize_Small(benchmark::State& state) {
    auto input = benchmark_utils::get_small_json();
    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        auto doc = jsom::parse_document(input);
        auto output = doc.to_json();
        benchmark::DoNotOptimize(output);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
                            * static_cast<int64_t>(input.size()));
}
BENCHMARK(BM_JSOM_ParseSerialize_Small);

static void BM_JSOM_ParseSerialize_Medium(benchmark::State& state) {
    auto input = benchmark_utils::get_medium_json();
    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        auto doc = jsom::parse_document(input);
        auto output = doc.to_json();
        benchmark::DoNotOptimize(output);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
                            * static_cast<int64_t>(input.size()));
}
BENCHMARK(BM_JSOM_ParseSerialize_Medium);

static void BM_JSOM_ParseSerialize_Large(benchmark::State& state) {
    auto input = benchmark_utils::get_large_json();
    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        auto doc = jsom::parse_document(input);
        auto output = doc.to_json();
        benchmark::DoNotOptimize(output);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
                            * static_cast<int64_t>(input.size()));
}
BENCHMARK(BM_JSOM_ParseSerialize_Large);

static void BM_JSOM_ParseSerialize_NumberHeavy(benchmark::State& state) {
    auto input = benchmark_utils::get_number_heavy_json();
    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        auto doc = jsom::parse_document(input);
        auto output = doc.to_json();
        benchmark::DoNotOptimize(output);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
                            * static_cast<int64_t>(input.size()));
}
BENCHMARK(BM_JSOM_ParseSerialize_NumberHeavy);

// nlohmann::json Parse-Serialize Comparison
static void BM_Nlohmann_ParseSerialize_Small(benchmark::State& state) {
    auto input = benchmark_utils::get_small_json();
    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        auto doc = nlohmann::json::parse(input);
        auto output = doc.dump();
        benchmark::DoNotOptimize(output);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
                            * static_cast<int64_t>(input.size()));
}
BENCHMARK(BM_Nlohmann_ParseSerialize_Small);

static void BM_Nlohmann_ParseSerialize_Medium(benchmark::State& state) {
    auto input = benchmark_utils::get_medium_json();
    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        auto doc = nlohmann::json::parse(input);
        auto output = doc.dump();
        benchmark::DoNotOptimize(output);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
                            * static_cast<int64_t>(input.size()));
}
BENCHMARK(BM_Nlohmann_ParseSerialize_Medium);

static void BM_Nlohmann_ParseSerialize_Large(benchmark::State& state) {
    auto input = benchmark_utils::get_large_json();
    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        auto doc = nlohmann::json::parse(input);
        auto output = doc.dump();
        benchmark::DoNotOptimize(output);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
                            * static_cast<int64_t>(input.size()));
}
BENCHMARK(BM_Nlohmann_ParseSerialize_Large);

static void BM_Nlohmann_ParseSerialize_NumberHeavy(benchmark::State& state) {
    auto input = benchmark_utils::get_number_heavy_json();
    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        auto doc = nlohmann::json::parse(input);
        auto output = doc.dump();
        benchmark::DoNotOptimize(output);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
                            * static_cast<int64_t>(input.size()));
}
BENCHMARK(BM_Nlohmann_ParseSerialize_NumberHeavy);
