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
    CustomTextureSettingsChanged,
    LightingQualityChanged,
    ShadowSettingsChanged,
    LightDirectionChanged,
    SceneClockChanged,
    DoorInteractionChanged,
    EnvironmentalAnimationChanged,
    DisplayChanged,
    ToggleGameMode,
    MirrorWorldChanged,
    DrawDistanceChanged,
    TextureCompressionChanged,
    RenderingResolutionChanged,
    MapTextureCompressionChanged,
    WeatherEffectsChanged,
    ColorThemeChanged,
    PlayerNameplateChanged,
    ChatLogChanged,
    CollisionVisibilityChanged,
    SoundSettingsChanged,
    HardwareCursorChanged,
    MouseCursorStyleChanged,
    MovementStyleChanged,
    TitleBackgroundChanged,
    ApplySettings
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
    ApplicationSettings::State* liveSettings = nullptr;
    ApplicationSettings::State pendingSettings;
    Win32Theme::State* theme = nullptr;
    GameUiConfig* gameUi = nullptr;
    GameUiConfig* liveGameUi = nullptr;
    GameUiConfig pendingGameUi = {};
    void* callbackContext = nullptr;
    EventCallback eventCallback = nullptr;
    IsGameModeCallback isGameModeCallback = nullptr;
    int categoryTab = 0;
    int activeTab = 0;
    int scrollOffsets[6] = {};
    bool editing = false;
    bool pendingDarkTheme = true;
    bool pendingGameMode = true;
    int capturingBinding = -1;
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
