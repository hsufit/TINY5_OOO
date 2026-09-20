module retire_unit #(parameter int RETIRE_WIDTH = 2) (
  input logic clk, reset, fetch_idle,
  input tiny5_pkg::rob_entry_t head_data [2],
  output logic [1:0] retire_count,
  output logic finish,
  output logic retire0_valid, retire1_valid,
  output logic [31:0] retire0_pc, retire1_pc, retire0_value, retire1_value,
  output logic [4:0] retire0_rd, retire1_rd,
  output logic halted, fault,
  output logic [1:0] fault_code
);
  always_comb begin
    retire_count = 0;
    finish = 0;
    if (!halted && !fault && head_data[0].valid && head_data[0].done) begin
      if (head_data[0].terminal) finish = fetch_idle;
      else begin
        retire_count = 1;
        if (RETIRE_WIDTH == 2 && head_data[1].valid && head_data[1].done &&
            !head_data[1].terminal) retire_count = 2;
      end
    end
  end
  always_ff @(posedge clk) begin
    if (reset) begin
      retire0_valid <= 0;
      retire1_valid <= 0;
      retire0_pc <= 0;
      retire1_pc <= 0;
      retire0_rd <= 0;
      retire1_rd <= 0;
      retire0_value <= 0;
      retire1_value <= 0;
      halted <= 0;
      fault <= 0;
      fault_code <= 0;
    end else begin
      retire0_valid <= retire_count > 0;
      retire1_valid <= retire_count > 1;
      if (retire_count > 0) begin
        retire0_pc <= head_data[0].meta.pc;
        retire0_rd <= head_data[0].meta.rd;
        retire0_value <= head_data[0].value;
      end
      if (retire_count > 1) begin
        retire1_pc <= head_data[1].meta.pc;
        retire1_rd <= head_data[1].meta.rd;
        retire1_value <= head_data[1].value;
      end
      if (finish) begin
        halted <= head_data[0].fault == 0;
        fault <= head_data[0].fault != 0;
        fault_code <= head_data[0].fault;
      end
    end
  end
endmodule
