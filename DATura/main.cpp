/*========================================================================================
 FFXI Model Viewer
 Win32 application skeleton + Direct3D 9 initialization
========================================================================================*/

#include "stdafx.h"
#include "ffxi_dat_resolver.h"
#include "dat_replacement_dialog.h"
#include "ffxi_coordinate_frame.h"
#include "zone_transition.h"
#include "zone_diagnostics_dialog.h"
#include "noesis_rapi.h"
#include "model_ff11.h"
#include "zone_dat_table.h"
#include "zone_music_table.h"
#include "character_dat_table.h"
#include "character_creation_dat_table.h"
#include "ffxi_internal_lists.h"
#include "prototype_area_table.h"
#include "npc_monster_dat_table.h"
#include "ffxi_companion_browser.h"
#include "ffxi_resource_browser.h"
#include "ffxi_paths.h"
#include "ffxi_path_info_dialog.h"
#include "ffxi_title_screen_renderer.h"
#include "ffxi_title_assets.h"
#include "ffxi_title_ui_primitives.h"
#include "title_scene_assets.h"
#include "ffxi_nation_selection.h"
#include "ffxi_nation_selection_renderer.h"
#include "d3d9_device.h"
#include "d3d_ui_renderer.h"
#include "d3d_math.h"
#include "d3d_model_render_state.h"
#include "model_renderer.h"
#include "zone_model_transform.h"
#include "zone_model_render_metadata.h"
#include "zone_environment_identity.h"
#include "zone_environment_render_state.h"
#include "zone_render_frustum.h"
#include "zone_weather_particles.h"
#include "zone_sky_dome.h"
#include "zone_environment_state.h"
#include "zone_environment_fog.h"
#include "zone_object_transform.h"
#include "zone_object_highlight_renderer.h"
#include "zone_object_transform_editor.h"
#include "zone_object_list_selection.h"
#include "zone_object_panel.h"
#include "zone_object_tree_population.h"
#include "zone_object_visibility.h"
#include "win32_drawing.h"
#include "win32_application.h"
#include "win32_theme.h"
#include "win32_owner_draw_menu.h"
#include "application_menu.h"
#include "ffxi_main_menu.h"
#include "application_settings.h"
#include "config_dialog.h"
#include "input_controller.h"
#include "interaction_controller.h"
#include "ffxi_install_path.h"
#include "ffxi_file_io.h"
#include "ffxi_dat_set_builder.h"
#include "ffxi_player_preset.h"
#include "ffxi_player_preset_dialog.h"
#include "ffxi_player_customization_state.h"
#include "ffxi_player_randomizer.h"
#include "low_poly_character_panel.h"
#include "ffxi_model_lifetime.h"
#include "ffxi_parser_diagnostics.h"
#include "scene_model_loader.h"
#include "scene_load_context.h"
#include "scene_request_resolver.h"
#include "ffxi_sqle_motion.h"
#include "ffxi_sqle_model_animation.h"
#include "ffxi_creation_selection.h"
#include "high_poly_creation_panel.h"
#include "ffxi_creation_animation_paths.h"
#include "ffxi_creation_camera.h"
#include "ffxi_creation_grounding.h"
#include "creation_model_loader.h"
#include "ffxi_creation_export.h"
#include "ffxi_save_result_dialog.h"
#include "npc_render_geometry.h"
#include "npc_nameplate_renderer.h"
#include "ffxi_bitmap_font.h"
#include "npc_interaction.h"
#include "npc_chat_window.h"
#include "zone_collision_geometry.h"
#include "zone_door_interaction.h"
#include "zone_elevator.h"
#include "orbit_camera.h"
#include "player_controller.h"
#include "player_model_loader.h"
#include "bgw_player.h"
#include "audio_player.h"
#include "home_point_effect.h"
#include "texture_viewer.h"
#include "game_ui_config.h"
#include "npc_placement.h"
#include "ffxi_event_table.h"
#include "ffxi_event_messages.h"
#include "ffxi_lore_zone_dat_crossref.h"
#include "resource.h"
#include <commctrl.h>
#include <windowsx.h>
#include <cmath>
#include <cstdio>
#include <cctype>
#include <cfloat>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <cctype>

// Keep the newly introduced developer-clock API visible even when Visual
// Studio is holding an older precompiled-header/index snapshot.
namespace ZoneEnvironmentState
{
void SetTimeOverride(int minuteOfDay);
void ClearTimeOverride();
}

using NpcRenderGeometry::ProjectNpcNameplate;
using ZoneObjectTransform::DebugTransform;
using ZoneObjectListSelection::SetCheckStateForMapObjectIndex;
using ZoneObjectVisibility::SetListVisibility;
using ZoneObjectVisibility::SetHidden;
using ZoneObjectVisibility::ShowOnlySelectedMapObject;
using ZoneObjectTreePopulation::PopulateExpandedNode;
using ZoneEnvironmentIdentity::ContainsLowerToken;

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")

//========================================================================================
// Window / D3D9 state
//========================================================================================

static HWND g_hWnd = NULL;
static D3D9Device::Runtime g_graphicsRuntime;

static const int  kDefaultWidth     = 1280;
static const int  kDefaultHeight    = 720;
static const wchar_t kWindowClassName[] = L"FFXIViewerWndClass";
static const wchar_t kWindowTitle[]     = L"DATura - FFXI Model Viewer";

// Loaded model state. Each asset owns its model together with the parser
// context that backs the model's allocations and textures.
static FFXIModelLifetime::OwnedModel g_zoneAsset;
static FFXIModelLifetime::OwnedModel g_creationAsset;
static FFXIModelLifetime::OwnedModel g_playerAsset;

struct NpcRenderAsset
{
    FFXIModelLifetime::OwnedModel resource;
    std::unique_ptr<HomePoint::Effect> homePoint;
    float animationTime = 0.0f;
    float nameplateLocalY = -2.5f;
    bool visibleLastFrame = false;
};

struct NpcRenderInstance
{
    FFXINpcPlacement::Placement placement;
    size_t assetIndex = 0;
    double homePointActivatedAt = -1000.0;
};

static std::vector<NpcRenderAsset> g_npcRenderAssets;
static std::vector<NpcRenderInstance> g_npcRenderInstances;
static double g_homePointSeconds = 0.0;

static std::vector<NpcNameplateRenderer::DrawItem> g_npcNameplates;
static NpcInteraction::State g_npcInteraction;
static TitleSceneAssets::State g_titleAssets;
static GameUiConfig   g_gameUiConfig    = {};
static LowPolyCharacterPanel::State g_lowPolyPanel;
static HighPolyCreationPanel::State g_highPolyCreationPanel;
static ZoneObjectPanel::State g_zoneObjectPanel;
// Transitional references retain the application-owned visibility workflows.
static HWND& g_hZoneObjectList = g_zoneObjectPanel.placedObjectList;
static HWND& g_hZoneUnrefObjectList = g_zoneObjectPanel.unreferencedObjectList;
static bool& g_populatingZoneObjectList = g_zoneObjectPanel.populatingObjectList;
static SceneLoadContext::State g_sceneLoadContext;
static std::string g_loadedZoneLabel = "No zone loaded";
static ApplicationSettings::State g_applicationSettings;
static ApplicationMenu::State g_applicationMenu;
static ConfigDialog::State g_configDialog;
static Win32Theme::State g_themeState = {};
// FFXI zone DAT coordinates need an X-axis flip to match the retail client.
// Leave that correction enabled when this option is off; checking the option
// deliberately displays the raw, mirrored orientation instead.
static std::vector<std::string> g_hiddenZoneObjects;
static std::string    g_highlightedZoneObject;
static ZoneEnvironmentState::Data g_zoneEnvironment = {};
static int g_zoneWeatherIndex = 0;
static ZoneEnvironmentState::Cache g_zoneEnvironmentCache;

static ZoneRenderFrustum::Data g_zoneRenderFrustum = {};
static float g_zoneVisibilityViewerPoint[3] = {};
static bool g_zoneVisibilityViewerPointValid = false;
static std::vector<unsigned char> g_zoneVisibleMapObjects;
static float g_playModeFrameSeconds = 1.0f / 60.0f;
static float g_adaptivePlayDrawDistance = 2000.0f;

static std::map<std::string, DebugTransform> g_zoneObjectOverrides;
static std::map<std::string, DebugTransform> g_zoneRuntimeOverrides;
static ZoneDoorInteraction::State g_doors;
static ZoneElevator::State g_metalworksElevator;
static bool g_doorPhysicsUpdated = false;
static D3DMATRIX g_pickView = {}, g_pickProjection = {};
static int g_pickWidth = 0, g_pickHeight = 0;

// Orbit camera state
static OrbitCamera::State g_orbitCamera;
static float& g_camYaw = g_orbitCamera.yaw;
static float& g_camPitch = g_orbitCamera.pitch;
static float& g_camDist = g_orbitCamera.distance;
static float (&g_camTarget)[3] = g_orbitCamera.target;
static bool g_cameraDebugOverlayVisible = false;
static bool g_developerConsoleOpen = false;
static std::string g_developerConsoleInput;
static std::vector<std::string> g_developerConsoleLog;
static std::vector<std::string> g_developerConsoleHistory;
static int g_developerConsoleScroll = 0;
static int g_developerConsoleHistoryIndex = -1;

static void ExecuteDeveloperCommand()
{
    std::string command = g_developerConsoleInput;
    g_developerConsoleInput.clear();
    if (command.empty()) return;
    g_developerConsoleHistory.push_back(command);
    g_developerConsoleHistoryIndex = -1;
    g_developerConsoleLog.push_back("> " + command);
    while (!command.empty() && std::isspace((unsigned char)command.front())) command.erase(command.begin());
    if (command == "time reset")
    {
        ZoneEnvironmentState::ClearTimeOverride();
        ZoneEnvironmentState::Invalidate(g_zoneEnvironmentCache, false);
        g_developerConsoleLog.push_back("Time override cleared");
    }
    else if (_strnicmp(command.c_str(), "time", 4) == 0)
    {
        int hour = -1, minute = -1;
        char meridiem[8] = {};
        const int parsed = sscanf_s(command.c_str() + 4, "%d:%d %7s",
            &hour, &minute, meridiem, (unsigned)_countof(meridiem));
        const bool hasMeridiem = parsed == 3;
        bool valid = parsed >= 2 && minute >= 0 && minute < 60;
        if (valid && hasMeridiem)
        {
            if (_stricmp(meridiem, "AM") == 0)
            {
                valid = hour >= 1 && hour <= 12;
                if (hour == 12) hour = 0;
            }
            else if (_stricmp(meridiem, "PM") == 0)
            {
                valid = hour >= 1 && hour <= 12;
                if (hour != 12) hour += 12;
            }
            else valid = false;
        }
        else if (valid)
            valid = hour >= 0 && hour < 24;
        if (valid)
        {
            ZoneEnvironmentState::SetTimeOverride(hour * 60 + minute);
            ZoneEnvironmentState::Invalidate(g_zoneEnvironmentCache, false);
            g_developerConsoleLog.push_back("Time set to " + command.substr(5));
        }
        else g_developerConsoleLog.push_back("Usage: time HH:MM");
    }
    else g_developerConsoleLog.push_back("Unknown command: " + command);
    g_developerConsoleScroll = 0;
}

// Mouse, keyboard, capture, and cursor state.
static InputController::State g_input;
static FFXIMainMenu::State g_ffxiMainMenu;

// Edit/game mode and selected game music have one policy owner. The application
// shell still performs the resulting player, menu, window, and audio operations.
static InteractionController::State g_interaction;

// Player simulation and player-camera state are owned together.
static PlayerController::State g_player;
static float g_playerAnimTime = 0.0f;

using ZoneCollisionTriangle = ZoneCollision::Triangle;

static ZoneCollision::Mesh g_zoneCollisionMesh;
// Transitional views keep existing renderer and UI call sites stable while
// collision-state ownership moves into ZoneCollision::Mesh.
static std::vector<ZoneCollisionTriangle>& g_zoneCollisionTris = g_zoneCollisionMesh.Triangles();
static ZoneCollision::SpatialIndex& g_zoneCollisionGrid = g_zoneCollisionMesh.Index();
static float (&g_zoneCollisionMin)[3] = g_zoneCollisionMesh.MinBounds();
static float (&g_zoneCollisionMax)[3] = g_zoneCollisionMesh.MaxBounds();
static bool& g_haveZoneCollisionBounds = g_zoneCollisionMesh.HasBounds();

static float ClampPlayDrawDistance(const float distance)
{
    if (!std::isfinite(distance))
        return 2000.0f;
    return std::max(750.0f, std::min(distance, 8000.0f));
}

static float EffectivePlayDrawDistance(const float configuredDistance,
                                       const bool unlimitedDistance,
                                       const bool playMode)
{
    if (!playMode || unlimitedDistance)
    {
        return configuredDistance;
    }

    const float configuredCap = ClampPlayDrawDistance(configuredDistance);
    if (g_adaptivePlayDrawDistance <= 0.0f ||
        g_adaptivePlayDrawDistance > configuredCap)
    {
        g_adaptivePlayDrawDistance = std::min(configuredCap, 2000.0f);
    }
    return std::min(configuredCap, g_adaptivePlayDrawDistance);
}

static void UpdateAdaptivePlayDrawDistance(const float deltaSeconds,
                                           const bool playMode)
{
    if (!playMode)
    {
        g_playModeFrameSeconds = 1.0f / 60.0f;
        g_adaptivePlayDrawDistance = 2000.0f;
        return;
    }
    if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds))
        return;

    const float sample = std::min(deltaSeconds, 0.25f);
    g_playModeFrameSeconds += (sample - g_playModeFrameSeconds) * 0.12f;

    const float configuredDistance = ApplicationSettings::DrawDistanceOptionValue(
        g_applicationSettings.drawDistanceIndex);
    const bool unlimitedDistance = ApplicationSettings::DrawDistanceOptionIsUnlimited(
        g_applicationSettings.drawDistanceIndex);
    const float configuredCap = unlimitedDistance ? 8000.0f :
        ClampPlayDrawDistance(configuredDistance);
    if (g_adaptivePlayDrawDistance <= 0.0f)
        g_adaptivePlayDrawDistance = std::min(configuredCap, 2000.0f);

    if (g_playModeFrameSeconds > 0.040f)
        g_adaptivePlayDrawDistance *= 0.82f;
    else if (g_playModeFrameSeconds > 0.030f)
        g_adaptivePlayDrawDistance *= 0.92f;
    else if (g_playModeFrameSeconds < 0.020f)
        g_adaptivePlayDrawDistance += 220.0f * sample;
    else if (g_playModeFrameSeconds < 0.024f)
        g_adaptivePlayDrawDistance += 80.0f * sample;

    g_adaptivePlayDrawDistance = std::min(
        ClampPlayDrawDistance(g_adaptivePlayDrawDistance), configuredCap);
}

// Registry keys used to persist a user-overridden path.
static const char kAppRegKey[]   = "Software\\FFXIViewer";
static const char kAppPathValue[]= "FFXIPath";
static const char kAppThemeValue[] = "ColorTheme";

// Hard-coded default FFXI installation path.
static const char kDefaultFFXIPath[] =
    "C:\\Program Files (x86)\\PlayOnline\\SquareEnix\\FINAL FANTASY XI\\";
static const char kTitleScreenZoneDat[] = "ROM/0/90.DAT"; // Konschtat Highlands
static const int  kTitleScreenMusicId = 108;              // Vana'diel March
// Authored title presentation. Keep these separate from the regular-zone
// camera defaults: the title backdrop is loaded through the normal zone path,
// but its opening composition is intentionally cinematic and deterministic.
static constexpr float kTitleScreenCameraTarget[3] =
{
    -284.089f, -41.906f, 328.520f
};
static constexpr float kTitleScreenCameraYaw = 10.1700f;
static constexpr float kTitleScreenCameraPitch = 0.1900f;
static constexpr float kTitleScreenCameraDistance = 260.0f;
static const char kTitleScreenWeatherToken[] = "/suny";

// Active FFXI root path used to seed file browsers.
// Starts as the hard-coded default; user may override via Settings menu.
static char g_ffxiPath[MAX_PATH] = {};
static bool g_titleScreenActive = false;
static bool g_highPolyCreationActive = false;
static bool g_nationSelectActive = false;
static bool g_characterSelectActive = false;
static bool g_characterDeleteActive = false;
static int  g_selectedNationIndex = 0;

struct SavedCharacterPreview
{
    FFXIModelLifetime::OwnedModel asset;
    std::string name;
};
static std::vector<SavedCharacterPreview> g_savedCharacterPreviews;
static int g_selectedCharacterPreview = 0;

static FFXICreationSelection g_creationSelection = {};
static int g_creationAnimationIndex = 2;
static char g_creationCharacterName[64] = "Adventurer";
static int  g_playerFaceVariant = 0;

using CreationSqleMotionInfo = FFXISqle::MotionInfo;

static CreationSqleMotionInfo g_creationBodyMotion = {};
static CreationSqleMotionInfo g_creationHeadMotion = {};
static float g_creationAnimTime = 0.0f;
static float g_creationHorizontalPlacement[3] = {};
static bool g_creationAnimatedCamera = true;
static FFXICreationCamera::Track g_creationCameraTrack = {};

static PlayerEquipState g_playerEquip =
{
    0,  // raceIndex
    1, 1, // main: Sword, first weapon DAT
    0, 0, // sub
    0, 0, // ranged
    0, 0, 0, 0, 0, // armor
    0, 0, false
};

static const int kWeaponVariantCount = 128;

static void LoadPlayerRaceModel(int raceIndex);
static void ReloadPlayerModelFromControls();
static void ShowLowPolyControlPanel();
static void ShowHighPolyCreationPanel();
static void BeginHighPolyCreationScene();
static const FFXICreationEntry *CurrentHighPolyCreationEntry();
static const char *CreationBodyAnimDatForRace(int raceIndex);
static const char *CreationHeadAnimDatForRace(int raceIndex);
static const char *CreationPreviewIdleBodyDatForRace(int raceIndex);
static const char *CreationPreviewIdleHeadDatForRace(int raceIndex);
static void PullHighPolyCreationStateFromControls();
static void PullLowPolyStateFromControls();
static void SyncLowPolyControlsFromState();
static void ClampLowPolyState();
static void ShowFFXIPathInfoDialog(const char *title, const char *labelText, const char *pathText);
static void ShowCurrentFFXIPathDialog();
static void ShowZoneObjectPanel();
static void ShowCurrentZoneResourceBrowser();
static void ShowResourceDatBrowser();
static void RefreshZoneObjectPanel();
static void UpdateZoneObjectEditControlState();
static void ReloadRememberedZone();
static bool IsGameMode();
static void ToggleEditGameMode();
static void UpdatePlayerCameraTarget();
static void ApplyDisplaySettings();
static void ApplyColorTheme(HWND hWnd);
static void SetColorTheme(int theme);
static void RefreshMainMenuTheme();
static void SyncAppMusic();
static void LoadTitleScreen();
static void BeginNationSelectScene();
static bool EnsureNationSelectAssets();
static void ReturnToTitleScreen();
static void LoadSelectedNationScene();
static void ConfirmExitFromTitleScreen();
static void HandleConfigDialogEvent(void* context, const ConfigDialog::Event& event);
static bool ConfigDialogIsGameMode(void* context);

//========================================================================================
// FFXI path management
//========================================================================================

static bool IsDarkColorTheme()
{
    return g_themeState.dark;
}

static void ApplyColorTheme(HWND hWnd)
{
    Win32Theme::ApplyWindowTheme(hWnd, g_themeState);
}

static void InitColorTheme()
{
    Win32Theme::InitializeState(g_themeState, kAppRegKey, kAppThemeValue, true);
}

static void SetColorTheme(int theme)
{
    Win32Theme::SetDarkMode(
        g_themeState, kAppRegKey, kAppThemeValue, theme != ApplicationSettings::LightTheme);
    ApplyColorTheme(g_hWnd);
    ConfigDialog::ApplyTheme(g_configDialog);
    RefreshMainMenuTheme();
}

// Initialize g_ffxiPath at startup:
//   1. If the user previously saved a custom path, use that.
//   2. Otherwise fall back to the hard-coded default installation path.
static void InitFFXIPath()
{
    FFXIInstallPath::InitializePath(kAppRegKey, kAppPathValue, kDefaultFFXIPath,
                                    g_ffxiPath, sizeof(g_ffxiPath));
    DatReplacementDialog::Initialize(g_ffxiPath,kAppRegKey);
}

// Open a folder-browser dialog so the user can manually choose the FFXI root.
// Saves the result and updates g_ffxiPath.
static void PromptSetFFXIPath()
{
    if (FFXIInstallPath::BrowseForFolder(g_hWnd,
            "Select the FFXI root folder\n"
            "(the folder that contains the ROM, ROM2, ROM3 \x85 subfolders)",
            g_ffxiPath, sizeof(g_ffxiPath)))
    {
        FFXIInstallPath::SavePath(kAppRegKey, kAppPathValue, g_ffxiPath);
        FFXIDatResolver::SetInstallRoot(g_ffxiPath);
        AudioPlayer_SetRootPath(g_ffxiPath);
        TextureViewer_SetRootPath(g_ffxiPath);
        FFXIBitmapFont::SetRootPath(g_ffxiPath);
        NpcNameplateRenderer::ClearCachedTextures();

        ShowFFXIPathInfoDialog("Path Saved", "FFXI path set to:", g_ffxiPath);
    }
}

// Reset to the hard-coded default path, clearing any saved custom path.
static void ResetFFXIPathToDefault()
{
    FFXIInstallPath::ClearSavedPath(kAppRegKey, kAppPathValue);
    strcpy_s(g_ffxiPath, kDefaultFFXIPath);
    FFXIDatResolver::SetInstallRoot(g_ffxiPath);
    AudioPlayer_SetRootPath(g_ffxiPath);
    TextureViewer_SetRootPath(g_ffxiPath);
    FFXIBitmapFont::SetRootPath(g_ffxiPath);
    NpcNameplateRenderer::ClearCachedTextures();

    ShowFFXIPathInfoDialog("Path Reset", "Path reset to default:", g_ffxiPath);
}

// Probe the PlayOnline registry for the FFXI install path and apply it if found.
// Saves the detected path so it persists across restarts.
static void AutoDetectFFXIPath()
{
    char detected[MAX_PATH] = {};
    if (FFXIInstallPath::DetectFromRegistry(detected, sizeof(detected)))
    {
        strcpy_s(g_ffxiPath, detected);
        FFXIDatResolver::SetInstallRoot(g_ffxiPath);
        FFXIInstallPath::SavePath(kAppRegKey, kAppPathValue, g_ffxiPath);
        AudioPlayer_SetRootPath(g_ffxiPath);
        TextureViewer_SetRootPath(g_ffxiPath);
        FFXIBitmapFont::SetRootPath(g_ffxiPath);
        NpcNameplateRenderer::ClearCachedTextures();

        ShowFFXIPathInfoDialog("Auto-Detect", "Detected FFXI path:", g_ffxiPath);
    }
    else
    {
        MessageBoxA(g_hWnd,
            "Could not find a PlayOnline installation in the registry.\n"
            "Use Settings > Set FFXI Path to locate it manually.",
            "Auto-Detect", MB_OK | MB_ICONWARNING);
    }
}

static void SyncSettingsMenuChecks()
{
    ApplicationMenu::Sync(g_applicationMenu, g_applicationSettings, IsGameMode());
}

static void ApplyTextureCompressionSetting()
{
    noeRAPI_t *rapis[] =
    {
        g_zoneAsset.ParserContext(),
        g_creationAsset.ParserContext(),
        g_playerAsset.ParserContext()
    };
    for (int i = 0; i < (int)(sizeof(rapis) / sizeof(rapis[0])); ++i)
    {
        if (!rapis[i])
            continue;
        bool alreadyApplied = false;
        for (int previous = 0; previous < i; ++previous)
            alreadyApplied = alreadyApplied || rapis[previous] == rapis[i];
        if (!alreadyApplied)
            rapis[i]->SetTextureCompressionEnabled(g_applicationSettings.enableTextureCompression);
    }
    g_titleAssets.SetTextureCompressionEnabled(
        g_applicationSettings.enableTextureCompression);
}

static void ConfigureRapiTextureSettings(noeRAPI_t *pRapi)
{
    if (pRapi)
        pRapi->SetTextureCompressionEnabled(g_applicationSettings.enableTextureCompression);
}

