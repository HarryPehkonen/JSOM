#include <benchmark/benchmark.h>
#include "jsom.hpp"
#include <nlohmann/json.hpp>
#include "benchmark_utils.hpp"

// Setup test data for DOM access benchmarks
static auto setup_jsom_test_data() -> jsom::JsonDocument {
    static auto doc = jsom::parse_document(benchmark_utils::get_medium_json());
    return doc;
}

static auto setup_nlohmann_test_data() -> nlohmann::json {
    static auto doc = nlohmann::json::parse(benchmark_utils::get_medium_json());
    return doc;
}

// JSOM DOM Access Benchmarks

static void BM_JSOM_AccessNestedValue(benchmark::State& state) {
    auto doc = setup_jsom_test_data();
    for (auto _ : state) {
        // Access nested values repeatedly
        auto status = doc["status"].as<std::string>();
        auto page = doc["pagination"]["page"].as<int>();
        auto first_product = doc["data"][0]["name"].as<std::string>();
        auto first_price = doc["data"][0]["price"]["amount"].as<double>();
        
        benchmark::DoNotOptimize(status);
        benchmark::DoNotOptimize(page);
        benchmark::DoNotOptimize(first_product);
        benchmark::DoNotOptimize(first_price);
    }
}
BENCHMARK(BM_JSOM_AccessNestedValue);

