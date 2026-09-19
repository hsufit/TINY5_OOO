#include "retirement_scoreboard.h"

#include <stdexcept>

RetirementScoreboard::RetirementScoreboard(sc_core::sc_module_name name)
    : sc_core::sc_module(name) {
    SC_METHOD(tick);
    sensitive << clk.pos();
    dont_initialize();
}

std::uint32_t RetirementScoreboard::reg(std::size_t index) const {
    if (index >= registers_.size()) {
        throw std::out_of_range("scoreboard register index is out of range");
    }
    return registers_[index];
}

void RetirementScoreboard::clear() {
    registers_.fill(0U);
    retirement_pcs_.clear();
    protocol_error_ = false;
}

void RetirementScoreboard::tick() {
    if (reset.read()) {
        clear();
        return;
    }
    if (!retire_valid.read()) {
        return;
    }

    const unsigned rd = retire_rd.read().to_uint();
    const std::uint32_t value = retire_value.read().to_uint();
    retirement_pcs_.push_back(retire_pc.read().to_uint());
    if (rd == 0U) {
        if (value != 0U) {
            protocol_error_ = true;
        }
    } else {
        registers_[rd] = value;
    }
    registers_[0] = 0U;
}
