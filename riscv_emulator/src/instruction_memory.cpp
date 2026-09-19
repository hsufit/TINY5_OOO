#include "instruction_memory.h"

#include "cpu_protocol.h"

#include <algorithm>
#include <stdexcept>

namespace {

sc_dt::sc_uint<2> status_value(InstructionResponseStatus status) {
    return static_cast<unsigned>(status);
}

}  // namespace

InstructionMemory::InstructionMemory(sc_core::sc_module_name name, Timing timing)
    : sc_core::sc_module(name), timing_(timing) {
    if (timing_.response_latency_cycles == 0U) {
        throw std::invalid_argument("instruction-memory response latency must be nonzero");
    }
    SC_METHOD(tick);
    sensitive << clk.pos();
    dont_initialize();
}

void InstructionMemory::set_timing(Timing timing) {
    if (timing.response_latency_cycles == 0U) {
        throw std::invalid_argument("instruction-memory response latency must be nonzero");
    }
    timing_ = timing;
}

void InstructionMemory::load_program(const std::uint8_t* bytes, std::size_t size) {
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

void InstructionMemory::reset_protocol() {
    state_ = State::AcceptingRequest;
    request_wait_count_ = 0U;
    response_cycles_remaining_ = 0U;
    pending_address_ = 0U;
    req_ready.write(timing_.request_wait_cycles == 0U);
    rsp_valid.write(false);
    rsp_data.write(0U);
    rsp_status.write(status_value(InstructionResponseStatus::RESERVED));
}

void InstructionMemory::begin_response() {
    const std::uint64_t address = pending_address_;
    InstructionResponseStatus status = InstructionResponseStatus::ACCESS_FAULT;
    std::uint32_t data = 0U;

    if ((address & 0x3U) == 0U && address == program_size_) {
        status = InstructionResponseStatus::END_OF_PROGRAM;
    } else if ((address & 0x3U) == 0U && address < program_size_ &&
               address + sizeof(std::uint32_t) <= program_size_) {
        const std::size_t base = static_cast<std::size_t>(address);
        data = static_cast<std::uint32_t>(memory_[base]) |
               (static_cast<std::uint32_t>(memory_[base + 1U]) << 8U) |
               (static_cast<std::uint32_t>(memory_[base + 2U]) << 16U) |
               (static_cast<std::uint32_t>(memory_[base + 3U]) << 24U);
        status = InstructionResponseStatus::OK;
    }

    rsp_data.write(data);
    rsp_status.write(status_value(status));
    rsp_valid.write(true);
    state_ = State::HoldingResponse;
}

void InstructionMemory::tick() {
    if (reset.read()) {
        reset_protocol();
        return;
    }

    switch (state_) {
    case State::AcceptingRequest:
        rsp_valid.write(false);
        if (timing_.request_wait_cycles == 0U) {
            req_ready.write(true);
        } else if (!req_valid.read()) {
            request_wait_count_ = 0U;
            req_ready.write(false);
        } else if (request_wait_count_ < timing_.request_wait_cycles) {
            ++request_wait_count_;
            req_ready.write(request_wait_count_ == timing_.request_wait_cycles);
        }

        if (req_valid.read() && req_ready.read()) {
            pending_address_ = req_addr.read().to_uint();
            req_ready.write(false);
            response_cycles_remaining_ = timing_.response_latency_cycles;
            state_ = State::WaitingForResponse;
        }
        break;

    case State::WaitingForResponse:
        req_ready.write(false);
        if (response_cycles_remaining_ > 1U) {
            --response_cycles_remaining_;
        } else {
            response_cycles_remaining_ = 0U;
            begin_response();
        }
        break;

    case State::HoldingResponse:
        req_ready.write(false);
        if (rsp_valid.read() && rsp_ready.read()) {
            rsp_valid.write(false);
            state_ = State::AcceptingRequest;
            request_wait_count_ = 0U;
            req_ready.write(timing_.request_wait_cycles == 0U);
        }
        break;
    }
}
