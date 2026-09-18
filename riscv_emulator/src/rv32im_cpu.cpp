#include "rv32im_cpu.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

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

void Rv32imCpu::load_program(const std::uint8_t* bytes, std::size_t size) {
    if (size > memory_.size()) {
        throw std::invalid_argument("program does not fit in instruction memory");
    }
    if (size != 0U && bytes == nullptr) {
        throw std::invalid_argument("non-empty program has a null byte pointer");
    }

    memory_.fill(0);
    if (size != 0U) {
        std::copy_n(bytes, size, memory_.begin());
    }
    program_size_ = size;
}

std::uint32_t Rv32imCpu::reg(std::size_t index) const {
    if (index >= registers_.size()) {
        throw std::out_of_range("RISC-V register index is out of range");
    }
    return registers_[index];
}

void Rv32imCpu::reset_state() {
    registers_.fill(0);
    pc_ = 0;
    retired_instructions_ = 0;
    halted_state_ = false;
    fault_state_ = false;
    fault_message_.clear();
    halted.write(false);
    fault.write(false);
}

std::uint32_t Rv32imCpu::fetch_word(std::uint32_t address) const {
    const std::size_t base = address;
    return static_cast<std::uint32_t>(memory_[base]) |
           (static_cast<std::uint32_t>(memory_[base + 1U]) << 8U) |
           (static_cast<std::uint32_t>(memory_[base + 2U]) << 16U) |
           (static_cast<std::uint32_t>(memory_[base + 3U]) << 24U);
}

void Rv32imCpu::raise_fault(const std::string& message) {
    fault_state_ = true;
    fault_message_ = message;
    fault.write(true);
}

void Rv32imCpu::tick() {
    if (reset.read()) {
        reset_state();
        return;
    }
    if (halted_state_ || fault_state_) {
        return;
    }
    if (pc_ == program_size_) {
        halted_state_ = true;
        halted.write(true);
        return;
    }
    if ((pc_ & 0x3U) != 0U || pc_ > program_size_ ||
        program_size_ - pc_ < sizeof(std::uint32_t)) {
        std::ostringstream message;
        message << "truncated or misaligned instruction at PC 0x" << std::hex << pc_;
        raise_fault(message.str());
        return;
    }

    const std::uint32_t instruction = fetch_word(pc_);
    if (!execute(instruction)) {
        std::ostringstream message;
        message << "unsupported instruction 0x" << std::hex << std::setw(8)
                << std::setfill('0') << instruction << " at PC 0x" << pc_;
        raise_fault(message.str());
        return;
    }

    registers_[0] = 0;
    pc_ += 4U;
    ++retired_instructions_;
    if (pc_ == program_size_) {
        halted_state_ = true;
        halted.write(true);
    }
}

bool Rv32imCpu::execute(std::uint32_t instruction) {
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
    return true;
}
