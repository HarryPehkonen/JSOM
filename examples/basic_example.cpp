#include "jsom.hpp"
#include <iostream>

using namespace jsom;

auto main() -> int 
{
    // Create parser with arena allocator for efficiency
    auto allocator = std::make_unique<ArenaAllocator>();
    StreamingParser parser(std::move(allocator));
    
    // Set up event handlers
    ParseEvents events;
    events.on_value = [](const std::string& pointer, const JsonValue& value) {
        std::cout << "Found value at " << pointer << ": " << value.raw() << '\n';
    };
    
    events.on_enter_object = [](const std::string& pointer) {
        std::cout << "Entering object at " << pointer << '\n';
    };
    
    events.on_error = [](size_t position, const std::string& message) {
        std::cerr << "Parse error at position " << position << ": " << message << '\n';
    };
    
    parser.set_events(events);
    
    // Example JSON to parse
    std::string json = R"({
        "name": "John Doe",
        "age": 30,
        "active": true,
        "scores": [95, 87, 92],
        "address": {
            "street": "123 Main St",
            "city": "Anytown"
        }
    })";
    
    std::cout << "Parsing JSON with streaming parser...\n";
    parser.parse_string(json);
    
    std::cout << "\nParser statistics:\n";
    std::cout << "Total paths discovered: " << parser.total_paths() << '\n';
    std::cout << "Memory usage: " << parser.memory_usage() << " bytes\n";
    
    return 0;
}