# JSOM CMakeLists.txt
# 
# This file provides the essential CMake configuration for JSOM.
# It focuses on core development needs: building, testing, benchmarking, and code quality.

cmake_minimum_required(VERSION 3.10)
project(JSOM VERSION 1.0.0 LANGUAGES CXX)

# Detect if JSOM is being built standalone or as a subdirectory
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(JSOM_IS_TOP_LEVEL ON)
else()
    set(JSOM_IS_TOP_LEVEL OFF)
endif()

# Compiler configuration
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # For clang-tidy and IDE support

# Compiler-specific optimizations
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Development flags for good diagnostics
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic")
    # Release flags for performance testing
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
endif()

# FetchContent for dependencies
include(FetchContent)

# ================================
# JSOM Library Target
# ================================

# Library with JSON Pointer implementation and formatting
add_library(jsom_lib
    src/json_document_pointer.cpp
    src/json_document_formatting.cpp
)
target_include_directories(jsom_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_features(jsom_lib PRIVATE cxx_std_17)

# Add modern C++ compile flags for the library
target_compile_options(jsom_lib INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

# Namespaced alias for modern CMake usage
add_library(JSOM::jsom ALIAS jsom_lib)

# ================================
# Build Options
# ================================

# Build options (must be defined before test/benchmark configuration)
option(JSOM_BUILD_BENCHMARKS "Build JSOM benchmarks" OFF)  # Always OFF unless explicitly enabled

if(JSOM_IS_TOP_LEVEL)
    option(JSOM_BUILD_TESTS "Build JSOM tests" ON)
else()
    option(JSOM_BUILD_TESTS "Build JSOM tests" OFF)
endif()

# ================================
# Testing Configuration
# ================================

if(JSOM_BUILD_TESTS)
    enable_testing()

    # Configure GoogleTest (essential for development standards compliance)
    set(INSTALL_GTEST OFF CACHE BOOL "Enable installation of googletest" FORCE)
    set(INSTALL_GMOCK OFF CACHE BOOL "Enable installation of googlemock" FORCE)

    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0  # Pin version for reproducible builds
    )

    FetchContent_MakeAvailable(googletest)

# Test executable
add_executable(jsom_tests
    # Core architecture tests
    tests/test_main.cpp
    tests/test_lazy_number.cpp           # LazyNumber class tests
    tests/test_json_document.cpp         # JsonDocument with std::variant
    tests/test_document_builder.cpp      # Integration tests
    tests/test_api_compatibility.cpp     # API examples validation

    # JSON Pointer tests
    tests/test_json_pointer.cpp          # JSON Pointer functionality tests

    # Performance regression tests
    tests/test_performance_regression.cpp

    # Memory safety tests
    tests/test_memory_safety.cpp

    # Format preservation tests
    tests/test_format_preservation.cpp

    # Unicode escape tests
    tests/test_unicode_escapes.cpp       # Unicode escape sequence handling

    # Ergonomic construction tests
    tests/test_ergonomic_construction.cpp # Factory methods and direct constructors

    # Cache invalidation tests
    tests/test_cache_invalidation.cpp    # Validates set()/push_back() invalidate path cache

    # Iteration tests
    tests/test_iteration.cpp             # Range-for, items(), keys()

    # Comparison operator tests
    tests/test_comparison.cpp            # Equality and ordering operators

    # Comment-tolerant parsing tests
    tests/test_comments.cpp              # // and /* */ comment support
)

target_link_libraries(jsom_tests
    jsom_lib
    gtest
    gtest_main
    gmock
    gmock_main
)

target_include_directories(jsom_tests PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Discover and register tests with CTest
include(GoogleTest)
gtest_discover_tests(jsom_tests)

# Custom target for running tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose --output-on-failure
    DEPENDS jsom_tests
    COMMENT "Running all tests"
)
endif()  # JSOM_BUILD_TESTS

# ================================
# Benchmark Configuration
# ================================

if(JSOM_BUILD_BENCHMARKS)
    # Configure Google Benchmark
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "Enable installation of benchmark" FORCE)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Enable testing of the benchmark library" FORCE)
    
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3  # Pin version for reproducible builds
    )
    
    # nlohmann::json for performance comparison
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3  # Pin version for fair comparison
    )
    
    FetchContent_MakeAvailable(benchmark nlohmann_json)
    
    # Benchmark executable
    add_executable(jsom_benchmarks
        # Use provided benchmark utilities
        benchmarks/benchmark_main.cpp
        benchmarks/benchmark_utils.hpp
        
        # Performance benchmarks
        benchmarks/benchmark_lazy_evaluation.cpp
        benchmarks/benchmark_parse_serialize.cpp
        benchmarks/benchmark_format_preservation.cpp
        benchmarks/benchmark_memory_usage.cpp
        
        # Compatibility benchmarks (same patterns as Phase 2)
        benchmarks/benchmark_parsing_compat.cpp
        benchmarks/benchmark_dom_access_compat.cpp
    )
    
    target_link_libraries(jsom_benchmarks
        jsom_lib
        benchmark::benchmark
        benchmark::benchmark_main
        nlohmann_json::nlohmann_json
    )
    
    target_include_directories(jsom_benchmarks PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks
    )
    
    # Benchmark targets for performance validation
    add_custom_target(run_benchmarks
        COMMAND jsom_benchmarks --benchmark_min_time=1.0s
        DEPENDS jsom_benchmarks
        COMMENT "Running performance benchmarks"
    )
    
    # Specific target for number-heavy performance validation
    add_custom_target(validate_number_performance
        COMMAND jsom_benchmarks --benchmark_filter="NumberHeavy" --benchmark_min_time=2.0s
        DEPENDS jsom_benchmarks
        COMMENT "Validating number-heavy JSON performance"
    )
    
    # Parse-serialize workflow validation
    add_custom_target(validate_parse_serialize
        COMMAND jsom_benchmarks --benchmark_filter="ParseSerialize" --benchmark_min_time=2.0s
        DEPENDS jsom_benchmarks
        COMMENT "Validating parse-serialize performance"
    )
    
    message(STATUS "Performance benchmarks enabled")
