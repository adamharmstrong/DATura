#pragma once

#include "ffxi_event_messages.h"
#include "ffxi_event_table.h"
#include "npc_event_runtime.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace NpcConversationController
{
struct State
{
    // The runtime keeps a pointer into scripts, so both are owned together.
    std::vector<FFXIEventTable::EntityScripts> scripts;
    std::vector<FFXIEventMessages::Entry> messages;
    NpcEventRuntime::State runtime;
    std::vector<std::string> fallbackFrames;
    std::size_t fallbackIndex = 0;
};

enum class PresentationKind
{
    None,
    Message,
    Choices,
    UnreadableChoice,
    Blocked,
};

struct Presentation
{
    PresentationKind kind = PresentationKind::None;
    std::string text;
    std::string prompt;
    std::vector<std::string> options;
    std::vector<std::uint32_t> optionValues;
    int selected = 0;
    bool unsupportedOpcode = false;
    std::uint16_t eventId = 0xffff;
    std::size_t programCounter = 0;
    std::uint8_t opcode = 0;
};

void Reset(State& state);
void BeginFallback(State& state, const std::string& dialogue);
bool HasFallbackFrame(const State& state);
const std::string* CurrentFallbackFrame(const State& state);
bool AdvanceFallback(State& state);
Presentation PresentNext(State& state);
void DismissMessage(State& state);
void SelectChoice(State& state, std::uint32_t value);
}
