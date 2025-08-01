#include <benchmark/benchmark.h>
#include "jsom.hpp"
#include <nlohmann/json.hpp>
#include "benchmark_utils.hpp"

// Setup test documents for serialization benchmarks
static auto setup_jsom_small() -> jsom::JsonDocument {
    static auto doc = jsom::parse_document(benchmark_utils::get_small_json());
    return doc;
}

static auto setup_jsom_medium() -> jsom::JsonDocument {
    static auto doc = jsom::parse_document(benchmark_utils::get_medium_json());
    return doc;
}

static auto setup_jsom_large() -> jsom::JsonDocument {
    static auto doc = jsom::parse_document(benchmark_utils::get_large_json());
    return doc;
}

static auto setup_jsom_deep_nested() -> jsom::JsonDocument {
    static auto doc = jsom::parse_document(benchmark_utils::get_deep_nested_json());
    return doc;
}

static auto setup_nlohmann_small() -> nlohmann::json {
    static auto doc = nlohmann::json::parse(benchmark_utils::get_small_json());
    return doc;
}

static auto setup_nlohmann_medium() -> nlohmann::json {
    static auto doc = nlohmann::json::parse(benchmark_utils::get_medium_json());
    return doc;
}

static auto setup_nlohmann_large() -> nlohmann::json {
    static auto doc = nlohmann::json::parse(benchmark_utils::get_large_json());
    return doc;
}

static auto setup_nlohmann_deep_nested() -> nlohmann::json {
    static auto doc = nlohmann::json::parse(benchmark_utils::get_deep_nested_json());
    return doc;
}

// JSOM Serialization Benchmarks

static void BM_JSOM_SerializeSmallCompact(benchmark::State& state) {
    auto doc = setup_jsom_small();
    for (auto _ : state) {
        auto json_str = doc.to_json(jsom::FormatPresets::Compact);
        benchmark::DoNotOptimize(json_str);
    }
}
BENCHMARK(BM_JSOM_SerializeSmallCompact);

static void BM_JSOM_SerializeSmallPretty(benchmark::State& state) {
    auto doc = setup_jsom_small();
    for (auto _ : state) {
        auto json_str = doc.to_json(jsom::FormatPresets::Pretty);
        benchmark::DoNotOptimize(json_str);
    }
}
BENCHMARK(BM_JSOM_SerializeSmallPretty);

static void BM_JSOM_SerializeMediumCompact(benchmark::State& state) {
    auto doc = setup_jsom_medium();
    for (auto _ : state) {
        auto json_str = doc.to_json(jsom::FormatPresets::Compact);
        benchmark::DoNotOptimize(json_str);
        state.SetBytesProcessed(json_str.size());
    }
}
BENCHMARK(BM_JSOM_SerializeMediumCompact);

static void BM_JSOM_SerializeMediumPretty(benchmark::State& state) {
    auto doc = setup_jsom_medium();
    for (auto _ : state) {
        auto json_str = doc.to_json(jsom::FormatPresets::Pretty);
        benchmark::DoNotOptimize(json_str);
        state.SetBytesProcessed(json_str.size());
    }
}
BENCHMARK(BM_JSOM_SerializeMediumPretty);

static void BM_JSOM_SerializeLargeCompact(benchmark::State& state) {
    auto doc = setup_jsom_large();
    for (auto _ : state) {
        auto json_str = doc.to_json(jsom::FormatPresets::Compact);
        benchmark::DoNotOptimize(json_str);
        state.SetBytesProcessed(json_str.size());
    }
}
BENCHMARK(BM_JSOM_SerializeLargeCompact);

static void BM_JSOM_SerializeLargePretty(benchmark::State& state) {
    auto doc = setup_jsom_large();
    for (auto _ : state) {
        auto json_str = doc.to_json(jsom::FormatPresets::Pretty);
        benchmark::DoNotOptimize(json_str);
        state.SetBytesProcessed(json_str.size());
    }
}
BENCHMARK(BM_JSOM_SerializeLargePretty);

static void BM_JSOM_SerializeDeepNestedCompact(benchmark::State& state) {
    auto doc = setup_jsom_deep_nested();
    for (auto _ : state) {
        auto json_str = doc.to_json(jsom::FormatPresets::Compact);
        benchmark::DoNotOptimize(json_str);
    }
}
BENCHMARK(BM_JSOM_SerializeDeepNestedCompact);

static void BM_JSOM_SerializeDeepNestedPretty(benchmark::State& state) {
    auto doc = setup_jsom_deep_nested();
    for (auto _ : state) {
        auto json_str = doc.to_json(jsom::FormatPresets::Pretty);
        benchmark::DoNotOptimize(json_str);
    }
}
BENCHMARK(BM_JSOM_SerializeDeepNestedPretty);

