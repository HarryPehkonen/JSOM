// Nesting-depth limits — the contract that makes JSOM safe on ANY input.
//
// The bug these pin down (CONFORMANCE.md, Finding 1): 30,000 nested arrays is
// VALID JSON and segfaults the parser. Measured 2026-09-16 on the 8 MB main-thread
// stack: 20,000 levels parse, 24,000 levels die; on a 1 MB thread stack the same
// document dies at ~3,000 levels — 3 KB of input. Objects die sooner than arrays.
// A stack overflow cannot be caught with try/catch, and it kills the whole
// process, so no amount of downstream error handling helps: the bound has to be
// taken on the way IN, and every traversal has to honour it.
//
// The promise is stated three ways:
//   1. the parser rejects any input nested deeper than the configured limit,
//   2. the traversals (serialize, compare, path listing) refuse a document deeper
//      than the limit rather than recursing into the stack,
//   3. all of it holds on a 1 MB thread stack, the smallest a server is likely to
//      hand a library.
//
// FAILURE MODE, AND WHY THAT IS ACCEPTABLE: a violation of (1) or (2) does not
// report a failed assertion, it kills the test process with SIGSEGV — that is the
// whole point, and gtest cannot catch it. The "red" for these tests is a dead
// process; run the binary and watch it.

#include <gtest/gtest.h>

#include <pthread.h>

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

#include "jsom/jsom.hpp"

using namespace jsom;

namespace {

/// The limit every test here is written against, straight from the library.
constexpr int kLimit = jsom::limits::MAX_NESTING_DEPTH;

/// 1 MB: a realistic worker-thread stack. Everything the library is allowed to do
/// at the default limit must fit in it (see the calibration table in the README).
constexpr size_t kServerThreadStackBytes = 1024u * 1024u;

auto nested_arrays(int depth) -> std::string {
    const auto n = static_cast<size_t>(depth);
    return std::string(n, '[') + std::string(n, ']');
}

auto nested_objects(int depth) -> std::string {
    std::string json;
    for (int i = 0; i < depth; ++i) {
        json += "{\"a\":";
    }
    json += "0";
    json.append(static_cast<size_t>(depth), '}');
    return json;
}

/// Closers are not required to attack a stack: 100,000 `[` and nothing else is
/// 100,000 bytes that used to kill the process.
auto open_brackets(int count) -> std::string {
    return std::string(static_cast<size_t>(count), '[');
}

/// Builds a document whose longest root-to-leaf path is exactly `depth` nodes, without
/// the parser — so the traversal guards have to hold on their own.
auto nest_programmatically(int depth) -> JsonDocument {
    auto doc = JsonDocument{0}; // innermost value: a scalar
    for (int level = 1; level < depth; ++level) {
        auto outer = JsonDocument::make_array();
        outer.push_back(std::move(doc));
        doc = std::move(outer);
    }
    return doc;
}

/// Longest root-to-leaf path in the *document* (a scalar is depth 1, so N nested
/// arrays are depth N), walked iteratively so the test helper itself cannot be the
/// thing that overflows.
auto document_depth(const JsonDocument& doc) -> int {
    struct Frame {
        const JsonDocument* node;
        int depth;
    };
    std::vector<Frame> work{{&doc, 1}};
    int deepest = 0;
    while (!work.empty()) {
        const Frame frame = work.back();
        work.pop_back();
        deepest = std::max(deepest, frame.depth);
        if (frame.node->is_array()) {
            for (const auto& child : *frame.node) {
                work.push_back({&child, frame.depth + 1});
            }
        } else if (frame.node->is_object()) {
            for (const auto& [key, child] : frame.node->items()) {
                work.push_back({&child, frame.depth + 1});
            }
        }
    }
    return deepest;
}

/// Asserts that `json` is rejected *because of its depth*, with the documented
/// error — not silently accepted and not fatal.
void expect_depth_rejection(const std::string& json) {
    try {
        const auto doc = parse_document(json);
        ADD_FAILURE() << "expected a nesting-depth rejection, got a document of depth "
                      << document_depth(doc) << " from " << json.size() << " bytes";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find("nesting depth"), std::string::npos)
            << "unexpected error message: " << error.what();
    }
}

