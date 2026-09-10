#include "stdafx.h"
#include "low_poly_character_panel.h"

#include "ffxi_player_customization_catalog.h"
#include "ffxi_player_customization_panel_state.h"
#include "win32_combo_box.h"
#include "win32_tool_window.h"

namespace LowPolyCharacterPanel
{
namespace
{
constexpr char kWindowClassName[] = "DATuraLowPolyPanelClass";
constexpr char kWindowTitle[] = "Low Poly Character";
constexpr int kWindowWidth = 482;
constexpr int kWindowHeight = 704;

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
        FFXIPlayerCustomizationPanelState::CreateControls(window);
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
                case IDC_LP_LOAD: Emit(*state, Command::LoadPreset); return 0;
                case IDC_LP_SAVE: Emit(*state, Command::SavePreset); return 0;
                case IDC_LP_RANDOM_CHARACTER: Emit(*state, Command::RandomizeCharacter); return 0;
                case IDC_LP_RANDOM_WEAPONS: Emit(*state, Command::RandomizeWeapons); return 0;
                case IDC_LP_RANDOM_ARMOR: Emit(*state, Command::RandomizeArmor); return 0;
                case IDC_LP_RANDOM_ACTION: Emit(*state, Command::RandomizeAction); return 0;
                case IDC_LP_RANDOM_ALL: Emit(*state, Command::RandomizeAll); return 0;
                case IDC_LP_PLAY: Emit(*state, Command::Play); return 0;
                case IDC_LP_STOP: Emit(*state, Command::Stop); return 0;
                case IDC_LP_RESET: Emit(*state, Command::Reset); return 0;
                }
            }

            if (id == IDC_LP_ANIM_MODE && code == CBN_SELCHANGE &&
                state->equipment && state->faceVariant)
            {
                Pull(*state);
                HWND animationBank = GetDlgItem(window, IDC_LP_ANIM_BANK);
                FFXIPlayerCustomizationCatalog::FillAnimation(
                    animationBank, -1, state->equipment->raceIndex,
                    state->equipment->animationMode);
                state->equipment->animationBank =
                    Win32ComboBox::ComboGetSelectedData(animationBank);
                Emit(*state, Command::SelectionChanged);
                return 0;
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

void Initialize(State& state, const HWND owner, PlayerEquipState& equipment,
                int& faceVariant, const EventHandler eventHandler,
                void* const eventContext)
{
    state.owner = owner;
    state.equipment = &equipment;
    state.faceVariant = &faceVariant;
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
    if (state.window && state.equipment && state.faceVariant)
    {
        FFXIPlayerCustomizationPanelState::SyncControls(
            state.window, *state.equipment, *state.faceVariant);
    }
}

void Pull(State& state)
{
    if (state.window && state.equipment && state.faceVariant)
    {
        FFXIPlayerCustomizationPanelState::PullControls(
            state.window, *state.equipment, *state.faceVariant);
    }
}

void Destroy(State& state)
{
    if (state.window && IsWindow(state.window))
        DestroyWindow(state.window);
    state.window = NULL;
}
}
