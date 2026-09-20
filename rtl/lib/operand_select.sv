module operand_select (
  input logic zero_operand, immediate_operand,
  input logic [31:0] immediate_value, register_value,
  input logic producer_present, producer_ready,
  input logic [31:0] producer_value,
  input tiny5_pkg::phys_t source_tag,
  output tiny5_pkg::operand_t operand
);
  always_comb begin
    operand.tag = source_tag;
    operand.ready = !producer_present || producer_ready;
    operand.value = producer_present ? producer_value : register_value;
    if (zero_operand || immediate_operand) begin
      operand.ready = 1;
      operand.tag = 0;
      operand.value = immediate_operand ? immediate_value : 32'b0;
    end
  end
endmodule
