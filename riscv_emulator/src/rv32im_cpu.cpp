#include "rv32im_cpu.h"

#include "cpu_protocol.h"

#include <cstring>
#include <limits>

namespace {

std::int32_t signed_value(std::uint32_t value) {
    std::int32_t result = 0;
    static_assert(sizeof(result) == sizeof(value), "RV32 requires 32-bit integers");
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

std::int32_t sign_extend(std::uint32_t value, unsigned width) {
    const std::uint32_t sign_bit = std::uint32_t{1} << (width - 1U);
    return signed_value((value ^ sign_bit) - sign_bit);
}

std::uint32_t arithmetic_shift_right(std::uint32_t value, unsigned amount) {
    if (amount == 0U) {
        return value;
    }

    const std::uint32_t shifted = value >> amount;
    if ((value & 0x80000000U) == 0U) {
        return shifted;
    }
    return shifted | (~std::uint32_t{0} << (32U - amount));
}

}  // namespace

Rv32imCpu::Rv32imCpu(sc_core::sc_module_name name) : sc_core::sc_module(name) {
    SC_METHOD(tick);
    sensitive << clk.pos();
    dont_initialize();
}

void Rv32imCpu::reset_state() {
    registers_.fill(0);
    pc_ = 0;
    state_ = State::IssueRequest;
    imem_req_valid.write(false);
    imem_req_addr.write(0U);
    imem_rsp_ready.write(false);
    retire_valid.write(false);
    retire_pc.write(0U);
    retire_rd.write(0U);
    retire_value.write(0U);
    halted.write(false);
    fault.write(false);
    fault_code.write(static_cast<unsigned>(CpuFaultCode::NONE));
}

void Rv32imCpu::raise_fault(unsigned code) {
    fault.write(true);
    fault_code.write(code);
    imem_req_valid.write(false);
    imem_rsp_ready.write(false);
    state_ = State::Stopped;
}

void Rv32imCpu::tick() {
    if (reset.read()) {
        reset_state();
        return;
    }
    retire_valid.write(false);

    switch (state_) {
    case State::IssueRequest:
        imem_req_addr.write(pc_);
        imem_req_valid.write(true);
        state_ = State::WaitForRequest;
        break;

    case State::WaitForRequest:
        if (imem_req_valid.read() && imem_req_ready.read()) {
            imem_req_valid.write(false);
            imem_rsp_ready.write(true);
            state_ = State::WaitForResponse;
        }
        break;

    case State::WaitForResponse:
        if (imem_rsp_valid.read() && imem_rsp_ready.read()) {
            imem_rsp_ready.write(false);
            const auto status = static_cast<InstructionResponseStatus>(
                imem_rsp_status.read().to_uint());
            if (status == InstructionResponseStatus::END_OF_PROGRAM) {
                halted.write(true);
                state_ = State::Stopped;
            } else if (status != InstructionResponseStatus::OK) {
                raise_fault(static_cast<unsigned>(CpuFaultCode::INSTRUCTION_ACCESS_FAULT));
            } else {
                unsigned rd = 0U;
                std::uint32_t value = 0U;
                if (!execute(imem_rsp_data.read().to_uint(), rd, value)) {
                    raise_fault(static_cast<unsigned>(CpuFaultCode::ILLEGAL_INSTRUCTION));
                } else {
                    retire_pc.write(pc_);
                    retire_rd.write(rd);
                    retire_value.write(value);
                    retire_valid.write(true);
                    pc_ += 4U;
                    state_ = State::IssueRequest;
                }
            }
        }
        break;

    case State::Stopped:
        break;
    }
}

bool Rv32imCpu::execute(std::uint32_t instruction, unsigned& retired_rd,
                        std::uint32_t& retired_value) {
    const std::uint32_t opcode = instruction & 0x7fU;
    const unsigned rd = (instruction >> 7U) & 0x1fU;
    const unsigned funct3 = (instruction >> 12U) & 0x7U;
    const unsigned rs1 = (instruction >> 15U) & 0x1fU;
    const unsigned rs2 = (instruction >> 20U) & 0x1fU;
    const unsigned funct7 = (instruction >> 25U) & 0x7fU;

    if (opcode == 0x13U) {  // RV32I OP-IMM
        const std::int32_t immediate = sign_extend(instruction >> 20U, 12U);
        const std::uint32_t lhs = registers_[rs1];
        std::uint32_t result = 0;

        switch (funct3) {
        case 0x0U:  // ADDI
            result = lhs + static_cast<std::uint32_t>(immediate);
            break;
        case 0x2U:  // SLTI
            result = signed_value(lhs) < immediate ? 1U : 0U;
            break;
        case 0x3U:  // SLTIU
            result = lhs < static_cast<std::uint32_t>(immediate) ? 1U : 0U;
            break;
        case 0x4U:  // XORI
            result = lhs ^ static_cast<std::uint32_t>(immediate);
            break;
        case 0x6U:  // ORI
            result = lhs | static_cast<std::uint32_t>(immediate);
            break;
        case 0x7U:  // ANDI
            result = lhs & static_cast<std::uint32_t>(immediate);
            break;
        case 0x1U: {  // SLLI
            if (funct7 != 0x00U) {
                return false;
            }
            const unsigned shift = (instruction >> 20U) & 0x1fU;
            result = lhs << shift;
            break;
        }
        case 0x5U: {  // SRLI/SRAI
            const unsigned shift = (instruction >> 20U) & 0x1fU;
            if (funct7 == 0x00U) {
                result = lhs >> shift;
            } else if (funct7 == 0x20U) {
                result = arithmetic_shift_right(lhs, shift);
            } else {
                return false;
            }
            break;
        }
        default:
            return false;
        }

        registers_[rd] = result;
        registers_[0] = 0U;
        retired_rd = rd;
        retired_value = registers_[rd];
        return true;
    }

    if (opcode != 0x33U) {  // RV32I OP or RV32M OP
        return false;
    }

    const std::uint32_t lhs = registers_[rs1];
    const std::uint32_t rhs = registers_[rs2];
    std::uint32_t result = 0;

    if (funct7 == 0x01U) {  // RV32M
        const std::int32_t signed_lhs = signed_value(lhs);
        const std::int32_t signed_rhs = signed_value(rhs);
        switch (funct3) {
        case 0x0U:  // MUL
            result = static_cast<std::uint32_t>(static_cast<std::uint64_t>(lhs) * rhs);
            break;
        case 0x1U: {  // MULH
            const std::int64_t product = static_cast<std::int64_t>(signed_lhs) * signed_rhs;
            result = static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32U);
            break;
        }
        case 0x2U: {  // MULHSU
            const std::int64_t product =
                static_cast<std::int64_t>(signed_lhs) * static_cast<std::int64_t>(rhs);
            result = static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32U);
            break;
        }
        case 0x3U: {  // MULHU
            const std::uint64_t product = static_cast<std::uint64_t>(lhs) * rhs;
            result = static_cast<std::uint32_t>(product >> 32U);
            break;
        }
        case 0x4U:  // DIV
            if (rhs == 0U) {
                result = std::numeric_limits<std::uint32_t>::max();
            } else if (signed_lhs == std::numeric_limits<std::int32_t>::min() &&
                       signed_rhs == -1) {
                result = lhs;
            } else {
                result = static_cast<std::uint32_t>(signed_lhs / signed_rhs);
            }
            break;
        case 0x5U:  // DIVU
            result = rhs == 0U ? std::numeric_limits<std::uint32_t>::max() : lhs / rhs;
            break;
        case 0x6U:  // REM
            if (rhs == 0U) {
                result = lhs;
            } else if (signed_lhs == std::numeric_limits<std::int32_t>::min() &&
                       signed_rhs == -1) {
                result = 0;
            } else {
                result = static_cast<std::uint32_t>(signed_lhs % signed_rhs);
            }
            break;
        case 0x7U:  // REMU
            result = rhs == 0U ? lhs : lhs % rhs;
            break;
        default:
            return false;
        }