static void ShowFFXIPathInfoDialog(const char *title, const char *labelText, const char *pathText)
{
    FFXIPathInfoDialog::Show(g_hWnd, title, labelText, pathText);
}

static void ShowCurrentFFXIPathDialog()
{
    ShowFFXIPathInfoDialog("FFXI Path", "Current FFXI path:", g_ffxiPath);
}

static void HandleConfigDialogEvent(void*, const ConfigDialog::Event& event)
{
    switch (event.command)
    {
    case ConfigDialog::Command::SetPath:
        PromptSetFFXIPath();
        break;
    case ConfigDialog::Command::AutoDetectPath:
        AutoDetectFFXIPath();
        break;
    case ConfigDialog::Command::ResetPath:
        ResetFFXIPathToDefault();
        break;
    case ConfigDialog::Command::ShowPath:
        ShowCurrentFFXIPathDialog();
        break;
    case ConfigDialog::Command::DoorInteractionChanged:
        g_doors.SetPhysics(g_applicationSettings.doorInteractionMode == ApplicationSettings::DoorPhysics);
        InvalidateRect(g_hWnd, NULL, FALSE);
        break;
    case ConfigDialog::Command::MipMappingChanged:
    case ConfigDialog::Command::BumpMappingChanged:
    case ConfigDialog::Command::LightingQualityChanged:
    case ConfigDialog::Command::EnvironmentalAnimationChanged:
        SyncSettingsMenuChecks();
        InvalidateRect(g_hWnd, NULL, FALSE);
        break;
    case ConfigDialog::Command::DisplayChanged:
        ApplyDisplaySettings();
        break;
    case ConfigDialog::Command::ToggleGameMode:
        ToggleEditGameMode();
        break;
    case ConfigDialog::Command::MirrorWorldChanged:
        SyncSettingsMenuChecks();
        ReloadRememberedZone();
        InvalidateRect(g_hWnd, NULL, FALSE);
        break;
    case ConfigDialog::Command::DrawDistanceChanged:
        InvalidateRect(g_hWnd, NULL, FALSE);
        break;
    case ConfigDialog::Command::TextureCompressionChanged:
        ApplyTextureCompressionSetting();
        InvalidateRect(g_hWnd, NULL, FALSE);
        break;
    case ConfigDialog::Command::ColorThemeChanged:
        SetColorTheme(event.value);
        break;
    case ConfigDialog::Command::ChatLogChanged:
        NpcChatWindow::SetSize(g_gameUiConfig.chatLogWidthPercent, g_gameUiConfig.chatLogHeightPercent);
        NpcChatWindow::SetTimeout(g_gameUiConfig.chatLogTimeoutSeconds);
        if (!GameUiConfig_SaveChatLog(g_gameUiConfig))
            MessageBoxA(ConfigDialog::Window(g_configDialog),
                "The chat log timeout changed for this session, but the settings file could not be saved.",
                "DATura Config", MB_OK | MB_ICONWARNING);
        break;
    case ConfigDialog::Command::PlayerNameplateChanged:
        NpcNameplateRenderer::ClearCachedTextures();
        InvalidateRect(g_hWnd, NULL, FALSE);
        if (!GameUiConfig_SavePlayerNameplate(g_gameUiConfig))
            MessageBoxA(ConfigDialog::Window(g_configDialog),
                "The nameplate changed for this session, but the settings file could not be saved.",
                "DATura Config", MB_OK | MB_ICONWARNING);
        break;
    case ConfigDialog::Command::CollisionVisibilityChanged:
        ZoneObjectPanel::SetCollisionVisible(
            g_zoneObjectPanel, g_applicationSettings.showCollisionGeometry);
        InvalidateRect(g_hWnd, NULL, FALSE);
        break;
    case ConfigDialog::Command::SoundSettingsChanged:
        SyncAppMusic();
        break;
    case ConfigDialog::Command::HardwareCursorChanged:
        InputController::SetHardwareCursorEnabled(
            g_input, g_applicationSettings.enableHardwareMouseCursor);
        break;
    case ConfigDialog::Command::ShowZoneObjects:
        ShowZoneObjectPanel();
        break;
    }
}

static bool ConfigDialogIsGameMode(void*)
{
    return IsGameMode();
}

//========================================================================================
// D3D9 helpers
//========================================================================================

static IDirect3DDevice9* GraphicsDevice()
{
    return g_graphicsRuntime.Device();
}

static D3D9Device::DisplayConfiguration CurrentDisplayConfiguration()
{
    const ApplicationSettings::ResolutionOption& resolution =
        ApplicationSettings::ResolutionOptionAt(g_applicationSettings.resolutionIndex);
    D3D9Device::DisplayConfiguration display;
    display.borderless = g_applicationSettings.windowMode == ApplicationSettings::Borderless;
    display.fullscreen = g_applicationSettings.windowMode == ApplicationSettings::Fullscreen;
    display.width = resolution.width;
    display.height = resolution.height;
    return display;
}

static void ReleaseDefaultPoolResources();
static void RecreateDefaultPoolResources();

static bool InitD3D()
{
    g_graphicsRuntime.SetDefaultPoolCallbacks(
        ReleaseDefaultPoolResources, RecreateDefaultPoolResources);
    const D3D9Device::InitializeResult result =
        g_graphicsRuntime.Initialize(g_hWnd, CurrentDisplayConfiguration());
    if (result == D3D9Device::InitializeResult::Direct3DUnavailable)
    {
        MessageBoxA(g_hWnd, "Direct3DCreate9 failed.", "D3D Error", MB_OK | MB_ICONERROR);
        return false;
    }
    if (result != D3D9Device::InitializeResult::Success)
    {
        MessageBoxA(g_hWnd, "Failed to create a Direct3D 9 device.\n"
                            "Make sure your drivers support D3D9.",
                    "D3D Error", MB_OK | MB_ICONERROR);
        return false;
    }

    return true;
}

// The graphics runtime invokes these hooks around every reset and shutdown.
static void ReleaseDefaultPoolResources()
{
    D3DModelRenderState::ReleaseFfxiPixelShaders();

    // TODO: register additional D3DPOOL_DEFAULT resources here as rendering
    // introduces vertex buffers, textures, or render targets in that pool.
}

static void RecreateDefaultPoolResources()
{
    // TODO: re-create registered default-pool resources as they are introduced.
}

static void ApplyDisplaySettings()
{
    if (!g_hWnd)
        return;

    g_applicationSettings.resolutionIndex =
        ApplicationSettings::ClampResolutionIndex(g_applicationSettings.resolutionIndex);
    g_applicationSettings.windowMode =
        ApplicationSettings::ClampWindowMode(g_applicationSettings.windowMode);

    g_graphicsRuntime.ApplyDisplayConfiguration(CurrentDisplayConfiguration());

    if (g_hWnd)
        InvalidateRect(g_hWnd, NULL, TRUE);
}

static void ShutdownD3D()
{
    BGM_Stop();
    g_graphicsRuntime.Shutdown();
}

static void LoadDatFile(const char *path, bool userContentLoad = true, bool renderEnvironment = false,
                        bool renderUnreferenced = false, bool preserveGameScreen = false);
static void UnloadNpcModels();
static void LoadNpcModelsForCurrentZone();
static void UpdateNpcAnimations(float dt);
static void TransitionPlayerZone(const ZoneTransition::Line& line, const PlayerController::State& previousPlayer);
static int g_transitionZoneId = -1;
static const ZoneTransition::Line* g_failedZoneTransition = nullptr;

namespace
{
enum class ZoneTransitionPhase
{
    None,
    FadingOut,
    Loading,
    FadingIn,
};

struct ZoneTransitionRuntime
{
    ZoneTransitionPhase phase = ZoneTransitionPhase::None;
    const ZoneTransition::Line* line = nullptr;
    PlayerController::State previousPlayer = {};
    float opacity = 0.0f;
};

struct LoadingOverlayState
{
    HANDLE thread = NULL;
    HANDLE ready = NULL;
    HWND window = NULL;
    RECT screenRect = {};
};

static ZoneTransitionRuntime g_zoneTransitionRuntime;
static LoadingOverlayState g_loadingOverlay;
static const wchar_t kLoadingOverlayClassName[] = L"DATuraZoneLoadingOverlay";
static const float kZoneTransitionFadeSeconds = 0.35f;

static bool IsZoneTransitionActive()
{
    return g_zoneTransitionRuntime.phase != ZoneTransitionPhase::None;
}

static LRESULT CALLBACK LoadingOverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        SetTimer(hWnd, 1, 125, NULL);
        return 0;

    case WM_TIMER:
        InvalidateRect(hWnd, NULL, FALSE);
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT ps = {};
            HDC dc = BeginPaint(hWnd, &ps);
            RECT bounds = {};
            GetClientRect(hWnd, &bounds);
            HBRUSH black = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
            FillRect(dc, &bounds, black);

            const ULONGLONG tick = GetTickCount64();
            const int dotCount = static_cast<int>((tick / 350) % 4);
            char text[16] = "Loading";
            for (int i = 0; i < dotCount; ++i)
                strcat_s(text, ".");

            HFONT font = CreateFontA(-22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_SWISS, "Arial");
            const int textHeight = 22;
            const SIZE loadingMaxSize = FFXIBitmapFont::Measure(L"Loading...", textHeight);
            const int textWidth = (std::max)(static_cast<int>(loadingMaxSize.cx), 96);
            const int rightMargin = 32;
            const int bottomMargin = 24;
            RECT textBounds = bounds;
            textBounds.right -= rightMargin;
            textBounds.left = (std::max)(textBounds.left, textBounds.right - textWidth);
            textBounds.top = (std::max)(textBounds.top, textBounds.bottom - 48);
            textBounds.bottom -= bottomMargin;
            Win32Drawing::DrawShadowText(dc, font, text, textBounds,
                DT_LEFT | DT_BOTTOM | DT_SINGLELINE | DT_NOPREFIX,
                RGB(255, 255, 255), 2);
            DeleteObject(font);
            EndPaint(hWnd, &ps);
        }
        return 0;

    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        KillTimer(hWnd, 1);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static DWORD WINAPI LoadingOverlayThreadProc(void* parameter)
{
    LoadingOverlayState* state = static_cast<LoadingOverlayState*>(parameter);
    HINSTANCE instance = GetModuleHandleW(NULL);
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = LoadingOverlayWndProc;
    windowClass.hInstance = instance;
    windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    windowClass.lpszClassName = kLoadingOverlayClassName;
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExW(&windowClass);

    const int width = state->screenRect.right - state->screenRect.left;
    const int height = state->screenRect.bottom - state->screenRect.top;
    state->window = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        kLoadingOverlayClassName, L"", WS_POPUP,
        state->screenRect.left, state->screenRect.top, width, height,
        g_hWnd, NULL, instance, NULL);
    if (state->window)
    {
        ShowWindow(state->window, SW_SHOWNOACTIVATE);
        UpdateWindow(state->window);
    }
    SetEvent(state->ready);

    MSG message = {};
    while (GetMessageW(&message, NULL, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    state->window = NULL;
    return 0;
}

static void StartLoadingOverlay()
{
    if (g_loadingOverlay.thread || !g_hWnd)
        return;

    RECT client = {};
    GetClientRect(g_hWnd, &client);
    POINT topLeft = { client.left, client.top };
    POINT bottomRight = { client.right, client.bottom };
    ClientToScreen(g_hWnd, &topLeft);
    ClientToScreen(g_hWnd, &bottomRight);
    g_loadingOverlay.screenRect = { topLeft.x, topLeft.y, bottomRight.x, bottomRight.y };
    g_loadingOverlay.ready = CreateEventW(NULL, TRUE, FALSE, NULL);
    g_loadingOverlay.thread = CreateThread(NULL, 0, LoadingOverlayThreadProc, &g_loadingOverlay, 0, NULL);
    if (g_loadingOverlay.ready)
        WaitForSingleObject(g_loadingOverlay.ready, 1000);
}

static void StopLoadingOverlay()
{
    if (g_loadingOverlay.window)
        PostMessageW(g_loadingOverlay.window, WM_CLOSE, 0, 0);
    if (g_loadingOverlay.thread)
        WaitForSingleObject(g_loadingOverlay.thread, 2000);
    if (g_loadingOverlay.thread)
        CloseHandle(g_loadingOverlay.thread);
    if (g_loadingOverlay.ready)
        CloseHandle(g_loadingOverlay.ready);
    g_loadingOverlay = {};
}
}

static void SetLoadedZoneLabelFromNameAndPath(const char *name, const char *path)
{
    char relativePath[MAX_PATH] = {};
    FFXIPath::MakeRelativePath(g_ffxiPath, path, relativePath, sizeof(relativePath));
    const char* discoveredName = FFXIPath::FindZoneNameByModelPath(g_ffxiPath, path);
    g_loadedZoneLabel = SceneLoadContext::FormatLoadedLabel(
        name, discoveredName, path, relativePath);

    ZoneObjectPanel::SetZoneLabel(g_zoneObjectPanel, g_loadedZoneLabel.c_str());
}

static void SetLoadedZoneLabelFromPath(const char *path)
{
    SetLoadedZoneLabelFromNameAndPath(nullptr, path);
}

static void RememberLoadedZoneContext(const char *path, const char *name,
                                      bool userContentLoad, bool renderEnvironment,
                                      bool renderUnreferenced, bool preserveGameScreen = false)
{
    SceneLoadContext::Options options;
    options.userContentLoad = userContentLoad;
    options.renderEnvironment = renderEnvironment;
    options.renderUnreferenced = renderUnreferenced;
    options.preserveGameScreen = preserveGameScreen;
    g_sceneLoadContext.Remember(path, name, options);
}

static void SetLoadedSceneContext(const SceneLoadContext::Request& request)
{
    SetLoadedZoneLabelFromNameAndPath(request.name.c_str(), request.path.c_str());
    g_sceneLoadContext.Remember(request);
}

static void ReloadRememberedZone()
{
    if (!g_sceneLoadContext.HasRememberedRequest())
        return;

    const SceneLoadContext::Request request = g_sceneLoadContext.Snapshot();
    LoadDatFile(
        request.path.c_str(), request.options.userContentLoad,
        request.options.renderEnvironment, request.options.renderUnreferenced,
        request.options.preserveGameScreen);
    if (!request.name.empty())
        SetLoadedZoneLabelFromNameAndPath(request.name.c_str(), request.path.c_str());
    g_sceneLoadContext.Remember(request);
}

//========================================================================================
// POLUtils-derived resource browser
//========================================================================================

static void ShowCurrentZoneResourceBrowser()
{
    FFXIResourceBrowser::ShowCurrentZone(
        g_hWnd, g_ffxiPath, g_sceneLoadContext.Path().c_str());
}

static void ShowResourceDatBrowser()
{
    FFXIResourceBrowser::ShowDatFile(g_hWnd, g_ffxiPath);
}

static void ShowZoneGeometryDiagnostics()
{
    if (!g_zoneAsset.Model())
    {
        MessageBoxA(g_hWnd, "Load a zone before capturing geometry diagnostics.", "Zone diagnostics", MB_OK);
        return;
    }
    try
    {
        // Match the renderer's LOD query even in zones without a PVS table.
        const FFXICoordinateFrame::Vector nativeViewer = {
            g_zoneVisibilityViewerPoint[0],g_zoneVisibilityViewerPoint[1],g_zoneVisibilityViewerPoint[2]};
        ZoneDiagnosticsDialog::Show(g_hWnd, ZoneSceneDiagnostics::Capture(*g_zoneAsset.Model(),
            g_zoneCollisionMesh, g_hiddenZoneObjects, g_zoneRuntimeOverrides, g_sceneLoadContext.Path(),
            !g_applicationSettings.mirrorWorldZones, nativeViewer));
    }
    catch (const std::exception& error) { MessageBoxA(g_hWnd,error.what(),"Zone diagnostic capture failed",MB_OK|MB_ICONERROR); }
}

//========================================================================================
// Matrix math (no D3DX dependency)
//========================================================================================

// Move the orbit target in view-relative directions, preserving mouse orbit/zoom behavior.
static void UpdateFlyCameraMovement(float dt)
{
    if (!g_hWnd || GetForegroundWindow() != g_hWnd)
        return;

    const InputController::MovementSnapshot movement = InputController::Movement(g_input);
    OrbitCamera::FlyInput input;
    input.forward = movement.forward;
    input.right = movement.right;
    input.vertical = movement.vertical;
    input.boost = movement.boost;
    input.slow = movement.slow;
    OrbitCamera::MoveFly(g_orbitCamera, input, dt);
}

static void ClearZoneCollision()
{
    g_zoneCollisionMesh.Clear();
    PlayerController::ClearCollisionState(g_player);
}

static void BuildZoneCollisionFromDAT()
{
    ClearZoneCollision();
    const int collisionTriCount = Model_FF11_GetLastCollisionTriangleCount();
    g_zoneCollisionMesh.Reserve(collisionTriCount);

    for (int i = 0; i < collisionTriCount; ++i)
    {
        const float *src = Model_FF11_GetLastCollisionTrianglePoints(i);
        if (!src)
            continue;

        ZoneCollisionTriangle tri = {};
        if (!ZoneCollision::BuildTriangle(src, !g_applicationSettings.mirrorWorldZones, tri))
            continue;

        const float doorPadding = g_doors.BindCollision(i, (int)g_zoneCollisionTris.size(), tri);
        g_zoneCollisionMesh.AddTriangle(tri, PlayerController::kCollisionRadius + doorPadding);
    }
}

static void ResetZoneCameraFromCollision()
{
    // Keep the editor fly/orbit controls at their historical defaults. The
    // collision data is only used to choose where that unchanged camera starts.
    g_camYaw = 0.0f;
    g_camPitch = -0.25f;
    g_camDist = 5.0f;

    if (!g_haveZoneCollisionBounds)
    {
        g_camTarget[0] = g_camTarget[1] = g_camTarget[2] = 0.0f;
        return;
    }

    g_camTarget[0] = (g_zoneCollisionMin[0] + g_zoneCollisionMax[0]) * 0.5f;
    g_camTarget[2] = (g_zoneCollisionMin[2] + g_zoneCollisionMax[2]) * 0.5f;

    // Position the actual camera just above the collision envelope, then
    // derive its target from the normal orbit offset. This fixes underground
    // starts without changing fly speed or any mouse/keyboard camera behavior.
    const float initialCameraY = g_zoneCollisionMin[1] - 10.0f;
    g_camTarget[1] = initialCameraY - g_camDist * sinf(g_camPitch);
}

static void ResetCameraForStandaloneModel(const noesisModel_t *model)
{
    if (!model)
        return;

    bool haveVertex = false;
    float minBounds[3] = {};
    float maxBounds[3] = {};
    for (const noesisModel_t::Submesh &submesh : model->submeshes)
    {
        for (const FFXIVertex &vertex : submesh.cpuVerts)
        {
            for (int axis = 0; axis < 3; ++axis)
            {
                if (!haveVertex || vertex.pos[axis] < minBounds[axis])
                    minBounds[axis] = vertex.pos[axis];
                if (!haveVertex || vertex.pos[axis] > maxBounds[axis])
                    maxBounds[axis] = vertex.pos[axis];
            }
            haveVertex = true;
        }
    }
    if (!haveVertex)
        return;

    float radiusSquared = 0.0f;
    for (int axis = 0; axis < 3; ++axis)
    {
        g_camTarget[axis] = (minBounds[axis] + maxBounds[axis]) * 0.5f;
        const float halfExtent = (maxBounds[axis] - minBounds[axis]) * 0.5f;
        radiusSquared += halfExtent * halfExtent;
    }
    const float radius = sqrtf(radiusSquared);
    g_camYaw = 0.0f;
    g_camPitch = -0.12f;
    g_camDist = std::clamp(radius * 2.6f, 2.5f, 500.0f);
}

static bool SpawnPlayerAtZoneHomePoint()
{
    // The NPC catalog includes Home Point entities at their authoritative zone
    // coordinates. Never substitute the centre of arbitrary zone geometry,
    // which is often empty space or outside the playable area. Collision is
    // used to refine the standing position when it is available, but it must
    // not reject a valid Home Point: several zones omit or incompletely export
    // collision around their Home Point object.
    for (const FFXINpcPlacement::Placement &placement : FFXINpcPlacement::Snapshot())
    {
        if (placement.name.compare(0, 10, "Home Point") != 0)
            continue;

        // NPC/Home Point coordinates use the same native X orientation as the
        // zone's authored MapGeo placements. Normal viewer mode mirrors the
        // completed zone model, so apply that identical mirror to entities.
        // The raw mirrored-zone option leaves both sources untouched.
        const bool mirrorEntities = !g_applicationSettings.mirrorWorldZones;
        float spawnX = mirrorEntities ? -placement.transform.x : placement.transform.x;
        float spawnY = placement.transform.y;
        float spawnZ = placement.transform.z;
        bool onGround = false;
        if (!g_zoneCollisionTris.empty())
        {
            float safeX = spawnX;
            float safeY = spawnY;
            float safeZ = spawnZ;
            if (ZoneCollision::FindNearestSafeFloor(
                g_zoneCollisionTris, g_zoneCollisionGrid,
                PlayerController::kCollisionRadius, PlayerController::kCollisionHeight,
                PlayerController::kStepHeight,
                spawnX, spawnY, spawnZ, &safeX, &safeY, &safeZ))
            {
                spawnX = safeX;
                spawnY = safeY;
                spawnZ = safeZ;
                onGround = true;
            }
        }

        const float spawnYaw = mirrorEntities ? -placement.transform.headingRadians :
                                                placement.transform.headingRadians;
        PlayerController::SetPose(g_player, spawnX, spawnY, spawnZ, spawnYaw, onGround);
        UpdatePlayerCameraTarget();
        PlayerController::SetRespawnPoint(g_player);
        return true;
    }
    return false;
}

static bool SnapPlacementToZoneFloor(FFXINpcPlacement::Transform &transform)
{
    if (g_zoneCollisionTris.empty())
        return false;

    float x = transform.x;
    float y = transform.y;
    float z = transform.z;
    if (!ZoneCollision::FindNearestSafeFloor(
            g_zoneCollisionTris, g_zoneCollisionGrid,
            PlayerController::kCollisionRadius, PlayerController::kCollisionHeight,
            PlayerController::kStepHeight,
            x, y, z, &x, &y, &z))
    {
        return false;
    }

    transform.x = x;
    transform.y = y;
    transform.z = z;
    return true;
}

static void UpdatePlayerCameraTarget()
{
    PlayerController::WriteCameraTarget(g_player, g_camTarget);
}

static bool IsEditMode()
{
    return InteractionController::IsEditMode(g_interaction);
}

static bool IsGameMode()
{
    return InteractionController::IsGameMode(g_interaction);
}

static void SyncAppMusic()
{
    const HWND foregroundWindow = GetForegroundWindow();
    InteractionController::PlaybackContext context;
    context.soundsEnabled = g_applicationSettings.enableSounds;
    context.playSoundsInBackground = g_applicationSettings.playSoundsInBackground;
    context.applicationOwnsForeground = !g_hWnd || foregroundWindow == g_hWnd ||
        foregroundWindow == ConfigDialog::Window(g_configDialog) ||
        AudioPlayer_OwnsWindow(foregroundWindow);
    context.externalPlayerOpen = AudioPlayer_IsOpen();
    context.titleScreenActive = g_titleScreenActive;
    context.titleMusicId = kTitleScreenMusicId;

    if (!context.soundsEnabled || (!context.playSoundsInBackground && !context.applicationOwnsForeground) ||
        !IsGameMode() || g_titleScreenActive || g_nationSelectActive)
        for (auto& asset : g_npcRenderAssets)
            if (asset.homePoint) asset.homePoint->StopSound();

    const InteractionController::PlaybackDecision decision =
        InteractionController::DecidePlayback(g_interaction, context);
    switch (decision.action)
    {
    case InteractionController::PlaybackAction::ControlExternalPlayer:
        AudioPlayer_SetPlaybackAllowed(decision.playbackAllowed);
        return;
    case InteractionController::PlaybackAction::PlayTitleMusic:
    case InteractionController::PlaybackAction::PlayGameMusic:
        BGM_PlayZoneMusic(g_ffxiPath, decision.musicId);
        return;
    case InteractionController::PlaybackAction::Stop:
    default:
        BGM_Stop();
        return;
    }
}

static void SetGameModeMusic(int musicId)
{
    InteractionController::SetGameMusicId(g_interaction, musicId);
    SyncAppMusic();
}