// JSOM Custom formatting benchmarks
static void BM_JSOM_SerializeCustomFormat(benchmark::State& state) {
    auto doc = setup_jsom_medium();
    jsom::JsonFormatOptions custom_opts;
    custom_opts.pretty = true;
    custom_opts.indent_size = 4;
    custom_opts.max_inline_array_size = 5;
    custom_opts.max_inline_object_size = 2;
    
    for (auto _ : state) {
        auto json_str = doc.to_json(custom_opts);
        benchmark::DoNotOptimize(json_str);
    }
}
BENCHMARK(BM_JSOM_SerializeCustomFormat);

static void BM_JSOM_SerializeDebugFormat(benchmark::State& state) {
    auto doc = setup_jsom_medium();
    for (auto _ : state) {
        auto json_str = doc.to_json(jsom::FormatPresets::Debug);
        benchmark::DoNotOptimize(json_str);
    }
}
BENCHMARK(BM_JSOM_SerializeDebugFormat);

static void BM_JSOM_SerializeConfigFormat(benchmark::State& state) {
    auto doc = setup_jsom_medium();
    for (auto _ : state) {
        auto json_str = doc.to_json(jsom::FormatPresets::Config);
        benchmark::DoNotOptimize(json_str);
    }
}
BENCHMARK(BM_JSOM_SerializeConfigFormat);

// nlohmann::json Serialization Benchmarks

static void BM_Nlohmann_SerializeSmallCompact(benchmark::State& state) {
    auto doc = setup_nlohmann_small();
    for (auto _ : state) {
        auto json_str = doc.dump();
        benchmark::DoNotOptimize(json_str);
    }
}
BENCHMARK(BM_Nlohmann_SerializeSmallCompact);

static void BM_Nlohmann_SerializeSmallPretty(benchmark::State& state) {
    auto doc = setup_nlohmann_small();
    for (auto _ : state) {
        auto json_str = doc.dump(2);  // 2 spaces indentation
        benchmark::DoNotOptimize(json_str);
    }
}
BENCHMARK(BM_Nlohmann_SerializeSmallPretty);

static void BM_Nlohmann_SerializeMediumCompact(benchmark::State& state) {
    auto doc = setup_nlohmann_medium();
    for (auto _ : state) {
        auto json_str = doc.dump();
        benchmark::DoNotOptimize(json_str);
        state.SetBytesProcessed(json_str.size());
    }
}
BENCHMARK(BM_Nlohmann_SerializeMediumCompact);

static void BM_Nlohmann_SerializeMediumPretty(benchmark::State& state) {
    auto doc = setup_nlohmann_medium();
    for (auto _ : state) {
        auto json_str = doc.dump(2);  // 2 spaces indentation
        benchmark::DoNotOptimize(json_str);
        state.SetBytesProcessed(json_str.size());
    }
}
BENCHMARK(BM_Nlohmann_SerializeMediumPretty);

static void BM_Nlohmann_SerializeLargeCompact(benchmark::State& state) {
    auto doc = setup_nlohmann_large();
    for (auto _ : state) {
        auto json_str = doc.dump();
        benchmark::DoNotOptimize(json_str);
        state.SetBytesProcessed(json_str.size());
    }
}
BENCHMARK(BM_Nlohmann_SerializeLargeCompact);

static void BM_Nlohmann_SerializeLargePretty(benchmark::State& state) {
    auto doc = setup_nlohmann_large();
    for (auto _ : state) {
        auto json_str = doc.dump(2);  // 2 spaces indentation
        benchmark::DoNotOptimize(json_str);
        state.SetBytesProcessed(json_str.size());
    }
}
BENCHMARK(BM_Nlohmann_SerializeLargePretty);

static void BM_Nlohmann_SerializeDeepNestedCompact(benchmark::State& state) {
    auto doc = setup_nlohmann_deep_nested();
    for (auto _ : state) {
        auto json_str = doc.dump();
        benchmark::DoNotOptimize(json_str);
    }
}
BENCHMARK(BM_Nlohmann_SerializeDeepNestedCompact);

static void BM_Nlohmann_SerializeDeepNestedPretty(benchmark::State& state) {
    auto doc = setup_nlohmann_deep_nested();
    for (auto _ : state) {
        auto json_str = doc.dump(2);  // 2 spaces indentation
        benchmark::DoNotOptimize(json_str);
    }
}
BENCHMARK(BM_Nlohmann_SerializeDeepNestedPretty);

