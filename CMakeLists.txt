cmake_minimum_required(VERSION 3.16)
project(beamlink LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(BEAMLINK_BUILD_TESTS "Build the unit tests" ON)
option(BEAMLINK_BUILD_BENCHMARKS "Build the Google Benchmark suite" ON)

add_library(beamlink_warnings INTERFACE)
if(MSVC)
  target_compile_options(beamlink_warnings INTERFACE /W4 /permissive-)
else()
  target_compile_options(beamlink_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

if(BEAMLINK_BUILD_TESTS)
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    GIT_SHALLOW TRUE)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
  enable_testing()
endif()

if(BEAMLINK_BUILD_BENCHMARKS)
  FetchContent_Declare(googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3
    GIT_SHALLOW TRUE)
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googlebenchmark)
endif()

add_subdirectory(src)

if(BEAMLINK_BUILD_TESTS)
  add_subdirectory(tests)
endif()

if(BEAMLINK_BUILD_BENCHMARKS)
  add_subdirectory(bench)
endif()
