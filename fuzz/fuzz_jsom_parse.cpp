// JSOM fuzz target: JSON parsing + serialization round-trip.
//
// Build with clang (libFuzzer):
//   cmake -B build-fuzz -DJSOM_BUILD_FUZZING=ON -DCMAKE_CXX_COMPILER=clang++ ..
//   cmake --build build-fuzz --target fuzz_jsom_parse
//   ./build-fuzz/fuzz/fuzz_jsom_parse fuzz/corpus/
//
// Invalid JSON is EXPECTED input: parse_document throws, we catch and return 0.
// A crash (or sanitizer report) means a real bug in the parser.

#include "jsom/jsom.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Cap input size: beyond this the parser is doing the same work at scale.
    if (size == 0 || size > (1u << 20)) {
        return 0;
    }

    const std::string json(reinterpret_cast<const char*>(data), size);
    try {
        auto doc = jsom::parse_document(json);
        // Round-trip: serialize the parsed document, then parse it again.
        const std::string out = doc.to_json();
        (void)jsom::parse_document(out);
    } catch (const std::exception&) {
        // Expected for malformed input; not a bug.
    }
    return 0;
}
