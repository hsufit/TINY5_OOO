#include RTL_MODEL_HEADER
#include "rtl_adapter.h"
#include "dual_retirement_scoreboard.h"
#include "instruction_memory.h"
#include "rv32im_cpu.h"
#include "test_catalog.h"
#include <systemc>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {
using sc_core::sc_signal;
using sc_dt::sc_uint;

struct FetchSignals : sc_core::sc_module {
    sc_signal<bool> req_valid{"req_valid"}, req_ready{"req_ready"};
    sc_signal<sc_uint<32>> req_addr{"req_addr"};
    sc_signal<bool> rsp_valid{"rsp_valid"}, rsp_ready{"rsp_ready"};
    sc_signal<sc_uint<32>> rsp_data{"rsp_data"};
    sc_signal<sc_uint<2>> rsp_status{"rsp_status"}, filtered_status{"filtered_status"};
    explicit FetchSignals(sc_core::sc_module_name name) : sc_module(name) {}
};
struct RetireSignals : sc_core::sc_module {
    sc_core::sc_vector<sc_signal<bool>> valid{"valid", 2};
    sc_core::sc_vector<sc_signal<sc_uint<32>>> pc{"pc", 2}, value{"value", 2};
    sc_core::sc_vector<sc_signal<sc_uint<5>>> rd{"rd", 2};
    sc_signal<bool> halted{"halted"}, fault{"fault"};
    sc_signal<sc_uint<2>> code{"code"};
    explicit RetireSignals(sc_core::sc_module_name name) : sc_module(name) {}
};

std::uint32_t imm(unsigned f3, unsigned rd, unsigned rs, unsigned immediate) {
    return ((immediate & 4095U) << 20U) | (rs << 15U) | (f3 << 12U) | (rd << 7U) | 0x13U;
}
std::uint32_t reg(unsigned f7, unsigned f3, unsigned rd, unsigned a, unsigned b) {
    return (f7 << 25U) | (b << 20U) | (a << 15U) | (f3 << 12U) | (rd << 7U) | 0x33U;
}
std::vector<std::uint8_t> bytes(const std::vector<std::uint32_t>& words) {
    std::vector<std::uint8_t> result;
    for (auto word : words)
        for (unsigned shift = 0; shift < 32U; shift += 8U)
            result.push_back(static_cast<std::uint8_t>(word >> shift));
    return result;
}
std::vector<std::uint8_t> random_program(unsigned seed) {
    std::mt19937 random(seed);
    std::vector<std::uint32_t> words;
    for (unsigned rd = 1; rd < 32; ++rd) words.push_back(imm(0, rd, 0, random()));
    for (unsigned i = 0; i < 225; ++i) {
        const unsigned rd = random() % 32U, a = random() % 32U, b = random() % 32U;
        const unsigned f3 = random() % 8U;
        switch (random() % 3U) {
        case 0: {
            unsigned value = random() & 4095U;
            if (f3 == 1U || f3 == 5U) {
                value &= 31U;
                if (f3 == 5U && (random() & 1U)) value |= 0x400U;
            }
            words.push_back(imm(f3, rd, a, value));
            break;
        }
        case 1: words.push_back(reg((f3 == 0U || f3 == 5U) && (random() & 1U) ? 32U : 0U,
                                    f3, rd, a, b)); break;
        default: words.push_back(reg(1, f3, rd, a, b)); break;
        }
    }
    return bytes(words);
}

