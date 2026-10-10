#include "test_catalog.h"

#include <utility>

namespace {
enum Register : unsigned {
    x0 = 0, x1, x2, x3, x4, x5, x6, x7, x8, x9,
    x10, x11, x12, x13, x14,
    x20 = 20,
};

std::uint32_t addi(unsigned rd, unsigned rs, int immediate) {
    return ((static_cast<std::uint32_t>(immediate) & 4095U) << 20U) |
           (rs << 15U) | (rd << 7U) | 0x13U;
}

std::uint32_t reg(unsigned funct7, unsigned funct3, unsigned rd, unsigned a, unsigned b) {
    return (funct7 << 25U) | (b << 20U) | (a << 15U) |
           (funct3 << 12U) | (rd << 7U) | 0x33U;
}

std::uint32_t branch(unsigned condition, unsigned a, unsigned b, int offset) {
    const auto bits = static_cast<std::uint32_t>(offset);
    return ((bits & 0x1000U) << 19U) | ((bits & 0x7e0U) << 20U) | (b << 20U) |
           (a << 15U) | (condition << 12U) | ((bits & 0x1eU) << 7U) |
           ((bits & 0x800U) >> 4U) | 0x63U;
}

std::uint32_t add(unsigned rd, unsigned a, unsigned b) { return reg(0, 0, rd, a, b); }
std::uint32_t mul(unsigned rd, unsigned a, unsigned b) { return reg(1, 0, rd, a, b); }
std::uint32_t div(unsigned rd, unsigned a, unsigned b) { return reg(1, 4, rd, a, b); }

// Branch offsets are in bytes, relative to the branch instruction's PC.
std::uint32_t beq(unsigned a, unsigned b, int offset) { return branch(0, a, b, offset); }
std::uint32_t bne(unsigned a, unsigned b, int offset) { return branch(1, a, b, offset); }

ProgramTest program(std::string name, const std::vector<std::uint32_t>& words,
                    std::vector<std::uint32_t> pcs, std::vector<RegisterExpectation> registers = {},
                    CpuFaultCode fault = CpuFaultCode::NONE) {
    std::vector<std::uint8_t> bytes;
    for (auto word : words)
        for (unsigned shift = 0; shift < 32; shift += 8)
            bytes.push_back(static_cast<std::uint8_t>(word >> shift));
    const auto count = pcs.size();
    return {std::move(name), std::move(bytes), std::move(registers), count, std::move(pcs),
            fault == CpuFaultCode::NONE ? ExpectedTermination::Halt : ExpectedTermination::Fault, fault};
}
}  // namespace

const ProgramTest& rv32im_branch_slow_not_taken_speculation_test() {
    static const ProgramTest test = program(
        "branch slow not taken useful speculation",
        {
            addi(x10, x0, 80),   // PC 0:  dividend
            addi(x11, x0, 2),    // PC 4:  divisor
            div(x1, x10, x11),   // PC 8:  x1 = 40
            beq(x1, x0, 20),     // PC 12: done at PC 32; not taken
            addi(x2, x0, 7),     // PC 16: useful independent work
            addi(x3, x0, 9),     // PC 20: useful independent work
            mul(x4, x2, x3),     // PC 24: x4 = 63
            add(x5, x4, x2),     // PC 28: x5 = 70
            add(x6, x5, x0),     // PC 32 (done): x6 = 70
        },
        {0, 4, 8, 12, 16, 20, 24, 28, 32},
        {{x1, 40U}, {x2, 7U}, {x3, 9U}, {x4, 63U}, {x5, 70U}, {x6, 70U},
         {x10, 80U}, {x11, 2U}});
    return test;
}

const ProgramTest& rv32im_branch_not_taken_divide_alu_chains_test() {
    static const ProgramTest test = program(
        "branch not taken divide and ALU chains",
        {
            addi(x10, x0, 120),  // PC 0:  initial dividend
            addi(x11, x0, 3),    // PC 4:  initial divisor
            div(x1, x10, x11),   // PC 8:  x1 = 40
            div(x2, x1, x11),    // PC 12: waits for x1; x2 = 13
            add(x3, x2, x2),     // PC 16: waits for x2; x3 = 26
            beq(x1, x0, 48),    // PC 20: done at PC 68; not taken
            addi(x4, x0, 7),    // PC 24: independent younger chain
            addi(x5, x4, 1),    // PC 28: x5 = 8
            addi(x6, x5, 1),    // PC 32: x6 = 9
            addi(x7, x6, 1),    // PC 36: x7 = 10
            addi(x8, x7, 1),    // PC 40: x8 = 11
            addi(x9, x8, 1),    // PC 44: x9 = 12
            addi(x10, x9, 1),   // PC 48: x10 = 13
            addi(x11, x10, 1),  // PC 52: x11 = 14
            addi(x12, x11, 1),  // PC 56: x12 = 15
            addi(x13, x12, 1),  // PC 60: x13 = 16
            addi(x14, x13, 1),  // PC 64: x14 = 17
            add(x20, x14, x3),  // PC 68 (done): x20 = 43
        },
        {0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64, 68},
        {{x1, 40U}, {x2, 13U}, {x3, 26U}, {x4, 7U}, {x5, 8U}, {x6, 9U},
         {x7, 10U}, {x8, 11U}, {x9, 12U}, {x10, 13U}, {x11, 14U}, {x12, 15U},
         {x13, 16U}, {x14, 17U}, {x20, 43U}});
    return test;
}

