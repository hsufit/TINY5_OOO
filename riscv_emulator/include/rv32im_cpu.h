#ifndef RV32IM_CPU_H
#define RV32IM_CPU_H

#include <systemc>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

class Rv32imCpu : public sc_core::sc_module {
public:
    static constexpr std::size_t kMemorySize = 4096;
    static constexpr std::size_t kRegisterCount = 32;

    sc_core::sc_in<bool> clk{"clk"};
    sc_core::sc_in<bool> reset{"reset"};
    sc_core::sc_out<bool> halted{"halted"};
    sc_core::sc_out<bool> fault{"fault"};

    SC_HAS_PROCESS(Rv32imCpu);
    explicit Rv32imCpu(sc_core::sc_module_name name);

    void load_program(const std::uint8_t* bytes, std::size_t size);

    template <std::size_t N>
    void load_program(const std::array<std::uint8_t, N>& bytes) {
        load_program(bytes.data(), bytes.size());
    }

    std::uint32_t reg(std::size_t index) const;
    std::uint32_t pc() const { return pc_; }
    std::uint64_t retired_instructions() const { return retired_instructions_; }
    const std::string& fault_message() const { return fault_message_; }

private:
    void tick();
    void reset_state();
    bool execute(std::uint32_t instruction);
    void raise_fault(const std::string& message);
    std::uint32_t fetch_word(std::uint32_t address) const;

    std::array<std::uint8_t, kMemorySize> memory_{};
    std::array<std::uint32_t, kRegisterCount> registers_{};
    std::size_t program_size_{0};
    std::uint32_t pc_{0};
    std::uint64_t retired_instructions_{0};
    bool halted_state_{false};
    bool fault_state_{false};
    std::string fault_message_;
};

#endif
