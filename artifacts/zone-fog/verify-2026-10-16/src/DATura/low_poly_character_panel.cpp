#include "stdafx.h"
#include "low_poly_character_panel.h"

#include "ffxi_player_customization_catalog.h"
#include "ffxi_player_customization_panel_state.h"
#include "win32_combo_box.h"
#include "win32_theme.h"
#include "win32_tool_window.h"

namespace LowPolyCharacterPanel
{
namespace
{
constexpr char kWindowClassName[] = "DATuraLowPolyPanelClass";
constexpr char kWindowTitle[] = "Low Poly Character";
constexpr int kWindowWidth = 522;
constexpr int kWindowHeight = 732;

const int kButtonIds[] =
{
    IDC_LP_LOAD, IDC_LP_SAVE, IDC_LP_RANDOM_CHARACTER,
    IDC_LP_RANDOM_WEAPONS, IDC_LP_RANDOM_ARMOR, IDC_LP_RANDOM_ACTION,
    IDC_LP_RANDOM_ALL, IDC_LP_PLAY, IDC_LP_STOP, IDC_LP_RESET
};

BOOL CALLBACK PrepareChildControl(HWND child, LPARAM)
{
    char className[32] = {};
    GetClassNameA(child, className, sizeof(className));
    const LONG_PTR style = GetWindowLongPtrA(child, GWL_STYLE);
    if (_stricmp(className, "BUTTON") == 0 &&
        (style & BS_TYPEMASK) == BS_GROUPBOX)
    {
        ShowWindow(child, SW_HIDE);
    }
    return TRUE;
}

void PrepareThemedControls(const HWND window)
{
    EnumChildWindows(window, PrepareChildControl, 0);
    for (const int id : kButtonIds)
    {
        const HWND button = GetDlgItem(window, id);
        if (!button)
            continue;
        LONG_PTR style = GetWindowLongPtrA(button, GWL_STYLE);
        style = (style & ~BS_TYPEMASK) | BS_OWNERDRAW;
        SetWindowLongPtrA(button, GWL_STYLE, style);
    }
}

void DrawSections(const State& state, const HDC dc)
{
    const bool dark = state.theme->dark;
    const HFONT font = state.theme->resources.sectionFont;
    const struct { RECT bounds; const char* title; } sections[] =
    {
        {{ 8,   8, 496, 102 }, "Character"},
        {{ 8, 110, 496, 268 }, "Weapons"},
        {{ 8, 276, 496, 452 }, "Armor"},
        {{ 8, 460, 496, 542 }, "Action"},
        {{ 8, 550, 496, 604 }, "Randomize"},
        {{ 8, 612, 496, 674 }, "Preset"}
    };
    for (const auto& section : sections)
        Win32Theme::DrawPanel(dc, section.bounds, section.title, dark, font);
}

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
        if (!state || !state->theme)
            return -1;
        FFXIPlayerCustomizationPanelState::CreateControls(window);
        PrepareThemedControls(window);
        Win32Theme::ApplyWindowTheme(window, *state->theme);
        Sync(*state);
        return 0;

    case WM_DRAWITEM:
        if (state && state->theme)
        {
            const DRAWITEMSTRUCT* draw =
                reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            Win32Theme::DrawButton(draw, state->theme->dark,
                state->theme->resources.font, false);
            return TRUE;
        }
        break;

    case WM_CTLCOLORSTATIC:
        if (state && state->theme)
        {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, Win32Theme::TextColor(state->theme->dark));
            return reinterpret_cast<LRESULT>(state->theme->resources.controlBrush);
        }
        break;

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        if (state && state->theme)
        {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetBkColor(dc, Win32Theme::EditColor(state->theme->dark));
            SetTextColor(dc, Win32Theme::TextColor(state->theme->dark));
            return reinterpret_cast<LRESULT>(state->theme->resources.editBrush);
        }
        break;

    case WM_ERASEBKGND:
        if (state && state->theme)
        {
            RECT client = {};
            GetClientRect(window, &client);
            FillRect(reinterpret_cast<HDC>(wParam), &client,
                state->theme->resources.windowBrush);
            return 1;
        }
        break;

    case WM_PAINT:
        if (state && state->theme)
        {
            PAINTSTRUCT paint = {};
            HDC dc = BeginPaint(window, &paint);
            FillRect(dc, &paint.rcPaint, state->theme->resources.windowBrush);
            DrawSections(*state, dc);
            EndPaint(window, &paint);
            return 0;
        }
        break;

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
                int& faceVariant, Win32Theme::State& theme,
                const EventHandler eventHandler,
                void* const eventContext)
{
    state.owner = owner;
    state.equipment = &equipment;
    state.faceVariant = &faceVariant;
    state.theme = &theme;
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
    if (state.theme)
        spec.backgroundBrush = state.theme->resources.windowBrush;
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