const ProgramTest& rv32im_branch_independent_taken_redirect_test() {
    static const ProgramTest test = program(
        "branch independent taken early redirect",
        {
            addi(x10, x0, 80),  // PC 0
            addi(x11, x0, 2),   // PC 4
            div(x1, x10, x11),  // PC 8:  older DIV, x1 = 40
            beq(x0, x0, 12),    // PC 12: target at PC 24; always taken
            addi(x1, x0, 99),   // PC 16: skipped
            div(x2, x10, x11),  // PC 20: skipped
            mul(x3, x10, x11),  // PC 24 (target): x3 = 160
        },
        {0, 4, 8, 12, 24},
        {{x1, 40U}, {x2, 0U}, {x3, 160U}, {x10, 80U}, {x11, 2U}});
    return test;
}

const ProgramTest& rv32im_branch_taken_bypasses_dependency_test() {
    static const ProgramTest test = program(
        "branch taken bypasses stalled add",
        {
            addi(x10, x0, 80),  // PC 0
            addi(x11, x0, 2),   // PC 4
            div(x1, x10, x11),  // PC 8:  x1 = 40
            add(x2, x1, x1),    // PC 12: waits for DIV; x2 = 80
            beq(x0, x0, 8),     // PC 16: ready before the ADD; target at PC 24
            addi(x2, x0, 99),   // PC 20: skipped
            mul(x3, x10, x11),  // PC 24 (target): x3 = 160
        },
        {0, 4, 8, 12, 16, 24},
        {{x1, 40U}, {x2, 80U}, {x3, 160U}, {x10, 80U}, {x11, 2U}});
    return test;
}

const ProgramTest& rv32im_branch_taken_discards_wrong_path_test() {
    static const ProgramTest test = program(
        "branch taken discards wrong-path work",
        {
            addi(x10, x0, 80),  // PC 0
            addi(x11, x0, 2),   // PC 4
            div(x1, x10, x11),  // PC 8:  x1 = 40
            bne(x1, x0, 12),    // PC 12: waits for DIV; target at PC 24
            div(x2, x10, x11),  // PC 16: wrong-path DIV
            addi(x1, x0, 99),   // PC 20: wrong-path renamed writer
            add(x3, x1, x0),    // PC 24 (target): x3 = 40
        },
        {0, 4, 8, 12, 24},
        {{x1, 40U}, {x2, 0U}, {x3, 40U}, {x10, 80U}, {x11, 2U}});
    return test;
}

const ProgramTest& rv32im_branch_backward_loop_test() {
    static const ProgramTest test = program(
        "branch backward loop not-taken assumption",
        {
            addi(x1, x0, 3),   // PC 0:  loop count
            addi(x1, x1, -1),  // PC 4 (loop): 3 -> 2 -> 1 -> 0
            bne(x1, x0, -4),   // PC 8:  taken twice, then falls through to PC 12
            addi(x2, x0, 7),   // PC 12: executed after the loop
        },
        {0, 4, 8, 4, 8, 4, 8, 12}, {{x1, 0U}, {x2, 7U}});
    return test;
}

const ProgramTest& rv32im_branch_fast_not_taken_no_gain_test() {
    static const ProgramTest test = program(
        "branch fast not taken no speculation gain",
        {
            bne(x0, x0, 12),  // PC 0:  never taken; done at PC 12
            addi(x2, x0, 7),   // PC 4
            addi(x3, x0, 9),   // PC 8
            add(x4, x2, x3),   // PC 12 (done): x4 = 16
        },
        {0, 4, 8, 12}, {{x2, 7U}, {x3, 9U}, {x4, 16U}});
    return test;
}

