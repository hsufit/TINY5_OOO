package tiny5_pkg;
  localparam int XLEN = 32;
  localparam int ROB_DEPTH = 16;
  localparam int IQ_DEPTH = 8;
  localparam int PHYS_REGS = 64;
  typedef logic [3:0] rob_tag_t;
  typedef logic [5:0] phys_t;
  typedef enum logic [4:0] {
    OP_ADD, OP_SUB, OP_SLL, OP_SLT, OP_SLTU, OP_XOR, OP_SRL, OP_SRA,
    OP_OR, OP_AND, OP_MUL, OP_MULH, OP_MULHSU, OP_MULHU,
    OP_DIV, OP_DIVU, OP_REM, OP_REMU
  } op_t;
  localparam logic [1:0] IMEM_OK = 0, IMEM_END = 1, IMEM_FAULT = 2;
  localparam logic [1:0] FAULT_NONE = 0, FAULT_ILLEGAL = 1, FAULT_ACCESS = 2;
  typedef struct packed {
    logic [31:0] pc, instruction;
    logic [1:0] status;
  } fetch_t;
  typedef struct packed {
    logic [31:0] pc, immediate;
    op_t op;
    logic [4:0] rs1, rs2, rd;
    logic immediate_b;
    logic terminal;
    logic [1:0] fault;
  } decode_t;
  typedef struct packed {
    logic [31:0] pc;
    logic [4:0] rd;
    phys_t pdst, old_pdst;
    rob_tag_t tag;
  } meta_t;
  typedef struct packed {
    logic ready;
    phys_t tag;
    logic [31:0] value;
  } operand_t;
  typedef struct packed {
    logic valid;
    meta_t meta;
    op_t op;
    operand_t a, b;
  } issue_t;
  typedef struct packed {
    meta_t meta;
    op_t op;
    logic [31:0] a, b;
  } execute_t;
  typedef struct packed {
    meta_t meta;
    logic [31:0] value;
  } result_t;
  typedef struct packed {
    logic valid, done, terminal;
    logic [1:0] fault;
    meta_t meta;
    logic [31:0] value;
  } rob_entry_t;
  function automatic logic is_muldiv(input op_t op);
    return op >= OP_MUL;
  endfunction
endpackage
