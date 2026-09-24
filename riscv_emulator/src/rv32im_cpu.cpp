#include "rv32im_cpu.h"

#include "cpu_protocol.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <deque>
#include <limits>

namespace {

enum class Op {
    Add, Sub, Sll, Slt, Sltu, Xor, Srl, Sra, Or, And,
    Mul, Mulh, Mulhsu, Mulhu, Div, Divu, Rem, Remu
};
bool muldiv(Op op) { return op >= Op::Mul; }

struct Fetch {
    std::uint32_t pc{}, instruction{};
    unsigned status{};
};
struct Decoded {
    std::uint32_t pc{}, immediate{};
    Op op{Op::Add};
    unsigned rs1{}, rs2{}, rd{}, fault{};
    bool immediate_b{}, terminal{};
};
struct Meta {
    std::uint32_t pc{};
    unsigned rd{}, tag{};
};
struct Operand {
    bool ready{};
    unsigned reg{};
    std::uint32_t value{};
};
struct Issue {
    bool valid{};
    Meta meta{};
    Op op{Op::Add};
    Operand a{}, b{};
};
struct Execute {
    Meta meta{};
    Op op{Op::Add};
    std::uint32_t a{}, b{};
};
struct Result {
    Meta meta{};
    std::uint32_t value{};
};
template<class T> struct Slot {
    bool valid{};
    T data{};
};
struct Completion {
    bool valid{}, done{}, terminal{};
    unsigned fault{};
    Result result{};
};

std::int32_t signed_value(std::uint32_t value) {
    std::int32_t result;
    static_assert(sizeof(result) == sizeof(value), "RV32 requires 32-bit integers");
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

Decoded decode(const Fetch& fetched) {
    Decoded d;
    const auto word = fetched.instruction;
    const unsigned opcode = word & 127U;
    const unsigned f3 = (word >> 12U) & 7U;
    const unsigned f7 = word >> 25U;
    d.pc = fetched.pc;
    d.rd = (word >> 7U) & 31U;
    d.rs1 = (word >> 15U) & 31U;
    d.rs2 = (word >> 20U) & 31U;
    d.immediate = word >> 20U;
    if ((d.immediate & 0x800U) != 0U) d.immediate |= 0xfffff000U;
    bool legal = true;
    constexpr std::array<Op, 8> integer_ops{
        Op::Add, Op::Sll, Op::Slt, Op::Sltu, Op::Xor, Op::Srl, Op::Or, Op::And};
    constexpr std::array<Op, 8> m_ops{
        Op::Mul, Op::Mulh, Op::Mulhsu, Op::Mulhu, Op::Div, Op::Divu, Op::Rem, Op::Remu};
    if (opcode == 0x13U) {
        d.immediate_b = true;
        d.rs2 = 0;
        d.op = integer_ops[f3];
        if (f3 == 1U) legal = f7 == 0U;
        if (f3 == 5U) {
            legal = f7 == 0U || f7 == 32U;
            if (f7 == 32U) d.op = Op::Sra;
        }
    } else if (opcode == 0x33U) {
        if (f7 == 1U) d.op = m_ops[f3];
        else {
            d.op = integer_ops[f3];
            legal = f7 == 0U;
            if ((f3 == 0U || f3 == 5U) && f7 == 32U) {
                legal = true;
                d.op = f3 == 0U ? Op::Sub : Op::Sra;
            }
        }
    } else legal = false;

    const auto status = static_cast<InstructionResponseStatus>(fetched.status);
    if (status != InstructionResponseStatus::OK || !legal) {
        d.terminal = true;
        d.rd = d.rs1 = d.rs2 = 0;
        if (status == InstructionResponseStatus::END_OF_PROGRAM) d.fault = 0;
        else if (status != InstructionResponseStatus::OK)
            d.fault = static_cast<unsigned>(CpuFaultCode::INSTRUCTION_ACCESS_FAULT);
        else d.fault = static_cast<unsigned>(CpuFaultCode::ILLEGAL_INSTRUCTION);
    }
    return d;
}

// Arithmetic is evaluated from captured operands. The pipeline below controls
// when the result becomes observable, including all 32 mul/div iteration edges.
Result execute(const Execute& e) {
    const auto a = e.a, b = e.b;
    const auto sa = signed_value(a), sb = signed_value(b);
    const unsigned shift = b & 31U;
    const bool overflow = sa == std::numeric_limits<std::int32_t>::min() && sb == -1;
    std::uint32_t value = 0;
    switch (e.op) {
    case Op::Add: value = a + b; break;
    case Op::Sub: value = a - b; break;
    case Op::Sll: value = a << shift; break;
    case Op::Slt: value = sa < sb; break;
    case Op::Sltu: value = a < b; break;
    case Op::Xor: value = a ^ b; break;
    case Op::Srl: value = a >> shift; break;
    case Op::Sra:
        value = a >> shift;
        if (shift != 0U && (a & 0x80000000U) != 0U)
            value |= ~std::uint32_t{0} << (32U - shift);
        break;
    case Op::Or: value = a | b; break;
    case Op::And: value = a & b; break;
    case Op::Mul: value = static_cast<std::uint32_t>(std::uint64_t{a} * b); break;
    case Op::Mulh:
        value = static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(static_cast<std::int64_t>(sa) * sb) >> 32U);
        break;
    case Op::Mulhsu:
        value = static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(static_cast<std::int64_t>(sa) * b) >> 32U);
        break;
    case Op::Mulhu: value = static_cast<std::uint32_t>((std::uint64_t{a} * b) >> 32U); break;
    case Op::Div: value = b == 0U ? 0xffffffffU : overflow ? a : static_cast<std::uint32_t>(sa / sb); break;
    case Op::Divu: value = b == 0U ? 0xffffffffU : a / b; break;
    case Op::Rem: value = b == 0U ? a : overflow ? 0U : static_cast<std::uint32_t>(sa % sb); break;
    case Op::Remu: value = b == 0U ? a : a % b; break;
    }
    return {e.meta, e.meta.rd == 0U ? 0U : value};
}

}  // namespace

