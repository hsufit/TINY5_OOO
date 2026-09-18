#include "rv32im_cpu.h"

#include <systemc>

#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct RegisterExpectation {
    unsigned index;
    std::uint32_t value;
};

class Rv32imTestbench : public sc_core::sc_module {
public:
    sc_core::sc_clock clock{"clock", 10, sc_core::SC_NS};
    sc_core::sc_signal<bool> reset{"reset"};
    sc_core::sc_signal<bool> halted{"halted"};
    sc_core::sc_signal<bool> fault{"fault"};
    Rv32imCpu cpu{"cpu"};

    SC_HAS_PROCESS(Rv32imTestbench);
    explicit Rv32imTestbench(sc_core::sc_module_name name) : sc_core::sc_module(name) {
        cpu.clk(clock);
        cpu.reset(reset);
        cpu.halted(halted);
        cpu.fault(fault);
        SC_THREAD(run);
    }

    unsigned failures() const { return failures_; }

private:
    template <std::size_t N>
    void start_program(const std::array<std::uint8_t, N>& program) {
        reset.write(true);
        cpu.load_program(program);
        wait(clock.posedge_event());
        wait(clock.negedge_event());
        reset.write(false);
    }

    bool wait_for_completion(unsigned maximum_cycles = 100) {
        for (unsigned cycle = 0; cycle < maximum_cycles; ++cycle) {
            wait(clock.negedge_event());
            if (halted.read() || fault.read()) {
                return true;
            }
        }
        return false;
    }

    template <std::size_t N>
    void expect_success(const std::string& name,
                        const std::array<std::uint8_t, N>& program,
                        const std::vector<RegisterExpectation>& expected) {
        start_program(program);
        if (!wait_for_completion()) {
            fail(name, "timed out");
            return;
        }
        if (fault.read()) {
            fail(name, cpu.fault_message());
            return;
        }

        bool passed = true;
        for (const RegisterExpectation& item : expected) {
            const std::uint32_t actual = cpu.reg(item.index);
            if (actual != item.value) {
                std::cerr << "[FAIL] " << name << ": x" << item.index << " was 0x"
                          << std::hex << std::setw(8) << std::setfill('0') << actual
                          << ", expected 0x" << std::setw(8) << item.value << std::dec
                          << '\n';
                ++failures_;
                passed = false;
            }
        }
        if (cpu.retired_instructions() != N / 4U) {
            fail(name, "retired-instruction count is incorrect");
            passed = false;
        }
        if (passed) {
            std::cout << "[PASS] " << name << '\n';
        }
    }

    template <std::size_t N>
    void expect_fault(const std::string& name,
                      const std::array<std::uint8_t, N>& program,
                      const std::string& expected_message) {
        start_program(program);
        if (!wait_for_completion()) {
            fail(name, "timed out");
            return;
        }
        if (!fault.read()) {
            fail(name, "program unexpectedly completed");
            return;
        }
        if (cpu.fault_message().find(expected_message) == std::string::npos) {
            fail(name, "unexpected fault message: " + cpu.fault_message());
            return;
        }
        std::cout << "[PASS] " << name << '\n';
    }

    void fail(const std::string& name, const std::string& reason) {
        std::cerr << "[FAIL] " << name << ": " << reason << '\n';
        ++failures_;
    }

    void run() {
        // The arrays below are the complete fake instruction memory images.
        // Every group of four bytes is one little-endian RISC-V instruction.
        constexpr std::array<std::uint8_t, 56> register_arithmetic{
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
        };
        expect_success("RV32I register arithmetic", register_arithmetic,
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
                        {14, 0xffffffffU}});

