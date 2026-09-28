module rename_control (
  input logic clk, reset,
  input logic [1:0] offer_count, dispatch_count,
  input tiny5_pkg::decode_t decoded [2],
  input tiny5_pkg::rob_tag_t allocate_tag [2],
  output logic [1:0] allow_count,
  output tiny5_pkg::issue_t prepared [2],
  output tiny5_pkg::phys_t read_address [4],
  input logic [31:0] read_value [4],
  input logic [1:0] complete_valid,
  input tiny5_pkg::result_t complete_data [2],
  input logic [1:0] retire_count,
  input tiny5_pkg::rob_entry_t retire_data [2]
);
  import tiny5_pkg::*;
  phys_t speculative_map [32], committed_map [32], candidate_map [32];
  logic [PHYS_REGS-1:0] ready_q, candidate_ready;
  phys_t free_register [2], release_register [2];
  logic [1:0] free_available, allocate_count, release_valid;
  integer used;
  logic blocked;
  issue_t mapped [2];
  free_list registers (
    .clk, .reset, .allocate_count, .available(free_available), .allocate_register(free_register),
    .release_valid, .release_register
  );
  always_comb begin
    for (int r = 0; r < 32; r++) candidate_map[r] = speculative_map[r];
    candidate_ready = ready_q;
    allow_count = 0;
    used = 0;
    blocked = 0;
    for (int p = 0; p < 2; p++) begin
      read_address[2*p] = candidate_map[decoded[p].rs1];
      read_address[2*p+1] = candidate_map[decoded[p].rs2];
      mapped[p] = '0;
      mapped[p].valid = 1;
      mapped[p].op = decoded[p].op;
      mapped[p].meta.pc = decoded[p].pc;
      mapped[p].meta.rd = decoded[p].rd;
      mapped[p].meta.tag = allocate_tag[p];
      mapped[p].a = '{ready:candidate_ready[read_address[2*p]], tag:read_address[2*p], value:32'b0};
      mapped[p].b = '{ready:candidate_ready[read_address[2*p+1]], tag:read_address[2*p+1], value:32'b0};
      if (decoded[p].immediate_b)
        mapped[p].b = '{ready:1'b1, tag:6'b0, value:32'b0};
      if (p < int'(offer_count) && !blocked) begin
        if (decoded[p].terminal) begin
          allow_count = allow_count + 1'b1;
          blocked = 1;
        end else if (decoded[p].rd != 0 && used >= int'(free_available)) blocked = 1;
        else begin
          allow_count = allow_count + 1'b1;
          if (decoded[p].rd != 0) begin
            mapped[p].meta.old_pdst = candidate_map[decoded[p].rd];
            mapped[p].meta.pdst = free_register[used];
            candidate_map[decoded[p].rd] = free_register[used];
            candidate_ready[free_register[used]] = 0;
            used++;
          end
        end
      end
    end
  end
  always_comb begin
    for (int p = 0; p < 2; p++) begin
      prepared[p] = mapped[p];
      prepared[p].a.value = read_value[2*p];
      prepared[p].b.value = decoded[p].immediate_b ? decoded[p].immediate : read_value[2*p+1];
    end
  end
  always_comb begin
    allocate_count = 0;
    release_valid = 0;
    for (int p = 0; p < 2; p++) begin
      release_register[p] = retire_data[p].meta.old_pdst;
      if (p < int'(dispatch_count) && !decoded[p].terminal && decoded[p].rd != 0)
        allocate_count = allocate_count + 1'b1;
      if (p < int'(retire_count) && retire_data[p].meta.rd != 0) release_valid[p] = 1;
    end
  end
  always_ff @(posedge clk) begin
    if (reset) begin
      ready_q <= '1;
      for (int r = 0; r < 32; r++) begin
        speculative_map[r] <= phys_t'(r);
        committed_map[r] <= phys_t'(r);
      end
    end else begin
      for (int p = 0; p < 2; p++) begin
        if (complete_valid[p] && complete_data[p].meta.rd != 0)
          ready_q[complete_data[p].meta.pdst] <= 1;
        if (p < int'(dispatch_count) && !decoded[p].terminal && decoded[p].rd != 0) begin
          speculative_map[decoded[p].rd] <= prepared[p].meta.pdst;
          ready_q[prepared[p].meta.pdst] <= 0;
        end
        if (p < int'(retire_count) && retire_data[p].meta.rd != 0)
          committed_map[retire_data[p].meta.rd] <= retire_data[p].meta.pdst;
      end
    end
  end
`ifndef SYNTHESIS
  always_ff @(posedge clk) if (!reset) begin
    assert (speculative_map[0] == 0 && committed_map[0] == 0 && ready_q[0]);
    for (int r = 1; r < 32; r++) begin
      assert (speculative_map[r] != 0 && committed_map[r] != 0);
      for (int s = r+1; s < 32; s++) begin
        assert (speculative_map[r] != speculative_map[s]);
        assert (committed_map[r] != committed_map[s]);
      end
    end
  end
`endif
endmodule
