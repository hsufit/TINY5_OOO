if(POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()
find_package(verilator REQUIRED HINTS $ENV{VERILATOR_ROOT} ${VERILATOR_ROOT})
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/filelists/common.f" TINY5_RTL_SOURCES)
list(TRANSFORM TINY5_RTL_SOURCES PREPEND "${PROJECT_SOURCE_DIR}/")
set(TINY5_TOP_SOURCES tops/tiny5_single_inorder.sv tops/tiny5_dual_inorder.sv tops/tiny5_dual_ooo.sv)
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
    SOURCES ${TINY5_RTL_SOURCES} ${TINY5_TOP_SOURCES} sim/core_test_wrapper.sv
    VERILATOR_ARGS ${TINY5_LINT_ARGS})
target_include_directories(${TARGET} SYSTEM PRIVATE
    "${VERILATOR_ROOT}/include" "${VERILATOR_ROOT}/include/vltstd")

set(DUAL_TARGET rtl_dual_inorder_tests)
set(DUAL_MODEL Vcore_dual_inorder)
add_executable(${DUAL_TARGET} sim/core_tests.cpp)
target_include_directories(${DUAL_TARGET} PRIVATE sim)
target_link_libraries(${DUAL_TARGET} PRIVATE
    rv32im_cpu rv32im_reference_cpu instruction_memory rv32im_test_support SystemC::systemc)
target_compile_definitions(${DUAL_TARGET} PRIVATE
    RTL_MODEL_HEADER="${DUAL_MODEL}.h" RTL_MODEL_CLASS=${DUAL_MODEL} RTL_MODE=1)
verilate(${DUAL_TARGET} SYSTEMC PREFIX ${DUAL_MODEL} TOP_MODULE core_test_wrapper
    SOURCES ${TINY5_RTL_SOURCES} ${TINY5_TOP_SOURCES} sim/core_test_wrapper.sv
    VERILATOR_ARGS ${TINY5_LINT_ARGS} -GDUAL_INORDER=1)
target_include_directories(${DUAL_TARGET} SYSTEM PRIVATE
    "${VERILATOR_ROOT}/include" "${VERILATOR_ROOT}/include/vltstd")

set(OOO_TARGET rtl_dual_ooo_tests)
set(OOO_MODEL Vcore_dual_ooo)
add_executable(${OOO_TARGET} sim/core_tests.cpp)
target_include_directories(${OOO_TARGET} PRIVATE sim)
target_link_libraries(${OOO_TARGET} PRIVATE
    rv32im_cpu rv32im_reference_cpu instruction_memory rv32im_test_support SystemC::systemc)
target_compile_definitions(${OOO_TARGET} PRIVATE
    RTL_MODEL_HEADER="${OOO_MODEL}.h" RTL_MODEL_CLASS=${OOO_MODEL} RTL_MODE=2)
verilate(${OOO_TARGET} SYSTEMC PREFIX ${OOO_MODEL} TOP_MODULE core_test_wrapper
    SOURCES ${TINY5_RTL_SOURCES} ${TINY5_TOP_SOURCES} sim/core_test_wrapper.sv
    VERILATOR_ARGS ${TINY5_LINT_ARGS} -GDUAL_OOO=1)
target_include_directories(${OOO_TARGET} SYSTEM PRIVATE
    "${VERILATOR_ROOT}/include" "${VERILATOR_ROOT}/include/vltstd")

set(WAVEFORM_TARGET rtl_add_mul_waveform)
set(WAVEFORM_MODEL Vcore_single_inorder_waveform)
add_executable(${WAVEFORM_TARGET} sim/add_mul_waveform_test.cpp)
target_include_directories(${WAVEFORM_TARGET} PRIVATE sim)
target_link_libraries(${WAVEFORM_TARGET} PRIVATE instruction_memory rv32im_test_support SystemC::systemc)
target_compile_definitions(${WAVEFORM_TARGET} PRIVATE
    RTL_MODEL_HEADER="${WAVEFORM_MODEL}.h" RTL_MODEL_CLASS=${WAVEFORM_MODEL} RTL_MODE=0)
