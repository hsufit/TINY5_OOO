#include "test_catalog.h"

#include <utility>

namespace {

std::vector<std::uint32_t> sequential_pcs(std::size_t count) {
    std::vector<std::uint32_t> pcs;
    pcs.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        pcs.push_back(static_cast<std::uint32_t>(index * 4U));
    }
    return pcs;
}

ProgramTest success(std::string name, std::vector<std::uint8_t> program,
                    std::vector<RegisterExpectation> registers) {
    const std::size_t count = program.size() / 4U;
    return ProgramTest{std::move(name),
                       std::move(program),
                       std::move(registers),
                       count,
                       sequential_pcs(count),
                       ExpectedTermination::Halt,
                       CpuFaultCode::NONE};
}

ProgramTest fault(std::string name, std::vector<std::uint8_t> program,
                  CpuFaultCode code) {
    return ProgramTest{std::move(name),
                       std::move(program),
                       {},
                       0U,
                       {},
                       ExpectedTermination::Fault,
                       code};
}

}  // namespace

const ProgramTest& rv32im_add_mul_test() {
    static const ProgramTest test = success(
        "ADD/MUL sequence",
        {
            0x93, 0x00, 0x60, 0x00,  // addi x1,x0,6
            0x13, 0x01, 0x70, 0x00,  // addi x2,x0,7
            0xb3, 0x81, 0x20, 0x00,  // add  x3,x1,x2
            0x33, 0x82, 0x20, 0x02,  // mul  x4,x1,x2
            0xb3, 0x82, 0x41, 0x00,  // add  x5,x3,x4
        },
        {{1, 6U}, {2, 7U}, {3, 13U}, {4, 42U}, {5, 55U}});
    return test;
}

const std::vector<ProgramTest>& rv32im_test_catalog() {
    static const std::vector<ProgramTest> tests{
        rv32im_add_mul_test(),
        success(
            "RV32I register arithmetic",
            {
                0x93, 0x00, 0xa0, 0x00,  // addi x1,  x0,  10
                0x13, 0x01, 0xd0, 0xff,  // addi x2,  x0,  -3
                0x93, 0x81, 0x50, 0x00,  // addi x3,  x1,  5
                0x33, 0x82, 0x20, 0x00,  // add  x4,  x1,  x2
                0xb3, 0x82, 0x20, 0x40,  // sub  x5,  x1,  x2
                0x33, 0x23, 0x11, 0x00,  // slt  x6,  x2,  x1
                0xb3, 0x33, 0x11, 0x00,  // sltu x7,  x2,  x1
                0x33, 0xc4, 0x20, 0x00,  // xor  x8,  x1,  x2
                0xb3, 0xe4, 0x20, 0x00,  // or   x9,  x1,  x2
                0x33, 0xf5, 0x20, 0x00,  // and  x10, x1,  x2
                0x93, 0x05, 0x20, 0x00,  // addi x11, x0,  2
                0x33, 0x96, 0xb0, 0x00,  // sll  x12, x1,  x11
                0xb3, 0x56, 0xb1, 0x00,  // srl  x13, x2,  x11
                0x33, 0x57, 0xb1, 0x40,  // sra  x14, x2,  x11
            },
            {{3, 15U},
             {4, 7U},
             {5, 13U},
             {6, 1U},
             {7, 0U},
             {8, 0xfffffff7U},
             {9, 0xffffffffU},
             {10, 8U},
             {12, 40U},
             {13, 0x3fffffffU},
             {14, 0xffffffffU}}),
        success(
            "RV32I immediate arithmetic",
            {
                0x93, 0x00, 0x00, 0xff,  // addi  x1, x0, -16
                0x13, 0xa1, 0xf0, 0xff,  // slti  x2, x1, -1
                0x93, 0xb1, 0xf0, 0xff,  // sltiu x3, x1, -1
                0x13, 0xc2, 0x50, 0x05,  // xori  x4, x1, 0x55
                0x93, 0x62, 0x30, 0x12,  // ori   x5, x0, 0x123
                0x13, 0xf3, 0xf0, 0x00,  // andi  x6, x1, 0x0f
                0x93, 0x93, 0x42, 0x00,  // slli  x7, x5, 4
                0x13, 0xd4, 0x40, 0x00,  // srli  x8, x1, 4
                0x93, 0xd4, 0x40, 0x40,  // srai  x9, x1, 4
            },
            {{1, 0xfffffff0U},
             {2, 1U},
             {3, 1U},
             {4, 0xffffffa5U},
             {5, 0x123U},
             {6, 0U},
             {7, 0x1230U},
             {8, 0x0fffffffU},
             {9, 0xffffffffU}}),
        success(
            "RV32M multiply and divide",
            {
                0x93, 0x00, 0xc0, 0xfe,  // addi   x1,  x0, -20
                0x13, 0x01, 0x60, 0x00,  // addi   x2,  x0, 6
                0xb3, 0x81, 0x20, 0x02,  // mul    x3,  x1, x2
                0x33, 0x92, 0x20, 0x02,  // mulh   x4,  x1, x2
                0xb3, 0xa2, 0x20, 0x02,  // mulhsu x5,  x1, x2
                0x33, 0xb3, 0x20, 0x02,  // mulhu  x6,  x1, x2
                0xb3, 0xc3, 0x20, 0x02,  // div    x7,  x1, x2
                0x33, 0xd4, 0x20, 0x02,  // divu   x8,  x1, x2
                0xb3, 0xe4, 0x20, 0x02,  // rem    x9,  x1, x2
                0x33, 0xf5, 0x20, 0x02,  // remu   x10, x1, x2
            },
            {{3, 0xffffff88U},
             {4, 0xffffffffU},
             {5, 0xffffffffU},
             {6, 5U},
             {7, 0xfffffffdU},
             {8, 0x2aaaaaa7U},
             {9, 0xfffffffeU},
             {10, 2U}}),
        success(
            "RV32M division corner cases",
            {
                0x93, 0x00, 0x90, 0xff,  // addi x1,  x0, -7
                0x13, 0x01, 0x00, 0x00,  // addi x2,  x0, 0
                0xb3, 0xc1, 0x20, 0x02,  // div  x3,  x1, x2
                0x33, 0xd2, 0x20, 0x02,  // divu x4,  x1, x2
                0xb3, 0xe2, 0x20, 0x02,  // rem  x5,  x1, x2
                0x33, 0xf3, 0x20, 0x02,  // remu x6,  x1, x2
                0x93, 0x03, 0x10, 0x00,  // addi x7,  x0, 1
                0x93, 0x93, 0xf3, 0x01,  // slli x7,  x7, 31
                0x13, 0x04, 0xf0, 0xff,  // addi x8,  x0, -1
                0xb3, 0xc4, 0x83, 0x02,  // div  x9,  x7, x8
            },
            {{3, 0xffffffffU},
             {4, 0xffffffffU},
             {5, 0xfffffff9U},
             {6, 0xfffffff9U},
             {7, 0x80000000U},
             {9, 0x80000000U}}),
        success("hard-wired x0",
                {
                    0x13, 0x00, 0xb0, 0x07,  // addi x0, x0, 123
                    0x93, 0x00, 0x70, 0x00,  // addi x1, x0, 7
                },
                {{0, 0U}, {1, 7U}}),
        fault("unsupported opcode", {0xff, 0xff, 0xff, 0xff},
              CpuFaultCode::ILLEGAL_INSTRUCTION),
        fault("truncated instruction", {0x93, 0x00, 0xa0},
              CpuFaultCode::INSTRUCTION_ACCESS_FAULT),
    };
    return tests;
}
