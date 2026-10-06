module core_test_wrapper #(
  parameter bit DUAL_INORDER = 0, DUAL_OOO = 0, OOO_RETIRE = 0
) (
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
  output logic [31:0] debug_issue_pc0, debug_issue_pc1, debug_complete_pc0, debug_complete_pc1,
  output logic [31:0] debug_dispatch_pc0, debug_dispatch_pc1,
  output logic [31:0] debug_dispatch_tag0, debug_dispatch_tag1, debug_issue_tag0, debug_issue_tag1,
  output logic [1:0] debug_dispatch_branch, debug_complete_branch, debug_dispatch_limit,
  output logic debug_redirect, debug_backend_flush, debug_cancel_muldiv, debug_frontend_full
);
  localparam int MODE = OOO_RETIRE ? 3 : DUAL_OOO ? 2 : DUAL_INORDER ? 1 : 0;
  if (MODE == 0) begin : single
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
    assign debug_dispatch_pc0 = dut.core.dispatch_data[0].pc;
    assign debug_dispatch_pc1 = dut.core.dispatch_data[1].pc;
    assign debug_dispatch_tag0 = {28'b0, dut.core.allocate_tag[0]};
    assign debug_dispatch_tag1 = {28'b0, dut.core.allocate_tag[1]};
    assign debug_issue_tag0 = {28'b0, dut.core.issue_data[0].meta.tag};
    assign debug_issue_tag1 = {28'b0, dut.core.issue_data[1].meta.tag};
    assign debug_dispatch_branch = {
      dut.core.dispatch_count > 1 && tiny5_pkg::is_branch(dut.core.dispatch_data[1].op),
      dut.core.dispatch_count > 0 && tiny5_pkg::is_branch(dut.core.dispatch_data[0].op)};
    assign debug_complete_branch = {
      dut.core.wb_valid[1] && dut.core.wb_data[1].branch.valid,
      dut.core.wb_valid[0] && dut.core.wb_data[0].branch.valid};
    assign debug_dispatch_limit = dut.core.dispatch_limit;
    assign debug_redirect = dut.core.redirect_valid;
    assign debug_backend_flush = dut.core.branch_backend_flush;
    assign debug_cancel_muldiv = dut.core.branch_backend_flush && dut.core.execution.muldiv.busy;
    assign debug_frontend_full = !dut.core.fetched_ready;
  end
  else if (MODE == 1) begin : dual
    tiny5_dual_inorder dut (.*);
    assign debug_issue_valid = dut.core.issue_valid;
    assign debug_issue_pc0 = dut.core.issue_data[0].meta.pc;
    assign debug_issue_pc1 = dut.core.issue_data[1].meta.pc;
    assign debug_complete_valid = dut.core.wb_valid;
    assign debug_complete_pc0 = dut.core.wb_data[0].meta.pc;
    assign debug_complete_pc1 = dut.core.wb_data[1].meta.pc;
    assign debug_dispatch_count = dut.core.dispatch_count;
    assign debug_rob_capacity = dut.core.rob_capacity;
    assign debug_iq_capacity = dut.core.iq_capacity;
    assign debug_dispatch_pc0 = dut.core.dispatch_data[0].pc;
    assign debug_dispatch_pc1 = dut.core.dispatch_data[1].pc;
    assign debug_dispatch_tag0 = {28'b0, dut.core.allocate_tag[0]};
    assign debug_dispatch_tag1 = {28'b0, dut.core.allocate_tag[1]};
    assign debug_issue_tag0 = {28'b0, dut.core.issue_data[0].meta.tag};
    assign debug_issue_tag1 = {28'b0, dut.core.issue_data[1].meta.tag};
    assign debug_dispatch_branch = {
      dut.core.dispatch_count > 1 && tiny5_pkg::is_branch(dut.core.dispatch_data[1].op),
      dut.core.dispatch_count > 0 && tiny5_pkg::is_branch(dut.core.dispatch_data[0].op)};
    assign debug_complete_branch = {
      dut.core.wb_valid[1] && dut.core.wb_data[1].branch.valid,
      dut.core.wb_valid[0] && dut.core.wb_data[0].branch.valid};
    assign debug_dispatch_limit = dut.core.dispatch_limit;
    assign debug_redirect = dut.core.redirect_valid;
    assign debug_backend_flush = dut.core.branch_backend_flush;
    assign debug_cancel_muldiv = dut.core.branch_backend_flush && dut.core.execution.muldiv.busy;
    assign debug_frontend_full = !dut.core.fetched_ready;
  end
  else if (MODE == 2) begin : ooo
    tiny5_dual_ooo dut (.*);
    assign debug_issue_valid = dut.core.issue_valid;
    assign debug_issue_pc0 = dut.core.issue_data[0].meta.pc;
    assign debug_issue_pc1 = dut.core.issue_data[1].meta.pc;
    assign debug_complete_valid = dut.core.wb_valid;
    assign debug_complete_pc0 = dut.core.wb_data[0].meta.pc;
    assign debug_complete_pc1 = dut.core.wb_data[1].meta.pc;
    assign debug_dispatch_count = dut.core.dispatch_count;
    assign debug_rob_capacity = dut.core.rob_capacity;
    assign debug_iq_capacity = dut.core.iq_capacity;
    assign debug_dispatch_pc0 = dut.core.dispatch_data[0].pc;
    assign debug_dispatch_pc1 = dut.core.dispatch_data[1].pc;
    assign debug_dispatch_tag0 = {28'b0, dut.core.allocate_tag[0]};
    assign debug_dispatch_tag1 = {28'b0, dut.core.allocate_tag[1]};
    assign debug_issue_tag0 = {28'b0, dut.core.issue_data[0].meta.tag};
    assign debug_issue_tag1 = {28'b0, dut.core.issue_data[1].meta.tag};
    assign debug_dispatch_branch = {
      dut.core.dispatch_count > 1 && tiny5_pkg::is_branch(dut.core.dispatch_data[1].op),
      dut.core.dispatch_count > 0 && tiny5_pkg::is_branch(dut.core.dispatch_data[0].op)};
    assign debug_complete_branch = {
      dut.core.wb_valid[1] && dut.core.wb_data[1].branch.valid,
      dut.core.wb_valid[0] && dut.core.wb_data[0].branch.valid};
    assign debug_dispatch_limit = dut.core.dispatch_limit;
    assign debug_redirect = dut.core.redirect_valid;
    assign debug_backend_flush = dut.core.branch_backend_flush;
    assign debug_cancel_muldiv = dut.core.branch_backend_flush && dut.core.execution.muldiv.busy;
    assign debug_frontend_full = !dut.core.fetched_ready;
  end  else if (MODE == 3) begin : ooo_retire
    tiny5_dual_ooo_retire dut (.*);
    assign debug_issue_valid = dut.core.issue_valid;
    assign debug_issue_pc0 = dut.core.issue_data[0].meta.pc;
    assign debug_issue_pc1 = dut.core.issue_data[1].meta.pc;
    assign debug_complete_valid = dut.core.wb_valid;
    assign debug_complete_pc0 = dut.core.wb_data[0].meta.pc;
    assign debug_complete_pc1 = dut.core.wb_data[1].meta.pc;
    assign debug_dispatch_count = dut.core.dispatch_count;
    assign debug_rob_capacity = dut.core.rob_capacity;
    assign debug_iq_capacity = dut.core.iq_capacity;
    assign debug_dispatch_pc0 = dut.core.dispatch_data[0].pc;
    assign debug_dispatch_pc1 = dut.core.dispatch_data[1].pc;
    assign debug_dispatch_tag0 = {28'b0, dut.core.allocate_tag[0]};
    assign debug_dispatch_tag1 = {28'b0, dut.core.allocate_tag[1]};
    assign debug_issue_tag0 = {28'b0, dut.core.issue_data[0].meta.tag};
    assign debug_issue_tag1 = {28'b0, dut.core.issue_data[1].meta.tag};
    assign debug_dispatch_branch = {
      dut.core.dispatch_count > 1 && tiny5_pkg::is_branch(dut.core.dispatch_data[1].op),
      dut.core.dispatch_count > 0 && tiny5_pkg::is_branch(dut.core.dispatch_data[0].op)};
    assign debug_complete_branch = {
      dut.core.wb_valid[1] && dut.core.wb_data[1].branch.valid,
      dut.core.wb_valid[0] && dut.core.wb_data[0].branch.valid};
    assign debug_dispatch_limit = dut.core.dispatch_limit;
    assign debug_redirect = dut.core.redirect_valid;
    assign debug_backend_flush = dut.core.branch_backend_flush;
    assign debug_cancel_muldiv = dut.core.branch_backend_flush && dut.core.execution.muldiv.busy;
    assign debug_frontend_full = !dut.core.fetched_ready;
  end
endmodule
