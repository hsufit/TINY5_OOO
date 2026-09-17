cmake_minimum_required(VERSION 3.16)
project(systemc_hello LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/SystemCDependency.cmake")

add_executable(systemc_hello main.cpp)
target_link_libraries(systemc_hello PRIVATE SystemC::systemc)
target_compile_options(systemc_hello PRIVATE -Wall -Wextra -Wpedantic -Werror)

include(CTest)
if(BUILD_TESTING)
    add_test(NAME systemc_hello COMMAND systemc_hello)
    set_tests_properties(systemc_hello PROPERTIES
        PASS_REGULAR_EXPRESSION "10 ns: Hello, SystemC!"
        TIMEOUT 10)
endif()
