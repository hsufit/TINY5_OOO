// Ordered two-port queue. Transfers are counts, always a prefix of the ports.
// Space released on this edge becomes available on the following cycle.
module packet_queue #(parameter int WIDTH = 32, DEPTH = 8) (
  input logic clk, reset, flush,
  input logic [1:0] push_count,
  input logic [WIDTH-1:0] push_data [2],
  output logic [1:0] push_capacity,
  input logic [1:0] pop_count,
  output logic [1:0] pop_available,
  output logic [WIDTH-1:0] pop_data [2]
);
  logic [WIDTH-1:0] data_q [DEPTH], data_d [DEPTH];
  integer count_q, count_d;
  always_comb begin
    push_capacity = 2'((DEPTH-count_q >= 2) ? 2 : DEPTH-count_q);
    pop_available = 2'((count_q >= 2) ? 2 : count_q);
    pop_data[0] = data_q[0];
    pop_data[1] = data_q[1];
    for (int i = 0; i < DEPTH; i++) begin
      data_d[i] = '0;
      if (i + int'(pop_count) < count_q)
        data_d[i] = data_q[i + int'(pop_count)];
    end
    for (int p = 0; p < 2; p++)
      if (p < int'(push_count)) data_d[count_q-int'(pop_count)+p] = push_data[p];
    count_d = count_q + int'(push_count) - int'(pop_count);
  end
  always_ff @(posedge clk) begin
    if (reset || flush) begin
      count_q <= 0;
      for (int i = 0; i < DEPTH; i++) data_q[i] <= '0;
    end else begin
      count_q <= count_d;
      for (int i = 0; i < DEPTH; i++) data_q[i] <= data_d[i];
    end
  end
`ifndef SYNTHESIS
  always_ff @(posedge clk) if (!reset && !flush) begin
    assert (push_count <= push_capacity);
    assert (pop_count <= pop_available);
    assert (count_q >= 0 && count_q <= DEPTH);
  end
`endif
endmodule
