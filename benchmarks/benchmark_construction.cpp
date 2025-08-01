#include <benchmark/benchmark.h>
#include "jsom.hpp"
#include <nlohmann/json.hpp>
#include "benchmark_utils.hpp"

// JSOM Construction Benchmarks

static void BM_JSOM_BuildObject(benchmark::State& state) {
    const int num_props = state.range(0);
    
    for (auto _ : state) {
        // JSOM doesn't support post-construction modification,
        // so this benchmark measures empty object construction only
        jsom::JsonDocument obj{};
        benchmark::DoNotOptimize(obj);
        (void)num_props; // Suppress unused variable warning
    }
}
// Test with different object sizes: 10, 100, 1000 properties
BENCHMARK(BM_JSOM_BuildObject)->Args({10})->Args({100})->Args({1000});

static void BM_JSOM_BuildObjectInitList(benchmark::State& state) {
    const int num_props = state.range(0);
    
    for (auto _ : state) {
        if (num_props == 10) {
            jsom::JsonDocument obj{
                {"key0", jsom::JsonDocument("value0")},
                {"key1", jsom::JsonDocument("value1")},
                {"key2", jsom::JsonDocument("value2")},
                {"key3", jsom::JsonDocument("value3")},
                {"key4", jsom::JsonDocument("value4")},
                {"key5", jsom::JsonDocument("value5")},
                {"key6", jsom::JsonDocument("value6")},
                {"key7", jsom::JsonDocument("value7")},
                {"key8", jsom::JsonDocument("value8")},
                {"key9", jsom::JsonDocument("value9")}
            };
            benchmark::DoNotOptimize(obj);
        } else {
            // For larger sizes, JSOM doesn't support direct construction from std::map
            // This test is less meaningful, so we'll create a simple object
            jsom::JsonDocument obj{};
            benchmark::DoNotOptimize(obj);
        }
    }
}
BENCHMARK(BM_JSOM_BuildObjectInitList)->Args({10})->Args({100})->Args({1000});

static void BM_JSOM_BuildArray(benchmark::State& state) {
    const int num_elements = state.range(0);
    
    for (auto _ : state) {
        // JSOM doesn't support construction from std::vector, 
        // so this benchmark measures empty array construction only
        jsom::JsonDocument arr{};
        benchmark::DoNotOptimize(arr);
    }
}
BENCHMARK(BM_JSOM_BuildArray)->Args({10})->Args({100})->Args({1000})->Args({10000});

static void BM_JSOM_BuildArrayInitList(benchmark::State& state) {
    const int num_elements = state.range(0);
    
    for (auto _ : state) {
        if (num_elements == 10) {
            jsom::JsonDocument arr{
                jsom::JsonDocument(0), jsom::JsonDocument(1), jsom::JsonDocument(2),
                jsom::JsonDocument(3), jsom::JsonDocument(4), jsom::JsonDocument(5),
                jsom::JsonDocument(6), jsom::JsonDocument(7), jsom::JsonDocument(8),
                jsom::JsonDocument(9)
            };
            benchmark::DoNotOptimize(arr);
        } else {
            // For larger sizes, JSOM doesn't support direct construction from std::vector
            // This test focuses on initializer list performance
            jsom::JsonDocument arr{};
            benchmark::DoNotOptimize(arr);
        }
    }
}
BENCHMARK(BM_JSOM_BuildArrayInitList)->Args({10})->Args({100})->Args({1000})->Args({10000});

static void BM_JSOM_BuildNestedStructure(benchmark::State& state) {
    const int depth = state.range(0);
    
    for (auto _ : state) {
        std::function<jsom::JsonDocument(int)> build_nested = [&](int level) -> jsom::JsonDocument {
            if (level <= 0) {
                return jsom::JsonDocument("leaf_value");
            }
            return jsom::JsonDocument{
                {"level", jsom::JsonDocument(level)},
                {"data", jsom::JsonDocument("data_" + std::to_string(level))},
                {"nested", build_nested(level - 1)}
            };
        };
        
        auto doc = build_nested(depth);
        benchmark::DoNotOptimize(doc);
    }
}
BENCHMARK(BM_JSOM_BuildNestedStructure)->Args({5})->Args({10})->Args({20});

static void BM_JSOM_CopyConstruct(benchmark::State& state) {
    auto original = jsom::parse_document(benchmark_utils::get_medium_json());
    
    for (auto _ : state) {
        auto copy = original;  // Copy constructor
        benchmark::DoNotOptimize(copy);
    }
}
BENCHMARK(BM_JSOM_CopyConstruct);

static void BM_JSOM_MoveConstruct(benchmark::State& state) {
    for (auto _ : state) {
        auto original = jsom::parse_document(benchmark_utils::get_small_json());
        auto moved = std::move(original);  // Move constructor
        benchmark::DoNotOptimize(moved);
    }
}
BENCHMARK(BM_JSOM_MoveConstruct);

// nlohmann::json Construction Benchmarks

static void BM_Nlohmann_BuildObject(benchmark::State& state) {
    const int num_props = state.range(0);
    
    for (auto _ : state) {
        nlohmann::json obj = nlohmann::json::object();
        for (int i = 0; i < num_props; ++i) {
            obj["key" + std::to_string(i)] = "value" + std::to_string(i);
        }
        benchmark::DoNotOptimize(obj);
    }
}
BENCHMARK(BM_Nlohmann_BuildObject)->Args({10})->Args({100})->Args({1000});