endif()

# ================================
# Code Quality Tools
# ================================

# Find source files for formatting and linting
file(GLOB_RECURSE JSOM_SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/*.cpp
)

# clang-format (essential for AI/DEVELOPMENT_STANDARDS/CODE_QUALITY.md compliance)
find_program(CLANG_FORMAT NAMES clang-format)
if(CLANG_FORMAT)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i ${JSOM_SOURCES}
        COMMENT "Formatting source files with clang-format"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    message(STATUS "clang-format available - use 'make format' for code formatting")
else()
    add_custom_target(format
        COMMAND ${CMAKE_COMMAND} -E echo "clang-format not found - install for code formatting"
    )
endif()

# clang-tidy (essential for AI/DEVELOPMENT_STANDARDS/CODE_QUALITY.md compliance)
find_program(CLANG_TIDY NAMES clang-tidy)
if(CLANG_TIDY)
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY} ${JSOM_SOURCES} -- -x c++ -I${CMAKE_CURRENT_SOURCE_DIR}/include -std=c++17
        COMMENT "Running clang-tidy on source files"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    message(STATUS "clang-tidy available - use 'make tidy' for static analysis")
else()
    add_custom_target(tidy
        COMMAND ${CMAKE_COMMAND} -E echo "clang-tidy not found - install for static analysis"
    )
endif()

# Combined quality check target
add_custom_target(quality_check
    COMMAND ${CMAKE_COMMAND} --build . --target format
    COMMAND ${CMAKE_COMMAND} --build . --target tidy
    COMMENT "Running all code quality checks"
)

# ================================
# Fuzzing and Memory Checking
# ================================

# Find fuzzing tools
find_program(CLANG_PLUS_PLUS NAMES clang++)
find_program(VALGRIND NAMES valgrind)

# Fuzzing target with libFuzzer
if(CLANG_PLUS_PLUS)
    add_custom_target(build_fuzzer
        COMMAND ${CLANG_PLUS_PLUS} -g -O1 -fsanitize=fuzzer,address,undefined
                -I${CMAKE_CURRENT_SOURCE_DIR}/include
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/fuzzer.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/json_document_pointer.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/json_document_formatting.cpp
                -o fuzz_jsom
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Building fuzzer with AddressSanitizer and UndefinedBehaviorSanitizer"
    )
    
    # Quick fuzzing (1 minute)
    add_custom_target(fuzz_quick
        COMMAND ${CMAKE_COMMAND} -E make_directory corpus
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/fuzz_jsom corpus -max_total_time=60
        DEPENDS build_fuzzer
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running quick fuzz test (1 minute)"
    )
    
    # Standard fuzzing (10 minutes)
    add_custom_target(fuzz
        COMMAND ${CMAKE_COMMAND} -E make_directory corpus
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/fuzz_jsom corpus -max_total_time=600
        DEPENDS build_fuzzer
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running standard fuzz test (10 minutes)"
    )
    
    # Long fuzzing (1 hour)
    add_custom_target(fuzz_long
        COMMAND ${CMAKE_COMMAND} -E make_directory corpus
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/fuzz_jsom corpus -max_total_time=3600
        DEPENDS build_fuzzer
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running extended fuzz test (1 hour)"
    )
    
    message(STATUS "Fuzzing available - use 'make fuzz' for 10-minute test, 'make fuzz_long' for 1-hour test")
else()
    add_custom_target(fuzz
        COMMAND ${CMAKE_COMMAND} -E echo "clang++ not found - install for fuzzing support"
    )
endif()

# Valgrind memory checking
if(VALGRIND)
    add_custom_target(memcheck
        COMMAND ${VALGRIND} --leak-check=full --show-leak-kinds=all 
                --track-origins=yes --error-exitcode=1
                ./jsom format ${CMAKE_CURRENT_SOURCE_DIR}/test_edge_cases.json > /dev/null
        DEPENDS jsom
        COMMENT "Running valgrind memory check"
    )
    
    add_custom_target(memcheck_tests
        COMMAND ${VALGRIND} --leak-check=full --error-exitcode=1
                ./jsom_tests
        DEPENDS jsom_tests
        COMMENT "Running valgrind on all tests"
    )
    
    message(STATUS "Valgrind available - use 'make memcheck' for memory checking")
else()
    add_custom_target(memcheck
        COMMAND ${CMAKE_COMMAND} -E echo "valgrind not found - install for memory checking"
    )
endif()

# Combined security testing target
add_custom_target(security_check
    COMMAND ${CMAKE_COMMAND} --build . --target fuzz_quick
    COMMAND ${CMAKE_COMMAND} --build . --target memcheck
    COMMENT "Running security checks (fuzzing + memory check)"
)

# ================================
# CLI Tool
# ================================

# JSOM CLI executable
add_executable(jsom
    src/jsom_cli.cpp
    src/json_document_pointer.cpp
)

target_link_libraries(jsom
    jsom_lib
)

target_include_directories(jsom PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Install target for CLI
install(TARGETS jsom
    RUNTIME DESTINATION bin
)

# ================================
# Development Convenience Targets
# ================================

# Complete development cycle target
add_custom_target(dev_cycle
    COMMAND ${CMAKE_COMMAND} --build . --target quality_check
    COMMAND ${CMAKE_COMMAND} --build . --target run_tests
    COMMAND ${CMAKE_COMMAND} --build . --target run_benchmarks
    COMMENT "Complete development cycle: format, lint, test, benchmark"
)

# Validation target (for final acceptance testing)
add_custom_target(validation
    COMMAND ${CMAKE_COMMAND} --build . --target run_tests
    COMMAND ${CMAKE_COMMAND} --build . --target validate_number_performance
    COMMAND ${CMAKE_COMMAND} --build . --target validate_parse_serialize
    COMMENT "Final acceptance validation"
)

# ================================
# Configuration Summary
# ================================

message(STATUS "")
message(STATUS "JSOM Configuration Summary:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Top-level build: ${JSOM_IS_TOP_LEVEL}")
message(STATUS "  Tests: ${JSOM_BUILD_TESTS}")
message(STATUS "  Benchmarks: ${JSOM_BUILD_BENCHMARKS}")
message(STATUS "")
message(STATUS "Available targets:")
if(JSOM_BUILD_TESTS)
    message(STATUS "  make jsom_tests            - Build test suite")
    message(STATUS "  make run_tests             - Run all tests")
endif()
if(JSOM_BUILD_BENCHMARKS)
    message(STATUS "  make jsom_benchmarks       - Build benchmarks")
    message(STATUS "  make run_benchmarks        - Run all benchmarks")
    message(STATUS "  make validate_number_performance - Test number-heavy performance")
    message(STATUS "  make validate_parse_serialize - Test parse-serialize performance")
endif()
message(STATUS "  make jsom                  - Build CLI tool")
message(STATUS "  make format                - Format code with clang-format")
message(STATUS "  make tidy                  - Lint code with clang-tidy")
message(STATUS "  make fuzz_quick            - Run 1-minute fuzz test")
message(STATUS "  make fuzz                  - Run 10-minute fuzz test")
message(STATUS "  make fuzz_long             - Run 1-hour fuzz test")
message(STATUS "  make memcheck              - Run valgrind on CLI")
if(JSOM_BUILD_TESTS)
    message(STATUS "  make memcheck_tests        - Run valgrind on test suite")
endif()
message(STATUS "  make security_check        - Run fuzzing + memory checks")
if(JSOM_BUILD_TESTS AND JSOM_BUILD_BENCHMARKS)
    message(STATUS "  make dev_cycle             - Complete development cycle")
    message(STATUS "  make validation            - Final acceptance testing")
endif()
message(STATUS "")