#ifndef CPU_PROTOCOL_H
#define CPU_PROTOCOL_H

#include <cstdint>

enum class InstructionResponseStatus : std::uint8_t {
    OK = 0,
    END_OF_PROGRAM = 1,
    ACCESS_FAULT = 2,
    RESERVED = 3,
};

enum class CpuFaultCode : std::uint8_t {
    NONE = 0,
    ILLEGAL_INSTRUCTION = 1,
    INSTRUCTION_ACCESS_FAULT = 2,
    RESERVED = 3,
};

#endif
