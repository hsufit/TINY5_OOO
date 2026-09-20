module elastic_reg #(parameter int WIDTH = 32) (
  input logic clk, reset, flush,
  input logic in_valid,
  output logic in_ready,
  input logic [WIDTH-1:0] in_data,
  output logic out_valid,
  input logic out_ready,
  output logic [WIDTH-1:0] out_data
);
  assign in_ready = !out_valid || out_ready;
  always_ff @(posedge clk) begin
    if (reset || flush) begin
      out_valid <= 1'b0;
      out_data <= '0;
    end else if (in_ready) begin
      out_valid <= in_valid;
      if (in_valid) out_data <= in_data;
    end
  end
`ifndef SYNTHESIS
  assert property (@(posedge clk) disable iff (reset || flush)
    out_valid && !out_ready |=> reset || flush || (out_valid && $stable(out_data)));
`endif
endmodule
