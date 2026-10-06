module integer_alu (
  input logic clk, reset, flush, in_valid,
  output logic in_ready,
  input tiny5_pkg::execute_t in_data,
  output logic out_valid,
  input logic out_ready,
  output tiny5_pkg::result_t out_data
);
  import tiny5_pkg::*;
  result_t result;
  always_comb begin
    result = '0;
    result.meta = in_data.meta;
    case (in_data.op)
      OP_ADD: result.value = in_data.a + in_data.b;
      OP_SUB: result.value = in_data.a - in_data.b;
      OP_SLL: result.value = in_data.a << in_data.b[4:0];
      OP_SLT: result.value = {31'b0, $signed(in_data.a) < $signed(in_data.b)};
      OP_SLTU: result.value = {31'b0, in_data.a < in_data.b};
      OP_XOR: result.value = in_data.a ^ in_data.b;
      OP_SRL: result.value = in_data.a >> in_data.b[4:0];
      OP_SRA: result.value = $signed(in_data.a) >>> in_data.b[4:0];
      OP_OR: result.value = in_data.a | in_data.b;
      OP_AND: result.value = in_data.a & in_data.b;
      OP_BEQ: result.branch.taken = in_data.a == in_data.b;
      OP_BNE: result.branch.taken = in_data.a != in_data.b;
      OP_BLT: result.branch.taken = $signed(in_data.a) < $signed(in_data.b);
      OP_BGE: result.branch.taken = $signed(in_data.a) >= $signed(in_data.b);
      OP_BLTU: result.branch.taken = in_data.a < in_data.b;
      OP_BGEU: result.branch.taken = in_data.a >= in_data.b;
      default: result.value = 0;
    endcase
    if (is_branch(in_data.op)) begin
      result.branch.valid = 1;
      result.branch.target = in_data.meta.pc + in_data.branch_offset;
      if (result.branch.taken && result.branch.target[1:0] != 0)
        result.branch.fault = FAULT_MISALIGNED;
    end
    if (in_data.meta.rd == 0) result.value = 0;
  end
  elastic_reg #(.WIDTH($bits(result_t))) storage (
    .clk, .reset, .flush, .in_valid, .in_ready, .in_data(result),
    .out_valid, .out_ready, .out_data
  );
endmodule
