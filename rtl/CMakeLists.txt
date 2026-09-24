if(POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()
find_package(verilator REQUIRED HINTS $ENV{VERILATOR_ROOT} ${VERILATOR_ROOT})
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/filelists/common.f" TINY5_RTL_SOURCES)
list(TRANSFORM TINY5_RTL_SOURCES PREPEND "${PROJECT_SOURCE_DIR}/")
set(TINY5_LINT_ARGS -Wall -Wno-UNUSEDSIGNAL -Wno-UNUSEDPARAM --assert)
set_source_files_properties(sim/core_tests.cpp sim/add_mul_waveform_test.cpp PROPERTIES
    COMPILE_OPTIONS "-Wall;-Wextra;-Wpedantic;-Werror")

set(TARGET rtl_single_inorder_tests)
set(MODEL Vcore_single_inorder)
add_executable(${TARGET} sim/core_tests.cpp)
target_include_directories(${TARGET} PRIVATE sim)
target_link_libraries(${TARGET} PRIVATE
    rv32im_cpu rv32im_reference_cpu instruction_memory rv32im_test_support SystemC::systemc)
target_compile_definitions(${TARGET} PRIVATE
    RTL_MODEL_HEADER="${MODEL}.h" RTL_MODEL_CLASS=${MODEL} RTL_MODE=0)
verilate(${TARGET} SYSTEMC PREFIX ${MODEL} TOP_MODULE core_test_wrapper
    SOURCES ${TINY5_RTL_SOURCES} tops/tiny5_single_inorder.sv sim/core_test_wrapper.sv
    VERILATOR_ARGS ${TINY5_LINT_ARGS})
target_include_directories(${TARGET} SYSTEM PRIVATE
    "${VERILATOR_ROOT}/include" "${VERILATOR_ROOT}/include/vltstd")

set(WAVEFORM_TARGET rtl_add_mul_waveform)
set(WAVEFORM_MODEL Vcore_single_inorder_waveform)
add_executable(${WAVEFORM_TARGET} sim/add_mul_waveform_test.cpp)
target_include_directories(${WAVEFORM_TARGET} PRIVATE sim)
target_link_libraries(${WAVEFORM_TARGET} PRIVATE instruction_memory rv32im_test_support SystemC::systemc)
target_compile_definitions(${WAVEFORM_TARGET} PRIVATE
    RTL_MODEL_HEADER="${WAVEFORM_MODEL}.h" RTL_MODEL_CLASS=${WAVEFORM_MODEL})
verilate(${WAVEFORM_TARGET} SYSTEMC TRACE_FST TRACE_STRUCTS
    PREFIX ${WAVEFORM_MODEL} TOP_MODULE core_test_wrapper
    SOURCES ${TINY5_RTL_SOURCES} tops/tiny5_single_inorder.sv sim/core_test_wrapper.sv
    VERILATOR_ARGS ${TINY5_LINT_ARGS})
target_include_directories(${WAVEFORM_TARGET} SYSTEM PRIVATE
    "${VERILATOR_ROOT}/include" "${VERILATOR_ROOT}/include/vltstd")

if(BUILD_TESTING)
    add_test(NAME rtl_single_inorder COMMAND ${TARGET})
    set_tests_properties(rtl_single_inorder PROPERTIES TIMEOUT 120)
    add_test(NAME rtl_add_mul_waveform COMMAND ${WAVEFORM_TARGET}
        "${CMAKE_CURRENT_BINARY_DIR}/add_mul.fst")
    set_tests_properties(rtl_add_mul_waveform PROPERTIES TIMEOUT 30)
endif()
