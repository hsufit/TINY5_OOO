# SystemC RV32IM Arithmetic Emulator

This directory contains a cycle-accurate SystemC CPU for the arithmetic subset
of RV32I and RV32M, matching `tiny5_single_inorder`. There are no
branches, jumps, loads, stores, CSRs, traps, or system calls.

`Rv32imCpu` models the RTL frontend queues, operand dependencies, issue and
completion queues, registered ALU, multiply/divide timing, two writeback lanes,
and single-instruction retirement. Multiply/divide results are computed from
captured operands and exposed after the RTL's 32 iteration edges and result
publication edge. Independent ALU instructions can execute during those steps.
All stage transfers use pre-edge state; newly freed queue capacity is visible
on the following cycle.

The original interpreter is preserved as `Rv32imReferenceCpu` in a separate
test-reference library. It remains independent of the pipeline model.

## Reusable interfaces

The CPU does not own instruction memory. `InstructionMemory` is a separate
4 KiB SystemC module with a one-outstanding-request ready/valid interface. Its
program-loading API accepts byte arrays or vectors, assembles 32-bit words in
little-endian order, and distinguishes successful fetches, the exact end of a
complete program, and access faults. Reset clears protocol state but preserves
the loaded image.

The normal timing profile has no request delay while idle and returns a
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
retirement ports. The RTL differential runner gives each of the RTL CPU,
SystemC pipeline, and interpreter its own identically configured memory.
It compares RTL/SystemC fetch handshakes, valid payloads, retirement, and
termination every cycle, and checks architectural traces against the interpreter.

## ADD/MUL sequence and cycle counts

The SystemC catalog and `rtl_add_mul_waveform` share the same program bytes
through `rv32im_add_mul_test()`:

```asm
addi x1, x0, 6
addi x2, x0, 7
add  x3, x1, x2
mul  x4, x1, x2
add  x5, x3, x4
```

The test requires five retirements at PCs 0, 4, 8, 12, and 16, with
`x1=6, x2=7, x3=13, x4=42, x5=55`, then a normal halt. It runs with both
memory timing profiles. SystemC tests do not generate waveforms.

Cycle 1 is the first rising edge after reset release. Counts include the edge
that asserts halt or fault, and exclude reset and subsequent sticky-output
checks. With the current RTL, the shared sequence prints:

```text
[PASS] ADD/MUL sequence [normal] cycles=66 retired=5
[PASS] ADD/MUL sequence [stalled] cycles=82 retired=5
```

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
catalog test (including ADD/MUL) and the standalone memory-protocol test through
CTest, then prints the catalog results and cycle counts. Verilator is not
required for this standalone build.

For cycle comparison against RTL, with Verilator installed:

```sh
./rtl/run.sh
```

This also runs the existing RTL waveform test. To run all regressions from a
fresh build directory:

```sh
cmake -S . -B /tmp/tiny5-build -DBUILD_TESTING=ON -DTINY5_BUILD_RTL=ON
cmake --build /tmp/tiny5-build --parallel 2
ctest --test-dir /tmp/tiny5-build --output-on-failure
```

Using this repository's development container:

```sh
./run_docker.sh ./riscv_emulator/run.sh
```