class Testbench : public sc_core::sc_module {
public:
    sc_core::sc_clock clock{"clock", 10, sc_core::SC_NS};
    sc_signal<bool> reset{"reset"}, reserved_status{"reserved_status"};
    FetchSignals rtl_fetch{"rtl_fetch"}, reference_fetch{"reference_fetch"};
    RetireSignals rtl_retire{"rtl_retire"}, reference_retire{"reference_retire"};
    RtlCpuAdapter<RTL_MODEL_CLASS> cpu{"cpu"};
    Rv32imCpu reference{"reference"};
    InstructionMemory memory{"memory"}, reference_memory{"reference_memory"};
    DualRetirementScoreboard scoreboard{"scoreboard"}, reference_scoreboard{"reference_scoreboard"};
    sc_signal<sc_uint<2>> issue_valid{"issue_valid"}, complete_valid{"complete_valid"};
    sc_signal<sc_uint<2>> dispatch_count{"dispatch_count"}, rob_capacity{"rob_capacity"}, iq_capacity{"iq_capacity"};
    sc_signal<sc_uint<32>> issue_pc0{"issue_pc0"}, issue_pc1{"issue_pc1"};
    sc_signal<sc_uint<32>> complete_pc0{"complete_pc0"}, complete_pc1{"complete_pc1"};

