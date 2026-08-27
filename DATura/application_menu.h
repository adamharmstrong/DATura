#pragma once

#include <windows.h>

#include "application_settings.h"
#include "win32_theme.h"

namespace ApplicationMenu
{
enum class Action
{
    None,
    OpenDat,
    OpenDatSet,
    ReturnToTitle,
    Exit,
    ShowConfig,
    SetPath,
    DetectPath,
    ResetPath,
    ShowPath,
    ToggleMipMapping,
    ToggleBumpMapping,
    CycleEnvironmentalAnimation,
    ToggleMirrorWorld,
    ToggleGameMode,
    ShowZoneObjects,
    CycleWeather,
    ShowCurrentZoneResources,
    OpenResourceDat,
    ShowTextureViewer,
    ShowCompanionBrowser,
    ShowAudioPlayer,
    StopAudio,
    CustomizePlayer
};

enum class SelectionType
{
    None,
    Zone,
    PrototypeArea,
    PlayerRace,
    CreationModel,
    NpcModel,
    MonsterModel
};

struct Command
{
    Action action = Action::None;
    SelectionType selectionType = SelectionType::None;
    int selectionIndex = -1;
};

struct State
{
    HWND owner = NULL;
    HMENU menu = NULL;
    const char* ffxiPath = NULL;
    Win32Theme::State* theme = nullptr;
};

void Initialize(State& state, HWND owner, const char* ffxiPath, Win32Theme::State& theme);
HMENU Build(State& state, const ApplicationSettings::State& settings, bool gameMode);
void Sync(State& state, const ApplicationSettings::State& settings, bool gameMode);
void RefreshTheme(State& state);
void Release(State& state);
Command Decode(UINT commandId);
UINT CommandId(Action action);
}