static void SetInteractionMode(const InteractionController::Mode mode)
{
    InputController::ConsumeJump(g_input);
    const InteractionController::ModeTransition transition =
        InteractionController::SetMode(g_interaction, mode);
    g_player.cameraActive = transition.playerCameraActive;

    if (g_player.cameraActive && g_playerAsset)
        UpdatePlayerCameraTarget();

    SyncSettingsMenuChecks();
    UpdateZoneObjectEditControlState();
    SyncAppMusic();

    if (g_hWnd)
        InvalidateRect(g_hWnd, NULL, FALSE);
}

static void ToggleEditGameMode()
{
    g_npcInteraction = {};
    g_doors.selected = -1;
    NpcChatWindow::Reset();
    if (IsEditMode())
    {
        if (!g_playerAsset)
        {
            LoadPlayerRaceModel(0);
            if (!g_playerAsset)
                return;
        }
        SetInteractionMode(InteractionController::Mode::Game);
    }
    else
    {
        SetInteractionMode(InteractionController::Mode::Edit);
    }
}

static void UnstickPlayerFromGeometry()
{
    if (!IsGameMode())
        return;

    if (PlayerController::Unstick(g_player, g_zoneCollisionMesh))
        UpdatePlayerCameraTarget();
}

static PlayerController::InputSnapshot CapturePlayerInputSnapshot()
{
    const InputController::MovementSnapshot movement = InputController::Movement(g_input);
    PlayerController::InputSnapshot input;
    input.turn = -movement.right;
    input.forward = movement.forward;
    input.strafe = movement.strafe;
    input.boost = movement.running;
    input.fastRunning = movement.fastRunning;
    if (InputController::MouseForwardActive(g_input))
    {
        input.forward = 1.0f;
        input.turn = 0.0f;
        input.strafe = 0.0f;
    }
    input.slow = movement.slow;
    return input;
}

static void UpdatePlayerMovement(const float dt)
{
    if (IsZoneTransitionActive())
        return;

    if (!g_hWnd || GetForegroundWindow() != g_hWnd)
        return;

    PlayerController::SimulationContext context = { &g_zoneCollisionMesh };
    if (g_doors.physics)
        context.beforeHorizontalMove = [](const PlayerController::State& player, float dx, float dz, float step) {
            g_doors.UpdatePhysics(step, g_zoneCollisionMesh, player.position, dx, dz,
                PlayerController::kCollisionRadius, PlayerController::kCollisionHeight);
            g_doorPhysicsUpdated = true;
        };
    auto input = CapturePlayerInputSnapshot();
    if (InputController::MouseForwardActive(g_input))
    {
        // The camera remains a normal orbit camera. The character follows its
        // horizontal heading while the mouse chord supplies forward input.
        g_player.yaw = g_camYaw;
        input.turn = 0.0f;
        input.forward = 1.0f;
        input.strafe = 0.0f;
    }
    input.jump = InputController::ConsumeJump(g_input);
    const ZoneTransition::Point previous = { g_player.position[0], g_player.position[1], g_player.position[2] };
    const auto previousPlayer = g_player;
    if (g_failedZoneTransition)
    {
        const auto native = FFXICoordinateFrame::SceneToNativeDat(previous,
            !g_applicationSettings.mirrorWorldZones);
        const auto& line = *g_failedZoneTransition;
        const float side = (native[0]-line.threshold[0])*line.outward[0] +
            (native[2]-line.threshold[2])*line.outward[2];
        if (side < -2) g_failedZoneTransition = nullptr;
    }
    const auto movement = PlayerController::UpdateMovement(
        g_player, input, dt, context);
    if (g_zoneAsset && g_playerAsset && !movement.respawned)
    {
        const ZoneTransition::Point current = { g_player.position[0], g_player.position[1], g_player.position[2] };
        if (const auto* line = ZoneTransition::Crossed(g_transitionZoneId, previous, current,
                !g_applicationSettings.mirrorWorldZones))
        {
            if (line != g_failedZoneTransition)
                TransitionPlayerZone(*line, previousPlayer);
            else
                g_player = previousPlayer;
        }
    }
    UpdatePlayerCameraTarget();
}

static void UpdatePlayerAnimation(float dt)
{
    if (!g_playerAsset || !GraphicsDevice())
        return;

    noesisModel_t *model = g_playerAsset.Model();
    if (!IsGameMode() && !g_playerEquip.animationPlaying)
    {
        g_playerAnimTime = 0.0f;
        model->RestoreBindPose(GraphicsDevice());
        return;
    }

    const bool active = g_hWnd && GetForegroundWindow() == g_hWnd;
    const auto input = active ? CapturePlayerInputSnapshot() : PlayerController::InputSnapshot{};
    const char *motion = IsGameMode() ? PlayerController::MovementAnimation(g_player, input) : "wlk";

    noesisAnim_t *clip = model->FindAnimation(motion);
    if (!clip)
        clip = model->FindAnimation("idl_relaxed");
    if (!clip)
    {
        model->RestoreBindPose(GraphicsDevice());
        g_playerAnimTime = 0.0f;
        return;
    }
    if (model->pAnim != clip)
    {
        model->pAnim = clip;
        g_playerAnimTime = 0.0f;
    }
    g_playerAnimTime += dt * (IsGameMode() ? PlayerController::MovementAnimationRate(g_player, input) : 1.0f);
    model->UpdateAnimation(g_playerAnimTime, GraphicsDevice());
}

static void UpdateHighPolyCreationAnimation(float dt)
{
    if (!g_highPolyCreationActive || !g_creationAsset || !GraphicsDevice())
        return;
    FFXISqleModelAnimation::UpdatePreview(
        g_creationAsset.Model(), GraphicsDevice(), g_creationAnimationIndex, dt,
        &g_creationAnimTime,
        g_creationBodyMotion, g_creationHeadMotion);
}

static void UpdateCameraMovement(float dt)
{
    if (IsGameMode())
        UpdatePlayerMovement(dt);
    else
    {
        InputController::ConsumeJump(g_input);
        UpdateFlyCameraMovement(dt);
    }
}

static void EndMouseLook()
{
    InputController::EndDrag(g_input);
}

static int SelectZoneMusicId(const FFXIZoneMusicEntry* pMusic)
{
    if (!pMusic)
        return 0;

    SYSTEMTIME localTime = {};
    GetLocalTime(&localTime);
    const bool useNight = (localTime.wHour >= 18 || localTime.wHour < 6);
    if (useNight && pMusic->nightMusicId != 0)
        return pMusic->nightMusicId;
    return (pMusic->dayMusicId != 0) ? pMusic->dayMusicId : pMusic->nightMusicId;
}

//========================================================================================
// Model rendering
//========================================================================================

static void UpdateZoneEnvironmentState()
{
    ZoneEnvironmentState::Update(g_zoneEnvironment, g_zoneEnvironmentCache, g_zoneWeatherIndex,
                                 gFF11LastEnvironmentRecords, ZoneEnvironmentState::CurrentMinuteOfDay());
}

static void GetRenderCameraPosition(float &x, float &y, float &z)
{
    OrbitCamera::GetPosition(g_orbitCamera, x, y, z);
}

static void RenderModel(noesisModel_t *pModel, bool allowObjectOverrides = false,
                        ModelRenderer::GeometryPass pass = ModelRenderer::GeometryPass::All,
                        bool drawDynamicActorShadow = true)
{
    if (!pModel || !GraphicsDevice())
        return;

    ModelRenderer::Context rendererContext = {};
    rendererContext.device = GraphicsDevice();
    rendererContext.geometryPass = pass;
    rendererContext.minuteOfDay = ZoneEnvironmentState::CurrentMinuteOfDay();
    rendererContext.lightingQuality = g_applicationSettings.lightingQuality;
    rendererContext.vegetationAnimationMode = g_applicationSettings.environmentalAnimationMode;
    rendererContext.rendersZoneObjects = allowObjectOverrides;
    rendererContext.dynamicActorShadows =
        drawDynamicActorShadow &&
        g_applicationSettings.lightingQuality == ApplicationSettings::LightingDynamicShadows;
    rendererContext.enableMipMapping = g_applicationSettings.enableMipMapping;
    rendererContext.indoorZone = g_zoneEnvironment.valid && g_zoneEnvironment.indoor;
    ZoneModelRenderMetadata::Prepare(pModel, GraphicsDevice());
    GraphicsDevice()->SetFVF(FFXI_VERTEX_FVF);
    D3DMATRIX baseWorld = {};
    GraphicsDevice()->GetTransform(D3DTS_WORLD, &baseWorld);

    // The dynamic tier adds an inexpensive planar pass for standalone actors. Zone terrain
    // has uneven ground, so projecting an entire zone onto a single plane would be wrong.
    ModelRenderer::DrawActorPlanarShadow(rendererContext, pModel, baseWorld);
    ModelRenderer::PrepareFixedFunctionPass(rendererContext, baseWorld);
    if (allowObjectOverrides)
        ZoneEnvironmentFog::Apply(GraphicsDevice(), g_zoneEnvironment);

    rendererContext.visibility.hiddenNames = &g_hiddenZoneObjects;
    rendererContext.visibility.viewerPointValid = g_zoneVisibilityViewerPointValid;
    rendererContext.visibility.lodViewerPoint = g_zoneVisibilityViewerPoint;
    rendererContext.visibility.visibleMapObjects = &g_zoneVisibleMapObjects;
    rendererContext.visibility.overrides = &g_zoneRuntimeOverrides;
    rendererContext.visibility.frustum = &g_zoneRenderFrustum;
    GetRenderCameraPosition(rendererContext.cameraPosition[0],
        rendererContext.cameraPosition[1], rendererContext.cameraPosition[2]);
    ModelRenderer::DrawGeometry(rendererContext, pModel, baseWorld);
}

//========================================================================================
// Model loading
//========================================================================================

static void ShowSceneModelLoadError(const SceneModelLoader::Error error)
{
    const char* title = "Load Error";
    const char* message = "The model could not be loaded.";
    UINT icon = MB_ICONERROR;

    switch (error)
    {
    case SceneModelLoader::Error::InvalidRequest:
        message = "Invalid model load request.";
        break;
    case SceneModelLoader::Error::FileOpenFailed:
        message = "Could not open file.";
        break;
    case SceneModelLoader::Error::FileEmptyOrUnreadable:
        message = "File is empty or unreadable.";
        break;
    case SceneModelLoader::Error::FileReadFailed:
        message = "Read error — incomplete file.";
        break;
    case SceneModelLoader::Error::UnrecognizedDat:
        message = "Not a recognised FFXI DAT file.";
        break;
    case SceneModelLoader::Error::StandardDatHasNoGeometry:
        title = "Load";
        message = "DAT loaded but contained no displayable geometry.";
        icon = MB_ICONWARNING;
        break;
    case SceneModelLoader::Error::CreationDatHasNoGeometry:
        title = "Load";
        message = "Creation DAT loaded but contained no displayable geometry.";
        icon = MB_ICONWARNING;
        break;
    case SceneModelLoader::Error::CreationMaterialOnly:
        title = "Creation Material DAT";
        message =
            "This is the material/texture half of a high-poly character creation pair.\n\n"
            "Choose the matching mesh DAT next to it in the menu. DMB texture binding is not implemented yet.";
        icon = MB_ICONINFORMATION;
        break;
    case SceneModelLoader::Error::SqleAnimationOnly:
        title = "SQLE Loader Pending";
        message =
            "This is an SQLE animation file for high-poly character creation models.\n\n"
            "DATura can identify it now, but SQLE animation loading is not implemented yet.";
        icon = MB_ICONINFORMATION;
        break;
    case SceneModelLoader::Error::UnrecognizedDatSet:
        message = "Not a recognised FFXI DAT Set file.";
        break;
    case SceneModelLoader::Error::DatSetHasNoGeometry:
        title = "Load";
        message = "DAT Set loaded but contained no displayable geometry.";
        icon = MB_ICONWARNING;
        break;
    case SceneModelLoader::Error::None:
    default:
        return;
    }

    MessageBoxA(g_hWnd, message, title, MB_OK | icon);
}

static void UnloadZoneModel()
{
    g_transitionZoneId = -1;
    g_failedZoneTransition = nullptr;
    ClearZoneCollision();
    UnloadNpcModels();
    FFXINpcPlacement::Clear();
    g_zoneAsset.Reset();
}

static void UnloadCreationModel()
{
    g_creationAsset.Reset();
    g_creationBodyMotion = {};
    g_creationHeadMotion = {};
    g_creationCameraTrack.Clear();
    g_creationAnimTime = 0.0f;
    g_creationHorizontalPlacement[0] = 0.0f;
    g_creationHorizontalPlacement[1] = 0.0f;
    g_creationHorizontalPlacement[2] = 0.0f;
}

static void UnloadPlayerModel()
{
    g_playerAsset.Reset();
    PlayerController::ResetModelPlacement(g_player);
    g_playerAnimTime = 0.0f;
}

static void UnloadTitleAssets()
{
    g_titleAssets.Reset();
}

// Clear state produced by the previously loaded zone model. Mode transitions,
// title state, and camera selection remain the responsibility of each loader.
static void ResetZoneModelLoadState(const char *path)
{
    UnloadZoneModel();
    g_hiddenZoneObjects.clear();
    g_zoneObjectOverrides.clear();
    g_zoneRuntimeOverrides.clear();
    g_doors = {};
    g_pickWidth = g_pickHeight = 0;
    gFF11LastMapObjects.clear();
    gFF11LastZoneVisibilityLeaves.clear();
    gFF11LastZoneVisibilityRecords.clear();
    gFF11LastZoneVisibilityTables.clear();
    gFF11LastMapGeoDrawBatches.clear();
    gFF11LastDatChunks.clear();
    gFF11LastEnvironmentRecords.clear();
    gFF11LastGeneratorRecords.clear();
    gFF11LastKeyframeRecords.clear();
    memset(&gFF11LastMapHeader, 0, sizeof(gFF11LastMapHeader));
    gFF11LastCollisionTriangles.clear();
    gFF11LastCollisionMeshes.clear();
    SetLoadedZoneLabelFromPath(path);
    RefreshZoneObjectPanel();
}

static void LoadDatFile(const char *path, bool userContentLoad, bool renderEnvironment,
                        bool renderUnreferenced, bool preserveGameScreen)
{
    RememberLoadedZoneContext(path, nullptr, userContentLoad, renderEnvironment,
                              renderUnreferenced, preserveGameScreen);
    g_zoneWeatherIndex = 0;
    ZoneEnvironmentState::Invalidate(g_zoneEnvironmentCache, false);

    if (userContentLoad && !preserveGameScreen)
    {
        g_titleScreenActive = false;
        g_highPolyCreationActive = false;
        g_nationSelectActive = false;
        InteractionController::SetGameMusicId(g_interaction, 0);
        UnloadCreationModel();
        UnloadTitleAssets();
        SyncAppMusic();
    }

    ResetZoneModelLoadState(path);

    SceneModelLoader::DatOptions loadOptions;
    loadOptions.enableTextureCompression =
        g_applicationSettings.enableTextureCompression;
    loadOptions.userContentLoad = userContentLoad;
    loadOptions.renderEnvironment = renderEnvironment;
    loadOptions.renderUnreferenced = renderUnreferenced;
    SceneModelLoader::Result loadResult =
        SceneModelLoader::LoadDat(GraphicsDevice(), path, loadOptions);

    if (loadResult.kind == SceneModelLoader::ContentKind::CreationDat)
        g_highPolyCreationActive = true;

    if (!loadResult.Succeeded())
    {
        ShowSceneModelLoadError(loadResult.error);
        return;
    }

    g_zoneAsset = std::move(loadResult.asset);
    if (loadResult.kind == SceneModelLoader::ContentKind::CreationDat)
    {
        g_camDist = 25.0f;
        g_camYaw = 0.0f;
        g_camPitch = -0.15f;
        g_camTarget[0] = g_camTarget[2] = 0.0f;
        g_camTarget[1] = -12.0f;
        return;
    }

    // The normal (unchecked) view corrects the DAT's mirrored X orientation.
    // Checked exposes that mirrored orientation as requested by the UI option.
    if (!g_applicationSettings.mirrorWorldZones)
        ZoneModelTransform::MirrorOnX(g_zoneAsset.Model(), GraphicsDevice());
    g_doors.Initialize(g_zoneAsset.Model(), !g_applicationSettings.mirrorWorldZones);
    g_doors.SetPhysics(g_applicationSettings.doorInteractionMode == ApplicationSettings::DoorPhysics);
    BuildZoneCollisionFromDAT();
    const int zoneId = FFXIPath::FindZoneIDByModelPath(g_ffxiPath, path);
    g_transitionZoneId = userContentLoad ? zoneId : -1;
    if (userContentLoad)
    {
        // NPC DAT parsing replaces the parser diagnostics. Keep this zone's
        // authored records available for the environment renderer.
        {
            FFXIParserDiagnostics::ScopedSnapshot parserDiagnostics;
            FFXINpcPlacement::LoadCatalogForZone(zoneId);
            LoadNpcModelsForCurrentZone();
        }
        ZoneEnvironmentState::Invalidate(g_zoneEnvironmentCache, true);
    }
    else
        FFXINpcPlacement::ResetForZone(zoneId);
    RefreshZoneObjectPanel();
    ResetZoneCameraFromCollision();
    if (userContentLoad && !preserveGameScreen)
    {
        SpawnPlayerAtZoneHomePoint();
        SetInteractionMode(InteractionController::Mode::Edit);
    }
}

static void LoadDatFile(const SceneLoadContext::Request& request)
{
    LoadDatFile(
        request.path.c_str(), request.options.userContentLoad,
        request.options.renderEnvironment, request.options.renderUnreferenced,
        request.options.preserveGameScreen);
}

static void CompletePlayerZoneTransitionLoad()
{
    const ZoneTransition::Line& line = *g_zoneTransitionRuntime.line;
    const PlayerController::State previousPlayer = g_zoneTransitionRuntime.previousPlayer;
    const auto resolution = SceneRequestResolver::ResolveZone(g_ffxiPath, line.toZone);
    if (!resolution.Succeeded())
    {
        g_player = previousPlayer;
        g_failedZoneTransition = &line;
        MessageBoxA(g_hWnd, "The destination zone DAT could not be resolved. Check the configured FFXI installation.",
            "Zone transition", MB_OK | MB_ICONWARNING);
        return;
    }

    const auto source = g_sceneLoadContext.Snapshot();
    const float cameraDistance = g_camDist;
    const float cameraPitch = g_camPitch;
    const float cameraYaw = g_camYaw;
    auto destination = resolution.request;
    destination.options.preserveGameScreen = true;
    LoadDatFile(destination);
    if (!g_zoneAsset)
    {
        // The shared loader reports the error. Restore the source scene and
        // pre-crossing checkpoint instead of leaving the player in an empty zone.
        auto sourceRequest = source;
        sourceRequest.options.preserveGameScreen = true;
        LoadDatFile(sourceRequest);
        SetLoadedSceneContext(source);
        g_player = previousPlayer;
        g_failedZoneTransition = &line;
        g_camDist = cameraDistance;
        g_camPitch = cameraPitch;
        g_camYaw = cameraYaw;
        SetInteractionMode(InteractionController::Mode::Game);
        const auto* music = FFXIZoneMusic_FindByID(line.fromZone);
        SetGameModeMusic(SelectZoneMusicId(music));
        return;
    }
    SetLoadedSceneContext(destination);

    const bool mirrorX = !g_applicationSettings.mirrorWorldZones;
    auto arrival = FFXICoordinateFrame::NativeDatToScene(line.arrival, mirrorX);
    float x = arrival[0], y = arrival[1], z = arrival[2];
    bool grounded = ZoneCollision::FindNearestSafeFloor(
        g_zoneCollisionTris, g_zoneCollisionGrid, PlayerController::kCollisionRadius,
        PlayerController::kCollisionHeight, PlayerController::kStepHeight,
        x, y, z, &x, &y, &z);
    // Keep grounding local to the authored entrance, not an unrelated room.
    grounded = grounded && std::fabs(x-arrival[0]) <= 2 &&
        std::fabs(z-arrival[2]) <= 2 && std::fabs(y-arrival[1]) <= 4;
    if (grounded) arrival = { x, y, z };
    PlayerController::SetPose(g_player, arrival[0], arrival[1], arrival[2],
        ZoneTransition::ArrivalYaw(line, mirrorX), grounded);
    PlayerController::SetRespawnPoint(g_player);
    g_camYaw = g_player.yaw;
    g_camDist = cameraDistance;
    g_camPitch = cameraPitch;
    g_playerAnimTime = 0;
    SetInteractionMode(InteractionController::Mode::Game);
    const auto* music = FFXIZoneMusic_FindByID(line.toZone);
    SetGameModeMusic(SelectZoneMusicId(music));
}

static void TransitionPlayerZone(const ZoneTransition::Line& line, const PlayerController::State& previousPlayer)
{
    const auto resolution = SceneRequestResolver::ResolveZone(g_ffxiPath, line.toZone);
    if (!resolution.Succeeded())
    {
        g_player = previousPlayer;
        g_failedZoneTransition = &line;
        MessageBoxA(g_hWnd, "The destination zone DAT could not be resolved. Check the configured FFXI installation.",
            "Zone transition", MB_OK | MB_ICONWARNING);
        return;
    }

    g_player = previousPlayer;
    g_zoneTransitionRuntime.phase = ZoneTransitionPhase::FadingOut;
    g_zoneTransitionRuntime.line = &line;
    g_zoneTransitionRuntime.previousPlayer = previousPlayer;
    g_zoneTransitionRuntime.opacity = 0.0f;
}

static void UpdateZoneTransition(float dt)
{
    if (!IsZoneTransitionActive())
        return;

    if (!(dt > 0.0f) || !std::isfinite(dt))
        dt = 1.0f / 60.0f;

    switch (g_zoneTransitionRuntime.phase)
    {
    case ZoneTransitionPhase::FadingOut:
        g_zoneTransitionRuntime.opacity += dt / kZoneTransitionFadeSeconds;
        if (g_zoneTransitionRuntime.opacity >= 1.0f)
        {
            g_zoneTransitionRuntime.opacity = 1.0f;
            g_zoneTransitionRuntime.phase = ZoneTransitionPhase::Loading;
        }
        break;

    case ZoneTransitionPhase::Loading:
        StartLoadingOverlay();
        CompletePlayerZoneTransitionLoad();
        StopLoadingOverlay();
        g_zoneTransitionRuntime.phase = ZoneTransitionPhase::FadingIn;
        g_zoneTransitionRuntime.opacity = 1.0f;
        break;

    case ZoneTransitionPhase::FadingIn:
        g_zoneTransitionRuntime.opacity -= dt / kZoneTransitionFadeSeconds;
        if (g_zoneTransitionRuntime.opacity <= 0.0f)
            g_zoneTransitionRuntime = {};
        break;

    case ZoneTransitionPhase::None:
    default:
        break;
    }
}

static void DrawZoneTransitionOverlay()
{
    if (!g_hWnd || !GraphicsDevice() || g_zoneTransitionRuntime.opacity <= 0.0f)
        return;

    int renderW = 0;
    int renderH = 0;
    D3D9Device::GetViewportOrClientSize(GraphicsDevice(), g_hWnd, &renderW, &renderH);
    const int alpha = (std::min)(255, (std::max)(0,
        static_cast<int>(g_zoneTransitionRuntime.opacity * 255.0f + 0.5f)));
    D3DUiRenderer::DrawSolidQuad(GraphicsDevice(), 0.0f, 0.0f,
        static_cast<float>(renderW), static_cast<float>(renderH),
        D3DCOLOR_ARGB(alpha, 0, 0, 0));
}

