#ifndef RV32IM_CPU_H
#define RV32IM_CPU_H

#include <systemc>

#include <cstddef>
#include <cstdint>
#include <memory>

class Rv32imCpu : public sc_core::sc_module {
public:
    static constexpr std::size_t kRegisterCount = 32;
    enum class SchedulingMode { InOrder, OutOfOrder };

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
    sc_core::sc_out<bool> retire1_valid{"retire1_valid"};
    sc_core::sc_out<sc_dt::sc_uint<32>> retire1_pc{"retire1_pc"};
    sc_core::sc_out<sc_dt::sc_uint<5>> retire1_rd{"retire1_rd"};
    sc_core::sc_out<sc_dt::sc_uint<32>> retire1_value{"retire1_value"};

    sc_core::sc_out<bool> halted{"halted"};
    sc_core::sc_out<bool> fault{"fault"};
    sc_core::sc_out<sc_dt::sc_uint<2>> fault_code{"fault_code"};

    SC_HAS_PROCESS(Rv32imCpu);
    explicit Rv32imCpu(sc_core::sc_module_name name, unsigned issue_width = 1,
                       SchedulingMode mode = SchedulingMode::InOrder, bool branch_at_retire = false);
    ~Rv32imCpu() override;

private:
    struct Pipeline;
    void tick();
    void drive_outputs();
    const unsigned issue_width_;
    const SchedulingMode mode_;
    const bool branch_at_retire_;
    std::unique_ptr<Pipeline> pipeline_;
    sc_core::sc_event state_changed_;
};

#endif
