module register_file #(parameter int WORDS = 64) (
  input logic clk, reset,
  input tiny5_pkg::phys_t read_address [4],
  output logic [31:0] read_value [4],
  input logic [1:0] write_valid,
  input tiny5_pkg::phys_t write_address [2],
  input logic [31:0] write_value [2]
);
  logic [31:0] registers [WORDS];
  for (genvar p = 0; p < 4; p++) begin : reads
    assign read_value[p] = read_address[p] == 0 ? 32'b0 :
                          registers[$clog2(WORDS)'(read_address[p])];
  end
  always_ff @(posedge clk) begin
    if (reset) begin
      for (int r = 0; r < WORDS; r++) registers[r] <= 0;
    end else begin
      // Ordered ports: the younger retirement wins when destinations coincide.
      for (int p = 0; p < 2; p++)
        if (write_valid[p] && write_address[p] != 0)
          registers[$clog2(WORDS)'(write_address[p])] <= write_value[p];
      registers[0] <= 0;
    end
  end
`ifndef SYNTHESIS
  always_ff @(posedge clk) if (!reset) begin
    for (int p = 0; p < 4; p++) assert (int'(read_address[p]) < WORDS);
    for (int p = 0; p < 2; p++)
      if (write_valid[p]) assert (int'(write_address[p]) < WORDS);
  end
`endif
endmodule
