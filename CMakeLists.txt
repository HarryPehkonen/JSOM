cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(JSOM VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable compile commands database for clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Library options
option(JSOM_HEADER_ONLY "Build as header-only library" ON)
option(JSOM_BUILD_TESTS "Build tests" ON)
option(JSOM_BUILD_EXAMPLES "Build examples" ON)

# Find or fetch Google Test for testing
if(JSOM_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
    )
    FetchContent_MakeAvailable(googletest)
endif()

# Create library target based on header-only option
if(JSOM_HEADER_ONLY)
    # Header-only interface library
    add_library(jsom INTERFACE)
    target_include_directories(jsom INTERFACE 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    
    # Compiler requirements for header-only
    target_compile_features(jsom INTERFACE cxx_std_17)
    
    message(STATUS "JSOM: Building as header-only library")
else()
    # Traditional compiled library
    add_library(jsom STATIC
        src/path_node.cpp
        src/json_value.cpp
        src/streaming_parser.cpp
        src/allocators.cpp
    )
    
    # Configure library target
    target_include_directories(jsom
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        PUBLIC $<INSTALL_INTERFACE:include>
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    
    # Compiler flags
    if(MSVC)
        target_compile_options(jsom PRIVATE /W4)
    else()
        target_compile_options(jsom PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    
    message(STATUS "JSOM: Building as compiled library")
endif()

# Alias for consistent naming
add_library(jsom::jsom ALIAS jsom)

# Add custom targets for formatting and linting (not part of build)
add_custom_target(format
    COMMAND clang-format -i ${CMAKE_SOURCE_DIR}/include/jsom.hpp ${CMAKE_SOURCE_DIR}/include/jsom/*.hpp
    COMMENT "Formatting source code with clang-format"
)

add_custom_target(tidy
    COMMAND clang-tidy ${CMAKE_SOURCE_DIR}/include/jsom.hpp -- -I${CMAKE_SOURCE_DIR}/include
    COMMENT "Running clang-tidy static analysis on header-only library"
)

# Sync check target - verify header-only and compiled versions stay aligned
add_custom_target(sync-check
    COMMAND ${CMAKE_COMMAND} -E echo "Testing header-only version..."
    COMMAND ${CMAKE_COMMAND} -B ${CMAKE_BINARY_DIR}/sync-test-header -DJSOM_HEADER_ONLY=ON -DJSOM_BUILD_EXAMPLES=OFF -S ${CMAKE_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}/sync-test-header --target jsom_tests
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}/sync-test-header ctest --output-on-failure
    COMMAND ${CMAKE_COMMAND} -E echo "Testing compiled version..."
    COMMAND ${CMAKE_COMMAND} -B ${CMAKE_BINARY_DIR}/sync-test-compiled -DJSOM_HEADER_ONLY=OFF -DJSOM_BUILD_EXAMPLES=OFF -S ${CMAKE_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}/sync-test-compiled --target jsom_tests
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}/sync-test-compiled ctest --output-on-failure
    COMMAND ${CMAKE_COMMAND} -E echo "  Both versions pass - implementations are in sync!"
    COMMENT "Verifying header-only and compiled versions produce identical test results"
)

# Testing
if(JSOM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Examples
if(JSOM_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Installation
include(GNUInstallDirs)

# Install headers
install(FILES include/jsom.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install traditional headers (for non-header-only builds)
if(NOT JSOM_HEADER_ONLY)
    install(DIRECTORY include/jsom/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/jsom
        FILES_MATCHING PATTERN "*.hpp"
    )
endif()

# Install library target
install(TARGETS jsom
    EXPORT jsomTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export targets
install(EXPORT jsomTargets
    FILE jsomTargets.cmake
    NAMESPACE jsom::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/jsom
)

# Create config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    jsomConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/jsomConfig.cmake.in
    jsomConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/jsom
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/jsomConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/jsomConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/jsom
)