// State corresponds to core_pipeline with ISSUE_WIDTH=1, OUT_OF_ORDER=0.
// Two-wide frontend transfers and two writeback lanes remain active in that RTL.
struct Rv32imCpu::Pipeline {
    static constexpr unsigned kIqDepth = 8, kRobDepth = 16;
    bool req_valid{}, outstanding{}, fetch_local_stopped{}, fetch_stopped{}, terminal_enqueued{};
    std::uint32_t req_addr{}, fetch_pc{}, pending_pc{};
    Slot<Fetch> fetched{};
    std::deque<Fetch> fifo{}, if2id{};
    std::deque<Decoded> id2dispatch{};
    std::array<Issue, kIqDepth> iq{};
    std::array<Completion, kRobDepth> rob{};
    unsigned head{}, tail{}, count{};
    std::array<std::uint32_t, 32> registers{};
    std::array<bool, 32> busy{};
    std::array<unsigned, 32> producer{};
    Slot<Execute> issue2ex{};
    Slot<Result> alu{}, md_output{};
    bool md_busy{};
    unsigned md_step{}, arbiter_next{};
    Result md_result{};
    std::array<Slot<Result>, 2> ex2wb{};
    Slot<Result> retired{};
    bool halted{}, fault{};
    unsigned fault_code{};

    struct Controls {
        bool stop{}, rsp_ready{}, finish{}, retire{}, dispatch{}, terminal_dispatch{};
        unsigned fifo_pop{}, id_pop{};
        Issue prepared{};
        std::array<bool, 2> wb_valid{};
        std::array<int, 2> grants{-1, -1};
        bool alu_ready{}, md_ready{}, ex_ready{};
        int issue_index{-1};
    };

    Operand operand(unsigned reg) const {
        if (reg == 0U) return {true, 0, 0};
        if (!busy[reg]) return {true, reg, registers[reg]};
        const auto& entry = rob[producer[reg]];
        return {entry.valid && entry.done, reg, entry.result.value};
    }

