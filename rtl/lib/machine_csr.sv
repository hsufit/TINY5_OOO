module machine_csr (
  input logic clk, reset, irq_external,
  input logic trap_valid,
  input logic [31:0] trap_pc, trap_cause, trap_tval,
  input logic mret_valid, csr_commit,
  input logic [31:0] csr_instruction, csr_source,
  output logic [31:0] csr_old_value, mtvec_base, mepc_value,
  output logic interrupt_enabled
);
  logic mie_bit, mpie_bit, meie_bit;
  logic [31:0] mscratch_q, mepc_q, mcause_q, mtval_q, mtvec_q;
  logic [31:0] csr_operand, csr_new_value;
  logic csr_write;
  logic [11:0] csr_address;
  logic [2:0] csr_op;

  assign csr_address = csr_instruction[31:20];
  assign csr_op = csr_instruction[14:12];
  assign csr_operand = csr_op[2] ? {27'b0, csr_instruction[19:15]} : csr_source;
  assign mtvec_base = mtvec_q;
  assign mepc_value = mepc_q;
  assign interrupt_enabled = mie_bit && meie_bit && irq_external;

  always_comb begin
    csr_old_value = 0;
    case (csr_address)
      12'h300: csr_old_value = {19'b0, 2'b11, 3'b0, mpie_bit, 3'b0, mie_bit, 3'b0};
      12'h301: csr_old_value = 32'h4000_1100; // RV32IM
      12'h304: csr_old_value = {20'b0, meie_bit, 11'b0};
      12'h305: csr_old_value = mtvec_q;
      12'h340: csr_old_value = mscratch_q;
      12'h341: csr_old_value = mepc_q;
      12'h342: csr_old_value = mcause_q;
      12'h343: csr_old_value = mtval_q;
      12'h344: csr_old_value = {20'b0, irq_external, 11'b0};
      default: csr_old_value = 0; // mhartid is zero.
    endcase
    csr_write = 0;
    csr_new_value = csr_old_value;
    case (csr_op)
      3'b001, 3'b101: begin csr_write = 1; csr_new_value = csr_operand; end
      3'b010, 3'b110: if (csr_instruction[19:15] != 0) begin
        csr_write = 1;
        csr_new_value = csr_old_value | csr_operand;
      end
      3'b011, 3'b111: if (csr_instruction[19:15] != 0) begin
        csr_write = 1;
        csr_new_value = csr_old_value & ~csr_operand;
      end
      default: ;
    endcase
  end

  always_ff @(posedge clk) begin
    if (reset) begin
      mie_bit <= 0;
      mpie_bit <= 0;
      meie_bit <= 0;
      mtvec_q <= 0;
      mscratch_q <= 0;
      mepc_q <= 0;
      mcause_q <= 0;
      mtval_q <= 0;
    end else if (trap_valid) begin
      mpie_bit <= mie_bit;
      mie_bit <= 0;
      mepc_q <= trap_pc;
      mcause_q <= trap_cause;
      mtval_q <= trap_tval;
    end else if (mret_valid) begin
      mie_bit <= mpie_bit;
      mpie_bit <= 1;
    end else if (csr_commit && csr_write) begin
      case (csr_address)
        12'h300: begin
          mie_bit <= csr_new_value[3];
          mpie_bit <= csr_new_value[7];
        end
        12'h304: meie_bit <= csr_new_value[11];
        12'h305: mtvec_q <= {csr_new_value[31:2], 2'b00};
        12'h340: mscratch_q <= csr_new_value;
        12'h341: mepc_q <= {csr_new_value[31:2], 2'b00};
        12'h342: mcause_q <= csr_new_value;
        12'h343: mtval_q <= csr_new_value;
        default: ; // misa and mip are read-only WARL fields.
      endcase
    end
  end
endmodule
