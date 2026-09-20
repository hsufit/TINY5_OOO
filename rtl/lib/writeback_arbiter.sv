module writeback_arbiter (
  input logic clk, reset, flush,
  input logic [2:0] in_valid,
  output logic [2:0] in_ready,
  input tiny5_pkg::result_t in_data [3],
  output logic [1:0] out_valid,
  input logic [1:0] out_ready,
  output tiny5_pkg::result_t out_data [2]
);
  integer next_q, next_d;
  logic [2:0] used;
  integer selected, candidate;
  always_comb begin
    next_d = next_q;
    used = 0;
    in_ready = 0;
    out_valid = 0;
    selected = -1;
    candidate = 0;
    for (int p = 0; p < 2; p++) begin
      out_data[p] = '0;
      selected = -1;
      for (int offset = 0; offset < 3; offset++) begin
        candidate = (next_q + offset) % 3;
        if (in_valid[candidate] && !used[candidate] && selected == -1)
          selected = candidate;
      end
      // The receiving elastic register supplies storage; only grant ready ports.
      if (selected != -1 && out_ready[p]) begin
        out_valid[p] = 1;
        out_data[p] = in_data[selected];
        in_ready[selected] = 1;
        used[selected] = 1;
        next_d = (selected + 1) % 3;
      end
    end
  end
  always_ff @(posedge clk)
    if (reset || flush) next_q <= 0;
    else next_q <= next_d;
endmodule