static void BM_JSOM_IterateArray(benchmark::State& state) {
    auto doc = setup_jsom_test_data();
    for (auto _ : state) {
        auto& data_array = doc["data"];
        int sum = 0;
        for (size_t i = 0; i < data_array.size(); ++i) {
            sum += data_array[i]["id"].as<int>();
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_JSOM_IterateArray);

static void BM_JSOM_TypeChecking(benchmark::State& state) {
    auto doc = setup_jsom_test_data();
    for (auto _ : state) {
        bool is_string = doc["status"].is_string();
        bool is_number = doc["pagination"]["page"].is_number();
        bool is_array = doc["data"].is_array();
        bool is_object = doc["pagination"].is_object();
        bool is_null = doc["nonexistent"].is_null();
        
        benchmark::DoNotOptimize(is_string);
        benchmark::DoNotOptimize(is_number);
        benchmark::DoNotOptimize(is_array);
        benchmark::DoNotOptimize(is_object);
        benchmark::DoNotOptimize(is_null);
    }
}
BENCHMARK(BM_JSOM_TypeChecking);

static void BM_JSOM_ValueExtraction(benchmark::State& state) {
    auto doc = setup_jsom_test_data();
    for (auto _ : state) {
        // Extract values with type conversion
        auto status = doc["status"].as<std::string>();
        auto page = doc["pagination"]["page"].as<int>();
        auto total = doc["pagination"]["total"].as<int>();
        auto first_price = doc["data"][0]["price"]["amount"].as<double>();
        auto first_in_stock = doc["data"][0]["inventory"]["in_stock"].as<bool>();
        
        benchmark::DoNotOptimize(status);
        benchmark::DoNotOptimize(page);
        benchmark::DoNotOptimize(total);
        benchmark::DoNotOptimize(first_price);
        benchmark::DoNotOptimize(first_in_stock);
    }
}
BENCHMARK(BM_JSOM_ValueExtraction);

// Large array iteration
static void BM_JSOM_LargeArrayIteration(benchmark::State& state) {
    std::string json = "[";
    for (int i = 0; i < 10000; ++i) {
        if (i > 0) json += ",";
        json += "{\"id\":" + std::to_string(i) + ",\"value\":" + std::to_string(i * 2) + "}";
    }
    json += "]";
    
    auto doc = jsom::parse_document(json);
    
    for (auto _ : state) {
        long long sum = 0;
        for (size_t i = 0; i < doc.size(); ++i) {
            sum += doc[i]["id"].as<int>();
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_JSOM_LargeArrayIteration);

// nlohmann::json DOM Access Benchmarks

static void BM_Nlohmann_AccessNestedValue(benchmark::State& state) {
    auto doc = setup_nlohmann_test_data();
    for (auto _ : state) {
        // Access nested values repeatedly
        auto status = doc["status"].get<std::string>();
        auto page = doc["pagination"]["page"].get<int>();
        auto first_product = doc["data"][0]["name"].get<std::string>();
        auto first_price = doc["data"][0]["price"]["amount"].get<double>();
        
        benchmark::DoNotOptimize(status);
        benchmark::DoNotOptimize(page);
        benchmark::DoNotOptimize(first_product);
        benchmark::DoNotOptimize(first_price);
    }
}
BENCHMARK(BM_Nlohmann_AccessNestedValue);

static void BM_Nlohmann_IterateArray(benchmark::State& state) {
    auto doc = setup_nlohmann_test_data();
    for (auto _ : state) {
        auto& data_array = doc["data"];
        int sum = 0;
        for (size_t i = 0; i < data_array.size(); ++i) {
            sum += data_array[i]["id"].get<int>();
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_Nlohmann_IterateArray);

static void BM_Nlohmann_TypeChecking(benchmark::State& state) {
    auto doc = setup_nlohmann_test_data();
    for (auto _ : state) {
        bool is_string = doc["status"].is_string();
        bool is_number = doc["pagination"]["page"].is_number();
        bool is_array = doc["data"].is_array();
        bool is_object = doc["pagination"].is_object();
        bool is_null = doc.contains("nonexistent") ? doc["nonexistent"].is_null() : true;
        
        benchmark::DoNotOptimize(is_string);
        benchmark::DoNotOptimize(is_number);
        benchmark::DoNotOptimize(is_array);
        benchmark::DoNotOptimize(is_object);
        benchmark::DoNotOptimize(is_null);
    }
}
BENCHMARK(BM_Nlohmann_TypeChecking);

static void BM_Nlohmann_ValueExtraction(benchmark::State& state) {
    auto doc = setup_nlohmann_test_data();
    for (auto _ : state) {
        // Extract values with type conversion
        auto status = doc["status"].get<std::string>();
        auto page = doc["pagination"]["page"].get<int>();
        auto total = doc["pagination"]["total"].get<int>();
        auto first_price = doc["data"][0]["price"]["amount"].get<double>();
        auto first_in_stock = doc["data"][0]["inventory"]["in_stock"].get<bool>();
        
        benchmark::DoNotOptimize(status);
        benchmark::DoNotOptimize(page);
        benchmark::DoNotOptimize(total);
        benchmark::DoNotOptimize(first_price);
        benchmark::DoNotOptimize(first_in_stock);
    }
}
BENCHMARK(BM_Nlohmann_ValueExtraction);

static void BM_Nlohmann_LargeArrayIteration(benchmark::State& state) {
    std::string json = "[";
    for (int i = 0; i < 10000; ++i) {
        if (i > 0) json += ",";
        json += "{\"id\":" + std::to_string(i) + ",\"value\":" + std::to_string(i * 2) + "}";
    }
    json += "]";
    
    auto doc = nlohmann::json::parse(json);
    
    for (auto _ : state) {
        long long sum = 0;
        for (size_t i = 0; i < doc.size(); ++i) {
            sum += doc[i]["id"].get<int>();
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_Nlohmann_LargeArrayIteration);

// Deep nesting access benchmarks
static void BM_JSOM_DeepNestingAccess(benchmark::State& state) {
    auto doc = jsom::parse_document(benchmark_utils::get_deep_nested_json());
    for (auto _ : state) {
        // Access deeply nested value
        auto final_message = doc["level0"]["next"]["level1"]["next"]["level2"]["next"]
                                ["level3"]["next"]["level4"]["next"]["final"]["message"].as<std::string>();
        benchmark::DoNotOptimize(final_message);
    }
}
BENCHMARK(BM_JSOM_DeepNestingAccess);

static void BM_Nlohmann_DeepNestingAccess(benchmark::State& state) {
    auto doc = nlohmann::json::parse(benchmark_utils::get_deep_nested_json());
    for (auto _ : state) {
        // Access deeply nested value
        auto final_message = doc["level0"]["next"]["level1"]["next"]["level2"]["next"]
                                ["level3"]["next"]["level4"]["next"]["final"]["message"].get<std::string>();
        benchmark::DoNotOptimize(final_message);
    }
}
BENCHMARK(BM_Nlohmann_DeepNestingAccess);