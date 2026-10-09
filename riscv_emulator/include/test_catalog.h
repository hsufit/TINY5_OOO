#ifndef TEST_CATALOG_H
#define TEST_CATALOG_H

#include "cpu_protocol.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct RegisterExpectation {
    unsigned index;
    std::uint32_t value;
};

enum class ExpectedTermination { Halt, Fault };

struct ProgramTest {
    std::string name;
    std::vector<std::uint8_t> program;
    std::vector<RegisterExpectation> expected_registers;
    std::size_t expected_retirement_count;
    std::vector<std::uint32_t> expected_retirement_pcs;
    ExpectedTermination expected_termination;
    CpuFaultCode expected_fault;
};

std::vector<ProgramTest> branch_test_catalog();
const std::vector<ProgramTest>& rv32im_test_catalog();
const ProgramTest& rv32im_add_mul_test();
const ProgramTest& rv32im_queued_alu_behind_multiply_test();
const ProgramTest& rv32im_queued_alu_chain_behind_multiply_test();
const ProgramTest& rv32im_queued_alu_hazards_test();
const ProgramTest& rv32im_branch_slow_not_taken_speculation_test();
const ProgramTest& rv32im_branch_independent_taken_redirect_test();
const ProgramTest& rv32im_branch_taken_bypasses_dependency_test();
const ProgramTest& rv32im_branch_taken_discards_wrong_path_test();
const ProgramTest& rv32im_branch_backward_loop_test();
const ProgramTest& rv32im_branch_fast_not_taken_no_gain_test();

#endif
