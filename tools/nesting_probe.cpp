// JSOM nesting-depth probe (standalone — NOT part of jsom_tests).
//
// Two questions this answers, both of which decided the depth-guard design
// (CONFORMANCE.md Finding 1, OPTIMIZATIONS.md "Depth guard"):
//   1. What does one nesting LEVEL cost, per path — parse, compare, serialize —
//      and therefore how much does the guard cost?
//   2. Does the bound actually hold on the shapes that used to be fatal?
//
// Build + run (from the repo root, after a Release lib build):
//   cmake -B build-rel -DCMAKE_BUILD_TYPE=Release -DJSOM_BUILD_TESTS=OFF
//   cmake --build build-rel --target jsom_lib
//   g++ -std=c++17 -O3 -march=native -Iinclude tools/nesting_probe.cpp \
//       build-rel/libjsom_lib.a -o /tmp/nesting_probe && /tmp/nesting_probe
//
// Measurement rules are the same as tools/perf_probe.cpp: interleave builds, run
// several rounds, report the best/median — absolute numbers across sessions are
// meaningless, ratios within a session are not.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <utility>
#include <jsom/jsom.hpp>

using namespace jsom;
using Clock = std::chrono::steady_clock;

namespace {

auto nested_arrays(int depth) -> std::string {
    const auto n = static_cast<size_t>(depth);
    return std::string(n, '[') + std::string(n, ']');
}

/// Builds a document of exactly `depth` nodes on its longest path, without the parser.
auto nest_programmatically(int depth) -> JsonDocument {
    auto doc = JsonDocument{0};
    for (int level = 1; level < depth; ++level) {
        auto outer = JsonDocument::make_array();
        outer.push_back(std::move(doc));
        doc = std::move(outer);
    }
    return doc;
}

template <typename F>
auto best_of(int rounds, int iters, F&& body) -> double {
    double best = 1e18;
    for (int round = 0; round < rounds; ++round) {
        const auto start = Clock::now();
        for (int i = 0; i < iters; ++i) {
            body();
        }
        const auto stop = Clock::now();
        const double per_iter =
            std::chrono::duration<double, std::nano>(stop - start).count() / iters;
        best = std::min(best, per_iter);
    }
    return best;
}

} // namespace

int main() {
    constexpr int kDepth = 200;
    constexpr int kRounds = 7;
    constexpr int kIters = 2000;

    const auto json = nested_arrays(kDepth);
    const auto deep_doc = nest_programmatically(kDepth);
    const auto twin = nest_programmatically(kDepth);

    const double parse_ns =
        best_of(kRounds, kIters, [&] { auto d = parse_document(json); (void)d; });
    const double compare_ns =
        best_of(kRounds, kIters, [&] { const bool eq = (deep_doc == twin); (void)eq; });
    const double serialize_ns =
        best_of(kRounds, kIters, [&] { auto s = deep_doc.to_json(); (void)s; });

    std::printf("depth-%d document, best of %d x %d iterations\n\n", kDepth, kRounds, kIters);
    std::printf("  parse      %9.3f us/document  (%7.2f ns per level)\n", parse_ns / 1000.0,
                parse_ns / kDepth);
    std::printf("  compare    %9.3f us/document  (%7.2f ns per level)\n", compare_ns / 1000.0,
                compare_ns / kDepth);
    std::printf("  serialize  %9.3f us/document  (%7.2f ns per level)\n", serialize_ns / 1000.0,
                serialize_ns / kDepth);

    std::printf("\n  (the guard is one comparison per container: compare this build\n"
                "   against a baseline built from the previous commit to price it)\n");

    std::printf("\nthe bound, on the shapes that used to be fatal:\n");
    const int probe_depths[] = {256, 257, 3000, 24000, 100000};
    for (const int depth : probe_depths) {
        for (const auto& [label, input] :
             {std::pair<const char*, std::string>{"balanced ", nested_arrays(depth)},
              {"opens only", std::string(static_cast<size_t>(depth), '[')}}) {
            try {
                auto doc = parse_document(input);
                std::printf("  %-10s %6d levels -> ACCEPTED\n", label, depth);
                (void)doc;
            } catch (const std::exception& error) {
                std::printf("  %-10s %6d levels -> rejected: %s\n", label, depth, error.what());
            }
        }
    }
    return 0;
}