std::vector<ProgramTest> branch_test_catalog() {
    std::vector<ProgramTest> tests;
    tests.insert(tests.end(), {
        rv32im_branch_slow_not_taken_speculation_test(),
        rv32im_branch_not_taken_divide_alu_chains_test(),
        rv32im_branch_independent_taken_redirect_test(),
        rv32im_branch_taken_bypasses_dependency_test(),
        rv32im_branch_taken_discards_wrong_path_test(),
        rv32im_branch_backward_loop_test(),
        rv32im_branch_fast_not_taken_no_gain_test()});
    struct Comparison { unsigned condition; int a, b; bool taken; };
    for (const auto c : {
        Comparison{0, 1, 1, true}, {0, 1, -1, false}, {1, 1, -1, true}, {1, 1, 1, false},
        {4, -1, 1, true}, {4, 1, -1, false}, {4, 1, 1, false},
        {5, 1, -1, true}, {5, -1, 1, false}, {5, 1, 1, true},
        {6, 1, -1, true}, {6, -1, 1, false}, {6, 1, 1, false},
        {7, -1, 1, true}, {7, 1, -1, false}, {7, 1, 1, true}}) {
        tests.push_back(program("branch condition " + std::to_string(c.condition) +
            " a=" + std::to_string(c.a) + " b=" + std::to_string(c.b),
            {addi(1, 0, c.a), addi(2, 0, c.b), branch(c.condition, 1, 2, 8),
             addi(3, 0, 99), addi(4, 0, 7)},
            c.taken ? std::vector<std::uint32_t>{0, 4, 8, 16} : std::vector<std::uint32_t>{0, 4, 8, 12, 16},
            {{3, c.taken ? 0U : 99U}, {4, 7}}));
    }
    tests.push_back(program("signed INT_MIN comparison",
        {addi(1, 0, 1), 0x01f09093U, addi(2, 0, -1), branch(4, 1, 2, 8), addi(3, 0, 99), addi(4, 0, 7)},
        {0, 4, 8, 12, 20}, {{1, 0x80000000U}, {3, 0}, {4, 7}}));
    tests.push_back(program("unsigned sign-bit comparison",
        {addi(1, 0, 1), 0x01f09093U, addi(2, 0, 1), branch(7, 1, 2, 8), addi(3, 0, 99), addi(4, 0, 7)},
        {0, 4, 8, 12, 20}, {{1, 0x80000000U}, {3, 0}, {4, 7}}));
    std::vector<std::uint32_t> loop_pcs{0, 4};
    for (unsigned i = 0; i < 20; ++i) loop_pcs.insert(loop_pcs.end(), {8, 12, 16});
    loop_pcs.push_back(20);
    tests.push_back(program("backward loop and repeated rename recovery",
        {addi(1, 0, 20), addi(2, 0, 0), addi(2, 2, 1), addi(1, 1, -1), branch(1, 1, 0, -8), addi(3, 2, 0)},
        loop_pcs, {{1, 0}, {2, 20}, {3, 20}}));
    tests.push_back(program("last-instruction backward branch discards provisional end",
        {addi(1, 0, 3), addi(1, 1, -1), branch(1, 1, 0, -4)},
        {0, 4, 8, 4, 8, 4, 8}, {{1, 0}}));
    tests.push_back(program("fixed encoding branch skips illegal instruction",
        {0x00000463U, 0xffffffffU, addi(1, 0, 7)}, {0, 8}, {{1, 7}}));
    tests.push_back(program("consecutive taken branches",
        {branch(0, 0, 0, 8), addi(1, 0, 99), branch(0, 0, 0, 8), addi(2, 0, 99), addi(3, 0, 7)},
        {0, 8, 16}, {{1, 0}, {2, 0}, {3, 7}}));
    tests.push_back(program("taken branch to exact program end", {branch(0, 0, 0, 4)}, {0}));
    tests.push_back(program("taken branch outside program", {branch(0, 0, 0, 16)}, {0}, {},
        CpuFaultCode::INSTRUCTION_ACCESS_FAULT));
    tests.push_back(program("negative branch target wraps", {branch(0, 0, 0, -4)}, {0}, {},
        CpuFaultCode::INSTRUCTION_ACCESS_FAULT));
    tests.push_back(program("minimum branch offset", {branch(0, 0, 0, -4096)}, {0}, {},
        CpuFaultCode::INSTRUCTION_ACCESS_FAULT));
    tests.push_back(program("taken branch misalignment", {addi(1, 0, 7), branch(0, 0, 0, 2)}, {0}, {{1, 7}},
        CpuFaultCode::INSTRUCTION_ADDRESS_MISALIGNED));
    tests.push_back(program("maximum branch offset misalignment", {branch(0, 0, 0, 4094)}, {}, {},
        CpuFaultCode::INSTRUCTION_ADDRESS_MISALIGNED));
    tests.push_back(program("untaken branch ignores target misalignment",
        {branch(1, 0, 0, 2), addi(1, 0, 7)}, {0, 4}, {{1, 7}}));
    for (unsigned condition : {2U, 3U})
        tests.push_back(program("reserved branch condition " + std::to_string(condition),
            {branch(condition, 0, 0, 4)}, {}, {}, CpuFaultCode::ILLEGAL_INSTRUCTION));
    std::vector<std::uint32_t> distant(1024, 0xffffffffU);
    distant.front() = branch(0, 0, 0, 4092);
    distant.back() = addi(1, 0, 7);
    tests.push_back(program("maximum aligned forward branch", distant, {0, 4092}, {{1, 7}}));
    return tests;
}