verilate(${WAVEFORM_TARGET} SYSTEMC TRACE_FST TRACE_STRUCTS
    PREFIX ${WAVEFORM_MODEL} TOP_MODULE core_test_wrapper
    SOURCES ${TINY5_RTL_SOURCES} ${TINY5_TOP_SOURCES} sim/core_test_wrapper.sv
    VERILATOR_ARGS ${TINY5_LINT_ARGS})
target_include_directories(${WAVEFORM_TARGET} SYSTEM PRIVATE
    "${VERILATOR_ROOT}/include" "${VERILATOR_ROOT}/include/vltstd")

set(DUAL_WAVEFORM_TARGET rtl_dual_add_mul_waveform)
set(DUAL_WAVEFORM_MODEL Vcore_dual_inorder_waveform)
add_executable(${DUAL_WAVEFORM_TARGET} sim/add_mul_waveform_test.cpp)
target_include_directories(${DUAL_WAVEFORM_TARGET} PRIVATE sim)
target_link_libraries(${DUAL_WAVEFORM_TARGET} PRIVATE instruction_memory rv32im_test_support SystemC::systemc)
target_compile_definitions(${DUAL_WAVEFORM_TARGET} PRIVATE
    RTL_MODEL_HEADER="${DUAL_WAVEFORM_MODEL}.h" RTL_MODEL_CLASS=${DUAL_WAVEFORM_MODEL} RTL_MODE=1)
verilate(${DUAL_WAVEFORM_TARGET} SYSTEMC TRACE_FST TRACE_STRUCTS
    PREFIX ${DUAL_WAVEFORM_MODEL} TOP_MODULE core_test_wrapper
    SOURCES ${TINY5_RTL_SOURCES} ${TINY5_TOP_SOURCES} sim/core_test_wrapper.sv
    VERILATOR_ARGS ${TINY5_LINT_ARGS} -GDUAL_INORDER=1)
target_include_directories(${DUAL_WAVEFORM_TARGET} SYSTEM PRIVATE
    "${VERILATOR_ROOT}/include" "${VERILATOR_ROOT}/include/vltstd")

set(OOO_WAVEFORM_TARGET rtl_dual_ooo_add_mul_waveform)
set(OOO_WAVEFORM_MODEL Vcore_dual_ooo_waveform)
add_executable(${OOO_WAVEFORM_TARGET} sim/add_mul_waveform_test.cpp)
target_include_directories(${OOO_WAVEFORM_TARGET} PRIVATE sim)
target_link_libraries(${OOO_WAVEFORM_TARGET} PRIVATE instruction_memory rv32im_test_support SystemC::systemc)
target_compile_definitions(${OOO_WAVEFORM_TARGET} PRIVATE
    RTL_MODEL_HEADER="${OOO_WAVEFORM_MODEL}.h" RTL_MODEL_CLASS=${OOO_WAVEFORM_MODEL} RTL_MODE=2)
verilate(${OOO_WAVEFORM_TARGET} SYSTEMC TRACE_FST TRACE_STRUCTS
    PREFIX ${OOO_WAVEFORM_MODEL} TOP_MODULE core_test_wrapper
    SOURCES ${TINY5_RTL_SOURCES} ${TINY5_TOP_SOURCES} sim/core_test_wrapper.sv
    VERILATOR_ARGS ${TINY5_LINT_ARGS} -GDUAL_OOO=1)
target_include_directories(${OOO_WAVEFORM_TARGET} SYSTEM PRIVATE
    "${VERILATOR_ROOT}/include" "${VERILATOR_ROOT}/include/vltstd")