    Controls controls(bool reset) const {
        Controls c;
        c.stop = fetch_stopped;
        for (const auto& f : if2id) c.stop = c.stop || decode(f).terminal;
        c.rsp_ready = outstanding &&
            (c.stop || fetch_local_stopped || !fetched.valid || fifo.size() < 8U);
        const bool idle = !req_valid && !outstanding && !fetched.valid;
        if (!halted && !fault && count != 0U && rob[head].done) {
            c.retire = !rob[head].terminal;
            c.finish = rob[head].terminal && idle;
        }
        c.fifo_pop = static_cast<unsigned>(std::min({fifo.size(), std::size_t{2}, 2U - if2id.size()}));
        c.id_pop = static_cast<unsigned>(std::min(if2id.size(), 2U - id2dispatch.size()));
        const bool iq_space = std::any_of(iq.begin(), iq.end(), [](const Issue& i) { return !i.valid; });
        if (!reset && !terminal_enqueued && !halted && !fault &&
            !id2dispatch.empty() && count < kRobDepth && iq_space) {
            const auto& d = id2dispatch.front();
            c.dispatch = d.terminal || d.rd == 0U || !busy[d.rd];
            c.terminal_dispatch = c.dispatch && d.terminal;
            c.prepared = {true, {d.pc, d.rd, tail}, d.op, operand(d.rs1), operand(d.rs2)};
            if (d.immediate_b) c.prepared.b = {true, 0, d.immediate};
        }
        for (unsigned p = 0; p < 2; ++p)
            c.wb_valid[p] = ex2wb[p].valid && !c.finish && !halted && !fault;

        // Match the three-unit round-robin arbiter; unit 1 is disabled.
        const std::array<bool, 3> valid{alu.valid, false, md_output.valid};
        std::array<bool, 3> used{};
        for (unsigned p = 0; p < 2; ++p) {
            for (unsigned offset = 0; offset < 3; ++offset) {
                const unsigned unit = (arbiter_next + offset) % 3U;
                if (valid[unit] && !used[unit]) {
                    c.grants[p] = static_cast<int>(unit);
                    used[unit] = true;
                    break;
                }
            }
        }
        c.alu_ready = !alu.valid || used[0];
        c.md_ready = !md_busy && (!md_output.valid || used[2]);
        c.ex_ready = muldiv(issue2ex.data.op) ? c.md_ready : c.alu_ready;
        const bool issue_ready = (!issue2ex.valid || c.ex_ready) &&
                                 !reset && !c.finish && !halted && !fault;
        unsigned best_age = kRobDepth;
        int oldest = -1;
        for (unsigned i = 0; i < kIqDepth; ++i) {
            const unsigned age = (iq[i].meta.tag + kRobDepth - head) % kRobDepth;
            if (iq[i].valid && age < best_age) {
                best_age = age;
                oldest = static_cast<int>(i);
            }
        }
        if (issue_ready && oldest >= 0) {
            const auto& entry = iq[static_cast<unsigned>(oldest)];
            const bool md_available = c.md_ready && !(issue2ex.valid && muldiv(issue2ex.data.op));
            if (entry.a.ready && entry.b.ready && (!muldiv(entry.op) || md_available))
                c.issue_index = oldest;
        }
        return c;
    }

