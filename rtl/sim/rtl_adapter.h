#ifndef TINY5_RTL_ADAPTER_H
#define TINY5_RTL_ADAPTER_H
#include <systemc>
#include <cstdint>

// Explicit numeric conversion keeps generated Verilator native port types out
// of the reusable SystemC memory and retirement interfaces.
template <class Model>
class RtlCpuAdapter : public sc_core::sc_module {
public:
    sc_core::sc_in<bool> clk{"clk"};
    sc_core::sc_in<bool> reset{"reset"};
    sc_core::sc_in<bool> imem_req_ready{"imem_req_ready"};
    sc_core::sc_in<bool> imem_rsp_valid{"imem_rsp_valid"};
    sc_core::sc_out<bool> imem_req_valid{"imem_req_valid"};
    sc_core::sc_out<bool> imem_rsp_ready{"imem_rsp_ready"};
    sc_core::sc_out<bool> retire0_valid{"retire0_valid"};
    sc_core::sc_out<bool> retire1_valid{"retire1_valid"};
    sc_core::sc_out<bool> halted{"halted"};
    sc_core::sc_out<bool> fault{"fault"};
    sc_core::sc_in<sc_dt::sc_uint<32>> imem_rsp_data{"imem_rsp_data"};
    sc_core::sc_in<sc_dt::sc_uint<2>> imem_rsp_status{"imem_rsp_status"};
    sc_core::sc_out<sc_dt::sc_uint<32>> imem_req_addr{"imem_req_addr"};
    sc_core::sc_out<sc_dt::sc_uint<32>> retire0_pc{"retire0_pc"};
    sc_core::sc_out<sc_dt::sc_uint<32>> retire1_pc{"retire1_pc"};
    sc_core::sc_out<sc_dt::sc_uint<5>> retire0_rd{"retire0_rd"};
    sc_core::sc_out<sc_dt::sc_uint<5>> retire1_rd{"retire1_rd"};
    sc_core::sc_out<sc_dt::sc_uint<32>> retire0_value{"retire0_value"};
    sc_core::sc_out<sc_dt::sc_uint<32>> retire1_value{"retire1_value"};
    sc_core::sc_out<sc_dt::sc_uint<2>> fault_code{"fault_code"};
    sc_core::sc_out<sc_dt::sc_uint<2>> debug_issue_valid{"debug_issue_valid"};
    sc_core::sc_out<sc_dt::sc_uint<2>> debug_complete_valid{"debug_complete_valid"};
    sc_core::sc_out<sc_dt::sc_uint<2>> debug_dispatch_count{"debug_dispatch_count"};
    sc_core::sc_out<sc_dt::sc_uint<2>> debug_rob_capacity{"debug_rob_capacity"};
    sc_core::sc_out<sc_dt::sc_uint<2>> debug_iq_capacity{"debug_iq_capacity"};
    sc_core::sc_out<sc_dt::sc_uint<32>> debug_issue_pc0{"debug_issue_pc0"};
    sc_core::sc_out<sc_dt::sc_uint<32>> debug_issue_pc1{"debug_issue_pc1"};
    sc_core::sc_out<sc_dt::sc_uint<32>> debug_complete_pc0{"debug_complete_pc0"};
    sc_core::sc_out<sc_dt::sc_uint<32>> debug_complete_pc1{"debug_complete_pc1"};
    Model model{"model"};
    SC_HAS_PROCESS(RtlCpuAdapter);
    explicit RtlCpuAdapter(sc_core::sc_module_name name) : sc_module(name) {
        model.clk(clk);
        model.reset(reset);
        model.imem_req_ready(imem_req_ready);
        model.imem_rsp_valid(imem_rsp_valid);
        model.imem_req_valid(imem_req_valid);
        model.imem_rsp_ready(imem_rsp_ready);
        model.retire0_valid(retire0_valid);
        model.retire1_valid(retire1_valid);
        model.halted(halted);
        model.fault(fault);
        model.imem_rsp_data(native_imem_rsp_data);
        model.imem_rsp_status(native_imem_rsp_status);
        model.imem_req_addr(native_imem_req_addr);
        model.retire0_pc(native_retire0_pc);
        model.retire1_pc(native_retire1_pc);
        model.retire0_rd(native_retire0_rd);
        model.retire1_rd(native_retire1_rd);
        model.retire0_value(native_retire0_value);
        model.retire1_value(native_retire1_value);
        model.fault_code(native_fault_code);
        model.debug_issue_valid(native_debug_issue_valid);
        model.debug_complete_valid(native_debug_complete_valid);
        model.debug_dispatch_count(native_debug_dispatch_count);
        model.debug_rob_capacity(native_debug_rob_capacity);
        model.debug_iq_capacity(native_debug_iq_capacity);
        model.debug_issue_pc0(native_debug_issue_pc0);
        model.debug_issue_pc1(native_debug_issue_pc1);
        model.debug_complete_pc0(native_debug_complete_pc0);
        model.debug_complete_pc1(native_debug_complete_pc1);
        SC_METHOD(convert);
        sensitive << imem_rsp_data << imem_rsp_status << native_imem_req_addr << native_retire0_pc << native_retire1_pc << native_retire0_rd << native_retire1_rd << native_retire0_value << native_retire1_value << native_fault_code << native_debug_issue_valid << native_debug_complete_valid << native_debug_dispatch_count << native_debug_rob_capacity << native_debug_iq_capacity << native_debug_issue_pc0 << native_debug_issue_pc1 << native_debug_complete_pc0 << native_debug_complete_pc1;
    }
private:
    sc_core::sc_signal<std::uint32_t> native_imem_rsp_data{"native_imem_rsp_data"};
    sc_core::sc_signal<std::uint32_t> native_imem_rsp_status{"native_imem_rsp_status"};
    sc_core::sc_signal<std::uint32_t> native_imem_req_addr{"native_imem_req_addr"};
    sc_core::sc_signal<std::uint32_t> native_retire0_pc{"native_retire0_pc"};
    sc_core::sc_signal<std::uint32_t> native_retire1_pc{"native_retire1_pc"};
    sc_core::sc_signal<std::uint32_t> native_retire0_rd{"native_retire0_rd"};
    sc_core::sc_signal<std::uint32_t> native_retire1_rd{"native_retire1_rd"};
    sc_core::sc_signal<std::uint32_t> native_retire0_value{"native_retire0_value"};
    sc_core::sc_signal<std::uint32_t> native_retire1_value{"native_retire1_value"};
    sc_core::sc_signal<std::uint32_t> native_fault_code{"native_fault_code"};
    sc_core::sc_signal<std::uint32_t> native_debug_issue_valid{"native_debug_issue_valid"};
    sc_core::sc_signal<std::uint32_t> native_debug_complete_valid{"native_debug_complete_valid"};
    sc_core::sc_signal<std::uint32_t> native_debug_dispatch_count{"native_debug_dispatch_count"};
    sc_core::sc_signal<std::uint32_t> native_debug_rob_capacity{"native_debug_rob_capacity"};
    sc_core::sc_signal<std::uint32_t> native_debug_iq_capacity{"native_debug_iq_capacity"};
    sc_core::sc_signal<std::uint32_t> native_debug_issue_pc0{"native_debug_issue_pc0"};
    sc_core::sc_signal<std::uint32_t> native_debug_issue_pc1{"native_debug_issue_pc1"};
    sc_core::sc_signal<std::uint32_t> native_debug_complete_pc0{"native_debug_complete_pc0"};
    sc_core::sc_signal<std::uint32_t> native_debug_complete_pc1{"native_debug_complete_pc1"};
    void convert() {
        native_imem_rsp_data.write(imem_rsp_data.read().to_uint());
        native_imem_rsp_status.write(imem_rsp_status.read().to_uint());
        imem_req_addr.write(native_imem_req_addr.read());
        retire0_pc.write(native_retire0_pc.read());
        retire1_pc.write(native_retire1_pc.read());
        retire0_rd.write(native_retire0_rd.read());
        retire1_rd.write(native_retire1_rd.read());
        retire0_value.write(native_retire0_value.read());
        retire1_value.write(native_retire1_value.read());
        fault_code.write(native_fault_code.read());
        debug_issue_valid.write(native_debug_issue_valid.read());
        debug_complete_valid.write(native_debug_complete_valid.read());
        debug_dispatch_count.write(native_debug_dispatch_count.read());
        debug_rob_capacity.write(native_debug_rob_capacity.read());
        debug_iq_capacity.write(native_debug_iq_capacity.read());
        debug_issue_pc0.write(native_debug_issue_pc0.read());
        debug_issue_pc1.write(native_debug_issue_pc1.read());
        debug_complete_pc0.write(native_debug_complete_pc0.read());
        debug_complete_pc1.write(native_debug_complete_pc1.read());
    }
};
#endif
