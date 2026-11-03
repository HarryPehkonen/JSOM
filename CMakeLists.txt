# JSOM CMakeLists.txt
# 
# This file provides the essential CMake configuration for JSOM.
# It focuses on core development needs: building, testing, benchmarking, and code quality.

cmake_minimum_required(VERSION 3.10)

project(JSOM VERSION 3.2.0 LANGUAGES CXX)

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
    # Release flags. PORTABLE BY DEFAULT. This used to read
    # "-O3 -DNDEBUG -march=native", which enables whatever instruction set the
    # BUILDING host happens to have. JSOM is published as packages that run on other
    # machines, so that made every published binary a SIGILL waiting for a different
    # CPU, and it made two builds of the same source incomparable — the benchmark and
    # fuzz evidence in OPTIMIZATIONS.md is only reproducible with fixed flags.
    # JSOM_NATIVE_TUNING restores the tuning half for local speed work:
    # -mtune=native changes scheduling but never enables an instruction the target
    # CPU may lack, so a build stays distributable. Re-measure anything recorded in
    # OPTIMIZATIONS.md before trusting an old number after this change.
    option(JSOM_NATIVE_TUNING "Tune Release builds for this machine (-mtune=native)" OFF)
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    if(JSOM_NATIVE_TUNING)
        string(APPEND CMAKE_CXX_FLAGS_RELEASE " -mtune=native")
    endif()
endif()

# FetchContent for dependencies
include(FetchContent)

# GNUInstallDirs gives the canonical install directories (and the multiarch lib
# directory on Debian) so the installed CMake configs can describe the same layout
# the packages actually install. Needed before the target's INSTALL_INTERFACE below.
include(GNUInstallDirs)

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
    # Required for install(EXPORT): without an INSTALL_INTERFACE the exported
    # JSOMTargets.cmake would carry build-tree paths that do not exist on a
    # consumer's machine.
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
# PUBLIC, not PRIVATE: the public headers require C++17, so a consumer that does not
# ask for it itself must inherit the requirement from the link line.
target_compile_features(jsom_lib PUBLIC cxx_std_17)