    void advance(const Controls& c, bool req_ready, bool rsp_valid,
                 std::uint32_t rsp_data, unsigned rsp_status) {
        // All decisions use this immutable pre-edge snapshot.
        Pipeline next = *this;

        if (fifo.size() < 8U || c.stop) next.fetched.valid = false;
        if (c.stop) next.fetch_local_stopped = true;
        if (!req_valid && !outstanding && !c.stop && !fetch_local_stopped && !fetched.valid) {
            next.req_valid = true;
            next.req_addr = fetch_pc;
        }
        if (req_valid && req_ready) {
            next.req_valid = false;
            next.outstanding = true;
            next.pending_pc = req_addr;
            next.fetch_pc = fetch_pc + 4U;
        }
        if (rsp_valid && c.rsp_ready) {
            next.outstanding = false;
            if (!c.stop && !fetch_local_stopped) {
                next.fetched = {true, {pending_pc, rsp_data, rsp_status}};
                if (rsp_status != static_cast<unsigned>(InstructionResponseStatus::OK))
                    next.fetch_local_stopped = true;
            }
        }
        if (c.stop) next.fetch_stopped = true;
        if (c.terminal_dispatch) next.terminal_enqueued = true;

        if (c.terminal_dispatch || c.finish) {
            next.fifo.clear();
            next.if2id.clear();
            next.id2dispatch.clear();
        } else {
            for (unsigned i = 0; i < c.fifo_pop; ++i) next.fifo.pop_front();
            if (fetched.valid && !terminal_enqueued && fifo.size() < 8U)
                next.fifo.push_back(fetched.data);
            for (unsigned i = 0; i < c.id_pop; ++i) next.if2id.pop_front();
            for (unsigned i = 0; i < c.fifo_pop; ++i) next.if2id.push_back(fifo[i]);
            if (c.dispatch) next.id2dispatch.pop_front();
            for (unsigned i = 0; i < c.id_pop; ++i) next.id2dispatch.push_back(decode(if2id[i]));
        }

        next.retired.valid = c.retire;
        if (c.retire) {
            next.retired.data = rob[head].result;
            const unsigned rd = rob[head].result.meta.rd;
            if (rd != 0U) {
                next.registers[rd] = rob[head].result.value;
                next.busy[rd] = false;
            }
        }
        if (c.finish) {
            next.halted = rob[head].fault == 0U;
            next.fault = rob[head].fault != 0U;
            next.fault_code = rob[head].fault;
            next.iq = {};
            next.rob = {};
            next.head = next.tail = next.count = 0;
            next.busy = {};
            next.producer = {};
            next.issue2ex = {};
            next.alu = {};
            next.md_output = {};
            next.md_busy = false;
            next.md_step = next.arbiter_next = 0;
            next.md_result = {};
            next.ex2wb = {};
        } else {
            // Completion, retirement, then allocation match RTL update priority.
            for (unsigned p = 0; p < 2; ++p) {
                if (c.wb_valid[p]) {
                    const auto& result = ex2wb[p].data;
                    assert(rob[result.meta.tag].valid && !rob[result.meta.tag].done);
                    next.rob[result.meta.tag].done = true;
                    next.rob[result.meta.tag].result.value = result.value;
                }
            }
            if (c.retire) {
                next.rob[head].valid = false;
                next.head = (head + 1U) % kRobDepth;
                --next.count;
            }
            if (c.dispatch) {
                const auto& d = id2dispatch.front();
                next.rob[tail] = {true, d.terminal, d.terminal, d.fault, {c.prepared.meta, 0}};
                next.tail = (tail + 1U) % kRobDepth;
                ++next.count;
                if (!d.terminal && d.rd != 0U) {
                    next.busy[d.rd] = true;
                    next.producer[d.rd] = tail;
                }
            }

            if (c.issue_index >= 0) next.iq[static_cast<unsigned>(c.issue_index)].valid = false;
            if (c.dispatch && !c.terminal_dispatch) {
                const auto free = std::find_if(next.iq.begin(), next.iq.end(),
                                              [](const Issue& i) { return !i.valid; });
                assert(free != next.iq.end());
                *free = c.prepared;
            }
            // Wake newly enqueued operands too, but issue sees pre-edge readiness.
            for (auto& entry : next.iq) {
                for (unsigned p = 0; p < 2; ++p) {
                    const auto& result = ex2wb[p].data;
                    if (!entry.valid || !c.wb_valid[p] || result.meta.rd == 0U) continue;
                    for (auto* source : {&entry.a, &entry.b}) {
                        if (!source->ready && source->reg == result.meta.rd) {
                            source->ready = true;
                            source->value = result.value;
                        }
                    }
                }
            }

            if (!issue2ex.valid || c.ex_ready) {
                next.issue2ex.valid = c.issue_index >= 0;
                if (c.issue_index >= 0) {
                    const auto& entry = iq[static_cast<unsigned>(c.issue_index)];
                    next.issue2ex.data = {entry.meta, entry.op, entry.a.value, entry.b.value};
                }
            }
            if (c.alu_ready) {
                next.alu.valid = issue2ex.valid && !muldiv(issue2ex.data.op);
                if (next.alu.valid) next.alu.data = execute(issue2ex.data);
            }
            const bool md_granted = c.grants[0] == 2 || c.grants[1] == 2;
            if (md_granted) next.md_output.valid = false;
            if (issue2ex.valid && muldiv(issue2ex.data.op) && c.md_ready) {
                next.md_busy = true;
                next.md_step = 0;
                next.md_result = execute(issue2ex.data);
            } else if (md_busy) {
                if (md_step < 32U) next.md_step = md_step + 1U;
                else {
                    next.md_busy = false;
                    next.md_output = {true, md_result};
                }
            }
            for (unsigned p = 0; p < 2; ++p) {
                next.ex2wb[p].valid = c.grants[p] >= 0;
                if (c.grants[p] >= 0) {
                    next.ex2wb[p].data = c.grants[p] == 0 ? alu.data : md_output.data;
                    next.arbiter_next = (static_cast<unsigned>(c.grants[p]) + 1U) % 3U;
                }
            }
        }
        assert(next.fifo.size() <= 8U && next.if2id.size() <= 2U && next.id2dispatch.size() <= 2U);
        assert(next.count <= kRobDepth && next.registers[0] == 0U);
        *this = std::move(next);
    }
};

Rv32imCpu::Rv32imCpu(sc_core::sc_module_name name)
    : sc_core::sc_module(name), pipeline_(std::make_unique<Pipeline>()) {
    SC_METHOD(tick);
    sensitive << clk.pos();
    dont_initialize();
    SC_METHOD(drive_outputs);
    sensitive << state_changed_;
}
Rv32imCpu::~Rv32imCpu() = default;

void Rv32imCpu::tick() {
    if (reset.read()) *pipeline_ = Pipeline{};
    else {
        const auto controls = pipeline_->controls(false);
        pipeline_->advance(controls, imem_req_ready.read(), imem_rsp_valid.read(),
                           imem_rsp_data.read().to_uint(), imem_rsp_status.read().to_uint());
    }
    state_changed_.notify(sc_core::SC_ZERO_TIME);
}

void Rv32imCpu::drive_outputs() {
    const auto& p = *pipeline_;
    imem_req_valid.write(p.req_valid);
    imem_req_addr.write(p.req_addr);
    imem_rsp_ready.write(p.controls(false).rsp_ready);
    retire_valid.write(p.retired.valid);
    retire_pc.write(p.retired.data.meta.pc);
    retire_rd.write(p.retired.data.meta.rd);
    retire_value.write(p.retired.data.value);
    halted.write(p.halted);
    fault.write(p.fault);
    fault_code.write(p.fault_code);
}