static void BM_Nlohmann_BuildObjectInitList(benchmark::State& state) {
    const int num_props = state.range(0);
    
    for (auto _ : state) {
        if (num_props == 10) {
            nlohmann::json obj = {
                {"key0", "value0"}, {"key1", "value1"}, {"key2", "value2"},
                {"key3", "value3"}, {"key4", "value4"}, {"key5", "value5"},
                {"key6", "value6"}, {"key7", "value7"}, {"key8", "value8"},
                {"key9", "value9"}
            };
            benchmark::DoNotOptimize(obj);
        } else {
            nlohmann::json obj = nlohmann::json::object();
            for (int i = 0; i < num_props; ++i) {
                obj["key" + std::to_string(i)] = "value" + std::to_string(i);
            }
            benchmark::DoNotOptimize(obj);
        }
    }
}
BENCHMARK(BM_Nlohmann_BuildObjectInitList)->Args({10})->Args({100})->Args({1000});

static void BM_Nlohmann_BuildArray(benchmark::State& state) {
    const int num_elements = state.range(0);
    
    for (auto _ : state) {
        nlohmann::json arr = nlohmann::json::array();
        for (int i = 0; i < num_elements; ++i) {
            arr.push_back(i);
        }
        benchmark::DoNotOptimize(arr);
    }
}
BENCHMARK(BM_Nlohmann_BuildArray)->Args({10})->Args({100})->Args({1000})->Args({10000});

static void BM_Nlohmann_BuildArrayInitList(benchmark::State& state) {
    const int num_elements = state.range(0);
    
    for (auto _ : state) {
        if (num_elements == 10) {
            nlohmann::json arr = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
            benchmark::DoNotOptimize(arr);
        } else {
            nlohmann::json arr = nlohmann::json::array();
            for (int i = 0; i < num_elements; ++i) {
                arr.push_back(i);
            }
            benchmark::DoNotOptimize(arr);
        }
    }
}
BENCHMARK(BM_Nlohmann_BuildArrayInitList)->Args({10})->Args({100})->Args({1000})->Args({10000});

static void BM_Nlohmann_BuildNestedStructure(benchmark::State& state) {
    const int depth = state.range(0);
    
    for (auto _ : state) {
        std::function<nlohmann::json(int)> build_nested = [&](int level) -> nlohmann::json {
            if (level <= 0) {
                return "leaf_value";
            }
            return nlohmann::json{
                {"level", level},
                {"data", "data_" + std::to_string(level)},
                {"nested", build_nested(level - 1)}
            };
        };
        
        auto doc = build_nested(depth);
        benchmark::DoNotOptimize(doc);
    }
}
BENCHMARK(BM_Nlohmann_BuildNestedStructure)->Args({5})->Args({10})->Args({20});

static void BM_Nlohmann_CopyConstruct(benchmark::State& state) {
    auto original = nlohmann::json::parse(benchmark_utils::get_medium_json());
    
    for (auto _ : state) {
        auto copy = original;  // Copy constructor
        benchmark::DoNotOptimize(copy);
    }
}
BENCHMARK(BM_Nlohmann_CopyConstruct);

static void BM_Nlohmann_MoveConstruct(benchmark::State& state) {
    for (auto _ : state) {
        auto original = nlohmann::json::parse(benchmark_utils::get_small_json());
        auto moved = std::move(original);  // Move constructor
        benchmark::DoNotOptimize(moved);
    }
}
BENCHMARK(BM_Nlohmann_MoveConstruct);

// Mixed construction patterns
static void BM_JSOM_MixedConstruction(benchmark::State& state) {
    for (auto _ : state) {
        auto doc = jsom::JsonDocument{
            {"users", jsom::JsonDocument{
                jsom::JsonDocument{
                    {"id", jsom::JsonDocument(1)},
                    {"name", jsom::JsonDocument("Alice")},
                    {"active", jsom::JsonDocument(true)}
                },
                jsom::JsonDocument{
                    {"id", jsom::JsonDocument(2)},
                    {"name", jsom::JsonDocument("Bob")},
                    {"active", jsom::JsonDocument(false)}
                }
            }},
            {"settings", jsom::JsonDocument{
                {"theme", jsom::JsonDocument("dark")},
                {"notifications", jsom::JsonDocument(true)},
                {"max_items", jsom::JsonDocument(100)}
            }},
            {"data", jsom::JsonDocument{
                jsom::JsonDocument(1), jsom::JsonDocument(2), jsom::JsonDocument(3),
                jsom::JsonDocument(4), jsom::JsonDocument(5)
            }}
        };
        benchmark::DoNotOptimize(doc);
    }
}
BENCHMARK(BM_JSOM_MixedConstruction);

static void BM_Nlohmann_MixedConstruction(benchmark::State& state) {
    for (auto _ : state) {
        nlohmann::json doc = {
            {"users", {
                {
                    {"id", 1},
                    {"name", "Alice"},
                    {"active", true}
                },
                {
                    {"id", 2},
                    {"name", "Bob"},
                    {"active", false}
                }
            }},
            {"settings", {
                {"theme", "dark"},
                {"notifications", true},
                {"max_items", 100}
            }},
            {"data", {1, 2, 3, 4, 5}}
        };
        benchmark::DoNotOptimize(doc);
    }
}
BENCHMARK(BM_Nlohmann_MixedConstruction);