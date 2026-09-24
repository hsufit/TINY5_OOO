A tiny RISC-V CPU workspace.

- `systemc_hello/`: minimal SystemC environment check
- `riscv_emulator/`: cycle-accurate RV32I/RV32M SystemC CPU, independent
  interpreter, reusable ready/valid instruction memory, and shared test catalog
- `rtl/`: single-issue in-order RTL CPU, cycle-by-cycle differential tests,
  and the ADD/MUL waveform test
