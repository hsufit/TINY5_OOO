module rv32_decode (
  input tiny5_pkg::fetch_t fetched,
  output tiny5_pkg::decode_t decoded
);
  import tiny5_pkg::*;
  logic legal;
  logic [6:0] opcode, funct7;
  logic [2:0] funct3;
  always_comb begin
    opcode = fetched.instruction[6:0];
    funct3 = fetched.instruction[14:12];
    funct7 = fetched.instruction[31:25];
    decoded = '0;
    decoded.pc = fetched.pc;
    decoded.op = OP_ADD;
    decoded.rd = fetched.instruction[11:7];
    decoded.rs1 = fetched.instruction[19:15];
    decoded.rs2 = fetched.instruction[24:20];
    decoded.immediate = {{20{fetched.instruction[31]}}, fetched.instruction[31:20]};
    legal = 1;
    case (opcode)
      7'h13: begin
        decoded.immediate_b = 1;
        decoded.rs2 = 0;
        case (funct3)
          0: decoded.op = OP_ADD;
          2: decoded.op = OP_SLT;
          3: decoded.op = OP_SLTU;
          4: decoded.op = OP_XOR;
          6: decoded.op = OP_OR;
          7: decoded.op = OP_AND;
          1: begin decoded.op = OP_SLL; legal = funct7 == 0; end
          5: begin
            decoded.op = funct7 == 7'h20 ? OP_SRA : OP_SRL;
            legal = funct7 == 0 || funct7 == 7'h20;
          end
          default: legal = 0;
        endcase
      end
      7'h33: begin
        if (funct7 == 1) begin
          case (funct3)
            0: decoded.op = OP_MUL;
            1: decoded.op = OP_MULH;
            2: decoded.op = OP_MULHSU;
            3: decoded.op = OP_MULHU;
            4: decoded.op = OP_DIV;
            5: decoded.op = OP_DIVU;
            6: decoded.op = OP_REM;
            7: decoded.op = OP_REMU;
          endcase
        end else begin
          legal = funct7 == 0;
          case (funct3)
            0: begin
              decoded.op = funct7 == 7'h20 ? OP_SUB : OP_ADD;
              legal = funct7 == 0 || funct7 == 7'h20;
            end
            1: decoded.op = OP_SLL;
            2: decoded.op = OP_SLT;
            3: decoded.op = OP_SLTU;
            4: decoded.op = OP_XOR;
            5: begin
              decoded.op = funct7 == 7'h20 ? OP_SRA : OP_SRL;
              legal = funct7 == 0 || funct7 == 7'h20;
            end
            6: decoded.op = OP_OR;
            7: decoded.op = OP_AND;
          endcase
        end
      end
      default: legal = 0;
    endcase
    if (fetched.status != IMEM_OK || !legal) begin
      decoded.terminal = 1;
      decoded.rd = 0;
      decoded.rs1 = 0;
      decoded.rs2 = 0;
      if (fetched.status == IMEM_END) decoded.fault = FAULT_NONE;
      else if (fetched.status != IMEM_OK) decoded.fault = FAULT_ACCESS;
      else decoded.fault = FAULT_ILLEGAL;
    end
  end
endmodule
