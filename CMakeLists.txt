cmake_minimum_required(VERSION 3.16)
project(opt1_pulsematch CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_compile_options(-O3 -march=native -Wall -Wextra)

add_executable(fast_matching_engine 
    src/engine_main.cpp 
    src/order_pool.cpp 
    src/fast_order_book.cpp
)
target_link_libraries(fast_matching_engine pthread)