static void DrawZoneBoundaryDots(const D3DMATRIX& viewProjection, int width, int height)
{
    if (!IsGameMode() || g_zoneTransitionRuntime.phase != ZoneTransitionPhase::None || !GraphicsDevice()) return;
    const ZoneTransition::Line* active = nullptr;
    float nearest = 1.0e30f;
    const bool mirrorX = !g_applicationSettings.mirrorWorldZones;
    // Transition coordinates are stored in DATura's native DAT frame.  Only
    // the optional world-X reflection belongs in this conversion; applying an
    // additional (x,-y,-z) transform puts the markers on the wrong floor.
    const auto playerNative = FFXICoordinateFrame::SceneToNativeDat(
        { g_player.position[0], g_player.position[1], g_player.position[2] }, mirrorX);
    for (const auto& line : ZoneTransition::lines)
    {
        if (line.fromZone != g_transitionZoneId) continue;
        const float distance = std::fabs((playerNative[0] - line.threshold[0]) * line.outward[0] +
            (playerNative[2] - line.threshold[2]) * line.outward[2]);
        if (distance < nearest) { nearest = distance; active = &line; }
    }
    if (!active) return;
    const auto native = active->threshold;
    const float boundaryDistance = std::fabs((playerNative[0] - native[0]) * active->outward[0] +
        (playerNative[2] - native[2]) * active->outward[2]);
    constexpr float kMarkerFadeDistance = 35.0f;
    if (boundaryDistance >= kMarkerFadeDistance) return;
    const float distanceAlpha = 1.0f - boundaryDistance / kMarkerFadeDistance;
    const float tangentX = active->outward[2];
    const float tangentZ = -active->outward[0];
    // Place the warning row just inside the current zone.  The transition
    // remains at `threshold`; the player must pass the dots before crossing
    // that plane and actually zoning.
    constexpr float kMarkerInsideOffset = 1.5f;
    const ULONGLONG tick = GetTickCount64();
    const DWORD pulse = static_cast<DWORD>((150.0f + 80.0f * std::fabs(std::sin(static_cast<double>(tick) * 0.004))) * distanceAlpha);
    for (int i = -8; i <= 8; ++i)
    {
        const float offset = static_cast<float>(i) * active->halfWidth * 0.20f;
        // Native DAT Y grows downward.  A negative offset therefore lifts the
        // guide above the floor instead of sinking it into the threshold.
        constexpr float kBoundaryMarkerHeight = -1.0f;
        const float dotY = playerNative[1] + kBoundaryMarkerHeight;
        const float dotX = native[0] - active->outward[0] * kMarkerInsideOffset + tangentX * offset;
        const float dotZ = native[2] - active->outward[2] * kMarkerInsideOffset + tangentZ * offset;
        const auto scene = FFXICoordinateFrame::NativeDatToScene({ dotX, dotY, dotZ }, mirrorX);
        const float radius = 0.035f + 0.010f * std::fabs(std::sin((static_cast<double>(tick) + i * 90.0) * 0.006));
        // Each fan has one center, twelve perimeter vertices, and a closing
        // perimeter vertex (15 total).  Keep two complete fans; the previous
        // 28-vertex allocation let the second fan write element 28.
        FFXIVertex verts[30] = {};
        const float rightX = g_pickView._11, rightZ = g_pickView._13;
        const float upX = g_pickView._21, upY = g_pickView._22, upZ = g_pickView._23;
        auto build = [&](float r, DWORD color, FFXIVertex* out)
        {
            out[0].pos[0]=scene[0]; out[0].pos[1]=scene[1]; out[0].pos[2]=scene[2]; out[0].diffuse=color;
            for (int n=0;n<=12;++n) { const float a=6.2831853f*n/12.0f; out[n+1].pos[0]=scene[0]+(rightX*std::cos(a)+upX*std::sin(a))*r; out[n+1].pos[1]=scene[1]+upY*std::sin(a)*r; out[n+1].pos[2]=scene[2]+(rightZ*std::cos(a)+upZ*std::sin(a))*r; out[n+1].diffuse=color; }
            out[14] = out[1];
        };
        build(radius + 0.018f, D3DCOLOR_ARGB(pulse, 65, 135, 235), verts);
        build(radius, D3DCOLOR_ARGB(pulse, 255, 255, 255), verts + 14);
        const D3DMATRIX identity = D3DMath::BuildIdentity();
        GraphicsDevice()->SetTransform(D3DTS_WORLD, &identity);
        GraphicsDevice()->SetFVF(FFXI_VERTEX_FVF); GraphicsDevice()->SetTexture(0, nullptr);
        GraphicsDevice()->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        GraphicsDevice()->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
        GraphicsDevice()->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        GraphicsDevice()->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
        GraphicsDevice()->SetRenderState(D3DRS_LIGHTING, FALSE);
        // Test against the already-rendered world depth buffer so walls,
        // pillars, and characters can hide markers that are behind them. Do
        // not write marker depth; the dots remain a non-invasive overlay.
        GraphicsDevice()->SetRenderState(D3DRS_ZENABLE, TRUE);
        GraphicsDevice()->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        GraphicsDevice()->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
        GraphicsDevice()->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE); GraphicsDevice()->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE); GraphicsDevice()->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA); GraphicsDevice()->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        GraphicsDevice()->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 12, verts, sizeof(FFXIVertex));
        GraphicsDevice()->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 12, verts + 14, sizeof(FFXIVertex));
    }
}

static void LoadTitleScreen()
{
    UnloadCreationModel();
    UnloadTitleAssets();
    InteractionController::SetGameMusicId(g_interaction, 0);
    SetInteractionMode(InteractionController::Mode::Edit);
    g_highPolyCreationActive = false;
    g_nationSelectActive = false;

    SceneLoadContext::Options backdropOptions;
    backdropOptions.preserveGameScreen = true;
    const SceneRequestResolver::Result backdrop =
        SceneRequestResolver::ResolveRelativeModel(
            g_ffxiPath, kTitleScreenZoneDat, nullptr, backdropOptions);
    if (!backdrop.Succeeded())
    {
        g_titleScreenActive = true;
        return;
    }

    // Load Konschtat through the same regular-zone content path used everywhere
    // else (environment, collision, NPC placements, and ordinary visibility).
    // Only suppress the UI transition that would otherwise leave the title
    // screen and unload the title-specific overlay assets.
    LoadDatFile(backdrop.request);

    // The logo/atlas/UI DAT loads below replace global parser diagnostics.
    // Keep the backdrop zone's authored 0x2F and 0x05 records alive.
    {
        FFXIParserDiagnostics::ScopedSnapshot parserDiagnostics;
        g_titleScreenActive = true;

        // Replace the regular zone loader's collision-derived camera with the
        // captured title presentation pose. Keeping the target as well as the
        // orbit values fixed makes the opening composition deterministic.
        for (int axis = 0; axis < 3; ++axis)
            g_camTarget[axis] = kTitleScreenCameraTarget[axis];
        g_camYaw = kTitleScreenCameraYaw;
        g_camPitch = kTitleScreenCameraPitch;
        g_camDist = kTitleScreenCameraDistance;

        const TitleSceneAssets::Paths paths =
        {
            g_gameUiConfig.title.logoDat,
            g_gameUiConfig.title.atlasDat,
            g_gameUiConfig.title.uiDat,
        };
        g_titleAssets.LoadAll(
            GraphicsDevice(), g_ffxiPath, paths,
            g_applicationSettings.enableTextureCompression);
    }
    ZoneEnvironmentState::Invalidate(g_zoneEnvironmentCache, true);

    // The title presentation starts with Konschtat's authored sunny sky. The
    // ordinary zone default remains independent of this title-only selection.
    UpdateZoneEnvironmentState();
    for (size_t weather = 0; weather < g_zoneEnvironmentCache.weatherGroups.size(); ++weather)
    {
        const ff11EnvironmentRecord_t *record = g_zoneEnvironmentCache.weatherGroups[weather];
        if (record && ZoneEnvironmentIdentity::ContainsLowerToken(
                record->directoryPath, kTitleScreenWeatherToken))
        {
            g_zoneWeatherIndex = static_cast<int>(weather);
            ZoneEnvironmentState::Invalidate(g_zoneEnvironmentCache, false);
            UpdateZoneEnvironmentState();
            break;
        }
    }

    SyncAppMusic();
}

static void LoadCreationEntry(const FFXICreationEntry *pEntry)
{
    if (!pEntry)
        return;

    SetInteractionMode(InteractionController::Mode::Edit);
    g_titleScreenActive = false;
    g_highPolyCreationActive = true;
    g_nationSelectActive = false;
    InteractionController::SetGameMusicId(g_interaction, 0);
    UnloadTitleAssets();
    SyncAppMusic();
    UnloadCreationModel();

    SceneLoadContext::Options creationZoneOptions;
    creationZoneOptions.userContentLoad = false;
    creationZoneOptions.renderEnvironment = true;
    creationZoneOptions.preserveGameScreen = true;
    const SceneRequestResolver::Result creationZone =
        SceneRequestResolver::ResolveRelativeModel(
            g_ffxiPath, kFFXICharacterCreationZoneDat,
            "Character Selection / Creation", creationZoneOptions);
    const bool creationZoneLoaded = g_zoneAsset &&
        g_sceneLoadContext.MatchesPath(creationZone.request.path.c_str());
    if (!creationZoneLoaded)
    {
        // Preserve the existing load error and old-zone teardown behavior even
        // when request validation already knows the backdrop file is missing.
        LoadDatFile(creationZone.request);
        if (g_zoneAsset)
            SetLoadedSceneContext(creationZone.request);
    }
    if (!g_zoneAsset)
        return;

    CreationModelLoader::Request modelRequest;
    modelRequest.ffxiRoot = g_ffxiPath;
    modelRequest.label = pEntry->label;
    modelRequest.bodyMeshDat = pEntry->bodyMeshDat;
    modelRequest.bodyMaterialDat = pEntry->bodyMaterialDat;
    modelRequest.headMeshDat = pEntry->headMeshDat;
    modelRequest.headMaterialDat = pEntry->headMaterialDat;
    modelRequest.bodyAnimationDat =
        CreationBodyAnimDatForRace(g_creationSelection.raceIndex);
    modelRequest.headAnimationDat =
        CreationHeadAnimDatForRace(g_creationSelection.raceIndex);
    modelRequest.headAlphaMode = pEntry->headAlphaMode;
    modelRequest.headYOffset = pEntry->headYOffset;
    modelRequest.enableTextureCompression =
        g_applicationSettings.enableTextureCompression;

    CreationModelLoader::Result modelResult =
        CreationModelLoader::Load(GraphicsDevice(), modelRequest);
    if (modelResult.skeletonInspectionPerformed)
        ZoneEnvironmentState::Invalidate(g_zoneEnvironmentCache, true);
    if (!modelResult.Succeeded())
    {
        switch (modelResult.error)
        {
        case CreationModelLoader::Error::NoMeshPaths:
            MessageBoxA(g_hWnd, "This character creation entry has no mesh DATs assigned.",
                        "Creation Load", MB_OK | MB_ICONWARNING);
            break;
        case CreationModelLoader::Error::NoDisplayableGeometry:
            MessageBoxA(g_hWnd, "Creation DATs loaded but contained no displayable geometry.",
                        "Creation Load", MB_OK | MB_ICONWARNING);
            break;
        case CreationModelLoader::Error::InvalidRequest:
        case CreationModelLoader::Error::MeshFileOpenFailed:
        default:
            MessageBoxA(g_hWnd, "Could not open one of the character creation mesh DATs.",
                        "Creation Load", MB_OK | MB_ICONERROR);
            break;
        }
        return;
    }

    g_creationAsset = std::move(modelResult.asset);
    g_creationBodyMotion = {};
    g_creationHeadMotion = {};
    g_creationAnimTime = 0.0f;
    if (g_creationAnimationIndex == 1)
    {
        FFXISqle::ReadMotionInfo(g_ffxiPath,
                                 CreationPreviewIdleBodyDatForRace(g_creationSelection.raceIndex),
                                 &g_creationBodyMotion);
        FFXISqle::ReadMotionInfo(g_ffxiPath,
                                 CreationPreviewIdleHeadDatForRace(g_creationSelection.raceIndex),
                                 &g_creationHeadMotion);
    }
    else if (g_creationAnimationIndex == 2)
    {
        FFXISqle::ReadMotionInfo(g_ffxiPath,
                                 CreationBodyAnimDatForRace(g_creationSelection.raceIndex),
                                 &g_creationBodyMotion);
        FFXISqle::ReadMotionInfo(g_ffxiPath,
                                 CreationHeadAnimDatForRace(g_creationSelection.raceIndex),
                                 &g_creationHeadMotion);
        // Begin long presentations at their first sustained transition instead
        // of making every model/UI reload sit through a multi-second opening
        // hold. The ordinary modulo playback still includes the entire file
        // after it wraps.
        g_creationAnimTime =
            FFXISqleModelAnimation::SuggestedPreviewStartTime(g_creationBodyMotion);
        FFXICreationCamera::Load(g_ffxiPath, g_creationSelection.raceIndex, 0,
                                 g_creationCameraTrack);
    }
    FFXISqleModelAnimation::BuildSkeletalAnimation(
        g_creationAsset.Model(), g_creationBodyMotion, g_creationHeadMotion);
    FFXICreationGrounding::FindHorizontalPlacement(
        g_creationAsset.Model(), g_zoneCollisionMesh, g_creationHorizontalPlacement);
    g_camDist = 25.0f;
    g_camYaw = 0.0f;
    g_camPitch = -0.15f;
    float creationRootOrigin[3] = {};
    if (FFXISqleModelAnimation::GetRootMotionOrigin(
            g_creationAsset.Model(), creationRootOrigin))
    {
        g_camTarget[0] = creationRootOrigin[0] + g_creationHorizontalPlacement[0];
        g_camTarget[1] = creationRootOrigin[1] - 8.0f;
        g_camTarget[2] = creationRootOrigin[2] + g_creationHorizontalPlacement[2];
    }
    else
    {
        g_camTarget[0] = g_camTarget[2] = 0.0f;
        g_camTarget[1] = -12.0f;
    }
}

static void LoadDatSetFile(const char *path)
{
    g_titleScreenActive = false;
    g_highPolyCreationActive = false;
    g_nationSelectActive = false;
    InteractionController::SetGameMusicId(g_interaction, 0);
    UnloadCreationModel();
    UnloadTitleAssets();
    SyncAppMusic();
    ResetZoneModelLoadState(path);

    SceneModelLoader::Result loadResult = SceneModelLoader::LoadDatSet(
        GraphicsDevice(), path, g_applicationSettings.enableTextureCompression);
    if (!loadResult.Succeeded())
    {
        ShowSceneModelLoadError(loadResult.error);
        return;
    }

    g_zoneAsset = std::move(loadResult.asset);
    g_camDist   = 5.0f;
    g_camYaw    = 0.0f;
    g_camPitch  = -0.25f;
    g_camTarget[0] = g_camTarget[1] = g_camTarget[2] = 0.0f;
}

static FFXIModelLifetime::OwnedModel LoadNpcStandaloneModel(unsigned int modelId)
{
    // The original retail NPC graph is contiguous with file 358. Expansion
    // graph IDs use additional lookup tables; do not substitute an unrelated
    // DAT when that mapping is not known.
    if (modelId > 0xDF)
        return {};

    char relativePath[MAX_PATH] = {};
    if (!FFXIPath::BuildRelativePathFromFileId(MAKELONG(modelId + 358, 1), relativePath, sizeof(relativePath)))
        return {};
    char fullPath[MAX_PATH] = {};
    FFXIPath::BuildFullPath(g_ffxiPath, relativePath, fullPath, sizeof(fullPath));

    BYTE *buffer = nullptr;
    DWORD bufferSize = 0;
    if (!FFXIFileIO::ReadWholeFile(fullPath, &buffer, &bufferSize))
        return {};

    FFXIModelLifetime::OwnedModel resource;
    resource.Adopt(nullptr, new noeRAPI_t(GraphicsDevice()));
    ConfigureRapiTextureSettings(resource.ParserContext());
    resource.ParserContext()->SetCurrentFilePath(fullPath);
    int modelCount = 0;
    noesisModel_t *model = Model_FF11_LoadDAT(
        buffer, (int)bufferSize, modelCount, resource.ParserContext());
    delete[] buffer;
    if (!model || modelCount == 0)
        return {};

    resource.AttachModel(model);
    return resource;
}

static FFXIModelLifetime::OwnedModel LoadNpcHumanoidModel(
    const std::vector<std::uint8_t> &look)
{
    char datSet[4096] = {};
    if (!FFXIDatSet::BuildNpc(g_ffxiPath, look, datSet, sizeof(datSet)))
        return {};

    FFXIModelLifetime::OwnedModel resource;
    resource.Adopt(nullptr, new noeRAPI_t(GraphicsDevice()));
    ConfigureRapiTextureSettings(resource.ParserContext());
    resource.ParserContext()->SetCurrentFilePath("DATura generated NPC.ff11datset");
    int modelCount = 0;
    noesisModel_t *model = Model_FF11_LoadDATSet(
        (BYTE *)datSet, (int)strlen(datSet), modelCount, resource.ParserContext());
    if (!model || modelCount == 0)
        return {};

    resource.AttachModel(model);
    return resource;
}

static void UnloadNpcModels()
{
    g_homePointSeconds = 0.0;
    g_npcInteraction = {};
    g_doors.selected = -1;
    NpcChatWindow::Reset();
    g_npcNameplates.clear();
    g_npcInteraction.targets.clear();
    NpcNameplateRenderer::ClearCachedTextures();
    g_npcRenderInstances.clear();
    g_npcRenderAssets.clear();
}

static void LoadNpcModelsForCurrentZone()
{
    UnloadNpcModels();
    if (!GraphicsDevice())
        return;

    std::map<std::string, size_t> appearanceCache;
    const size_t invalidAsset = static_cast<size_t>(-1);
    for (const FFXINpcPlacement::Placement &placement : FFXINpcPlacement::Snapshot())
    {
        if (!placement.visible)
            continue;
        const std::string key = FFXINpcPlacement::AppearanceKey(placement.appearance);
        const auto cached = appearanceCache.find(key);
        size_t assetIndex = invalidAsset;
        if (cached != appearanceCache.end())
            assetIndex = cached->second;
        else
        {
            NpcRenderAsset asset;
            if (placement.appearance.kind == FFXINpcPlacement::AppearanceKind::ModelId &&
                placement.appearance.modelId == HomePoint::ModelId)
            {
                auto effect = std::make_unique<HomePoint::Effect>();
                if (effect->Load(GraphicsDevice(), g_ffxiPath, g_applicationSettings.enableTextureCompression))
                    asset.homePoint = std::move(effect);
            }
            else if (placement.appearance.kind == FFXINpcPlacement::AppearanceKind::HumanoidLook)
                asset.resource = LoadNpcHumanoidModel(placement.appearance.rawLook);
            else if (placement.appearance.kind == FFXINpcPlacement::AppearanceKind::ModelId)
                asset.resource = LoadNpcStandaloneModel(placement.appearance.modelId);

            if (asset.resource || asset.homePoint)
            {
                // Assemble the initial skinned pose before measuring the head
                // anchor; otherwise the first frame uses component bind space.
                if (asset.homePoint)
                    asset.nameplateLocalY = HomePoint::NameplateY;
                else if (NpcRenderGeometry::SelectIdleAnimation(asset.resource.Model()))
                    asset.resource.Model()->UpdateAnimation(0.0f, GraphicsDevice());
                else
                    asset.resource.Model()->RestoreBindPose(GraphicsDevice());
                if (!asset.homePoint)
                    asset.nameplateLocalY = NpcRenderGeometry::ComputeNameplateLocalY(asset.resource.Model());
                assetIndex = g_npcRenderAssets.size();
                g_npcRenderAssets.push_back(std::move(asset));
            }
            appearanceCache[key] = assetIndex;
        }
        if (assetIndex != invalidAsset)
        {
            FFXINpcPlacement::Placement scenePlacement = placement;
            scenePlacement.transform = FFXINpcPlacement::ToSceneTransform(
                placement.transform, !g_applicationSettings.mirrorWorldZones);
            // The crystal's authored ground ring must stay at the catalog's
            // elevation; nearby steps/bridges can otherwise pull it off its base.
            if (!g_npcRenderAssets[assetIndex].homePoint)
                SnapPlacementToZoneFloor(scenePlacement.transform);
            g_npcRenderInstances.push_back({ std::move(scenePlacement), assetIndex });
        }
    }
}

static void UpdateNpcAnimations(float dt)
{
    if (std::isfinite(dt) && dt > 0) g_homePointSeconds += dt;
    if (!GraphicsDevice())
        return;
    for (NpcRenderAsset &asset : g_npcRenderAssets)
    {
        if (!asset.resource || !asset.visibleLastFrame)
            continue;
        asset.animationTime += dt;
        asset.resource.Model()->UpdateAnimation(asset.animationTime, GraphicsDevice());
        asset.nameplateLocalY =
            NpcRenderGeometry::ComputeNameplateLocalY(asset.resource.Model());
    }
}

static const char *CreationBodyAnimDatForRace(int raceIndex)
{
    return FFXICreationAnimationPaths::BodyBase(raceIndex);
}

static const char *CreationHeadAnimDatForRace(int raceIndex)
{
    const FFXICreationEntry *entry = CurrentHighPolyCreationEntry();
    return FFXICreationAnimationPaths::HeadBase(raceIndex, entry ? entry->headMeshDat : nullptr);
}

static const char *CreationPreviewIdleBodyDatForRace(int raceIndex)
{
    return FFXICreationAnimationPaths::BodyIdle(raceIndex);
}

static const char *CreationPreviewIdleHeadDatForRace(int raceIndex)
{
    const FFXICreationEntry *entry = CurrentHighPolyCreationEntry();
    return FFXICreationAnimationPaths::HeadIdle(raceIndex, entry ? entry->headMeshDat : nullptr);
}

static void SaveHighPolyCreationCharacter()
{
    PullHighPolyCreationStateFromControls();

    char defaultPath[MAX_PATH] = {};
    FFXICreationExport::MakeDefaultNoesisPath(g_creationCharacterName,
                                               defaultPath, sizeof(defaultPath));

    char savePath[MAX_PATH] = {};
    strcpy_s(savePath, defaultPath);
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    const HWND panel = HighPolyCreationPanel::Window(g_highPolyCreationPanel);
    ofn.hwndOwner = panel ? panel : g_hWnd;
    ofn.lpstrFilter = "Noesis Scene (*.noesis)\0*.noesis\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = savePath;
    ofn.nMaxFile = sizeof(savePath);
    ofn.lpstrTitle = "Save Character";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = "noesis";
    char characterDir[MAX_PATH] = {};
    FFXIFileIO::GetExecutableDirectory(characterDir, sizeof(characterDir));
    strcat_s(characterDir, "Characters");
    CreateDirectoryA(characterDir, nullptr);
    ofn.lpstrInitialDir = characterDir;
    if (!GetSaveFileNameA(&ofn))
        strcpy_s(savePath, defaultPath);

    char noesisPath[MAX_PATH] = {};
    char datSetPath[MAX_PATH] = {};
    FFXIFileIO::ReplaceExtension(savePath, ".noesis", noesisPath, sizeof(noesisPath));
    FFXIFileIO::ReplaceExtension(savePath, ".ff11datset", datSetPath, sizeof(datSetPath));

    char noesisText[4096] = {};
    char datSetText[4096] = {};
    const FFXICreationEntry *creationEntry = CurrentHighPolyCreationEntry();
    FFXICreationExport::Inputs exportInputs;
    exportInputs.raceIndex = g_creationSelection.raceIndex;
    exportInputs.equipmentIndex = g_creationSelection.equipmentIndex;
    exportInputs.bodyMeshDat = creationEntry ? creationEntry->bodyMeshDat : nullptr;
    exportInputs.headMeshDat = creationEntry ? creationEntry->headMeshDat : nullptr;
    FFXICreationExport::BuildNoesisScene(exportInputs, noesisText, sizeof(noesisText));
    const FFXIDatSet::LowPolyOptions lowPolyOptions =
    {
        g_creationSelection.raceIndex,
        g_creationSelection.faceIndex,
        g_creationSelection.equipmentIndex,
    };
    FFXIDatSet::BuildLowPoly(lowPolyOptions, datSetText, sizeof(datSetText));
    // Character roster files must resolve against DATura's configured FFXI
    // root, not an optional PlayOnline registry key.
    char escapedRoot[MAX_PATH * 2] = {};
    for (const char* source = g_ffxiPath; *source && strlen(escapedRoot) + 2 < sizeof(escapedRoot); ++source)
    {
        if (*source == '\\') strcat_s(escapedRoot, "\\");
        char character[2] = {*source, 0};
        strcat_s(escapedRoot, character);
    }
    char* pathKey = strstr(datSetText, "setPathKey");
    if (pathKey)
    {
        char* lineEnd = strchr(pathKey, '\n');
        if (lineEnd)
        {
            char remainder[4096] = {};
            strcpy_s(remainder, sizeof(remainder), lineEnd + 1);
            sprintf_s(pathKey, sizeof(datSetText) - (pathKey - datSetText),
                "setPathAbs\t\"%s\"\n%s", escapedRoot, remainder);
        }
    }

    const bool wroteNoesis = FFXIFileIO::WriteTextFile(noesisPath, noesisText);
    const bool wroteDatSet = FFXIFileIO::WriteTextFile(datSetPath, datSetText);
    if (!wroteNoesis || !wroteDatSet)
    {
        MessageBoxA(panel ? panel : g_hWnd,
                    "Could not save one or both character files.",
                    "Save Character", MB_OK | MB_ICONERROR);
        return;
    }

    FFXISaveResultDialog::Show(g_highPolyCreationActive && g_hWnd ? g_hWnd : NULL,
                               g_hWnd, noesisPath, datSetPath);
}

