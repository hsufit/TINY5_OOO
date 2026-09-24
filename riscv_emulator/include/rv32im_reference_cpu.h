#ifndef RV32IM_REFERENCE_CPU_H
#define RV32IM_REFERENCE_CPU_H

#include <systemc>

#include <array>
#include <cstddef>
#include <cstdint>

class Rv32imReferenceCpu : public sc_core::sc_module {
public:
    static constexpr std::size_t kRegisterCount = 32;

    sc_core::sc_in<bool> clk{"clk"};
    sc_core::sc_in<bool> reset{"reset"};

    sc_core::sc_out<bool> imem_req_valid{"imem_req_valid"};
    sc_core::sc_in<bool> imem_req_ready{"imem_req_ready"};
    sc_core::sc_out<sc_dt::sc_uint<32>> imem_req_addr{"imem_req_addr"};

    sc_core::sc_in<bool> imem_rsp_valid{"imem_rsp_valid"};
    sc_core::sc_out<bool> imem_rsp_ready{"imem_rsp_ready"};
    sc_core::sc_in<sc_dt::sc_uint<32>> imem_rsp_data{"imem_rsp_data"};
    sc_core::sc_in<sc_dt::sc_uint<2>> imem_rsp_status{"imem_rsp_status"};

    sc_core::sc_out<bool> retire_valid{"retire_valid"};
    sc_core::sc_out<sc_dt::sc_uint<32>> retire_pc{"retire_pc"};
    sc_core::sc_out<sc_dt::sc_uint<5>> retire_rd{"retire_rd"};
    sc_core::sc_out<sc_dt::sc_uint<32>> retire_value{"retire_value"};

    sc_core::sc_out<bool> halted{"halted"};
    sc_core::sc_out<bool> fault{"fault"};
    sc_core::sc_out<sc_dt::sc_uint<2>> fault_code{"fault_code"};

    SC_HAS_PROCESS(Rv32imReferenceCpu);
    explicit Rv32imReferenceCpu(sc_core::sc_module_name name);

private:
    enum class State { IssueRequest, WaitForRequest, WaitForResponse, Stopped };

    void tick();
    void reset_state();
    bool execute(std::uint32_t instruction, unsigned& rd, std::uint32_t& value);
    void raise_fault(unsigned code);

    std::array<std::uint32_t, kRegisterCount> registers_{};
    std::uint32_t pc_{0};
    State state_{State::IssueRequest};
};

#endif

