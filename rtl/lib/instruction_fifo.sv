module instruction_fifo (
  input logic clk, reset, flush,
  input logic in_valid,
  output logic in_ready,
  input tiny5_pkg::fetch_t in_data,
  input logic [1:0] pop_count,
  output logic [1:0] available,
  output tiny5_pkg::fetch_t out_data [2]
);
  logic [1:0] capacity;
  tiny5_pkg::fetch_t pushes [2];
  assign pushes[0] = in_data;
  assign pushes[1] = '0;
  assign in_ready = capacity != 0;
  packet_queue #(.WIDTH($bits(tiny5_pkg::fetch_t)), .DEPTH(8)) storage (
    .clk, .reset, .flush, .push_count({1'b0, in_valid && in_ready}),
    .push_data(pushes), .push_capacity(capacity), .pop_count,
    .pop_available(available), .pop_data(out_data)
  );
endmodule
