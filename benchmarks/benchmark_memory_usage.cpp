#include "benchmark_utils.hpp"
#include <benchmark/benchmark.h>
#include <jsom/jsom.hpp>

namespace {
constexpr int DOCUMENT_COUNT = 100;
constexpr int ACCESS_COUNT = 10;
} // namespace
#include <nlohmann/json.hpp>

// Memory Usage Benchmarks

// Small numbers memory efficiency test
static void BM_JSOM_SmallNumbers(benchmark::State& state) {
    const auto* json = R"({
        "count": 1,
        "page": 2,
        "limit": 10,
        "offset": 0,
        "status": 200,
        "retry": 3
    })";

    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        auto doc = jsom::parse_document(json);

        // Access to verify functionality
        auto count = doc["count"].as<int>();
        auto page = doc["page"].as<int>();

        benchmark::DoNotOptimize(doc);
        benchmark::DoNotOptimize(count);
        benchmark::DoNotOptimize(page);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
                            * static_cast<int64_t>(strlen(json)));
}
BENCHMARK(BM_JSOM_SmallNumbers);

// Large numbers memory usage
static void BM_JSOM_LargeNumbers(benchmark::State& state) {
    const auto* json = R"({
        "id": 123456789012345,
        "timestamp": 1641024000000,
        "user_id": 987654321098765,
        "transaction_id": 555444333222111
    })";

    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        auto doc = jsom::parse_document(json);

        // NOLINTNEXTLINE(readability-identifier-length)
        auto id = doc["id"].as<long long>();
        auto timestamp = doc["timestamp"].as<long long>();

        benchmark::DoNotOptimize(doc);
        benchmark::DoNotOptimize(id);
        benchmark::DoNotOptimize(timestamp);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
                            * static_cast<int64_t>(strlen(json)));
}
BENCHMARK(BM_JSOM_LargeNumbers);

// Memory stress test with many documents
static void BM_JSOM_ManyDocuments(benchmark::State& state) {
    auto json = benchmark_utils::get_small_json();

    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        std::vector<jsom::JsonDocument> docs;
        docs.reserve(DOCUMENT_COUNT);

        // Create many document instances
        for (int i = 0; i < DOCUMENT_COUNT; ++i) {
            docs.push_back(jsom::parse_document(json));
        }

        // Access some values to trigger lazy evaluation
        for (int i = 0; i < ACCESS_COUNT; ++i) {
            // NOLINTNEXTLINE(readability-identifier-length)
            auto id = docs[i]["id"].as<long long>();
            benchmark::DoNotOptimize(id);
        }

        benchmark::DoNotOptimize(docs);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
                            * static_cast<int64_t>(json.size()) * DOCUMENT_COUNT);
}
BENCHMARK(BM_JSOM_ManyDocuments);

// Document copying performance
static void BM_JSOM_DocumentCopy(benchmark::State& state) {
    auto json = benchmark_utils::get_medium_json();
    auto original = jsom::parse_document(json);

    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        // Copy document (should copy lazy evaluation state)
        auto copy1 = original;
        auto copy2 = copy1;

        // Access values in copies
        auto status1 = copy1["status"].as<std::string>();
        auto page2 = copy2["pagination"]["page"].as<int>();

        benchmark::DoNotOptimize(copy1);
        benchmark::DoNotOptimize(copy2);
        benchmark::DoNotOptimize(status1);
        benchmark::DoNotOptimize(page2);
    }
}
BENCHMARK(BM_JSOM_DocumentCopy);

// Comparison benchmarks with nlohmann
static void BM_Nlohmann_SmallNumbers(benchmark::State& state) {
    const auto* json = R"({
        "count": 1,
        "page": 2,
        "limit": 10,
        "offset": 0,
        "status": 200,
        "retry": 3
    })";

    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        auto doc = nlohmann::json::parse(json);

        auto count = doc["count"].get<int>();
        auto page = doc["page"].get<int>();

        benchmark::DoNotOptimize(doc);
        benchmark::DoNotOptimize(count);
        benchmark::DoNotOptimize(page);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
                            * static_cast<int64_t>(strlen(json)));
}
BENCHMARK(BM_Nlohmann_SmallNumbers);

static void BM_Nlohmann_ManyDocuments(benchmark::State& state) {
    auto json = benchmark_utils::get_small_json();

    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        std::vector<nlohmann::json> docs;
        docs.reserve(DOCUMENT_COUNT);

        for (int i = 0; i < DOCUMENT_COUNT; ++i) {
            docs.push_back(nlohmann::json::parse(json));
        }

        for (int i = 0; i < ACCESS_COUNT; ++i) {
            // NOLINTNEXTLINE(readability-identifier-length)
            auto id = docs[i]["id"].get<long long>();
            benchmark::DoNotOptimize(id);
        }

        benchmark::DoNotOptimize(docs);
    }
    // NOLINTNEXTLINE(readability-magic-numbers)
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
                            * static_cast<int64_t>(json.size()) * DOCUMENT_COUNT);
}
BENCHMARK(BM_Nlohmann_ManyDocuments);

static void BM_Nlohmann_DocumentCopy(benchmark::State& state) {
    auto json = benchmark_utils::get_medium_json();
    auto original = nlohmann::json::parse(json);

    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        auto copy1 = original;
        auto copy2 = copy1;

        auto status1 = copy1["status"].get<std::string>();
        auto page2 = copy2["pagination"]["page"].get<int>();

        benchmark::DoNotOptimize(copy1);
        benchmark::DoNotOptimize(copy2);
        benchmark::DoNotOptimize(status1);
        benchmark::DoNotOptimize(page2);
    }
}
BENCHMARK(BM_Nlohmann_DocumentCopy);
