#include "stdafx.h"
#include "application_menu.h"

#include "noesis_rapi.h"
#include "model_ff11.h"
#include "character_creation_dat_table.h"
#include "character_dat_table.h"
#include "ffxi_paths.h"
#include "npc_monster_dat_table.h"
#include "prototype_area_table.h"
#include "win32_owner_draw_menu.h"
#include "zone_dat_table.h"

namespace
{
enum CommandId
{
    FileOpenDat = 1001,
    FileOpenDatSet = 1002,
    FileExit = 1003,
    FileReturnTitle = 1004,
    SettingsSetPath = 2001,
    SettingsResetPath = 2002,
    SettingsDetectPath = 2003,
    SettingsShowPath = 2004,
    SettingsMipMapping = 2005,
    SettingsBumpMapping = 2006,
    SettingsEnvironmentalAnimation = 2007,
    SettingsMirrorWorld = 2008,
    SettingsConfig = 2009,
    ZoneBase = 3000,
    CreationModelBase = 4000,
    PlayerRaceBase = 5000,
    ViewToggleGameMode = 6001,
    PlayerCustomize = 6002,
    ViewZoneObjects = 6003,
    ViewCycleWeather = 6010,
    ResourceCurrentZone = 6101,
    ResourceOpenDat = 6102,
    AudioPlayer = 6201,
    AudioStop = 6202,
    TextureViewer = 6251,
    CompanionBrowser = 6261,
    PrototypeAreaBase = 6300,
    NpcModelBase = 10000,
    MonsterModelBase = 12000
};

HMENU BuildStandaloneModelMenu(
    const char* ffxiPath,
    const FFXIStandaloneModelGroup* groups,
    const int groupCount,
    const UINT commandBase)
{
    const HMENU catalogMenu = CreatePopupMenu();
    int flatIndex = 0;
    for (int groupIndex = 0; groupIndex < groupCount; ++groupIndex)
    {
        const FFXIStandaloneModelGroup& group = groups[groupIndex];
        const HMENU groupMenu = CreatePopupMenu();
        for (int entryIndex = 0; entryIndex < group.count; ++entryIndex, ++flatIndex)
        {
            const FFXIStandaloneModelEntry& entry = group.entries[entryIndex];
            char fullPath[MAX_PATH] = {};
            FFXIPath::BuildFullPath(ffxiPath, entry.dat, fullPath, sizeof(fullPath));
            const UINT flags = FFXIPath::FileExists(fullPath)
                ? MF_STRING
                : MF_STRING | MF_GRAYED;
            AppendMenuA(groupMenu, flags, commandBase + flatIndex, entry.label);
        }
        AppendMenuA(catalogMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(groupMenu), group.name);
    }
    return catalogMenu;
}

void ApplyBackgroundRecursive(const HMENU menu, const HBRUSH backgroundBrush)
{
    if (!menu)
        return;

    MENUINFO menuInfo = {};
    menuInfo.cbSize = sizeof(menuInfo);
    menuInfo.fMask = MIM_BACKGROUND;
    menuInfo.hbrBack = backgroundBrush;
    SetMenuInfo(menu, &menuInfo);

    const int count = GetMenuItemCount(menu);
    for (int i = 0; i < count; ++i)
        ApplyBackgroundRecursive(GetSubMenu(menu, i), backgroundBrush);
}

ApplicationMenu::Action DecodeAction(const UINT commandId)
{
    using ApplicationMenu::Action;
    switch (commandId)
    {
    case FileOpenDat: return Action::OpenDat;
    case FileOpenDatSet: return Action::OpenDatSet;
    case FileReturnTitle: return Action::ReturnToTitle;
    case FileExit: return Action::Exit;
    case SettingsConfig: return Action::ShowConfig;
    case SettingsSetPath: return Action::SetPath;
    case SettingsDetectPath: return Action::DetectPath;
    case SettingsResetPath: return Action::ResetPath;
    case SettingsShowPath: return Action::ShowPath;
    case SettingsMipMapping: return Action::ToggleMipMapping;
    case SettingsBumpMapping: return Action::ToggleBumpMapping;
    case SettingsEnvironmentalAnimation: return Action::CycleEnvironmentalAnimation;
    case SettingsMirrorWorld: return Action::ToggleMirrorWorld;
    case ViewToggleGameMode: return Action::ToggleGameMode;
    case ViewZoneObjects: return Action::ShowZoneObjects;
    case ViewCycleWeather: return Action::CycleWeather;
    case ResourceCurrentZone: return Action::ShowCurrentZoneResources;
    case ResourceOpenDat: return Action::OpenResourceDat;
    case TextureViewer: return Action::ShowTextureViewer;
    case CompanionBrowser: return Action::ShowCompanionBrowser;
    case AudioPlayer: return Action::ShowAudioPlayer;
    case AudioStop: return Action::StopAudio;
    case PlayerCustomize: return Action::CustomizePlayer;
    default: return Action::None;
    }
}
}

