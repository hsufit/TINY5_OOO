#include "cpu_protocol.h"
#include "instruction_memory.h"
#include "retirement_scoreboard.h"
#include "rv32im_cpu.h"
#include "test_catalog.h"

#include <systemc>

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

class Rv32imTestbench : public sc_core::sc_module {
public:
    sc_core::sc_clock clock{"clock", 10, sc_core::SC_NS};
    sc_core::sc_signal<bool> reset{"reset"};
    sc_core::sc_signal<bool> imem_req_valid{"imem_req_valid"};
    sc_core::sc_signal<bool> imem_req_ready{"imem_req_ready"};
    sc_core::sc_signal<sc_dt::sc_uint<32>> imem_req_addr{"imem_req_addr"};
    sc_core::sc_signal<bool> imem_rsp_valid{"imem_rsp_valid"};
    sc_core::sc_signal<bool> imem_rsp_ready{"imem_rsp_ready"};
    sc_core::sc_signal<sc_dt::sc_uint<32>> imem_rsp_data{"imem_rsp_data"};
    sc_core::sc_signal<sc_dt::sc_uint<2>> imem_rsp_status{"imem_rsp_status"};
    sc_core::sc_signal<bool> retire_valid{"retire_valid"};
    sc_core::sc_signal<sc_dt::sc_uint<32>> retire_pc{"retire_pc"};
    sc_core::sc_signal<sc_dt::sc_uint<5>> retire_rd{"retire_rd"};
    sc_core::sc_signal<sc_dt::sc_uint<32>> retire_value{"retire_value"};
    sc_core::sc_signal<bool> halted{"halted"};
    sc_core::sc_signal<bool> fault{"fault"};
    sc_core::sc_signal<sc_dt::sc_uint<2>> fault_code{"fault_code"};

    Rv32imCpu cpu{"cpu"};
    InstructionMemory memory{"memory"};
    RetirementScoreboard scoreboard{"scoreboard"};

    SC_HAS_PROCESS(Rv32imTestbench);
    explicit Rv32imTestbench(sc_core::sc_module_name name) : sc_core::sc_module(name) {
        cpu.clk(clock);
        cpu.reset(reset);
        cpu.imem_req_valid(imem_req_valid);
        cpu.imem_req_ready(imem_req_ready);
        cpu.imem_req_addr(imem_req_addr);
        cpu.imem_rsp_valid(imem_rsp_valid);
        cpu.imem_rsp_ready(imem_rsp_ready);
        cpu.imem_rsp_data(imem_rsp_data);
        cpu.imem_rsp_status(imem_rsp_status);
        cpu.retire_valid(retire_valid);
        cpu.retire_pc(retire_pc);
        cpu.retire_rd(retire_rd);
        cpu.retire_value(retire_value);
        cpu.halted(halted);
        cpu.fault(fault);
        cpu.fault_code(fault_code);

        memory.clk(clock);
        memory.reset(reset);
        memory.req_valid(imem_req_valid);
        memory.req_ready(imem_req_ready);
        memory.req_addr(imem_req_addr);
        memory.rsp_valid(imem_rsp_valid);
        memory.rsp_ready(imem_rsp_ready);
        memory.rsp_data(imem_rsp_data);
        memory.rsp_status(imem_rsp_status);

        scoreboard.clk(clock);
        scoreboard.reset(reset);
        scoreboard.retire_valid(retire_valid);
        scoreboard.retire_pc(retire_pc);
        scoreboard.retire_rd(retire_rd);
        scoreboard.retire_value(retire_value);

        SC_THREAD(run);
    }

    unsigned failures() const { return failures_; }

private:
    void start_program(const ProgramTest& test, InstructionMemory::Timing timing) {
        reset.write(true);
        memory.set_timing(timing);
        memory.load_program(test.program);
        wait(clock.posedge_event());
        wait(clock.negedge_event());
        reset.write(false);
    }

    bool wait_for_completion(unsigned maximum_cycles = 1000U) {
        for (unsigned cycle = 0; cycle < maximum_cycles; ++cycle) {
            wait(clock.negedge_event());
            if (halted.read() || fault.read()) {
                return true;
            }
        }
        return false;
    }

    void fail(const std::string& name, const std::string& reason) {
        std::cerr << "[FAIL] " << name << ": " << reason << '\n';
        ++failures_;
    }

    void check_test(const ProgramTest& test, const std::string& mode) {
        const std::string name = test.name + " [" + mode + "]";
        const unsigned failures_before = failures_;
        if (!wait_for_completion()) {
            fail(name, "timed out");
            return;
        }

        const bool expected_halt = test.expected_termination == ExpectedTermination::Halt;
        if (halted.read() != expected_halt || fault.read() == expected_halt) {
            fail(name, "incorrect halt/fault termination");
        }

        const auto actual_fault = static_cast<CpuFaultCode>(fault_code.read().to_uint());
        if (actual_fault != test.expected_fault) {
            fail(name, "incorrect fault code");
        }
        if (scoreboard.protocol_error()) {
            fail(name, "invalid retirement event for x0");
        }
        if (scoreboard.retirement_count() != test.expected_retirement_count) {
            fail(name, "incorrect retirement count");
        }
        if (scoreboard.retirement_pcs() != test.expected_retirement_pcs) {
            fail(name, "incorrect retirement PC sequence");
        }

        for (const RegisterExpectation& item : test.expected_registers) {
            const std::uint32_t actual = scoreboard.reg(item.index);
            if (actual != item.value) {
                std::cerr << "[FAIL] " << name << ": x" << item.index << " was 0x"
                          << std::hex << std::setw(8) << std::setfill('0') << actual
                          << ", expected 0x" << std::setw(8) << item.value << std::dec
                          << '\n';
                ++failures_;
            }
        }

        const bool saved_halt = halted.read();
        const bool saved_fault = fault.read();
        const unsigned saved_code = fault_code.read().to_uint();
        wait(clock.posedge_event());
        wait(clock.posedge_event());
        wait(clock.negedge_event());
        if (halted.read() != saved_halt || fault.read() != saved_fault ||
            fault_code.read().to_uint() != saved_code) {
            fail(name, "termination outputs were not sticky");
        }

        if (failures_ == failures_before) {
            std::cout << "[PASS] " << name << '\n';
        }
    }

    void run() {
        for (const ProgramTest& test : rv32im_test_catalog()) {
            start_program(test, InstructionMemory::normal_timing());
            check_test(test, "normal");
            start_program(test, InstructionMemory::stalled_timing());
            check_test(test, "stalled");
        }

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
