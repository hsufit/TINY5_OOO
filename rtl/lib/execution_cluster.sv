module execution_cluster #(parameter int ALU_COUNT = 2) (
  input logic clk, reset, flush,
  input logic [1:0] in_valid,
  output logic [1:0] in_ready,
  input tiny5_pkg::execute_t in_data [2],
  output logic [1:0] out_valid,
  input logic [1:0] out_ready,
  output tiny5_pkg::result_t out_data [2],
  output logic muldiv_available
);
  import tiny5_pkg::*;
  logic [2:0] unit_valid, unit_ready, result_valid, result_ready, used;
  execute_t unit_data [3];
  result_t results [3];
  integer selected;
  always_comb begin
    unit_valid = 0;
    in_ready = 0;
    used = 0;
    selected = -1;
    for (int u = 0; u < 3; u++) unit_data[u] = '0;
    for (int p = 0; p < 2; p++) begin
      selected = -1;
      if (is_muldiv(in_data[p].op)) begin
        if (unit_ready[2] && !used[2]) selected = 2;
      end else begin
        for (int u = 0; u < ALU_COUNT; u++)
          if (unit_ready[u] && !used[u] && selected == -1) selected = u;
      end
      if (selected != -1) begin
        in_ready[p] = 1;
        if (in_valid[p]) begin
          used[selected] = 1;
          unit_valid[selected] = 1;
          unit_data[selected] = in_data[p];
        end
      end
    end
  end
  for (genvar u = 0; u < 2; u++) begin : alus
    if (u < ALU_COUNT) begin : enabled
      integer_alu alu (
        .clk, .reset, .flush, .in_valid(unit_valid[u]), .in_ready(unit_ready[u]),
        .in_data(unit_data[u]), .out_valid(result_valid[u]),
        .out_ready(result_ready[u]), .out_data(results[u])
      );
    end else begin : disabled
      assign unit_ready[u] = 0;
      assign result_valid[u] = 0;
      assign results[u] = '0;
    end
  end
  muldiv_iterative muldiv (
    .clk, .reset, .flush, .in_valid(unit_valid[2]), .in_ready(unit_ready[2]),
    .in_data(unit_data[2]), .out_valid(result_valid[2]),
    .out_ready(result_ready[2]), .out_data(results[2])
  );
  assign muldiv_available = unit_ready[2];
  writeback_arbiter arbiter (
    .clk, .reset, .flush, .in_valid(result_valid), .in_ready(result_ready),
    .in_data(results), .out_valid, .out_ready, .out_data
  );
endmodule
