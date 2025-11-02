#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <jsom/jsom.hpp>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Skip very large inputs to avoid timeout
    if (size > 1024 * 1024) { // 1MB limit
        return 0;
    }

    // Create string from fuzzer input
    std::string json_input(reinterpret_cast<const char*>(data), size);

    try {
        // Test parsing - this is the primary target
        auto doc = jsom::parse_document(json_input);

        // Test serialization (if parse succeeded)
        std::string output = doc.to_json();

        // Test different formatting options
        if (size < 10000) { // Only for smaller inputs to avoid timeout
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

    } catch (const std::exception&) {
        // Expected for invalid JSON - not a bug
        // Parser should handle malformed input gracefully
    } catch (...) {
        // Unexpected exception type - this would be a bug
        // Fuzzer found something that causes undefined behavior
        std::abort();
    }

    return 0;
}
