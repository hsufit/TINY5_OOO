# SystemC RV32IM Arithmetic Emulator

This directory contains a small, clocked SystemC emulator for the arithmetic
part of the 32-bit RISC-V I and M extensions. It is intentionally not a full
RISC-V machine: there are no branches, jumps, loads, stores, CSRs, traps, or
system calls.

## Supported instructions

- RV32I immediate: `ADDI`, `SLTI`, `SLTIU`, `XORI`, `ORI`, `ANDI`, `SLLI`,
  `SRLI`, `SRAI`
- RV32I register: `ADD`, `SUB`, `SLL`, `SLT`, `SLTU`, `XOR`, `SRL`, `SRA`,
  `OR`, `AND`
- RV32M: `MUL`, `MULH`, `MULHSU`, `MULHU`, `DIV`, `DIVU`, `REM`, `REMU`

The CPU executes one instruction on each rising clock edge. `x0` is always
zero. A program halts successfully when the PC reaches the byte immediately
after the loaded image. Unsupported or incomplete instructions set the
`fault` output and provide a message through `fault_message()`.

## Fake instruction memory

Programs are ordinary little-endian byte arrays. The tests use arrays like:

```cpp
constexpr std::array<std::uint8_t, 12> program{
    0x93, 0x00, 0xc0, 0x00,  // addi x1, x0, 12
    0x13, 0x01, 0xc0, 0xff,  // addi x2, x0, -4
    0xb3, 0x81, 0x20, 0x02,  // mul  x3, x1, x2
};

cpu.load_program(program);
```

This produces `x3 = 0xffffffd0` (-48). The full self-checking test programs
are in `tests/rv32im_tests.cpp`; all their initial operand values are created
by `ADDI` (and, for `INT32_MIN`, an arithmetic shift after `ADDI`).

## Build and run

On a host with CMake, a C++17 compiler, and SystemC installed:

```sh
./riscv_emulator/run.sh
```

Or, using this repository's development container:

```sh
./run_docker.sh ./riscv_emulator/run.sh
```