struct SmallStackTask {
    const std::function<void()>* body;
    bool returned{false};
};

auto run_on_thread_stack(void* arg) -> void* {
    auto* task = static_cast<SmallStackTask*>(arg);
    (*task->body)();
    task->returned = true;
    return nullptr;
}

/// Runs `body` on a thread with a 1 MB stack. An overflow kills the whole process,
/// so "it returned" is the only observable — which is exactly the guarantee under
/// test.
auto survives_on_small_stack(const std::function<void()>& body) -> bool {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, kServerThreadStackBytes);

    SmallStackTask task{&body, false};
    pthread_t thread{};
    const int created = pthread_create(&thread, &attr, run_on_thread_stack, &task);
    pthread_attr_destroy(&attr);
    if (created != 0) {
        ADD_FAILURE() << "could not create the test thread (pthread_create error " << created << ")";
        return false;
    }
    pthread_join(thread, nullptr);
    return task.returned;
}

} // namespace

// ============================================================
// 1. The parser bounds nesting depth
// ============================================================

TEST(NestingLimitTest, DocumentAtTheLimitParses) {
    const auto doc = parse_document(nested_arrays(kLimit));
    EXPECT_EQ(document_depth(doc), kLimit);
    EXPECT_EQ(doc.size(), 1u);
}

TEST(NestingLimitTest, DocumentOneLevelDeeperIsRejected) {
    expect_depth_rejection(nested_arrays(kLimit + 1));
}

TEST(NestingLimitTest, ObjectsAreBoundedToo) {
    EXPECT_NO_THROW((void)parse_document(nested_objects(kLimit)));
    expect_depth_rejection(nested_objects(kLimit + 1));
}

TEST(NestingLimitTest, HostileInputWithoutClosersIsRejected) {
    expect_depth_rejection(open_brackets(kLimit + 1));
}

TEST(NestingLimitTest, TheHistoricalCrashInputsAreRejected) {
    // The exact shapes that used to kill the process, at the depths that used to
    // matter: 3,000 (the 1 MB-stack killer), 24,000 (the 8 MB-stack killer),
    // 30,000 (the documented one) and 100,000 (the conformance suite's file).
    for (const int depth : {3000, 24000, 30000, 100000}) {
        expect_depth_rejection(open_brackets(depth));
        expect_depth_rejection(nested_arrays(depth));
    }
}

TEST(NestingLimitTest, LimitIsConfigurablePerParse) {
    JsonParseOptions options;
    options.max_depth = 8;

    EXPECT_NO_THROW((void)parse_document(nested_arrays(8), options));
    EXPECT_THROW((void)parse_document(nested_arrays(9), options), std::runtime_error);
}

TEST(NestingLimitTest, LimitCanBeRaisedAboveTheDefault) {
    // The knob cuts both ways: a caller who knows their stack may raise it.
    JsonParseOptions options;
    options.max_depth = kLimit * 4;

    const auto doc = parse_document(nested_arrays(kLimit * 4), options);
    EXPECT_EQ(document_depth(doc), kLimit * 4);
}

TEST(NestingLimitTest, PresetsCarryTheDefaultLimit) {
    EXPECT_EQ(ParsePresets::Default.max_depth, kLimit);
    EXPECT_EQ(ParsePresets::Unicode.max_depth, kLimit);
    EXPECT_EQ(ParsePresets::Comments.max_depth, kLimit);
}

