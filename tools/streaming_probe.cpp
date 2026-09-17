// Does the streaming (legacy) path really reject empty containers?
// Proof for CONFORMANCE.md Finding 4: both entry points, same inputs, side by side.
#include <iostream>
#include <string>
#include <vector>
#include <jsom/jsom.hpp>

namespace {

const std::vector<std::string> kInputs = {
    "[]", "[[]]", "{}", "{\"a\":{}}", "[1]", "[[1]]", "{\"a\":1}", "[{},[[]]]", " [ ] ",
};

auto outcome(const std::string& json, bool streaming) -> std::string {
    try {
        if (streaming) {
            (void)jsom::parse_document_streaming(json);
        } else {
            (void)jsom::parse_document(json);
        }
        return "accepted";
    } catch (const std::exception& e) {
        return std::string("REJECTED: ") + e.what();
    }
}

} // namespace

auto main() -> int {
    std::cout << "input          parse_document        parse_document_streaming\n";
    std::cout << "-------------------------------------------------------------"
                 "------------------\n";
    for (const auto& json : kInputs) {
        std::cout << "[" << json << "]";
        for (int pad = 0; pad < 14 - static_cast<int>(json.size()); ++pad) {
            std::cout << ' ';
        }
        std::cout << outcome(json, false);
        for (int pad = 0; pad < 22 - static_cast<int>(outcome(json, false).size()); ++pad) {
            std::cout << ' ';
        }
        std::cout << outcome(json, true) << "\n";
    }
    return 0;
}
