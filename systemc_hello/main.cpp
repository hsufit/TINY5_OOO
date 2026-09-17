#include <systemc>

#include <iostream>

SC_MODULE(HelloWorld) {
    SC_CTOR(HelloWorld) {
        SC_THREAD(greet);
    }

    void greet() {
        std::cout << sc_core::sc_time_stamp() << ": Hello, SystemC!\n";
        sc_core::wait(10, sc_core::SC_NS);
        std::cout << sc_core::sc_time_stamp() << ": Hello, SystemC!\n";
        sc_core::sc_stop();
    }
};

int sc_main(int, char*[]) {
    HelloWorld hello("hello");
    sc_core::sc_start();

    if (sc_core::sc_time_stamp() != sc_core::sc_time(10, sc_core::SC_NS)) {
        std::cerr << "Simulation did not reach the expected time.\n";
        return 1;
    }
    return 0;
}