// nlohmann custom formatting
static void BM_Nlohmann_SerializeCustomIndent(benchmark::State& state) {
    auto doc = setup_nlohmann_medium();
    for (auto _ : state) {
        auto json_str = doc.dump(4);  // 4 spaces indentation
        benchmark::DoNotOptimize(json_str);
    }
}
BENCHMARK(BM_Nlohmann_SerializeCustomIndent);

// Array-heavy serialization benchmarks
static void BM_JSOM_SerializeLargeArray(benchmark::State& state) {
    // JSOM doesn't support construction from std::vector
    // Create a smaller array using initializer list
    jsom::JsonDocument arr{
        jsom::JsonDocument(0), jsom::JsonDocument(1), jsom::JsonDocument(2),
        jsom::JsonDocument(3), jsom::JsonDocument(4), jsom::JsonDocument(5),
        jsom::JsonDocument(6), jsom::JsonDocument(7), jsom::JsonDocument(8),
        jsom::JsonDocument(9)
    };
    
    for (auto _ : state) {
        auto json_str = arr.to_json(jsom::FormatPresets::Compact);
        benchmark::DoNotOptimize(json_str);
        state.SetBytesProcessed(json_str.size());
    }
}
BENCHMARK(BM_JSOM_SerializeLargeArray);

static void BM_Nlohmann_SerializeLargeArray(benchmark::State& state) {
    nlohmann::json arr = nlohmann::json::array();
    for (int i = 0; i < 10000; ++i) {
        arr.push_back(i);
    }
    
    for (auto _ : state) {
        auto json_str = arr.dump();
        benchmark::DoNotOptimize(json_str);
        state.SetBytesProcessed(json_str.size());
    }
}
BENCHMARK(BM_Nlohmann_SerializeLargeArray);

// Object-heavy serialization benchmarks
static void BM_JSOM_SerializeLargeObject(benchmark::State& state) {
    // JSOM doesn't support construction from std::map
    // Create a smaller object using initializer list
    jsom::JsonDocument obj{
        {"key0", jsom::JsonDocument("value0")},
        {"key1", jsom::JsonDocument("value1")},
        {"key2", jsom::JsonDocument("value2")},
        {"key3", jsom::JsonDocument("value3")},
        {"key4", jsom::JsonDocument("value4")}
    };
    
    for (auto _ : state) {
        auto json_str = obj.to_json(jsom::FormatPresets::Compact);
        benchmark::DoNotOptimize(json_str);
        state.SetBytesProcessed(json_str.size());
    }
}
BENCHMARK(BM_JSOM_SerializeLargeObject);

static void BM_Nlohmann_SerializeLargeObject(benchmark::State& state) {
    nlohmann::json obj = nlohmann::json::object();
    for (int i = 0; i < 1000; ++i) {
        obj["key" + std::to_string(i)] = "value" + std::to_string(i);
    }
    
    for (auto _ : state) {
        auto json_str = obj.dump();
        benchmark::DoNotOptimize(json_str);
        state.SetBytesProcessed(json_str.size());
    }
}
BENCHMARK(BM_Nlohmann_SerializeLargeObject);

// String escaping benchmarks
static void BM_JSOM_SerializeEscapedStrings(benchmark::State& state) {
    jsom::JsonDocument doc{
        {"quote", jsom::JsonDocument("He said \"Hello World\"")},
        {"backslash", jsom::JsonDocument("Path\\to\\file")},
        {"newline", jsom::JsonDocument("Line1\nLine2\nLine3")},
        {"tab", jsom::JsonDocument("Col1\tCol2\tCol3")},
        {"unicode", jsom::JsonDocument("Café münü 🚀")},
        {"control", jsom::JsonDocument(std::string("\x01\x02\x03\x04\x05", 5))}
    };
    
    for (auto _ : state) {
        auto json_str = doc.to_json();
        benchmark::DoNotOptimize(json_str);
    }
}
BENCHMARK(BM_JSOM_SerializeEscapedStrings);

static void BM_Nlohmann_SerializeEscapedStrings(benchmark::State& state) {
    nlohmann::json doc = {
        {"quote", "He said \"Hello World\""},
        {"backslash", "Path\\to\\file"},
        {"newline", "Line1\nLine2\nLine3"},
        {"tab", "Col1\tCol2\tCol3"},
        {"unicode", "Café münü 🚀"},
        {"control", std::string("\x01\x02\x03\x04\x05", 5)}
    };
    
    for (auto _ : state) {
        auto json_str = doc.dump();
        benchmark::DoNotOptimize(json_str);
    }
}
BENCHMARK(BM_Nlohmann_SerializeEscapedStrings);