        constexpr std::array<std::uint8_t, 36> immediate_arithmetic{
            0x93, 0x00, 0x00, 0xff,  // addi  x1, x0, -16
            0x13, 0xa1, 0xf0, 0xff,  // slti  x2, x1, -1
            0x93, 0xb1, 0xf0, 0xff,  // sltiu x3, x1, -1
            0x13, 0xc2, 0x50, 0x05,  // xori  x4, x1, 0x55
            0x93, 0x62, 0x30, 0x12,  // ori   x5, x0, 0x123
            0x13, 0xf3, 0xf0, 0x00,  // andi  x6, x1, 0x0f
            0x93, 0x93, 0x42, 0x00,  // slli  x7, x5, 4
            0x13, 0xd4, 0x40, 0x00,  // srli  x8, x1, 4
            0x93, 0xd4, 0x40, 0x40,  // srai  x9, x1, 4
        };
        expect_success("RV32I immediate arithmetic", immediate_arithmetic,
                       {{1, 0xfffffff0U},
                        {2, 1U},
                        {3, 1U},
                        {4, 0xffffffa5U},
                        {5, 0x123U},
                        {6, 0U},
                        {7, 0x1230U},
                        {8, 0x0fffffffU},
                        {9, 0xffffffffU}});

        constexpr std::array<std::uint8_t, 40> multiply_divide{
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
        };
        expect_success("RV32M multiply and divide", multiply_divide,
                       {{3, 0xffffff88U},
                        {4, 0xffffffffU},
                        {5, 0xffffffffU},
                        {6, 5U},
                        {7, 0xfffffffdU},
                        {8, 0x2aaaaaa7U},
                        {9, 0xfffffffeU},
                        {10, 2U}});

        constexpr std::array<std::uint8_t, 40> division_corner_cases{
            0x93, 0x00, 0x90, 0xff,  // addi x1,  x0, -7
            0x13, 0x01, 0x00, 0x00,  // addi x2,  x0, 0
            0xb3, 0xc1, 0x20, 0x02,  // div  x3,  x1, x2
            0x33, 0xd2, 0x20, 0x02,  // divu x4,  x1, x2
            0xb3, 0xe2, 0x20, 0x02,  // rem  x5,  x1, x2
            0x33, 0xf3, 0x20, 0x02,  // remu x6,  x1, x2
            0x93, 0x03, 0x10, 0x00,  // addi x7,  x0, 1
            0x93, 0x93, 0xf3, 0x01,  // slli x7,  x7, 31 (INT32_MIN)
            0x13, 0x04, 0xf0, 0xff,  // addi x8,  x0, -1
            0xb3, 0xc4, 0x83, 0x02,  // div  x9,  x7, x8 (overflow case)
        };
        expect_success("RV32M division corner cases", division_corner_cases,
                       {{3, 0xffffffffU},
                        {4, 0xffffffffU},
                        {5, 0xfffffff9U},
                        {6, 0xfffffff9U},
                        {7, 0x80000000U},
                        {9, 0x80000000U}});

        constexpr std::array<std::uint8_t, 8> hardwired_zero{
            0x13, 0x00, 0xb0, 0x07,  // addi x0, x0, 123 (discarded)
            0x93, 0x00, 0x70, 0x00,  // addi x1, x0, 7
        };
        expect_success("hard-wired x0", hardwired_zero, {{0, 0U}, {1, 7U}});

        constexpr std::array<std::uint8_t, 4> unsupported{
            0xff, 0xff, 0xff, 0xff,
        };
        expect_fault("unsupported opcode", unsupported, "unsupported instruction");

        constexpr std::array<std::uint8_t, 3> truncated{
            0x93, 0x00, 0xa0,
        };
        expect_fault("truncated instruction", truncated, "truncated or misaligned");

        if (failures_ == 0U) {
            std::cout << "All RV32IM arithmetic tests passed\n";
        } else {
            std::cerr << failures_ << " RV32IM arithmetic test(s) failed\n";
        }
        sc_core::sc_stop();
    }

    unsigned failures_{0};
};

}  // namespace

int sc_main(int, char*[]) {
    Rv32imTestbench testbench("testbench");
    sc_core::sc_start();
    return testbench.failures() == 0U ? 0 : 1;
}
