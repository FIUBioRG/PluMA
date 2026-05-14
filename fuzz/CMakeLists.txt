cmake_minimum_required(VERSION 3.14)
project(pluma_parallel_fuzz LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SANITIZER_FLAGS "-fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SANITIZER_FLAGS}")

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)

add_library(parallel_core_fuzz STATIC
    ${SRC_DIR}/ConfigParser.cxx
    ${SRC_DIR}/ResourceBudget.cxx
    ${SRC_DIR}/ParallelScheduler.cxx
)
target_include_directories(parallel_core_fuzz PUBLIC ${SRC_DIR})

function(add_fuzz_target name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE parallel_core_fuzz)
endfunction()

add_fuzz_target(fuzz_parse_size       fuzz_parse_size.cxx)
add_fuzz_target(fuzz_parse_config     fuzz_parse_config.cxx)
add_fuzz_target(fuzz_parse_plugin_task fuzz_parse_plugin_task.cxx)
add_fuzz_target(fuzz_resource_budget  fuzz_resource_budget.cxx)
