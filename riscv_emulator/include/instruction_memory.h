#ifndef INSTRUCTION_MEMORY_H
#define INSTRUCTION_MEMORY_H

#include <systemc>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

class InstructionMemory : public sc_core::sc_module {
public:
    static constexpr std::size_t kCapacity = 4096;

    struct Timing {
        unsigned request_wait_cycles;
        unsigned response_latency_cycles;
    };

    static constexpr Timing normal_timing() { return Timing{0U, 1U}; }
    static constexpr Timing stalled_timing() { return Timing{2U, 3U}; }

    sc_core::sc_in<bool> clk{"clk"};
    sc_core::sc_in<bool> reset{"reset"};
    sc_core::sc_in<bool> req_valid{"req_valid"};
    sc_core::sc_out<bool> req_ready{"req_ready"};
    sc_core::sc_in<sc_dt::sc_uint<32>> req_addr{"req_addr"};
    sc_core::sc_out<bool> rsp_valid{"rsp_valid"};
    sc_core::sc_in<bool> rsp_ready{"rsp_ready"};
    sc_core::sc_out<sc_dt::sc_uint<32>> rsp_data{"rsp_data"};
    sc_core::sc_out<sc_dt::sc_uint<2>> rsp_status{"rsp_status"};

    SC_HAS_PROCESS(InstructionMemory);
    explicit InstructionMemory(sc_core::sc_module_name name,
                               Timing timing = normal_timing());

    void set_timing(Timing timing);
    void load_program(const std::uint8_t* bytes, std::size_t size);
    void load_program(const std::vector<std::uint8_t>& bytes) {
        load_program(bytes.data(), bytes.size());
    }

    template <std::size_t N>
    void load_program(const std::array<std::uint8_t, N>& bytes) {
        load_program(bytes.data(), bytes.size());
    }

    std::size_t program_size() const { return program_size_; }

private:
    enum class State { AcceptingRequest, WaitingForResponse, HoldingResponse };

    void tick();
    void reset_protocol();
    void begin_response();

    Timing timing_;
    std::array<std::uint8_t, kCapacity> memory_{};
    std::size_t program_size_{0};
    State state_{State::AcceptingRequest};
    unsigned request_wait_count_{0};
    unsigned response_cycles_remaining_{0};
    std::uint32_t pending_address_{0};
};

#endif
