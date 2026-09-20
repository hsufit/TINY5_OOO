#ifndef RETIREMENT_STATE_H
#define RETIREMENT_STATE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

struct RetirementEvent {
    std::uint32_t pc;
    unsigned rd;
    std::uint32_t value;
    bool operator==(const RetirementEvent& other) const {
        return pc == other.pc && rd == other.rd && value == other.value;
    }
};

// Clock-independent state shared by scalar and superscalar monitors.
class RetirementState {
public:
    void clear() {
        registers_.fill(0U);
        pcs_.clear();
        events_.clear();
        protocol_error_ = false;
    }
    void accept(std::uint32_t pc, unsigned rd, std::uint32_t value) {
        pcs_.push_back(pc);
        events_.push_back({pc, rd, value});
        if (rd >= registers_.size() || (rd == 0U && value != 0U)) {
            protocol_error_ = true;
        } else if (rd != 0U) {
            registers_[rd] = value;
        }
    }
    std::uint32_t reg(std::size_t index) const { return registers_.at(index); }
    std::size_t retirement_count() const { return events_.size(); }
    const std::vector<std::uint32_t>& retirement_pcs() const { return pcs_; }
    const std::vector<RetirementEvent>& events() const { return events_; }
    bool protocol_error() const { return protocol_error_; }
    void flag_protocol_error() { protocol_error_ = true; }

private:
    std::array<std::uint32_t, 32> registers_{};
    std::vector<std::uint32_t> pcs_;
    std::vector<RetirementEvent> events_;
    bool protocol_error_{false};
};

#endif
