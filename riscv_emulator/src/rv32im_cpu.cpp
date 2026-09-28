#include "rv32im_cpu.h"

#include "cpu_protocol.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <deque>
#include <limits>
#include <stdexcept>

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
    unsigned rd{}, tag{}, pdst{}, old_pdst{};
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

// Models core_pipeline with ISSUE_WIDTH=1 or 2 and both scheduling modes.
struct Rv32imCpu::Pipeline {
    static constexpr unsigned kIqDepth = 8, kRobDepth = 16, kPhysRegs = 64;
    explicit Pipeline(unsigned width, SchedulingMode scheduling) : issue_width(width), mode(scheduling) {
        if (mode == SchedulingMode::OutOfOrder) {
            for (unsigned r = 0; r < 32; ++r) speculative_map[r] = committed_map[r] = r;
            for (unsigned r = 0; r < kPhysRegs; ++r) {
                free_register[r] = r >= 32;
                ready_register[r] = true;
            }
        }
    }
    unsigned issue_width;
    SchedulingMode mode;
    bool req_valid{}, outstanding{}, fetch_local_stopped{}, fetch_stopped{}, terminal_enqueued{};
    std::uint32_t req_addr{}, fetch_pc{}, pending_pc{};
    Slot<Fetch> fetched{};
    std::deque<Fetch> fifo{}, if2id{};
    std::deque<Decoded> id2dispatch{};
    std::array<Issue, kIqDepth> iq{};
    std::array<Completion, kRobDepth> rob{};
    unsigned head{}, tail{}, count{};
    std::array<std::uint32_t, kPhysRegs> registers{};
    std::array<bool, 32> busy{};
    std::array<unsigned, 32> producer{};
    std::array<unsigned, 32> speculative_map{}, committed_map{};
    std::array<bool, kPhysRegs> free_register{}, ready_register{};
    std::array<Slot<Execute>, 2> issue2ex{};
    std::array<Slot<Result>, 2> alu{};
    Slot<Result> md_output{};
    bool md_busy{};
    unsigned md_step{}, arbiter_next{};
    Result md_result{};
    std::array<Slot<Result>, 2> ex2wb{};
    std::array<Slot<Result>, 2> retired{};
    bool halted{}, fault{};
    unsigned fault_code{};

    struct Controls {
        bool stop{}, rsp_ready{}, finish{}, terminal_dispatch{};
        unsigned fifo_pop{}, id_pop{}, retire_count{}, dispatch_count{};
        std::array<Issue, 2> prepared{};
        std::array<bool, 2> wb_valid{};
        std::array<int, 2> grants{-1, -1};
        std::array<bool, 2> alu_ready{}, ex_ready{};
        std::array<int, 2> ex_unit{-1, -1};
        bool md_ready{};
        int md_input{-1};
        std::array<int, 2> issue_index{-1, -1};
    };

    Operand operand(unsigned reg) const {
        if (reg == 0U) return {true, 0, 0};
        if (!busy[reg]) return {true, reg, registers[reg]};
        const auto& entry = rob[producer[reg]];
        return {entry.valid && entry.done, reg, entry.result.value};
    }

    void prepare_ooo_dispatch(Controls& c, unsigned offer_count) const {
        auto candidate_map = speculative_map;
        auto candidate_ready = ready_register;
        std::array<unsigned, 2> available{};
        unsigned free_count = 0;
        for (unsigned r = 1; r < kPhysRegs && free_count < 2; ++r)
            if (free_register[r]) available[free_count++] = r;
        unsigned used = 0;
        for (unsigned p = 0; p < offer_count; ++p) {
            const auto& d = id2dispatch[p];
            auto& prepared = c.prepared[p];
            prepared.valid = true;
            prepared.op = d.op;
            prepared.meta = {d.pc, d.rd, (tail + p) % kRobDepth, 0, 0};
            const unsigned a = candidate_map[d.rs1], b = candidate_map[d.rs2];
            prepared.a = {candidate_ready[a], a, registers[a]};
            prepared.b = d.immediate_b ? Operand{true, 0, d.immediate} :
                                         Operand{candidate_ready[b], b, registers[b]};
            if (d.terminal) {
                ++c.dispatch_count;
                c.terminal_dispatch = true;
                break;
            }
            if (d.rd != 0U && used == free_count) break;
            ++c.dispatch_count;
            if (d.rd != 0U) {
                prepared.meta.old_pdst = candidate_map[d.rd];
                prepared.meta.pdst = available[used];
                candidate_map[d.rd] = available[used];
                candidate_ready[available[used]] = false;
                ++used;
            }
        }
    }

