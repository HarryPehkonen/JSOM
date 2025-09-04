#include "benchmark_utils.hpp"
#include <benchmark/benchmark.h>
#include <jsom/jsom.hpp>

namespace {
constexpr int FIRST_N_RECORDS = 10;
constexpr int REPETITIONS = 100;
} // namespace
#include <nlohmann/json.hpp>

// Lazy Evaluation Performance Tests

// Mixed Access Pattern (String + Number Access)
static void BM_JSOM_MixedAccess_Medium(benchmark::State& state) {
    auto json = benchmark_utils::get_medium_json();
    auto doc = jsom::parse_document(json);

    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        // String access (no lazy evaluation)
        auto status = doc["status"].as<std::string>();

        // Number access (triggers lazy evaluation)
        auto page = doc["pagination"]["page"].as<int>();
        auto total = doc["pagination"]["total"].as<int>();

        // Access first 10 items with mixed types
        // NOLINTNEXTLINE(readability-magic-numbers)
        for (int i = 0; i < FIRST_N_RECORDS; ++i) {
            // NOLINTNEXTLINE(readability-identifier-length)
            auto id = doc["data"][i]["id"].as<int>();
            auto name = doc["data"][i]["name"].as<std::string>();
            auto price = doc["data"][i]["price"]["amount"].as<double>();

            benchmark::DoNotOptimize(id);
            benchmark::DoNotOptimize(name);
            benchmark::DoNotOptimize(price);
        }

        benchmark::DoNotOptimize(status);
        benchmark::DoNotOptimize(page);
        benchmark::DoNotOptimize(total);
    }
}
BENCHMARK(BM_JSOM_MixedAccess_Medium);

// Pure Number Access (Maximum Lazy Evaluation Impact)
static void BM_JSOM_NumberAccess_NumberHeavy(benchmark::State& state) {
    auto json = benchmark_utils::get_number_heavy_json();
    auto doc = jsom::parse_document(json);

    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        // Access all numeric fields in first 10 records
        for (int i = 0; i < FIRST_N_RECORDS; ++i) {
            auto timestamp = doc["metrics"][i]["timestamp"].as<int>();
            auto cpu_usage = doc["metrics"][i]["cpu_usage"].as<double>();
            auto memory_usage = doc["metrics"][i]["memory_usage"].as<double>();
            auto disk_io_read = doc["metrics"][i]["disk_io_read"].as<int>();
            auto response_time = doc["metrics"][i]["response_time_ms"].as<double>();
            auto error_rate = doc["metrics"][i]["error_rate"].as<double>();

            benchmark::DoNotOptimize(timestamp);
            benchmark::DoNotOptimize(cpu_usage);
            benchmark::DoNotOptimize(memory_usage);
            benchmark::DoNotOptimize(disk_io_read);
            benchmark::DoNotOptimize(response_time);
            benchmark::DoNotOptimize(error_rate);
        }
    }
}
BENCHMARK(BM_JSOM_NumberAccess_NumberHeavy);

// Repeated Access (Caching Test)
static void BM_JSOM_RepeatedAccess(benchmark::State& state) {
    auto json = benchmark_utils::get_medium_json();
    auto doc = jsom::parse_document(json);

    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        // Access the same values multiple times to test caching
        for (int rep = 0; rep < REPETITIONS; ++rep) {
            auto page = doc["pagination"]["page"].as<int>();
            auto price = doc["data"][0]["price"]["amount"].as<double>();

            benchmark::DoNotOptimize(page);
            benchmark::DoNotOptimize(price);
        }
    }
}
BENCHMARK(BM_JSOM_RepeatedAccess);

// Nested Object Access (simplified)
static void BM_JSOM_NestedAccess(benchmark::State& state) {
    auto json = benchmark_utils::get_medium_json();
    auto doc = jsom::parse_document(json);

    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        // Access nested values
        auto page = doc["pagination"]["page"].as<int>();
        auto price = doc["data"][0]["price"]["amount"].as<double>();
        auto inventory = doc["data"][0]["inventory"]["quantity"].as<int>();
        auto weight = doc["data"][0]["dimensions"]["weight"].as<double>();

        benchmark::DoNotOptimize(page);
        benchmark::DoNotOptimize(price);
        benchmark::DoNotOptimize(inventory);
        benchmark::DoNotOptimize(weight);
    }
}
BENCHMARK(BM_JSOM_NestedAccess);

// nlohmann comparison for mixed access
static void BM_Nlohmann_MixedAccess_Medium(benchmark::State& state) {
    auto json = benchmark_utils::get_medium_json();
    auto doc = nlohmann::json::parse(json);

    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        auto status = doc["status"].get<std::string>();
        auto page = doc["pagination"]["page"].get<int>();
        auto total = doc["pagination"]["total"].get<int>();

        for (int i = 0; i < FIRST_N_RECORDS; ++i) {
            // NOLINTNEXTLINE(readability-identifier-length)
            auto id = doc["data"][i]["id"].get<int>();
            auto name = doc["data"][i]["name"].get<std::string>();
            auto price = doc["data"][i]["price"]["amount"].get<double>();

            benchmark::DoNotOptimize(id);
            benchmark::DoNotOptimize(name);
            benchmark::DoNotOptimize(price);
        }

        benchmark::DoNotOptimize(status);
        benchmark::DoNotOptimize(page);
        benchmark::DoNotOptimize(total);
    }
}
BENCHMARK(BM_Nlohmann_MixedAccess_Medium);

// nlohmann comparison for number access
static void BM_Nlohmann_NumberAccess_NumberHeavy(benchmark::State& state) {
    auto json = benchmark_utils::get_number_heavy_json();
    auto doc = nlohmann::json::parse(json);

    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        for (int i = 0; i < FIRST_N_RECORDS; ++i) {
            auto timestamp = doc["metrics"][i]["timestamp"].get<int>();
            auto cpu_usage = doc["metrics"][i]["cpu_usage"].get<double>();
            auto memory_usage = doc["metrics"][i]["memory_usage"].get<double>();
            auto disk_io_read = doc["metrics"][i]["disk_io_read"].get<int>();
            auto response_time = doc["metrics"][i]["response_time_ms"].get<double>();
            auto error_rate = doc["metrics"][i]["error_rate"].get<double>();

            benchmark::DoNotOptimize(timestamp);
            benchmark::DoNotOptimize(cpu_usage);
            benchmark::DoNotOptimize(memory_usage);
            benchmark::DoNotOptimize(disk_io_read);
            benchmark::DoNotOptimize(response_time);
            benchmark::DoNotOptimize(error_rate);
        }
    }
}
BENCHMARK(BM_Nlohmann_NumberAccess_NumberHeavy);
