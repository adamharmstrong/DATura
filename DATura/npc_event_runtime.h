#pragma once

#include "ffxi_event_table.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace NpcEventRuntime
{
enum class Step
{
    Done,
    Message,
    Choice,
    Blocked,
};

struct Outcome
{
    Step step = Step::Done;
    bool unsupportedOpcode = false;
    std::uint16_t eventId = 0xffff;
    std::size_t programCounter = 0;
    std::uint8_t opcode = 0;
};

struct State
{
    // Points into the owning conversation's event-script collection.
    const FFXIEventTable::Event* event = nullptr;
    std::array<std::uint32_t, 80> local = {};
    std::array<std::uint32_t, 96> zone = {};
    std::array<std::uint32_t, 96> zoneMemorize = {};
    std::array<std::uint32_t, 96> zone1700 = {};
    std::array<std::uint16_t, 8> jumps = {};
    std::size_t pc = 0;
    std::size_t jumpIndex = 0;
    std::uint32_t pendingMessage = 0xffffffffu;
    std::uint32_t pendingChoiceMessage = 0xffffffffu;
    std::uint32_t defaultChoice = 0;
    std::uint32_t hiddenChoiceMask = 0;
    std::uint32_t randomState = 0x2545f491u;
    bool messageOpen = false;
    bool waitingChoice = false;
    bool finished = false;
};

// Byte length of the event VM instruction at pc, following the retail PC
// client's handlers (atom0s/xievents, cross-checked against the PS2 client).
// Returns 0 for opcodes with no handler or an unknown sub-mode.
std::size_t InstructionSize(const std::vector<std::uint8_t>& code, std::size_t pc);

void Begin(State& state, const FFXIEventTable::Event& event);
Outcome Advance(State& state);
void DismissMessage(State& state);
void SelectChoice(State& state, std::uint32_t selected);
}
