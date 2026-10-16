#pragma once

#include "ffxi_event_table.h"

#include <array>
#include <cstddef>
#include <cstdint>

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
    std::array<std::uint16_t, 8> jumps = {};
    std::size_t pc = 0;
    std::size_t jumpIndex = 0;
    std::uint32_t pendingMessage = 0xffffffffu;
    std::uint32_t pendingChoiceMessage = 0xffffffffu;
    std::uint32_t defaultChoice = 0;
    std::uint32_t hiddenChoiceMask = 0;
    bool messageOpen = false;
    bool waitingChoice = false;
    bool finished = false;
};

void Begin(State& state, const FFXIEventTable::Event& event);
Outcome Advance(State& state);
void DismissMessage(State& state);
void SelectChoice(State& state, std::uint32_t selected);
}
