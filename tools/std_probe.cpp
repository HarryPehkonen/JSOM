// Does the library compile and run under C++17, C++20 and C++23?
//
// The README claims those standards work; this probe is the evidence, and the `std`
// stage in tools/ci.sh compiles it with each `-std=` in turn. Exercises a parse, a
// JSON Pointer lookup and a serialize — enough to instantiate the public API surface
// (variant, LazyNumber, formatter) under each standard.
//
// Build by hand:
//   for s in 17 20 23; do
//       g++ -std=c++$s -Wall -Wextra -Wpedantic -Werror -Iinclude tools/std_probe.cpp
//           build/libjsom_lib.a -o /tmp/std_probe && /tmp/std_probe || echo "C++$s FAILED"
//   done
#include <jsom/jsom.hpp>
#include <string>

auto main() -> int {
    const auto doc = jsom::parse_document(R"({"a":[1,2,{"b":true}],"n":1.5e3})");
    const auto text = doc.to_json();

    const bool ok = doc.at("/a/2/b").as<bool>() && doc.exists("/n") && !text.empty()
                    && doc.find("/a/1") != nullptr;

    return ok ? 0 : 1;
}
