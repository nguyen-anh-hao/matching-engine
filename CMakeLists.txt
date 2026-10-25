cmake_minimum_required(VERSION 3.16)
project(matching_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(ENGINE_SANITIZERS "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(ENGINE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# spdlog (async, structured logging) is used by the HTTP adapter only.
include(FetchContent)
FetchContent_Declare(
    spdlog
    URL https://github.com/gabime/spdlog/archive/refs/tags/v1.14.1.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(spdlog)

find_package(Threads REQUIRED)

# Shared warning/sanitizer flags, applied only to this project's targets so
# third-party code fetched above is not affected.
add_library(engine_build_flags INTERFACE)
if(MSVC)
    target_compile_options(engine_build_flags INTERFACE /W4 /permissive-)
    if(ENGINE_WARNINGS_AS_ERRORS)
        target_compile_options(engine_build_flags INTERFACE /WX)
    endif()
else()
    target_compile_options(engine_build_flags INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor)
    if(ENGINE_WARNINGS_AS_ERRORS)
        target_compile_options(engine_build_flags INTERFACE -Werror)
    endif()
endif()

if(ENGINE_SANITIZERS)
    if(MSVC)
        message(FATAL_ERROR "ENGINE_SANITIZERS is only supported with GCC/Clang")
    endif()
    target_compile_options(engine_build_flags INTERFACE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(engine_build_flags INTERFACE -fsanitize=address,undefined)
endif()

add_library(engine_core
    src/engine.cpp
    src/engine_queries.cpp
    src/fixed_point.cpp
    src/order_book.cpp
    src/order_lifecycle.cpp
    src/order_validation.cpp
    src/trade_posting.cpp
)
target_include_directories(engine_core PUBLIC include)
target_link_libraries(engine_core PUBLIC engine_build_flags)

add_executable(matching-engine
    src/api.cpp
    src/api_orders.cpp
    src/api_queries.cpp
    src/api_serialization.cpp
    src/http.cpp
    src/json.cpp
    src/main.cpp
    src/metrics.cpp
    src/net.cpp
    src/server.cpp
)
target_link_libraries(matching-engine PRIVATE engine_core Threads::Threads spdlog::spdlog)
if(WIN32)
    target_link_libraries(matching-engine PRIVATE ws2_32)
endif()

enable_testing()

function(engine_add_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE engine_core)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

engine_add_test(unit_test_matching tests/unit_test_matching.cpp)
engine_add_test(unit_test_queries tests/unit_test_queries.cpp)
engine_add_test(unit_test_tif tests/unit_test_tif.cpp)
engine_add_test(unit_test_stp tests/unit_test_stp.cpp)
engine_add_test(unit_test_fixed_point tests/unit_test_fixed_point.cpp)
engine_add_test(unit_test_engine_internals tests/unit_test_engine_internals.cpp)
# The internals tests exercise the private helpers that live next to the engine
# sources, so they need the src directory on their include path.
target_include_directories(unit_test_engine_internals PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
engine_add_test(unit_test_json tests/unit_test_json.cpp src/json.cpp)
engine_add_test(unit_test_http tests/unit_test_http.cpp src/http.cpp src/net.cpp)
if(WIN32)
    target_link_libraries(unit_test_http PRIVATE ws2_32)
endif()

add_executable(benchmark_matching tests/benchmark_matching.cpp)
target_link_libraries(benchmark_matching PRIVATE engine_core)