    SC_HAS_PROCESS(Testbench);
    explicit Testbench(sc_core::sc_module_name name) : sc_module(name) {
        bind_fetch(cpu, rtl_fetch);
        bind_fetch(reference, reference_fetch);
        bind_memory(memory, rtl_fetch);
        bind_memory(reference_memory, reference_fetch);
        cpu.retire0_valid(rtl_retire.valid[0]); cpu.retire1_valid(rtl_retire.valid[1]);
        cpu.retire0_pc(rtl_retire.pc[0]); cpu.retire1_pc(rtl_retire.pc[1]);
        cpu.retire0_rd(rtl_retire.rd[0]); cpu.retire1_rd(rtl_retire.rd[1]);
        cpu.retire0_value(rtl_retire.value[0]); cpu.retire1_value(rtl_retire.value[1]);
        cpu.halted(rtl_retire.halted); cpu.fault(rtl_retire.fault); cpu.fault_code(rtl_retire.code);
        reference.retire_valid(reference_retire.valid[0]);
        reference.retire_pc(reference_retire.pc[0]);
        reference.retire_rd(reference_retire.rd[0]);
        reference.retire_value(reference_retire.value[0]);
        reference.halted(reference_retire.halted); reference.fault(reference_retire.fault);
        reference.fault_code(reference_retire.code);
        bind_scoreboard(scoreboard, rtl_retire);
        bind_scoreboard(reference_scoreboard, reference_retire);
        cpu.debug_issue_valid(issue_valid); cpu.debug_issue_pc0(issue_pc0); cpu.debug_issue_pc1(issue_pc1);
        cpu.debug_complete_valid(complete_valid); cpu.debug_complete_pc0(complete_pc0);
        cpu.debug_complete_pc1(complete_pc1); cpu.debug_dispatch_count(dispatch_count);
        cpu.debug_rob_capacity(rob_capacity); cpu.debug_iq_capacity(iq_capacity);
        SC_METHOD(filter_status);
        sensitive << reserved_status << rtl_fetch.rsp_status << reference_fetch.rsp_status;
        SC_METHOD(observe);
        sensitive << clock.posedge_event();
        dont_initialize();
        SC_THREAD(run);
    }
    unsigned failures() const { return failures_; }

private:
    template<class Cpu> void bind_fetch(Cpu& target, FetchSignals& signals) {
        target.clk(clock); target.reset(reset);
        target.imem_req_valid(signals.req_valid); target.imem_req_ready(signals.req_ready);
        target.imem_req_addr(signals.req_addr); target.imem_rsp_valid(signals.rsp_valid);
        target.imem_rsp_ready(signals.rsp_ready); target.imem_rsp_data(signals.rsp_data);
        target.imem_rsp_status(signals.filtered_status);
    }
    void bind_memory(InstructionMemory& target, FetchSignals& signals) {
        target.clk(clock); target.reset(reset);
        target.req_valid(signals.req_valid); target.req_ready(signals.req_ready); target.req_addr(signals.req_addr);
        target.rsp_valid(signals.rsp_valid); target.rsp_ready(signals.rsp_ready);
        target.rsp_data(signals.rsp_data); target.rsp_status(signals.rsp_status);
    }
    void bind_scoreboard(DualRetirementScoreboard& target, RetireSignals& signals) {
        target.clk(clock); target.reset(reset);
        for (unsigned lane = 0; lane < 2; ++lane) {
            target.valid[lane](signals.valid[lane]); target.pc[lane](signals.pc[lane]);
            target.rd[lane](signals.rd[lane]); target.value[lane](signals.value[lane]);
        }
    }
    void filter_status() {
        rtl_fetch.filtered_status.write(reserved_status.read() ? 3U : rtl_fetch.rsp_status.read().to_uint());
        reference_fetch.filtered_status.write(reserved_status.read() ? 3U : reference_fetch.rsp_status.read().to_uint());
    }
    void observe() {
        if (reset.read()) { issued_.clear(); completed_.clear(); return; }
        const unsigned issued = issue_valid.read().to_uint();
        if (issued == 3U) ++dual_issue_cycles_;
        if (rtl_retire.valid[1].read()) ++dual_retire_cycles_;
        if (dispatch_count.read() == 2U) ++dual_dispatch_cycles_;
        if (iq_capacity.read() == 0U) ++full_iq_cycles_;
        if (rob_capacity.read() == 0U) ++full_rob_cycles_;
        if ((issued & 1U) != 0U) issued_.push_back(issue_pc0.read().to_uint());
        if ((issued & 2U) != 0U) issued_.push_back(issue_pc1.read().to_uint());
        const unsigned completed = complete_valid.read().to_uint();
        if ((completed & 1U) != 0U) completed_.push_back(complete_pc0.read().to_uint());
        if ((completed & 2U) != 0U) completed_.push_back(complete_pc1.read().to_uint());
    }
    void fail(const std::string& reason) {
        std::cerr << "[FAIL] " << active_name_ << ": " << reason << '\n';
        ++failures_;
    }
    void start(const std::vector<std::uint8_t>& program, InstructionMemory::Timing timing, bool reserved = false) {
        reset.write(true);
        reserved_status.write(reserved);
        memory.set_timing(timing); reference_memory.set_timing(timing);
        memory.load_program(program); reference_memory.load_program(program);
        wait(clock.posedge_event());
        wait(clock.negedge_event());
        reset.write(false);
    }
    bool stopped(const RetireSignals& signals) const { return signals.halted.read() || signals.fault.read(); }
    void check(const std::vector<std::uint8_t>& program, const ProgramTest* catalog = nullptr,
               bool require_overlap = false) {
        const unsigned before = failures_;
        const unsigned timeout = 2000U + 128U * static_cast<unsigned>(program.size() / 4U);
        unsigned cycle = 0;
        for (; cycle < timeout; ++cycle) {
            wait(clock.negedge_event());
            if (stopped(rtl_retire) && stopped(reference_retire)) break;
        }
        if (cycle == timeout) { fail("timeout"); return; }
        const auto& actual = scoreboard.state;
        const auto& expected = reference_scoreboard.state;
        if (actual.protocol_error()) fail("invalid retirement protocol");
        if (actual.events() != expected.events()) {
            fail("retirement trace differs from SystemC reference (actual " +
                 std::to_string(actual.retirement_count()) + ", expected " +
                 std::to_string(expected.retirement_count()) + ")");
            for (std::size_t i = 0; i < std::min(actual.events().size(), expected.events().size()); ++i) {
                if (!(actual.events()[i] == expected.events()[i])) {
                    const auto a = actual.events()[i], e = expected.events()[i];
                    std::cerr << "  event " << i << ": pc=" << a.pc << " rd=" << a.rd
                              << " value=" << std::hex << a.value << "; expected pc=" << e.pc
                              << " rd=" << e.rd << " value=" << e.value << std::dec << '\n';
                    break;
                }
            }
        }
        if (rtl_retire.halted.read() != reference_retire.halted.read() ||
            rtl_retire.fault.read() != reference_retire.fault.read() ||
            rtl_retire.code.read() != reference_retire.code.read()) fail("termination differs from reference");
        if (catalog != nullptr) {
            if (actual.retirement_count() != catalog->expected_retirement_count ||
                actual.retirement_pcs() != catalog->expected_retirement_pcs) fail("catalog retirement expectation");
            for (const auto& item : catalog->expected_registers)
                if (actual.reg(item.index) != item.value) fail("catalog register x" + std::to_string(item.index));
            if (rtl_retire.code.read().to_uint() != static_cast<unsigned>(catalog->expected_fault))
                fail("catalog fault code");
            if (rtl_retire.halted.read() != (catalog->expected_termination == ExpectedTermination::Halt))
                fail("catalog halt expectation");
        }
        if (!std::is_sorted(issued_.begin(), issued_.end())) {
            ++reordered_programs_;
            if (RTL_MODE != 2) fail("in-order controller issued past an older instruction");
        }
        if (require_overlap) {
            const auto divide = std::find(completed_.begin(), completed_.end(), 0U);
            const auto add = std::find(completed_.begin(), completed_.end(), 4U);
            if (divide == completed_.end() || add == completed_.end() || add >= divide)
                fail("independent integer operation did not complete during older divide");
        }
        const auto saved_count = actual.retirement_count();
        const bool saved_halt = rtl_retire.halted.read(), saved_fault = rtl_retire.fault.read();
        const auto saved_code = rtl_retire.code.read();
        for (unsigned i = 0; i < 4; ++i) wait(clock.negedge_event());
        if (actual.retirement_count() != saved_count || rtl_retire.halted.read() != saved_halt ||
            rtl_retire.fault.read() != saved_fault || rtl_retire.code.read() != saved_code ||
            rtl_fetch.req_valid.read()) fail("termination is not sticky and quiescent");
        ++programs_;
        if (before == failures_) std::cout << "[PASS] " << active_name_ << '\n';
    }
    void differential(const std::string& name, const std::vector<std::uint8_t>& program,
                      bool require_overlap = false) {
        for (unsigned mode = 0; mode < 2; ++mode) {
            active_name_ = name + (mode == 0 ? " [normal]" : " [stalled]");
            start(program, mode == 0 ? InstructionMemory::normal_timing() : InstructionMemory::stalled_timing());
            check(program, nullptr, require_overlap);
        }
    }
    void run() {
        for (const auto& test : rv32im_test_catalog()) {
            for (unsigned mode = 0; mode < 2; ++mode) {
                active_name_ = test.name + (mode == 0 ? " [normal]" : " [stalled]");
                start(test.program, mode == 0 ? InstructionMemory::normal_timing() : InstructionMemory::stalled_timing());
                check(test.program, &test);
            }
        }
        differential("empty program", {});
        differential("independent integer work during divide", bytes({
            reg(1, 4, 1, 0, 0), imm(0, 2, 0, 7), reg(0, 0, 3, 2, 2)}), true);
        std::vector<std::uint32_t> pressure{reg(1, 4, 1, 0, 0), imm(0, 2, 1, 7)};
        for (unsigned rd = 3; rd < 31; ++rd) pressure.push_back(imm(0, rd, 0, rd));
        pressure.push_back(reg(1, 4, 1, 2, 3));
        pressure.push_back(reg(1, 5, 2, 3, 4));
        for (unsigned rd = 3; rd < 31; ++rd) pressure.push_back(reg(0, 0, rd, 1, rd));
        differential("buffering dual issue and ready bypass", bytes(pressure));
        differential("RAW WAR WAW and repeated rename", bytes({
            imm(0, 1, 0, 7), imm(0, 2, 0, 3), reg(1, 0, 3, 1, 2),
            reg(0, 0, 4, 3, 1), reg(0, 0, 5, 1, 2), imm(0, 1, 0, 5),
            reg(0, 0, 1, 1, 1), reg(0, 0, 1, 1, 1), reg(0, 0, 6, 1, 4)}));
        differential("multiply signed extremes and division overflow", bytes({
            imm(0, 1, 0, 1), imm(1, 1, 1, 31), imm(0, 2, 0, 4095),
            reg(1, 1, 3, 1, 2), reg(1, 2, 4, 1, 2), reg(1, 3, 5, 1, 2),
            reg(1, 4, 6, 1, 2), reg(1, 6, 7, 1, 2), reg(1, 7, 8, 1, 2),
            reg(1, 6, 9, 1, 0), reg(1, 0, 0, 1, 2), reg(1, 4, 0, 1, 2)}));
        differential("end of program drains long operation", bytes({reg(1, 4, 1, 0, 0)}));
        differential("precise illegal after long operation", bytes({
            reg(1, 4, 1, 0, 0), imm(0, 2, 0, 4), 0xffffffffU, imm(0, 3, 0, 99)}));
        auto truncated = bytes({reg(1, 4, 1, 0, 0), imm(0, 2, 0, 4)});
        truncated.insert(truncated.end(), {0x13, 0, 0});
        differential("precise truncated fetch after valid prefix", truncated);
        for (const auto illegal : {imm(1, 1, 0, 32), imm(5, 1, 0, 32), reg(2, 0, 1, 0, 0),
                                   0x00000037U, 0x00000017U, 0x00000003U, 0x00000063U, 0x00000073U})
            differential("strict illegal encoding " + std::to_string(illegal), bytes({imm(0, 1, 0, 5), illegal}));
        for (unsigned seed = 1; seed <= 16; ++seed)
            differential("random arithmetic seed " + std::to_string(seed), random_program(seed));
        std::vector<std::uint32_t> full_image(1024, imm(0, 1, 1, 1));
        differential("4 KiB image and repeated ROB wrap", bytes(full_image));
        active_name_ = "reserved memory response status";
        const auto one = bytes({imm(0, 1, 0, 7)});
        start(one, InstructionMemory::normal_timing(), true);
        check(one);
        // Reset cancels both outstanding fetches and iterative arithmetic.
        for (const unsigned delay : {1U, 3U, 12U, 24U, 70U}) {
            active_name_ = "reset in flight after " + std::to_string(delay) + " cycles";
            start(bytes(pressure), InstructionMemory::stalled_timing());
            for (unsigned i = 0; i < delay; ++i) wait(clock.negedge_event());
            start(one, InstructionMemory::normal_timing());
            check(one);
        }
        active_name_ = "microarchitecture coverage";
        if (RTL_MODE != 0 && (dual_issue_cycles_ == 0 || dual_retire_cycles_ == 0 || dual_dispatch_cycles_ == 0))
            fail("dual issue/retirement/dispatch was not exercised");
        if (RTL_MODE == 0 && (dual_issue_cycles_ != 0 || dual_retire_cycles_ != 0))
            fail("single-issue width exceeded");
        if (RTL_MODE == 2 && reordered_programs_ == 0) fail("out-of-order bypass was not exercised");
        std::cout << "Programs=" << programs_ << " dual_issue_cycles=" << dual_issue_cycles_
                  << " dual_retire_cycles=" << dual_retire_cycles_ << " dual_dispatch_cycles=" << dual_dispatch_cycles_
                  << " reordered_programs=" << reordered_programs_ << " IQ_full=" << full_iq_cycles_
                  << " ROB_full=" << full_rob_cycles_ << '\n';
        if (failures_ == 0) std::cout << "All RTL differential tests passed\n";
        sc_core::sc_stop();
    }
    std::string active_name_;
    std::vector<std::uint32_t> issued_, completed_;
    unsigned failures_{0}, programs_{0}, dual_issue_cycles_{0}, dual_retire_cycles_{0};
    unsigned dual_dispatch_cycles_{0}, reordered_programs_{0}, full_iq_cycles_{0}, full_rob_cycles_{0};
};
} // namespace

int sc_main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Testbench testbench{"testbench"};
    sc_core::sc_start();
    return testbench.failures() == 0 ? 0 : 1;
}
