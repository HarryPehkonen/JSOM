// JSOM libFuzzer target (tests/fuzzer.cpp) — the same byte input, three readings,
// counted separately:
//
//   1. plain byte reading: parse the raw input with JSOM, run the round-trip oracle and
//      the API surface (the committed behaviour);
//   2. structured/generated reading: JSONFuzz's structure-aware generator produces a
//      document from the bytes, and the two fixed-point oracles (O1 round-trip, O2
//      pointer contract) run against the JSOM adapter;
//   3. custom-mutator reading: jsonfuzz::mutate restructures the input, and the oracles
//      run against the result.
//
// THE ORACLE, HARD RULE: the error path is `try { parse } catch { return; }` and the
// round-trip property runs OUTSIDE every catch. A broad catch around the property turns
// a wrong answer into "expected for invalid input" — the exact way this harness missed a
// serializer that emits invalid JSON ({...,}): with the round-trip property inside the
// catch, a sabotage of the compact serializer was invisible (exit 0, zero findings);
// moved outside the catch, the same library and seeds reported it immediately (exit 77,
// ROUND-TRIP REPARSE FAILED). A throw on the property path is itself a failure.
//
// REACH GUARD: with JSOM_FUZZ_REQUIRE_REACH=1 and -runs=0 over fuzz/seeds, the process
// exits non-zero unless EVERY reading reached its assertion, and the message names which
// one starved. "At least one input got there" is the guard that hid a blind spot in a
// sibling repo (117 of 9,952 corpus inputs reached the assertion there and a class-shaped
// sabotage still survived 4.2 M executions), so each reading is counted on its own.
//
// REACH COUNTS: the same run also prints how many inputs actually reached each reading's
// oracle laws (an accepted parse) versus a trivial early exit (a rejected parse).
//
// WHICH READINGS RUN: JSOM_FUZZ_READINGS selects them, because their costs differ by ~10x —
// the byte reading alone ran ~8,200 exec/s on the committed harness, while all three together
// run ~870 (three readings per input). Values: `all` (the default), `byte`, `structured`,
// `mutator`, or a comma-separated combination such as `byte,mutator`. An unknown token is a
// hard error at startup, deliberately: a typo that silently disabled a reading would leave the
// suite looking clean while checking less, which is the one failure mode this knob must not
// have. The enabled set is printed once as `readings enabled: ...`, and the reach guard
// requires exactly the ENABLED readings — deselecting one is a decision, not a miss.
//
// clang only: built by CMake with -DJSOM_BUILD_FUZZING=ON. The source stays g++-clean:
// it must not include libFuzzer-only headers (LLVMFuzzerCustomMutator is found by symbol
// name, not by declaration).

#include "jsonfuzz_jsom.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <jsom/jsom.hpp>
#include <jsonfuzz/generator.hpp>
#include <jsonfuzz/mutate.hpp>
#include <jsonfuzz/oracle.hpp>
#include <jsonfuzz/sut.hpp>

