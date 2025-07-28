# Test executable
add_executable(jsom_tests
    test_main.cpp
    test_path_node.cpp
    test_json_value.cpp
    test_string_escaping.cpp
    test_allocators.cpp
    test_streaming_parser.cpp
)

# Link test dependencies  
target_link_libraries(jsom_tests
    PRIVATE GTest::gtest_main
    PRIVATE jsom::jsom
)

# Auto-discover tests
include(GoogleTest)
gtest_discover_tests(jsom_tests)