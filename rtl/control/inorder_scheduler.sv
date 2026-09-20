module inorder_scheduler #(parameter int ISSUE_WIDTH = 2) (
  input tiny5_pkg::issue_t entries [tiny5_pkg::IQ_DEPTH],
  input tiny5_pkg::rob_tag_t head_tag,
  input logic [1:0] out_ready,
  input logic muldiv_available,
  output logic [1:0] out_valid,
  output tiny5_pkg::execute_t out_data [2],
  output logic [tiny5_pkg::IQ_DEPTH-1:0] remove_mask
);
  import tiny5_pkg::*;
  integer oldest, age, best_age;
  logic blocked, md_used;
  always_comb begin
    remove_mask = 0;
    out_valid = 0;
    blocked = 0;
    md_used = 0;
    oldest = -1;
    age = 0;
    best_age = ROB_DEPTH;
    for (int p = 0; p < 2; p++) begin
      out_data[p] = '0;
      oldest = -1;
      best_age = ROB_DEPTH;
      for (int i = 0; i < IQ_DEPTH; i++) begin
        age = int'(rob_tag_t'(entries[i].meta.tag - head_tag));
        if (entries[i].valid && !remove_mask[i] && age < best_age) begin
          oldest = i;
          best_age = age;
        end
      end
      if (p < ISSUE_WIDTH && out_ready[p] && !blocked && oldest != -1) begin
        if (!entries[oldest].a.ready || !entries[oldest].b.ready ||
            (is_muldiv(entries[oldest].op) && (!muldiv_available || md_used))) blocked = 1;
        else begin
          out_valid[p] = 1;
          remove_mask[oldest] = 1;
          out_data[p] = '{meta:entries[oldest].meta, op:entries[oldest].op,
                         a:entries[oldest].a.value, b:entries[oldest].b.value};
          if (is_muldiv(entries[oldest].op)) md_used = 1;
        end
      end
    end
  end
endmodule
