cmake_minimum_required(VERSION 3.16)
project(phase3_pulsematch CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_compile_options(-O3 -march=native -Wall -Wextra)

add_executable(phase3_server 
    src/server_main.cpp 
    src/order_pool.cpp 
    src/fast_order_book.cpp
    src/async_wal.cpp
    src/network_receiver.cpp
)
target_link_libraries(phase3_server pthread uring)

add_executable(udp_client tests/udp_benchmark_client.cpp)
target_link_libraries(udp_client pthread)