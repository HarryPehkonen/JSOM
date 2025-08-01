#include <benchmark/benchmark.h>
#include <iostream>

// This file serves as the main entry point for all benchmarks.
// The actual benchmark functions are defined in separate files.

int main(int argc, char** argv) {
    std::cout << "JSOM vs nlohmann::json Performance Benchmarks\n";
    std::cout << "=============================================\n\n";
    
    // Initialize and run benchmarks
    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();
    return 0;
}