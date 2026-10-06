// Option 2: sequential prefetch, with no younger instruction past dispatch.
module branch_control_blocking (
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
  import tiny5_pkg::*;
  logic pending;
  rob_tag_t pending_tag;
  assign dispatch_limit = pending ? 2'd0 : is_branch(decoded[0].op) ? 2'd1 : 2'd2;
  assign retire_limit = 2;
  assign backend_flush = 0;
  assign restore = 0;
  assign front_flush = redirect_valid;
  always_comb begin
    redirect_valid = 0;
    redirect_pc = 0;
    for (int p = 0; p < 2; p++) begin
      if (!reset && !finish && pending && complete_valid[p] &&
          complete_data[p].meta.tag == pending_tag && complete_data[p].branch.valid &&
          complete_data[p].branch.fault == FAULT_NONE && complete_data[p].branch.taken) begin
        redirect_valid = 1;
        redirect_pc = complete_data[p].branch.target;
      end
    end
  end
  always_ff @(posedge clk) begin
    if (reset || finish) begin
      pending <= 0;
      pending_tag <= 0;
    end else begin
      for (int p = 0; p < 2; p++) begin
        if (p < int'(dispatch_count) && is_branch(decoded[p].op)) begin
          pending <= 1;
          pending_tag <= allocate_tag[p];
        end
        if (pending && complete_valid[p] && complete_data[p].meta.tag == pending_tag &&
            complete_data[p].branch.valid && complete_data[p].branch.fault == FAULT_NONE)
          pending <= 0;
      end
    end
  end
`ifndef SYNTHESIS
  always_ff @(posedge clk) if (!reset && !finish) begin
    if (pending) assert (dispatch_count == 0);
    if (dispatch_count == 2) assert (!is_branch(decoded[0].op));
  end
`endif
endmodule