TEST(NestingLimitTest, EmptyContainersDoNotConsumeTheNestingBudget) {
    // The counter is raised on entry and lowered on EVERY normal return, including the
    // early return for an empty container. The first draft of the guard missed one of
    // those early returns, which leaked a level per empty container: a document full of
    // `{}` and `[]` then hit the limit early. 400 of them, followed by a legal 200-deep
    // branch, must still parse.
    std::string json = "[";
    for (int i = 0; i < 200; ++i) {
        json += "{},";
        json += "[]";
        json += ",";
    }
    json += nested_arrays(200);
    json += "]";
    EXPECT_NO_THROW((void)parse_document(json));

    // And the budget is still a budget: swap the legal branch for an over-deep one.
    std::string too_deep = "[";
    for (int i = 0; i < 200; ++i) {
        too_deep += "{},";
        too_deep += "[]";
        too_deep += ",";
    }
    too_deep += open_brackets(kLimit + 1);
    too_deep += "]";
    expect_depth_rejection(too_deep);
}

// ============================================================
// 2. The traversals honour the same bound
// ============================================================

TEST(NestingLimitTest, SerializationOfAnOverDeepDocumentThrows) {
    const auto doc = nest_programmatically(kLimit + 2);

    EXPECT_THROW((void)doc.to_json(), std::runtime_error);
    EXPECT_THROW((void)doc.to_json(/*pretty=*/true), std::runtime_error);
}

TEST(NestingLimitTest, ComparisonOfAnOverDeepDocumentThrows) {
    const auto lhs = nest_programmatically(kLimit + 2);
    const auto rhs = nest_programmatically(kLimit + 2);

    EXPECT_THROW((void)(lhs == rhs), std::runtime_error);
    EXPECT_THROW((void)(lhs < rhs), std::runtime_error);
}

TEST(NestingLimitTest, PathListingOfAnOverDeepDocumentThrows) {
    const auto doc = nest_programmatically(kLimit + 2);

    EXPECT_THROW((void)doc.list_paths(), std::runtime_error);
}

TEST(NestingLimitTest, TraversalsStillWorkOnALegalDocument) {
    // The guards must not cost us the cases that are supposed to work.
    const auto doc = nest_programmatically(kLimit);
    const auto twin = nest_programmatically(kLimit);

    EXPECT_EQ(document_depth(doc), kLimit);
    EXPECT_EQ(doc, twin);
    EXPECT_NO_THROW((void)doc.list_paths());
}

// ============================================================
// 3. …and all of it holds on a 1 MB thread stack
// ============================================================

TEST(NestingLimitTest, DeepestLegalDocumentSurvivesAServerSizedStack) {
    const bool survived = survives_on_small_stack([] {
        // An exception escaping the thread function would call std::terminate and take
        // the whole binary with it, hiding every other result: report it instead.
        try {
            const auto doc = parse_document(nested_arrays(kLimit));
            EXPECT_EQ(document_depth(doc), kLimit);

            // Every traversal the library offers, on the deepest document it allows.
            const auto json = doc.to_json();
            EXPECT_FALSE(json.empty());
            const auto pretty = doc.to_json(true);
            EXPECT_FALSE(pretty.empty());
            EXPECT_EQ(doc, doc);
            EXPECT_FALSE(doc.list_paths().empty());
            EXPECT_GT(doc.to_json(FormatPresets::Pretty).size(), 0u);

            const auto clone = doc;
            EXPECT_EQ(clone, doc);
        } catch (const std::exception& error) {
            ADD_FAILURE() << "a document at the limit must survive every traversal, got: "
                          << error.what();
        }
    });
    EXPECT_TRUE(survived) << "the deepest legal document must fit in a 1 MB stack";
}

TEST(NestingLimitTest, HostileInputSurvivesAServerSizedStack) {
    const bool survived = survives_on_small_stack([] {
        for (const int depth : {kLimit + 1, 3000, 30000, 100000}) {
            try {
                (void)parse_document(open_brackets(depth));
                ADD_FAILURE() << "depth " << depth << " should have been rejected";
            } catch (const std::runtime_error& error) {
                EXPECT_NE(std::string(error.what()).find("nesting depth"), std::string::npos);
            }
        }
    });
    EXPECT_TRUE(survived) << "hostile input must be rejected, not fatal, on a server-sized stack";
}
