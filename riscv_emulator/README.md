# SystemC RV32IM Arithmetic Emulator

This directory contains a clocked reference CPU for the arithmetic subset of
RV32I and RV32M. It is intentionally not a full RISC-V machine: there are no
branches, jumps, loads, stores, CSRs, traps, or system calls.

## Reusable interfaces

The CPU no longer owns instruction memory. `InstructionMemory` is a separate
4 KiB SystemC module with a one-outstanding-request ready/valid interface. Its
program-loading API accepts byte arrays or vectors, assembles 32-bit words in
little-endian order, and distinguishes successful fetches, the exact end of a
complete program, and access faults. Reset clears protocol state but preserves
the loaded image.

The normal timing profile keeps the request channel ready and returns a
response one cycle after acceptance. The stalled profile waits two request
cycles and returns the response after three cycles. A valid response remains
stable until accepted.

The CPU exposes the same fetch interface plus CPU-neutral retirement and
termination ports:

```text
imem_req_valid/ready, imem_req_addr
imem_rsp_valid/ready, imem_rsp_data, imem_rsp_status
retire_valid, retire_pc, retire_rd, retire_value
halted, fault, fault_code
```

Instruction responses use `OK=0`, `END_OF_PROGRAM=1`, `ACCESS_FAULT=2`, and
`RESERVED=3`. CPU faults use `NONE=0`, `ILLEGAL_INSTRUCTION=1`,
`INSTRUCTION_ACCESS_FAULT=2`, and `RESERVED=3`.

`halted`, `fault`, and `fault_code` remain stable until reset. Every successful
instruction emits one retirement event; an instruction targeting `x0` emits
register zero and value zero.

The test programs and typed expectations live in `test_catalog.cpp`. The
`RetirementScoreboard` reconstructs all architectural registers solely from
retirement ports. Consequently, the testbench does not inspect CPU internals
and can be reused when a Verilated SystemVerilog CPU implements these ports.
The `instruction_memory` and `rv32im_test_support` CMake libraries can likewise
be linked into that future runner.

## Supported instructions

- RV32I immediate: `ADDI`, `SLTI`, `SLTIU`, `XORI`, `ORI`, `ANDI`, `SLLI`,
  `SRLI`, `SRAI`
- RV32I register: `ADD`, `SUB`, `SLL`, `SLT`, `SLTU`, `XOR`, `SRL`, `SRA`,
  `OR`, `AND`
- RV32M: `MUL`, `MULH`, `MULHSU`, `MULHU`, `DIV`, `DIVU`, `REM`, `REMU`

## Build and run

On a host with CMake, a C++17 compiler, and SystemC installed:

```sh
./riscv_emulator/run.sh
```

This builds with `-Wall -Wextra -Wpedantic -Werror` and runs both the CPU
catalog test and the standalone memory-protocol test through CTest.

Using this repository's development container:

```sh
./run_docker.sh ./riscv_emulator/run.sh
```
