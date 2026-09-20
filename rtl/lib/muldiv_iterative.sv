module muldiv_iterative (
  input logic clk, reset, flush, in_valid,
  output logic in_ready,
  input tiny5_pkg::execute_t in_data,
  output logic out_valid,
  input logic out_ready,
  output tiny5_pkg::result_t out_data
);
  import tiny5_pkg::*;
  logic busy, negative_a, negative_b;
  logic [5:0] step;
  execute_t request;
  logic [63:0] product, multiplicand, corrected_product;
  logic [31:0] multiplier, quotient, divisor;
  logic [32:0] remainder, trial_remainder;
  logic [31:0] magnitude_a, magnitude_b, corrected_quotient, corrected_remainder;
  logic signed_a, signed_b;
  logic [31:0] final_value;
  assign in_ready = !busy && (!out_valid || out_ready);
  always_comb begin
    signed_a = in_data.a[31] && (in_data.op == OP_MULH || in_data.op == OP_MULHSU ||
                               in_data.op == OP_DIV || in_data.op == OP_REM);
    signed_b = in_data.b[31] && (in_data.op == OP_MULH ||
                               in_data.op == OP_DIV || in_data.op == OP_REM);
    magnitude_a = signed_a ? (~in_data.a + 32'd1) : in_data.a;
    magnitude_b = signed_b ? (~in_data.b + 32'd1) : in_data.b;
    trial_remainder = {remainder[31:0], quotient[31]};
    corrected_product = (negative_a ^ negative_b) ? (~product + 64'd1) : product;
    corrected_quotient = (negative_a ^ negative_b) ? (~quotient + 32'd1) : quotient;
    corrected_remainder = negative_a ? (~remainder[31:0] + 32'd1) : remainder[31:0];
    case (request.op)
      OP_MUL: final_value = corrected_product[31:0];
      OP_MULH, OP_MULHSU, OP_MULHU: final_value = corrected_product[63:32];
      OP_DIV, OP_DIVU: final_value = request.b == 0 ? 32'hffffffff : corrected_quotient;
      default: final_value = request.b == 0 ? request.a : corrected_remainder;
    endcase
    if (request.meta.rd == 0) final_value = 0;
  end
  always_ff @(posedge clk) begin
    if (reset || flush) begin
      busy <= 0;
      out_valid <= 0;
      out_data <= '0;
      request <= '0;
      step <= 0;
      negative_a <= 0;
      negative_b <= 0;
      product <= 0;
      multiplicand <= 0;
      multiplier <= 0;
      quotient <= 0;
      divisor <= 0;
      remainder <= 0;
    end else begin
      if (out_ready) out_valid <= 0;
      if (in_valid && in_ready) begin
        busy <= 1;
        request <= in_data;
        step <= 0;
        negative_a <= signed_a;
        negative_b <= signed_b;
        product <= 0;
        multiplicand <= {32'b0, magnitude_a};
        multiplier <= magnitude_b;
        quotient <= magnitude_a;
        divisor <= magnitude_b;
        remainder <= 0;
      end else if (busy) begin
        if (step < 32) begin
          step <= step + 1'b1;
          if (request.op <= OP_MULHU) begin
            if (multiplier[0]) product <= product + multiplicand;
            multiplicand <= multiplicand << 1;
            multiplier <= multiplier >> 1;
          end else begin
            if (trial_remainder >= {1'b0, divisor}) begin
              remainder <= trial_remainder - {1'b0, divisor};
              quotient <= {quotient[30:0], 1'b1};
            end else begin
              remainder <= trial_remainder;
              quotient <= {quotient[30:0], 1'b0};
            end
          end
        end else begin
          busy <= 0;
          out_valid <= 1;
          out_data <= '{meta:request.meta, value:final_value};
        end
      end
    end
  end
`ifndef SYNTHESIS
  assert property (@(posedge clk) disable iff (reset || flush)
    out_valid && !out_ready |=> reset || flush || (out_valid && $stable(out_data)));
`endif
endmodule