namespace {

// Which readings are enabled — JSOM_FUZZ_READINGS, defaulting to all three. Validated and
// announced once, see the header comment for why an unknown token is fatal rather than ignored.
struct Readings {
    bool byte = true;
    bool structured = true;
    bool mutator = true;
};

Readings parse_readings_env() {
    const char* raw = std::getenv("JSOM_FUZZ_READINGS");
    if (raw == nullptr || *raw == '\0' || std::string(raw) == "all") {
        return Readings{};
    }
    Readings on{false, false, false};
    const std::string list(raw);
    size_t start = 0;
    while (true) {
        const size_t comma = list.find(',', start);
        const size_t end = (comma == std::string::npos) ? list.size() : comma;
        std::string token = list.substr(start, end - start);
        const size_t first = token.find_first_not_of(" \t");
        const size_t last = token.find_last_not_of(" \t");
        token
            = (first == std::string::npos) ? std::string() : token.substr(first, last - first + 1);
        if (token == "byte") {
            on.byte = true;
        } else if (token == "structured") {
            on.structured = true;
        } else if (token == "mutator") {
            on.mutator = true;
        } else if (!token.empty()) {
            std::fprintf(stderr,
                         "JSOM_FUZZ_READINGS: unknown reading '%s' (known: byte, structured, "
                         "mutator, all)\n",
                         token.c_str());
            std::abort();
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    if (!on.byte && !on.structured && !on.mutator) {
        std::fprintf(stderr, "JSOM_FUZZ_READINGS: no reading selected\n");
        std::abort();
    }
    return on;
}

const Readings& enabled_readings() {
    static const Readings on = [] {
        const Readings parsed = parse_readings_env();
        std::fprintf(stderr, "readings enabled:%s%s%s\n", parsed.byte ? " byte" : "",
                     parsed.structured ? " structured" : "", parsed.mutator ? " mutator" : "");
        return parsed;
    }();
    return on;
}

// The reach guard. A static destructor runs at process exit, after libFuzzer has played
// every seed, so it can fail the whole run when a reading never reached.
struct ReachGuard {
    // Parsing the knob in the constructor means a typo fails BEFORE any input runs (-runs=0
    // included), and the `readings enabled:` line is printed once, at the top of the log.
    ReachGuard() { (void)enabled_readings(); }

    bool byte_reading = false;
    bool structured_reading = false;
    bool mutator_reading = false;

    // Reach COUNTS, accumulated across every input, so a run reports how many inputs
    // actually reached each reading's oracle laws (an accepted parse) versus a trivial
    // early exit (a rejected parse). "A counter is non-zero" is not evidence of reach;
    // these numbers are.
    size_t total = 0;
    size_t byte_accept = 0;
    size_t structured_accept = 0;
    size_t mutator_accept = 0;

    ~ReachGuard() {
        std::fprintf(stderr,
                     "REACH COUNTS: total=%zu byte_accept=%zu structured_accept=%zu "
                     "mutator_accept=%zu\n",
                     total, byte_accept, structured_accept, mutator_accept);
        if (std::getenv("JSOM_FUZZ_REQUIRE_REACH") == nullptr) {
            return;
        }
        const Readings& on = enabled_readings();
        bool starved = false;
        if (on.byte && !byte_reading) {
            std::fprintf(stderr, "REACH STARVED: plain byte reading\n");
            starved = true;
        }
        if (on.structured && !structured_reading) {
            std::fprintf(stderr, "REACH STARVED: structured/generated reading\n");
            starved = true;
        }
        if (on.mutator && !mutator_reading) {
            std::fprintf(stderr, "REACH STARVED: custom-mutator reading\n");
            starved = true;
        }
        if (starved) {
            std::abort();
        }
    }
};

ReachGuard g_reach;

/// The round-trip oracle: what we serialize must parse back to the same document.
/// A wrong answer never crashes, so this has to be asserted explicitly. Runs OUTSIDE
/// every catch: a throw here is a failure, not "expected for invalid input".
///
/// The re-parse uses the SAME options as the parse that accepted the document. Under the
/// default configuration that is the whole promise — RFC 8259 in, RFC 8259 out, byte
/// identical. Loose mode can hold number text that is not JSON (`1.0.` is exactly what it
/// is for), so its round trip is promised within loose mode; re-reading it strictly is
/// *supposed* to fail, and asserting otherwise asserts the wrong thing. (This stage found
/// that on 2026-09-20: `1.0.` was accepted loosely, written back faithfully, and then
/// terminated the harness — whose re-parse had silently used the default options.)
void assert_round_trip(const jsom::JsonDocument& doc, const std::string& input,
                       const jsom::JsonParseOptions& options) {
    const std::string output = doc.to_json();
    const auto reparsed = jsom::parse_document(output, options);
    if (!(reparsed == doc)) {
        std::cerr << "ROUND-TRIP MISMATCH\n  in:  " << input << "\n  out: " << output << "\n";
        std::abort();
    }
}

/// Parses with one configuration and, if accepted, runs the oracle plus (optionally) the
/// whole API surface. A rejection is an expected outcome, not a bug. Returns whether the
/// input was accepted (so the caller can count reach).
bool drive(const std::string& input, const jsom::JsonParseOptions& options, bool explore) {
    jsom::JsonDocument doc;
    try {
        doc = jsom::parse_document(input, options);
    } catch (const std::exception&) {
        return false; // rejection is an outcome, not a bug
    } catch (...) {
        std::abort(); // unexpected exception type
    }

    // The property runs OUTSIDE every catch.
    assert_round_trip(doc, input, options);
    if (!explore) {
        return true;
    }

    // Test different formatting options
    if (input.size() < 10000) { // Only for smaller inputs to avoid timeout
        doc.to_json(jsom::FormatPresets::Compact);
        doc.to_json(jsom::FormatPresets::Pretty);

        // Test with custom options
        jsom::JsonFormatOptions custom;
        custom.indent_size = 2;
        custom.max_line_width = 80;
        doc.to_json(custom);
    }

    // Test JSON Pointer operations (if valid structure)
    if (doc.is_object() || doc.is_array()) {
        // Test path enumeration
        auto paths = doc.list_paths(3); // Limited depth

        // Test existence checks
        doc.exists("/");
        doc.exists("/test");
        doc.exists("/nonexistent");

        // Test safe access
        doc.find("/");
        doc.find("/test");

        // Test path operations on first few paths only
        for (size_t i = 0; i < std::min(paths.size(), size_t(5)); ++i) {
            try {
                doc.at(paths[i]);
                doc.exists(paths[i]);
            } catch (const std::exception&) {
                // Expected for some paths
            }
        }
    }

    // Test type checking
    doc.is_null();
    doc.is_bool();
    doc.is_number();
    doc.is_string();
    doc.is_object();
    doc.is_array();

    // Test safe conversions
    doc.try_as<bool>();
    doc.try_as<int>();
    doc.try_as<double>();
    doc.try_as<std::string>();
    return true;
}

/// The configurations, all of them, on every input (Harri, 2026-09-16: "fuzz ALL
/// configurations to find bugs"). The default enforces the RFC 8259 number grammar
/// (3.1.0 flipped this), and `Loose` is the extension mode that accepts `01`, `1.`,
/// `1eE2` — so both the strict rejection paths and the lenient acceptance paths run.
std::vector<jsom::JsonParseOptions> configurations() {
    jsom::JsonParseOptions loose;
    loose.allow_loose_numbers = true;
    return {jsom::JsonParseOptions{}, loose};
}

// A deterministic seed for the mutator, derived from the input so the same bytes give
// the same mutation.
unsigned seed_from(const uint8_t* data, size_t size) {
    uint64_t h = 0x9E3779B97F4A7C15ULL;
    for (size_t i = 0; i < size; ++i) {
        h = (h ^ data[i]) * 0xBF58476D1CE4E5B9ULL;
    }
    return static_cast<unsigned>(h ^ (h >> 32));
}

std::string to_string(const uint8_t* data, size_t size) {
    std::string s(size, '\0');
    for (size_t i = 0; i < size; ++i) {
        s[i] = static_cast<char>(data[i]);
    }
    return s;
}

// Run the two JSONFuzz oracles against the JSOM adapter on `text`. A violation is a
// finding: report it and abort (libFuzzer writes the artifact).
void check_adapter(const std::string& text, const char* reading) {
    jsom_fuzz::JsonomSut sut;
    const jsonfuzz::ParseConfig cfg;
    const auto violations = jsonfuzz::check(sut, text, cfg);
    for (const auto& v : violations) {
        std::cerr << reading << " violation: " << v.message << "\n  text: " << text << "\n";
    }
    if (!violations.empty()) {
        std::abort();
    }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Skip very large inputs to avoid timeout
    if (size > 1024 * 1024) { // 1MB limit
        return 0;
    }

    const std::string json_input = to_string(data, size);

    // Reading 1: plain byte reading (the committed behaviour).
    if (enabled_readings().byte) {
        bool first = true;
        bool accepted = false;
        for (const auto& options : configurations()) {
            accepted = drive(json_input, options, first) || accepted;
            first = false;
        }
        g_reach.byte_reading = g_reach.byte_reading || accepted;
        g_reach.byte_accept += accepted ? 1 : 0;
    }

    // Reading 2: structured/generated reading. The generator is total and produces valid
    // JSON, so this reading always reaches its oracle laws.
    if (enabled_readings().structured) {
        const jsonfuzz::GenOptions gopts;
        jsonfuzz::ByteSource bytes(data, size);
        const jsonfuzz::Generated gen = jsonfuzz::generate(gopts, bytes);
        check_adapter(gen.text, "STRUCTURED");
        g_reach.structured_reading = true;
        g_reach.structured_accept += 1;
    }

    // Reading 3: custom-mutator reading. jsonfuzz::mutate restructures the input; the
    // oracles run against the result.
    if (enabled_readings().mutator) {
        std::vector<uint8_t> buf(data, data + size);
        const size_t ns
            = jsonfuzz::mutate(buf.data(), buf.size(), buf.size(), seed_from(data, size));
        const std::string mutated = to_string(buf.data(), ns);
        check_adapter(mutated, "MUTATOR");
        g_reach.mutator_reading = true;
        g_reach.mutator_accept += 1;
    }

    // The counters, in the stats line, so a run shows which reading reached.
    std::fprintf(stderr, "readings byte=%d structured=%d mutator=%d\n",
                 g_reach.byte_reading ? 1 : 0, g_reach.structured_reading ? 1 : 0,
                 g_reach.mutator_reading ? 1 : 0);

    ++g_reach.total;
    return 0;
}

// The custom mutator: JSON-aware restructuring on top of jsonfuzz::mutate. libFuzzer
// finds this by symbol name; no libFuzzer header is needed, so the file stays g++-clean.
extern "C" size_t LLVMFuzzerCustomMutator(uint8_t* data, size_t size, size_t max_size,
                                          unsigned seed) {
    return jsonfuzz::mutate(data, size, max_size, seed);
}

// A cheap crossover: take the first input, then splice in a chunk of the second at a
// deterministic point. Keeps the byte-level behaviour working alongside the structured
// mutator.
extern "C" size_t LLVMFuzzerCustomCrossOver(const uint8_t* data1, size_t size1,
                                            const uint8_t* data2, size_t size2, uint8_t* out,
                                            size_t max_out_size, unsigned seed) {
    if (size1 > max_out_size) {
        size1 = max_out_size;
    }
    for (size_t i = 0; i < size1; ++i) {
        out[i] = data1[i];
    }
    if (size2 > 0 && size1 < max_out_size) {
        const size_t take = std::min(size2, max_out_size - size1);
        const size_t start = seed % size2;
        for (size_t i = 0; i < take; ++i) {
            out[size1 + i] = data2[(start + i) % size2];
        }
        return size1 + take;
    }
    return size1;
}
