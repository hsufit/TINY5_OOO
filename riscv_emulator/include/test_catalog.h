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

const std::vector<ProgramTest>& rv32im_test_catalog();

#endif
