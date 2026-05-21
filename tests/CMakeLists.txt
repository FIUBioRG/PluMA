cmake_minimum_required(VERSION 3.14)
project(pluma_parallel_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../vendor/Catch2 Catch2)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)

add_library(parallel_core STATIC
    ${SRC_DIR}/ConfigParser.cxx
    ${SRC_DIR}/ResourceBudget.cxx
    ${SRC_DIR}/ParallelScheduler.cxx
)
target_include_directories(parallel_core PUBLIC ${SRC_DIR})

add_executable(tests
    test_config_parser.cxx
    test_resource_budget.cxx
    test_parallel_scheduler.cxx
)
target_link_libraries(tests PRIVATE parallel_core Catch2::Catch2WithMain)
target_include_directories(tests PRIVATE ${SRC_DIR})

include(CTest)
include(${CMAKE_CURRENT_SOURCE_DIR}/../vendor/Catch2/extras/Catch.cmake)
catch_discover_tests(tests)
