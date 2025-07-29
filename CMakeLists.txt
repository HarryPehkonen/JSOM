cmake_minimum_required(VERSION 3.10)
project(JSOM VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

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