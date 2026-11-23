#include "stdafx.h"
#include "npc_event_runtime.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace NpcEventRuntime
{
namespace
{
std::uint16_t Read16(const FFXIEventTable::Event& event, const std::size_t at)
{
    if (at + 2 > event.code.size()) return 0;
    return static_cast<std::uint16_t>(event.code[at] | (event.code[at + 1] << 8));
}

std::uint32_t ResolveOperand(const FFXIEventTable::Event& event,
                             const std::uint16_t operand)
{
    return (operand & 0x8000u) != 0 && (operand & 0x7fffu) < event.references.size()
        ? event.references[operand & 0x7fffu]
        : operand;
}

std::int32_t GetWork(State& state, const std::size_t operandOffset,
                     const std::int32_t shift = 0)
{
    if (!state.event) return 0;
    const std::uint32_t operand = static_cast<std::uint32_t>(
        static_cast<std::int32_t>(Read16(*state.event, state.pc + operandOffset)) + shift);
    if ((operand & 0x8000u) != 0)
        return static_cast<std::int32_t>(ResolveOperand(
            *state.event, static_cast<std::uint16_t>(operand)));
    if (operand >= 4096u)
    {
        const std::size_t index = operand - 4096u;
        return index < state.zone.size()
            ? static_cast<std::int32_t>(state.zone[index]) : 0;
    }
    return operand < state.local.size()
        ? static_cast<std::int32_t>(state.local[operand]) : 0;
}

void SetWork(State& state, const std::size_t operandOffset,
             const std::uint32_t value, const std::int32_t shift = 0)
{
    if (!state.event) return;
    const std::uint32_t operand = static_cast<std::uint32_t>(
        static_cast<std::int32_t>(Read16(*state.event, state.pc + operandOffset)) + shift);
    if (operand >= 4096u)
    {
        const std::size_t index = operand - 4096u;
        if (index < state.zone.size()) state.zone[index] = value;
    }
    else if (operand < state.local.size())
    {
        state.local[operand] = value;
    }
}

std::size_t FixedSize(const std::uint8_t op)
{
    switch (op)
    {
    case 0x00: case 0x21: case 0x23: case 0x25: case 0x58: case 0x7f:
        return 1;
    case 0x43:
        return 2;
    case 0x01: case 0x05: case 0x06: case 0x0b: case 0x0c:
    case 0x1a: case 0x1c: case 0x1d: case 0x34: case 0x35: case 0x48: case 0x63:
        return 3;
    case 0x03: case 0x6f: case 0x76: case 0x80: case 0x99:
        return 5;
    case 0x07: case 0x08: case 0x09: case 0x0a: case 0x0d: case 0x0e:
    case 0x0f: case 0x10: case 0x11: case 0x14: case 0x15: case 0x19:
    case 0x24: case 0x27: case 0x28: case 0x29: case 0x2a: case 0x2b:
    case 0x3e: case 0x49: case 0x6e:
        return 7;
    case 0x02: case 0x40: case 0x41: case 0x4e: case 0x6c:
        return 9;
    case 0x2c: case 0xb0:
        return 12;
    case 0x45:
        return 17;
    case 0x46:
        return 2;
    case 0x53: case 0x54: case 0x55: case 0x5b: case 0x66:
        return 9;
    case 0x5d:
        return 5;
    case 0x70:
        return 1;
    default:
        return 0;
    }
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
    case 0x11: value = a >> (b & 31); break;
    case 0x14: value = a * b; break;
    case 0x15: value = (a && b) ? a / b : 0; break;
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
    for (int budget = 0; budget < 100000; ++budget)
    {
        if (state.pc >= state.event->code.size())
        {
            state.finished = true;
            return { Step::Done };
        }
        const std::uint8_t op = state.event->code[state.pc];
        switch (op)
        {
        case 0x00: case 0x21:
            state.finished = true;
            return { Step::Done };
        case 0x01:
            state.pc = Read16(*state.event, state.pc + 1);
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
            if (state.jumpIndex >= state.jumps.size())
            {
                state.finished = true;
                return { Step::Done };
            }
            state.jumps[state.jumpIndex++] = static_cast<std::uint16_t>(state.pc + 3);
            state.pc = Read16(*state.event, state.pc + 1);
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
            if (state.pc + 12 > state.event->code.size() || state.event->code[state.pc + 1] != 0)
                return { Step::Blocked };
            state.pendingMessage = static_cast<std::uint32_t>(GetWork(state, 10));
            state.messageOpen = true;
            state.pc += 12;
            break;
        case 0x23:
            if (state.messageOpen)
                return { Step::Message };
            state.pc += 1;
            break;
        case 0x24:
            state.pendingChoiceMessage = static_cast<std::uint32_t>(GetWork(state, 1));
            state.defaultChoice = static_cast<std::uint32_t>(
                std::max<std::int32_t>(0, GetWork(state, 3)));
            state.hiddenChoiceMask = static_cast<std::uint32_t>(GetWork(state, 5));
            state.waitingChoice = true;
            state.pc += 7;
            break;
        case 0x25: case 0x7f:
            if (state.waitingChoice)
                return { Step::Choice };
            state.pc += 1;
            break;
        case 0x3e:
        {
            const std::int32_t bit = GetWork(state, 3);
            const std::int32_t word = GetWork(state, 1, bit >> 5);
            state.pc = ((word & (1 << (bit & 31))) == 0)
                ? Read16(*state.event, state.pc + 5)
                : state.pc + 7;
            break;
        }
        default:
        {
            const std::size_t size = FixedSize(op);
            if (size == 0)
                return { Step::Blocked, true, state.event->id, state.pc, op };
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
