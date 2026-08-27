#include "stdafx.h"
#include "high_poly_creation_panel.h"

#include "ffxi_creation_panel_state.h"
#include "win32_tool_window.h"

#include <limits>

namespace HighPolyCreationPanel
{
namespace
{
constexpr char kWindowClassName[] = "DATuraHighPolyCreationPanelClass";
constexpr char kWindowTitle[] = "High Poly Character Creation";
constexpr int kWindowWidth = 378;
constexpr int kWindowHeight = 406;

State* StateForWindow(const HWND window)
{
    return reinterpret_cast<State*>(
        GetWindowLongPtrA(window, GWLP_USERDATA));
}

void Emit(State& state, const Command command)
{
    if (state.eventHandler)
        state.eventHandler(state.eventContext, command);
}

LRESULT CALLBACK WindowProcedure(const HWND window, const UINT message,
                                 const WPARAM wParam, const LPARAM lParam)
{
    State* state = StateForWindow(window);
    if (message == WM_NCCREATE)
    {
        const CREATESTRUCTA* create =
            reinterpret_cast<const CREATESTRUCTA*>(lParam);
        state = static_cast<State*>(create->lpCreateParams);
        SetWindowLongPtrA(window, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(state));
        if (state)
            state->window = window;
    }

    switch (message)
    {
    case WM_CREATE:
        if (!state)
            return -1;
        FFXICreationPanelState::CreateControls(
            window, state->characterName ? state->characterName : "");
        Sync(*state);
        return 0;

    case WM_COMMAND:
        if (state)
        {
            const WORD id = LOWORD(wParam);
            const WORD code = HIWORD(wParam);
            if (code == BN_CLICKED)
            {
                switch (id)
                {
                case IDC_HP_TITLE:
                    Emit(*state, Command::ReturnToTitle);
                    return 0;
                case IDC_HP_NEXT:
                    Emit(*state, Command::ChooseNation);
                    return 0;
                case IDC_HP_SAVE:
                    Emit(*state, Command::SaveCharacter);
                    return 0;
                case IDC_HP_ANIMATED_CAMERA:
                    Pull(*state);
                    Sync(*state);
                    return 0;
                }
            }

            if (code == CBN_SELCHANGE)
            {
                Emit(*state, Command::SelectionChanged);
                return 0;
            }
        }
        break;

    case WM_CLOSE:
        ShowWindow(window, SW_HIDE);
        return 0;

    case WM_DESTROY:
        if (state && state->window == window)
            state->window = NULL;
        SetWindowLongPtrA(window, GWLP_USERDATA, 0);
        return 0;
    }

    return DefWindowProcA(window, message, wParam, lParam);
}
}

void Initialize(State& state, const HWND owner,
                FFXICreationSelection& selection, int& animationIndex,
                bool& animatedCamera, char* const characterName,
                const std::size_t characterNameCapacity,
                const EventHandler eventHandler, void* const eventContext)
{
    state.owner = owner;
    state.selection = &selection;
    state.animationIndex = &animationIndex;
    state.animatedCamera = &animatedCamera;
    state.characterName = characterName;
    state.characterNameCapacity = characterNameCapacity;
    state.eventHandler = eventHandler;
    state.eventContext = eventContext;
}

HWND Window(const State& state) noexcept
{
    return state.window;
}

void Show(State& state, const bool activate)
{
    if (state.window)
    {
        Sync(state);
        Win32ToolWindow::Show(state.window, activate);
        return;
    }

    Win32ToolWindow::Spec spec =
    {
        WindowProcedure, kWindowClassName, kWindowTitle,
        kWindowWidth, kWindowHeight
    };
    spec.createParameter = &state;
    state.window = Win32ToolWindow::Create(state.owner, spec);
    if (state.window)
        Win32ToolWindow::Show(state.window, false);
}

void Hide(const State& state)
{
    if (state.window)
        ShowWindow(state.window, SW_HIDE);
}

void Sync(State& state)
{
    if (state.window && state.selection && state.animationIndex &&
        state.animatedCamera)
    {
        FFXICreationPanelState::SyncControls(
            state.window, *state.selection, *state.animationIndex,
            *state.animatedCamera);
    }
}

void Pull(State& state)
{
    if (!state.window)
        return;

    if (state.selection && state.animationIndex && state.animatedCamera)
    {
        FFXICreationPanelState::PullControls(
            state.window, *state.selection, *state.animationIndex,
            *state.animatedCamera);
    }

    if (state.characterName && state.characterNameCapacity > 0)
    {
        const std::size_t maximumInt =
            static_cast<std::size_t>((std::numeric_limits<int>::max)());
        const int capacity = static_cast<int>(
            state.characterNameCapacity > maximumInt
                ? maximumInt
                : state.characterNameCapacity);
        GetWindowTextA(GetDlgItem(state.window, IDC_HP_NAME),
                       state.characterName, capacity);
    }
}

void Destroy(State& state)
{
    if (state.window && IsWindow(state.window))
        DestroyWindow(state.window);
    state.window = NULL;
}
}
