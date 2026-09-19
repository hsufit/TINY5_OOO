#ifndef RETIREMENT_SCOREBOARD_H
#define RETIREMENT_SCOREBOARD_H

#include <systemc>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

class RetirementScoreboard : public sc_core::sc_module {
public:
    static constexpr std::size_t kRegisterCount = 32;

    sc_core::sc_in<bool> clk{"clk"};
    sc_core::sc_in<bool> reset{"reset"};
    sc_core::sc_in<bool> retire_valid{"retire_valid"};
    sc_core::sc_in<sc_dt::sc_uint<32>> retire_pc{"retire_pc"};
    sc_core::sc_in<sc_dt::sc_uint<5>> retire_rd{"retire_rd"};
    sc_core::sc_in<sc_dt::sc_uint<32>> retire_value{"retire_value"};

    SC_HAS_PROCESS(RetirementScoreboard);
    explicit RetirementScoreboard(sc_core::sc_module_name name);

    std::uint32_t reg(std::size_t index) const;
    std::size_t retirement_count() const { return retirement_pcs_.size(); }
    const std::vector<std::uint32_t>& retirement_pcs() const { return retirement_pcs_; }
    bool protocol_error() const { return protocol_error_; }

private:
    void tick();
    void clear();

    std::array<std::uint32_t, kRegisterCount> registers_{};
    std::vector<std::uint32_t> retirement_pcs_;
    bool protocol_error_{false};
};

#endif
