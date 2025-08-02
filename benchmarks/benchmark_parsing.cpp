#include <benchmark/benchmark.h>
#include "jsom.hpp"
#include <nlohmann/json.hpp>
#include "benchmark_utils.hpp"

// JSOM Parsing Benchmarks

static void BM_JSOM_ParseSmall(benchmark::State& state) {
    const std::string json = benchmark_utils::get_small_json();
    for (auto _ : state) {
        auto doc = jsom::parse_document(json);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(state.iterations() * json.size());
}
BENCHMARK(BM_JSOM_ParseSmall);

static void BM_JSOM_ParseMedium(benchmark::State& state) {
    const std::string json = benchmark_utils::get_medium_json();
    for (auto _ : state) {
        auto doc = jsom::parse_document(json);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(state.iterations() * json.size());
}
BENCHMARK(BM_JSOM_ParseMedium);

static void BM_JSOM_ParseLarge(benchmark::State& state) {
    const std::string json = benchmark_utils::get_large_json();
    for (auto _ : state) {
        auto doc = jsom::parse_document(json);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(state.iterations() * json.size());
}
BENCHMARK(BM_JSOM_ParseLarge);

static void BM_JSOM_ParseDeepNested(benchmark::State& state) {
    const std::string json = benchmark_utils::get_deep_nested_json();
    for (auto _ : state) {
        auto doc = jsom::parse_document(json);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(state.iterations() * json.size());
}
BENCHMARK(BM_JSOM_ParseDeepNested);

// Array-heavy JSON parsing
static void BM_JSOM_ParseArrayHeavy(benchmark::State& state) {
    std::string json = "[";
    for (int i = 0; i < 10000; ++i) {
        if (i > 0) json += ",";
        json += std::to_string(i);
    }
    json += "]";
    
    for (auto _ : state) {
        auto doc = jsom::parse_document(json);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(state.iterations() * json.size());
}
BENCHMARK(BM_JSOM_ParseArrayHeavy);

// Object-heavy JSON parsing
static void BM_JSOM_ParseObjectHeavy(benchmark::State& state) {
    std::string json = "{";
    for (int i = 0; i < 1000; ++i) {
        if (i > 0) json += ",";
        json += "\"key" + std::to_string(i) + "\":\"value" + std::to_string(i) + "\"";
    }
    json += "}";
    
    for (auto _ : state) {
        auto doc = jsom::parse_document(json);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(state.iterations() * json.size());
}
BENCHMARK(BM_JSOM_ParseObjectHeavy);

// Number-heavy JSON parsing
static void BM_JSOM_ParseNumberHeavy(benchmark::State& state) {
    const std::string json = benchmark_utils::get_number_heavy_json();
    for (auto _ : state) {
        auto doc = jsom::parse_document(json);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(state.iterations() * json.size());
}
BENCHMARK(BM_JSOM_ParseNumberHeavy);

// nlohmann::json Parsing Benchmarks

static void BM_Nlohmann_ParseSmall(benchmark::State& state) {
    const std::string json = benchmark_utils::get_small_json();
    for (auto _ : state) {
        auto doc = nlohmann::json::parse(json);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(state.iterations() * json.size());
}
BENCHMARK(BM_Nlohmann_ParseSmall);

static void BM_Nlohmann_ParseMedium(benchmark::State& state) {
    const std::string json = benchmark_utils::get_medium_json();
    for (auto _ : state) {
        auto doc = nlohmann::json::parse(json);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(state.iterations() * json.size());
}
BENCHMARK(BM_Nlohmann_ParseMedium);

static void BM_Nlohmann_ParseLarge(benchmark::State& state) {
    const std::string json = benchmark_utils::get_large_json();
    for (auto _ : state) {
        auto doc = nlohmann::json::parse(json);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(state.iterations() * json.size());
}
BENCHMARK(BM_Nlohmann_ParseLarge);

static void BM_Nlohmann_ParseDeepNested(benchmark::State& state) {
    const std::string json = benchmark_utils::get_deep_nested_json();
    for (auto _ : state) {
        auto doc = nlohmann::json::parse(json);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(state.iterations() * json.size());
}
BENCHMARK(BM_Nlohmann_ParseDeepNested);

static void BM_Nlohmann_ParseArrayHeavy(benchmark::State& state) {
    std::string json = "[";
    for (int i = 0; i < 10000; ++i) {
        if (i > 0) json += ",";
        json += std::to_string(i);
    }
    json += "]";
    
    for (auto _ : state) {
        auto doc = nlohmann::json::parse(json);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(state.iterations() * json.size());
}
BENCHMARK(BM_Nlohmann_ParseArrayHeavy);

static void BM_Nlohmann_ParseObjectHeavy(benchmark::State& state) {
    std::string json = "{";
    for (int i = 0; i < 1000; ++i) {
        if (i > 0) json += ",";
        json += "\"key" + std::to_string(i) + "\":\"value" + std::to_string(i) + "\"";
    }
    json += "}";
    
    for (auto _ : state) {
        auto doc = nlohmann::json::parse(json);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(state.iterations() * json.size());
}
BENCHMARK(BM_Nlohmann_ParseObjectHeavy);

// Error handling benchmarks
static void BM_JSOM_ParseInvalid(benchmark::State& state) {
    const std::string invalid_json = R"({"invalid": json, "missing": "quotes})";
    
    for (auto _ : state) {
        try {
            auto doc = jsom::parse_document(invalid_json);
            benchmark::DoNotOptimize(doc);
        } catch (...) {
            // Expected to throw
        }
    }
}
BENCHMARK(BM_JSOM_ParseInvalid);

static void BM_Nlohmann_ParseNumberHeavy(benchmark::State& state) {
    const std::string json = benchmark_utils::get_number_heavy_json();
    for (auto _ : state) {
        auto doc = nlohmann::json::parse(json);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(state.iterations() * json.size());
}
BENCHMARK(BM_Nlohmann_ParseNumberHeavy);

static void BM_Nlohmann_ParseInvalid(benchmark::State& state) {
    const std::string invalid_json = R"({"invalid": json, "missing": "quotes})";
    
    for (auto _ : state) {
        try {
            auto doc = nlohmann::json::parse(invalid_json);
            benchmark::DoNotOptimize(doc);
        } catch (...) {
            // Expected to throw
        }
    }
}
BENCHMARK(BM_Nlohmann_ParseInvalid);