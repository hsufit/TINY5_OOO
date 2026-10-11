module core_pipeline #(
  parameter int ISSUE_WIDTH = 2,
  parameter bit OUT_OF_ORDER = 0,
  parameter bit BRANCH_AT_RETIRE = 0,
  parameter bit ENABLE_TRAPS = 0
) (
  input logic clk, reset, irq_external,
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
  logic [1:0] dispatch_limit, retire_limit;
  logic redirect_valid, branch_redirect_valid, branch_front_flush, branch_backend_flush;
  logic branch_restore, restore;
  logic backend_flush, branch_fault;
  logic [31:0] redirect_pc, branch_redirect_pc;
  logic sys_pending, sys_retire, mret_commit, csr_commit;
  logic trap_exception, trap_interrupt, trap_taken, csr_interrupt_enabled;
  logic [31:0] arch_next_pc, trap_pc, trap_cause, trap_tval;
  logic [31:0] csr_old_value, mtvec_base, mepc_value, system_read_value;
  phys_t committed_address, committed_write_address, system_read_address;

  if (BRANCH_AT_RETIRE) begin : retire_branches
    branch_control_retire control (
      .clk, .reset, .finish, .decoded(dispatch_data), .dispatch_count, .allocate_tag,
      .complete_valid(wb_valid), .complete_data(wb_data), .head_data(retire_data), .retire_count,
      .dispatch_limit, .retire_limit, .redirect_valid(branch_redirect_valid),
      .redirect_pc(branch_redirect_pc), .front_flush(branch_front_flush),
      .backend_flush(branch_backend_flush), .restore(branch_restore)
    );
  end else begin : blocking_branches
    branch_control_blocking control (
      .clk, .reset, .finish, .recovery(trap_taken || mret_commit),
      .decoded(dispatch_data), .dispatch_count, .allocate_tag,
      .complete_valid(wb_valid), .complete_data(wb_data), .head_data(retire_data), .retire_count,
      .dispatch_limit, .retire_limit, .redirect_valid(branch_redirect_valid),
      .redirect_pc(branch_redirect_pc), .front_flush(branch_front_flush),
      .backend_flush(branch_backend_flush), .restore(branch_restore)
    );
  end
  assign trap_exception = ENABLE_TRAPS && !reset && !halted && !fault &&
    retire_data[0].valid && retire_data[0].done && retire_data[0].terminal &&
    retire_data[0].fault != FAULT_NONE;
  assign trap_interrupt = ENABLE_TRAPS && !reset && !halted && !fault &&
    csr_interrupt_enabled && !trap_exception;
  assign trap_taken = trap_exception || trap_interrupt;
  assign trap_pc = trap_exception ? retire_data[0].meta.pc : arch_next_pc;
  assign trap_cause = trap_interrupt ? CAUSE_EXTERNAL :
    retire_data[0].trap_cause != 0 ? retire_data[0].trap_cause :
    retire_data[0].fault == FAULT_MISALIGNED ? CAUSE_INSTR_MISALIGNED :
    retire_data[0].fault == FAULT_ACCESS ? CAUSE_INSTR_ACCESS : CAUSE_ILLEGAL;
  assign trap_tval = trap_interrupt ? 0 :
    retire_data[0].fault == FAULT_MISALIGNED ? retire_data[0].branch.target :
    retire_data[0].fault == FAULT_ACCESS ? retire_data[0].meta.pc :
    trap_cause == CAUSE_ILLEGAL ? retire_data[0].instruction : 0;
  assign sys_retire = retire_count != 0 && retire_data[0].serial;
  assign mret_commit = ENABLE_TRAPS && sys_retire && retire_data[0].instruction == 32'h3020_0073;
  assign csr_commit = ENABLE_TRAPS && sys_retire && !mret_commit;
  assign redirect_valid = trap_taken || mret_commit || branch_redirect_valid;
  assign redirect_pc = trap_taken ? mtvec_base : mret_commit ? mepc_value : branch_redirect_pc;
  assign backend_flush = finish || trap_taken || mret_commit || branch_backend_flush;
  assign restore = branch_restore || (OUT_OF_ORDER && (trap_taken || mret_commit));
  assign branch_fault = (wb_valid[0] && wb_data[0].branch.fault != FAULT_NONE) ||
                        (wb_valid[1] && wb_data[1].branch.fault != FAULT_NONE);
  assign system_read_address = OUT_OF_ORDER ? committed_address :
                               {1'b0, retire_data[0].instruction[19:15]};
  machine_csr csr (
    .clk, .reset, .irq_external, .trap_valid(trap_taken), .trap_pc, .trap_cause, .trap_tval,
    .mret_valid(mret_commit), .csr_commit, .csr_instruction(retire_data[0].instruction),
    .csr_source(system_read_value), .csr_old_value, .mtvec_base, .mepc_value,
    .interrupt_enabled(csr_interrupt_enabled)
  );
  always_ff @(posedge clk) begin
    if (reset) arch_next_pc <= 0;
    else if (trap_taken) arch_next_pc <= mtvec_base;
    else if (mret_commit) arch_next_pc <= mepc_value;
    else if (retire_count > 1) arch_next_pc <= retire_data[1].branch.valid &&
      retire_data[1].branch.taken ? retire_data[1].branch.target : retire_data[1].meta.pc + 4;
    else if (retire_count > 0) arch_next_pc <= retire_data[0].branch.valid &&
      retire_data[0].branch.taken ? retire_data[0].branch.target : retire_data[0].meta.pc + 4;
  end

  fetch_unit fetch (
    .clk, .reset, .stop(fetch_stop), .redirect_valid, .redirect_pc,
    .req_valid(imem_req_valid), .req_ready(imem_req_ready),
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
    rv32_decode #(.ENABLE_TRAPS(ENABLE_TRAPS)) decoder
      (.fetched(decode_input[p]), .decoded(decoded[p]));
  end
  assign id_pop = if_available < id_capacity ? if_available : id_capacity;
  id2dispatch decode_dispatch_register (
    .clk, .reset, .flush(front_flush), .push_count(id_pop), .in_data(decoded),
    .capacity(id_capacity), .pop_count(dispatch_count), .available(id_available), .out_data(dispatch_data)
  );
  assign fetch_stop = fetch_stopped || branch_fault ||
    (if_available > 0 && decoded[0].terminal) || (if_available > 1 && decoded[1].terminal);
  assign front_flush = terminal_dispatch || finish || branch_front_flush ||
                       branch_fault || trap_taken || mret_commit;
  always_ff @(posedge clk) begin
    if (reset || redirect_valid) begin
      fetch_stopped <= 0;
      terminal_enqueued <= 0;
    end else begin
      if (fetch_stop) fetch_stopped <= 1;
      if (terminal_dispatch || branch_fault) terminal_enqueued <= 1;
    end
  end
  always_ff @(posedge clk) begin
    if (reset || backend_flush) sys_pending <= 0;
    else if (sys_retire) sys_pending <= 0;
    else for (int p = 0; p < 2; p++)
      if (p < int'(dispatch_count) && dispatch_data[p].serial) sys_pending <= 1;
  end
  always_comb begin
    offer_count = id_available;
    if (int'(offer_count) > ISSUE_WIDTH) offer_count = 2'(ISSUE_WIDTH);
    if (offer_count > rob_capacity) offer_count = rob_capacity;
    if (offer_count > iq_capacity) offer_count = iq_capacity;
    if (offer_count > dispatch_limit) offer_count = dispatch_limit;
    if (sys_pending) offer_count = 0;
    else if (id_available != 0 && dispatch_data[0].serial && offer_count > 1)
      offer_count = 1;
    if (reset || terminal_enqueued || halted || fault || backend_flush ||
        branch_front_flush || branch_fault) offer_count = 0;
    dispatch_count = allow_count;
    enqueue_count = 0;
    terminal_dispatch = 0;
    for (int p = 0; p < 2; p++) begin
      allocate_data[p] = '0;
      allocate_data[p].valid = 1;
      allocate_data[p].done = dispatch_data[p].terminal || dispatch_data[p].serial;
      allocate_data[p].terminal = dispatch_data[p].terminal;
      allocate_data[p].serial = dispatch_data[p].serial;
      allocate_data[p].fault = dispatch_data[p].fault;
      allocate_data[p].trap_cause = dispatch_data[p].trap_cause;
      allocate_data[p].instruction = dispatch_data[p].instruction;
      allocate_data[p].meta = prepared[p].meta;
      if (p < int'(dispatch_count)) begin
        if (dispatch_data[p].terminal) terminal_dispatch = 1;
        else if (!dispatch_data[p].serial) enqueue_count = enqueue_count + 1'b1;
      end
    end
  end
  if (OUT_OF_ORDER) begin : out_of_order
    rename_control operands (
      .clk, .reset, .restore, .offer_count, .dispatch_count, .decoded(dispatch_data), .allocate_tag,
      .allow_count, .prepared, .read_address, .read_value, .complete_valid(wb_valid),
      .complete_data(wb_data), .retire_count, .retire_data,
      .committed_source(retire_data[0].instruction[19:15]),
      .committed_destination(retire_data[0].meta.rd),
      .committed_address, .committed_write_address
    );
    for (genvar p = 0; p < 4; p++) begin : unused_lookups
      assign lookup_tag[p] = '0;
    end
    ooo_scheduler scheduler (
      .entries(iq_entries), .head_tag, .out_ready(issue_ready), .muldiv_available(md_issue_available),
      .out_valid(issue_valid), .out_data(issue_data), .remove_mask
    );
  end else begin : in_order
    assign committed_address = '0;
    assign committed_write_address = '0;
    inorder_operands operands (
      .clk, .reset, .flush(backend_flush), .offer_count, .dispatch_count, .decoded(dispatch_data),
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
    if (sys_retire && retire_data[0].meta.rd != 0) begin
      write_address[0] = OUT_OF_ORDER ? committed_write_address :
                         {1'b0, retire_data[0].meta.rd};
      write_value[0] = csr_old_value;
      write_valid[0] = 1;
    end
  end
  register_file #(.WORDS(OUT_OF_ORDER ? PHYS_REGS : 32)) register_bank (
    .clk, .reset, .read_address, .read_value, .system_read_address,
    .system_read_value, .write_valid, .write_address, .write_value
  );
  issue_queue issue_buffer (
    .clk, .reset, .flush(backend_flush), .enqueue_count, .enqueue_data(prepared), .capacity(iq_capacity),
    .remove_mask, .complete_valid(wb_valid), .complete_data(wb_data), .entries(iq_entries)
  );
  assign md_issue_available = md_available &&
    !(ex_valid[0] && is_muldiv(ex_data[0].op)) && !(ex_valid[1] && is_muldiv(ex_data[1].op));
  for (genvar p = 0; p < 2; p++) begin : execute_registers
    assign issue_ready[p] = stage_ready[p] && p < ISSUE_WIDTH && !backend_flush && !halted && !fault && !reset;
    issue2ex issue_execute_register (
      .clk, .reset, .flush(backend_flush), .in_valid(issue_valid[p]), .in_ready(stage_ready[p]),
      .in_data(issue_data[p]), .out_valid(ex_valid[p]), .out_ready(ex_ready[p]), .out_data(ex_data[p])
    );
    ex2wb execute_writeback_register (
      .clk, .reset, .flush(backend_flush), .in_valid(result_valid[p]), .in_ready(result_ready[p]),
      .in_data(result_data[p]), .out_valid(wb_stage_valid[p]), .out_ready(1'b1), .out_data(wb_data[p])
    );
  end
  execution_cluster #(.ALU_COUNT(ISSUE_WIDTH)) execution (
    .clk, .reset, .flush(backend_flush), .in_valid(ex_valid), .in_ready(ex_ready), .in_data(ex_data),
    .out_valid(result_valid), .out_ready(result_ready), .out_data(result_data), .muldiv_available(md_available)
  );
  assign wb_valid = wb_stage_valid & {2{!backend_flush && !halted && !fault && !reset}};
  completion_queue completion (
    .clk, .reset, .flush(backend_flush), .allocate_count(dispatch_count), .allocate_data,
    .capacity(rob_capacity), .allocate_tag, .complete_valid(wb_valid), .complete_data(wb_data),
    .retire_count, .head_data(retire_data), .head_tag, .lookup_tag, .lookup_data
  );
  retire_unit #(.RETIRE_WIDTH(ISSUE_WIDTH), .ENABLE_TRAPS(ENABLE_TRAPS)) retirement (
    .clk, .reset, .fetch_idle, .suppress_retire(trap_taken), .system_value(csr_old_value),
    .head_data(retire_data), .retire_limit, .retire_count, .finish,
    .retire0_valid, .retire1_valid, .retire0_pc, .retire1_pc, .retire0_value, .retire1_value,
    .retire0_rd, .retire1_rd, .halted, .fault, .fault_code
  );
`ifndef SYNTHESIS
  initial assert (!BRANCH_AT_RETIRE || OUT_OF_ORDER);
  always_ff @(posedge clk) if (!reset) begin
    assert (!retire1_valid || retire0_valid);
    assert (!(halted && fault));
    if (halted || fault) assert (!retire0_valid && !retire1_valid);
    if (ISSUE_WIDTH == 1) assert (!retire1_valid && !issue_valid[1]);
  end
`endif
endmodule