        registers_[rd] = result;
        registers_[0] = 0U;
        retired_rd = rd;
        retired_value = registers_[rd];
        return true;
    }

    switch (funct3) {  // RV32I register-register arithmetic
    case 0x0U:
        if (funct7 == 0x00U) {
            result = lhs + rhs;  // ADD
        } else if (funct7 == 0x20U) {
            result = lhs - rhs;  // SUB
        } else {
            return false;
        }
        break;
    case 0x1U:  // SLL
        if (funct7 != 0x00U) {
            return false;
        }
        result = lhs << (rhs & 0x1fU);
        break;
    case 0x2U:  // SLT
        if (funct7 != 0x00U) {
            return false;
        }
        result = signed_value(lhs) < signed_value(rhs) ? 1U : 0U;
        break;
    case 0x3U:  // SLTU
        if (funct7 != 0x00U) {
            return false;
        }
        result = lhs < rhs ? 1U : 0U;
        break;
    case 0x4U:  // XOR
        if (funct7 != 0x00U) {
            return false;
        }
        result = lhs ^ rhs;
        break;
    case 0x5U:
        if (funct7 == 0x00U) {
            result = lhs >> (rhs & 0x1fU);  // SRL
        } else if (funct7 == 0x20U) {
            result = arithmetic_shift_right(lhs, rhs & 0x1fU);  // SRA
        } else {
            return false;
        }
        break;
    case 0x6U:  // OR
        if (funct7 != 0x00U) {
            return false;
        }
        result = lhs | rhs;
        break;
    case 0x7U:  // AND
        if (funct7 != 0x00U) {
            return false;
        }
        result = lhs & rhs;
        break;
    default:
        return false;
    }

    registers_[rd] = result;
    registers_[0] = 0U;
    retired_rd = rd;
    retired_value = registers_[rd];
    return true;
}
