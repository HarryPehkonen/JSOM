// JSOM hot-path performance probe (standalone — NOT part of jsom_benchmarks).
//
// Purpose: quick, low-friction A/B measurement of parse/serialize hot paths
// (see OPTIMIZATIONS.md). Standalone on purpose: builds in seconds without
// the google-benchmark/nlohmann fetch, so interleaved A/B runs are cheap.
//
// Build + run (from the repo root, after a Release lib build):
//   cmake -B build-rel -DCMAKE_BUILD_TYPE=Release -DJSOM_BUILD_TESTS=OFF
//   cmake --build build-rel --target jsom_lib
//   g++ -std=c++17 -O3 -march=native -Iinclude tools/perf_probe.cpp \
//       build-rel/libjsom_lib.a -o /tmp/perf_probe && /tmp/perf_probe
//
// Measurement rules (learned the hard way — see OPTIMIZATIONS.md):
//  - Never compare sequential before/after runs: noise is +/-5%. Compile TWO
//    binaries (current vs candidate) and interleave A/B/A/B in one session.
//  - Run 3x and report the spread. A real win is consistent across rounds.
//  - Use REALISTIC input shapes: 9-char keys are SSO, which hides key-handling
//    wins; realistic keys are 20-30 chars.
// JSOM performance probe — scratch harness (NOT part of the repo).
// Generates representative JSON inputs and times parse + serialize.
#include <jsom/jsom.hpp>
#include <chrono>
#include <cstdio>
#include <string>

using namespace jsom;
using Clock = std::chrono::steady_clock;

static double ms(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

// input builders -----------------------------------------------------------
static std::string make_number_heavy(size_t n) {
    std::string s = "{\"numbers\":[";
    for (size_t i = 0; i < n; ++i) {
        if (i) s += ',';
        s += std::to_string(i * 137 % 997) + "." + std::to_string(i % 89) + "e" + std::to_string(i % 4);
    }
    s += "]}";
    return s;
}
static std::string make_string_heavy(size_t n) {
    std::string s = "{\"items\":[";
    for (size_t i = 0; i < n; ++i) {
        if (i) s += ',';
        s += "\"value with spaces and punct!@#$%^&*()_" + std::to_string(i) + "\"";
    }
    s += "]}";
    return s;
}
static std::string make_object_heavy(size_t n) {
    std::string s = "{";
    for (size_t i = 0; i < n; ++i) {
        if (i) s += ',';
        s += "\"field_" + std::to_string(i) + "\":" + std::to_string(i * 7 % 1000);
    }
    s += "}";
    return s;
}
static std::string make_deep(size_t depth) {
    std::string s;
    for (size_t i = 0; i < depth; ++i) s += '[';
    s += "42";
    for (size_t i = 0; i < depth; ++i) s += ']';
    return s;
}

static std::string make_longkey_object(size_t n) {
    // Realistic config-style keys: 20-30 chars, not SSO (>15 chars)
    std::string s = "{";
    for (size_t i = 0; i < n; ++i) {
        if (i) s += ',';
        s += "\"application_configuration_key_" + std::to_string(i) + "\":" + std::to_string(i * 7 % 1000);
    }
    s += "}";
    return s;
}

template <typename F>
static void bench(const char* name, int iters, F&& f) {
    auto a = Clock::now();
    f();  // warmup
    f();
    auto b = Clock::now();
    volatile size_t sink = 0;
    for (int i = 0; i < iters; ++i) sink += f();
    auto c = Clock::now();
    double total = ms(b, c);
    printf("%-22s %6d iters  %8.2f ms  %10.2f us/op  (sink %zu)\n", name, iters, total,
           total * 1000.0 / iters, sink);
}

int main() {
    const auto num_json = make_number_heavy(2000);
    const auto str_json = make_string_heavy(1000);
    const auto obj_json = make_object_heavy(2000);
    const auto deep_json = make_deep(300);
    printf("inputs: num=%zuB str=%zuB obj=%zuB deep=%zuB\n\n", num_json.size(), str_json.size(),
           obj_json.size(), deep_json.size());

    bench("parse numbers 2000", 400, [&] {
        auto d = parse_document(num_json);
        return d.size();
    });
    bench("parse strings 1000", 400, [&] {
        auto d = parse_document(str_json);
        return d.size();
    });
    bench("parse objects 2000", 400, [&] {
        auto d = parse_document(obj_json);
        return d.size();
    });
    bench("parse deep 300", 400, [&] {
        auto d = parse_document(deep_json);
        return d.size();
    });

    auto obj_doc = parse_document(obj_json);
    const auto longkey_json = make_longkey_object(2000);
    printf("longkey object: %zuB\n", longkey_json.size());
    bench("parse longkey obj 2000", 300, [&] {
        auto d = parse_document(longkey_json);
        return d.size();
    });
    bench("serialize objects 2000", 400, [&] {
        auto s = obj_doc.to_json();
        return s.size();
    });
    auto num_doc = parse_document(num_json);
    bench("serialize numbers 2000", 400, [&] {
        auto s = num_doc.to_json();
        return s.size();
    });
    return 0;
}
