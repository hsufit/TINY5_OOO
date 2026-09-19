cmake_minimum_required(VERSION 3.16)
project(riscv_emulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/SystemCDependency.cmake")

add_library(rv32im_cpu src/rv32im_cpu.cpp)
target_include_directories(rv32im_cpu PUBLIC include)
target_link_libraries(rv32im_cpu PUBLIC SystemC::systemc)
target_compile_options(rv32im_cpu PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_library(instruction_memory src/instruction_memory.cpp)
target_include_directories(instruction_memory PUBLIC include)
target_link_libraries(instruction_memory PUBLIC SystemC::systemc)
target_compile_options(instruction_memory PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_library(rv32im_test_support
    src/retirement_scoreboard.cpp
    src/test_catalog.cpp)
target_include_directories(rv32im_test_support PUBLIC include)
target_link_libraries(rv32im_test_support PUBLIC SystemC::systemc)
target_compile_options(rv32im_test_support PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(rv32im_tests tests/rv32im_tests.cpp)
target_link_libraries(rv32im_tests PRIVATE
    rv32im_cpu
    instruction_memory
    rv32im_test_support)
target_compile_options(rv32im_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(instruction_memory_tests tests/instruction_memory_tests.cpp)
target_link_libraries(instruction_memory_tests PRIVATE instruction_memory)
target_compile_options(instruction_memory_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)

include(CTest)
if(BUILD_TESTING)
    add_test(NAME rv32im_arithmetic COMMAND rv32im_tests)
    set_tests_properties(rv32im_arithmetic PROPERTIES
        PASS_REGULAR_EXPRESSION "All RV32IM arithmetic tests passed"
        TIMEOUT 10)
    add_test(NAME instruction_memory_protocol COMMAND instruction_memory_tests)
    set_tests_properties(instruction_memory_protocol PROPERTIES
        PASS_REGULAR_EXPRESSION "All instruction-memory protocol tests passed"
        TIMEOUT 10)
endif()
