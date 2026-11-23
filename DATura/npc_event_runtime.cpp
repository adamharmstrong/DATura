#include "stdafx.h"
#include "npc_event_runtime.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

// Offline interpreter for retail zone event scripts (ROM/21 and friends).
// Opcode sizes and branch behaviour follow the retail PC client as documented
// in atom0s/xievents, cross-checked against the decompiled PS2 client
// (tmp/ffxi-ps2-decompile, XiEvent::ExecProg). Server round trips, camera,
// movement, animation and scheduler opcodes have no offline effect, so they
// are stepped over with their real lengths instead of stopping the script.
namespace NpcEventRuntime
{
namespace
{
// Fixed-length opcodes. 0 means the length depends on a sub-mode byte (see
// InstructionSize) or that the opcode has no handler in the retail client.
constexpr std::uint8_t kFixedSize[256] = {
    1, 3, 8, 5, 3, 3, 3, 5, 5, 5, 5, 3, 3, 5, 5, 5,  // 0x00
    5, 5, 3, 5, 5, 5, 7, 7, 7, 5, 3, 1, 3, 3, 5, 0,  // 0x10
    2, 1, 2, 1, 7, 1, 1, 7, 7, 7, 6, 7, 13, 13, 1, 6,  // 0x20
    1, 0, 3, 2, 3, 3, 7, 9, 3, 3, 7, 11, 7, 7, 7, 7,  // 0x30
    9, 9, 1, 2, 5, 17, 0, 0, 3, 7, 9, 7, 1, 1, 6, 3,  // 0x40
    13, 13, 15, 13, 13, 15, 5, 3, 1, 0, 0, 0, 0, 5, 5, 0,  // 0x50
    0, 2, 17, 3, 11, 11, 0, 5, 1, 4, 7, 9, 9, 7, 7, 1,  // 0x60
    1, 0, 0, 11, 2, 0, 5, 5, 1, 0, 0, 5, 6, 3, 0, 1,  // 0x70
    5, 6, 7, 3, 1, 1, 6, 2, 2, 3, 1, 25, 0, 5, 1, 1,  // 0x80
    1, 3, 6, 3, 6, 3, 1, 5, 1, 5, 1, 1, 3, 0, 2, 17,  // 0x90
    15, 15, 15, 15, 2, 2, 0, 0, 6, 3, 17, 0, 0, 12, 0, 8,  // 0xa0
    12, 4, 0, 0, 0, 4, 0, 0, 27, 8, 13, 17, 15, 15, 3, 0,  // 0xb0
    3, 5, 0, 7, 11, 17, 15, 15, 7, 1, 1, 1, 0, 17, 15, 15,  // 0xc0
    17, 15, 15, 6, 0, 17, 15, 15, 0, 2, 0, 0, 0, 0, 0, 0,  // 0xd0
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xe0
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xf0
};

std::size_t SubModeSize(const std::uint8_t op, const std::uint8_t sub)
{
    switch (op)
    {
    case 0x1f: case 0x5a: return sub == 0 ? 8 : 2;   // MOVE / MOVE2
    case 0x31: case 0x47: return sub == 0 ? 10 : 2;  // SMOVE / pending XZY
    case 0x46: return sub == 2 ? 4 : 2;              // camera control
    case 0x5b: case 0x66: return 15;                 // extended scheduler
    case 0x60: return sub <= 1 ? 4 : (sub == 2 ? 6 : 2);
    case 0x72: return sub == 0 ? 4 : 6;              // weather
    case 0x75: return sub == 0 ? 4 : 2;              // load room
    case 0x79: return sub == 1 ? 12 : 10;            // look at
    case 0xa6: return sub == 2 ? 4 : 2;              // event map number
    case 0xa7: case 0xb2: return sub == 1 ? 4 : 2;
    case 0xc2: return sub == 1 ? 4 : (sub == 2 ? 6 : 2);
    case 0x59:
    {
        static constexpr std::uint8_t s[] = { 4, 8, 4, 8, 8, 7, 6, 4, 8 };
        return sub < sizeof(s) ? s[sub] : 0;
    }
    case 0x5c:
        if (sub < 8) return 4;
        if ((sub >= 0x80 && sub <= 0x87) || sub == 0xa0 || sub == 0xa1) return 6;
        return 0;
    case 0x5f:
    {
        static constexpr std::uint8_t s[] = { 2, 2, 6, 16, 16, 18, 18, 14 };
        return sub < sizeof(s) ? s[sub] : 0;
    }
    case 0x71:
        switch (sub)
        {
        case 0x00: case 0x01: case 0x02: case 0x21: case 0x51: case 0x53: return 2;
        case 0x03: case 0x10: case 0x11: case 0x13: case 0x30: case 0x31:
        case 0x40: case 0x50: case 0x52: case 0x55: return 4;
        case 0x12: case 0x32: return 6;
        case 0x41: return 8;
        case 0x54: return 10;
        case 0x20: return 16;
        default: return 0;
        }
    case 0x7a:
    {
        static constexpr std::uint8_t s[] = { 6, 7, 6, 2, 8, 6 };
        return sub < sizeof(s) ? s[sub] : 0;
    }
    case 0x7e:
    {
        static constexpr std::uint8_t s[] = { 6, 6, 6, 16, 6, 6, 18, 8, 6 };
        return sub < sizeof(s) ? s[sub] : 0;
    }
    case 0x8c:
    {
        static constexpr std::uint8_t s[] = { 8, 2, 12, 10, 10, 14 };
        return sub < sizeof(s) ? s[sub] : 0;
    }
    case 0x9d:
    {
        static constexpr std::uint8_t s[] = {
            8, 8, 6, 8, 8, 8, 8, 6, 23, 9, 10, 10, 8, 10, 10, 10, 10 };
        return sub < sizeof(s) ? s[sub] : 0;
    }
    case 0xab:
        if (sub == 0x11 || (sub >= 0x14 && sub <= 0x18)) return 4;
        if (sub == 0x1b || sub == 0x1c) return 6;
        return sub <= 0x1a ? 2 : 0;
    case 0xac:
    {
        static constexpr std::uint8_t s[] = { 4, 4, 6, 6, 8 };
        return sub < sizeof(s) ? s[sub] : 0;
    }
    case 0xae:
    {
        static constexpr std::uint8_t s[] = { 6, 8, 8, 8, 8, 10, 6, 10, 10 };
        return sub < sizeof(s) ? s[sub] : 0;
    }
    case 0xb3:
    {
        static constexpr std::uint8_t s[] = { 4, 14, 2, 4, 4, 18, 4, 4, 2, 4 };
        return sub < sizeof(s) ? s[sub] : 2;
    }
    case 0xb4:
    {
        static constexpr std::uint8_t s[] = {
            20, 6, 6, 2, 6, 3, 3, 4, 2, 4, 4, 2, 4, 2, 2, 6, 6, 6, 6, 20, 12, 2 };
        return sub < sizeof(s) ? s[sub] : 0;
    }
    case 0xb6:
    {
        static constexpr std::uint8_t s[] = {
            4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 20, 4, 14, 16, 4, 2, 4, 2, 2, 6, 6 };
        return sub < sizeof(s) ? s[sub] : 0;
    }
    case 0xb7:
    {
        static constexpr std::uint8_t s[] = { 10, 8, 8, 8, 8 };
        return sub < sizeof(s) ? s[sub] : 0;
    }
    case 0xbf:
        return (sub == 0x00 || sub == 0x20 || sub == 0x40 || sub == 0x60) ? 8 : 10;
    case 0xcc:
        switch (sub)
        {
        case 0x00: case 0x01: case 0x03: return 10;
        case 0x02: return 14;
        case 0x10: return 6;
        case 0x11: case 0x20: return 4;
        default: return 0;
        }
    case 0xd4:
    {
        static constexpr std::uint8_t s[] = { 8, 8, 8, 6, 12, 12 };
        return sub < sizeof(s) ? s[sub] : 0;
    }
    case 0xd8:
    {
        static constexpr std::uint8_t s[] = { 6, 8, 8, 8, 12 };
        return sub < sizeof(s) ? s[sub] : 6;
    }
    default:
        return 0;
    }
}

std::uint16_t Read16(const FFXIEventTable::Event& event, const std::size_t at)
{
    if (at + 2 > event.code.size()) return 0;
    return static_cast<std::uint16_t>(event.code[at] | (event.code[at + 1] << 8));
}

// XiEvent::getworkofs: decode a work operand found at (base + offset).
std::int32_t ReadOperand(const State& state, const std::size_t base,
                         const std::size_t operandOffset, const std::int32_t shift = 0)
{
    if (!state.event) return 0;
    const std::uint32_t value = static_cast<std::uint32_t>(
        static_cast<std::int32_t>(Read16(*state.event, base + operandOffset)) + shift) & 0xffffu;
    if ((value & 0x8000u) != 0)
    {
        const std::size_t index = value & 0x7fffu;
        return index < state.event->references.size()
            ? static_cast<std::int32_t>(state.event->references[index]) : 0;
    }
    if (value < 2048u)
        return value < state.local.size() ? static_cast<std::int32_t>(state.local[value]) : 0;
    if (value >= 4096u && value < 4096u + state.zone.size())
        return static_cast<std::int32_t>(state.zone[value - 4096u]);
    if (value >= 4352u && value < 4352u + state.zoneMemorize.size())
        return static_cast<std::int32_t>(state.zoneMemorize[value - 4352u]);
    if (value >= 5888u && value < 5888u + state.zone1700.size())
        return static_cast<std::int32_t>(state.zone1700[value - 5888u]);
    // 0x7f00+ are live actor/player properties (position, job, race...).
    // Level-gated dialogue reads 0x7f08/0x7f88; report level 1 like the
    // client does for a non-player event actor.
    if (value == 0x7f08u || value == 0x7f88u)
        return 1;
    return 0;
}

std::int32_t GetWork(const State& state, const std::size_t operandOffset,
                     const std::int32_t shift = 0)
{
    return ReadOperand(state, state.pc, operandOffset, shift);
}

void WriteOperand(State& state, const std::size_t base, const std::size_t operandOffset,
                  const std::uint32_t value, const std::int32_t shift = 0)
{
    if (!state.event) return;
    const std::uint32_t operand = static_cast<std::uint32_t>(
        static_cast<std::int32_t>(Read16(*state.event, base + operandOffset)) + shift) & 0xffffu;
    if ((operand & 0x8000u) != 0)
        return;
    if (operand < state.local.size())
        state.local[operand] = value;
    else if (operand >= 4096u && operand < 4096u + state.zone.size())
        state.zone[operand - 4096u] = value;
    else if (operand >= 4352u && operand < 4352u + state.zoneMemorize.size())
        state.zoneMemorize[operand - 4352u] = value;
    else if (operand >= 5888u && operand < 5888u + state.zone1700.size())
        state.zone1700[operand - 5888u] = value;
}

void SetWork(State& state, const std::size_t operandOffset,
             const std::uint32_t value, const std::int32_t shift = 0)
{
    WriteOperand(state, state.pc, operandOffset, value, shift);
}

std::uint32_t NextRandom(State& state)
{
    state.randomState = state.randomState * 1103515245u + 12345u;
    return (state.randomState >> 16) & 0x7fffu;
}

std::uint32_t BitMask(const std::int32_t first, const std::int32_t last)
{
    std::uint32_t mask = 0;
    for (std::int32_t bit = 0; bit < 32; ++bit)
        if (first <= bit && last >= bit)
            mask |= 1u << bit;
    return mask;
}

void BinaryOp(State& state, const std::uint8_t op)
{
    const std::uint32_t a = static_cast<std::uint32_t>(GetWork(state, 1));
    const std::uint32_t b = static_cast<std::uint32_t>(GetWork(state, 3));
    std::uint32_t value = 0;
    switch (op)
    {
    case 0x07: value = a + b; break;
    case 0x08: value = a - b; break;
    case 0x09: value = a | (1u << (b & 31)); break;
    case 0x0a: value = a & ~(1u << (b & 31)); break;
    case 0x0d: value = a & b; break;
    case 0x0e: value = a | b; break;
    case 0x0f: value = a ^ b; break;
    case 0x10: value = a << (b & 31); break;
    case 0x11: value = static_cast<std::uint32_t>(static_cast<std::int32_t>(a) >> (b & 31)); break;
    case 0x14: value = a * b; break;
    case 0x15:
        value = (a && b) ? static_cast<std::uint32_t>(
            static_cast<std::int32_t>(a) / static_cast<std::int32_t>(b)) : 0;
        break;
    }
    SetWork(state, 1, value);
    state.pc += 5;
}

void If(State& state)
{
    if (!state.event) return;
    const std::uint8_t kind = state.pc + 5 < state.event->code.size()
        ? state.event->code[state.pc + 5] & 0x0f : 0;
    const std::int32_t a = GetWork(state, 1);
    const std::int32_t b = GetWork(state, 3);
    const std::size_t target = Read16(*state.event, state.pc + 6);
    bool take = true;
    switch (kind)
    {
    case 0: take = a != b; break;
    case 1: case 7: take = a == b; break;
    case 2: take = a <= b; break;
    case 3: take = a >= b; break;
    case 4: take = a < b; break;
    case 5: take = a > b; break;
    case 6: case 9: take = ((std::uint32_t)b & (std::uint32_t)a) == 0; break;
    case 8: take = ((std::uint32_t)a | (std::uint32_t)b) == 0; break;
    case 10: take = (~(std::uint32_t)a & (std::uint32_t)b) == 0; break;
    }
    state.pc = take ? target : state.pc + 8;
}

bool PushJump(State& state, const std::size_t returnTo)
{
    if (state.jumpIndex >= state.jumps.size())
        return false;
    state.jumps[state.jumpIndex++] = static_cast<std::uint16_t>(returnTo);
    return true;
}

void BeginChoice(State& state, const std::size_t base, const std::size_t size)
{
    state.pendingChoiceMessage = static_cast<std::uint32_t>(ReadOperand(state, base, 1));
    state.defaultChoice = static_cast<std::uint32_t>(
        std::max<std::int32_t>(0, ReadOperand(state, base, 3)));
    state.hiddenChoiceMask = static_cast<std::uint32_t>(ReadOperand(state, base, 5));
    state.waitingChoice = true;
    state.pc += size;
}

// Opcode 0x9d: table reads/writes and computed gosubs. Returns false when the
// script has to stop.
bool TableOp(State& state, const std::size_t size)
{
    const FFXIEventTable::Event& event = *state.event;
    const std::size_t at = state.pc;
    switch (event.code[at + 1])
    {
    case 0x00: case 0x0a:
    {
        // dst(+4) = work operand table(+2)[index(+6)], index clamped by limit(+8).
        const std::int32_t limit = GetWork(state, 8);
        std::int32_t index = GetWork(state, 6);
        if (limit && limit <= index)
            index = 0;
        const std::size_t table = Read16(event, at + 2);
        const std::int32_t value =
            ReadOperand(state, table, 2u * static_cast<std::size_t>(std::max(0, index)));
        SetWork(state, 4, static_cast<std::uint32_t>(value));
        state.pc += size;
        return true;
    }
    case 0x05:
    {
        // table(+2)[index(+6)] = value(+4)
        const std::int32_t index = GetWork(state, 6);
        const std::uint32_t value = static_cast<std::uint32_t>(GetWork(state, 4));
        const std::size_t table = Read16(event, at + 2);
        WriteOperand(state, table + 2u * static_cast<std::size_t>(std::max(0, index)), 0, value);
        state.pc += size;
        return true;
    }
    case 0x07:
    {
        // gosub table(+2)[index(+4)], returning past this instruction.
        const std::int32_t index = GetWork(state, 4);
        const std::size_t table = Read16(event, at + 2);
        const std::size_t target = Read16(event, table + 2u * static_cast<std::size_t>(std::max(0, index)));
        if (!PushJump(state, at + size))
            return false;
        state.pc = target;
        return true;
    }
    default:
        // String tables, cross-event tables and comparisons have no offline
        // data; take the fall-through path.
        state.pc += size;
        return true;
    }
}
}

std::size_t InstructionSize(const std::vector<std::uint8_t>& code, const std::size_t pc)
{
    if (pc >= code.size())
        return 0;
    const std::uint8_t op = code[pc];
    const std::size_t fixed = kFixedSize[op];
    if (fixed != 0)
        return fixed;
    const std::uint8_t sub = pc + 1 < code.size() ? code[pc + 1] : 0;
    return SubModeSize(op, sub);
}

void Begin(State& state, const FFXIEventTable::Event& event)
{
    state = {};
    state.event = &event;
    state.pc = event.entryOffset;
}

Outcome Advance(State& state)
{
    if (!state.event || state.finished)
        return { Step::Done };
    const FFXIEventTable::Event& event = *state.event;
    for (int budget = 0; budget < 100000; ++budget)
    {
        if (state.pc >= event.code.size())
        {
            state.finished = true;
            return { Step::Done };
        }
        const std::uint8_t op = event.code[state.pc];
        switch (op)
        {
        case 0x00: case 0x21: case 0x26:
            // END / event end / halt (waits on the server forever offline).
            state.finished = true;
            return { Step::Done };
        case 0x01:
            state.pc = Read16(event, state.pc + 1);
            break;
        case 0x02:
            If(state);
            break;
        case 0x03:
            SetWork(state, 1, static_cast<std::uint32_t>(GetWork(state, 3)));
            state.pc += 5;
            break;
        case 0x05:
            SetWork(state, 1, 1); state.pc += 3; break;
        case 0x06:
            SetWork(state, 1, 0); state.pc += 3; break;
        case 0x0b:
            SetWork(state, 1, static_cast<std::uint32_t>(GetWork(state, 1) + 1)); state.pc += 3; break;
        case 0x0c:
            SetWork(state, 1, static_cast<std::uint32_t>(GetWork(state, 1) - 1)); state.pc += 3; break;
        case 0x07: case 0x08: case 0x09: case 0x0a: case 0x0d: case 0x0e:
        case 0x0f: case 0x10: case 0x11: case 0x14: case 0x15:
            BinaryOp(state, op);
            break;
        case 0x12:
            SetWork(state, 1, NextRandom(state)); state.pc += 3; break;
        case 0x13:
        {
            const std::int32_t range = GetWork(state, 3) + 1;
            SetWork(state, 1, range > 0 ? NextRandom(state) % static_cast<std::uint32_t>(range) : 0);
            state.pc += 5;
            break;
        }
        case 0x19:
        {
            const std::uint32_t a = static_cast<std::uint32_t>(GetWork(state, 1));
            const std::uint32_t b = static_cast<std::uint32_t>(GetWork(state, 3));
            SetWork(state, 1, b);
            SetWork(state, 3, a);
            state.pc += 5;
            break;
        }
        case 0x1a:
            if (!PushJump(state, state.pc + 3))
            {
                state.finished = true;
                return { Step::Done };
            }
            state.pc = Read16(event, state.pc + 1);
            break;
        case 0x1b:
            if (state.jumpIndex == 0)
            {
                state.finished = true;
                return { Step::Done };
            }
            state.pc = state.jumps[--state.jumpIndex];
            break;
        case 0x1d: case 0x48:
            state.pendingMessage = static_cast<std::uint32_t>(GetWork(state, 1));
            state.messageOpen = true;
            state.pc += 3;
            break;
        case 0x2b: case 0x49:
            state.pendingMessage = static_cast<std::uint32_t>(GetWork(state, 5));
            state.messageOpen = true;
            state.pc += 7;
            break;
        case 0xb0:
            if (state.pc + 12 <= event.code.size() && event.code[state.pc + 1] == 0)
            {
                state.pendingMessage = static_cast<std::uint32_t>(GetWork(state, 10));
                state.messageOpen = true;
            }
            state.pc += 12;
            break;
        case 0x23:
            if (state.messageOpen)
                return { Step::Message };
            state.pc += 1;
            break;
        case 0x24:
            BeginChoice(state, state.pc, 7);
            break;
        case 0xd4:
        {
            const std::uint8_t sub = state.pc + 1 < event.code.size() ? event.code[state.pc + 1] : 0xff;
            if (sub == 0 || sub == 2)
                BeginChoice(state, state.pc + 1, 8);
            else
            {
                const std::size_t size = InstructionSize(event.code, state.pc);
                if (size == 0)
                    return { Step::Blocked, true, event.id, state.pc, op };
                state.pc += size;
            }
            break;
        }
        case 0x25: case 0x7f:
            if (state.waitingChoice)
                return { Step::Choice };
            state.pc += 1;
            break;
        case 0x3c: case 0x3d:
        {
            const std::uint32_t bit = static_cast<std::uint32_t>(GetWork(state, 3));
            const std::int32_t words = GetWork(state, 5);
            const std::int32_t word = static_cast<std::int32_t>(bit) >> 5;
            if (word < words)
            {
                const std::uint32_t current = static_cast<std::uint32_t>(GetWork(state, 1, word));
                const std::uint32_t mask = 1u << (bit & 31);
                SetWork(state, 1, op == 0x3c ? current | mask : current & ~mask, word);
            }
            state.pc += 7;
            break;
        }
        case 0x3e:
        {
            const std::int32_t bit = GetWork(state, 3);
            const std::int32_t word = GetWork(state, 1, bit >> 5);
            state.pc = ((word & (1 << (bit & 31))) == 0)
                ? Read16(event, state.pc + 5)
                : state.pc + 7;
            break;
        }
        case 0x3f:
        {
            const std::int32_t a = GetWork(state, 3);
            const std::int32_t b = GetWork(state, 5);
            SetWork(state, 1, (a && b) ? static_cast<std::uint32_t>(a % b) : 0);
            state.pc += 7;
            break;
        }
        case 0x40:
        {
            const std::uint32_t first = static_cast<std::uint32_t>(GetWork(state, 1));
            const std::uint32_t mask = BitMask(GetWork(state, 1), GetWork(state, 3));
            const std::uint32_t current = static_cast<std::uint32_t>(GetWork(state, 5));
            const std::uint32_t value = static_cast<std::uint32_t>(GetWork(state, 7));
            SetWork(state, 5, (~mask & current) | (mask & (value << (first & 31))));
            state.pc += 9;
            break;
        }
        case 0x41:
        {
            const std::uint32_t first = static_cast<std::uint32_t>(GetWork(state, 1));
            const std::uint32_t mask = BitMask(GetWork(state, 1), GetWork(state, 3));
            const std::uint32_t current = static_cast<std::uint32_t>(GetWork(state, 5));
            SetWork(state, 7, (mask & current) >> (first & 31));
            state.pc += 9;
            break;
        }
        case 0x44:
            // "if actor exists": event actors are always present offline.
            state.pc += 5;
            break;
        case 0x9d:
        {
            const std::size_t size = InstructionSize(event.code, state.pc);
            if (size == 0)
                return { Step::Blocked, true, event.id, state.pc, op };
            if (!TableOp(state, size))
            {
                state.finished = true;
                return { Step::Done };
            }
            break;
        }
        default:
        {
            const std::size_t size = InstructionSize(event.code, state.pc);
            if (size == 0)
                return { Step::Blocked, true, event.id, state.pc, op };
            state.pc += size;
            break;
        }
        }
    }
    return { Step::Blocked };
}

void DismissMessage(State& state)
{
    state.messageOpen = false;
    state.pendingMessage = 0xffffffffu;
}

void SelectChoice(State& state, const std::uint32_t selected)
{
    state.zone[0] = selected;
    state.waitingChoice = false;
    state.pendingChoiceMessage = 0xffffffffu;
}
}