    void select_ooo_issue(Controls& c, bool md_issue_available) const {
        bool md_used = false;
        for (unsigned p = 0; p < issue_width; ++p) {
            const bool stage_ready = !issue2ex[p].valid || c.ex_ready[p];
            if (!stage_ready || c.finish || halted || fault) continue;
            unsigned best_age = kRobDepth;
            for (unsigned i = 0; i < kIqDepth; ++i) {
                const auto& entry = iq[i];
                const unsigned age = (entry.meta.tag + kRobDepth - head) % kRobDepth;
                if (entry.valid && i != static_cast<unsigned>(c.issue_index[0]) &&
                    entry.a.ready && entry.b.ready &&
                    (!muldiv(entry.op) || (md_issue_available && !md_used)) && age < best_age) {
                    best_age = age;
                    c.issue_index[p] = static_cast<int>(i);
                }
            }
            if (c.issue_index[p] >= 0 && muldiv(iq[static_cast<unsigned>(c.issue_index[p])].op))
                md_used = true;
        }
    }

    Controls controls(bool reset) const {
        Controls c;
        c.stop = fetch_stopped;
        for (const auto& f : if2id) c.stop = c.stop || decode(f).terminal;
        c.rsp_ready = outstanding &&
            (c.stop || fetch_local_stopped || !fetched.valid || fifo.size() < 8U);
        const bool idle = !req_valid && !outstanding && !fetched.valid;
        if (!halted && !fault && count != 0U && rob[head].done) {
            if (rob[head].terminal) c.finish = idle;
            else {
                c.retire_count = 1;
                const unsigned second = (head + 1U) % kRobDepth;
                if (issue_width == 2 && count > 1U && rob[second].done && !rob[second].terminal)
                    c.retire_count = 2;
            }
        }
        c.fifo_pop = static_cast<unsigned>(std::min({fifo.size(), std::size_t{2}, 2U - if2id.size()}));
        c.id_pop = static_cast<unsigned>(std::min(if2id.size(), 2U - id2dispatch.size()));
        const unsigned iq_space = static_cast<unsigned>(std::count_if(
            iq.begin(), iq.end(), [](const Issue& entry) { return !entry.valid; }));
        unsigned offer_count = std::min({static_cast<unsigned>(id2dispatch.size()), issue_width,
                                         kRobDepth - count, iq_space});
        if (reset || terminal_enqueued || halted || fault) offer_count = 0;
        if (mode == SchedulingMode::OutOfOrder) prepare_ooo_dispatch(c, offer_count);
        else {
            auto candidate_busy = busy;
            for (unsigned p = 0; p < offer_count; ++p) {
                const auto& d = id2dispatch[p];
                c.prepared[p] = {true, {d.pc, d.rd, (tail + p) % kRobDepth, d.rd, 0}, d.op,
                                 operand(d.rs1), operand(d.rs2)};
                if (d.terminal) {
                    ++c.dispatch_count;
                    c.terminal_dispatch = true;
                    break;
                }
                if (d.rd != 0U && candidate_busy[d.rd]) break;
                if (p == 1U && !id2dispatch[0].terminal && id2dispatch[0].rd != 0U) {
                    const unsigned older_rd = id2dispatch[0].rd;
                    if (d.rs1 == older_rd) c.prepared[p].a = {false, d.rs1, 0};
                    if (!d.immediate_b && d.rs2 == older_rd) c.prepared[p].b = {false, d.rs2, 0};
                }
                if (d.immediate_b) c.prepared[p].b = {true, 0, d.immediate};
                ++c.dispatch_count;
                if (d.rd != 0U) candidate_busy[d.rd] = true;
            }
        }
        for (unsigned p = 0; p < 2; ++p)
            c.wb_valid[p] = ex2wb[p].valid && !c.finish && !halted && !fault;

        const std::array<bool, 3> valid{alu[0].valid, issue_width == 2 && alu[1].valid,
                                         md_output.valid};
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
        for (unsigned u = 0; u < issue_width; ++u)
            c.alu_ready[u] = !alu[u].valid || used[u];
        c.md_ready = !md_busy && (!md_output.valid || used[2]);
        std::array<bool, 3> unit_used{};
        for (unsigned p = 0; p < 2; ++p) {
            int selected = -1;
            if (muldiv(issue2ex[p].data.op)) {
                if (c.md_ready && !unit_used[2]) selected = 2;
            } else {
                for (unsigned u = 0; u < issue_width; ++u)
                    if (c.alu_ready[u] && !unit_used[u] && selected < 0)
                        selected = static_cast<int>(u);
            }
            if (selected >= 0) {
                c.ex_ready[p] = true;
                if (issue2ex[p].valid) {
                    unit_used[static_cast<unsigned>(selected)] = true;
                    if (selected == 2) c.md_input = static_cast<int>(p);
                    else c.ex_unit[p] = selected;
                }
            }
        }

        bool blocked = false;
        bool md_used = false;
        const bool md_issue_available = c.md_ready &&
            !(issue2ex[0].valid && muldiv(issue2ex[0].data.op)) &&
            !(issue2ex[1].valid && muldiv(issue2ex[1].data.op));
        if (mode == SchedulingMode::OutOfOrder) {
            if (!reset) select_ooo_issue(c, md_issue_available);
        } else {
            for (unsigned p = 0; p < issue_width; ++p) {
                unsigned best_age = kRobDepth;
                int oldest = -1;
                for (unsigned i = 0; i < kIqDepth; ++i) {
                    const unsigned age = (iq[i].meta.tag + kRobDepth - head) % kRobDepth;
                    if (iq[i].valid && i != static_cast<unsigned>(c.issue_index[0]) && age < best_age) {
                        best_age = age;
                        oldest = static_cast<int>(i);
                    }
                }
                const bool stage_ready = !issue2ex[p].valid || c.ex_ready[p];
                if (!stage_ready || blocked || reset || c.finish || halted || fault || oldest < 0) continue;
                const auto& entry = iq[static_cast<unsigned>(oldest)];
                if (!entry.a.ready || !entry.b.ready ||
                    (muldiv(entry.op) && (!md_issue_available || md_used))) blocked = true;
                else {
                    c.issue_index[p] = oldest;
                    if (muldiv(entry.op)) md_used = true;
                }
            }
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
            for (unsigned p = 0; p < c.dispatch_count; ++p) next.id2dispatch.pop_front();
            for (unsigned i = 0; i < c.id_pop; ++i) next.id2dispatch.push_back(decode(if2id[i]));
        }

        for (unsigned p = 0; p < 2; ++p) {
            next.retired[p].valid = p < c.retire_count;
            if (p < c.retire_count) {
                const auto& result = rob[(head + p) % kRobDepth].result;
                next.retired[p].data = result;
                if (result.meta.rd != 0U) {
                    if (mode == SchedulingMode::OutOfOrder) {
                        assert(result.meta.old_pdst != 0U && !free_register[result.meta.old_pdst]);
                        next.free_register[result.meta.old_pdst] = true;
                        next.committed_map[result.meta.rd] = result.meta.pdst;
                    } else {
                        next.registers[result.meta.rd] = result.value;
                        next.busy[result.meta.rd] = false;
                    }
                }
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
                    if (mode == SchedulingMode::OutOfOrder && result.meta.rd != 0U) {
                        next.registers[result.meta.pdst] = result.value;
                        next.ready_register[result.meta.pdst] = true;
                    }
                }
            }
            for (unsigned p = 0; p < c.retire_count; ++p) {
                next.rob[(head + p) % kRobDepth].valid = false;
            }
            next.head = (head + c.retire_count) % kRobDepth;
            next.tail = (tail + c.dispatch_count) % kRobDepth;
            next.count = count + c.dispatch_count - c.retire_count;
            for (unsigned p = 0; p < c.dispatch_count; ++p) {
                const auto& d = id2dispatch[p];
                next.rob[(tail + p) % kRobDepth] =
                    {true, d.terminal, d.terminal, d.fault, {c.prepared[p].meta, 0}};
                if (!d.terminal && d.rd != 0U) {
                    if (mode == SchedulingMode::OutOfOrder) {
                        const unsigned pdst = c.prepared[p].meta.pdst;
                        assert(pdst != 0U && free_register[pdst]);
                        next.free_register[pdst] = false;
                        next.ready_register[pdst] = false;
                        next.speculative_map[d.rd] = pdst;
                    } else {
                        next.busy[d.rd] = true;
                        next.producer[d.rd] = (tail + p) % kRobDepth;
                    }
                }
            }

            for (int index : c.issue_index)
                if (index >= 0) next.iq[static_cast<unsigned>(index)].valid = false;
            for (unsigned p = 0; p < c.dispatch_count; ++p) {
                if (id2dispatch[p].terminal) continue;
                const auto free = std::find_if(next.iq.begin(), next.iq.end(),
                                              [](const Issue& i) { return !i.valid; });
                assert(free != next.iq.end());
                *free = c.prepared[p];
            }
            // Wake newly enqueued operands too, but issue sees pre-edge readiness.
            for (auto& entry : next.iq) {
                for (unsigned p = 0; p < 2; ++p) {
                    const auto& result = ex2wb[p].data;
                    if (!entry.valid || !c.wb_valid[p] || result.meta.rd == 0U) continue;
                    const unsigned tag = mode == SchedulingMode::OutOfOrder ? result.meta.pdst : result.meta.rd;
                    for (auto* source : {&entry.a, &entry.b}) {
                        if (!source->ready && source->reg == tag) {
                            source->ready = true;
                            source->value = result.value;
                        }
                    }
                }
            }

            for (unsigned p = 0; p < 2; ++p) {
                if (!issue2ex[p].valid || c.ex_ready[p]) {
                    next.issue2ex[p].valid = c.issue_index[p] >= 0;
                    if (c.issue_index[p] >= 0) {
                        const auto& entry = iq[static_cast<unsigned>(c.issue_index[p])];
                        next.issue2ex[p].data = {entry.meta, entry.op, entry.a.value, entry.b.value};
                    }
                }
            }
            for (unsigned u = 0; u < issue_width; ++u) {
                if (!c.alu_ready[u]) continue;
                next.alu[u].valid = false;
                for (unsigned p = 0; p < 2; ++p)
                    if (c.ex_unit[p] == static_cast<int>(u)) {
                        next.alu[u] = {true, execute(issue2ex[p].data)};
                    }
            }
            const bool md_granted = c.grants[0] == 2 || c.grants[1] == 2;
            if (md_granted) next.md_output.valid = false;
            if (c.md_input >= 0) {
                next.md_busy = true;
                next.md_step = 0;
                next.md_result = execute(issue2ex[static_cast<unsigned>(c.md_input)].data);
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
                    const unsigned unit = static_cast<unsigned>(c.grants[p]);
                    next.ex2wb[p].data = unit == 2 ? md_output.data : alu[unit].data;
                    next.arbiter_next = (static_cast<unsigned>(c.grants[p]) + 1U) % 3U;
                }
            }
        }
        assert(c.dispatch_count <= issue_width && c.retire_count <= issue_width);
        assert(next.fifo.size() <= 8U && next.if2id.size() <= 2U && next.id2dispatch.size() <= 2U);
        assert(next.count <= kRobDepth && next.registers[0] == 0U);
        assert(!next.retired[1].valid || next.retired[0].valid);
        assert(issue_width == 2U || !next.issue2ex[1].valid);
        *this = std::move(next);
    }
};

