A tiny RISC-V CPU workspace.

- `systemc_hello/`: minimal SystemC environment check
- `riscv_emulator/`: cycle-accurate RV32I/RV32M SystemC CPU, independent
  interpreter, reusable ready/valid instruction memory, and shared test catalog
- `rtl/`: single and dual issue in-order and dual issue out-of-order RTL CPUs, differential tests,
  and ADD/MUL waveform tests

All CPUs support `BEQ`, `BNE`, `BLT`, `BGE`, `BLTU`, and `BGEU`, assuming
always not taken. The existing cores block younger dispatch while continuing
fetch (option 2). `tiny5_dual_ooo_retire` permits speculative execution and
recovers at retirement (option 4). Both policies share the datapath; see
[RTL architecture](docs/rtl_architecture.md).

Run SystemC tests with `./riscv_emulator/run.sh` and RTL tests with `./rtl/run.sh`.
To build and run every SystemC, RTL, protocol, and waveform suite:

```sh
cmake -S . -B build -DBUILD_TESTING=ON -DTINY5_BUILD_RTL=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```
