#include RTL_MODEL_HEADER
#include "dual_retirement_scoreboard.h"
#include "instruction_memory.h"
#include "rtl_adapter.h"

#include <systemc>
#include <verilated.h>
#include <verilated_fst_sc.h>

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {
using sc_core::sc_signal;
using sc_dt::sc_uint;

class AddMulWaveformTestbench : public sc_core::sc_module {
public:
    sc_core::sc_clock clock{"clock", 10, sc_core::SC_NS};
    sc_signal<bool> reset{"reset"};
    sc_signal<bool> req_valid{"req_valid"}, req_ready{"req_ready"};
    sc_signal<bool> rsp_valid{"rsp_valid"}, rsp_ready{"rsp_ready"};
    sc_signal<sc_uint<32>> req_addr{"req_addr"}, rsp_data{"rsp_data"};
    sc_signal<sc_uint<2>> rsp_status{"rsp_status"};
    sc_core::sc_vector<sc_signal<bool>> retire_valid{"retire_valid", 2};
    sc_core::sc_vector<sc_signal<sc_uint<32>>> retire_pc{"retire_pc", 2};
    sc_core::sc_vector<sc_signal<sc_uint<5>>> retire_rd{"retire_rd", 2};
    sc_core::sc_vector<sc_signal<sc_uint<32>>> retire_value{"retire_value", 2};
    sc_signal<bool> halted{"halted"}, fault{"fault"};
    sc_signal<sc_uint<2>> fault_code{"fault_code"};
    sc_signal<sc_uint<2>> issue_valid{"issue_valid"}, complete_valid{"complete_valid"};
    sc_signal<sc_uint<2>> dispatch_count{"dispatch_count"};
    sc_signal<sc_uint<2>> rob_capacity{"rob_capacity"}, iq_capacity{"iq_capacity"};
    sc_signal<sc_uint<32>> issue_pc0{"issue_pc0"}, issue_pc1{"issue_pc1"};
    sc_signal<sc_uint<32>> complete_pc0{"complete_pc0"}, complete_pc1{"complete_pc1"};

    RtlCpuAdapter<RTL_MODEL_CLASS> cpu{"cpu"};
    InstructionMemory memory{"memory"};
    DualRetirementScoreboard scoreboard{"scoreboard"};

    SC_HAS_PROCESS(AddMulWaveformTestbench);
    explicit AddMulWaveformTestbench(sc_core::sc_module_name name) : sc_module(name) {
        cpu.clk(clock);
        cpu.reset(reset);
        cpu.imem_req_valid(req_valid);
        cpu.imem_req_ready(req_ready);
        cpu.imem_req_addr(req_addr);
        cpu.imem_rsp_valid(rsp_valid);
        cpu.imem_rsp_ready(rsp_ready);
        cpu.imem_rsp_data(rsp_data);
        cpu.imem_rsp_status(rsp_status);
        cpu.retire0_valid(retire_valid[0]);
        cpu.retire1_valid(retire_valid[1]);
        cpu.retire0_pc(retire_pc[0]);
        cpu.retire1_pc(retire_pc[1]);
        cpu.retire0_rd(retire_rd[0]);
        cpu.retire1_rd(retire_rd[1]);
        cpu.retire0_value(retire_value[0]);
        cpu.retire1_value(retire_value[1]);
        cpu.halted(halted);
        cpu.fault(fault);
        cpu.fault_code(fault_code);
        cpu.debug_issue_valid(issue_valid);
        cpu.debug_complete_valid(complete_valid);
        cpu.debug_dispatch_count(dispatch_count);
        cpu.debug_rob_capacity(rob_capacity);
        cpu.debug_iq_capacity(iq_capacity);
        cpu.debug_issue_pc0(issue_pc0);
        cpu.debug_issue_pc1(issue_pc1);
        cpu.debug_complete_pc0(complete_pc0);
        cpu.debug_complete_pc1(complete_pc1);

        memory.clk(clock);
        memory.reset(reset);
        memory.req_valid(req_valid);
        memory.req_ready(req_ready);
        memory.req_addr(req_addr);
        memory.rsp_valid(rsp_valid);
        memory.rsp_ready(rsp_ready);
        memory.rsp_data(rsp_data);
        memory.rsp_status(rsp_status);

        scoreboard.clk(clock);
        scoreboard.reset(reset);
        for (unsigned lane = 0; lane < 2; ++lane) {
            scoreboard.valid[lane](retire_valid[lane]);
            scoreboard.pc[lane](retire_pc[lane]);
            scoreboard.rd[lane](retire_rd[lane]);
            scoreboard.value[lane](retire_value[lane]);
        }

        SC_METHOD(observe);
        sensitive << clock.posedge_event();
        dont_initialize();
        SC_THREAD(run);
    }

    unsigned failures() const { return failures_; }

private:
    void observe() {
        if (!reset.read() && retire_valid[1].read()) lane1_retired_ = true;
    }

    void fail(const std::string& reason) {
        std::cerr << "[FAIL] ADD/MUL waveform test: " << reason << '\n';
        ++failures_;
    }

    void run() {
        // addi x1,x0,6; addi x2,x0,7; add x3,x1,x2;
        // mul x4,x1,x2; add x5,x3,x4
        const std::vector<std::uint8_t> program{
            0x93, 0x00, 0x60, 0x00,
            0x13, 0x01, 0x70, 0x00,
            0xb3, 0x81, 0x20, 0x00,
            0x33, 0x82, 0x20, 0x02,
            0xb3, 0x82, 0x41, 0x00,
        };
        const std::vector<RetirementEvent> expected{
            {0, 1, 6}, {4, 2, 7}, {8, 3, 13}, {12, 4, 42}, {16, 5, 55},
        };

        reset.write(true);
        memory.load_program(program);
        wait(clock.posedge_event());
        wait(clock.negedge_event());
        reset.write(false);

        unsigned cycle = 0;
        for (; cycle < 500 && !halted.read() && !fault.read(); ++cycle)
            wait(clock.negedge_event());

        if (cycle == 500) fail("timed out");
        if (fault.read()) fail("unexpected processor fault " + std::to_string(fault_code.read().to_uint()));
        if (!halted.read()) fail("processor did not halt");
        if (lane1_retired_) fail("retirement lane 1 was used by a single-issue core");
        if (scoreboard.state.protocol_error()) fail("invalid retirement protocol");
        if (scoreboard.state.events() != expected) fail("retirement trace or result values differ");

        if (failures_ == 0)
            std::cout << "[PASS] ADD/MUL waveform test: x3=13, x4=42, x5=55\n";
        sc_core::sc_stop();
    }

    unsigned failures_{0};
    bool lane1_retired_{false};
};
}  // namespace

int sc_main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);
    const std::string waveform = argc > 1 ? argv[1] : "add_mul.fst";

    AddMulWaveformTestbench testbench{"testbench"};
    sc_core::sc_start(sc_core::SC_ZERO_TIME);

    VerilatedFstSc trace;
    testbench.cpu.model.trace(&trace, 99);
    trace.open(waveform.c_str());
    sc_core::sc_start();
    trace.close();

    if (testbench.failures() == 0)
        std::cout << "Waveform written to " << waveform << '\n';
    return testbench.failures() == 0 ? 0 : 1;
}