static void BeginNationSelectScene()
{
    PullHighPolyCreationStateFromControls();
    HighPolyCreationPanel::Hide(g_highPolyCreationPanel);

    g_titleScreenActive = false;
    g_highPolyCreationActive = true;
    g_nationSelectActive = true;
    g_selectedNationIndex = 0;
    EnsureNationSelectAssets();
    InvalidateRect(g_hWnd, NULL, FALSE);
}

static bool TextStartsWithNoCase(const std::string& text, const char* prefix)
{
    if (!prefix)
        return false;
    const std::size_t length = std::strlen(prefix);
    if (text.size() < length)
        return false;
    for (std::size_t i = 0; i < length; ++i)
    {
        if (std::tolower(static_cast<unsigned char>(text[i])) !=
            std::tolower(static_cast<unsigned char>(prefix[i])))
            return false;
    }
    return true;
}

static bool IsObjectLikeNpcTarget(const FFXINpcPlacement::Placement& placement)
{
    const std::string& name = placement.name;
    return name.empty() || name == "???" ||
        TextStartsWithNoCase(name, "Target") ||
        TextStartsWithNoCase(name, "Home Point") ||
        TextStartsWithNoCase(name, "Survival Guide") ||
        TextStartsWithNoCase(name, "Waypoint");
}

static std::string RetailNpcDialogueText(const FFXINpcPlacement::Placement& placement)
{
    auto trace = [](const std::string& text)
    {
        OutputDebugStringA(("[DATura NPC dialogue] " + text + "\n").c_str());
    };
    trace("begin entity=" + std::to_string(placement.entityId) + " name=" + placement.name);
    const FFXILoreZoneDatEntry* zone = FFXILoreZoneDat_FindByZoneID(FFXINpcPlacement::ZoneId());
    if (!zone || !g_ffxiPath || !g_ffxiPath[0])
    {
        trace("missing zone mapping or FFXI root; zone=" + std::to_string(FFXINpcPlacement::ZoneId()));
        return std::string();
    }
    char eventPath[MAX_PATH] = {};
    char messagePath[MAX_PATH] = {};
    FFXIPath::BuildFullPath(g_ffxiPath, zone->eventDat, eventPath, sizeof(eventPath));
    FFXIPath::BuildFullPath(g_ffxiPath, zone->dialogDat, messagePath, sizeof(messagePath));
    trace(std::string("event=") + eventPath + " messages=" + messagePath);
    std::vector<FFXIEventTable::EntityScripts> entities;
    std::vector<FFXIEventMessages::Entry> messages;
    if (!FFXIEventTable::Load(eventPath, entities))
    {
        trace("event table load failed");
        return std::string();
    }
    trace("event blocks=" + std::to_string(entities.size()));
    if (!FFXIEventMessages::Load(messagePath, messages))
    {
        trace("event message table load failed");
        return std::string();
    }
    trace("message entries=" + std::to_string(messages.size()));
    for (const auto& scripts : entities)
    {
        if (scripts.entityId != placement.entityId) continue;
        trace("matched entity events=" + std::to_string(scripts.events.size()));
        for (const auto& event : scripts.events)
        {
            const auto ids = FFXIEventTable::MessageIds(event);
            for (const std::uint16_t id : ids)
            {
                trace("event=" + std::to_string(event.id) + " message=" + std::to_string(id));
                if (id < messages.size() && !messages[id].text.empty())
                {
                    trace("resolved message");
                    return messages[id].text;
                }
            }
        }
        trace("matched entity had no resolvable message");
    }
    trace("entity not found");
    return std::string();
}

static std::string NpcDialogueText(const FFXINpcPlacement::Placement& placement)
{
    if (!placement.dialogue.empty())
        return placement.dialogue;
    if (!placement.starterQuestName.empty())
        return "I can offer you the quest \"" + placement.starterQuestName + "\".";
    const std::string retail = RetailNpcDialogueText(placement);
    if (!retail.empty())
        return retail;
    if (!IsObjectLikeNpcTarget(placement))
        return "My retail dialogue exists, but DATura has not mapped this NPC's event script yet.";
    return std::string();
}

static void ReturnToTitleScreen()
{
    HighPolyCreationPanel::Hide(g_highPolyCreationPanel);
    LowPolyCharacterPanel::Hide(g_lowPolyPanel);
    ZoneObjectPanel::Hide(g_zoneObjectPanel);

    UnloadPlayerModel();
    g_highPolyCreationActive = false;
    g_nationSelectActive = false;
    LoadTitleScreen();
    if (g_hWnd)
        InvalidateRect(g_hWnd, NULL, FALSE);
}

static void ApplyCreationStateToLowPolyPlayer()
{
    g_playerEquip.raceIndex =
        (g_creationSelection.raceIndex >= 0 &&
         g_creationSelection.raceIndex < kFFXICharRaceCount)
            ? g_creationSelection.raceIndex
            : 0;
    g_playerFaceVariant = (g_creationSelection.faceIndex > 0)
        ? g_creationSelection.faceIndex
        : 0;

    const int equipmentVariant = (g_creationSelection.equipmentIndex == 1) ? 1 : 0;
    g_playerEquip.mainType = 0;
    g_playerEquip.mainItem = 0;
    g_playerEquip.subType = 0;
    g_playerEquip.subItem = 0;
    g_playerEquip.rangedType = 0;
    g_playerEquip.rangedItem = 0;
    g_playerEquip.headItem = 0;
    g_playerEquip.bodyItem = equipmentVariant;
    g_playerEquip.handsItem = equipmentVariant;
    g_playerEquip.legsItem = equipmentVariant;
    g_playerEquip.feetItem = equipmentVariant;
    g_playerEquip.animationBank = 0;
    g_playerEquip.animationMode = 0;
    g_playerEquip.animationPlaying = false;
    ClampLowPolyState();
}

static void ConfirmExitFromTitleScreen()
{
    const int result = MessageBoxA(g_hWnd,
        "Are you sure you want to exit DATura?",
        "Exit DATura",
        MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (result == IDYES)
    {
        if (g_hWnd)
            DestroyWindow(g_hWnd);
        else
            PostQuitMessage(0);
    }
}

static void ClampLowPolyState()
{
    FFXIPlayerCustomizationState::Clamp(g_playerEquip, g_playerFaceVariant);
}

static void FinishLowPolyRandomize()
{
    SyncLowPolyControlsFromState();
    ReloadPlayerModelFromControls();
}

static void RandomizeLowPolyCharacterSection()
{
    PullLowPolyStateFromControls();

    FFXIPlayerRandomizer::RandomizeCharacter(g_playerEquip, g_playerFaceVariant);
    g_playerAnimTime = 0.0f;
    FinishLowPolyRandomize();
}

static void RandomizeLowPolyWeaponsSection()
{
    PullLowPolyStateFromControls();

    FFXIPlayerRandomizer::RandomizeWeapons(g_playerEquip);
    g_playerAnimTime = 0.0f;
    FinishLowPolyRandomize();
}

static void RandomizeLowPolyArmorSection()
{
    PullLowPolyStateFromControls();

    FFXIPlayerRandomizer::RandomizeArmor(g_playerEquip);
    g_playerAnimTime = 0.0f;
    FinishLowPolyRandomize();
}

static void RandomizeLowPolyActionSection()
{
    PullLowPolyStateFromControls();

    FFXIPlayerRandomizer::RandomizeAction(g_playerEquip, g_playerFaceVariant);
    g_playerAnimTime = 0.0f;
    FinishLowPolyRandomize();
}

static void RandomizeLowPolyAll()
{
    PullLowPolyStateFromControls();

    FFXIPlayerRandomizer::RandomizeAll(g_playerEquip, g_playerFaceVariant);
    g_playerAnimTime = 0.0f;
    FinishLowPolyRandomize();
}

static void SyncLowPolyControlsFromState()
{
    LowPolyCharacterPanel::Sync(g_lowPolyPanel);
}

static const FFXICreationEntry *CurrentHighPolyCreationEntry()
{
    return FFXICreationSelectionState::CurrentEntry(g_creationSelection);
}

static bool SetHighPolyCreationSelectionFromFlatIndex(int flatIndex)
{
    return FFXICreationSelectionState::SetFromFlatIndex(flatIndex, g_creationSelection);
}

static void SyncHighPolyCreationControlsFromState()
{
    HighPolyCreationPanel::Sync(g_highPolyCreationPanel);
}

static void PullHighPolyCreationStateFromControls()
{
    HighPolyCreationPanel::Pull(g_highPolyCreationPanel);
}

static void ReloadHighPolyCreationFromControls()
{
    PullHighPolyCreationStateFromControls();
    if (g_creationAnimationIndex > 0 &&
        FFXICreationAnimationPaths::SequenceUsesInitialEquipment(g_creationSelection.raceIndex))
    {
        g_creationSelection.equipmentIndex = g_creationAnimationIndex == 2 ? 1 : 0;
    }
    SyncHighPolyCreationControlsFromState();

    const FFXICreationEntry *pEntry = CurrentHighPolyCreationEntry();
    if (pEntry)
        LoadCreationEntry(pEntry);
}

static void PullLowPolyStateFromControls()
{
    LowPolyCharacterPanel::Pull(g_lowPolyPanel);
}

static void SaveLowPolyPreset()
{
    char path[MAX_PATH] = {};
    const HWND panel = LowPolyCharacterPanel::Window(g_lowPolyPanel);
    if (!FFXIPlayerPresetDialog::PromptForSavePath(panel ? panel : g_hWnd,
                                                   path, sizeof(path)))
    {
        return;
    }
    PullLowPolyStateFromControls();
    FFXIPlayerPresetDialog::Write(
        path, FFXIPlayerCustomizationState::CapturePreset(
            g_playerEquip, g_playerFaceVariant));
}

static void LoadLowPolyPreset()
{
    FFXIPlayerPreset::Data preset = FFXIPlayerCustomizationState::CapturePreset(
        g_playerEquip, g_playerFaceVariant);
    const HWND panel = LowPolyCharacterPanel::Window(g_lowPolyPanel);
    if (FFXIPlayerPresetDialog::Load(panel ? panel : g_hWnd, &preset))
    {
        FFXIPlayerCustomizationState::ApplyPreset(
            preset, g_playerEquip, g_playerFaceVariant);
        ClampLowPolyState();
        SyncLowPolyControlsFromState();
        ReloadPlayerModelFromControls();
    }
}

static void HandleLowPolyPanelEvent(
    void*, const LowPolyCharacterPanel::Command command)
{
    switch (command)
    {
    case LowPolyCharacterPanel::Command::LoadPreset:
        LoadLowPolyPreset();
        break;
    case LowPolyCharacterPanel::Command::SavePreset:
        SaveLowPolyPreset();
        break;
    case LowPolyCharacterPanel::Command::RandomizeCharacter:
        RandomizeLowPolyCharacterSection();
        break;
    case LowPolyCharacterPanel::Command::RandomizeWeapons:
        RandomizeLowPolyWeaponsSection();
        break;
    case LowPolyCharacterPanel::Command::RandomizeArmor:
        RandomizeLowPolyArmorSection();
        break;
    case LowPolyCharacterPanel::Command::RandomizeAction:
        RandomizeLowPolyActionSection();
        break;
    case LowPolyCharacterPanel::Command::RandomizeAll:
        RandomizeLowPolyAll();
        break;
    case LowPolyCharacterPanel::Command::Play:
        PullLowPolyStateFromControls();
        g_playerEquip.animationPlaying = true;
        ReloadPlayerModelFromControls();
        break;
    case LowPolyCharacterPanel::Command::Stop:
        g_playerEquip.animationPlaying = false;
        if (g_playerAsset)
            g_playerAsset.Model()->RestoreBindPose(GraphicsDevice());
        break;
    case LowPolyCharacterPanel::Command::Reset:
        g_playerEquip.animationPlaying = false;
        g_playerAnimTime = 0.0f;
        if (g_playerAsset)
            g_playerAsset.Model()->RestoreBindPose(GraphicsDevice());
        break;
    case LowPolyCharacterPanel::Command::SelectionChanged:
        ReloadPlayerModelFromControls();
        break;
    }
}

static void HandleHighPolyCreationPanelEvent(
    void*, const HighPolyCreationPanel::Command command)
{
    switch (command)
    {
    case HighPolyCreationPanel::Command::ReturnToTitle:
        ReturnToTitleScreen();
        break;
    case HighPolyCreationPanel::Command::ChooseNation:
        BeginNationSelectScene();
        break;
    case HighPolyCreationPanel::Command::SaveCharacter:
        SaveHighPolyCreationCharacter();
        break;
    case HighPolyCreationPanel::Command::SelectionChanged:
        ReloadHighPolyCreationFromControls();
        break;
    }
}

static void ShowLowPolyControlPanel()
{
    LowPolyCharacterPanel::Show(g_lowPolyPanel);
}

static void ShowHighPolyCreationPanel()
{
    HighPolyCreationPanel::Show(g_highPolyCreationPanel);
}

static void BeginHighPolyCreationScene()
{
    LowPolyCharacterPanel::Hide(g_lowPolyPanel);

    g_nationSelectActive = false;
    // Match the standard FFXI character-creation presentation on entry.
    g_creationSelection = {};
    g_creationSelection.equipmentIndex = 1; // Initial Equipment
    g_creationAnimationIndex = 2;           // Character creation sequence
    g_creationAnimatedCamera = true;

    const FFXICreationEntry *pEntry = CurrentHighPolyCreationEntry();
    if (pEntry)
        LoadCreationEntry(pEntry);
    ShowHighPolyCreationPanel();
}

static void PopulateZoneDataTree()
{
    ZoneObjectPanel::RebuildTree(g_zoneObjectPanel, g_loadedZoneLabel.c_str());
}

static int GetSelectedZoneObjectIndex()
{
    return ZoneObjectPanel::SelectedMapObjectIndex(g_zoneObjectPanel);
}

static const char *GetSelectedZoneObjectName()
{
    const int index = GetSelectedZoneObjectIndex();
    return (index >= 0) ? Model_FF11_GetLastMapObjectDisplayName(index) : "";
}

static void UpdateZoneObjectEditControlState()
{
    ZoneObjectPanel::SetEditingEnabled(g_zoneObjectPanel, IsEditMode());
}

static void RefreshZoneObjectPanel()
{
    ZoneObjectPanel::RefreshData data = {};
    data.zoneLabel = g_loadedZoneLabel.c_str();
    data.overrides = &g_zoneObjectOverrides;
    data.hiddenObjectNames = &g_hiddenZoneObjects;
    data.collisionTriangleCount = static_cast<int>(g_zoneCollisionTris.size());
    const noesisModel_t* zoneModel = g_zoneAsset.Model();
    data.modelMeshCount = zoneModel ? static_cast<int>(zoneModel->submeshes.size()) : 0;
    data.modelMaterialCount = (zoneModel && zoneModel->pMatData)
        ? zoneModel->pMatData->matCount : 0;
    data.modelTextureCount = (zoneModel && zoneModel->pMatData)
        ? zoneModel->pMatData->texCount : 0;
    data.modelBoneCount = zoneModel ? zoneModel->boneCount : 0;
    data.editingEnabled = IsEditMode();
    ZoneObjectPanel::Refresh(g_zoneObjectPanel, data);
}

static void BeginZoneObjectPanelRefresh()
{
    ZoneObjectPanel::BeginRefresh(g_zoneObjectPanel, g_loadedZoneLabel.c_str());
}

static void HandleZoneObjectPanelEvent(
    void*, const ZoneObjectPanel::Event& event)
{
    const HWND panel = ZoneObjectPanel::Window(g_zoneObjectPanel);
    switch (event.type)
    {
    case ZoneObjectPanel::EventType::RefreshRequested:
        RefreshZoneObjectPanel();
        return;
    case ZoneObjectPanel::EventType::TreeExpansionRequested:
        PopulateExpandedNode(
            ZoneObjectPanel::TreeWindow(g_zoneObjectPanel, event.tree),
            event.treeItem, event.treeItemData, g_hiddenZoneObjects);
        return;
    case ZoneObjectPanel::EventType::TreeSelectionChanged:
        ZoneObjectPanel::SetTreeSelectedMapObjectIndex(
            g_zoneObjectPanel,
            event.mapObjectIndex >= 0 &&
            event.mapObjectIndex < Model_FF11_GetLastMapObjectCount()
                ? event.mapObjectIndex : -1);
        ZoneObjectPanel::PopulateTransformFields(
            g_zoneObjectPanel, GetSelectedZoneObjectIndex(), g_zoneObjectOverrides);
        return;
    case ZoneObjectPanel::EventType::MapObjectVisibilityChanged:
        if (event.mapObjectIndex >= 0)
        {
            SetHidden(g_hiddenZoneObjects,
                Model_FF11_GetLastMapObjectDisplayName(event.mapObjectIndex),
                !event.mapObjectVisible);
            if (g_hWnd)
                InvalidateRect(g_hWnd, NULL, FALSE);
        }
        return;
    case ZoneObjectPanel::EventType::MapObjectSelectionChanged:
        ZoneObjectPanel::PopulateTransformFields(
            g_zoneObjectPanel, GetSelectedZoneObjectIndex(), g_zoneObjectOverrides);
        return;
    case ZoneObjectPanel::EventType::Command:
        break;
    }

    switch (event.command)
    {
    case ZoneObjectPanel::Command::ShowPlaced:
        SetListVisibility(g_hZoneObjectList, true, g_hiddenZoneObjects,
                          g_populatingZoneObjectList, Model_FF11_GetLastMapObjectDisplayName);
        PopulateZoneDataTree();
        break;
    case ZoneObjectPanel::Command::HidePlaced:
        SetListVisibility(g_hZoneObjectList, false, g_hiddenZoneObjects,
                          g_populatingZoneObjectList, Model_FF11_GetLastMapObjectDisplayName);
        PopulateZoneDataTree();
        break;
    case ZoneObjectPanel::Command::ShowRaw:
        SetListVisibility(g_hZoneUnrefObjectList, true, g_hiddenZoneObjects,
                          g_populatingZoneObjectList, Model_FF11_GetLastMapObjectDisplayName);
        PopulateZoneDataTree();
        break;
    case ZoneObjectPanel::Command::HideRaw:
        SetListVisibility(g_hZoneUnrefObjectList, false, g_hiddenZoneObjects,
                          g_populatingZoneObjectList, Model_FF11_GetLastMapObjectDisplayName);
        PopulateZoneDataTree();
        break;
    case ZoneObjectPanel::Command::ToggleCombinedTree:
        ZoneObjectPanel::ToggleCombinedTree(g_zoneObjectPanel);
        PopulateZoneDataTree();
        if (panel)
        {
            RECT client = {};
            GetClientRect(panel, &client);
            SendMessageA(panel, WM_SIZE, 0,
                MAKELPARAM(client.right - client.left, client.bottom - client.top));
        }
        break;
    case ZoneObjectPanel::Command::ShowOnlySelected:
        {
            const int selectedIndex = GetSelectedZoneObjectIndex();
            if (selectedIndex >= 0)
            {
                ShowOnlySelectedMapObject(
                    g_hZoneObjectList, g_hZoneUnrefObjectList, selectedIndex,
                    Model_FF11_GetLastMapObjectCount(), g_hiddenZoneObjects,
                    g_populatingZoneObjectList, Model_FF11_GetLastMapObjectDisplayName);
                PopulateZoneDataTree();
            }
        }
        break;
    case ZoneObjectPanel::Command::HideSelected:
        {
            const int selectedIndex = GetSelectedZoneObjectIndex();
            if (selectedIndex >= 0)
            {
                SetHidden(g_hiddenZoneObjects,
                    Model_FF11_GetLastMapObjectDisplayName(selectedIndex), true);
                SetCheckStateForMapObjectIndex(
                    g_hZoneObjectList, g_hZoneUnrefObjectList, selectedIndex, FALSE);
                PopulateZoneDataTree();
            }
        }
        break;
    case ZoneObjectPanel::Command::ToggleHighlight:
        {
            const char* objectName = GetSelectedZoneObjectName();
            if (objectName && objectName[0])
            {
                if (g_highlightedZoneObject == objectName)
                    g_highlightedZoneObject.clear();
                else
                    g_highlightedZoneObject = objectName;
                ZoneObjectPanel::SetHighlightActive(
                    g_zoneObjectPanel, !g_highlightedZoneObject.empty());
            }
        }
        break;
    case ZoneObjectPanel::Command::CenterSelected:
        {
            const int selectedIndex = GetSelectedZoneObjectIndex();
            if (selectedIndex >= 0)
            {
                float translation[3] = {};
                float rotation[3] = {};
                float scale[3] = {};
                Model_FF11_GetLastMapObjectTransform(selectedIndex, translation, scale, rotation);
                const char* objectName = Model_FF11_GetLastMapObjectDisplayName(selectedIndex);
                const std::map<std::string, DebugTransform>::const_iterator overrideIt =
                    g_zoneObjectOverrides.find(objectName ? objectName : "");
                if (overrideIt != g_zoneObjectOverrides.end())
                    memcpy(translation, overrideIt->second.trans, sizeof(translation));
                g_camTarget[0] = translation[0];
                g_camTarget[1] = translation[1];
                g_camTarget[2] = translation[2];
                if (g_camDist < 12.0f)
                    g_camDist = 12.0f;
            }
        }
        break;
    case ZoneObjectPanel::Command::ApplyTransform:
        {
            const int mapObjectIndex = GetSelectedZoneObjectIndex();
            if (mapObjectIndex >= 0)
            {
                const char* objectName = Model_FF11_GetLastMapObjectDisplayName(mapObjectIndex);
                if (objectName && objectName[0])
                    g_zoneObjectOverrides[objectName] = event.transform;
                RefreshZoneObjectPanel();
            }
        }
        break;
    case ZoneObjectPanel::Command::SetCollisionVisibility:
        g_applicationSettings.showCollisionGeometry = event.collisionVisible;
        RefreshZoneObjectPanel();
        break;
    }
    if (g_hWnd)
        InvalidateRect(g_hWnd, NULL, FALSE);
}

static void ShowZoneObjectPanel()
{
    ZoneObjectPanel::Show(
        g_zoneObjectPanel, g_loadedZoneLabel.c_str(),
        g_applicationSettings.showCollisionGeometry, IsEditMode());
    BeginZoneObjectPanelRefresh();
}

static void LoadPlayerRaceModel(int raceIndex)
{
    g_titleScreenActive = false;
    g_highPolyCreationActive = false;
    UnloadCreationModel();
    UnloadTitleAssets();
    BGM_Stop();
    if (raceIndex < 0 || raceIndex >= kFFXICharRaceCount)
        return;

    g_playerEquip.raceIndex = raceIndex;
    ClampLowPolyState();
    const FFXICharRace &race = kFFXICharRaces[raceIndex];
    if (race.count < 8)
    {
        MessageBoxA(g_hWnd, "Character table entry is incomplete.", "Player Load", MB_OK | MB_ICONWARNING);
        return;
    }

    UnloadPlayerModel();

    PlayerModelLoader::Request request;
    request.ffxiRoot = g_ffxiPath;
    request.customization.raceIndex = raceIndex;
    request.customization.faceVariant = g_playerFaceVariant;
    request.customization.animationBank = g_playerEquip.animationBank;
    request.customization.headItem = g_playerEquip.headItem;
    request.customization.bodyItem = g_playerEquip.bodyItem;
    request.customization.handsItem = g_playerEquip.handsItem;
    request.customization.legsItem = g_playerEquip.legsItem;
    request.customization.feetItem = g_playerEquip.feetItem;
    request.customization.mainItem = g_playerEquip.mainItem;
    request.customization.subItem = g_playerEquip.subItem;
    request.customization.rangedItem = g_playerEquip.rangedItem;
    request.enableTextureCompression =
        g_applicationSettings.enableTextureCompression;

    PlayerModelLoader::Result loadResult =
        PlayerModelLoader::Load(GraphicsDevice(), request);
    if (!loadResult.Succeeded())
    {
        const char* message = loadResult.error ==
            PlayerModelLoader::Error::IncompleteRaceEntry ?
            "Character table entry is incomplete." :
            "Player DAT set loaded but contained no displayable geometry.";
        MessageBoxA(g_hWnd, message, "Player Load", MB_OK | MB_ICONWARNING);
        return;
    }

    g_playerAsset = std::move(loadResult.asset);
    g_player.groundOffset = loadResult.groundOffset;
    g_player.cameraTargetLocalY = loadResult.cameraTargetLocalY;
    g_playerAnimTime = 0.0f;
    PlayerController::SetPose(
        g_player, g_camTarget[0], g_camTarget[1], g_camTarget[2], g_camYaw, false);
    if (!g_zoneCollisionTris.empty())
    {
        float floorY = 0.0f;
        float floorN[3] = {};
        if (ZoneCollision::FindFloorAt(g_zoneCollisionTris, g_zoneCollisionGrid,
                                       PlayerController::kCollisionRadius,
                                       g_player.position[0], g_player.position[2],
                                       g_player.position[1] - 40.0f,
                                       g_player.position[1] + 240.0f,
                                       &floorY, floorN))
        {
            g_player.position[1] = floorY;
            g_player.verticalVelocity = 0.0f;
            g_player.onGround = true;
        }
    }
    SetInteractionMode(InteractionController::Mode::Game);
    g_camDist = 10.0f;
    g_camPitch = -0.35f;
    PlayerController::SetRespawnPoint(g_player);

    SyncLowPolyControlsFromState();
}

static void ReloadPlayerModelFromControls()
{
    PullLowPolyStateFromControls();
    LoadPlayerRaceModel(g_playerEquip.raceIndex);
}

//========================================================================================
// Render
//========================================================================================

static void BeginCharacterSelectScene()
{
    // Reuse the authored character-creation zone and camera setup.
    g_creationSelection = {};
    g_creationSelection.equipmentIndex = 1;
    const FFXICreationEntry* creationEntry = CurrentHighPolyCreationEntry();
    LoadCreationEntry(creationEntry);
    UnloadCreationModel();
    g_titleAssets.LoadUi(GraphicsDevice(), g_ffxiPath, g_gameUiConfig.title.uiDat,
        g_applicationSettings.enableTextureCompression);
    g_savedCharacterPreviews.clear();
    char exeDir[MAX_PATH] = {}, pattern[MAX_PATH] = {};
    FFXIFileIO::GetExecutableDirectory(exeDir, sizeof(exeDir));
    sprintf_s(pattern, "%sCharacters\\*.ff11datset", exeDir);
    WIN32_FIND_DATAA file = {};
    HANDLE handle = FindFirstFileA(pattern, &file);
    if (handle != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            char path[MAX_PATH] = {};
            sprintf_s(path, "%sCharacters\\%s", exeDir, file.cFileName);
            SceneModelLoader::Result result = SceneModelLoader::LoadDatSet(
                GraphicsDevice(), path, g_applicationSettings.enableTextureCompression);
            if (!result.Succeeded()) continue;
            SavedCharacterPreview preview;
            preview.asset = std::move(result.asset);
            preview.name = file.cFileName;
            const size_t dot = preview.name.find_last_of('.');
            if (dot != std::string::npos) preview.name.erase(dot);
            g_savedCharacterPreviews.push_back(std::move(preview));
        } while (FindNextFileA(handle, &file));
        FindClose(handle);
    }
    if (g_savedCharacterPreviews.empty())
    {
        char message[MAX_PATH + 128] = {};
        sprintf_s(message, "No loadable character DAT sets were found in:\n%sCharacters\\\n\nSave a character there first, then try again.", exeDir);
        MessageBoxA(g_hWnd, message, "Select Character", MB_OK | MB_ICONINFORMATION);
    }
    g_selectedCharacterPreview = 0;
    g_titleScreenActive = false;
    g_nationSelectActive = false;
    g_highPolyCreationActive = false;
    g_characterSelectActive = true;
    InvalidateRect(g_hWnd, nullptr, FALSE);
}

