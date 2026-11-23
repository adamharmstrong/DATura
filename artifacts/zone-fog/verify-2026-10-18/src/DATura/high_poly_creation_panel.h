#pragma once

#include "ffxi_creation_selection.h"

#include <cstddef>
#include <windows.h>

namespace HighPolyCreationPanel
{
enum class Command
{
    ReturnToTitle,
    ChooseNation,
    SaveCharacter,
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
    FFXICreationSelection* selection = nullptr;
    int* animationIndex = nullptr;
    bool* animatedCamera = nullptr;
    char* characterName = nullptr;
    std::size_t characterNameCapacity = 0;
    EventHandler eventHandler = nullptr;
    void* eventContext = nullptr;
};

void Initialize(State& state, HWND owner, FFXICreationSelection& selection,
                int& animationIndex, bool& animatedCamera,
                char* characterName, std::size_t characterNameCapacity,
                EventHandler eventHandler, void* eventContext = nullptr);
HWND Window(const State& state) noexcept;
void Show(State& state, bool activate = true);
void Hide(const State& state);
void Sync(State& state);
void Pull(State& state);
void Destroy(State& state);
}
