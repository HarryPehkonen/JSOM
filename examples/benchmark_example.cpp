#include "jsom.hpp"
#include <iostream>
#include <chrono>
#include <sstream>

using namespace jsom;

// Generate test JSON with specified structure
auto generate_test_json(size_t object_count, size_t array_size) -> std::string
{
    std::ostringstream json;
    json << "{\n";
    
    for (size_t i = 0; i < object_count; ++i) {
        if (i > 0) json << ",\n";
        json << "  \"object" << i << "\": {\n";
        json << "    \"id\": " << i << ",\n";
        json << "    \"name\": \"Object " << i << "\",\n";
        json << "    \"values\": [";
        
        for (size_t j = 0; j < array_size; ++j) {
            if (j > 0) json << ", ";
            json << (i * array_size + j);
        }
        
        json << "]\n  }";
    }
    
    json << "\n}";
    return json.str();
}

auto benchmark_allocator(const std::string& json, const std::string& allocator_name, 
                        std::unique_ptr<IAllocator> allocator) -> void
{
    std::cout << "\nBenchmarking " << allocator_name << ":\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    StreamingParser parser(std::move(allocator));
    
    // Count discovered values
    size_t value_count = 0;
    ParseEvents events;
    events.on_value = [&value_count](const std::string& /*pointer*/, const JsonValue& /*value*/) {
        ++value_count;
    };
    parser.set_events(events);
    
    parser.parse_string(json);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "  Parse time: " << duration.count() << " microseconds\n";
    std::cout << "  Values found: " << value_count << '\n';
    std::cout << "  Total paths: " << parser.total_paths() << '\n';
    std::cout << "  Memory usage: " << parser.memory_usage() << " bytes\n";
}

auto main() -> int 
{
    std::cout << "JSOM Streaming Parser Benchmark\n";
    std::cout << "================================\n";
    
    // Generate test data
    constexpr size_t OBJECT_COUNT = 100;
    constexpr size_t ARRAY_SIZE = 10;
    
    std::cout << "Generating test JSON with " << OBJECT_COUNT << " objects, " 
              << ARRAY_SIZE << " array elements each...\n";
    
    std::string test_json = generate_test_json(OBJECT_COUNT, ARRAY_SIZE);
    std::cout << "Generated JSON size: " << test_json.size() << " bytes\n";
    
    // Benchmark both allocators
    benchmark_allocator(test_json, "ArenaAllocator", std::make_unique<ArenaAllocator>());
    benchmark_allocator(test_json, "StandardAllocator", std::make_unique<StandardAllocator>());
    
    return 0;
}