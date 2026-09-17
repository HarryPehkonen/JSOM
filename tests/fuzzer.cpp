#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <jsom/jsom.hpp>

namespace {

/// The round-trip oracle: what we serialize must parse back to the same document.
/// A wrong answer never crashes, so this has to be asserted explicitly.
void assert_round_trip(const jsom::JsonDocument& doc, const std::string& input) {
    const std::string output = doc.to_json();
    const auto reparsed = jsom::parse_document(output);
    if (!(reparsed == doc)) {
        std::cerr << "ROUND-TRIP MISMATCH\n  in:  " << input << "\n  out: " << output << "\n";
        std::abort();
    }
}

/// Parses with one configuration and, if accepted, runs the oracle plus (optionally) the
/// whole API surface. A rejection is an expected outcome, not a bug.
void drive(const std::string& input, const jsom::JsonParseOptions& options, bool explore) {
    auto doc = jsom::parse_document(input, options);
    assert_round_trip(doc, input);
    if (!explore) {
        return;
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
}

/// The configurations, all of them, on every input (Harri, 2026-09-16: "fuzz ALL
/// configurations to find bugs"). `Lazy` is the default that ships — numbers keep the
/// extension behaviour, the lexer rules are unconditional — and `All` adds the spec's
/// number grammar, which is where the strict rejection paths live.
std::vector<jsom::JsonParseOptions> configurations() {
    jsom::JsonParseOptions strict_all;
    strict_all.validate_numbers = true;
    return {jsom::JsonParseOptions{}, strict_all};
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Skip very large inputs to avoid timeout
    if (size > 1024 * 1024) { // 1MB limit
        return 0;
    }

    // Create string from fuzzer input
    const std::string json_input(reinterpret_cast<const char*>(data), size);

    try {
        bool first = true;
        for (const auto& options : configurations()) {
            // Explore the whole API in one configuration only: it doubles the work per
            // input otherwise, and the interesting difference between the two is whether
            // they accept, not what the DOM looks like afterwards.
            drive(json_input, options, first);
            first = false;
        }
    } catch (const std::exception&) {
        // Expected for invalid JSON - not a bug
        // Parser should handle malformed input gracefully
    } catch (...) {
        // Unexpected exception type - this would be a bug
        std::abort();
    }

    return 0;
}
