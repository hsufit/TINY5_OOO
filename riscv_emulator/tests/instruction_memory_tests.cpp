#include "cpu_protocol.h"
#include "instruction_memory.h"

#include <systemc>

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct Response {
    std::uint32_t data;
    InstructionResponseStatus status;
};

class InstructionMemoryTestbench : public sc_core::sc_module {
public:
    sc_core::sc_clock clock{"clock", 10, sc_core::SC_NS};
    sc_core::sc_signal<bool> reset{"reset"};
    sc_core::sc_signal<bool> req_valid{"req_valid"};
    sc_core::sc_signal<bool> req_ready{"req_ready"};
    sc_core::sc_signal<sc_dt::sc_uint<32>> req_addr{"req_addr"};
    sc_core::sc_signal<bool> rsp_valid{"rsp_valid"};
    sc_core::sc_signal<bool> rsp_ready{"rsp_ready"};
    sc_core::sc_signal<sc_dt::sc_uint<32>> rsp_data{"rsp_data"};
    sc_core::sc_signal<sc_dt::sc_uint<2>> rsp_status{"rsp_status"};
    InstructionMemory memory{"memory"};

    SC_HAS_PROCESS(InstructionMemoryTestbench);
    explicit InstructionMemoryTestbench(sc_core::sc_module_name name)
        : sc_core::sc_module(name) {
        memory.clk(clock);
        memory.reset(reset);
        memory.req_valid(req_valid);
        memory.req_ready(req_ready);
        memory.req_addr(req_addr);
        memory.rsp_valid(rsp_valid);
        memory.rsp_ready(rsp_ready);
        memory.rsp_data(rsp_data);
        memory.rsp_status(rsp_status);
        SC_THREAD(run);
    }

    unsigned failures() const { return failures_; }

private:
    void fail(const std::string& name, const std::string& reason) {
        std::cerr << "[FAIL] " << name << ": " << reason << '\n';
        ++failures_;
    }

    void begin(const std::vector<std::uint8_t>& program,
               InstructionMemory::Timing timing = InstructionMemory::normal_timing(),
               bool reload = true) {
        req_valid.write(false);
        rsp_ready.write(false);
        reset.write(true);
        memory.set_timing(timing);
        if (reload) {
            memory.load_program(program);
        }
        wait(clock.posedge_event());
        wait(clock.negedge_event());
        reset.write(false);
    }

    unsigned issue_request(std::uint32_t address) {
        req_addr.write(address);
        req_valid.write(true);
        unsigned wait_cycles = 0U;
        while (!req_ready.read()) {
            wait(clock.posedge_event());
            wait(clock.negedge_event());
            ++wait_cycles;
        }
        wait(clock.posedge_event());
        wait(clock.negedge_event());
        req_valid.write(false);
        return wait_cycles;
    }

    unsigned wait_for_response() {
        unsigned cycles = 0U;
        while (!rsp_valid.read()) {
            wait(clock.posedge_event());
            wait(clock.negedge_event());
            ++cycles;
        }
        return cycles;
    }

    Response consume_response() {
        const Response response{
            rsp_data.read().to_uint(),
            static_cast<InstructionResponseStatus>(rsp_status.read().to_uint())};
        rsp_ready.write(true);
        wait(clock.posedge_event());
        wait(clock.negedge_event());
        rsp_ready.write(false);
        return response;
    }

    Response transact(std::uint32_t address) {
        (void)issue_request(address);
        (void)wait_for_response();
        return consume_response();
    }

    void expect_status(const std::string& name, std::uint32_t address,
                       InstructionResponseStatus expected) {
        const Response response = transact(address);
        if (response.status != expected) {
            fail(name, "incorrect response status");
        } else {
            std::cout << "[PASS] " << name << '\n';
        }
    }

    void run() {
        const std::vector<std::uint8_t> words{
            0x78, 0x56, 0x34, 0x12, 0xef, 0xcd, 0xab, 0x90};
        begin(words);
        Response response = transact(0U);
        if (response.status != InstructionResponseStatus::OK ||
            response.data != 0x12345678U) {
            fail("little-endian assembly", "incorrect word response");
        } else {
            std::cout << "[PASS] little-endian assembly\n";
        }
        expect_status("exact end of program", 8U,
                      InstructionResponseStatus::END_OF_PROGRAM);

        begin(words, InstructionMemory::stalled_timing());
        const unsigned request_wait = issue_request(4U);
        if (request_wait != 2U) {
            fail("request backpressure", "request was not delayed by two cycles");
        } else {
            std::cout << "[PASS] request backpressure\n";
        }
        const unsigned response_wait = wait_for_response();
        if (response_wait != 3U) {
            fail("delayed response", "response was not delayed by three cycles");
        } else {
            std::cout << "[PASS] delayed response\n";
        }

        const std::uint32_t held_data = rsp_data.read().to_uint();
        const unsigned held_status = rsp_status.read().to_uint();
        wait(clock.posedge_event());
        wait(clock.posedge_event());
        wait(clock.negedge_event());
        if (!rsp_valid.read() || rsp_data.read().to_uint() != held_data ||
            rsp_status.read().to_uint() != held_status) {
            fail("response backpressure", "valid response payload changed while blocked");
        } else {
            std::cout << "[PASS] response backpressure\n";
        }
        response = consume_response();
        if (response.status != InstructionResponseStatus::OK ||
            response.data != 0x90abcdefU) {
            fail("stalled response payload", "incorrect held response");
        }

        begin({});
        expect_status("empty program", 0U, InstructionResponseStatus::END_OF_PROGRAM);

        begin({0x93, 0x00, 0xa0});
        expect_status("truncated program", 0U,
                      InstructionResponseStatus::ACCESS_FAULT);

        begin(words);
        expect_status("misaligned address", 2U,
                      InstructionResponseStatus::ACCESS_FAULT);
        expect_status("out-of-range address", 0xfffffffcU,
                      InstructionResponseStatus::ACCESS_FAULT);

        begin({0x01, 0x02, 0x03, 0x04, 0x05, 0x06});
        expect_status("incomplete four-byte fetch", 4U,
                      InstructionResponseStatus::ACCESS_FAULT);

        begin(words);
        response = transact(0U);
        begin({}, InstructionMemory::normal_timing(), false);
        const Response retained = transact(0U);
        if (response.status != InstructionResponseStatus::OK ||
            retained.status != InstructionResponseStatus::OK ||
            retained.data != response.data) {
            fail("reset retains program", "program bytes changed across reset");
        } else {
            std::cout << "[PASS] reset retains program\n";
        }

        if (failures_ == 0U) {
            std::cout << "All instruction-memory protocol tests passed\n";
        } else {
            std::cerr << failures_ << " instruction-memory protocol test(s) failed\n";
        }
        sc_core::sc_stop();
    }

    unsigned failures_{0};
};

}  // namespace

int sc_main(int, char*[]) {
    InstructionMemoryTestbench testbench("testbench");
    sc_core::sc_start();
    return testbench.failures() == 0U ? 0 : 1;
}