static void BeginCharacterDeleteScene()
{
    BeginCharacterSelectScene();
    g_characterSelectActive = false;
    g_characterDeleteActive = true;
}

static void ActivateTitleButton(int buttonIndex)
{
    switch (buttonIndex)
    {
    case 1: // Create Character
        BeginHighPolyCreationScene();
        break;

    case 4: // Back
        ConfirmExitFromTitleScreen();
        break;

    case 0:
        BeginCharacterSelectScene();
        break;

    case 2:
        BeginCharacterDeleteScene();
        break;

    case 3:
        ConfigDialog::Show(g_configDialog);
        break;
    }
}

static void LoadSelectedNationScene()
{
    PullHighPolyCreationStateFromControls();
    HighPolyCreationPanel::Hide(g_highPolyCreationPanel);

    if (g_selectedNationIndex < 0 || g_selectedNationIndex >= FFXINationSelection::Count())
        g_selectedNationIndex = 0;

    const FFXINationSelection::Info& nation =
        FFXINationSelection::InfoForIndex(g_selectedNationIndex);
    const SceneRequestResolver::Result resolution =
        SceneRequestResolver::ResolveZone(g_ffxiPath, nation.zoneId);
    if (!resolution.Succeeded())
    {
        MessageBoxA(g_hWnd, "The selected nation zone has no model DAT on record and could not be resolved from FTABLE/VTABLE.",
                    "Nation Select", MB_OK | MB_ICONWARNING);
        return;
    }

    LoadDatFile(resolution.request);
    SetLoadedSceneContext(resolution.request);
    if (!g_zoneAsset)
        return;

    ApplyCreationStateToLowPolyPlayer();
    LoadPlayerRaceModel(g_playerEquip.raceIndex);

    const FFXIZoneMusicEntry *pMusic = FFXIZoneMusic_FindByID(nation.zoneId);
    if (pMusic)
        SetGameModeMusic(SelectZoneMusicId(pMusic));

    SyncLowPolyControlsFromState();
    if (g_hWnd)
        InvalidateRect(g_hWnd, NULL, FALSE);
}

static bool EnsureNationSelectAssets()
{
    return g_titleAssets.LoadUi(
        GraphicsDevice(), g_ffxiPath, g_gameUiConfig.title.uiDat,
        g_applicationSettings.enableTextureCompression);
}

static void DrawNationSelectTextures()
{
    if (!g_nationSelectActive || !g_hWnd)
        return;

    EnsureNationSelectAssets();
    const FFXINationSelectionRenderer::Context context =
    {
        GraphicsDevice(), g_hWnd, g_applicationSettings.enableMipMapping,
        g_titleAssets.UiModel(),
        g_gameUiConfig.nation, g_selectedNationIndex
    };
    FFXINationSelectionRenderer::DrawTextures(context);
}

static void DrawNationSelectOverlay(HDC overlayDc = nullptr)
{
    if (!g_nationSelectActive || !g_hWnd)
        return;

    const FFXINationSelectionRenderer::Context context =
    {
        GraphicsDevice(), g_hWnd, g_applicationSettings.enableMipMapping,
        g_titleAssets.UiModel(),
        g_gameUiConfig.nation, g_selectedNationIndex, overlayDc
    };
    FFXINationSelectionRenderer::DrawOverlay(context);
}

static void DrawTitleScreenTextures()
{
    if (!g_titleScreenActive || !g_hWnd)
        return;

    const FFXITitleScreenRenderer::Context context =
    {
        GraphicsDevice(), g_hWnd, g_applicationSettings.enableMipMapping,
        g_titleAssets.LogoModel(), g_titleAssets.AtlasModel(), g_titleAssets.UiModel(),
        g_gameUiConfig.title, g_input.clientMouse
    };
    FFXITitleScreenRenderer::DrawTextures(context);
}

static void DrawTitleScreenOverlay(HDC overlayDc = nullptr)
{
    if (!g_titleScreenActive || !g_hWnd)
        return;

    const FFXITitleScreenRenderer::Context context =
    {
        GraphicsDevice(), g_hWnd, g_applicationSettings.enableMipMapping,
        g_titleAssets.LogoModel(), g_titleAssets.AtlasModel(), g_titleAssets.UiModel(),
        g_gameUiConfig.title, g_input.clientMouse, overlayDc
    };
    FFXITitleScreenRenderer::DrawOverlay(context);
}

static RECT CharacterRosterBackRect(int width, int height);
static RECT CharacterRosterConfirmRect(int width, int height);

static void DrawCharacterRosterOverlay(HDC dc)
{
    if ((!g_characterSelectActive && !g_characterDeleteActive) || !dc) return;
    RECT panel = { 16, 70, 260, 360 };
    HBRUSH brush = CreateSolidBrush(g_characterDeleteActive ? RGB(55, 12, 18) : RGB(12, 16, 28));
    FillRect(dc, &panel, brush); DeleteObject(brush);
    HFONT font = CreateFontA(-18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, "Arial");
    for (size_t i = 0; i < g_savedCharacterPreviews.size(); ++i)
    {
        RECT row = { 28, 84 + (int)i * 30, 248, 110 + (int)i * 30 };
        if ((int)i == g_selectedCharacterPreview)
        {
            HBRUSH selected = CreateSolidBrush(RGB(65, 75, 135));
            FillRect(dc, &row, selected); DeleteObject(selected);
        }
        Win32Drawing::DrawShadowText(dc, font, g_savedCharacterPreviews[i].name.c_str(), row,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE, RGB(240, 240, 250), 1);
    }
    RECT hint = { 270, 690, 1240, 720 };
    Win32Drawing::DrawShadowText(dc, font,
        g_characterDeleteActive ? "Enter deletes the selected character. Backspace returns." :
        "Enter loads the selected character. Backspace returns.", hint,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(240, 240, 250), 1);
    RECT client{};
    GetClientRect(g_hWnd, &client);
    RECT back = CharacterRosterBackRect(client.right, client.bottom);
    Win32Drawing::DrawShadowText(dc, font, "Back", back,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(255, 255, 255), 1);
    RECT confirm = CharacterRosterConfirmRect(client.right, client.bottom);
    Win32Drawing::DrawShadowText(dc, font, "Confirm", confirm,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(255, 255, 255), 1);
    DeleteObject(font);
}

static RECT CharacterRosterBackRect(int width, int height)
{
    const int w = 180, h = 32;
    return { width - w * 2 - 42, height - 58, width - w - 42, height - 58 + h };
}
static RECT CharacterRosterConfirmRect(int width, int height)
{
    const int w = 180, h = 32;
    return { width - w - 28, height - 58, width - 28, height - 58 + h };
}

