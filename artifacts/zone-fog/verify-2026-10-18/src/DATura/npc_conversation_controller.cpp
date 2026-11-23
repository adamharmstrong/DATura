#include "stdafx.h"
#include "npc_conversation_controller.h"

#include <algorithm>
#include <utility>

namespace NpcConversationController
{
void Reset(State& state)
{
    state = {};
}

void BeginFallback(State& state, const std::string& dialogue)
{
    state.fallbackFrames.clear();
    state.fallbackIndex = 0;
    std::size_t start = 0;
    while (start <= dialogue.size())
    {
        const std::size_t end = dialogue.find('\n', start);
        std::string frame = dialogue.substr(start,
            end == std::string::npos ? std::string::npos : end - start);
        if (!frame.empty() && frame.back() == '\r')
            frame.pop_back();
        if (!frame.empty())
            state.fallbackFrames.push_back(std::move(frame));
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    if (state.fallbackFrames.empty())
        state.fallbackFrames.push_back(dialogue);
}

bool HasFallbackFrame(const State& state)
{
    return state.fallbackIndex < state.fallbackFrames.size();
}

const std::string* CurrentFallbackFrame(const State& state)
{
    return HasFallbackFrame(state) ? &state.fallbackFrames[state.fallbackIndex] : nullptr;
}

bool AdvanceFallback(State& state)
{
    if (state.fallbackIndex + 1 >= state.fallbackFrames.size())
        return false;
    ++state.fallbackIndex;
    return true;
}

Presentation PresentNext(State& state)
{
    for (;;)
    {
        const NpcEventRuntime::Outcome outcome = NpcEventRuntime::Advance(state.runtime);
        if (outcome.step == NpcEventRuntime::Step::Message)
        {
            const std::uint32_t id = state.runtime.pendingMessage;
            if (id < state.messages.size() && !state.messages[id].text.empty())
            {
                Presentation result;
                result.kind = PresentationKind::Message;
                result.text = state.messages[id].text;
                return result;
            }
            NpcEventRuntime::DismissMessage(state.runtime);
            continue;
        }
        if (outcome.step == NpcEventRuntime::Step::Choice)
        {
            const std::uint32_t id = state.runtime.pendingChoiceMessage;
            Presentation result;
            if (id < state.messages.size() &&
                FFXIEventMessages::SplitChoiceText(state.messages[id],
                    state.runtime.hiddenChoiceMask, result.prompt,
                    result.options, result.optionValues))
            {
                result.kind = PresentationKind::Choices;
                const auto selected = std::find(result.optionValues.begin(),
                    result.optionValues.end(), state.runtime.defaultChoice);
                result.selected = selected == result.optionValues.end() ? 0 :
                    static_cast<int>(selected - result.optionValues.begin());
                return result;
            }
            result.kind = PresentationKind::UnreadableChoice;
            return result;
        }
        Presentation result;
        if (outcome.step == NpcEventRuntime::Step::Blocked)
        {
            result.kind = PresentationKind::Blocked;
            result.unsupportedOpcode = outcome.unsupportedOpcode;
            result.eventId = outcome.eventId;
            result.programCounter = outcome.programCounter;
            result.opcode = outcome.opcode;
        }
        return result;
    }
}

void DismissMessage(State& state)
{
    NpcEventRuntime::DismissMessage(state.runtime);
}

void SelectChoice(State& state, const std::uint32_t value)
{
    NpcEventRuntime::SelectChoice(state.runtime, value);
}
}
