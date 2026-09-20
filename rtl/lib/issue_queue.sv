module issue_queue (
  input logic clk, reset, flush,
  input logic [1:0] enqueue_count,
  input tiny5_pkg::issue_t enqueue_data [2],
  output logic [1:0] capacity,
  input logic [tiny5_pkg::IQ_DEPTH-1:0] remove_mask,
  input logic [1:0] complete_valid,
  input tiny5_pkg::result_t complete_data [2],
  output tiny5_pkg::issue_t entries [tiny5_pkg::IQ_DEPTH]
);
  import tiny5_pkg::*;
  issue_t next_entries [IQ_DEPTH];
  integer free_count, selected;
  always_comb begin
    free_count = 0;
    selected = -1;
    for (int i = 0; i < IQ_DEPTH; i++) begin
      if (!entries[i].valid) free_count++;
      next_entries[i] = entries[i];
      if (remove_mask[i]) next_entries[i].valid = 0;
    end
    capacity = 2'((free_count >= 2) ? 2 : free_count);
    for (int p = 0; p < 2; p++) begin
      selected = -1;
      for (int i = 0; i < IQ_DEPTH; i++)
        if (!next_entries[i].valid && selected == -1) selected = i;
      if (p < int'(enqueue_count) && selected != -1)
        next_entries[selected] = enqueue_data[p];
    end
    // Wake newly enqueued instructions too: dispatch may coincide with writeback.
    for (int i = 0; i < IQ_DEPTH; i++) begin
      for (int p = 0; p < 2; p++) begin
        if (next_entries[i].valid && complete_valid[p] && complete_data[p].meta.rd != 0) begin
          if (!next_entries[i].a.ready && next_entries[i].a.tag == complete_data[p].meta.pdst) begin
            next_entries[i].a.ready = 1;
            next_entries[i].a.value = complete_data[p].value;
          end
          if (!next_entries[i].b.ready && next_entries[i].b.tag == complete_data[p].meta.pdst) begin
            next_entries[i].b.ready = 1;
            next_entries[i].b.value = complete_data[p].value;
          end
        end
      end
    end
  end
  always_ff @(posedge clk) begin
    for (int i = 0; i < IQ_DEPTH; i++) begin
      if (reset || flush) entries[i] <= '0;
      else entries[i] <= next_entries[i];
    end
  end
`ifndef SYNTHESIS
  always_ff @(posedge clk) if (!reset && !flush) begin
    assert (enqueue_count <= capacity);
    for (int i = 0; i < IQ_DEPTH; i++) begin
      if (remove_mask[i]) assert (entries[i].valid && entries[i].a.ready && entries[i].b.ready);
      for (int j = i+1; j < IQ_DEPTH; j++)
        if (entries[i].valid && entries[j].valid) assert (entries[i].meta.tag != entries[j].meta.tag);
    end
  end
`endif
endmodule
