// A const JsonDocument must be safe to READ from several threads at once.
//
// The property under test is a contract, not a number: const access must not modify
// hidden state. The oracle is ThreadSanitizer (`cmake -B build-tsan -DJSOM_SANITIZE=thread`
// + the `tsan` CI stage) — a data race is invisible to every other check, including
// ASan/UBSan and the value assertions below, which is exactly why the contract needs a
// dedicated gate.
//
// Values are asserted here too: race-free but wrong would still be a bug.
#include <gtest/gtest.h>

#include <atomic>
#include <exception>
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
constexpr int kRounds = 400;

} // namespace

TEST(ConstAccessSafetyTest, ConcurrentConstReadsAreCorrect) {
    const auto doc = jsom::parse_document(kJson);
    const std::vector<std::string> paths = {"/count",
                                            "/items",
                                            "/items/0",
                                            "/items/0/name",
                                            "/items/1/id",
                                            "/nested/deep/deeper/value"};

    std::atomic<int> reads{0};
    std::atomic<int> wrong_answers{0};

    const auto worker = [&doc, &paths, &reads, &wrong_answers](int seed) {
        for (int round = 0; round < kRounds; ++round) {
            for (size_t i = 0; i < paths.size(); ++i) {
                const auto& path = paths[i];
                // Alternate the three const entry points so all of them are exercised.
                switch ((round + seed + static_cast<int>(i)) % 3) {
                case 0: {
                    const auto* found = doc.find(path);
                    if (found == nullptr) {
                        wrong_answers.fetch_add(1);
                    } else {
                        reads.fetch_add(1);
                    }
                    break;
                }
                case 1: {
                    try {
                        (void)doc.at(path);
                        reads.fetch_add(1);
                    } catch (const std::exception&) {
                        wrong_answers.fetch_add(1);
                    }
                    break;
                }
                default: {
                    if (doc.exists(path)) {
                        reads.fetch_add(1);
                    } else {
                        wrong_answers.fetch_add(1);
                    }
                    break;
                }
                }
            }
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back(worker, t);
    }
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(wrong_answers.load(), 0);
    EXPECT_GT(reads.load(), 0);

    // Values must still be right afterwards.
    EXPECT_EQ(doc.at("/count").as<int>(), 2);
    EXPECT_EQ(doc.at("/items/1/name").as<std::string>(), "second");
    EXPECT_EQ(doc.at("/nested/deep/deeper/value").as<int>(), 42);
}

TEST(ConstAccessSafetyTest, ConcurrentConstReadsLeaveTheDocumentUnchanged) {
    const auto doc = jsom::parse_document(kJson);
    const std::string before = doc.to_json();
    const auto paths_before = doc.list_paths();

    const auto worker = [&doc] {
        for (int round = 0; round < kRounds; ++round) {
            (void)doc.at("/items/0/name");
            (void)doc.find("/nested/deep/deeper/value");
            (void)doc.exists("/count");
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back(worker);
    }
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(doc.to_json(), before);
    EXPECT_EQ(doc.list_paths(), paths_before);
}
