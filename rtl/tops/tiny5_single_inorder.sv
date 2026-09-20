module tiny5_single_inorder (
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
  output logic [1:0] fault_code
);
  core_pipeline #(.ISSUE_WIDTH(1), .OUT_OF_ORDER(0)) core (.*);
endmodule