# Add modern C++ compile flags for the library
target_compile_options(jsom_lib INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

# The installed library is libjsom.so.3 / libjsom.a — "jsom", not the internal
# target name "jsom_lib" (which leaked into the hand-built packages as
# libjsom_lib.a and made pkg-config consumers link -ljsom_lib).
#
# VERSION/SOVERSION give a shared build a real SONAME. Without them a shared build
# produces an unversioned libjsom.so, which no distribution package can name
# correctly (Debian policy: the package carries the soname's major — libjsom3) and
# no soname-based dependency can track. Both values come from the same single
# source as everything else: the project() line above.
set_target_properties(jsom_lib PROPERTIES
    OUTPUT_NAME jsom
    # EXPORT_NAME is what makes the installed package export JSOM::jsom. The
    # ALIAS below is build-tree only — install(EXPORT ... NAMESPACE JSOM::)
    # publishes the TARGET name, so without this the package exported
    # JSOM::jsom_lib and a consumer following the documented
    # target_link_libraries(app PRIVATE JSOM::jsom) failed at configure with
    # 'Target "app" links to JSOM::jsom but the target was not found.'
    EXPORT_NAME jsom
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
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
# scripts/gate.sh turns it on in its own build dir. JSONFuzz comes in via FetchContent pinned
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
    tests/test_formatter_layout_matrix.cpp # widened documents x options net (pre-refactor)
    tests/test_formatter_layout_width.cpp # TDD for the extracted available_line_width() helper
    tests/test_utf8.cpp                   # UTF-8 decoder used by escape_unicode
    tests/test_escaping.cpp               # one escaping routine: agreement, stream state, round trip

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

    # Number grammar tests: §6 enforcement by default, and the loose-numbers option
    tests/test_number_grammar.cpp        # section 6 enforced by default
    tests/test_number_loose.cpp          # the loose-numbers extension switch

    # Lexical conformance (always on: escapes, control chars, whitespace)
    tests/test_lexical_conformance.cpp   # the spec rules, enforced regardless of options
    tests/test_const_access_safety.cpp   # const reads from several threads (TSan stage)

    # Typed parse/format errors
    tests/test_parse_error.cpp           # jsom::ParseError / ParseErrorCode
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

# Thread-safety probe. Not a gtest binary on purpose: the `tsan` stage in scripts/gate.sh
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
# Keep this list in step with the format gate in scripts/gate.sh (ci_sources): the gate
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
# harness (tests/fuzzer.cpp) drives JSONFuzz's structure-aware generator, mutators and
# oracles, so it needs JSONFuzz. The normal build and the `pristine` stage do not enable
# this option, so they never fetch it.
if(JSOM_BUILD_FUZZING)
    # JSONFuzz: fetched from GitHub, pinned to the release tag this adapter targets.
    #
    # JSONFuzz::core is the WHOLE interface. Its nlohmann dependency is PRIVATE (the SUT
    # interface is strings-only, so no model type reaches JSOM), and JSOM does not list
    # JSONFuzz's translation units — a source added or renamed upstream arrives by itself
    # instead of silently falling out of this build. That hand-list was the drift risk:
    # JSONFuzz now guards its own developer targets behind JSONFUZZ_IS_TOP_LEVEL, so
    # taking it in as a subdirectory is safe, and its
    # tools/kit-probes/consumer-subdirectory.sh holds the contract this line depends on
    # (a consumer gets JSONFuzz::core and nothing else).
    #
    # Offline, or while changing both repos: -DFETCHCONTENT_SOURCE_DIR_JSONFUZZ=<path>,
    # which CI passes through as CI_FUZZ_JSONFUZZ_DIR.
    if(DEFINED JSONFUZZ_SOURCE_DIR AND NOT DEFINED FETCHCONTENT_SOURCE_DIR_JSONFUZZ)
        set(FETCHCONTENT_SOURCE_DIR_JSONFUZZ "${JSONFUZZ_SOURCE_DIR}")
    endif()
    FetchContent_Declare(
        jsonfuzz
        GIT_REPOSITORY https://github.com/HarryPehkonen/JSONFuzz.git
        GIT_TAG        v0.2.0  # pin: the release that carries the seven oracle laws
    )
    FetchContent_MakeAvailable(jsonfuzz)
    message(STATUS "JSONFuzz: JSONFuzz::core from ${jsonfuzz_SOURCE_DIR}")

    # The JSOM JSONFuzz adapter: a jsonfuzz::SystemUnderTest wrapping jsom::JsonDocument.
    add_library(jsom_jsonfuzz_adapter fuzz/jsonfuzz_jsom.cpp)
    target_include_directories(jsom_jsonfuzz_adapter PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/fuzz
    )
    target_link_libraries(jsom_jsonfuzz_adapter PUBLIC jsom_lib JSONFuzz::core)
    target_compile_features(jsom_jsonfuzz_adapter PRIVATE cxx_std_17)

    # The harness as a compile-only OBJECT library in this build, so the tidy gate has a
    # translation unit for it (the same pattern JSONFuzz uses for its fuzz target). The
    # source stays g++-clean: it must not include libFuzzer-only headers.
    add_library(jsom_fuzz_obj OBJECT tests/fuzzer.cpp)
    # Usage requirements (including JSONFuzz's include dirs) come from the adapter target,
    # so this file names no path inside JSONFuzz's tree.
    target_link_libraries(jsom_fuzz_obj PRIVATE jsom_jsonfuzz_adapter)
    target_compile_features(jsom_fuzz_obj PRIVATE cxx_std_17)

    # The libFuzzer executable (clang only). The custom mutator and the structured
    # readings live in tests/fuzzer.cpp; jsonfuzz_core provides jsonfuzz::mutate and the
    # oracles, jsom_jsonfuzz_adapter provides the SUT.
    add_executable(fuzz_jsom tests/fuzzer.cpp)
    target_link_libraries(fuzz_jsom PRIVATE jsom_lib jsom_jsonfuzz_adapter JSONFuzz::core)
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
            jsom_lib jsom_jsonfuzz_adapter JSONFuzz::core gtest gtest_main)
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

# Install rules for the CLI, the library, the headers, the CMake package, the
# pkg-config file and the man pages.
#
# Three components, one per package (see cmake/CPack.cmake):
#   cli      the jsom(1) binary and its man page
#   runtime  the shared library with its soname link
#   dev      headers, the generated version.hpp, CMake package config,
#            pkg-config file, the linker symlink and the jsom(3) man page
#
# The runtime/dev split is the one that matters. NAMELINK_COMPONENT puts
# libjsom.so.3 — the soname link every dynamically linked consumer needs at
# startup — in the RUNTIME package and the bare libjsom.so symlink in dev. A
# hand-built package that shipped only the versioned file made every consumer die
# with "libjsom.so.3: cannot open shared object file" while the package itself
# looked complete; scripts/package.sh consumes the built packages to prove the
# split holds.
if(JSOM_INSTALL)
    include(CMakePackageConfigHelpers)

    set(JSOM_COMPONENT_CLI cli)
    set(JSOM_COMPONENT_RUNTIME runtime)
    set(JSOM_COMPONENT_DEV dev)

    install(TARGETS jsom
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                COMPONENT ${JSOM_COMPONENT_CLI}
    )

    install(TARGETS jsom_lib
        EXPORT JSOMTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
                COMPONENT ${JSOM_COMPONENT_RUNTIME}
                NAMELINK_COMPONENT ${JSOM_COMPONENT_DEV}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
                COMPONENT ${JSOM_COMPONENT_DEV}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                COMPONENT ${JSOM_COMPONENT_RUNTIME}
    )

    # The public headers, and the version header CMake generates from project().
    # COMPONENT precedes FILES_MATCHING PATTERN deliberately: the other order is
    # accepted by CMake and then silently drops the component assignment.
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/jsom
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        COMPONENT ${JSOM_COMPONENT_DEV}
        FILES_MATCHING PATTERN "*.hpp"
    )
    install(FILES ${JSOM_GENERATED_INCLUDE_DIR}/jsom/version.hpp
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/jsom
        COMPONENT ${JSOM_COMPONENT_DEV}
    )

    # CMake package: find_package(JSOM CONFIG REQUIRED) -> JSOM::jsom
    set(JSOM_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/JSOM)
    install(EXPORT JSOMTargets
        FILE JSOMTargets.cmake
        NAMESPACE JSOM::
        DESTINATION ${JSOM_INSTALL_CMAKEDIR}
        COMPONENT ${JSOM_COMPONENT_DEV}
    )
    configure_package_config_file(
        cmake/JSOMConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/JSOMConfig.cmake
        INSTALL_DESTINATION ${JSOM_INSTALL_CMAKEDIR}
    )
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/JSOMConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/JSOMConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/JSOMConfigVersion.cmake
        DESTINATION ${JSOM_INSTALL_CMAKEDIR}
        COMPONENT ${JSOM_COMPONENT_DEV}
    )

    # pkg-config, for build systems that do not use CMake.
    set(JSOM_PC_PREFIX "${CMAKE_INSTALL_PREFIX}")
    set(JSOM_PC_LIBDIR "\${prefix}/${CMAKE_INSTALL_LIBDIR}")
    set(JSOM_PC_INCLUDEDIR "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
    set(JSOM_PC_DESCRIPTION
        "JSOM — JavaScript Object Models: a C++17 JSON library with RFC 6901 JSON Pointer navigation")
    # The public headers require C++17; a consumer building without CMake has no
    # other place to learn that.
    set(JSOM_PC_CFLAGS " -std=c++17")
    configure_file(cmake/jsom.pc.in ${CMAKE_CURRENT_BINARY_DIR}/jsom.pc @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/jsom.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
        COMPONENT ${JSOM_COMPONENT_DEV}
    )

    # Man pages are generated from man/*.scd by scdoc, so no checked-in .1/.3 can
    # drift from its source. A development build on a machine without scdoc still
    # works; scripts/package.sh asserts scdoc is present for a package build.
    find_program(SCDOC_EXECUTABLE scdoc)
    if(SCDOC_EXECUTABLE)
        add_custom_target(jsom-manpages ALL
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/man"
            # Two commands, no pipe: `scdoc < in | gzip > out` hides an scdoc error
            # behind gzip's success, which is how a page that failed to compile was
            # once shipped. Each COMMAND's exit status is checked by the build.
            COMMAND sh -c "${SCDOC_EXECUTABLE} < '${CMAKE_CURRENT_SOURCE_DIR}/man/jsom.1.scd' > '${CMAKE_CURRENT_BINARY_DIR}/man/jsom.1'"
            COMMAND gzip -9nf "${CMAKE_CURRENT_BINARY_DIR}/man/jsom.1"
            COMMAND sh -c "${SCDOC_EXECUTABLE} < '${CMAKE_CURRENT_SOURCE_DIR}/man/jsom.3.scd' > '${CMAKE_CURRENT_BINARY_DIR}/man/jsom.3'"
            COMMAND gzip -9nf "${CMAKE_CURRENT_BINARY_DIR}/man/jsom.3"
            COMMENT "Building jsom.1.gz and jsom.3.gz from man/*.scd"
            VERBATIM
        )
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/man/jsom.1.gz"
            DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
            COMPONENT ${JSOM_COMPONENT_CLI}
        )
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/man/jsom.3.gz"
            DESTINATION ${CMAKE_INSTALL_MANDIR}/man3
            COMPONENT ${JSOM_COMPONENT_DEV}
        )
    else()
        message(STATUS "scdoc not found: jsom.1 and jsom.3 will not be built or installed")
    endif()
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

# ================================
# Packaging (opt-in)
# ================================
# OFF for ordinary builds, so nothing about the dev, test or fuzz configurations
# changes. A package build is:
#
#   cmake -S . -B build-pkg -DJSOM_ENABLE_PACKAGING=ON -DBUILD_SHARED_LIBS=ON \
#         -DJSOM_BUILD_TESTS=OFF -DJSOM_BUILD_BENCHMARKS=OFF
#   cmake --build build-pkg
#   (cd build-pkg && cpack -G DEB)        # or -G RPM
#
# BUILD_SHARED_LIBS=ON is not optional: cmake/CPack.cmake refuses a static
# packaging build by design, because an empty runtime package is worse than a
# failed build. scripts/package.sh runs this whole sequence and then consumes the
# packages it produced, which is what proves the component split.
option(JSOM_ENABLE_PACKAGING "Generate distribution packages (cpack)" OFF)
if(JSOM_ENABLE_PACKAGING)
    include(cmake/CPack.cmake)
endif()
