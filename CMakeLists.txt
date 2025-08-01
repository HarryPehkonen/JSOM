cmake_minimum_required(VERSION 3.10)
project(JSOM VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# Prevent GoogleTest from being installed
set(INSTALL_GTEST OFF CACHE BOOL "Enable installation of googletest" FORCE)
set(INSTALL_GMOCK OFF CACHE BOOL "Enable installation of googlemock" FORCE)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)

FetchContent_MakeAvailable(googletest)

add_library(jsom INTERFACE)
target_include_directories(jsom INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(jsom INTERFACE cxx_std_17)

# Optional JSOM CLI executable (enabled with -DCLI=ON)
option(CLI "Build JSOM CLI tool" OFF)

if(CLI)
    add_executable(jsom_cli src/main.cpp)
    target_link_libraries(jsom_cli jsom)
    target_include_directories(jsom_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    set_target_properties(jsom_cli PROPERTIES OUTPUT_NAME jsom)
    
    message(STATUS "JSOM CLI tool enabled")
endif()

# Optional benchmarks (enabled with -DBENCHMARKS=ON)
option(BENCHMARKS "Build performance benchmarks" OFF)

if(BENCHMARKS)
    # Disable benchmark install to keep it out of our install targets
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "Enable installation of benchmark" FORCE)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Enable testing of the benchmark library" FORCE)
    
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3
    )
    
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    
    FetchContent_MakeAvailable(benchmark nlohmann_json)
    
    # Create benchmark executable
    add_executable(jsom_benchmarks
        benchmarks/benchmark_main.cpp
        benchmarks/benchmark_parsing.cpp
        benchmarks/benchmark_dom_access.cpp
        benchmarks/benchmark_construction.cpp
        benchmarks/benchmark_serialization.cpp
    )
    
    target_link_libraries(jsom_benchmarks
        jsom
        benchmark::benchmark
        benchmark::benchmark_main
        nlohmann_json::nlohmann_json
    )
    
    target_include_directories(jsom_benchmarks PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks
    )
    
    # Add custom target to run benchmarks
    add_custom_target(run_benchmarks
        COMMAND jsom_benchmarks
        DEPENDS jsom_benchmarks
        COMMENT "Running performance benchmarks"
    )
    
    message(STATUS "JSOM benchmarks enabled")
endif()

enable_testing()

add_executable(jsom_tests
    tests/test_main.cpp
    tests/test_smoke.cpp
    tests/test_utf8.cpp
    tests/test_parsing.cpp
    tests/test_strings.cpp
    tests/test_containers.cpp
    tests/test_phase5.cpp
    tests/test_path_tracking.cpp
    tests/test_phase7.cpp
    tests/test_batch_parser.cpp
    tests/test_intelligent_serialization.cpp
)

target_link_libraries(jsom_tests
    jsom
    gtest
    gtest_main
    gmock
    gmock_main
)

target_include_directories(jsom_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

include(GoogleTest)
gtest_discover_tests(jsom_tests)

add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS jsom_tests
)

# Find source files for formatting and linting
file(GLOB_RECURSE JSOM_SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/include/jsom.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
)

# Format target using clang-format
find_program(CLANG_FORMAT NAMES clang-format)
if(CLANG_FORMAT)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i ${JSOM_SOURCES}
        COMMENT "Formatting source files with clang-format"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
else()
    add_custom_target(format
        COMMAND ${CMAKE_COMMAND} -E echo "clang-format not found"
    )
endif()

# Tidy target using clang-tidy
find_program(CLANG_TIDY NAMES clang-tidy)
if(CLANG_TIDY)
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY} ${JSOM_SOURCES} -- -x c++ -I${CMAKE_CURRENT_SOURCE_DIR}/include -std=c++17
        COMMENT "Running clang-tidy on source files"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
else()
    add_custom_target(tidy
        COMMAND ${CMAKE_COMMAND} -E echo "clang-tidy not found"
    )
endif()

# Installation configuration
include(GNUInstallDirs)

# Install the header file
install(FILES include/jsom.hpp 
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/jsom
)

# Install the CLI tool if built
if(CLI)
    install(TARGETS jsom_cli
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# Configure package config files for find_package
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/jsomConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/jsomConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/jsom
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/jsomConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# Install the CMake config files
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/jsomConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/jsomConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/jsom
)

# Install the interface library target
install(TARGETS jsom
    EXPORT jsomTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT jsomTargets
    FILE jsomTargets.cmake
    NAMESPACE jsom::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/jsom
)