if(BUILD_TESTING)
    add_test(NAME rtl_single_inorder COMMAND ${TARGET})
    set_tests_properties(rtl_single_inorder PROPERTIES TIMEOUT 120)
    add_test(NAME rtl_dual_inorder COMMAND ${DUAL_TARGET})
    set_tests_properties(rtl_dual_inorder PROPERTIES TIMEOUT 120)
    add_test(NAME rtl_dual_ooo COMMAND ${OOO_TARGET})
    set_tests_properties(rtl_dual_ooo PROPERTIES TIMEOUT 120)
    add_test(NAME rtl_add_mul_waveform COMMAND ${WAVEFORM_TARGET}
        "${CMAKE_CURRENT_BINARY_DIR}/add_mul.fst")
    set_tests_properties(rtl_add_mul_waveform PROPERTIES TIMEOUT 30)
    add_test(NAME rtl_dual_add_mul_waveform COMMAND ${DUAL_WAVEFORM_TARGET}
        "${CMAKE_CURRENT_BINARY_DIR}/add_mul_dual.fst")
    set_tests_properties(rtl_dual_add_mul_waveform PROPERTIES TIMEOUT 30)
    add_test(NAME rtl_dual_ooo_add_mul_waveform COMMAND ${OOO_WAVEFORM_TARGET}
        "${CMAKE_CURRENT_BINARY_DIR}/add_mul_dual_ooo.fst")
    set_tests_properties(rtl_dual_ooo_add_mul_waveform PROPERTIES TIMEOUT 30)
    add_test(NAME rtl_queued_alu_waveform COMMAND ${WAVEFORM_TARGET}
        "${CMAKE_CURRENT_BINARY_DIR}/queued_alu.fst" queued_alu)
    set_tests_properties(rtl_queued_alu_waveform PROPERTIES TIMEOUT 30)
    add_test(NAME rtl_dual_queued_alu_waveform COMMAND ${DUAL_WAVEFORM_TARGET}
        "${CMAKE_CURRENT_BINARY_DIR}/queued_alu_dual.fst" queued_alu)
    set_tests_properties(rtl_dual_queued_alu_waveform PROPERTIES TIMEOUT 30)
    add_test(NAME rtl_dual_ooo_queued_alu_waveform COMMAND ${OOO_WAVEFORM_TARGET}
        "${CMAKE_CURRENT_BINARY_DIR}/queued_alu_dual_ooo.fst" queued_alu)
    set_tests_properties(rtl_dual_ooo_queued_alu_waveform PROPERTIES TIMEOUT 30)
    add_test(NAME rtl_queued_alu_chain_waveform COMMAND ${WAVEFORM_TARGET}
        "${CMAKE_CURRENT_BINARY_DIR}/queued_alu_chain.fst" queued_alu_chain)
    set_tests_properties(rtl_queued_alu_chain_waveform PROPERTIES TIMEOUT 30)
    add_test(NAME rtl_dual_queued_alu_chain_waveform COMMAND ${DUAL_WAVEFORM_TARGET}
        "${CMAKE_CURRENT_BINARY_DIR}/queued_alu_chain_dual.fst" queued_alu_chain)
    set_tests_properties(rtl_dual_queued_alu_chain_waveform PROPERTIES TIMEOUT 30)
    add_test(NAME rtl_dual_ooo_queued_alu_chain_waveform COMMAND ${OOO_WAVEFORM_TARGET}
        "${CMAKE_CURRENT_BINARY_DIR}/queued_alu_chain_dual_ooo.fst" queued_alu_chain)
    set_tests_properties(rtl_dual_ooo_queued_alu_chain_waveform PROPERTIES TIMEOUT 30)
    add_test(NAME rtl_queued_alu_hazards_waveform COMMAND ${WAVEFORM_TARGET}
        "${CMAKE_CURRENT_BINARY_DIR}/queued_alu_hazards.fst" queued_alu_hazards)
    set_tests_properties(rtl_queued_alu_hazards_waveform PROPERTIES TIMEOUT 30)
    add_test(NAME rtl_dual_queued_alu_hazards_waveform COMMAND ${DUAL_WAVEFORM_TARGET}
        "${CMAKE_CURRENT_BINARY_DIR}/queued_alu_hazards_dual.fst" queued_alu_hazards)
    set_tests_properties(rtl_dual_queued_alu_hazards_waveform PROPERTIES TIMEOUT 30)
    add_test(NAME rtl_dual_ooo_queued_alu_hazards_waveform COMMAND ${OOO_WAVEFORM_TARGET}
        "${CMAKE_CURRENT_BINARY_DIR}/queued_alu_hazards_dual_ooo.fst" queued_alu_hazards)
    set_tests_properties(rtl_dual_ooo_queued_alu_hazards_waveform PROPERTIES TIMEOUT 30)
endif()
