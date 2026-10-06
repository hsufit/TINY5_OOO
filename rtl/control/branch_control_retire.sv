// Option 4: execute the sequential path, recover only at ordered retirement.
module branch_control_retire (
  input logic clk, reset, finish,
  input tiny5_pkg::decode_t decoded [2],
  input logic [1:0] dispatch_count,
  input tiny5_pkg::rob_tag_t allocate_tag [2],
  input logic [1:0] complete_valid,
  input tiny5_pkg::result_t complete_data [2],
  input tiny5_pkg::rob_entry_t head_data [2],
  input logic [1:0] retire_count,
  output logic [1:0] dispatch_limit, retire_limit,
  output logic redirect_valid,
  output logic [31:0] redirect_pc,
  output logic front_flush, backend_flush, restore
);
  assign dispatch_limit = 2;
  // A taken branch retires alone. In lane 1, defer it until the next cycle.
  assign retire_limit = (head_data[0].branch.valid && head_data[0].branch.taken) ||
                        (head_data[1].valid && head_data[1].branch.valid &&
                         head_data[1].branch.taken) ? 2'd1 : 2'd2;
  assign redirect_valid = !reset && !finish && retire_count != 0 &&
    head_data[0].branch.valid && head_data[0].branch.taken && !head_data[0].terminal;
  assign redirect_pc = head_data[0].branch.target;
  assign front_flush = redirect_valid;
  assign backend_flush = redirect_valid;
  assign restore = redirect_valid;
`ifndef SYNTHESIS
  always_ff @(posedge clk) if (!reset && restore) begin
    assert (retire_count == 1 && head_data[0].meta.rd == 0);
    assert (dispatch_count == 0 && complete_valid == 0);
  end
`endif
endmodule
