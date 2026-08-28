cmake_minimum_required(VERSION 3.16)
project(naive_pulsematch CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(naive_server 
    src/server_main.cpp 
    src/order_book.cpp 
    src/matching_engine.cpp
)
target_link_libraries(naive_server pthread)

add_executable(benchmark_client tests/benchmark_client.cpp)
target_link_libraries(benchmark_client pthread)