module core_pipeline #(
  parameter int ISSUE_WIDTH = 2,
  parameter bit OUT_OF_ORDER = 0
) (
  input logic clk, reset,
  output logic imem_req_valid,
  input logic imem_req_ready,
  output logic [31:0] imem_req_addr,
  input logic imem_rsp_valid,
  output logic imem_rsp_ready,
  input logic [31:0] imem_rsp_data,
  input logic [1:0] imem_rsp_status,
  output logic retire0_valid, retire1_valid,
  output logic [31:0] retire0_pc, retire1_pc, retire0_value, retire1_value,
  output logic [4:0] retire0_rd, retire1_rd,
  output logic halted, fault,
  output logic [1:0] fault_code
);
  import tiny5_pkg::*;
  fetch_t fetched, fifo_data [2], decode_input [2];
  decode_t decoded [2], dispatch_data [2];
  logic fetched_valid, fetched_ready, fetch_idle, fetch_stop, fetch_stopped;
  logic [1:0] fifo_available, fifo_pop, if_capacity, if_available, id_pop, id_capacity, id_available;
  logic [1:0] offer_count, allow_count, dispatch_count, enqueue_count, rob_capacity, iq_capacity;
  logic front_flush, finish, terminal_dispatch, terminal_enqueued;
  issue_t prepared [2], iq_entries [IQ_DEPTH];
  rob_entry_t allocate_data [2], retire_data [2], lookup_data [4];
  rob_tag_t allocate_tag [2], lookup_tag [4], head_tag;
  phys_t read_address [4], write_address [2];
  logic [31:0] read_value [4], write_value [2];
  logic [1:0] write_valid, retire_count;
  logic [IQ_DEPTH-1:0] remove_mask;
  logic [1:0] issue_valid, issue_ready, stage_ready, ex_valid, ex_ready;
  execute_t issue_data [2], ex_data [2];
  logic [1:0] result_valid, result_ready, wb_stage_valid, wb_valid;
  result_t result_data [2], wb_data [2];
  logic md_available, md_issue_available;

  fetch_unit fetch (
    .clk, .reset, .stop(fetch_stop), .req_valid(imem_req_valid), .req_ready(imem_req_ready),
    .req_addr(imem_req_addr), .rsp_valid(imem_rsp_valid), .rsp_ready(imem_rsp_ready),
    .rsp_data(imem_rsp_data), .rsp_status(imem_rsp_status),
    .out_valid(fetched_valid), .out_ready(fetched_ready), .out_data(fetched), .idle(fetch_idle)
  );
  instruction_fifo fetch_buffer (
    .clk, .reset, .flush(front_flush), .in_valid(fetched_valid && !terminal_enqueued),
    .in_ready(fetched_ready), .in_data(fetched), .pop_count(fifo_pop),
    .available(fifo_available), .out_data(fifo_data)
  );
  assign fifo_pop = fifo_available < if_capacity ? fifo_available : if_capacity;
  if2id fetch_decode_register (
    .clk, .reset, .flush(front_flush), .push_count(fifo_pop), .in_data(fifo_data),
    .capacity(if_capacity), .pop_count(id_pop), .available(if_available), .out_data(decode_input)
  );
  for (genvar p = 0; p < 2; p++) begin : decoders
    rv32_decode decoder (.fetched(decode_input[p]), .decoded(decoded[p]));
  end
  assign id_pop = if_available < id_capacity ? if_available : id_capacity;
  id2dispatch decode_dispatch_register (
    .clk, .reset, .flush(front_flush), .push_count(id_pop), .in_data(decoded),
    .capacity(id_capacity), .pop_count(dispatch_count), .available(id_available), .out_data(dispatch_data)
  );
  assign fetch_stop = fetch_stopped ||
    (if_available > 0 && decoded[0].terminal) || (if_available > 1 && decoded[1].terminal);
  assign front_flush = terminal_dispatch || finish;
  always_ff @(posedge clk) begin
    if (reset) begin
      fetch_stopped <= 0;
      terminal_enqueued <= 0;
    end else begin
      if (fetch_stop) fetch_stopped <= 1;
      if (terminal_dispatch) terminal_enqueued <= 1;
    end
  end
  always_comb begin
    offer_count = id_available;
    if (int'(offer_count) > ISSUE_WIDTH) offer_count = 2'(ISSUE_WIDTH);
    if (offer_count > rob_capacity) offer_count = rob_capacity;
    if (offer_count > iq_capacity) offer_count = iq_capacity;
    if (reset || terminal_enqueued || halted || fault) offer_count = 0;
    dispatch_count = allow_count;
    enqueue_count = 0;
    terminal_dispatch = 0;
    for (int p = 0; p < 2; p++) begin
      allocate_data[p] = '0;
      allocate_data[p].valid = 1;
      allocate_data[p].done = dispatch_data[p].terminal;
      allocate_data[p].terminal = dispatch_data[p].terminal;
      allocate_data[p].fault = dispatch_data[p].fault;
      allocate_data[p].meta = prepared[p].meta;
      if (p < int'(dispatch_count)) begin
        if (dispatch_data[p].terminal) terminal_dispatch = 1;
        else enqueue_count = enqueue_count + 1'b1;
      end
    end
  end
  if (OUT_OF_ORDER) begin : out_of_order
    rename_control operands (
      .clk, .reset, .offer_count, .dispatch_count, .decoded(dispatch_data), .allocate_tag,
      .allow_count, .prepared, .read_address, .read_value, .complete_valid(wb_valid),
      .complete_data(wb_data), .retire_count, .retire_data
    );
    for (genvar p = 0; p < 4; p++) begin : unused_lookups
      assign lookup_tag[p] = '0;
    end
    ooo_scheduler scheduler (
      .entries(iq_entries), .head_tag, .out_ready(issue_ready), .muldiv_available(md_issue_available),
      .out_valid(issue_valid), .out_data(issue_data), .remove_mask
    );
  end else begin : in_order
    inorder_operands operands (
      .clk, .reset, .flush(finish), .offer_count, .dispatch_count, .decoded(dispatch_data),
      .allocate_tag, .allow_count, .prepared, .read_address, .read_value, .lookup_tag,
      .lookup_data, .retire_count, .retire_data
    );
    inorder_scheduler #(.ISSUE_WIDTH(ISSUE_WIDTH)) scheduler (
      .entries(iq_entries), .head_tag, .out_ready(issue_ready), .muldiv_available(md_issue_available),
      .out_valid(issue_valid), .out_data(issue_data), .remove_mask
    );
  end
  always_comb begin
    write_valid = 0;
    for (int p = 0; p < 2; p++) begin
      write_address[p] = OUT_OF_ORDER ? wb_data[p].meta.pdst : {1'b0, retire_data[p].meta.rd};
      write_value[p] = OUT_OF_ORDER ? wb_data[p].value : retire_data[p].value;
      write_valid[p] = OUT_OF_ORDER ? wb_valid[p] : p < int'(retire_count);
    end
  end
  register_file #(.WORDS(OUT_OF_ORDER ? PHYS_REGS : 32)) register_bank (
    .clk, .reset, .read_address, .read_value, .write_valid, .write_address, .write_value
  );
  issue_queue issue_buffer (
    .clk, .reset, .flush(finish), .enqueue_count, .enqueue_data(prepared), .capacity(iq_capacity),
    .remove_mask, .complete_valid(wb_valid), .complete_data(wb_data), .entries(iq_entries)
  );
  assign md_issue_available = md_available &&
    !(ex_valid[0] && is_muldiv(ex_data[0].op)) && !(ex_valid[1] && is_muldiv(ex_data[1].op));
  for (genvar p = 0; p < 2; p++) begin : execute_registers
    assign issue_ready[p] = stage_ready[p] && p < ISSUE_WIDTH && !finish && !halted && !fault && !reset;
    issue2ex issue_execute_register (
      .clk, .reset, .flush(finish), .in_valid(issue_valid[p]), .in_ready(stage_ready[p]),
      .in_data(issue_data[p]), .out_valid(ex_valid[p]), .out_ready(ex_ready[p]), .out_data(ex_data[p])
    );
    ex2wb execute_writeback_register (
      .clk, .reset, .flush(finish), .in_valid(result_valid[p]), .in_ready(result_ready[p]),
      .in_data(result_data[p]), .out_valid(wb_stage_valid[p]), .out_ready(1'b1), .out_data(wb_data[p])
    );
  end
  execution_cluster #(.ALU_COUNT(ISSUE_WIDTH)) execution (
    .clk, .reset, .flush(finish), .in_valid(ex_valid), .in_ready(ex_ready), .in_data(ex_data),
    .out_valid(result_valid), .out_ready(result_ready), .out_data(result_data), .muldiv_available(md_available)
  );
  assign wb_valid = wb_stage_valid & {2{!finish && !halted && !fault}};
  completion_queue completion (
    .clk, .reset, .flush(finish), .allocate_count(dispatch_count), .allocate_data,
    .capacity(rob_capacity), .allocate_tag, .complete_valid(wb_valid), .complete_data(wb_data),
    .retire_count, .head_data(retire_data), .head_tag, .lookup_tag, .lookup_data
  );
  retire_unit #(.RETIRE_WIDTH(ISSUE_WIDTH)) retirement (
    .clk, .reset, .fetch_idle, .head_data(retire_data), .retire_count, .finish,
    .retire0_valid, .retire1_valid, .retire0_pc, .retire1_pc, .retire0_value, .retire1_value,
    .retire0_rd, .retire1_rd, .halted, .fault, .fault_code
  );
`ifndef SYNTHESIS
  always_ff @(posedge clk) if (!reset) begin
    assert (!retire1_valid || retire0_valid);
    assert (!(halted && fault));
    if (halted || fault) assert (!retire0_valid && !retire1_valid);
    if (ISSUE_WIDTH == 1) assert (!retire1_valid && !issue_valid[1]);
  end
`endif
endmodule
