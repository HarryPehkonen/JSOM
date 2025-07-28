# Basic usage example
add_executable(basic_example basic_example.cpp)
target_link_libraries(basic_example PRIVATE jsom::jsom)

# Performance benchmark example  
add_executable(benchmark_example benchmark_example.cpp)
target_link_libraries(benchmark_example PRIVATE jsom::jsom)