#pragma once

#include <windows.h>

#include "application_settings.h"
#include "win32_theme.h"
#include "game_ui_config.h"

namespace ConfigDialog
{
enum class Command
{
    SetPath,
    AutoDetectPath,
    ResetPath,
    ShowPath,
    MipMappingChanged,
    BumpMappingChanged,
    LightingQualityChanged,
    DoorInteractionChanged,
    EnvironmentalAnimationChanged,
    DisplayChanged,
    ToggleGameMode,
    MirrorWorldChanged,
    DrawDistanceChanged,
    TextureCompressionChanged,
    ColorThemeChanged,
    PlayerNameplateChanged,
    ChatLogChanged,
    CollisionVisibilityChanged,
    SoundSettingsChanged,
    HardwareCursorChanged,
    ShowZoneObjects
};

struct Event
{
    Command command;
    int value = 0;
};

using EventCallback = void (*)(void* context, const Event& event);
using IsGameModeCallback = bool (*)(void* context);

struct State
{
    HWND owner = NULL;
    HWND window = NULL;
    const char* ffxiPath = NULL;
    ApplicationSettings::State* settings = nullptr;
    Win32Theme::State* theme = nullptr;
    GameUiConfig* gameUi = nullptr;
    void* callbackContext = nullptr;
    EventCallback eventCallback = nullptr;
    IsGameModeCallback isGameModeCallback = nullptr;
};

void Initialize(
    State& state,
    HWND owner,
    const char* ffxiPath,
    ApplicationSettings::State& settings,
    Win32Theme::State& theme,
    GameUiConfig& gameUi,
    EventCallback eventCallback,
    IsGameModeCallback isGameModeCallback,
    void* callbackContext = nullptr);
void Show(State& state);
void Close(State& state);
void Sync(State& state);
void ApplyTheme(State& state);
HWND Window(const State& state);
}
