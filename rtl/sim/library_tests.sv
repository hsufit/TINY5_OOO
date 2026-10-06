// Simulation-only protocol and resource tests. Production RTL contains no delays.
module library_tests;
  import tiny5_pkg::*;
  logic clk, reset = 1;
  initial begin
    clk = 0;
    forever #5 clk = ~clk;
  end
  task automatic tick;
    @(posedge clk);
    #1;
    @(negedge clk);
  endtask

  logic q_flush = 0;
  logic [1:0] q_push = 0, q_pop = 0, q_capacity, q_available;
  logic [31:0] q_in [2], q_out [2];
  packet_queue #(.WIDTH(32), .DEPTH(4)) queue_under_test (
    .clk, .reset, .flush(q_flush), .push_count(q_push), .push_data(q_in),
    .push_capacity(q_capacity), .pop_count(q_pop), .pop_available(q_available), .pop_data(q_out)
  );
  logic e_flush = 0, e_in_valid = 0, e_in_ready, e_out_valid, e_out_ready = 0;
  logic [31:0] e_in_data = 0, e_out_data;
  elastic_reg elastic_under_test (
    .clk, .reset, .flush(e_flush), .in_valid(e_in_valid), .in_ready(e_in_ready),
    .in_data(e_in_data), .out_valid(e_out_valid), .out_ready(e_out_ready), .out_data(e_out_data)
  );
  logic md_flush = 0, md_in_valid = 0, md_in_ready, md_out_valid, md_out_ready = 0;
  execute_t md_in;
  result_t md_out;
  muldiv_iterative muldiv_under_test (
    .clk, .reset, .flush(md_flush), .in_valid(md_in_valid), .in_ready(md_in_ready),
    .in_data(md_in), .out_valid(md_out_valid), .out_ready(md_out_ready), .out_data(md_out)
  );
  logic [1:0] f_allocate = 0, f_available, f_release = 0;
  phys_t f_register [2], f_released [2];
  free_list #(.REG_COUNT(36)) free_under_test (
    .clk, .reset, .restore(1'b0), .restore_free(36'b0), .allocate_count(f_allocate), .available(f_available), .allocate_register(f_register),
    .release_valid(f_release), .release_register(f_released)
  );
  logic a_flush = 0;
  logic [2:0] a_valid = 0, a_ready;
  result_t a_in [3], a_out [2];
  logic [1:0] a_out_valid, a_out_ready = 0;
  writeback_arbiter arbiter_under_test (
    .clk, .reset, .flush(a_flush), .in_valid(a_valid), .in_ready(a_ready), .in_data(a_in),
    .out_valid(a_out_valid), .out_ready(a_out_ready), .out_data(a_out)
  );
  issue_t s_entries [IQ_DEPTH];
  logic [1:0] s_ready = 0, io_valid, oo_valid;
  logic s_md = 0;
  execute_t io_data [2], oo_data [2];
  logic [IQ_DEPTH-1:0] io_remove, oo_remove;
  inorder_scheduler io_scheduler (
    .entries(s_entries), .head_tag(4'd14), .out_ready(s_ready), .muldiv_available(s_md),
    .out_valid(io_valid), .out_data(io_data), .remove_mask(io_remove)
  );
  ooo_scheduler oo_scheduler (
    .entries(s_entries), .head_tag(4'd14), .out_ready(s_ready), .muldiv_available(s_md),
    .out_valid(oo_valid), .out_data(oo_data), .remove_mask(oo_remove)
  );

  logic c_flush = 0;
  logic [1:0] c_allocate = 0, c_capacity, c_complete = 0, c_retire = 0;
  rob_entry_t c_in [2], c_head [2], c_lookup [4];
  result_t c_results [2];
  rob_tag_t c_tags [2], c_head_tag, c_lookup_tags [4];
  meta_t saved_meta [16];
  completion_queue completion_under_test (
    .clk, .reset, .flush(c_flush), .allocate_count(c_allocate), .allocate_data(c_in),
    .capacity(c_capacity), .allocate_tag(c_tags), .complete_valid(c_complete), .complete_data(c_results),
    .retire_count(c_retire), .head_data(c_head), .head_tag(c_head_tag),
    .lookup_tag(c_lookup_tags), .lookup_data(c_lookup)
  );
  rob_entry_t t_head [2];
  logic [1:0] t_count, t_code;
  logic t_idle = 0, t_finish, t_v0, t_v1, t_halt, t_fault;
  logic [31:0] t_pc0, t_pc1, t_value0, t_value1;
  logic [4:0] t_rd0, t_rd1;
  retire_unit retire_under_test (
    .clk, .reset, .retire_limit(2'd2), .fetch_idle(t_idle), .head_data(t_head), .retire_count(t_count), .finish(t_finish),
    .retire0_valid(t_v0), .retire1_valid(t_v1), .retire0_pc(t_pc0), .retire1_pc(t_pc1),
    .retire0_value(t_value0), .retire1_value(t_value1), .retire0_rd(t_rd0), .retire1_rd(t_rd1),
    .halted(t_halt), .fault(t_fault), .fault_code(t_code)
  );
  logic [1:0] r_offer = 0, r_dispatch = 0, r_allow, r_complete = 0, r_retire = 0;
  decode_t r_decode [2];
  rob_tag_t r_tags [2];
  issue_t r_prepared [2];
  phys_t r_address [4];
  logic [31:0] r_value [4];
  result_t r_results [2];
  rob_entry_t r_retired [2];
  meta_t rename_first, rename_second;
  rename_control rename_under_test (
    .clk, .reset, .restore(1'b0), .offer_count(r_offer), .dispatch_count(r_dispatch), .decoded(r_decode),
    .allocate_tag(r_tags), .allow_count(r_allow), .prepared(r_prepared),
    .read_address(r_address), .read_value(r_value), .complete_valid(r_complete),
    .complete_data(r_results), .retire_count(r_retire), .retire_data(r_retired)
  );

  function automatic logic [31:0] expected_m(input op_t op, input logic [31:0] a, b);
    logic signed [63:0] signed_a, signed_b, signed_product;
    logic [63:0] unsigned_product;
    signed_a = {{32{a[31]}}, a};
    signed_b = {{32{b[31]}}, b};
    signed_product = signed_a * (op == OP_MULHSU ? $signed({32'b0, b}) : signed_b);
    unsigned_product = {32'b0, a} * {32'b0, b};
    case (op)
      OP_MUL: return unsigned_product[31:0];
      OP_MULH, OP_MULHSU: return signed_product[63:32];
      OP_MULHU: return unsigned_product[63:32];
      OP_DIV: begin
        if (b == 0) return 32'hffffffff;
        if (a == 32'h80000000 && b == 32'hffffffff) return a;
        return $signed(a) / $signed(b);
      end
      OP_DIVU: return b == 0 ? 32'hffffffff : a / b;
      OP_REM: begin
        if (b == 0) return a;
        if (a == 32'h80000000 && b == 32'hffffffff) return 0;
        return $signed(a) % $signed(b);
      end
      default: return b == 0 ? a : a % b;
    endcase
  endfunction
  task automatic check_muldiv(input op_t op, input logic [31:0] a, b);
    logic [31:0] expected_value;
    expected_value = expected_m(op, a, b);
    md_in = '0;
    md_in.meta.rd = 1;
    md_in.meta.pdst = 35;
    md_in.meta.tag = 9;
    md_in.op = op;
    md_in.a = a;
    md_in.b = b;
    md_in_valid = 1;
    #1;
    assert (md_in_ready) else $fatal(1, "M unit did not accept idle request");
    tick();
    md_in_valid = 0;
    repeat (32) begin
      tick();
      assert (!md_out_valid && !md_in_ready) else $fatal(1, "M latency below 33 cycles");
    end
    tick();
    assert (md_out_valid && md_out.value == expected_value && md_out.meta == md_in.meta)
      else $fatal(1, "M arithmetic op=%0d a=%h b=%h result=%h expected=%h", op, a, b, md_out.value, expected_value);
    repeat (3) begin
      tick();
      assert (md_out_valid && md_out.value == expected_value && !md_in_ready)
        else $fatal(1, "M result changed under backpressure");
    end
    md_out_ready = 1;
    tick();
    md_out_ready = 0;
    assert (!md_out_valid) else $fatal;
  endtask

  logic [31:0] corners [9];
  initial begin
    for (int p = 0; p < 2; p++) begin
      q_in[p] = 0;
      f_released[p] = 0;
      c_in[p] = '0;
      c_results[p] = '0;
      t_head[p] = '0;
      r_decode[p] = '0;
      r_tags[p] = rob_tag_t'(p);
      r_results[p] = '0;
      r_retired[p] = '0;
    end
    for (int p = 0; p < 3; p++) a_in[p] = '0;
    for (int p = 0; p < 4; p++) begin c_lookup_tags[p] = 0; r_value[p] = 11; end
    for (int p = 0; p < IQ_DEPTH; p++) s_entries[p] = '0;
    for (int p = 0; p < 16; p++) saved_meta[p] = '0;
    md_in = '0;
    repeat (2) tick();
    reset = 0;

    q_push = 2; q_in[0] = 11; q_in[1] = 22;
    tick();
    assert (q_out[0] == 11 && q_out[1] == 22 && q_available == 2) else $fatal;
    q_pop = 1; q_in[0] = 33; q_in[1] = 44;
    tick();
    q_pop = 0; q_push = 0;
    assert (q_out[0] == 22 && q_out[1] == 33 && q_capacity == 1) else $fatal;
    repeat (3) tick();
    assert (q_out[0] == 22 && q_out[1] == 33) else $fatal;
    q_push = 1; q_in[0] = 55;
    tick();
    q_push = 0; q_pop = 2;
    assert (q_capacity == 0) else $fatal;
    tick();
    q_pop = 0;
    assert (q_out[0] == 44 && q_out[1] == 55) else $fatal;
    q_flush = 1; tick(); q_flush = 0;
    assert (q_available == 0) else $fatal;
    e_in_valid = 1; e_in_data = 32'habcdef01;
    tick(); e_in_valid = 0;
    repeat (3) begin
      tick(); assert (e_out_valid && !e_in_ready && e_out_data == 32'habcdef01) else $fatal;
    end
    e_flush = 1; tick(); e_flush = 0;
    assert (!e_out_valid) else $fatal;
    $display("[PASS] partial packets, full queue, stable stalls, flush");

    #1;
    assert (f_available == 2 && f_register[0] == 32 && f_register[1] == 33) else $fatal;
    f_allocate = 2; tick();
    assert (f_register[0] == 34 && f_register[1] == 35) else $fatal;
    tick(); f_allocate = 0;
    assert (f_available == 0) else $fatal;
    f_release = 1; f_released[0] = 32; tick(); f_release = 0;
    assert (f_available == 1 && f_register[0] == 32) else $fatal;
    f_allocate = 1; f_release = 1; f_released[0] = 33;
    tick(); f_allocate = 0; f_release = 0;
    assert (f_available == 1 && f_register[0] == 33) else $fatal;
    $display("[PASS] free-list exhaustion and simultaneous allocate/release");

    for (int p = 0; p < 3; p++) begin a_in[p].meta.tag = rob_tag_t'(p); a_in[p].value = 32'(100+p); end
    a_valid = 7; a_out_ready = 0; #1;
    assert (a_ready == 0) else $fatal;
    a_out_ready = 1; #1;
    assert (a_out_valid == 1 && a_out[0].value == 100) else $fatal;
    tick(); #1;
    assert (a_out[0].value == 101) else $fatal;
    tick(); #1;
    assert (a_out[0].value == 102) else $fatal;
    tick();
    a_out_ready = 3; #1;
    assert (a_out_valid == 3 && a_out[0].value == 100 && a_out[1].value == 101) else $fatal;
    a_valid = 0; a_out_ready = 0;
    $display("[PASS] three completions, blocked writeback, round-robin fairness");

    for (int p = 0; p < 3; p++) begin
      s_entries[p].valid = 1; s_entries[p].meta.tag = rob_tag_t'(14+p);
      s_entries[p].meta.pc = 32'(4*p); s_entries[p].a.ready = 1; s_entries[p].b.ready = 1;
    end
    s_entries[0].a.ready = 0; s_ready = 3; s_md = 1; #1;
    assert (io_valid == 0 && oo_valid == 3 && oo_data[0].meta.pc == 4 && oo_data[1].meta.pc == 8) else $fatal;
    s_entries[0].a.ready = 1; s_entries[0].op = OP_DIV; s_md = 0; #1;
    assert (io_valid == 0 && oo_valid == 3) else $fatal;
    s_md = 1; #1;
    assert (io_valid == 3 && io_data[0].meta.pc == 0 && io_data[1].meta.pc == 4) else $fatal;
    s_ready = 0;
    $display("[PASS] ordered issue, ready bypass, structural stalls, age wrap");

    r_decode[0].op = OP_ADD; r_decode[0].rd = 1; r_decode[0].immediate_b = 1;
    r_decode[0].immediate = 7;
    r_decode[1].op = OP_ADD; r_decode[1].rd = 1; r_decode[1].rs1 = 1; r_decode[1].rs2 = 1;
    r_offer = 2; #1;
    assert (r_allow == 2 && r_prepared[0].meta.pdst == 32 && r_prepared[0].meta.old_pdst == 1 &&
            r_prepared[1].meta.pdst == 33 && r_prepared[1].meta.old_pdst == 32 &&
            r_prepared[1].a.tag == 32 && !r_prepared[1].a.ready && r_prepared[1].b.tag == 32) else $fatal;
    rename_first = r_prepared[0].meta; rename_second = r_prepared[1].meta;
    // The preceding combinational checks may cross a rising edge. Drive the
    // transfer count on a falling edge so it is accepted exactly once.
    @(negedge clk);
    r_dispatch = 2; tick(); r_dispatch = 0;
    r_offer = 1; r_decode[0].rd = 2; r_decode[0].rs1 = 1; #1;
    assert (r_prepared[0].a.tag == 33 && !r_prepared[0].a.ready)
      else $fatal(1, "newest mapping tag=%0d ready=%0d map=%0d", r_prepared[0].a.tag,
                  r_prepared[0].a.ready, rename_under_test.speculative_map[1]);
    r_complete = 1; r_results[0].meta = rename_first; tick(); r_complete = 0; #1;
    assert (!r_prepared[0].a.ready) else $fatal;
    r_complete = 1; r_results[0].meta = rename_second; tick(); r_complete = 0; #1;
    assert (r_prepared[0].a.ready) else $fatal;
    r_retired[0].meta = rename_first; r_retired[1].meta = rename_second;
    r_retire = 2; tick(); r_retire = 0; #1;
    assert (r_prepared[0].meta.pdst == 1) else $fatal;
    r_offer = 0;
    $display("[PASS] same-bundle RAW/WAW renaming and ordered physical reclamation");

    for (int pair = 0; pair < 8; pair++) begin
      for (int p = 0; p < 2; p++) begin
        c_in[p] = '0; c_in[p].valid = 1;
        c_in[p].meta = '{pc:32'(pair*8+p*4), rd:5'd1, pdst:6'd1, old_pdst:6'd0, tag:c_tags[p]};
        saved_meta[pair*2+p] = c_in[p].meta;
      end
      c_allocate = 2; tick();
    end
    c_allocate = 0;
    assert (c_capacity == 0 && c_head_tag == 0 && !c_head[0].done) else $fatal;
    c_complete = 3;
    c_results[0] = '{meta:saved_meta[5], value:32'd5, branch:'0};
    c_results[1] = '{meta:saved_meta[6], value:32'd6, branch:'0};
    tick(); c_complete = 0;
    assert (!c_head[0].done) else $fatal;
    c_complete = 3;
    c_results[0] = '{meta:saved_meta[0], value:32'd10, branch:'0};
    c_results[1] = '{meta:saved_meta[1], value:32'd11, branch:'0};
    tick(); c_complete = 0;
    assert (c_head[0].done && c_head[1].done && c_head[0].value == 10) else $fatal;
    c_retire = 2; tick(); c_retire = 0;
    assert (c_head_tag == 2 && c_tags[0] == 0 && c_capacity == 2) else $fatal;
    for (int p = 0; p < 2; p++) begin
      c_in[p] = '0; c_in[p].valid = 1;
      c_in[p].meta = '{pc:32'(64+p*4), rd:5'd2, pdst:6'd2, old_pdst:6'd0, tag:c_tags[p]};
    end
    c_allocate = 2; tick(); c_allocate = 0;
    assert (c_capacity == 0 && c_head[0].meta.pc == 8) else $fatal;
    c_flush = 1; tick(); c_flush = 0;
    assert (!c_head[0].valid && c_capacity == 2) else $fatal;
    $display("[PASS] full completion queue, unordered completions, circular reuse");

    corners = '{32'b0, 32'b1, 32'd2, 32'hffffffff, 32'hfffffffe,
                32'h80000000, 32'h7fffffff, 32'hffff0001, 32'h00010001};
    for (int operation = int'(OP_MUL); operation <= int'(OP_REMU); operation++)
      for (int a = 0; a < 9; a++)
        for (int b = 0; b < 9; b++) check_muldiv(op_t'(operation), corners[a], corners[b]);
    md_in_valid = 1; tick(); md_in_valid = 0;
    repeat (10) tick();
    md_flush = 1; tick(); md_flush = 0;
    repeat (35) tick();
    assert (md_in_ready && !md_out_valid) else $fatal;
    check_muldiv(OP_DIV, 32'd21, 32'd3);
    $display("[PASS] 649 M operations, exact latency, held results, cancellation");

    t_head[0].valid = 1; t_head[0].done = 1; t_head[0].meta.rd = 1; t_head[0].value = 77;
    t_head[1].valid = 1; t_head[1].done = 1; t_head[1].terminal = 1; t_head[1].fault = FAULT_ILLEGAL;
    #1; assert (t_count == 1 && !t_finish) else $fatal;
    tick();
    assert (t_v0 && !t_v1 && !t_fault && t_value0 == 77) else $fatal;
    t_head[0] = t_head[1];
    t_head[1].terminal = 0; t_head[1].fault = 0; t_head[1].value = 99;
    #1; assert (t_count == 0 && !t_finish) else $fatal;
    tick();
    t_idle = 1; tick();
    assert (t_fault && !t_halt && t_code == FAULT_ILLEGAL && !t_v0 && !t_v1) else $fatal;
    repeat (3) tick();
    assert (t_fault && t_count == 0 && !t_v0 && !t_v1) else $fatal;
    $display("[PASS] precise terminal prefix, fetch drain, younger result suppression");
    $display("All RTL library tests passed");
    $finish;
  end
  initial begin
    #1000000;
    $fatal(1, "library test watchdog");
  end
endmodule
