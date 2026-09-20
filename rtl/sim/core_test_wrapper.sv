module core_test_wrapper (
  input logic clk, reset,
  output logic imem_req_valid,
  input logic imem_req_ready,
  output logic [31:0] imem_req_addr,
  input logic imem_rsp_valid,
  output logic imem_rsp_ready,
  input logic [31:0] imem_rsp_data,
  input logic [1:0] imem_rsp_status,
  output logic retire0_valid, retire1_valid,
  output logic [31:0] retire0_pc, retire1_pc, retire0_value, retire1_value,
  output logic [4:0] retire0_rd, retire1_rd,
  output logic halted, fault,
  output logic [1:0] fault_code,
  output logic [1:0] debug_issue_valid, debug_complete_valid, debug_dispatch_count,
  output logic [1:0] debug_rob_capacity, debug_iq_capacity,
  output logic [31:0] debug_issue_pc0, debug_issue_pc1, debug_complete_pc0, debug_complete_pc1
);
  tiny5_single_inorder dut (.*);
  assign debug_issue_valid = dut.core.issue_valid;
  assign debug_issue_pc0 = dut.core.issue_data[0].meta.pc;
  assign debug_issue_pc1 = dut.core.issue_data[1].meta.pc;
  assign debug_complete_valid = dut.core.wb_valid;
  assign debug_complete_pc0 = dut.core.wb_data[0].meta.pc;
  assign debug_complete_pc1 = dut.core.wb_data[1].meta.pc;
  assign debug_dispatch_count = dut.core.dispatch_count;
  assign debug_rob_capacity = dut.core.rob_capacity;
  assign debug_iq_capacity = dut.core.iq_capacity;
endmodule
