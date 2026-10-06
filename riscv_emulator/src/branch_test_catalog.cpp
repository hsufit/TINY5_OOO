#include "test_catalog.h"
#include <utility>

namespace {
std::uint32_t addi(unsigned rd, unsigned rs, int immediate) {
    return ((static_cast<std::uint32_t>(immediate) & 4095U) << 20U) |
           (rs << 15U) | (rd << 7U) | 0x13U;
}
std::uint32_t branch(unsigned condition, unsigned a, unsigned b, int offset) {
    const auto bits = static_cast<std::uint32_t>(offset);
    return ((bits & 0x1000U) << 19U) | ((bits & 0x7e0U) << 20U) | (b << 20U) |
           (a << 15U) | (condition << 12U) | ((bits & 0x1eU) << 7U) |
           ((bits & 0x800U) >> 4U) | 0x63U;
}
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
} // namespace

std::vector<ProgramTest> branch_test_catalog() {
    std::vector<ProgramTest> tests;
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