static void DrawCameraDebugOverlay(HDC overlayDc = nullptr)
{
    if (!g_cameraDebugOverlayVisible || !g_hWnd)
        return;

    float eyeX = 0.0f;
    float eyeY = 0.0f;
    float eyeZ = 0.0f;
    GetRenderCameraPosition(eyeX, eyeY, eyeZ);

    constexpr float kRadiansToDegrees = 57.29577951308232f;
    const FFXIZoneEntry* debugZone = FFXIZone::FindByID(g_transitionZoneId);
    const char* debugZoneName = debugZone ? debugZone->name : (g_loadedZoneLabel.empty() ? "(unknown)" : g_loadedZoneLabel.c_str());
    char text[768] = {};
    sprintf_s(text,
        "CAMERA DEBUG  [T to hide]\r\n"
        "Zone:   %s  (ID %d)\r\n"
        "Player: X % .3f   Y % .3f   Z % .3f\r\n"
        "Eye:    X % .3f   Y % .3f   Z % .3f\r\n"
        "Target: X % .3f   Y % .3f   Z % .3f\r\n"
        "Yaw:    % .4f rad  (% .2f deg)\r\n"
        "Pitch:  % .4f rad  (% .2f deg)\r\n"
        "Distance: %.3f\r\n"
        "Weather: %s",
        debugZoneName, g_transitionZoneId,
        g_player.position[0], g_player.position[1], g_player.position[2],
        eyeX, eyeY, eyeZ,
        g_camTarget[0], g_camTarget[1], g_camTarget[2],
        g_camYaw, g_camYaw * kRadiansToDegrees,
        g_camPitch, g_camPitch * kRadiansToDegrees,
        g_camDist,
        g_zoneEnvironment.weatherPath[0] ? g_zoneEnvironment.weatherPath : "(none)");

    const bool ownsDc = overlayDc == nullptr;
    HDC dc = ownsDc ? GetDC(g_hWnd) : overlayDc;
    if (!dc)
        return;

    const int savedDc = SaveDC(dc);
    HFONT font = CreateFontA(
        -15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");

    // Measure the complete multiline string instead of relying on a fixed
    // panel height. Extra pixels accommodate glyph descenders and the shadow.
    RECT textBounds = { 18, 16, 18, 16 };
    HFONT previousFont = static_cast<HFONT>(SelectObject(dc, font));
    DrawTextA(dc, text, -1, &textBounds,
        DT_LEFT | DT_TOP | DT_NOPREFIX | DT_CALCRECT);
    SelectObject(dc, previousFont);
    const std::wstring bitmapText(text, text + strlen(text));
    if (FFXIBitmapFont::Supports(bitmapText))
    {
        const SIZE extent = FFXIBitmapFont::Measure(bitmapText, 15);
        textBounds.right = textBounds.left + extent.cx;
        textBounds.bottom = textBounds.top + extent.cy;
    }
    textBounds.right += 2;
    textBounds.bottom += 2;

    RECT backgroundBounds =
    {
        10, 10, textBounds.right + 8, textBounds.bottom + 6
    };
    HBRUSH background = CreateSolidBrush(RGB(12, 16, 22));
    FillRect(dc, &backgroundBounds, background);
    DeleteObject(background);

    Win32Drawing::DrawShadowText(
        dc, font, text, textBounds, DT_LEFT | DT_TOP | DT_NOPREFIX,
        RGB(235, 240, 248), 1);
    DeleteObject(font);

    if (savedDc)
        RestoreDC(dc, savedDc);
    if (ownsDc)
        ReleaseDC(g_hWnd, dc);
}

static void DrawFfxiMainMenuOverlay(HDC overlayDc = nullptr)
{
    if (!overlayDc || !IsGameMode())
        return;
    RECT client = {};
    GetClientRect(g_hWnd, &client);
    const bool showMogHouse = g_loadedZoneLabel.find("Mog House") != std::string::npos;
    FFXIMainMenu::Draw(overlayDc, g_ffxiMainMenu,
                       client.right - client.left, client.bottom - client.top, showMogHouse);
}

static void DrawFfxiMainMenuTextures()
{
    if (!g_ffxiMainMenu.open || !IsGameMode() || !g_hWnd) return;
    int width = 0, height = 0;
    D3D9Device::GetViewportOrClientSize(GraphicsDevice(), g_hWnd, &width, &height);
    if (!g_titleAssets.HasUi())
        g_titleAssets.LoadUi(GraphicsDevice(), g_ffxiPath, g_gameUiConfig.title.uiDat,
            g_applicationSettings.enableTextureCompression);
    const bool showMogHouse = g_loadedZoneLabel.find("Mog House") != std::string::npos;
    FFXIMainMenu::DrawTextures(GraphicsDevice(), g_applicationSettings.enableMipMapping,
        g_titleAssets.UiModel(), g_ffxiMainMenu, width, height, showMogHouse);
}

static void DrawDeveloperConsoleOverlay(HDC dc)
{
    if (!g_developerConsoleOpen || !dc) return;
    RECT panel = { 12, 12, 820, 360 };
    HBRUSH brush = CreateSolidBrush(RGB(10, 12, 18));
    FillRect(dc, &panel, brush); DeleteObject(brush);
    HFONT font = CreateFontA(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_MODERN, "Consolas");
    const int lineHeight = 19;
    const int visibleLines = 15;
    const int end = (int)g_developerConsoleLog.size() - g_developerConsoleScroll;
    const int start = std::max(0, end - visibleLines);
    for (int i = start; i < end; ++i)
    {
        RECT line = { 24, 24 + (i - start) * lineHeight, 808, 24 + (i - start + 1) * lineHeight };
        Win32Drawing::DrawShadowText(dc, font, g_developerConsoleLog[i].c_str(), line,
            DT_LEFT | DT_SINGLELINE, RGB(220, 230, 240), 1);
    }
    RECT line = { 24, 320, 808, 348 };
    std::string input = "> " + g_developerConsoleInput;
    Win32Drawing::DrawShadowText(dc, font, input.c_str(), line,
        DT_LEFT | DT_SINGLELINE, RGB(255, 220, 120), 1);
    DeleteObject(font);
}

static float BoundsDerivedZoneFarPlane(const noesisModel_t* model,
                                       const float cameraX, const float cameraY,
                                       const float cameraZ)
{
    float farthestDistanceSq = 0.0f;
    bool foundBounds = false;
    if (model)
    {
        for (const noesisModel_t::Submesh& submesh : model->submeshes)
        {
            if (!submesh.hasBounds || submesh.environmentObject)
                continue;
            const float dx = std::max(
                fabsf(submesh.boundsMin[0] - cameraX),
                fabsf(submesh.boundsMax[0] - cameraX));
            const float dy = std::max(
                fabsf(submesh.boundsMin[1] - cameraY),
                fabsf(submesh.boundsMax[1] - cameraY));
            const float dz = std::max(
                fabsf(submesh.boundsMin[2] - cameraZ),
                fabsf(submesh.boundsMax[2] - cameraZ));
            const float distanceSq = dx * dx + dy * dy + dz * dz;
            if (std::isfinite(distanceSq))
            {
                farthestDistanceSq = std::max(farthestDistanceSq, distanceSq);
                foundBounds = true;
            }
        }
    }

    // The margin covers transformed edges and avoids clipping exactly on an
    // AABB corner. The fallback is used only for models without valid bounds.
    return foundBounds ? std::max(1024.0f, sqrtf(farthestDistanceSq) * 1.05f + 64.0f)
                       : 32768.0f;
}

static void Render()
{
    if (!g_graphicsRuntime.IsInitialized())
        return;

    if (g_graphicsRuntime.PrepareFrame() != D3D9Device::FrameStatus::Ready)
    {
        Sleep(10);
        return;
    }

    UpdateZoneEnvironmentState();

    noesisModel_t* const zoneModel = g_zoneAsset.Model();
    noesisModel_t* const creationModel = g_creationAsset.Model();
    noesisModel_t* const playerModel = g_playerAsset.Model();

    // Clear back buffer and depth/stencil. Outdoor zone DATs author this color
    // alongside their sky and fog; non-zone screens keep DATura's neutral fill.
    const DWORD clearColor = (zoneModel && g_zoneEnvironment.valid) ?
        g_zoneEnvironment.clearColor : D3DCOLOR_XRGB(25, 32, 38);
    GraphicsDevice()->Clear(0, NULL,
        D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
        clearColor,
        1.0f, 0);

    g_npcNameplates.clear();
    g_npcInteraction.targets.clear();
    g_zoneRuntimeOverrides = g_zoneObjectOverrides;
    for (const auto& entry : g_doors.transforms)
        g_zoneRuntimeOverrides.insert(entry);
    if (SUCCEEDED(GraphicsDevice()->BeginScene()))
    {
        // ---- Camera setup ----
        if (g_characterSelectActive || g_characterDeleteActive)
        {
            g_camTarget[0] = 0.0f;
            // Aim at the character's torso rather than the ground plane so
            // the roster framing does not leave excessive space below them.
            // Raise the target slightly to better center taller characters
            // in the preview (previously 1.0f was too low).
            g_camTarget[1] = -1.6f; // Camera position height <-------------------------------------------------------------------------------------
            g_camTarget[2] = 0.0f;
            // Character previews face along the X axis, so place the camera
            // on that axis to show their fronts instead of their profiles.
            g_camYaw = 3.14159265f * 0.5f;
            g_camPitch = -0.10f;
            // Keep the previews large enough to inspect while allowing a
            // wider lineup to fit as more saved characters are added.
            g_camDist = std::max(6.0f,
                4.0f + (float)g_savedCharacterPreviews.size() * 1.0f);
        }
        // Orbit camera position is derived by the camera module.
        float renderCameraEye[3] = {};
        GetRenderCameraPosition(renderCameraEye[0], renderCameraEye[1],
                                renderCameraEye[2]);

        float renderCameraTarget[3] = {
            g_camTarget[0], g_camTarget[1], g_camTarget[2]
        };
        float creationCameraFov = 3.14159265f * 0.25f;
        const bool useCreationCamera =
            g_highPolyCreationActive && g_creationAnimationIndex == 2 &&
            g_creationAnimatedCamera &&
            FFXICreationCamera::Sample(g_creationCameraTrack, g_creationAnimTime,
                                       renderCameraEye, renderCameraTarget,
                                       &creationCameraFov);
        if (useCreationCamera)
        {
            // The actor is placed only in X/Z. Apply that same horizontal
            // displacement to the authored camera and leave altitude intact.
            renderCameraEye[0] += g_creationHorizontalPlacement[0];
            renderCameraEye[2] += g_creationHorizontalPlacement[2];
            renderCameraTarget[0] += g_creationHorizontalPlacement[0];
            renderCameraTarget[2] += g_creationHorizontalPlacement[2];
        }

        D3DMATRIX view = D3DMath::BuildLookAtLH(
            renderCameraEye[0], renderCameraEye[1], renderCameraEye[2],
            renderCameraTarget[0], renderCameraTarget[1], renderCameraTarget[2]);

        int renderW = 0;
        int renderH = 0;
        D3D9Device::GetViewportOrClientSize(GraphicsDevice(), g_hWnd, &renderW, &renderH);
        const float w = (float)renderW;
        const float h2 = (float)renderH;
        const float aspect = (h2 > 0.0f) ? (w / h2) : (16.0f / 9.0f);

        const float configuredDrawDistance =
            ApplicationSettings::DrawDistanceOptionValue(
                g_applicationSettings.drawDistanceIndex);
        const bool unlimitedDrawDistance = g_titleScreenActive ||
            ApplicationSettings::DrawDistanceOptionIsUnlimited(
                g_applicationSettings.drawDistanceIndex);
        const float effectiveDrawDistance = EffectivePlayDrawDistance(
            configuredDrawDistance, unlimitedDrawDistance, IsGameMode());
        const float terrainProjectionDistance = unlimitedDrawDistance ?
            BoundsDerivedZoneFarPlane(zoneModel, renderCameraEye[0],
                                      renderCameraEye[1], renderCameraEye[2]) :
            ((zoneModel && g_zoneEnvironment.valid) ?
                std::min(effectiveDrawDistance, g_zoneEnvironment.drawDistance) :
                effectiveDrawDistance);
        const float nearPlane = zoneModel ? D3DMath::ZoneNearPlane : 0.01f;
        D3DMATRIX proj = D3DMath::BuildPerspectiveFovLH(
            useCreationCamera ? creationCameraFov : 3.14159265f * 0.25f,
			aspect, nearPlane, terrainProjectionDistance);
		const float skyProjectionDistance = (zoneModel && g_zoneEnvironment.valid) ?
			std::max(terrainProjectionDistance, g_zoneEnvironment.skyRadius * 1.125f) :
			terrainProjectionDistance;
        D3DMATRIX skyProj = D3DMath::BuildPerspectiveFovLH(
            3.14159265f * 0.25f, aspect, nearPlane, skyProjectionDistance);
        g_pickView = view; g_pickProjection = proj;
        g_pickWidth = static_cast<int>(w); g_pickHeight = static_cast<int>(h2);
        const D3DMATRIX viewProjection = D3DMath::Multiply(view, proj);
        ZoneRenderFrustum::Build(g_zoneRenderFrustum, view, proj);
        // Retail selects the active cell from the player, not the third-person
        // eye. In DATura the orbit target is the equivalent position; the eye
        // can be high above and outside every finite cell volume.
        FFXICoordinateFrame::SceneToNativeDat(renderCameraTarget,
            !g_applicationSettings.mirrorWorldZones, g_zoneVisibilityViewerPoint);
        g_zoneVisibilityViewerPointValid = Model_FF11_HasZoneVisibilityData();
        g_zoneVisibleMapObjects.clear();
        if (g_zoneVisibilityViewerPointValid)
        {
            std::vector<unsigned int> visibleMapObjects;
            if (Model_FF11_GetZoneVisibleMapObjects(g_zoneVisibilityViewerPoint, visibleMapObjects))
            {
                g_zoneVisibleMapObjects.resize(gFF11LastZoneVisibilityRecords.size(), 0);
                for (unsigned int mapObjectIndex : visibleMapObjects)
                {
                    if (mapObjectIndex < g_zoneVisibleMapObjects.size())
                        g_zoneVisibleMapObjects[(size_t)mapObjectIndex] = 1;
                }
            }
        }
		GraphicsDevice()->SetTransform(D3DTS_VIEW,       &view);
		GraphicsDevice()->SetTransform(D3DTS_PROJECTION, zoneModel ? &skyProj : &proj);

        D3DMATRIX world = D3DMath::BuildIdentity();
        GraphicsDevice()->SetTransform(D3DTS_WORLD,      &world);

        if (zoneModel)
		{
			// The 0x2F draw distance controls terrain/fog, but authored cloud shells
			// and the dome live near skyBoxRadius (~2030). Give sky its own far plane,
			// then restore the terrain projection for the world and NPC passes.
			ZoneSkyDome::Parameters sky;
			sky.environmentValid = g_zoneEnvironment.valid;
			sky.indoor = g_zoneEnvironment.indoor;
			sky.radius = g_zoneEnvironment.skyRadius;
			sky.spokeCount = g_zoneEnvironment.sphereSpokeCount;
			sky.ringCount = g_zoneEnvironment.ringCount;
			sky.ringElevations = g_zoneEnvironment.ringElevations;
			sky.ringColors = g_zoneEnvironment.ringColors;
			ZoneSkyDome::Draw(GraphicsDevice(), sky, renderCameraEye[0], renderCameraEye[1],
                                renderCameraEye[2], skyProjectionDistance);
			ZoneEnvironmentRenderState::DrawCameraShells(
				GraphicsDevice(), zoneModel, g_applicationSettings.enableMipMapping,
                g_zoneEnvironment.weatherPath,
				gFF11LastGeneratorRecords, gFF11LastKeyframeRecords,
                                renderCameraEye[0], renderCameraEye[1], renderCameraEye[2]);
			GraphicsDevice()->SetTransform(D3DTS_PROJECTION, &proj);
			GraphicsDevice()->SetTransform(D3DTS_WORLD, &world);
            RenderModel(zoneModel, true, ModelRenderer::GeometryPass::Opaque);
        }

        if (creationModel)
        {
            D3DMATRIX creationWorld = world;
            float creationRootMotion[3] = {};
            FFXISqleModelAnimation::SampleRootMotion(
                creationModel, g_creationAnimTime, creationRootMotion);
            creationWorld._41 += g_creationHorizontalPlacement[0] + creationRootMotion[0];
            creationWorld._42 = creationRootMotion[1];
            creationWorld._43 += g_creationHorizontalPlacement[2] + creationRootMotion[2];
            GraphicsDevice()->SetTransform(D3DTS_PROJECTION, &proj);
            GraphicsDevice()->SetTransform(D3DTS_WORLD, &creationWorld);
            RenderModel(creationModel);
            GraphicsDevice()->SetTransform(D3DTS_WORLD, &world);
        }

        if (g_characterSelectActive || g_characterDeleteActive)
        {
            const float spacing = 4.0f;
            const float center = (float)(g_savedCharacterPreviews.size() - 1) * spacing * 0.5f;
            for (size_t i = 0; i < g_savedCharacterPreviews.size(); ++i)
            {
                if (!g_savedCharacterPreviews[i].asset) continue;
                D3DMATRIX previewWorld = world;
                previewWorld._41 += (float)i * spacing - center;
                GraphicsDevice()->SetTransform(D3DTS_WORLD, &previewWorld);
                RenderModel(g_savedCharacterPreviews[i].asset.Model());
            }
            GraphicsDevice()->SetTransform(D3DTS_WORLD, &world);
            noesisTex_t* rosterButton = FFXITitleAssets::FindTitleTexture(
                g_titleAssets.UiModel(), "buttonto");
            if (rosterButton && rosterButton->pD3DTex)
            {
                POINT mouse = D3D9Device::MapClientPointToViewport(
                    g_hWnd, (int)w, (int)h2, g_input.clientMouse);
                const RECT button = CharacterRosterBackRect((int)w, (int)h2);
                D3DUiRenderer::DrawSolidQuad(GraphicsDevice(),
                    (float)button.left, (float)button.top,
                    (float)(button.right - button.left),
                    (float)(button.bottom - button.top), 0xFF111522);
                FFXITitleUiPrimitives::DrawButton(GraphicsDevice(), g_applicationSettings.enableMipMapping,
                    rosterButton, (float)button.left, (float)button.top,
                    (float)(button.right - button.left), (float)(button.bottom - button.top),
                    PtInRect(&button, mouse) != FALSE);
                const RECT confirm = CharacterRosterConfirmRect((int)w, (int)h2);
                D3DUiRenderer::DrawSolidQuad(GraphicsDevice(),
                    (float)confirm.left, (float)confirm.top,
                    (float)(confirm.right - confirm.left),
                    (float)(confirm.bottom - confirm.top), 0xFF111522);
                FFXITitleUiPrimitives::DrawButton(GraphicsDevice(), g_applicationSettings.enableMipMapping,
                    rosterButton, (float)confirm.left, (float)confirm.top,
                    (float)(confirm.right - confirm.left), (float)(confirm.bottom - confirm.top),
                    PtInRect(&confirm, mouse) != FALSE);
            }
        }

        for (NpcRenderAsset &asset : g_npcRenderAssets)
            asset.visibleLastFrame = false;

        std::vector<HomePoint::Instance> homePoints;
        std::vector<const NpcRenderInstance*> visibleHomePoints;
        for (const auto& npc : g_npcRenderInstances)
            if (npc.placement.visible && npc.assetIndex < g_npcRenderAssets.size() &&
                g_npcRenderAssets[npc.assetIndex].homePoint)
                homePoints.push_back({npc.placement.entityId,
                    {npc.placement.transform.x, npc.placement.transform.y, npc.placement.transform.z},
                    npc.homePointActivatedAt});
        const float listenerRight[3] = {view._11, view._21, view._31};
        const HWND soundForeground = GetForegroundWindow();
        const bool homePointAudioAllowed = IsGameMode() && !g_titleScreenActive && !g_nationSelectActive &&
            g_applicationSettings.enableSounds &&
            (g_applicationSettings.playSoundsInBackground || soundForeground == g_hWnd ||
             soundForeground == ConfigDialog::Window(g_configDialog) || AudioPlayer_OwnsWindow(soundForeground));
        for (auto& asset : g_npcRenderAssets)
            if (asset.homePoint)
                asset.homePoint->UpdateSound(homePoints, g_player.position, listenerRight,
                    homePointAudioAllowed, g_applicationSettings.maxSimultaneousSounds);

        for (const NpcRenderInstance &npc : g_npcRenderInstances)
        {
            if (npc.assetIndex >= g_npcRenderAssets.size() || !npc.placement.visible)
                continue;
            NpcRenderAsset &asset = g_npcRenderAssets[npc.assetIndex];
            if (!asset.resource && !asset.homePoint)
                continue;
            FFXINpcPlacement::Transform transform = npc.placement.transform;
            // Catalog headings and character DATs both face +X at zero. The
            // player's camera-relative quarter-turn does not apply to NPCs.
            D3DMATRIX npcWorld = FFXINpcPlacement::BuildWorldTransform(transform);

            NpcNameplateRenderer::DrawItem nameplate;
            float worldX = 0.0f, worldY = 0.0f, worldZ = 0.0f;
            if (!ProjectNpcNameplate(npcWorld, asset.nameplateLocalY, viewProjection,
                                     w, h2, &nameplate.screenX, &nameplate.screenY,
                                     &nameplate.depth, &worldX, &worldY, &worldZ))
                continue;

            // The zone DAT's spatial tree uses the map geometry's native X
            // orientation. Normal viewer mode mirrors that geometry to match
            // server/entity coordinates, so transform both query points back.
            const float cameraVisibilityPoint[3] =
            {
                g_zoneVisibilityViewerPoint[0],
                g_zoneVisibilityViewerPoint[1],
                g_zoneVisibilityViewerPoint[2]
            };
            const auto npcVisibilityPoint = FFXICoordinateFrame::SceneToNativeDat(
                {transform.x, transform.y, transform.z}, !g_applicationSettings.mirrorWorldZones);
            if (!Model_FF11_IsZonePointVisible(cameraVisibilityPoint, npcVisibilityPoint.data()))
                continue;

            if (IsGameMode() && !g_titleScreenActive && !g_nationSelectActive)
            {
                float feetX, feetY, feetDepth, unusedX, unusedY, unusedZ;
                if (ProjectNpcNameplate(npcWorld, 0.0f, viewProjection, w, h2,
                    &feetX, &feetY, &feetDepth, &unusedX, &unusedY, &unusedZ))
                {
                    const float radius = (std::max)(12.0f, std::fabs(feetY - nameplate.screenY) * 0.3f);
                    g_npcInteraction.targets.push_back({ npc.placement.entityId,
                        (std::min)(feetX, nameplate.screenX) - radius,
                        (std::min)(feetY, nameplate.screenY) - 20.0f,
                        (std::max)(feetX, nameplate.screenX) + radius,
                        (std::max)(feetY, nameplate.screenY) + 4.0f, nameplate.depth });
                }
            }

            asset.visibleLastFrame = true;
            GraphicsDevice()->SetTransform(D3DTS_WORLD, &npcWorld);
            if (asset.homePoint)
                visibleHomePoints.push_back(&npc);
            else
            {
                // A planar shadow redraws every opaque submesh. Keep the playable
                // character shadow, but avoid multiplying that full extra pass by
                // every NPC in a populated zone.
                RenderModel(asset.resource.Model(), false, ModelRenderer::GeometryPass::All, false);
            }

            if (!npc.placement.name.empty() &&
                !FFXIPath::StringEqualsNoCase(npc.placement.name.c_str(), "blank"))
            {
                nameplate.name = npc.placement.name;
                nameplate.roleTitle = npc.placement.roleTitle;
                nameplate.starterQuestAvailable = npc.placement.starterQuestAvailable;
                if (IsGameMode() && g_npcInteraction.selected == npc.placement.entityId)
                    nameplate.name = "> " + nameplate.name + " <";
                const float dx = worldX - renderCameraEye[0];
                const float dy = worldY - renderCameraEye[1];
                const float dz = worldZ - renderCameraEye[2];
                nameplate.distanceSquared = dx * dx + dy * dy + dz * dz;
                g_npcNameplates.push_back(std::move(nameplate));
            }
        }
        GraphicsDevice()->SetTransform(D3DTS_WORLD, &world);

        if (playerModel)
        {
            // Character DAT forward is offset from DATura's movement/camera convention by 90 degrees.
            D3DMATRIX playerWorld = D3DMath::BuildYawTranslation(
                g_player.yaw + (3.1415926535f * 0.5f), g_player.position);
            GraphicsDevice()->SetTransform(D3DTS_WORLD, &playerWorld);
            RenderModel(playerModel);
            GraphicsDevice()->SetTransform(D3DTS_WORLD, &world);

            // Use the posed mesh after animation to keep the label above the
            // current race/equipment, with the same camera-distance scaling as NPCs.
            NpcNameplateRenderer::DrawItem playerLabel;
            const float playerNameplateY =
                NpcRenderGeometry::ComputeNameplateLocalY(playerModel);
            float labelX, labelY, labelZ;
            const auto& appearance = g_gameUiConfig.playerNameplate;
            if (appearance.enabled && ProjectNpcNameplate(playerWorld, playerNameplateY, viewProjection,
                w, h2, &playerLabel.screenX, &playerLabel.screenY, &playerLabel.depth,
                &labelX, &labelY, &labelZ))
            {
                playerLabel.name = appearance.name[0] ? appearance.name : g_creationCharacterName;
                if (playerLabel.name.empty()) playerLabel.name = "Adventurer";
                playerLabel.nameColor = 0xFFFFFFFF;
                playerLabel.icon = appearance.icon;
                playerLabel.jobMaster = appearance.jobMaster;
                playerLabel.roleTitle = GameUiConfig_PlayerSubtitle(appearance);
                playerLabel.iconColor = D3DCOLOR_XRGB(GetRValue(appearance.linkshellColor),
                    GetGValue(appearance.linkshellColor), GetBValue(appearance.linkshellColor));
                const float dx = labelX - renderCameraEye[0];
                const float dy = labelY - renderCameraEye[1];
                const float dz = labelZ - renderCameraEye[2];
                playerLabel.distanceSquared = dx * dx + dy * dy + dz * dz;
                g_npcNameplates.push_back(std::move(playerLabel));
            }
        }

        // Water and other translucent zone surfaces blend after actors have
        // populated depth and color, preserving submerged/foreground occlusion.
        if (zoneModel)
        {
            GraphicsDevice()->SetTransform(D3DTS_WORLD, &world);
            RenderModel(zoneModel, true, ModelRenderer::GeometryPass::Transparent);
            ZoneObjectHighlightRenderer::Draw(
                GraphicsDevice(), zoneModel, g_highlightedZoneObject, g_zoneRuntimeOverrides);
        }
        // Crystal layers blend against completed actor/terrain depth. Rendering
        // them in the opaque NPC loop would let a later actor erase the aura.
        std::sort(visibleHomePoints.begin(), visibleHomePoints.end(), [&](const auto* a, const auto* b)
        {
            const auto distance = [&](const auto* npc)
            {
                const auto& t = npc->placement.transform;
                const float dx = t.x - renderCameraEye[0], dy = t.y - renderCameraEye[1], dz = t.z - renderCameraEye[2];
                return dx*dx + dy*dy + dz*dz;
            };
            return distance(a) > distance(b);
        });
        GraphicsDevice()->SetTransform(D3DTS_PROJECTION, &proj);
        for (const auto* npc : visibleHomePoints)
        {
            const auto npcWorld = FFXINpcPlacement::BuildWorldTransform(npc->placement.transform);
            g_npcRenderAssets[npc->assetIndex].homePoint->Draw(GraphicsDevice(), npcWorld,
                renderCameraEye, g_homePointSeconds, npc->homePointActivatedAt,
                g_applicationSettings.enableMipMapping);
        }
        if (g_applicationSettings.showCollisionGeometry)
            ZoneCollision::DrawOverlay(GraphicsDevice(), g_zoneCollisionMesh);

        // Precipitation blends against the completed scene and depth-tests against
        // actors as well as terrain. Drawing it before actors erases foreground drops.
        if (zoneModel)
        {
            ZoneWeatherParticles::Draw(
                GraphicsDevice(), zoneModel, g_zoneEnvironment.valid,
                g_applicationSettings.enableMipMapping, g_zoneEnvironment.weatherPath,
                gFF11LastGeneratorRecords, gFF11LastKeyframeRecords,
                renderCameraEye[0], renderCameraEye[1], renderCameraEye[2]);
            GraphicsDevice()->SetTransform(D3DTS_WORLD, &world);
        }

        if (IsGameMode() && g_doors.selected >= 0 && g_doors.selected < (int)g_doors.doors.size())
        {
            const auto& door = g_doors.doors[g_doors.selected];
            auto marker = D3DMath::BuildIdentity();
            marker._41 = door.center[0]; marker._42 = door.minY - 0.25f; marker._43 = door.center[2];
            NpcNameplateRenderer::DrawItem label;
            float wx, wy, wz;
            if (ProjectNpcNameplate(marker, 0, viewProjection, w, h2,
                &label.screenX, &label.screenY, &label.depth, &wx, &wy, &wz))
            {
                label.name = "> Door <";
                const float dx = door.center[0] - g_player.position[0];
                const float dz = door.center[2] - g_player.position[2];
                label.roleTitle = g_doors.physics ? "Walk into door to push" : door.targetAngle != 0 ? "Open" :
                    (door.angle != 0 ? "Closing" :
                    (dx*dx + dz*dz > 36 ? "Move closer to open" : "Click again to open"));
                label.distanceSquared = dx*dx + dz*dz;
                g_npcNameplates.push_back(std::move(label));
            }
        }
        if (!g_titleScreenActive && !g_nationSelectActive)
            NpcNameplateRenderer::Draw(GraphicsDevice(), g_npcNameplates);
        DrawTitleScreenTextures();
        DrawNationSelectTextures();
        DrawFfxiMainMenuTextures();
        DrawZoneBoundaryDots(viewProjection, w, h2);
        DrawZoneTransitionOverlay();

        GraphicsDevice()->EndScene();
    }

    // The title/nation labels are GDI-rendered.  Paint them into the
    // lockable backbuffer before Present so they cannot flicker independently
    // of the Direct3D title artwork on the window surface.
    IDirect3DSurface9* backBuffer = nullptr;
    HDC overlayDc = nullptr;
    if (SUCCEEDED(GraphicsDevice()->GetBackBuffer(
            0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)) && backBuffer)
    {
        backBuffer->GetDC(&overlayDc);
        if (overlayDc)
        {
            DrawTitleScreenOverlay(overlayDc);
            DrawCharacterRosterOverlay(overlayDc);
            DrawNationSelectOverlay(overlayDc);
            DrawCameraDebugOverlay(overlayDc);
            DrawFfxiMainMenuOverlay(overlayDc);
            DrawDeveloperConsoleOverlay(overlayDc);
            backBuffer->ReleaseDC(overlayDc);
        }
        backBuffer->Release();
    }

    NpcChatWindow::Draw(GraphicsDevice());
    g_graphicsRuntime.Present();
}

//========================================================================================
// File open helpers
//========================================================================================

//========================================================================================
// Menu
//========================================================================================

static void LoadStandaloneModelEntry(const FFXIStandaloneModelEntry *entry, const char *kind)
{
    if (!entry)
        return;

    const SceneRequestResolver::Result resolution =
        SceneRequestResolver::ResolveRelativeModel(g_ffxiPath, entry->dat, entry->label);
    if (!resolution.Succeeded())
    {
        char message[256] = {};
        sprintf_s(message, "The selected %s DAT was not found under the configured FFXI path.",
                  kind ? kind : "model");
        MessageBoxA(g_hWnd, message, "Model Missing", MB_OK | MB_ICONWARNING);
        return;
    }

    UnloadPlayerModel();
    LoadDatFile(resolution.request);
    if (!g_zoneAsset)
        return;

    SetLoadedSceneContext(resolution.request);
    SetGameModeMusic(0);
    ResetCameraForStandaloneModel(g_zoneAsset.Model());
    InvalidateRect(g_hWnd, NULL, FALSE);
}

static void LoadCompanionModel(void *, const char *label, const char *datPath)
{
    const FFXIStandaloneModelEntry entry = { label, datPath };
    LoadStandaloneModelEntry(&entry, "companion model");
}

static void ShowCompanionBrowser()
{
    FFXICompanionBrowser::Show(g_hWnd, g_ffxiPath, LoadCompanionModel);
}

static void RefreshMainMenuTheme()
{
    ApplicationMenu::RefreshTheme(g_applicationMenu);
}

static bool HandleApplicationMenuCommand(
    const ApplicationMenu::Command& command,
    const HWND window)
{
    switch (command.selectionType)
    {
    case ApplicationMenu::SelectionType::NpcModel:
        LoadStandaloneModelEntry(
            FFXIStandaloneModel_GetEntry(
                kFFXINpcModelGroups, kFFXINpcModelGroupCount, command.selectionIndex),
            "NPC model");
        return true;

    case ApplicationMenu::SelectionType::MonsterModel:
        LoadStandaloneModelEntry(
            FFXIStandaloneModel_GetEntry(
                kFFXIMonsterModelGroups, kFFXIMonsterModelGroupCount, command.selectionIndex),
            "monster model");
        return true;

    case ApplicationMenu::SelectionType::PrototypeArea:
        if (command.selectionIndex >= 0 && command.selectionIndex < kFFXIPrototypeAreaCount)
        {
            const FFXIPrototypeAreaEntry& area = kFFXIPrototypeAreas[command.selectionIndex];
            const SceneRequestResolver::Result resolution =
                SceneRequestResolver::ResolveRelativeModel(
                    g_ffxiPath, area.modelDat, area.name);
            if (resolution.Succeeded())
            {
                LoadDatFile(resolution.request);
                SetLoadedSceneContext(resolution.request);
                SetGameModeMusic(0);
            }
            else
            {
                MessageBoxA(g_hWnd,
                    "This prototype DAT was not found under the configured FFXI path.",
                    "Prototype Area Missing", MB_OK | MB_ICONWARNING);
            }
        }
        return true;

    case ApplicationMenu::SelectionType::Zone:
        {
            const int zoneId = command.selectionIndex;
            const SceneRequestResolver::Result resolution =
                SceneRequestResolver::ResolveZone(g_ffxiPath, zoneId);
            if (resolution.Succeeded())
            {
                LoadDatFile(resolution.request);
                SetLoadedSceneContext(resolution.request);
                const FFXIZoneMusicEntry* music = FFXIZoneMusic_FindByID(zoneId);
                SetGameModeMusic(music ? SelectZoneMusicId(music) : 0);
            }
            else
            {
                MessageBoxA(g_hWnd,
                    "This zone has no model DAT on record and could not be resolved from FTABLE/VTABLE.",
                    "No Model", MB_OK | MB_ICONWARNING);
            }
        }
        return true;

    case ApplicationMenu::SelectionType::PlayerRace:
        LoadPlayerRaceModel(command.selectionIndex);
        ShowLowPolyControlPanel();
        return true;

    case ApplicationMenu::SelectionType::CreationModel:
        if (SetHighPolyCreationSelectionFromFlatIndex(command.selectionIndex))
        {
            LowPolyCharacterPanel::Hide(g_lowPolyPanel);
            const FFXICreationEntry* entry = CurrentHighPolyCreationEntry();
            if (entry)
                LoadCreationEntry(entry);
            ShowHighPolyCreationPanel();
        }
        return true;

    case ApplicationMenu::SelectionType::None:
        break;
    }

    switch (command.action)
    {
    case ApplicationMenu::Action::OpenDat:
        {
            char path[MAX_PATH] = {};
            if (FFXIFileIO::BrowseForDatFile(
                    g_hWnd, g_ffxiPath, path, sizeof(path), "Open FFXI DAT"))
                LoadDatFile(path);
        }
        return true;
    case ApplicationMenu::Action::OpenDatSet:
        {
            char path[MAX_PATH] = {};
            if (FFXIFileIO::BrowseForDatSetFile(g_hWnd, g_ffxiPath, path, sizeof(path)))
                LoadDatSetFile(path);
        }
        return true;
    case ApplicationMenu::Action::ReturnToTitle:
        ReturnToTitleScreen();
        return true;
    case ApplicationMenu::Action::Exit:
        PostQuitMessage(0);
        return true;
    case ApplicationMenu::Action::ShowConfig:
        ConfigDialog::Show(g_configDialog);
        return true;
    case ApplicationMenu::Action::SetPath:
        PromptSetFFXIPath();
        ConfigDialog::Sync(g_configDialog);
        return true;
    case ApplicationMenu::Action::DetectPath:
        AutoDetectFFXIPath();
        ConfigDialog::Sync(g_configDialog);
        return true;
    case ApplicationMenu::Action::ResetPath:
        ResetFFXIPathToDefault();
        ConfigDialog::Sync(g_configDialog);
        return true;
    case ApplicationMenu::Action::ShowPath:
        ShowCurrentFFXIPathDialog();
        return true;
    case ApplicationMenu::Action::ToggleMipMapping:
        g_applicationSettings.enableMipMapping = !g_applicationSettings.enableMipMapping;
        SyncSettingsMenuChecks();
        ConfigDialog::Sync(g_configDialog);
        InvalidateRect(window, NULL, FALSE);
        return true;
    case ApplicationMenu::Action::ToggleBumpMapping:
        g_applicationSettings.enableBumpMapping = !g_applicationSettings.enableBumpMapping;
        SyncSettingsMenuChecks();
        ConfigDialog::Sync(g_configDialog);
        InvalidateRect(window, NULL, FALSE);
        return true;
    case ApplicationMenu::Action::CycleEnvironmentalAnimation:
        g_applicationSettings.environmentalAnimationMode =
            ApplicationSettings::ClampEnvironmentalAnimationMode(
                (g_applicationSettings.environmentalAnimationMode + 1) % 3);
        SyncSettingsMenuChecks();
        ConfigDialog::Sync(g_configDialog);
        InvalidateRect(window, NULL, FALSE);
        return true;
    case ApplicationMenu::Action::ToggleMirrorWorld:
        g_applicationSettings.mirrorWorldZones = !g_applicationSettings.mirrorWorldZones;
        SyncSettingsMenuChecks();
        ConfigDialog::Sync(g_configDialog);
        ReloadRememberedZone();
        InvalidateRect(window, NULL, FALSE);
        return true;
    case ApplicationMenu::Action::ToggleGameMode:
        ToggleEditGameMode();
        ConfigDialog::Sync(g_configDialog);
        return true;
    case ApplicationMenu::Action::ShowZoneObjects:
        ShowZoneObjectPanel();
        return true;
    case ApplicationMenu::Action::CycleWeather:
        ++g_zoneWeatherIndex;
        UpdateZoneEnvironmentState();
        InvalidateRect(window, NULL, FALSE);
        return true;
    case ApplicationMenu::Action::ShowCurrentZoneResources:
        ShowCurrentZoneResourceBrowser();
        return true;
    case ApplicationMenu::Action::ShowZoneGeometryDiagnostics:
        ShowZoneGeometryDiagnostics();
        return true;
    case ApplicationMenu::Action::ShowDatReplacements:
        DatReplacementDialog::Show(g_hWnd);
        return true;
    case ApplicationMenu::Action::OpenResourceDat:
        ShowResourceDatBrowser();
        return true;
    case ApplicationMenu::Action::ShowTextureViewer:
        TextureViewer_Show(g_hWnd, g_ffxiPath);
        return true;
    case ApplicationMenu::Action::ShowCompanionBrowser:
        ShowCompanionBrowser();
        return true;
    case ApplicationMenu::Action::ShowAudioPlayer:
        AudioPlayer_Show(g_hWnd, g_ffxiPath);
        SyncAppMusic();
        return true;
    case ApplicationMenu::Action::StopAudio:
        AudioPlayer_StopPlayback();
        return true;
    case ApplicationMenu::Action::CustomizePlayer:
        ShowLowPolyControlPanel();
        if (!g_playerAsset)
            ReloadPlayerModelFromControls();
        return true;
    case ApplicationMenu::Action::None:
        return false;
    }
    return false;
}

static void HandleInputAction(const InputController::Action action, HWND window)
{
    switch (action)
    {
    case InputController::Action::Back:
        if (g_characterSelectActive || g_characterDeleteActive)
        {
            g_characterSelectActive = false;
            g_characterDeleteActive = false;
            g_savedCharacterPreviews.clear();
            LoadTitleScreen();
            return;
        }
        if (g_nationSelectActive)
        {
            g_nationSelectActive = false;
            ShowHighPolyCreationPanel();
            InvalidateRect(window, NULL, FALSE);
        }
        break;
    case InputController::Action::Confirm:
        if (g_characterSelectActive && !g_savedCharacterPreviews.empty())
        {
            UnloadPlayerModel();
            g_playerAsset = std::move(g_savedCharacterPreviews[g_selectedCharacterPreview].asset);
            g_savedCharacterPreviews.clear();
            g_characterSelectActive = false;
            SetInteractionMode(InteractionController::Mode::Game);
            InvalidateRect(window, nullptr, FALSE);
            return;
        }
        if (g_characterDeleteActive && !g_savedCharacterPreviews.empty())
        {
            const std::string name = g_savedCharacterPreviews[g_selectedCharacterPreview].name;
            if (MessageBoxA(window, "Delete the selected character? This cannot be undone.",
                "Delete Character Warning", MB_YESNO | MB_ICONWARNING) == IDYES)
            {
                char exeDir[MAX_PATH] = {}, path[MAX_PATH] = {};
                FFXIFileIO::GetExecutableDirectory(exeDir, sizeof(exeDir));
                sprintf_s(path, "%sCharacters\\%s.ff11datset", exeDir, name.c_str()); DeleteFileA(path);
                sprintf_s(path, "%sCharacters\\%s.noesis", exeDir, name.c_str()); DeleteFileA(path);
                BeginCharacterDeleteScene();
            }
            return;
        }
        if (g_nationSelectActive)
            LoadSelectedNationScene();
        break;
    case InputController::Action::OpenDat:
        SendMessageA(window, WM_COMMAND,
            ApplicationMenu::CommandId(ApplicationMenu::Action::OpenDat), 0);
        break;
    case InputController::Action::ToggleGameMode:
        SendMessageA(window, WM_COMMAND,
            ApplicationMenu::CommandId(ApplicationMenu::Action::ToggleGameMode), 0);
        break;
    case InputController::Action::UnstickPlayer:
        UnstickPlayerFromGeometry();
        break;
    case InputController::Action::CycleWeather:
        SendMessageA(window, WM_COMMAND,
            ApplicationMenu::CommandId(ApplicationMenu::Action::CycleWeather), 0);
        break;
    case InputController::Action::ToggleCameraDebugOverlay:
        g_cameraDebugOverlayVisible = !g_cameraDebugOverlayVisible;
        InvalidateRect(window, NULL, FALSE);
        break;
    case InputController::Action::None:
        break;
    }
}

//========================================================================================
// Window procedure
//========================================================================================

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_developerConsoleOpen && msg != WM_KEYDOWN && msg != WM_CHAR &&
        msg != WM_KEYUP && msg != WM_MOUSEWHEEL && msg != WM_PAINT &&
        msg != WM_ERASEBKGND && msg != WM_DESTROY)
        return 0;
    switch (msg)
    {
    // The client area is owned by the continuously rendered D3D9 frame.  Do
    // not let the class background brush erase it during an invalidation;
    // that produces a visible black flash over the title/nation UI while the
    // next frame is being presented.
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            BeginPaint(hWnd, &paint);
            EndPaint(hWnd, &paint);
        }
        return 0;

    case WM_MEASUREITEM:
        {
            MEASUREITEMSTRUCT *measure = (MEASUREITEMSTRUCT *)lParam;
            if (measure && measure->CtlType == ODT_MENU)
            {
                Win32OwnerDrawMenu::MeasureItem(g_hWnd, measure, g_themeState.resources.font);
                return TRUE;
            }
        }
        break;

    case WM_DRAWITEM:
        {
            const DRAWITEMSTRUCT *draw = (const DRAWITEMSTRUCT *)lParam;
            if (draw && draw->CtlType == ODT_MENU)
            {
                Win32OwnerDrawMenu::DrawItem(draw, IsDarkColorTheme(), g_themeState.resources.font);
                return TRUE;
            }
        }
        break;

    case WM_ACTIVATEAPP:
        if (!wParam)
            InputController::FocusLost(g_input);
        SyncAppMusic();
        return 0;

    case WM_DATURA_AUDIO_PLAYER_CLOSED:
        SyncAppMusic();
        return 0;

    case WM_SIZE:
        NpcChatWindow::Layout(hWnd);
        // Ignore if minimized or if the device doesn't exist yet
        if (g_graphicsRuntime.IsInitialized() && wParam != SIZE_MINIMIZED)
        {
            const int w = LOWORD(lParam);
            const int h = HIWORD(lParam);
            if (w > 0 && h > 0)
                g_graphicsRuntime.Resize(w, h);
        }
        return 0;

    case WM_COMMAND:
        if (HandleApplicationMenuCommand(
                ApplicationMenu::Decode(LOWORD(wParam)), hWnd))
            return 0;
        break;

    case WM_LBUTTONUP:
        if (!IsGameMode()) break;
        if (g_ffxiMainMenu.open)
        {
            RECT client = {};
            GetClientRect(hWnd, &client);
            HDC dc = GetDC(hWnd);
            POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            const bool clickedTab = FFXIMainMenu::ClickTab(
                g_ffxiMainMenu, dc, client.right, client.bottom, point);
            if (dc) ReleaseDC(hWnd, dc);
            if (clickedTab)
            {
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            return 0;
        }
        if (!InputController::PlayerMouseButton(g_input, true, false,
                GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam))) return 0;
        if (IsGameMode() && !g_titleScreenActive && !g_nationSelectActive)
        {
            int width = 0, height = 0;
            D3D9Device::GetViewportOrClientSize(GraphicsDevice(), hWnd, &width, &height);
            POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            point = D3D9Device::MapClientPointToViewport(hWnd, width, height, point);
            ZoneObjectVisibility::RenderContext visibility;
            visibility.hiddenNames = &g_hiddenZoneObjects;
            visibility.viewerPointValid = g_zoneVisibilityViewerPointValid;
            visibility.lodViewerPoint = g_zoneVisibilityViewerPoint;
            visibility.visibleMapObjects = &g_zoneVisibleMapObjects;
            visibility.overrides = &g_zoneRuntimeOverrides;
            visibility.frustum = &g_zoneRenderFrustum;
            const auto doorHit = g_doors.Pick(g_zoneAsset.Model(), visibility,
                g_pickView, g_pickProjection, g_pickWidth, g_pickHeight, (float)point.x, (float)point.y);
            float npcDepth = 1.0f;
            for (const auto& target : g_npcInteraction.targets)
                if (point.x >= target.left && point.x <= target.right &&
                    point.y >= target.top && point.y <= target.bottom)
                    npcDepth = (std::min)(npcDepth, target.depth);
            if (doorHit.door >= 0 && doorHit.depth <= npcDepth)
            {
                g_npcInteraction.selected = 0;
                g_doors.Click(doorHit.door, g_player.position);
                return 0;
            }
            g_doors.selected = -1;
            if (g_npcInteraction.Click((float)point.x, (float)point.y))
            {
                for (auto& npc : g_npcRenderInstances)
                {
                    if (npc.placement.entityId != g_npcInteraction.selected ||
                        npc.assetIndex >= g_npcRenderAssets.size()) continue;
                    auto& asset = g_npcRenderAssets[npc.assetIndex];
                    if (!asset.homePoint) break;
                    HomePoint::Instance instance{npc.placement.entityId,
                        {npc.placement.transform.x, npc.placement.transform.y, npc.placement.transform.z}};
                    float distanceSquared = 0;
                    for (int axis = 0; axis < 3; ++axis)
                        distanceSquared += (instance.position[axis] - g_player.position[axis]) *
                                           (instance.position[axis] - g_player.position[axis]);
                    if (distanceSquared <= HomePoint::InteractionDistance * HomePoint::InteractionDistance &&
                        g_homePointSeconds - npc.homePointActivatedAt >= 1.0)
                    {
                        npc.homePointActivatedAt = g_homePointSeconds;
                        const float right[3] = {g_pickView._11, g_pickView._21, g_pickView._31};
                        asset.homePoint->ActivateSound(instance, g_player.position, right);
                    }
                    return 0;
                }
                FFXINpcPlacement::Placement placement;
                if (FFXINpcPlacement::Find(g_npcInteraction.selected, placement))
                    NpcChatWindow::Show(hWnd, placement.name, NpcDialogueText(placement));
            }
            return 0;
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (g_characterSelectActive || g_characterDeleteActive)
        {
            POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            int rosterWidth = 0, rosterHeight = 0;
            D3D9Device::GetViewportOrClientSize(GraphicsDevice(), hWnd, &rosterWidth, &rosterHeight);
            RECT back = CharacterRosterBackRect(rosterWidth, rosterHeight);
            if (PtInRect(&back, point))
            {
                HandleInputAction(InputController::Action::Back, hWnd);
                return 0;
            }
            RECT confirm = CharacterRosterConfirmRect(rosterWidth, rosterHeight);
            if (PtInRect(&confirm, point))
            {
                HandleInputAction(InputController::Action::Confirm, hWnd);
                return 0;
            }
            const int row = (GET_Y_LPARAM(lParam) - 84) / 30;
            if (GET_X_LPARAM(lParam) >= 28 && GET_X_LPARAM(lParam) < 248 &&
                row >= 0 && row < (int)g_savedCharacterPreviews.size())
                g_selectedCharacterPreview = row;
            return 0;
        }
        if (IsGameMode() && !g_titleScreenActive && !g_nationSelectActive)
        {
            InputController::PlayerMouseButton(g_input, true, true,
                GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            UpdatePlayerCameraTarget();
            return 0;
        }
        if (g_nationSelectActive)
        {
            int uiW = 0, uiH = 0;
            D3D9Device::GetViewportOrClientSize(GraphicsDevice(), g_hWnd, &uiW, &uiH);
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            const int nationIndex = GameUiConfig_GetNationCardIndex(
                g_gameUiConfig.nation, uiW, uiH, FFXINationSelection::Count(),
                D3D9Device::MapClientPointToViewport(g_hWnd, uiW, uiH, pt));
            if (nationIndex >= 0)
            {
                g_selectedNationIndex = nationIndex;
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            const auto actionButtons = FFXINationSelectionRenderer::GetActionButtonRects(
                g_gameUiConfig.nation, uiW, uiH);
            POINT viewportPoint = D3D9Device::MapClientPointToViewport(g_hWnd, uiW, uiH, pt);
            if (PtInRect(&actionButtons.confirm, viewportPoint))
            {
                LoadSelectedNationScene();
                return 0;
            }
            if (PtInRect(&actionButtons.back, viewportPoint))
            {
                HandleInputAction(InputController::Action::Back, hWnd);
                return 0;
            }
        }
        if (g_titleScreenActive)
        {
            int uiW = 0, uiH = 0;
            D3D9Device::GetViewportOrClientSize(GraphicsDevice(), g_hWnd, &uiW, &uiH);
            const int buttonIndex = GameUiConfig_GetTitleMenuButtonIndex(
                g_gameUiConfig.title, uiW, uiH,
                D3D9Device::MapClientPointToViewport(
                    g_hWnd, uiW, uiH, g_input.clientMouse));
            if (buttonIndex >= 0)
            {
                ActivateTitleButton(buttonIndex);
                return 0;
            }
        }
        break;

    // ---- Mouse camera orbit ----
    case WM_RBUTTONDOWN:
        if (IsGameMode())
        {
            InputController::PlayerMouseButton(g_input, false, true,
                GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            UpdatePlayerCameraTarget();
            return 0;
        }
        InputController::BeginOrbit(
            g_input, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_RBUTTONUP:
        if (IsGameMode())
        {
            InputController::PlayerMouseButton(g_input, false, false,
                GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        }
        EndMouseLook();
        return 0;

    case WM_MBUTTONDOWN:
        if (g_highPolyCreationActive && !g_titleScreenActive)
        {
            InputController::BeginPan(
                g_input, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        }
        break;

    case WM_MBUTTONUP:
        if (g_input.dragMode == InputController::DragMode::Pan)
        {
            EndMouseLook();
            return 0;
        }
        break;

    case WM_CAPTURECHANGED:
        InputController::CaptureChanged(g_input, reinterpret_cast<HWND>(lParam));
        return 0;

    case WM_KILLFOCUS:
        InputController::FocusLost(g_input);
        break;

    case WM_MOUSEMOVE:
        {
            const InputController::DragDelta delta = InputController::MouseMoved(
                g_input, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            TRACKMOUSEEVENT tme = {};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hWnd;
            TrackMouseEvent(&tme);
            if (delta.mode == InputController::DragMode::Pan)
            {
                OrbitCamera::Pan(g_orbitCamera, delta.x, delta.y);
            }
            else if (delta.mode == InputController::DragMode::Orbit)
            {
                OrbitCamera::Rotate(g_orbitCamera, delta.x, delta.y);
            }
        }
        return 0;

    case WM_MOUSELEAVE:
        InputController::MouseLeft(g_input);
        return 0;

    case WM_MOUSEWHEEL:
        if (g_developerConsoleOpen)
        {
            const int direction = GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? 1 : -1;
            const int maxScroll = std::max(0, (int)g_developerConsoleLog.size() - 15);
            g_developerConsoleScroll = std::max(0, std::min(maxScroll,
                g_developerConsoleScroll + direction));
            return 0;
        }
        OrbitCamera::Zoom(
            g_orbitCamera,
            static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) /
                static_cast<float>(WHEEL_DELTA));
        return 0;

    // ---- Keyboard ----
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
        if (wParam == VK_MENU && IsGameMode())
        {
            if (msg == WM_SYSKEYDOWN)
                InputController::KeyDown(g_input, VK_MENU);
            else
                InputController::KeyUp(g_input, VK_MENU);
            return 0;
        }
        break;

    case WM_KEYDOWN:
        if (wParam == VK_OEM_3)
        {
            g_developerConsoleOpen = !g_developerConsoleOpen;
            return 0;
        }
        if (g_developerConsoleOpen && wParam == VK_ESCAPE)
        { g_developerConsoleOpen = false; return 0; }
        if (g_developerConsoleOpen && wParam == VK_BACK)
        { if (!g_developerConsoleInput.empty()) g_developerConsoleInput.pop_back(); return 0; }
        if (g_developerConsoleOpen && (wParam == VK_UP || wParam == VK_DOWN))
        {
            if (!g_developerConsoleHistory.empty())
            {
                if (wParam == VK_UP)
                    g_developerConsoleHistoryIndex = std::min((int)g_developerConsoleHistory.size() - 1, g_developerConsoleHistoryIndex + 1);
                else
                    g_developerConsoleHistoryIndex = std::max(-1, g_developerConsoleHistoryIndex - 1);
                g_developerConsoleInput = g_developerConsoleHistoryIndex >= 0
                    ? g_developerConsoleHistory[g_developerConsoleHistory.size() - 1 - g_developerConsoleHistoryIndex] : "";
            }
            return 0;
        }
        if (g_developerConsoleOpen && wParam == VK_RETURN)
        { ExecuteDeveloperCommand(); return 0; }
        if (g_developerConsoleOpen)
            return 0;
        // FFXI consumes Escape from the chat log first.  Each press removes
        // one visible line; only a later press, after the log is hidden, may
        // open the main menu.
        if (wParam == VK_ESCAPE && IsGameMode() && NpcChatWindow::HandleEscape(lParam))
            return 0;
        if (wParam == VK_ESCAPE && IsGameMode() &&
            (g_doors.selected >= 0 || g_npcInteraction.selected))
        {
            g_npcInteraction.selected = 0;
            g_doors.selected = -1;
            return 0;
        }
        if (IsGameMode())
        {
            const bool menuKey = wParam == VK_SUBTRACT || wParam == VK_OEM_MINUS;
            const FFXIMainMenu::Action menuAction = menuKey
                ? FFXIMainMenu::HandleKey(g_ffxiMainMenu, VK_ESCAPE)
                : FFXIMainMenu::HandleKey(g_ffxiMainMenu, static_cast<unsigned int>(wParam));
            if (menuAction != FFXIMainMenu::Action::None)
            {
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            const bool menuNavigationKey =
                wParam == VK_UP || wParam == VK_DOWN || wParam == VK_LEFT ||
                wParam == VK_RIGHT || wParam == VK_RETURN || wParam == VK_ESCAPE ||
                menuKey;
            if (g_ffxiMainMenu.open && menuNavigationKey)
                return 0;
        }
        HandleInputAction(
            InputController::KeyDown(g_input, static_cast<unsigned int>(wParam)), hWnd);
        return 0;

    case WM_CHAR:
        if (g_developerConsoleOpen && wParam >= 32 && wParam < 127 && wParam != '`' && wParam != '~')
        {
            g_developerConsoleInput.push_back((char)wParam);
            return 0;
        }
        break;

    case WM_KEYUP:
        if (g_developerConsoleOpen) return 0;
        InputController::KeyUp(g_input, static_cast<unsigned int>(wParam));
        return 0;

    case WM_DESTROY:
        EndMouseLook();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

//========================================================================================
// WinMain
//========================================================================================

static void InitializeMainWindowState()
{
    InputController::Initialize(
        g_input, g_hWnd, g_applicationSettings.enableHardwareMouseCursor);
    ApplyColorTheme(g_hWnd);
    SetWindowTextW(g_hWnd, kWindowTitle);
    InitFFXIPath();
    FFXIBitmapFont::SetRootPath(g_ffxiPath);
    ApplicationMenu::Initialize(g_applicationMenu, g_hWnd, g_ffxiPath, g_themeState);
    SetMenu(g_hWnd,
        ApplicationMenu::Build(g_applicationMenu, g_applicationSettings, IsGameMode()));
    ConfigDialog::Initialize(
        g_configDialog, g_hWnd, g_ffxiPath, g_applicationSettings, g_themeState, g_gameUiConfig,
        HandleConfigDialogEvent, ConfigDialogIsGameMode);
    LowPolyCharacterPanel::Initialize(
        g_lowPolyPanel, g_hWnd, g_playerEquip, g_playerFaceVariant,
        HandleLowPolyPanelEvent);
    HighPolyCreationPanel::Initialize(
        g_highPolyCreationPanel, g_hWnd, g_creationSelection,
        g_creationAnimationIndex, g_creationAnimatedCamera,
        g_creationCharacterName, sizeof(g_creationCharacterName),
        HandleHighPolyCreationPanelEvent);
    ZoneObjectPanel::Initialize(
        g_zoneObjectPanel, g_hWnd, HandleZoneObjectPanelEvent);
}

static bool InitializeGraphicsState()
{
    if (!InitD3D())
        return false;
    LoadTitleScreen();
    return true;
}

static void RunApplicationFrame(void*, const float deltaSeconds)
{
    UpdateAdaptivePlayDrawDistance(deltaSeconds, IsGameMode());

    if (deltaSeconds > 0.0f)
    {
        if (g_transitionZoneId == ZoneElevator::kMetalworksZone &&
            ZoneElevator::Update(g_metalworksElevator, deltaSeconds,
                                 g_player.position, g_player.onGround))
        {
            g_player.verticalVelocity = 0.0f;
            g_player.jumping = false;
            PlayerController::SetLastSafePoint(g_player);
        }
        g_doorPhysicsUpdated = false;
        g_doors.Update(deltaSeconds, g_zoneCollisionMesh);
        UpdateCameraMovement(deltaSeconds);
        if (g_doors.physics && !g_doorPhysicsUpdated)
            g_doors.UpdatePhysics(deltaSeconds, g_zoneCollisionMesh,
                IsGameMode() && g_playerAsset ? g_player.position : nullptr);
        UpdatePlayerAnimation(deltaSeconds);
        UpdateNpcAnimations(deltaSeconds);
        UpdateHighPolyCreationAnimation(deltaSeconds);
        UpdateZoneTransition(deltaSeconds);
    }

    Render();
}

static void ShutdownApplication(HINSTANCE hInstance)
{
    InputController::Shutdown(g_input);
    LowPolyCharacterPanel::Destroy(g_lowPolyPanel);
    HighPolyCreationPanel::Destroy(g_highPolyCreationPanel);
    ZoneObjectPanel::Destroy(g_zoneObjectPanel);
    UnloadTitleAssets();
    UnloadCreationModel();
    UnloadPlayerModel();
    UnloadZoneModel();
    ShutdownD3D();
    ConfigDialog::Close(g_configDialog);

    if (g_hWnd && IsWindow(g_hWnd))
        SetMenu(g_hWnd, NULL);
    ApplicationMenu::Release(g_applicationMenu);

    if (g_hWnd && IsWindow(g_hWnd))
        DestroyWindow(g_hWnd);
    g_hWnd = NULL;

    Win32Theme::ReleaseState(g_themeState);
    UnregisterClassW(kWindowClassName, hInstance);
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE /*hPrevInstance*/,
                   _In_ LPSTR /*lpCmdLine*/, _In_ int nCmdShow)
{
    InitColorTheme();
    GameUiConfig_Load(g_gameUiConfig);
    NpcChatWindow::SetSize(g_gameUiConfig.chatLogWidthPercent, g_gameUiConfig.chatLogHeightPercent);
    NpcChatWindow::SetTimeout(g_gameUiConfig.chatLogTimeoutSeconds);
    Win32Application::InitializeCommonControls(
        ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES | ICC_TAB_CLASSES);

    Win32Application::WindowSpec windowSpec;
    windowSpec.instance = hInstance;
    windowSpec.windowProcedure = WndProc;
    windowSpec.className = kWindowClassName;
    windowSpec.title = kWindowTitle;
    windowSpec.clientWidth = kDefaultWidth;
    windowSpec.clientHeight = kDefaultHeight;
    windowSpec.backgroundBrush = (HBRUSH)GetStockObject(BLACK_BRUSH);
    windowSpec.icon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DATURA));
    windowSpec.smallIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DATURA));
    windowSpec.hasMenu = true;

    Win32Application::CreateWindowResult createResult;
    g_hWnd = Win32Application::CreateMainWindow(windowSpec, createResult);
    if (!g_hWnd)
    {
        const char* message = createResult == Win32Application::CreateWindowResult::CreationFailed
            ? "CreateWindowEx failed."
            : "RegisterClassExW failed.";
        MessageBoxA(NULL, message, "Error", MB_OK | MB_ICONERROR);
        Win32Theme::ReleaseState(g_themeState);
        return 1;
    }

    InitializeMainWindowState();
    const int exitCode = InitializeGraphicsState()
        ? Win32Application::Run(g_hWnd, nCmdShow, RunApplicationFrame)
        : 1;

    ShutdownApplication(hInstance);
    return exitCode;
}
