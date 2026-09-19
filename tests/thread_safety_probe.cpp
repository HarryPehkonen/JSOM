// Thread-safety gate: a const JsonDocument must be safe to READ from several threads.
//
// Deliberately not a gtest binary: this is built with ThreadSanitizer
// (`cmake -B build-tsan -DJSOM_SANITIZE=thread`) and TSan needs the whole program
// instrumented, so linking the test framework would cost minutes per run. The `tsan`
// stage in tools/ci.sh builds and runs exactly this file.
//
// Exit code 0 = clean. TSan reports a race with a non-zero exit and a full report; any
// wrong answer here also exits non-zero.
#include <cstddef>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include <jsom/jsom.hpp>

namespace {

constexpr const char* kJson = R"({
    "count": 2,
    "items": [ {"id": 0, "name": "first"}, {"id": 1, "name": "second"} ],
    "nested": {"deep": {"deeper": {"value": 42}}}
})";

constexpr int kThreads = 4;
constexpr int kRounds = 500;

} // namespace

auto main() -> int {
    const auto doc = jsom::parse_document(kJson);
    const std::vector<std::string> paths
        = {"/count",        "/items",      "/items/0",
           "/items/0/name", "/items/1/id", "/nested/deep/deeper/value"};

    std::vector<int> failures(static_cast<size_t>(kThreads), 0);

    const auto run = [&doc, &paths, &failures](int id) {
        int local = 0;
        for (int round = 0; round < kRounds; ++round) {
            for (size_t i = 0; i < paths.size(); ++i) {
                const auto& path = paths[i];
                switch ((round + id + static_cast<int>(i)) % 3) {
                case 0:
                    local += doc.find(path) == nullptr ? 1 : 0;
                    break;
                case 1:
                    try {
                        (void)doc.at(path);
                    } catch (const std::exception&) {
                        ++local;
                    }
                    break;
                default:
                    local += doc.exists(path) ? 0 : 1;
                    break;
                }
            }
        }
        failures[static_cast<size_t>(id)] = local;
    };

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back(run, t);
    }
    for (auto& thread : threads) {
        thread.join();
    }

    int total_failures = 0;
    for (const int count : failures) {
        total_failures += count;
    }

    // Values must be right afterwards, too: race-free but wrong is still a bug.
    const bool values_ok = doc.at("/count").as<int>() == 2
                           && doc.at("/items/1/name").as<std::string>() == "second"
                           && doc.at("/nested/deep/deeper/value").as<int>() == 42;

    if (total_failures != 0 || !values_ok) {
        std::fprintf(stderr, "FAILED: %d wrong answers, values_ok=%d\n", total_failures,
                     values_ok ? 1 : 0);
        return 1;
    }

    std::printf("clean: %d threads x %d rounds x %zu paths, values correct\n", kThreads, kRounds,
                paths.size());
    return 0;
}
