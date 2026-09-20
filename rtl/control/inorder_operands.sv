module inorder_operands (
  input logic clk, reset, flush,
  input logic [1:0] offer_count, dispatch_count,
  input tiny5_pkg::decode_t decoded [2],
  input tiny5_pkg::rob_tag_t allocate_tag [2],
  output logic [1:0] allow_count,
  output tiny5_pkg::issue_t prepared [2],
  output tiny5_pkg::phys_t read_address [4],
  input logic [31:0] read_value [4],
  output tiny5_pkg::rob_tag_t lookup_tag [4],
  input tiny5_pkg::rob_entry_t lookup_data [4],
  input logic [1:0] retire_count,
  input tiny5_pkg::rob_entry_t retire_data [2]
);
  import tiny5_pkg::*;
  logic [31:0] busy, candidate_busy;
  rob_tag_t producer [32];
  logic blocked;
  operand_t selected_operands [4];
  logic [3:0] same_bundle;
  for (genvar p = 0; p < 2; p++) begin : lanes
    assign read_address[2*p] = {1'b0, decoded[p].rs1};
    assign read_address[2*p+1] = {1'b0, decoded[p].rs2};
    assign lookup_tag[2*p] = producer[decoded[p].rs1];
    assign lookup_tag[2*p+1] = producer[decoded[p].rs2];
    for (genvar s = 0; s < 2; s++) begin : sources
      localparam int PORT = 2*p+s;
      assign same_bundle[PORT] = p == 1 && !decoded[0].terminal && decoded[0].rd != 0 &&
                                  read_address[PORT] == {1'b0, decoded[0].rd};
      operand_select select_operand (
        .zero_operand(read_address[PORT] == 0),
        .immediate_operand(s == 1 && decoded[p].immediate_b),
        .immediate_value(decoded[p].immediate), .register_value(read_value[PORT]),
        .producer_present(busy[read_address[PORT][4:0]] || same_bundle[PORT]),
        .producer_ready(lookup_data[PORT].valid && lookup_data[PORT].done && !same_bundle[PORT]),
        .producer_value(lookup_data[PORT].value), .source_tag(read_address[PORT]),
        .operand(selected_operands[PORT])
      );
    end
  end
  always_comb begin
    candidate_busy = busy;
    allow_count = 0;
    blocked = 0;
    for (int p = 0; p < 2; p++) begin
      prepared[p] = '0;
      prepared[p].valid = 1;
      prepared[p].op = decoded[p].op;
      prepared[p].meta = '{pc:decoded[p].pc, rd:decoded[p].rd,
                            pdst:{1'b0, decoded[p].rd}, old_pdst:6'b0, tag:allocate_tag[p]};
      prepared[p].a = selected_operands[2*p];
      prepared[p].b = selected_operands[2*p+1];
      if (p < int'(offer_count) && !blocked) begin
        if (decoded[p].terminal) begin
          allow_count = allow_count + 1'b1;
          blocked = 1;
        end else if (decoded[p].rd != 0 && candidate_busy[decoded[p].rd]) blocked = 1;
        else begin
          allow_count = allow_count + 1'b1;
          if (decoded[p].rd != 0) candidate_busy[decoded[p].rd] = 1;
        end
      end
    end
  end
  always_ff @(posedge clk) begin
    if (reset || flush) begin
      busy <= 0;
      for (int r = 0; r < 32; r++) producer[r] <= 0;
    end else begin
      for (int p = 0; p < 2; p++) begin
        if (p < int'(retire_count) && retire_data[p].meta.rd != 0)
          busy[retire_data[p].meta.rd] <= 0;
        if (p < int'(dispatch_count) && !decoded[p].terminal && decoded[p].rd != 0) begin
          busy[decoded[p].rd] <= 1;
          producer[decoded[p].rd] <= allocate_tag[p];
        end
      end
    end
  end
endmodule
