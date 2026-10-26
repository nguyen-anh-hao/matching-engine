cmake_minimum_required(VERSION 3.16)
project(matching_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(ENGINE_SANITIZERS "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(ENGINE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(ENGINE_BUILD_TESTS "Build the unit test binaries" ON)
option(ENGINE_BUILD_BENCHMARKS "Build the matching benchmark" ON)

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

add_subdirectory(src)

if(ENGINE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(ENGINE_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
