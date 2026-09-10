#pragma once

#include "ffxi_player_customization_state.h"

#include <windows.h>

namespace LowPolyCharacterPanel
{
enum class Command
{
    LoadPreset,
    SavePreset,
    RandomizeCharacter,
    RandomizeWeapons,
    RandomizeArmor,
    RandomizeAction,
    RandomizeAll,
    Play,
    Stop,
    Reset,
    SelectionChanged
};

using EventHandler = void (*)(void* context, Command command);

struct State
{
    State() = default;
    State(const State&) = delete;
    State& operator=(const State&) = delete;
    State(State&&) = delete;
    State& operator=(State&&) = delete;

    HWND owner = NULL;
    HWND window = NULL;
    PlayerEquipState* equipment = nullptr;
    int* faceVariant = nullptr;
    EventHandler eventHandler = nullptr;
    void* eventContext = nullptr;
};

void Initialize(State& state, HWND owner, PlayerEquipState& equipment,
                int& faceVariant, EventHandler eventHandler,
                void* eventContext = nullptr);
HWND Window(const State& state) noexcept;
void Show(State& state, bool activate = true);
void Hide(const State& state);
void Sync(State& state);
void Pull(State& state);
void Destroy(State& state);
}
