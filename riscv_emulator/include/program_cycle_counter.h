#ifndef PROGRAM_CYCLE_COUNTER_H
#define PROGRAM_CYCLE_COUNTER_H

#include <cstdint>

// Sample at each falling edge following a non-reset rising edge. Include the
// terminating edge, then freeze while other CPUs finish or sticky checks run.
class ProgramCycleCounter {
public:
    void reset() { cycles_ = 0; stopped_ = false; }
    void sample(bool terminated) {
        if (!stopped_) {
            ++cycles_;
            stopped_ = terminated;
        }
    }
    std::uint64_t cycles() const { return cycles_; }
    bool stopped() const { return stopped_; }

private:
    std::uint64_t cycles_{};
    bool stopped_{};
};

#endif
