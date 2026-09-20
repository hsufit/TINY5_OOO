module issue2ex (
  input logic clk, reset, flush, in_valid,
  output logic in_ready,
  input tiny5_pkg::execute_t in_data,
  output logic out_valid,
  input logic out_ready,
  output tiny5_pkg::execute_t out_data
);
  elastic_reg #(.WIDTH($bits(tiny5_pkg::execute_t))) storage (.*);
endmodule