Rv32imCpu::Rv32imCpu(sc_core::sc_module_name name, unsigned issue_width, SchedulingMode mode)
    : sc_core::sc_module(name), issue_width_(issue_width), mode_(mode),
      pipeline_(std::make_unique<Pipeline>(issue_width, mode)) {
    if (issue_width != 1U && issue_width != 2U) throw std::invalid_argument("issue_width must be 1 or 2");
    if (mode == SchedulingMode::OutOfOrder && issue_width != 2U)
        throw std::invalid_argument("out-of-order mode requires issue_width 2");
    SC_METHOD(tick);
    sensitive << clk.pos();
    dont_initialize();
    SC_METHOD(drive_outputs);
    sensitive << state_changed_;
}
Rv32imCpu::~Rv32imCpu() = default;

void Rv32imCpu::tick() {
    if (reset.read()) *pipeline_ = Pipeline{issue_width_, mode_};
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
    retire_valid.write(p.retired[0].valid);
    retire_pc.write(p.retired[0].data.meta.pc);
    retire_rd.write(p.retired[0].data.meta.rd);
    retire_value.write(p.retired[0].data.value);
    retire1_valid.write(p.retired[1].valid);
    retire1_pc.write(p.retired[1].data.meta.pc);
    retire1_rd.write(p.retired[1].data.meta.rd);
    retire1_value.write(p.retired[1].data.value);
    halted.write(p.halted);
    fault.write(p.fault);
    fault_code.write(p.fault_code);
}
