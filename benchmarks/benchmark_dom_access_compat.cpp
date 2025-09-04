#include "benchmark_utils.hpp"
#include <benchmark/benchmark.h>
#include <jsom/jsom.hpp>
#include <nlohmann/json.hpp>

// DOM Access Benchmarks (Compatibility with Phase 2)

// Container Navigation Performance
static void BM_JSOM_ContainerAccess_Medium(benchmark::State& state) {
    auto json = benchmark_utils::get_medium_json();
    auto doc = jsom::parse_document(json);

    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        // Object access patterns
        auto status = doc["status"];
        auto pagination = doc["pagination"];
        auto data = doc["data"];

        // Array iteration
        // NOLINTNEXTLINE(readability-magic-numbers)
        for (int i = 0; i < 50; ++i) {
            auto item = doc["data"][i];
            auto price_obj = item["price"];
            auto inventory_obj = item["inventory"];

            benchmark::DoNotOptimize(item);
            benchmark::DoNotOptimize(price_obj);
            benchmark::DoNotOptimize(inventory_obj);
        }

        benchmark::DoNotOptimize(status);
        benchmark::DoNotOptimize(pagination);
        benchmark::DoNotOptimize(data);
    }
}
BENCHMARK(BM_JSOM_ContainerAccess_Medium);

// Type Checking Performance
static void BM_JSOM_TypeChecking_Medium(benchmark::State& state) {
    auto json = benchmark_utils::get_medium_json();
    auto doc = jsom::parse_document(json);

    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        // Type checking operations
        bool status_is_string = doc["status"].is_string();
        bool page_is_number = doc["pagination"]["page"].is_number();
        bool data_is_array = doc["data"].is_array();

        // Check types of first 10 items
        // NOLINTNEXTLINE(readability-magic-numbers)
        for (int i = 0; i < 10; ++i) {
            bool id_is_number = doc["data"][i]["id"].is_number();
            bool name_is_string = doc["data"][i]["name"].is_string();
            bool active_is_bool = doc["data"][i]["active"].is_bool();

            benchmark::DoNotOptimize(id_is_number);
            benchmark::DoNotOptimize(name_is_string);
            benchmark::DoNotOptimize(active_is_bool);
        }

        benchmark::DoNotOptimize(status_is_string);
        benchmark::DoNotOptimize(page_is_number);
        benchmark::DoNotOptimize(data_is_array);
    }
}
BENCHMARK(BM_JSOM_TypeChecking_Medium);

// Construction Performance
static void BM_JSOM_Construction(benchmark::State& state) {
    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        jsom::JsonDocument doc{
            {"name", jsom::JsonDocument("John Doe")},
            // NOLINTNEXTLINE(readability-magic-numbers)
            {"age", jsom::JsonDocument(30)},
            // NOLINTNEXTLINE(readability-magic-numbers)
            {"salary", jsom::JsonDocument(75000.50)},
            {"active", jsom::JsonDocument(true)},
            {"tags",
             jsom::JsonDocument{jsom::JsonDocument("developer"), jsom::JsonDocument("senior")}},
            {"address", jsom::JsonDocument{{"street", jsom::JsonDocument("123 Main St")},
                                           // NOLINTNEXTLINE(readability-magic-numbers)
                                           {"zip", jsom::JsonDocument(12345)}}}};

        benchmark::DoNotOptimize(doc);
    }
}
BENCHMARK(BM_JSOM_Construction);

// Serialization Performance
static void BM_JSOM_Serialization_Medium(benchmark::State& state) {
    auto json = benchmark_utils::get_medium_json();
    auto doc = jsom::parse_document(json);

    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        auto output = doc.to_json();
        benchmark::DoNotOptimize(output);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
                            * static_cast<int64_t>(json.size()));
}
BENCHMARK(BM_JSOM_Serialization_Medium);

// nlohmann::json comparison benchmarks
static void BM_Nlohmann_ContainerAccess_Medium(benchmark::State& state) {
    auto json = benchmark_utils::get_medium_json();
    auto doc = nlohmann::json::parse(json);

    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        auto status = doc["status"];
        auto pagination = doc["pagination"];
        auto data = doc["data"];

        // NOLINTNEXTLINE(readability-magic-numbers)
        for (int i = 0; i < 50; ++i) {
            auto item = doc["data"][i];
            auto price_obj = item["price"];
            auto inventory_obj = item["inventory"];

            benchmark::DoNotOptimize(item);
            benchmark::DoNotOptimize(price_obj);
            benchmark::DoNotOptimize(inventory_obj);
        }

        benchmark::DoNotOptimize(status);
        benchmark::DoNotOptimize(pagination);
        benchmark::DoNotOptimize(data);
    }
}
BENCHMARK(BM_Nlohmann_ContainerAccess_Medium);

static void BM_Nlohmann_TypeChecking_Medium(benchmark::State& state) {
    auto json = benchmark_utils::get_medium_json();
    auto doc = nlohmann::json::parse(json);

    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        bool status_is_string = doc["status"].is_string();
        bool page_is_number = doc["pagination"]["page"].is_number();
        bool data_is_array = doc["data"].is_array();

        // NOLINTNEXTLINE(readability-magic-numbers)
        for (int i = 0; i < 10; ++i) {
            bool id_is_number = doc["data"][i]["id"].is_number();
            bool name_is_string = doc["data"][i]["name"].is_string();
            bool active_is_bool = doc["data"][i]["active"].is_boolean();

            benchmark::DoNotOptimize(id_is_number);
            benchmark::DoNotOptimize(name_is_string);
            benchmark::DoNotOptimize(active_is_bool);
        }

        benchmark::DoNotOptimize(status_is_string);
        benchmark::DoNotOptimize(page_is_number);
        benchmark::DoNotOptimize(data_is_array);
    }
}
BENCHMARK(BM_Nlohmann_TypeChecking_Medium);

static void BM_Nlohmann_Construction(benchmark::State& state) {
    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        nlohmann::json doc = {{"name", "John Doe"},
                              // NOLINTNEXTLINE(readability-magic-numbers)
                              {"age", 30},
                              // NOLINTNEXTLINE(readability-magic-numbers)
                              {"salary", 75000.50},
                              {"active", true},
                              {"tags", {"developer", "senior"}},
                              // NOLINTNEXTLINE(readability-magic-numbers)
                              {"address", {{"street", "123 Main St"}, {"zip", 12345}}}};

        benchmark::DoNotOptimize(doc);
    }
}
BENCHMARK(BM_Nlohmann_Construction);

static void BM_Nlohmann_Serialization_Medium(benchmark::State& state) {
    auto json = benchmark_utils::get_medium_json();
    auto doc = nlohmann::json::parse(json);

    // NOLINTNEXTLINE(readability-identifier-length)
    for (auto _ : state) {
        auto output = doc.dump();
        benchmark::DoNotOptimize(output);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
                            * static_cast<int64_t>(json.size()));
}
BENCHMARK(BM_Nlohmann_Serialization_Medium);
