# JSOM CMakeLists.txt
# 
# This file provides the essential CMake configuration for JSOM.
# It focuses on core development needs: building, testing, benchmarking, and code quality.

cmake_minimum_required(VERSION 3.10)

project(JSOM VERSION 4.0.0 LANGUAGES CXX)

# ================================
# Version — single source of truth
# ================================
# PROJECT_VERSION, set by project() above, is the ONLY place the version is written.
# `cmake/version.hpp.in` is configured here, so everything that needs the version —
# the CLI, tests, any consumer including <jsom/version.hpp> — reads the same value.
# To release: edit the project() line, then tag.
set(JSOM_GENERATED_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.hpp.in
    ${JSOM_GENERATED_INCLUDE_DIR}/jsom/version.hpp
    @ONLY
)

# Detect if JSOM is being built standalone or as a subdirectory
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(JSOM_IS_TOP_LEVEL ON)
else()
    set(JSOM_IS_TOP_LEVEL OFF)
endif()

# Default to an optimized build unless the user chose a build type
# (OPTIMIZATIONS.md #2): an unset CMAKE_BUILD_TYPE silently compiles at -O0,
# which made the documented `cmake -B build` flow 5-20x slower than it should
# be. An explicitly chosen type (e.g. -DCMAKE_BUILD_TYPE=Debug) still wins.
#
# TOP LEVEL ONLY: CMAKE_BUILD_TYPE is a CACHE variable, so a subproject that
# sets it configures its CONSUMER too — Computo fetched JSOM and its Pages
# workflow, which passes no build type, silently compiled at -O3 for months
# (card t_772eabd9). JSOM's own default is unchanged for JSOM's own builds.
if(JSOM_IS_TOP_LEVEL AND NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Install prefix: default to the user's ~/.local so `install` needs no root.
# Value-based check (not CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT) so it
# also applies to EXISTING build dirs whose cache already holds /usr/local.
# Override at install time with: cmake --install build --prefix <path>
# TOP LEVEL ONLY, for the same reason as the build type above: as a subproject
# this would rewrite the CONSUMER's install prefix (card t_772eabd9).
if(JSOM_IS_TOP_LEVEL AND CMAKE_INSTALL_PREFIX STREQUAL "/usr/local")
    set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.local" CACHE PATH "Install prefix" FORCE)
endif()

# Compiler configuration
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compile database: JSOM's own developer aid (clang-tidy, IDE), so it is asked for only
# by JSOM's own build. Like the two defaults above this is the same class of leak: the
# variable is a normal one, so it does not reach a consumer's directory, but JSOM enabling
# it still writes JSOM's translation units into the CONSUMER's compile_commands.json — so
# a consumer that sets nothing gets a database that holds JSOM's sources and NONE of its
# own, and a tidy gate reading that file would lint JSOM and skip the consumer (card
# t_772eabd9). A consumer that wants the database asks for it itself - jsonTools and
# Computo both pass -DCMAKE_EXPORT_COMPILE_COMMANDS=ON, and JSOM's TUs are then in it
# because the consumer's directory scope reaches this subdirectory.
if(JSOM_IS_TOP_LEVEL)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

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
    $<BUILD_INTERFACE:${JSOM_GENERATED_INCLUDE_DIR}>
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
# The libFuzzer target needs clang AND the JSONFuzz sibling repo (structure-aware
# generator + mutators + oracles). OFF by default so JSOM's normal build and its
# `pristine` stage do not need the sibling repo or a network fetch. The `fuzz` stage in
# tools/ci.sh turns it on in its own build dir. JSONFuzz comes in via FetchContent pinned
# to a pushed commit, with -DJSONFUZZ_SOURCE_DIR=<path> as the offline override (the
# pattern JSONFuzz itself uses for nlohmann).
option(JSOM_BUILD_FUZZING "Build the libFuzzer target (clang only; needs JSONFuzz)" OFF)
set(JSOM_SANITIZE "" CACHE STRING
    "Sanitizers to build with: ON/address (AddressSanitizer + UBSan), thread (ThreadSanitizer), or empty")
if(JSOM_SANITIZE)
    if(JSOM_SANITIZE STREQUAL "thread")
        # ThreadSanitizer: the only detector for the data races a shared document can
        # suffer. Cannot be combined with ASan in one build.
        set(JSOM_SANITIZE_FLAGS "-fsanitize=thread")
    else()
        set(JSOM_SANITIZE_FLAGS "-fsanitize=address,undefined")
    endif()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${JSOM_SANITIZE_FLAGS} -fno-omit-frame-pointer -g")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${JSOM_SANITIZE_FLAGS}")
    message(STATUS "  Sanitizers: ${JSOM_SANITIZE_FLAGS}")
