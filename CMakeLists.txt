cmake_minimum_required(VERSION 3.16)
project(matching_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# spdlog (header-only, async-capable) for structured, non-blocking logging.
include(FetchContent)
FetchContent_Declare(
    spdlog
    URL https://github.com/gabime/spdlog/archive/refs/tags/v1.14.1.tar.gz
)
FetchContent_MakeAvailable(spdlog)

add_library(engine_core
    src/order_book.cpp
    src/engine.cpp
)
target_include_directories(engine_core PUBLIC include)

find_package(Threads REQUIRED)

add_executable(matching-engine
    src/main.cpp
    src/server.cpp
)
target_link_libraries(matching-engine PRIVATE engine_core Threads::Threads spdlog::spdlog)

if(WIN32)
    target_link_libraries(matching-engine PRIVATE ws2_32)
endif()

enable_testing()

add_executable(unit_test_orderbook tests/unit_test_orderbook.cpp)
target_link_libraries(unit_test_orderbook PRIVATE engine_core)
add_test(NAME unit_test_orderbook COMMAND unit_test_orderbook)

add_executable(benchmark_matching tests/benchmark_matching.cpp)
target_link_libraries(benchmark_matching PRIVATE engine_core)
