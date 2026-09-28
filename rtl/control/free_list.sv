module free_list #(parameter int REG_COUNT = tiny5_pkg::PHYS_REGS) (
  input logic clk, reset,
  input logic [1:0] allocate_count,
  output logic [1:0] available,
  output tiny5_pkg::phys_t allocate_register [2],
  input logic [1:0] release_valid,
  input tiny5_pkg::phys_t release_register [2]
);
  logic [REG_COUNT-1:0] free_q;
  integer found;
  always_comb begin
    found = 0;
    allocate_register[0] = 0;
    allocate_register[1] = 0;
    for (int r = 1; r < REG_COUNT; r++) begin
      if (free_q[r] && found < 2) begin
        allocate_register[found] = tiny5_pkg::phys_t'(r);
        found++;
      end
    end
    available = 2'(found);
  end
  always_ff @(posedge clk) begin
    if (reset) begin
      for (int r = 0; r < REG_COUNT; r++) free_q[r] <= r >= 32;
    end else begin
      for (int p = 0; p < 2; p++) begin
        if (release_valid[p] && release_register[p] != 0) free_q[release_register[p]] <= 1;
        if (p < int'(allocate_count)) free_q[allocate_register[p]] <= 0;
      end
    end
  end
`ifndef SYNTHESIS
  always_ff @(posedge clk) if (!reset) begin
    assert (allocate_count <= available);
    assert (!free_q[0]);
    for (int p = 0; p < 2; p++)
      if (release_valid[p]) assert (release_register[p] != 0 && !free_q[release_register[p]]);
    if (&release_valid) assert (release_register[0] != release_register[1]);
  end
`endif
endmodule
