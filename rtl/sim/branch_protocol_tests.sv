// Focused redirect, dependency restoration, and retirement-lane tests.
module branch_protocol_tests;
  import tiny5_pkg::*;
  logic clk, reset = 1;
  initial begin
    clk = 0;
    forever #5 clk = ~clk;
  end
  task automatic tick;
    @(posedge clk); #1; @(negedge clk);
  endtask

  logic stop = 0, redirect = 0, req_valid, req_ready = 0, rsp_valid = 0, rsp_ready;
  logic [31:0] redirect_pc = 0, req_addr, rsp_data = 32'h13;
  logic [1:0] rsp_status = IMEM_OK;
  logic out_valid, out_ready = 0, idle;
  fetch_t out_data;
  fetch_unit fetch (
    .clk, .reset, .stop, .redirect_valid(redirect), .redirect_pc,
    .req_valid, .req_ready, .req_addr, .rsp_valid, .rsp_ready, .rsp_data, .rsp_status,
    .out_valid, .out_ready, .out_data, .idle
  );
  task automatic restart;
    reset = 1; stop = 0; redirect = 0; req_ready = 0;
    rsp_valid = 0; rsp_status = IMEM_OK; out_ready = 0;
    tick(); reset = 0;
  endtask
  task automatic expect_request(input logic [31:0] address);
    for (int n = 0; n < 8 && !req_valid; n++) tick();
    assert (req_valid && req_addr == address) else $fatal(1, "request %h, expected %h", req_addr, address);
  endtask
  task automatic accept_request;
    req_ready = 1; tick(); req_ready = 0;
    assert (!req_valid && rsp_ready) else $fatal;
  endtask
  task automatic respond(input logic [1:0] status);
    rsp_status = status; rsp_valid = 1; tick(); rsp_valid = 0;
  endtask

  logic restore = 0;
  logic [1:0] offer = 0, dispatch = 0, allow_count, complete_valid = 0, retire_count = 0;
  decode_t decoded [2];
  rob_tag_t tags [2];
  issue_t prepared [2];
  phys_t read_address [4];
  logic [31:0] read_value [4];
  result_t complete_data [2];
  rob_entry_t retired [2];
  meta_t committed_writer, younger_writer;
  rename_control rename (
    .clk, .reset, .restore, .offer_count(offer), .dispatch_count(dispatch), .decoded,
    .allocate_tag(tags), .allow_count, .prepared, .read_address, .read_value,
    .complete_valid, .complete_data, .retire_count, .retire_data(retired)
  );

  rob_entry_t heads [2];
  logic [1:0] limit, count, dispatch_limit, code;
  logic finish, branch_redirect, front_flush, backend_flush, branch_restore;
  logic [31:0] branch_pc, pc0, pc1, value0, value1;
  logic [4:0] rd0, rd1;
  logic valid0, valid1, halted, fault;
  branch_control_retire recovery (
    .clk, .reset, .finish, .decoded, .dispatch_count(2'b0), .allocate_tag(tags),
    .complete_valid(2'b0), .complete_data, .head_data(heads), .retire_count(count),
    .dispatch_limit, .retire_limit(limit), .redirect_valid(branch_redirect), .redirect_pc(branch_pc),
    .front_flush, .backend_flush, .restore(branch_restore)
  );
  retire_unit retirement (
    .clk, .reset, .fetch_idle(1'b1), .head_data(heads), .retire_limit(limit),
    .retire_count(count), .finish, .retire0_valid(valid0), .retire1_valid(valid1),
    .retire0_pc(pc0), .retire1_pc(pc1), .retire0_value(value0), .retire1_value(value1),
    .retire0_rd(rd0), .retire1_rd(rd1), .halted, .fault, .fault_code(code)
  );

  initial begin
    for (int p = 0; p < 2; p++) begin
      decoded[p] = '0; tags[p] = rob_tag_t'(p); complete_data[p] = '0;
      retired[p] = '0; heads[p] = '0;
    end
    for (int p = 0; p < 4; p++) read_value[p] = 32'(p);
    restart();
    expect_request(0);
    redirect = 1; redirect_pc = 32; tick(); redirect = 0;
    repeat (3) begin
      tick(); assert (req_valid && req_addr == 0 && !out_valid) else $fatal;
    end
    accept_request();
    respond(IMEM_FAULT);
    assert (!out_valid) else $fatal(1, "stale fault escaped redirect");
    expect_request(32); accept_request(); respond(IMEM_OK);
    assert (out_valid && out_data.pc == 32) else $fatal;
    // Redirect discards a held buffered response without delivering it downstream.
    redirect = 1; redirect_pc = 64; tick(); redirect = 0;
    assert (!out_valid) else $fatal;
    expect_request(64);
    $display("[PASS] redirect drains held request, stale fault and buffered output");

    restart(); expect_request(0); accept_request();
    redirect = 1; redirect_pc = 16;
    respond(IMEM_END); redirect = 0;
    assert (!out_valid) else $fatal;
    expect_request(16);
    $display("[PASS] redirect concurrent with terminal response");

    restart(); expect_request(0); accept_request(); respond(IMEM_END);
    assert (out_valid && out_data.status == IMEM_END) else $fatal;
    out_ready = 1; tick(); out_ready = 0; stop = 1; tick();
    assert (idle) else $fatal;
    redirect = 1; redirect_pc = 24; tick(); redirect = 0; stop = 0;
    expect_request(24);
    // Reset while draining an unaccepted old request cancels the redirect too.
    redirect = 1; redirect_pc = 80; tick(); redirect = 0;
    restart(); expect_request(0);
    $display("[PASS] restart after provisional stop and reset during redirect");

    // Commit x1 -> p32, then speculatively overwrite x1 in another register.
    restart();
    decoded[0].op = OP_ADD; decoded[0].rd = 1; offer = 1; #1;
    assert (allow_count == 1 && prepared[0].meta.pdst == 32) else $fatal;
    committed_writer = prepared[0].meta; dispatch = 1; tick(); dispatch = 0; offer = 0;
    complete_data[0].meta = committed_writer; complete_data[0].value = 7;
    complete_valid = 1; tick(); complete_valid = 0;
    retired[0].meta = committed_writer; retire_count = 1; tick(); retire_count = 0;
    offer = 1; tags[0] = 1; #1;
    younger_writer = prepared[0].meta; dispatch = 1; tick(); dispatch = 0; offer = 0;
    decoded[0].rs1 = 1; #1;
    assert (read_address[0] == younger_writer.pdst && !prepared[0].a.ready) else $fatal;
    restore = 1; tick(); restore = 0; #1;
    assert (read_address[0] == committed_writer.pdst && prepared[0].a.ready) else $fatal;
    assert (rename.registers.free_q[younger_writer.pdst] &&
            !rename.registers.free_q[committed_writer.pdst] && !rename.registers.free_q[0]) else $fatal;
    assert ($countones(rename.registers.free_q) == PHYS_REGS-32) else $fatal;
    $display("[PASS] committed rename restoration and physical register reclamation");

    // A taken branch in lane 1 must wait; then it retires alone and redirects.
    heads[0].valid = 1; heads[0].done = 1; heads[0].meta.pc = 0; heads[0].meta.rd = 1;
    heads[1].valid = 1; heads[1].done = 1; heads[1].meta.pc = 4;
    heads[1].branch = '{valid:1'b1, taken:1'b1, target:32'd40, fault:FAULT_NONE};
    #1; assert (count == 1 && !branch_redirect) else $fatal;
    tick(); assert (valid0 && !valid1 && pc0 == 0) else $fatal;
    heads[0] = heads[1]; heads[1] = '0;
    heads[1].valid = 1; heads[1].done = 1; heads[1].meta.pc = 8;
    #1; assert (count == 1 && branch_redirect && branch_pc == 40 && branch_restore) else $fatal;
    tick(); assert (valid0 && !valid1 && pc0 == 4 && !halted && !fault) else $fatal;
    heads[0] = '0; heads[1] = '0;
    $display("[PASS] taken branch retires alone in both retirement positions");
    $display("All branch protocol tests passed");
    $finish;
  end
  initial begin
    #20000; $fatal(1, "branch protocol test timeout");
  end
endmodule