endif()

if(JSOM_IS_TOP_LEVEL)
    option(JSOM_BUILD_TESTS "Build JSOM tests" ON)
    option(JSOM_INSTALL "Install JSOM's CLI and artifacts" ON)
else()
    option(JSOM_BUILD_TESTS "Build JSOM tests" OFF)
    option(JSOM_INSTALL "Install JSOM's CLI and artifacts" OFF)
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
    tests/test_api_compatibility.cpp     # API examples validation

    # JSON Pointer tests
    tests/test_json_pointer.cpp          # JSON Pointer functionality tests

    # Performance regression tests
    tests/test_performance_regression.cpp

    # Memory safety tests
    tests/test_memory_safety.cpp

    # Format preservation tests
    tests/test_format_preservation.cpp
    tests/test_formatter_invariants.cpp   # formatting never changes meaning; idempotent
    tests/test_formatter_options.cpp      # one test per formatting option
    tests/test_utf8.cpp                   # UTF-8 decoder used by escape_unicode

    # Unicode escape tests
    tests/test_unicode_escapes.cpp       # Unicode escape sequence handling

    # Ergonomic construction tests
    tests/test_ergonomic_construction.cpp # Factory methods and direct constructors

    # Cache invalidation tests
    tests/test_navigation_freshness.cpp  # lookups always see the current document

    # Iteration tests
    tests/test_iteration.cpp             # Range-for, items(), keys()

    # Comparison operator tests
    tests/test_comparison.cpp            # Equality and ordering operators

    # Comment-tolerant parsing tests
    tests/test_comments.cpp              # // and /* */ comment support

    # Nesting-depth limit tests (RFC 8259 §9 resource limits)
    tests/test_nesting_limits.cpp        # no input may exhaust the stack

    # Opt-in number validation tests
    tests/test_number_grammar.cpp        # section 6 enforced by default
    tests/test_number_loose.cpp          # the loose-numbers extension switch

    # Lexical conformance (always on: escapes, control chars, whitespace)
    tests/test_lexical_conformance.cpp   # the spec rules, enforced regardless of options
    tests/test_const_access_safety.cpp   # const reads from several threads (TSan stage)
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
# Tests read vendored data (the RFC 8259 corpus) from the source tree, so the path is
# baked in: with a relative path the corpus test passed from the project root and failed
# under `ctest`, which runs from the build directory.
target_compile_definitions(jsom_tests PRIVATE JSOM_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

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

# Thread-safety probe. Not a gtest binary on purpose: the `tsan` stage in tools/ci.sh
# builds this one file with ThreadSanitizer, and instrumenting the test framework too
# would cost minutes per run. It is built in every configuration, so a break here cannot
# hide until the thread gate runs.
find_package(Threads REQUIRED)
add_executable(thread_safety_probe tests/thread_safety_probe.cpp)
target_link_libraries(thread_safety_probe PRIVATE jsom_lib Threads::Threads)
target_include_directories(thread_safety_probe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

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

# Find source files for formatting and linting.
# Keep this list in step with the format gate in tools/ci.sh (ci_sources): the gate
# checks exactly what `make format` would rewrite.
file(GLOB_RECURSE JSOM_SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/*.cpp
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
    # Analyze project code ONLY (src + include), driven by compile_commands.json
    # (-p). Never feed test files to tidy: gtest macros generate thousands of
    # false positives and the gtest include dirs are not on the tool's path.
    file(GLOB_RECURSE JSOM_TIDY_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
    )
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY} -p ${CMAKE_BINARY_DIR}
                -header-filter=${CMAKE_CURRENT_SOURCE_DIR}/include
                ${JSOM_TIDY_SOURCES}
        COMMENT "Running clang-tidy on project sources (CODING_STANDARDS.md gate)"
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

# Fuzzing target with libFuzzer. Gated behind JSOM_BUILD_FUZZING (default OFF): the
# harness (tests/fuzzer.cpp) now drives JSONFuzz's structure-aware generator, mutators
# and oracles, so it needs the JSONFuzz sibling repo. The normal build and the `pristine`
# stage do not enable this option, so they never fetch JSONFuzz.
if(JSOM_BUILD_FUZZING)
    # JSONFuzz: FetchContent pinned to a pushed commit, with -DJSONFUZZ_SOURCE_DIR=<path>
    # as the offline override (the pattern JSONFuzz itself uses for nlohmann). The pinned
    # commit is the HEAD this adapter was written and tested against.
    #
    # DEVIATION from a plain FetchContent_MakeAvailable: JSONFuzz's CMakeLists defines a
    # target named `format` (and `tidy`-adjacent helpers) that collides with JSOM's own
    # `format` target when JSONFuzz is pulled in as a subdirectory — measured, the
    # configure dies with "add_custom_target cannot create target 'format' because
    # another target with the same name already exists". So instead of calling JSONFuzz's
    # CMakeLists we fetch its SOURCE and build the jsonfuzz_core translation units
    # directly here, replicating only the two things that CMakeLists does for them: the
    # generated <jsonfuzz/version.hpp> and the nlohmann dependency. The SUT interface is
    # strings-only, so jsonfuzz_core's nlohmann dependency stays PRIVATE and never leaks
    # into JSOM's targets.
    if(DEFINED JSONFUZZ_SOURCE_DIR)
        message(STATUS "JSONFuzz: using source dir ${JSONFUZZ_SOURCE_DIR}")
        set(JSONFUZZ_SRC ${JSONFUZZ_SOURCE_DIR})
    else()
        FetchContent_Declare(
            jsonfuzz
            GIT_REPOSITORY https://github.com/HarryPehkonen/JSONFuzz.git
            GIT_TAG        b1fbabb  # pin: the commit the JSOM adapter targets
        )
        FetchContent_GetProperties(jsonfuzz)
        if(NOT jsonfuzz_POPULATED)
            FetchContent_Populate(jsonfuzz)
        endif()
        set(JSONFUZZ_SRC ${jsonfuzz_SOURCE_DIR})
    endif()

    # Replicate JSONFuzz's generated version header (its CMakeLists configure_file).
    set(JSONFUZZ_GENERATED_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/jsonfuzz-generated)
    set(PROJECT_VERSION_MAJOR 0)
    set(PROJECT_VERSION_MINOR 1)
    set(PROJECT_VERSION_PATCH 0)
    set(PROJECT_VERSION "0.1.0")
    configure_file(
        ${JSONFUZZ_SRC}/cmake/version.hpp.in
        ${JSONFUZZ_GENERATED_INCLUDE_DIR}/jsonfuzz/version.hpp
        @ONLY
    )

    # nlohmann/json is jsonfuzz_core's own model (PRIVATE, never a SUT).
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
    )
    FetchContent_MakeAvailable(nlohmann_json)

    # jsonfuzz_core, built from JSONFuzz's sources (see the deviation note above).
    add_library(jsonfuzz_core
        ${JSONFUZZ_SRC}/src/jsonfuzz_core.cpp
        ${JSONFUZZ_SRC}/src/jsonfuzz_sut.cpp
        ${JSONFUZZ_SRC}/src/jsonfuzz_generator.cpp
        ${JSONFUZZ_SRC}/src/jsonfuzz_mutate.cpp
        ${JSONFUZZ_SRC}/src/jsonfuzz_oracle.cpp
        ${JSONFUZZ_SRC}/src/jsonfuzz_readings.cpp
    )
    target_include_directories(jsonfuzz_core PUBLIC
        ${JSONFUZZ_SRC}/include
        ${JSONFUZZ_GENERATED_INCLUDE_DIR}
    )
    target_link_libraries(jsonfuzz_core PRIVATE nlohmann_json::nlohmann_json)
    target_compile_features(jsonfuzz_core PRIVATE cxx_std_17)

    # The JSOM JSONFuzz adapter: a jsonfuzz::SystemUnderTest wrapping jsom::JsonDocument.
    add_library(jsom_jsonfuzz_adapter fuzz/jsonfuzz_jsom.cpp)
    target_include_directories(jsom_jsonfuzz_adapter PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/fuzz
    )
    target_link_libraries(jsom_jsonfuzz_adapter PUBLIC jsom_lib jsonfuzz_core)
    target_compile_features(jsom_jsonfuzz_adapter PRIVATE cxx_std_17)

    # The harness as a compile-only OBJECT library in this build, so the tidy gate has a
    # translation unit for it (the same pattern JSONFuzz uses for its fuzz target). The
    # source stays g++-clean: it must not include libFuzzer-only headers.
    add_library(jsom_fuzz_obj OBJECT tests/fuzzer.cpp)
    target_include_directories(jsom_fuzz_obj PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/fuzz
        ${JSONFUZZ_SRC}/include
        ${JSONFUZZ_GENERATED_INCLUDE_DIR}
    )
    target_compile_features(jsom_fuzz_obj PRIVATE cxx_std_17)

    # The libFuzzer executable (clang only). The custom mutator and the structured
    # readings live in tests/fuzzer.cpp; jsonfuzz_core provides jsonfuzz::mutate and the
    # oracles, jsom_jsonfuzz_adapter provides the SUT.
    add_executable(fuzz_jsom tests/fuzzer.cpp)
    target_link_libraries(fuzz_jsom PRIVATE jsom_lib jsom_jsonfuzz_adapter jsonfuzz_core)
    target_include_directories(fuzz_jsom PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/fuzz
    )
    target_compile_options(fuzz_jsom PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer -g)
    target_link_options(fuzz_jsom PRIVATE -fsanitize=fuzzer,address,undefined)

    # Adapter tests (need JSONFuzz, so they live in this gated build, not jsom_tests).
    if(JSOM_BUILD_TESTS)
        add_executable(jsom_fuzz_tests tests/test_jsonfuzz_adapter.cpp)
        target_link_libraries(jsom_fuzz_tests PRIVATE
            jsom_lib jsom_jsonfuzz_adapter jsonfuzz_core gtest gtest_main)
        target_include_directories(jsom_fuzz_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/fuzz
        )
        add_custom_target(run_fuzz_tests
            COMMAND jsom_fuzz_tests
            DEPENDS jsom_fuzz_tests
            COMMENT "Running the JSONFuzz adapter tests"
        )
    endif()

    # Quick fuzzing (1 minute)
    add_custom_target(fuzz_quick
        COMMAND ${CMAKE_COMMAND} -E make_directory corpus
        COMMAND $<TARGET_FILE:fuzz_jsom> corpus ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/seeds
                -dict=${CMAKE_CURRENT_SOURCE_DIR}/fuzz/jsom.dict
                -artifact_prefix=corpus/
                -max_total_time=60
        DEPENDS fuzz_jsom
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running quick fuzz test (1 minute)"
    )

    # Standard fuzzing (10 minutes)
    add_custom_target(fuzz
        COMMAND ${CMAKE_COMMAND} -E make_directory corpus
        COMMAND $<TARGET_FILE:fuzz_jsom> corpus ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/seeds
                -dict=${CMAKE_CURRENT_SOURCE_DIR}/fuzz/jsom.dict
                -artifact_prefix=corpus/
                -max_total_time=600
        DEPENDS fuzz_jsom
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running standard fuzz test (10 minutes)"
    )

    # Long fuzzing (1 hour)
    add_custom_target(fuzz_long
        COMMAND ${CMAKE_COMMAND} -E make_directory corpus
        COMMAND $<TARGET_FILE:fuzz_jsom> corpus ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/seeds
                -dict=${CMAKE_CURRENT_SOURCE_DIR}/fuzz/jsom.dict
                -artifact_prefix=corpus/
                -max_total_time=3600
        DEPENDS fuzz_jsom
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running extended fuzz test (1 hour)"
    )

    message(STATUS "Fuzzing available - use 'make fuzz' for 10-minute test, 'make fuzz_long' for 1-hour test")
else()
    add_custom_target(fuzz
        COMMAND ${CMAKE_COMMAND} -E echo "fuzzing disabled - configure with -DJSOM_BUILD_FUZZING=ON (needs clang and JSONFuzz)"
    )
endif()

# ================================
# RFC 8259 conformance suite
# ================================
# Corpus: third_party/json_test_suite (nst/JSONTestSuite, MIT, (c) 2016 Nicolas Seriot).
# y_* must be accepted, n_* must be rejected, i_* is the suite abstaining.
# A failure here is a conformance bug, not a flaky test, so this target exits
# non-zero — which is why it is deliberately NOT part of the default build: run
# it with `cmake --build build --target run_conformance`.
add_executable(jsom_conformance
    tools/conformance_runner.cpp
    src/json_document_pointer.cpp
    src/json_document_formatting.cpp
)
target_include_directories(jsom_conformance PRIVATE include)
target_compile_options(jsom_conformance PRIVATE -Wall -Wextra -Wpedantic -Werror)
# The vendored corpus lives in the source tree; bake the path in so the runner (and the
# test that reads the same files) works from any working directory.
target_compile_definitions(jsom_conformance PRIVATE JSOM_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

add_custom_target(run_conformance
    COMMAND jsom_conformance ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json_test_suite
    DEPENDS jsom_conformance
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running the RFC 8259 conformance suite (failures are conformance bugs)"
)

add_custom_target(conformance_report
    COMMAND jsom_conformance ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json_test_suite --list
    DEPENDS jsom_conformance
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Conformance verdict including the i_ decisions the spec leaves open"
)

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

# Warnings as errors for JSOM's OWN targets only (CODING_STANDARDS.md rule 11).
# Fetched dependencies (googletest, benchmark, nlohmann_json) keep their own
# flags — never apply -Werror to them.
foreach(jsom_target jsom_lib jsom jsom_tests jsom_benchmarks thread_safety_probe
        jsom_jsonfuzz_adapter jsom_fuzz_obj fuzz_jsom jsom_fuzz_tests)
    if(TARGET ${jsom_target})
        target_compile_options(${jsom_target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endforeach()

# gcc < 15: -Wmaybe-uninitialized false-positives on std::variant move/assign
# internals in optimized (-O2/-O3 -DNDEBUG) builds (gcc#101905 family; fixed
# in gcc 15+). Trips in test TUs that construct+move JsonDocuments inside
# gtest virtual TestBody functions. Scope: tests target only, GNU < 15 —
# library and CLI keep the full -Werror set on every compiler.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
   AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15
   AND TARGET jsom_tests)
    target_compile_options(jsom_tests PRIVATE -Wno-maybe-uninitialized)
endif()

# Install target for CLI
if(JSOM_INSTALL)
    install(TARGETS jsom
        RUNTIME DESTINATION bin
    )
endif()

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