namespace ApplicationMenu
{
void Initialize(
    State& state,
    const HWND owner,
    const char* const ffxiPath,
    Win32Theme::State& theme)
{
    state.owner = owner;
    state.ffxiPath = ffxiPath;
    state.theme = &theme;
}

HMENU Build(State& state, const ApplicationSettings::State& settings, const bool gameMode)
{
    const HMENU menuBar = CreateMenu();
    const HMENU fileMenu = CreatePopupMenu();
    const HMENU settingsMenu = CreatePopupMenu();
    const HMENU viewMenu = CreatePopupMenu();
    const HMENU resourceMenu = CreatePopupMenu();
    const HMENU imageMenu = CreatePopupMenu();
    const HMENU audioMenu = CreatePopupMenu();
    const HMENU companionMenu = CreatePopupMenu();

    AppendMenuA(fileMenu, MF_STRING, FileOpenDat, "Open DAT...\tCtrl+O");
    AppendMenuA(fileMenu, MF_STRING, FileOpenDatSet, "Open DAT Set...");
    AppendMenuA(fileMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(fileMenu, MF_STRING, FileReturnTitle, "Return to Title Screen");
    AppendMenuA(fileMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(fileMenu, MF_STRING, FileExit, "Exit\tAlt+F4");

    AppendMenuA(settingsMenu, MF_STRING, SettingsConfig, "Config...");
    AppendMenuA(settingsMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(settingsMenu, MF_STRING, SettingsSetPath, "Set FFXI Path...");
    AppendMenuA(settingsMenu, MF_STRING, SettingsDetectPath, "Auto-Detect Path");
    AppendMenuA(settingsMenu, MF_STRING, SettingsResetPath, "Reset to Default Path");
    AppendMenuA(settingsMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(settingsMenu, MF_STRING, SettingsShowPath, "Show Current Path");
    AppendMenuA(settingsMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(settingsMenu, MF_STRING, SettingsMipMapping, "Enable MIP Mapping");
    AppendMenuA(settingsMenu, MF_STRING, SettingsBumpMapping, "Enable Bump Mapping");
    AppendMenuA(settingsMenu, MF_STRING, SettingsMirrorWorld, "Mirror World Zones");
    AppendMenuA(settingsMenu, MF_STRING, SettingsEnvironmentalAnimation,
        "Vegetation Animation: Smooth");

    AppendMenuA(viewMenu, MF_STRING, ViewToggleGameMode, "Toggle Edit/Game Mode\tF");
    AppendMenuA(viewMenu, MF_STRING, ViewZoneObjects, "Zone Objects...");
    AppendMenuA(viewMenu, MF_STRING, ViewCycleWeather, "Cycle Zone Weather\tV");

    AppendMenuA(resourceMenu, MF_STRING, ResourceCurrentZone, "Current Zone Dialog / NPCs...");
    AppendMenuA(resourceMenu, MF_STRING, ResourceOpenDat, "Open Resource DAT...");
    AppendMenuA(imageMenu, MF_STRING, TextureViewer, "Image / Texture Viewer...");
    AppendMenuA(audioMenu, MF_STRING, AudioPlayer, "Music / SFX Player...");
    AppendMenuA(audioMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(audioMenu, MF_STRING, AudioStop, "Stop Playback");
    AppendMenuA(companionMenu, MF_STRING, CompanionBrowser, "Companion Browser...");

    const HMENU zonesMenu = CreatePopupMenu();
    struct ZoneMenuCategory
    {
        std::string name;
        HMENU menu;
    };
    std::vector<ZoneMenuCategory> zoneCategories;
    for (int groupIndex = 0; groupIndex < kFFXIZoneGroupCount; ++groupIndex)
    {
        const FFXIZoneGroup& group = kFFXIZoneGroups[groupIndex];
        const std::string groupName = group.name ? group.name : "";
        const size_t slash = groupName.find(" / ");
        const std::string categoryName = slash != std::string::npos
            ? groupName.substr(0, slash)
            : "Other";
        const std::string regionName = slash != std::string::npos
            ? groupName.substr(slash + 3)
            : groupName;

        HMENU categoryMenu = NULL;
        for (const ZoneMenuCategory& category : zoneCategories)
        {
            if (category.name == categoryName)
            {
                categoryMenu = category.menu;
                break;
            }
        }
        if (!categoryMenu)
        {
            categoryMenu = CreatePopupMenu();
            zoneCategories.push_back({ categoryName, categoryMenu });
            AppendMenuA(zonesMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(categoryMenu),
                categoryName.c_str());
        }

        const HMENU regionMenu = CreatePopupMenu();
        for (int zoneIndex = 0; zoneIndex < group.count; ++zoneIndex)
        {
            const int zoneId = group.ids[zoneIndex];
            const FFXIZoneEntry* zone = FFXIZone::FindByID(zoneId);
            if (!zone)
                continue;

            char label[128] = {};
            char fullPath[MAX_PATH] = {};
            const bool hasModel = FFXIZone::HasModel(zone) ||
                FFXIPath::ResolveZoneModelPath(
                    state.ffxiPath, zoneId, fullPath, sizeof(fullPath));
            const UINT flags = hasModel ? MF_STRING : MF_STRING | MF_GRAYED;
            sprintf_s(label, "[%d] %s", zoneId, zone->name);
            AppendMenuA(regionMenu, flags, ZoneBase + zoneId, label);
        }
        AppendMenuA(categoryMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(regionMenu),
            regionName.c_str());
    }

    HMENU internalCategory = NULL;
    for (const ZoneMenuCategory& category : zoneCategories)
    {
        if (category.name == "Internal")
        {
            internalCategory = category.menu;
            break;
        }
    }
    if (!internalCategory)
    {
        internalCategory = CreatePopupMenu();
        AppendMenuA(zonesMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(internalCategory), "Internal");
    }

    const HMENU prototypeAreas = CreatePopupMenu();
    for (int i = 0; i < kFFXIPrototypeAreaCount; ++i)
    {
        AppendMenuA(prototypeAreas, MF_STRING, PrototypeAreaBase + i,
            kFFXIPrototypeAreas[i].name);
    }
    AppendMenuA(internalCategory, MF_POPUP, reinterpret_cast<UINT_PTR>(prototypeAreas),
        "Prototype Areas");

    const HMENU creationMenu = CreatePopupMenu();
    int flatIndex = 0;
    for (int raceIndex = 0; raceIndex < kFFXICreationRaceCount; ++raceIndex)
    {
        const FFXICreationRace& race = kFFXICreationRaces[raceIndex];
        const HMENU raceMenu = CreatePopupMenu();
        for (int entryIndex = 0; entryIndex < race.count; ++entryIndex)
        {
            const char* entryLabel = race.entries[entryIndex].label
                ? race.entries[entryIndex].label
                : "Face";
            const char* faceLabel = strstr(entryLabel, " + ");
            faceLabel = faceLabel ? faceLabel + 3 : entryLabel;

            const HMENU faceMenu = CreatePopupMenu();
            for (int variant = 0; variant < 2 && entryIndex + variant < race.count; ++variant)
            {
                const FFXICreationEntry& entry = race.entries[entryIndex + variant];
                const char* equipmentLabel = entry.label &&
                    strncmp(entry.label, "Initial Equipment", 17) == 0
                    ? "Initial Equipment"
                    : "No Equipment";
                AppendMenuA(faceMenu, MF_STRING, CreationModelBase + flatIndex, equipmentLabel);
                ++flatIndex;
            }
            AppendMenuA(raceMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(faceMenu), faceLabel);
            ++entryIndex;
        }
        AppendMenuA(creationMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(raceMenu), race.name);
    }

    const HMENU playerMenu = CreatePopupMenu();
    AppendMenuA(playerMenu, MF_STRING, PlayerCustomize, "Equipment / Animation...");
    AppendMenuA(playerMenu, MF_SEPARATOR, 0, NULL);
    for (int raceIndex = 0; raceIndex < kFFXICharRaceCount; ++raceIndex)
    {
        AppendMenuA(playerMenu, MF_STRING, PlayerRaceBase + raceIndex,
            kFFXICharRaces[raceIndex].name);
    }

    const HMENU npcMenu = BuildStandaloneModelMenu(
        state.ffxiPath, kFFXINpcModelGroups, kFFXINpcModelGroupCount, NpcModelBase);
    const HMENU monsterMenu = BuildStandaloneModelMenu(
        state.ffxiPath, kFFXIMonsterModelGroups, kFFXIMonsterModelGroupCount, MonsterModelBase);
    const HMENU assetsMenu = CreatePopupMenu();
    const HMENU playerModelsMenu = CreatePopupMenu();
    const HMENU toolsMenu = CreatePopupMenu();

    AppendMenuA(playerModelsMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(creationMenu), "High Poly");
    AppendMenuA(playerModelsMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(playerMenu), "Low Poly");
    AppendMenuA(assetsMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(zonesMenu), "Zones");
    AppendMenuA(assetsMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(playerModelsMenu), "Player Models");
    AppendMenuA(assetsMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(assetsMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(npcMenu), "NPCs");
    AppendMenuA(assetsMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(monsterMenu), "Monsters");
    AppendMenuA(assetsMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(companionMenu), "Companions");
    AppendMenuA(toolsMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(resourceMenu), "Resources");
    AppendMenuA(toolsMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(imageMenu), "Images / Textures");
    AppendMenuA(toolsMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(audioMenu), "Audio");
    AppendMenuA(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), "File");
    AppendMenuA(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(assetsMenu), "Assets");
    AppendMenuA(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(toolsMenu), "Tools");
    AppendMenuA(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(viewMenu), "View");
    AppendMenuA(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(settingsMenu), "Settings");

    Win32OwnerDrawMenu::StyleMenuBar(menuBar);
    state.menu = menuBar;
    Sync(state, settings, gameMode);
    RefreshTheme(state);
    return menuBar;
}

void Sync(State& state, const ApplicationSettings::State& settings, const bool gameMode)
{
    if (!state.menu)
        return;

    CheckMenuItem(state.menu, SettingsMipMapping,
        MF_BYCOMMAND | (settings.enableMipMapping ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(state.menu, SettingsBumpMapping,
        MF_BYCOMMAND | (settings.enableBumpMapping ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(state.menu, SettingsMirrorWorld,
        MF_BYCOMMAND | (settings.mirrorWorldZones ? MF_CHECKED : MF_UNCHECKED));
    char environmentalAnimationText[96] = {};
    sprintf_s(environmentalAnimationText, "Vegetation Animation: %s",
        ApplicationSettings::EnvironmentalAnimationModeName(
            settings.environmentalAnimationMode));
    ModifyMenuA(state.menu, SettingsEnvironmentalAnimation,
        MF_BYCOMMAND | MF_STRING |
            (settings.environmentalAnimationMode !=
                ApplicationSettings::EnvironmentalAnimationOff ? MF_CHECKED : MF_UNCHECKED),
        SettingsEnvironmentalAnimation, environmentalAnimationText);
    CheckMenuItem(state.menu, ViewToggleGameMode,
        MF_BYCOMMAND | (gameMode ? MF_CHECKED : MF_UNCHECKED));
}

void RefreshTheme(State& state)
{
    if (!state.menu || !state.theme)
        return;
    ApplyBackgroundRecursive(state.menu, state.theme->resources.controlBrush);
    if (state.owner)
    {
        DrawMenuBar(state.owner);
        RedrawWindow(state.owner, NULL, NULL, RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW);
    }
}

void Release(State& state)
{
    if (state.menu)
        DestroyMenu(state.menu);
    state.menu = NULL;
}

Command Decode(const UINT commandId)
{
    Command command;
    command.action = DecodeAction(commandId);
    if (command.action != Action::None)
        return command;

    const int numericId = static_cast<int>(commandId);

    const int npcCount = FFXIStandaloneModel_TotalEntries(
        kFFXINpcModelGroups, kFFXINpcModelGroupCount);
    if (numericId >= NpcModelBase && numericId < NpcModelBase + npcCount)
    {
        command.selectionType = SelectionType::NpcModel;
        command.selectionIndex = numericId - NpcModelBase;
    }
    else
    {
        const int monsterCount = FFXIStandaloneModel_TotalEntries(
            kFFXIMonsterModelGroups, kFFXIMonsterModelGroupCount);
        if (numericId >= MonsterModelBase && numericId < MonsterModelBase + monsterCount)
        {
            command.selectionType = SelectionType::MonsterModel;
            command.selectionIndex = numericId - MonsterModelBase;
        }
        else if (numericId >= PrototypeAreaBase &&
                 numericId < PrototypeAreaBase + kFFXIPrototypeAreaCount)
        {
            command.selectionType = SelectionType::PrototypeArea;
            command.selectionIndex = numericId - PrototypeAreaBase;
        }
        else if (numericId >= ZoneBase && numericId < ZoneBase + kFFXIZoneCount)
        {
            command.selectionType = SelectionType::Zone;
            command.selectionIndex = numericId - ZoneBase;
        }
        else if (numericId >= PlayerRaceBase && numericId < PlayerRaceBase + kFFXICharRaceCount)
        {
            command.selectionType = SelectionType::PlayerRace;
            command.selectionIndex = numericId - PlayerRaceBase;
        }
        else if (numericId >= CreationModelBase &&
                 numericId < CreationModelBase + FFXICreation_TotalEntries())
        {
            command.selectionType = SelectionType::CreationModel;
            command.selectionIndex = numericId - CreationModelBase;
        }
    }
    return command;
}

UINT CommandId(const Action action)
{
    switch (action)
    {
    case Action::OpenDat: return FileOpenDat;
    case Action::ToggleGameMode: return ViewToggleGameMode;
    case Action::CycleWeather: return ViewCycleWeather;
    default: return 0;
    }
}
}
