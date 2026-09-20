module if2id (
  input logic clk, reset, flush,
  input logic [1:0] push_count,
  input tiny5_pkg::fetch_t in_data [2],
  output logic [1:0] capacity,
  input logic [1:0] pop_count,
  output logic [1:0] available,
  output tiny5_pkg::fetch_t out_data [2]
);
  packet_queue #(.WIDTH($bits(tiny5_pkg::fetch_t)), .DEPTH(2)) storage (
    .clk, .reset, .flush, .push_count, .push_data(in_data),
    .push_capacity(capacity), .pop_count, .pop_available(available), .pop_data(out_data)
  );
endmodule
