#ifndef DUAL_RETIREMENT_SCOREBOARD_H
#define DUAL_RETIREMENT_SCOREBOARD_H

#include "retirement_state.h"
#include <systemc>

class DualRetirementScoreboard : public sc_core::sc_module {
public:
    sc_core::sc_in<bool> clk{"clk"}, reset{"reset"};
    sc_core::sc_vector<sc_core::sc_in<bool>> valid{"valid", 2};
    sc_core::sc_vector<sc_core::sc_in<sc_dt::sc_uint<32>>> pc{"pc", 2}, value{"value", 2};
    sc_core::sc_vector<sc_core::sc_in<sc_dt::sc_uint<5>>> rd{"rd", 2};
    RetirementState state;

    SC_HAS_PROCESS(DualRetirementScoreboard);
    explicit DualRetirementScoreboard(sc_core::sc_module_name name) : sc_module(name) {
        SC_METHOD(tick);
        sensitive << clk.pos();
        dont_initialize();
    }

private:
    void tick() {
        if (reset.read()) { state.clear(); return; }
        if (valid[1].read() && !valid[0].read()) state.flag_protocol_error();
        for (unsigned lane = 0; lane < 2; ++lane)
            if (valid[lane].read())
                state.accept(pc[lane].read().to_uint(), rd[lane].read().to_uint(),
                             value[lane].read().to_uint());
    }
};

#endif
