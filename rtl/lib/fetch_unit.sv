module fetch_unit (
  input logic clk, reset, stop,
  input logic redirect_valid,
  input logic [31:0] redirect_pc,
  output logic req_valid,
  input logic req_ready,
  output logic [31:0] req_addr,
  input logic rsp_valid,
  output logic rsp_ready,
  input logic [31:0] rsp_data,
  input logic [1:0] rsp_status,
  output logic out_valid,
  input logic out_ready,
  output tiny5_pkg::fetch_t out_data,
  output logic idle
);
  import tiny5_pkg::*;
  logic outstanding, stopped, redirect_pending;
  logic [31:0] restart_pc;
  logic [31:0] pc, pending_pc;
  // A presented request is never withdrawn. Responses after stop are drained.
  assign rsp_ready = outstanding &&
    (stop || stopped || redirect_valid || redirect_pending || !out_valid || out_ready);
  assign idle = !req_valid && !outstanding && !out_valid && !redirect_pending;
  always_ff @(posedge clk) begin
    if (reset) begin
      req_valid <= 0;
      req_addr <= 0;
      pc <= 0;
      pending_pc <= 0;
      outstanding <= 0;
      stopped <= 0;
      redirect_pending <= 0;
      restart_pc <= 0;
      out_valid <= 0;
      out_data <= '0;
    end else begin
      if (out_ready || stop || redirect_valid) out_valid <= 0;
      if (stop) stopped <= 1;
      if (!req_valid && !outstanding && !stop && !stopped && !out_valid &&
          !redirect_valid && !redirect_pending) begin
        req_valid <= 1;
        req_addr <= pc;
      end
      if (req_valid && req_ready) begin
        req_valid <= 0;
        outstanding <= 1;
        pending_pc <= req_addr;
        pc <= pc + 4;
      end
      if (rsp_valid && rsp_ready) begin
        outstanding <= 0;
        if (!stop && !stopped && !redirect_valid && !redirect_pending) begin
          out_valid <= 1;
          out_data <= '{pc:pending_pc, instruction:rsp_data, status:rsp_status};
          if (rsp_status != IMEM_OK) stopped <= 1;
        end
      end
      // Preserve any old request until accepted, then drain its response.
      // Redirect wins over provisional stop/status from the discarded path.
      if (redirect_valid) begin
        restart_pc <= redirect_pc;
        redirect_pending <= 1;
        stopped <= 0;
      end else if (redirect_pending && !req_valid && !outstanding) begin
        pc <= restart_pc;
        redirect_pending <= 0;
      end
    end
  end
`ifndef SYNTHESIS
  assert property (@(posedge clk) disable iff (reset)
    req_valid && !req_ready |=> reset || (req_valid && $stable(req_addr)));
`endif
endmodule
