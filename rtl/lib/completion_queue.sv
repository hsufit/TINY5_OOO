module completion_queue (
  input logic clk, reset, flush,
  input logic [1:0] allocate_count,
  input tiny5_pkg::rob_entry_t allocate_data [2],
  output logic [1:0] capacity,
  output tiny5_pkg::rob_tag_t allocate_tag [2],
  input logic [1:0] complete_valid,
  input tiny5_pkg::result_t complete_data [2],
  input logic [1:0] retire_count,
  output tiny5_pkg::rob_entry_t head_data [2],
  output tiny5_pkg::rob_tag_t head_tag,
  input tiny5_pkg::rob_tag_t lookup_tag [4],
  output tiny5_pkg::rob_entry_t lookup_data [4]
);
  import tiny5_pkg::*;
  rob_entry_t entries [ROB_DEPTH];
  rob_tag_t head, tail;
  integer count;
  assign capacity = 2'((ROB_DEPTH-count >= 2) ? 2 : ROB_DEPTH-count);
  assign allocate_tag[0] = tail;
  assign allocate_tag[1] = tail + 1'b1;
  assign head_tag = head;
  always_comb begin
    head_data[0] = entries[head];
    head_data[1] = entries[rob_tag_t'(head + 1'b1)];
    head_data[0].valid = count > 0;
    head_data[1].valid = count > 1;
    for (int p = 0; p < 4; p++) lookup_data[p] = entries[lookup_tag[p]];
  end
  always_ff @(posedge clk) begin
    if (reset || flush) begin
      head <= 0;
      tail <= 0;
      count <= 0;
      for (int i = 0; i < ROB_DEPTH; i++) entries[i] <= '0;
    end else begin
      for (int p = 0; p < 2; p++) begin
        if (complete_valid[p]) begin
          entries[complete_data[p].meta.tag].done <= 1;
          entries[complete_data[p].meta.tag].value <= complete_data[p].value;
        end
        if (p < int'(retire_count)) entries[rob_tag_t'(head + rob_tag_t'(p))].valid <= 0;
        if (p < int'(allocate_count)) entries[allocate_tag[p]] <= allocate_data[p];
      end
      head <= head + rob_tag_t'(retire_count);
      tail <= tail + rob_tag_t'(allocate_count);
      count <= count + int'(allocate_count) - int'(retire_count);
    end
  end
`ifndef SYNTHESIS
  always_ff @(posedge clk) if (!reset && !flush) begin
    assert (count >= 0 && count <= ROB_DEPTH);
    assert (allocate_count <= capacity);
    for (int p = 0; p < 2; p++) begin
      if (complete_valid[p]) begin
        assert (entries[complete_data[p].meta.tag].valid);
        assert (!entries[complete_data[p].meta.tag].done);
        assert (entries[complete_data[p].meta.tag].meta == complete_data[p].meta);
      end
      if (p < int'(retire_count)) assert (head_data[p].valid && head_data[p].done);
    end
    if (&complete_valid) assert (complete_data[0].meta.tag != complete_data[1].meta.tag);
  end
`endif
endmodule
