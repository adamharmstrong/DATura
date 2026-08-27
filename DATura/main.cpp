/*========================================================================================
 FFXI Model Viewer
 Win32 application skeleton + Direct3D 9 initialization
========================================================================================*/

#include "stdafx.h"
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
#include "title_scene_assets.h"
#include "ffxi_nation_selection.h"
#include "ffxi_nation_selection_renderer.h"
#include "d3d9_device.h"
#include "d3d_math.h"
#include "d3d_model_buffers.h"
#include "d3d_model_render_state.h"
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
#include "zone_object_list_columns.h"
#include "zone_object_list_rows.h"
#include "zone_object_list_selection.h"
#include "zone_object_list_view.h"
#include "zone_object_panel_loading.h"
#include "zone_object_panel_edit_controls.h"
#include "zone_object_panel_style.h"
#include "zone_object_panel.h"
#include "zone_object_tree_population.h"
#include "zone_object_tree_builder.h"
#include "zone_object_visibility.h"
#include "win32_drawing.h"
#include "win32_application.h"
#include "win32_theme.h"
#include "win32_owner_draw_menu.h"
#include "application_menu.h"
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
#include "zone_collision_geometry.h"
#include "orbit_camera.h"
#include "player_controller.h"
#include "player_model_loader.h"
#include "bgw_player.h"
#include "audio_player.h"
#include "texture_viewer.h"
#include "game_ui_config.h"
#include "npc_placement.h"
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

using NpcRenderGeometry::ProjectNpcNameplate;
using ZoneObjectListView::SetSubItem;
using ZoneObjectListColumns::SetupCollisionColumns;
using ZoneObjectListColumns::SetupDrawBatchColumns;
using ZoneObjectListColumns::SetupMapObjectColumns;
using ZoneObjectListRows::InsertCollisionRow;
using ZoneObjectListRows::InsertDrawBatchRow;
using ZoneObjectListRows::InsertMapObjectRow;
using ZoneObjectTransform::DebugTransform;
using ZoneObjectListSelection::GetSelectedMapObjectIndex;
using ZoneObjectListSelection::SetCheckStateForMapObjectIndex;
using ZoneObjectPanelEditControls::SetUnreferencedEditorVisible;
using ZoneObjectPanelStyle::ApplyListColors;
using ZoneObjectPanelStyle::ApplyTreeColors;
using ZoneObjectVisibility::SetListVisibility;
using ZoneObjectVisibility::SetHidden;
using ZoneObjectVisibility::ShowOnlySelectedMapObject;
using ZoneObjectTreePopulation::PopulateExpandedNode;
using ZoneEnvironmentIdentity::ContainsLowerToken;
using ZoneEnvironmentIdentity::IsEnvironmentObjectName;
using ZoneModelRenderMetadata::IsAnimatedWaterSurface;

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
    float animationTime = 0.0f;
    float nameplateLocalY = -2.5f;
    bool visibleLastFrame = false;
};

struct NpcRenderInstance
{
    FFXINpcPlacement::Placement placement;
    size_t assetIndex = 0;
};

static std::vector<NpcRenderAsset> g_npcRenderAssets;
static std::vector<NpcRenderInstance> g_npcRenderInstances;

static std::vector<NpcNameplateRenderer::DrawItem> g_npcNameplates;
static TitleSceneAssets::State g_titleAssets;
static GameUiConfig   g_gameUiConfig    = {};
static LowPolyCharacterPanel::State g_lowPolyPanel;
static HighPolyCreationPanel::State g_highPolyCreationPanel;
static ZoneObjectPanel::State g_zoneObjectPanel;
// Transitional references keep the behavior-heavy panel procedure stable while
// its formerly independent globals move under one explicit lifetime owner.
static HWND& g_hZoneObjectPanel = g_zoneObjectPanel.window;
static HWND& g_hZoneLabel = g_zoneObjectPanel.zoneLabel;
static HWND& g_hZoneObjectList = g_zoneObjectPanel.placedObjectList;
static HWND& g_hZoneUnrefObjectList = g_zoneObjectPanel.unreferencedObjectList;
static HWND& g_hZoneCollisionObjectList = g_zoneObjectPanel.collisionObjectList;
static HWND& g_hZoneDrawBatchList = g_zoneObjectPanel.drawBatchList;
static HWND& g_hZoneDataTree = g_zoneObjectPanel.dataTree;
static HWND& g_hZoneRawDataTree = g_zoneObjectPanel.rawDataTree;
static HWND& g_hZoneCollisionDataTree = g_zoneObjectPanel.collisionDataTree;
static HWND& g_hZoneLoadingLabel = g_zoneObjectPanel.loadingLabel;
static HWND& g_hZonePlacedShowAllButton = g_zoneObjectPanel.placedShowAllButton;
static HWND& g_hZonePlacedHideAllButton = g_zoneObjectPanel.placedHideAllButton;
static HWND& g_hZoneUnrefShowAllButton = g_zoneObjectPanel.unreferencedShowAllButton;
static HWND& g_hZoneUnrefHideAllButton = g_zoneObjectPanel.unreferencedHideAllButton;
static HWND& g_hZoneStatusLabel = g_zoneObjectPanel.statusLabel;
static HWND (&g_hZoneUnrefEdits)[ZoneObjectPanel::kUnreferencedFieldCount] =
    g_zoneObjectPanel.unreferencedEdits;
static HWND (&g_hZoneUnrefLabels)[ZoneObjectPanel::kUnreferencedFieldCount] =
    g_zoneObjectPanel.unreferencedLabels;
static HWND& g_hZoneUnrefApplyButton = g_zoneObjectPanel.unreferencedApplyButton;
static HWND& g_hZoneCollisionVisibleCheck = g_zoneObjectPanel.collisionVisibleCheck;
static bool& g_populatingZoneObjectList = g_zoneObjectPanel.populatingObjectList;
static int& g_zoneObjectColumnMode = g_zoneObjectPanel.placedColumnMode;
static int& g_zoneUnrefObjectColumnMode = g_zoneObjectPanel.unreferencedColumnMode;
static int& g_zoneCollisionObjectColumnMode = g_zoneObjectPanel.collisionColumnMode;
static int& g_zoneDrawBatchColumnMode = g_zoneObjectPanel.drawBatchColumnMode;
static int& g_zoneTreeSelectedMapObjectIndex = g_zoneObjectPanel.treeSelectedMapObjectIndex;
static bool& g_zoneCombinedObjectTree = g_zoneObjectPanel.combinedObjectTree;
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

static std::map<std::string, DebugTransform> g_zoneObjectOverrides;

// Orbit camera state
static OrbitCamera::State g_orbitCamera;
static float& g_camYaw = g_orbitCamera.yaw;
static float& g_camPitch = g_orbitCamera.pitch;
static float& g_camDist = g_orbitCamera.distance;
static float (&g_camTarget)[3] = g_orbitCamera.target;

// Mouse, keyboard, capture, and cursor state.
static InputController::State g_input;

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

// Registry keys used to persist a user-overridden path.
static const char kAppRegKey[]   = "Software\\FFXIViewer";
static const char kAppPathValue[]= "FFXIPath";
static const char kAppThemeValue[] = "ColorTheme";

// Hard-coded default FFXI installation path.
static const char kDefaultFFXIPath[] =
    "C:\\Program Files (x86)\\PlayOnline\\SquareEnix\\FINAL FANTASY XI\\";
static const char kTitleScreenZoneDat[] = "ROM/0/90.DAT"; // Konschtat Highlands
static const int  kTitleScreenMusicId = 108;              // Vana'diel March

// Active FFXI root path used to seed file browsers.
// Starts as the hard-coded default; user may override via Settings menu.
static char g_ffxiPath[MAX_PATH] = {};
static bool g_titleScreenActive = false;
static bool g_highPolyCreationActive = false;
static bool g_nationSelectActive = false;
static int  g_selectedNationIndex = 0;

static FFXICreationSelection g_creationSelection = {};
static int g_creationAnimationIndex = 0;
static char g_creationCharacterName[64] = "Adventurer";
static int  g_playerFaceVariant = 0;

using CreationSqleMotionInfo = FFXISqle::MotionInfo;

static CreationSqleMotionInfo g_creationBodyMotion = {};
static CreationSqleMotionInfo g_creationHeadMotion = {};
static float g_creationAnimTime = 0.0f;
static float g_creationHorizontalPlacement[3] = {};
static bool g_creationAnimatedCamera = false;
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
        AudioPlayer_SetRootPath(g_ffxiPath);
        TextureViewer_SetRootPath(g_ffxiPath);

        ShowFFXIPathInfoDialog("Path Saved", "FFXI path set to:", g_ffxiPath);
    }
}

// Reset to the hard-coded default path, clearing any saved custom path.
static void ResetFFXIPathToDefault()
{
    FFXIInstallPath::ClearSavedPath(kAppRegKey, kAppPathValue);
    strcpy_s(g_ffxiPath, kDefaultFFXIPath);
    AudioPlayer_SetRootPath(g_ffxiPath);
    TextureViewer_SetRootPath(g_ffxiPath);

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
        FFXIInstallPath::SavePath(kAppRegKey, kAppPathValue, g_ffxiPath);
        AudioPlayer_SetRootPath(g_ffxiPath);
        TextureViewer_SetRootPath(g_ffxiPath);

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
    case ConfigDialog::Command::CollisionVisibilityChanged:
        if (g_hZoneCollisionVisibleCheck)
        {
            SendMessageA(g_hZoneCollisionVisibleCheck, BM_SETCHECK,
                g_applicationSettings.showCollisionGeometry ? BST_CHECKED : BST_UNCHECKED, 0);
        }
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

static void SetLoadedZoneLabelFromNameAndPath(const char *name, const char *path)
{
    char relativePath[MAX_PATH] = {};
    FFXIPath::MakeRelativePath(g_ffxiPath, path, relativePath, sizeof(relativePath));
    const char* discoveredName = FFXIPath::FindZoneNameByModelPath(g_ffxiPath, path);
    g_loadedZoneLabel = SceneLoadContext::FormatLoadedLabel(
        name, discoveredName, path, relativePath);

    if (g_hZoneLabel)
        SetWindowTextA(g_hZoneLabel, g_loadedZoneLabel.c_str());
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

        g_zoneCollisionMesh.AddTriangle(tri, PlayerController::kCollisionRadius);
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

        // NPC/Home Point coordinates are in the normal server-facing world
        // orientation. The raw mirrored-zone option leaves MapGeo's native X
        // axis in place, so mirror the spawn (and heading) to keep it on the
        // same authored Home Point rather than the opposite side of the map.
        const bool rawMirroredZone = g_applicationSettings.mirrorWorldZones;
        float spawnX = rawMirroredZone ? -placement.transform.x : placement.transform.x;
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

        const float spawnYaw = rawMirroredZone ? -placement.transform.headingRadians :
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
    input.vertical = movement.vertical;
    input.boost = movement.boost;
    input.slow = movement.slow;
    return input;
}

static void UpdatePlayerMovement(const float dt)
{
    if (!g_hWnd || GetForegroundWindow() != g_hWnd)
        return;

    const PlayerController::SimulationContext context = { &g_zoneCollisionMesh };
    PlayerController::UpdateMovement(
        g_player, CapturePlayerInputSnapshot(), dt, context);
    UpdatePlayerCameraTarget();
}

static void UpdatePlayerAnimation(float dt)
{
    if (!g_playerAsset || !GraphicsDevice())
        return;

    const bool moving = InputController::IsMovementActive(g_input);

    if (!g_playerEquip.animationPlaying && (!IsGameMode() || !moving))
    {
        g_playerAnimTime = 0.0f;
        g_playerAsset.Model()->RestoreBindPose(GraphicsDevice());
        return;
    }

    g_playerAnimTime += dt;
    g_playerAsset.Model()->UpdateAnimation(g_playerAnimTime, GraphicsDevice());
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
        UpdateFlyCameraMovement(dt);
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

#if 0 // Retained temporarily as a reference while the optimized renderer replaces this path.
static void RenderModelLegacy(noesisModel_t *pModel, bool allowObjectOverrides = false)
{
    if (!pModel || !GraphicsDevice()) return;

    GraphicsDevice()->SetFVF(FFXI_VERTEX_FVF);
    D3DMATRIX baseWorld = {};
    GraphicsDevice()->GetTransform(D3DTS_WORLD, &baseWorld);
    D3DModelRenderState::ApplyFixedFunctionModelState(
        GraphicsDevice(), g_applicationSettings.enableMipMapping);
	if (allowObjectOverrides)
		ZoneEnvironmentFog::Apply(GraphicsDevice(), g_zoneEnvironment);

    // Baseline render states — no fixed-function lighting, depth test on
    GraphicsDevice()->SetRenderState(D3DRS_LIGHTING,        FALSE);
    GraphicsDevice()->SetRenderState(D3DRS_ZENABLE,         TRUE);
    GraphicsDevice()->SetRenderState(D3DRS_ZWRITEENABLE,    TRUE);
    GraphicsDevice()->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
    GraphicsDevice()->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);

    // Texture sampler — bilinear
    const noesisMatData_t *pMD = pModel->pMatData;

    for (size_t i = 0; i < pModel->submeshes.size(); ++i)
    {
        noesisModel_t::Submesh &sm = pModel->submeshes[i];
        if (!sm.pVB || !sm.pIB || sm.triCount == 0) continue;
        if (allowObjectOverrides && IsEnvironmentObjectName(sm.objectName)) continue;
        if (allowObjectOverrides && !IsZoneObjectVisible(sm.objectName)) continue;

        // Resolve material → texture
        IDirect3DTexture9 *pTex = nullptr;
        bool twoSided = true; // FFXI defaults to two-sided
        float alphaRef = 0.0f;
        bool softBlend = false;
		bool expandDxt3Alpha = false;
        if (pMD && !sm.materialName.empty())
        {
            const noesisMaterial_t *pMat = pMD->FindMaterial(sm.materialName.c_str());
            if (pMat)
            {
                if (pMat->texIdx >= 0 && pMat->texIdx < pMD->texCount)
                {
                    const noesisTex_t *pTexObj = pMD->textures[pMat->texIdx];
					if (pTexObj)
					{
						pTex = pTexObj->pD3DTex;
						expandDxt3Alpha = pTexObj->texType == NOESISTEX_DXT3;
					}
                }
                twoSided = (pMat->flags & NMATFLAG_TWOSIDED) != 0;
                alphaRef = pMat->alphaTest;
                softBlend = !pMat->noDefaultBlend;
            }
        }

        if (softBlend)
            continue;

        ZoneObjectTransform::ApplyWorldTransform(
            GraphicsDevice(), sm.objectName, allowObjectOverrides, g_zoneObjectOverrides, baseWorld);

        // Per-submesh render states (opaque pass only — softBlend meshes are skipped above)
        // The runtime world transform reverses FFXI's original winding before
        // D3D sees it. In DATura's rendered coordinate space, exterior faces
        // are counter-clockwise, so cull clockwise triangles for a back-face
        // cull request.
        GraphicsDevice()->SetRenderState(D3DRS_CULLMODE,
            twoSided ? D3DCULL_NONE : D3DCULL_CW);
        GraphicsDevice()->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        GraphicsDevice()->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        GraphicsDevice()->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
        GraphicsDevice()->SetRenderState(D3DRS_DEPTHBIAS, 0);
		GraphicsDevice()->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);

        if (alphaRef > 0.0f)
        {
            GraphicsDevice()->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
            GraphicsDevice()->SetRenderState(D3DRS_ALPHAREF,  (DWORD)(alphaRef * 255.0f));
            GraphicsDevice()->SetRenderState(D3DRS_ALPHAFUNC,  D3DCMP_GREATER);
        }
        else
        {
            GraphicsDevice()->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        }

		const bool shaderActive = pTex && D3DModelRenderState::SetFfxiTexturePixelShader(
			GraphicsDevice(), alphaRef > 0.0f, expandDxt3Alpha);
        D3DModelRenderState::SetTextureStageForOptionalTexture(GraphicsDevice(), pTex,
			shaderActive ? D3DTOP_SELECTARG1 :
            (alphaRef > 0.0f ? D3DTOP_MODULATE4X : D3DTOP_MODULATE2X));
        D3DModelRenderState::SetTextureScroll(
            GraphicsDevice(), allowObjectOverrides && IsAnimatedWaterSurface(sm), 0.012f, 0.006f);
        GraphicsDevice()->SetStreamSource(0, sm.pVB, 0, (UINT)sizeof(FFXIVertex));
        GraphicsDevice()->SetIndices(sm.pIB);
        GraphicsDevice()->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0,
                                         (UINT)sm.vertCount, 0, (UINT)sm.triCount);
    }

    struct SoftBlendSubmesh
    {
        size_t index;
        float distanceSq;
    };
    std::vector<SoftBlendSubmesh> softBlendSubmeshes;
    float cameraX = 0.0f;
    float cameraY = 0.0f;
    float cameraZ = 0.0f;
    GetRenderCameraPosition(cameraX, cameraY, cameraZ);

    // FFXI's 0x8000 zone layers do not write depth. Keep them in the same
    // transparent pass, but draw the farthest surfaces first so overlapping
    // crag detail layers compose instead of depending on DAT record order.
    for (size_t i = 0; i < pModel->submeshes.size(); ++i)
    {
        const noesisModel_t::Submesh &sm = pModel->submeshes[i];
        if (!sm.pVB || !sm.pIB || sm.triCount == 0) continue;
        if (allowObjectOverrides && IsEnvironmentObjectName(sm.objectName)) continue;
        if (allowObjectOverrides && !IsZoneObjectVisible(sm.objectName)) continue;
        if (!pMD || sm.materialName.empty()) continue;

        const noesisMaterial_t *pMat = pMD->FindMaterial(sm.materialName.c_str());
        if (!pMat || pMat->noDefaultBlend)
            continue;

        float centerX = 0.0f;
        float centerY = 0.0f;
        float centerZ = 0.0f;
        if (!sm.cpuVerts.empty())
        {
            const FFXIVertex *samples[] =
            {
                &sm.cpuVerts.front(),
                &sm.cpuVerts[sm.cpuVerts.size() / 2],
                &sm.cpuVerts.back()
            };
            for (int sampleIndex = 0; sampleIndex < 3; ++sampleIndex)
            {
                centerX += samples[sampleIndex]->pos[0];
                centerY += samples[sampleIndex]->pos[1];
                centerZ += samples[sampleIndex]->pos[2];
            }
            centerX /= 3.0f;
            centerY /= 3.0f;
            centerZ /= 3.0f;
        }

        const float dx = centerX - cameraX;
        const float dy = centerY - cameraY;
        const float dz = centerZ - cameraZ;
        SoftBlendSubmesh draw = { i, dx * dx + dy * dy + dz * dz };
        softBlendSubmeshes.push_back(draw);
    }
    std::stable_sort(softBlendSubmeshes.begin(), softBlendSubmeshes.end(),
        [](const SoftBlendSubmesh &a, const SoftBlendSubmesh &b)
        {
            return a.distanceSq > b.distanceSq;
        });

    for (size_t drawIndex = 0; drawIndex < softBlendSubmeshes.size(); ++drawIndex)
    {
        noesisModel_t::Submesh &sm = pModel->submeshes[softBlendSubmeshes[drawIndex].index];
        if (!sm.pVB || !sm.pIB || sm.triCount == 0) continue;
        if (allowObjectOverrides && !IsZoneObjectVisible(sm.objectName)) continue;

        IDirect3DTexture9 *pTex = nullptr;
        bool twoSided = true;
        bool softBlend = false;
		bool expandDxt3Alpha = false;
		if (pMD && !sm.materialName.empty())
        {
            const noesisMaterial_t *pMat = pMD->FindMaterial(sm.materialName.c_str());
            if (pMat)
            {
                if (pMat->texIdx >= 0 && pMat->texIdx < pMD->texCount)
                {
                    const noesisTex_t *pTexObj = pMD->textures[pMat->texIdx];
					if (pTexObj)
					{
						pTex = pTexObj->pD3DTex;
						expandDxt3Alpha = pTexObj->texType == NOESISTEX_DXT3;
					}
                }
                twoSided = (pMat->flags & NMATFLAG_TWOSIDED) != 0;
                softBlend = !pMat->noDefaultBlend;
            }
        }

        if (!softBlend)
            continue;

        ZoneObjectTransform::ApplyWorldTransform(
            GraphicsDevice(), sm.objectName, allowObjectOverrides, g_zoneObjectOverrides, baseWorld);

        GraphicsDevice()->SetRenderState(D3DRS_CULLMODE,
            twoSided ? D3DCULL_NONE : D3DCULL_CW);
        GraphicsDevice()->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        GraphicsDevice()->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        GraphicsDevice()->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        GraphicsDevice()->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
		GraphicsDevice()->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
		// 0x8000 is true source-alpha blending. Its zero-alpha texels naturally
		// contribute nothing; alpha testing here would create a second, incorrect
		// cutout rule on textured rock and decal layers.
        GraphicsDevice()->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		// Blended zone layers only need a small coplanar nudge. A large slope-scale
		// bias lets terrain decals win depth tests against nearby 3D structures.
		GraphicsDevice()->SetRenderState(D3DRS_DEPTHBIAS, D3DModelRenderState::FloatBits(-0.000001f));
		GraphicsDevice()->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);

		const bool shaderActive = pTex && D3DModelRenderState::SetFfxiTexturePixelShader(
			GraphicsDevice(), true, expandDxt3Alpha);
		D3DModelRenderState::SetTextureStageForOptionalTexture(
			GraphicsDevice(), pTex, shaderActive ? D3DTOP_SELECTARG1 : D3DTOP_MODULATE4X);
		D3DModelRenderState::SetTextureScroll(
			GraphicsDevice(), allowObjectOverrides && IsAnimatedWaterSurface(sm), -0.009f, 0.004f);
        GraphicsDevice()->SetStreamSource(0, sm.pVB, 0, (UINT)sizeof(FFXIVertex));
        GraphicsDevice()->SetIndices(sm.pIB);
        GraphicsDevice()->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0,
                                         (UINT)sm.vertCount, 0, (UINT)sm.triCount);
    }

    // Clean up texture binding
    GraphicsDevice()->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    GraphicsDevice()->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    GraphicsDevice()->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
	GraphicsDevice()->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    GraphicsDevice()->SetRenderState(D3DRS_DEPTHBIAS, 0);
	GraphicsDevice()->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);
    GraphicsDevice()->SetTexture(0, nullptr);
	D3DModelRenderState::SetTextureScroll(GraphicsDevice(), false);
	GraphicsDevice()->SetPixelShader(nullptr);
    GraphicsDevice()->SetTransform(D3DTS_WORLD, &baseWorld);
    GraphicsDevice()->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE2X);
    GraphicsDevice()->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE2X);
    GraphicsDevice()->SetStreamSource(0, nullptr, 0, 0);
    GraphicsDevice()->SetIndices(nullptr);
}

#endif

static void RenderModel(noesisModel_t *pModel, bool allowObjectOverrides = false)
{
    if (!pModel || !GraphicsDevice())
        return;

    ZoneModelRenderMetadata::Prepare(pModel, GraphicsDevice());
    GraphicsDevice()->SetFVF(FFXI_VERTEX_FVF);
    D3DMATRIX baseWorld = {};
    GraphicsDevice()->GetTransform(D3DTS_WORLD, &baseWorld);

    // The dynamic tier adds an inexpensive planar pass for standalone actors. Zone terrain
    // has uneven ground, so projecting an entire zone onto a single plane would be wrong.
    if (!allowObjectOverrides &&
        g_applicationSettings.lightingQuality == ApplicationSettings::LightingDynamicShadows)
    {
        float groundY = 0.0f;
        bool hasGround = false;
        for (const noesisModel_t::Submesh &submesh : pModel->submeshes)
        {
            // Character DAT submeshes do not consistently populate render bounds;
            // use their CPU vertices so the actor pass is never silently skipped.
            const std::vector<FFXIVertex> &vertices = !submesh.cpuVerts.empty()
                ? submesh.cpuVerts
                : submesh.cpuBindVerts;
            for (const FFXIVertex &vertex : vertices)
            {
                if (!hasGround || vertex.pos[1] < groundY)
                {
                    groundY = vertex.pos[1];
                    hasGround = true;
                }
            }
        }

        if (hasGround)
        {
            const float angle = (static_cast<float>(ZoneEnvironmentState::CurrentMinuteOfDay()) / 1440.0f) *
                6.2831853f;
            const float lightX = -0.45f * cosf(angle);
            const float lightY = -0.35f - 0.65f * fmaxf(sinf(angle - 1.5707963f), 0.15f);
            const float lightZ = 0.45f * sinf(angle);
            D3DMATRIX shadow = {};
            shadow._11 = lightY;
            shadow._21 = -lightX;
            shadow._23 = -lightZ;
            shadow._31 = 0.0f;
            shadow._33 = lightY;
            shadow._41 = lightX * groundY;
            shadow._42 = groundY * lightY;
            shadow._43 = lightZ * groundY;
            shadow._44 = lightY;

            const D3DMATRIX shadowWorld = D3DMath::Multiply(baseWorld, shadow);
            D3DMATERIAL9 shadowMaterial = {};
            shadowMaterial.Diffuse = { 0.0f, 0.0f, 0.0f, 0.32f };
            shadowMaterial.Ambient = shadowMaterial.Diffuse;
            GraphicsDevice()->SetFVF(FFXI_VERTEX_FVF);
            GraphicsDevice()->SetTransform(D3DTS_WORLD, &shadowWorld);
            GraphicsDevice()->SetMaterial(&shadowMaterial);
            GraphicsDevice()->SetRenderState(D3DRS_LIGHTING, FALSE);
            GraphicsDevice()->SetRenderState(D3DRS_COLORVERTEX, FALSE);
            GraphicsDevice()->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
            GraphicsDevice()->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            GraphicsDevice()->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
            GraphicsDevice()->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
            // The projected vertices deliberately land on the receiver plane. An
            // equal-depth test plus a tiny bias prevents the terrain depth from
            // rejecting the entire shadow pass.
            GraphicsDevice()->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
            GraphicsDevice()->SetRenderState(D3DRS_DEPTHBIAS,
                D3DModelRenderState::FloatBits(-0.00001f));
            GraphicsDevice()->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);
            GraphicsDevice()->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            GraphicsDevice()->SetTexture(0, nullptr);
            GraphicsDevice()->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
            GraphicsDevice()->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
            GraphicsDevice()->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
            GraphicsDevice()->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
            for (size_t index : pModel->opaqueSubmeshOrder)
                D3DModelBuffers::DrawSubmesh(GraphicsDevice(), pModel, pModel->submeshes[index]);
            GraphicsDevice()->SetTransform(D3DTS_WORLD, &baseWorld);
        }
    }
    D3DModelRenderState::ApplyFixedFunctionModelState(
        GraphicsDevice(), g_applicationSettings.enableMipMapping);
    if (allowObjectOverrides)
        ZoneEnvironmentFog::Apply(GraphicsDevice(), g_zoneEnvironment);

    D3DModelRenderState::ApplyDynamicLighting(
        GraphicsDevice(), g_applicationSettings.lightingQuality,
        ZoneEnvironmentState::CurrentMinuteOfDay(),
        allowObjectOverrides && g_zoneEnvironment.valid && g_zoneEnvironment.indoor);
    GraphicsDevice()->SetRenderState(D3DRS_ZENABLE, TRUE);
    GraphicsDevice()->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    GraphicsDevice()->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    GraphicsDevice()->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    GraphicsDevice()->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESS);
    GraphicsDevice()->SetRenderState(D3DRS_DEPTHBIAS, 0);
    GraphicsDevice()->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);
    GraphicsDevice()->SetTransform(D3DTS_WORLD, &baseWorld);

    bool textureAnimationEnabled = false;
    float textureAnimationU = 0.0f;
    float textureAnimationV = 0.0f;
    D3DModelRenderState::SetTextureScroll(GraphicsDevice(), false);
    auto updateTextureAnimation = [&](bool enabled, float speedU, float speedV)
    {
        if (enabled == textureAnimationEnabled &&
            (!enabled || (speedU == textureAnimationU && speedV == textureAnimationV)))
            return;
        D3DModelRenderState::SetTextureScroll(GraphicsDevice(), enabled, speedU, speedV);
        textureAnimationEnabled = enabled;
        textureAnimationU = speedU;
        textureAnimationV = speedV;
    };

    D3DModelRenderState::MaterialBindingCache materialCache;

    ZoneObjectVisibility::RenderContext renderVisibility;
    renderVisibility.hiddenNames = &g_hiddenZoneObjects;
    renderVisibility.viewerPointValid = g_zoneVisibilityViewerPointValid;
    renderVisibility.visibleMapObjects = &g_zoneVisibleMapObjects;
    renderVisibility.overrides = &g_zoneObjectOverrides;
    renderVisibility.frustum = &g_zoneRenderFrustum;

    const bool editedObjectTransforms = allowObjectOverrides && !g_zoneObjectOverrides.empty();
    const bool canUseOpaqueBatches = !pModel->opaqueBatches.empty() &&
        (!allowObjectOverrides || (g_hiddenZoneObjects.empty() && g_zoneObjectOverrides.empty() &&
                                   g_zoneVisibleMapObjects.empty()));
    if (canUseOpaqueBatches)
    {
        for (const noesisModel_t::OpaqueBatch &batch : pModel->opaqueBatches)
        {
            if (allowObjectOverrides &&
                !ZoneRenderFrustum::IntersectsBounds(
                    g_zoneRenderFrustum, batch.boundsMin, batch.boundsMax, batch.hasBounds))
                continue;
            D3DModelRenderState::ApplyOpaqueMaterial(
                GraphicsDevice(), materialCache, batch.pMaterial, batch.pTexture);
            updateTextureAnimation(allowObjectOverrides && batch.animatedWater, 0.012f, 0.006f);
            D3DModelBuffers::DrawOpaqueBatch(GraphicsDevice(), pModel, batch);
        }
    }
    else
    {
        for (size_t index : pModel->opaqueSubmeshOrder)
        {
            noesisModel_t::Submesh &sm = pModel->submeshes[index];
            if (!D3DModelBuffers::HasDrawBuffers(pModel, sm)) continue;
            if (allowObjectOverrides && sm.environmentObject) continue;
            if (!ZoneObjectVisibility::PassesRenderVisibility(
                    pModel, sm, allowObjectOverrides, renderVisibility)) continue;
            if (editedObjectTransforms)
                ZoneObjectTransform::ApplyWorldTransform(
                    GraphicsDevice(), sm.objectName, true, g_zoneObjectOverrides, baseWorld);
            D3DModelRenderState::ApplyOpaqueMaterial(
                GraphicsDevice(), materialCache, sm.pResolvedMaterial, sm.pResolvedTexture);
            updateTextureAnimation(allowObjectOverrides && sm.animatedWater, 0.012f, 0.006f);
            D3DModelBuffers::DrawSubmesh(GraphicsDevice(), pModel, sm);
        }
    }

    struct SortedSoftBlend
    {
        size_t index;
        float distanceSq;
    };
    std::vector<SortedSoftBlend> visibleSoftBlend;
    visibleSoftBlend.reserve(pModel->softBlendSubmeshOrder.size());
    float cameraX, cameraY, cameraZ;
    GetRenderCameraPosition(cameraX, cameraY, cameraZ);
    for (size_t index : pModel->softBlendSubmeshOrder)
    {
        const noesisModel_t::Submesh &sm = pModel->submeshes[index];
        if (!D3DModelBuffers::HasDrawBuffers(pModel, sm)) continue;
        if (allowObjectOverrides && sm.environmentObject) continue;
        if (!ZoneObjectVisibility::PassesRenderVisibility(
                pModel, sm, allowObjectOverrides, renderVisibility)) continue;
        const float dx = sm.boundsCenter[0] - cameraX;
        const float dy = sm.boundsCenter[1] - cameraY;
        const float dz = sm.boundsCenter[2] - cameraZ;
        visibleSoftBlend.push_back({ index, dx * dx + dy * dy + dz * dz });
    }
    std::stable_sort(visibleSoftBlend.begin(), visibleSoftBlend.end(),
        [](const SortedSoftBlend &a, const SortedSoftBlend &b)
        {
            return a.distanceSq > b.distanceSq;
        });

    GraphicsDevice()->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    GraphicsDevice()->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    GraphicsDevice()->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    GraphicsDevice()->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    GraphicsDevice()->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    GraphicsDevice()->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    GraphicsDevice()->SetRenderState(D3DRS_DEPTHBIAS, D3DModelRenderState::FloatBits(-0.000001f));
    materialCache.Invalidate();

    for (const SortedSoftBlend &draw : visibleSoftBlend)
    {
        noesisModel_t::Submesh &sm = pModel->submeshes[draw.index];
        if (editedObjectTransforms)
            ZoneObjectTransform::ApplyWorldTransform(
                GraphicsDevice(), sm.objectName, true, g_zoneObjectOverrides, baseWorld);
        D3DModelRenderState::ApplyTransparentMaterial(
            GraphicsDevice(), materialCache, sm.pResolvedMaterial, sm.pResolvedTexture);
        updateTextureAnimation(allowObjectOverrides && sm.animatedWater, -0.009f, 0.004f);
        D3DModelBuffers::DrawSubmesh(GraphicsDevice(), pModel, sm);
    }

    GraphicsDevice()->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    GraphicsDevice()->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    GraphicsDevice()->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    GraphicsDevice()->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESS);
    GraphicsDevice()->SetRenderState(D3DRS_DEPTHBIAS, 0);
    GraphicsDevice()->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);
    GraphicsDevice()->SetTexture(0, nullptr);
    updateTextureAnimation(false, 0.0f, 0.0f);
    GraphicsDevice()->SetPixelShader(nullptr);
    GraphicsDevice()->SetTransform(D3DTS_WORLD, &baseWorld);
    GraphicsDevice()->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE2X);
    GraphicsDevice()->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE2X);
    GraphicsDevice()->SetStreamSource(0, nullptr, 0, 0);
    GraphicsDevice()->SetIndices(nullptr);
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
    BuildZoneCollisionFromDAT();
    const int zoneId = FFXIPath::FindZoneIDByModelPath(g_ffxiPath, path);
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
    if (userContentLoad)
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

        g_camYaw = 0.72f;
        g_camPitch = -0.34f;
        // Preserve the title-screen framing while keeping the collision-aware
        // target chosen by LoadDatFile. Never begin the backdrop camera underground.
        if (g_camDist < 180.0f)
            g_camDist = 180.0f;

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

    // The retail-style title composition uses Konschtat's thunder sky. The
    // generic zone default remains Fine; only the title backdrop selects thdr.
    UpdateZoneEnvironmentState();
    for (size_t weather = 0; weather < g_zoneEnvironmentCache.weatherGroups.size(); ++weather)
    {
        const ff11EnvironmentRecord_t *record = g_zoneEnvironmentCache.weatherGroups[weather];
        if (record && ZoneEnvironmentIdentity::ContainsLowerToken(record->directoryPath, "/thdr"))
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
    if (look.size() != 20 || FFXINpcPlacement::LookU16(look, 0) != 1)
        return {};

    const int raceIndex = (int)look[3] - 1;
    if (raceIndex < 0 || raceIndex >= kFFXICharRaceCount)
        return {};
    const FFXICharRace &race = kFFXICharRaces[raceIndex];
    if (race.count < 9)
        return {};

    char datSet[4096] = {};
    strcat_s(datSet, "NOESIS_FF11_DAT_SET\nsetPathAbs \"");
    strcat_s(datSet, g_ffxiPath);
    strcat_s(datSet, "\"\n");
    FFXIDatSet::AppendLine(datSet, sizeof(datSet), "__skeleton", race.entries[0].dat);
    FFXIDatSet::AppendLine(datSet, sizeof(datSet), "__animation", race.entries[8].dat);
    FFXIDatSet::AppendVariantLine(datSet, sizeof(datSet), "face", race, 1, (int)look[2]);
    FFXIDatSet::AppendVariantLine(datSet, sizeof(datSet), "head", race, 2, (int)(FFXINpcPlacement::LookU16(look, 4) & 0x0FFF));
    FFXIDatSet::AppendVariantLine(datSet, sizeof(datSet), "body", race, 3, (int)(FFXINpcPlacement::LookU16(look, 6) & 0x0FFF));
    FFXIDatSet::AppendVariantLine(datSet, sizeof(datSet), "hands", race, 4, (int)(FFXINpcPlacement::LookU16(look, 8) & 0x0FFF));
    FFXIDatSet::AppendVariantLine(datSet, sizeof(datSet), "legs", race, 5, (int)(FFXINpcPlacement::LookU16(look, 10) & 0x0FFF));
    FFXIDatSet::AppendVariantLine(datSet, sizeof(datSet), "feet", race, 6, (int)(FFXINpcPlacement::LookU16(look, 12) & 0x0FFF));

    const unsigned int mainId = FFXINpcPlacement::LookU16(look, 14) & 0x0FFF;
    const unsigned int subId = FFXINpcPlacement::LookU16(look, 16) & 0x0FFF;
    const unsigned int rangedId = FFXINpcPlacement::LookU16(look, 18) & 0x0FFF;
    if (mainId > 0)
        FFXIDatSet::AppendVariantLine(datSet, sizeof(datSet), "main", race, 7, (int)mainId - 1);
    if (subId > 0)
        FFXIDatSet::AppendVariantLine(datSet, sizeof(datSet), "sub", race, 7, (int)subId - 1);
    if (rangedId > 0)
        FFXIDatSet::AppendVariantLine(datSet, sizeof(datSet), "ranged", race, 7, (int)rangedId - 1);

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
    g_npcNameplates.clear();
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
            if (placement.appearance.kind == FFXINpcPlacement::AppearanceKind::HumanoidLook)
                asset.resource = LoadNpcHumanoidModel(placement.appearance.rawLook);
            else if (placement.appearance.kind == FFXINpcPlacement::AppearanceKind::ModelId)
                asset.resource = LoadNpcStandaloneModel(placement.appearance.modelId);

            if (asset.resource)
            {
                // Assemble the initial skinned pose before measuring the head
                // anchor; otherwise the first frame uses component bind space.
                asset.resource.Model()->UpdateAnimation(0.0f, GraphicsDevice());
                asset.nameplateLocalY =
                    NpcRenderGeometry::ComputeNameplateLocalY(asset.resource.Model());
                assetIndex = g_npcRenderAssets.size();
                g_npcRenderAssets.push_back(std::move(asset));
            }
            appearanceCache[key] = assetIndex;
        }
        if (assetIndex != invalidAsset)
        {
            FFXINpcPlacement::Placement scenePlacement = placement;
            if (g_applicationSettings.mirrorWorldZones)
            {
                scenePlacement.transform.x = -scenePlacement.transform.x;
                scenePlacement.transform.headingRadians = -scenePlacement.transform.headingRadians;
            }
            SnapPlacementToZoneFloor(scenePlacement.transform);
            g_npcRenderInstances.push_back({ std::move(scenePlacement), assetIndex });
        }
    }
}

static void UpdateNpcAnimations(float dt)
{
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
    g_creationSelection = {};

    const FFXICreationEntry *pEntry = CurrentHighPolyCreationEntry();
    if (pEntry)
        LoadCreationEntry(pEntry);
    ShowHighPolyCreationPanel();
}

static bool ZoneObjectIsUnreferenced(int index)
{
    const char *displayName = Model_FF11_GetLastMapObjectDisplayName(index);
    return displayName && strncmp(displayName, "env:", 4) == 0;
}

static void PopulateZoneDataTree()
{
    ZoneObjectTreeBuilder::Context context = {};
    context.placedObjectList = g_hZoneObjectList;
    context.unreferencedObjectList = g_hZoneUnrefObjectList;
    context.dataTree = g_hZoneDataTree;
    context.rawDataTree = g_hZoneRawDataTree;
    context.collisionDataTree = g_hZoneCollisionDataTree;
    context.combinedObjectTree = g_zoneCombinedObjectTree;
    context.loadedZoneLabel = g_loadedZoneLabel.c_str();
    ZoneObjectTreeBuilder::Build(context);
}

static void InsertZoneInfoRow(const char *name, const char *value)
{
    if (!g_hZoneObjectList)
        return;
    LVITEMA item = {};
    item.mask = LVIF_TEXT;
    item.iItem = ListView_GetItemCount(g_hZoneObjectList);
    item.iSubItem = 0;
    item.pszText = const_cast<char *>(name);
    const int row = (int)SendMessageA(g_hZoneObjectList, LVM_INSERTITEMA, 0, (LPARAM)&item);
    SetSubItem(g_hZoneObjectList, row, 1, value);
}

static int GetSelectedZoneObjectIndex()
{
    return GetSelectedMapObjectIndex(
        g_hZoneObjectList, g_hZoneUnrefObjectList, g_zoneTreeSelectedMapObjectIndex);
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
    if (!g_hZoneObjectList || !g_hZoneUnrefObjectList || !g_hZoneCollisionObjectList || !g_hZoneDrawBatchList ||
        !g_hZoneDataTree || !g_hZoneRawDataTree || !g_hZoneCollisionDataTree)
        return;

    g_populatingZoneObjectList = true;
    g_zoneTreeSelectedMapObjectIndex = -1;
    SendMessageA(g_hZoneObjectList, WM_SETREDRAW, FALSE, 0);
    SendMessageA(g_hZoneUnrefObjectList, WM_SETREDRAW, FALSE, 0);
    SendMessageA(g_hZoneCollisionObjectList, WM_SETREDRAW, FALSE, 0);
    SendMessageA(g_hZoneDrawBatchList, WM_SETREDRAW, FALSE, 0);
    SendMessageA(g_hZoneDataTree, WM_SETREDRAW, FALSE, 0);
    SendMessageA(g_hZoneRawDataTree, WM_SETREDRAW, FALSE, 0);
    SendMessageA(g_hZoneCollisionDataTree, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(g_hZoneObjectList);
    ListView_DeleteAllItems(g_hZoneUnrefObjectList);
    ListView_DeleteAllItems(g_hZoneCollisionObjectList);
    ListView_DeleteAllItems(g_hZoneDrawBatchList);
    SetupMapObjectColumns(g_hZoneObjectList, g_zoneObjectColumnMode, 0);
    SetupMapObjectColumns(g_hZoneUnrefObjectList, g_zoneUnrefObjectColumnMode, 1);
    SetupCollisionColumns(g_hZoneCollisionObjectList, g_zoneCollisionObjectColumnMode, 0);
    SetupDrawBatchColumns(g_hZoneDrawBatchList, g_zoneDrawBatchColumnMode, 0);
    ApplyListColors(g_hZoneObjectList, true);
    ApplyListColors(g_hZoneUnrefObjectList, true);
    ApplyListColors(g_hZoneCollisionObjectList, false);
    ApplyListColors(g_hZoneDrawBatchList, false);
    ApplyTreeColors(g_hZoneDataTree);
    ApplyTreeColors(g_hZoneRawDataTree);
    ApplyTreeColors(g_hZoneCollisionDataTree);

    if (g_hZoneLabel)
        SetWindowTextA(g_hZoneLabel, g_loadedZoneLabel.c_str());

    const int mapObjectCount = Model_FF11_GetLastMapObjectCount();
    for (int i = 0; i < mapObjectCount; ++i)
    {
        InsertMapObjectRow(ZoneObjectIsUnreferenced(i) ? g_hZoneUnrefObjectList : g_hZoneObjectList,
                           i, g_zoneObjectOverrides, g_hiddenZoneObjects);
    }
    const int collisionMeshCount = Model_FF11_GetLastCollisionMeshCount();
    for (int i = 0; i < collisionMeshCount; ++i)
    {
        InsertCollisionRow(g_hZoneCollisionObjectList, i);
    }
    const int drawBatchCount = Model_FF11_GetLastMapGeoDrawBatchCount();
    for (int i = 0; i < drawBatchCount; ++i)
    {
        InsertDrawBatchRow(g_hZoneDrawBatchList, i);
    }
    PopulateZoneDataTree();

    g_populatingZoneObjectList = false;
    SetUnreferencedEditorVisible(g_hZoneUnrefLabels, g_hZoneUnrefEdits, 9,
                                 g_hZoneUnrefApplyButton, true);
    if (g_hZoneCollisionVisibleCheck)
    {
        char collisionText[128] = {};
        sprintf_s(collisionText, "Show collision mesh (%d triangles)",
                  (int)g_zoneCollisionTris.size());
        SetWindowTextA(g_hZoneCollisionVisibleCheck, collisionText);
        ShowWindow(g_hZoneCollisionVisibleCheck, SW_SHOW);
    }
    if (g_hZonePlacedShowAllButton)
        ShowWindow(g_hZonePlacedShowAllButton, SW_SHOW);
    if (g_hZonePlacedHideAllButton)
        ShowWindow(g_hZonePlacedHideAllButton, SW_SHOW);
    if (g_hZoneUnrefShowAllButton)
        ShowWindow(g_hZoneUnrefShowAllButton, SW_SHOW);
    if (g_hZoneUnrefHideAllButton)
        ShowWindow(g_hZoneUnrefHideAllButton, SW_SHOW);
    UpdateZoneObjectEditControlState();
    if (g_hZoneStatusLabel)
    {
        const noesisModel_t* zoneModel = g_zoneAsset.Model();
        const int meshCount = zoneModel ? (int)zoneModel->submeshes.size() : 0;
        const int materialCount = (zoneModel && zoneModel->pMatData) ?
            zoneModel->pMatData->matCount : 0;
        const int textureCount = (zoneModel && zoneModel->pMatData) ?
            zoneModel->pMatData->texCount : 0;
        const int boneCount = zoneModel ? zoneModel->boneCount : 0;
        char statusText[512] = {};
        const int placedCount = g_hZoneObjectList ? ListView_GetItemCount(g_hZoneObjectList) : 0;
        const int mapGeoResourceCount = g_hZoneUnrefObjectList ? ListView_GetItemCount(g_hZoneUnrefObjectList) : 0;
        const int collisionGridEntryCount = g_hZoneCollisionObjectList ? ListView_GetItemCount(g_hZoneCollisionObjectList) : 0;
        sprintf_s(statusText,
                  "Map records: %d     Unreferenced MapGeo: %d     Draw batches: %d     Collision grid entries: %d     Meshes: %d     Textures: %d     Materials: %d     Bones: %d",
                  placedCount, mapGeoResourceCount, drawBatchCount,
                  collisionGridEntryCount, meshCount, textureCount, materialCount, boneCount);
        SetWindowTextA(g_hZoneStatusLabel, statusText);
    }
    ZoneObjectTransformEditor::PopulateMapObjectFields(
        g_hZoneUnrefEdits, 9, GetSelectedZoneObjectIndex(), g_zoneObjectOverrides);
    SendMessageA(g_hZoneObjectList, WM_SETREDRAW, TRUE, 0);
    SendMessageA(g_hZoneUnrefObjectList, WM_SETREDRAW, TRUE, 0);
    SendMessageA(g_hZoneCollisionObjectList, WM_SETREDRAW, TRUE, 0);
    SendMessageA(g_hZoneDrawBatchList, WM_SETREDRAW, TRUE, 0);
    SendMessageA(g_hZoneDataTree, WM_SETREDRAW, TRUE, 0);
    SendMessageA(g_hZoneRawDataTree, WM_SETREDRAW, TRUE, 0);
    SendMessageA(g_hZoneCollisionDataTree, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_hZoneObjectList, NULL, TRUE);
    InvalidateRect(g_hZoneUnrefObjectList, NULL, TRUE);
    InvalidateRect(g_hZoneCollisionObjectList, NULL, TRUE);
    InvalidateRect(g_hZoneDrawBatchList, NULL, TRUE);
    InvalidateRect(g_hZoneDataTree, NULL, TRUE);
    InvalidateRect(g_hZoneRawDataTree, NULL, TRUE);
    InvalidateRect(g_hZoneCollisionDataTree, NULL, TRUE);
    if (g_hZoneLoadingLabel)
        ShowWindow(g_hZoneLoadingLabel, SW_HIDE);
}

static void BeginZoneObjectPanelRefresh()
{
    ZoneObjectPanelLoading::Context context;
    context.panel = g_hZoneObjectPanel;
    context.zoneLabel = g_hZoneLabel;
    context.placedList = g_hZoneObjectList;
    context.unreferencedList = g_hZoneUnrefObjectList;
    context.collisionList = g_hZoneCollisionObjectList;
    context.drawBatchList = g_hZoneDrawBatchList;
    context.dataTree = g_hZoneDataTree;
    context.rawDataTree = g_hZoneRawDataTree;
    context.collisionDataTree = g_hZoneCollisionDataTree;
    context.loadingLabel = g_hZoneLoadingLabel;
    context.loadedZoneLabel = g_loadedZoneLabel.c_str();
    context.placedColumnMode = &g_zoneObjectColumnMode;
    context.unreferencedColumnMode = &g_zoneUnrefObjectColumnMode;
    context.collisionColumnMode = &g_zoneCollisionObjectColumnMode;
    context.drawBatchColumnMode = &g_zoneDrawBatchColumnMode;
    context.populateMessage = ZoneObjectPanel::kPopulateMessage;
    ZoneObjectPanelLoading::Begin(context);
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
        g_zoneTreeSelectedMapObjectIndex =
            event.mapObjectIndex >= 0 &&
            event.mapObjectIndex < Model_FF11_GetLastMapObjectCount()
                ? event.mapObjectIndex : -1;
        ZoneObjectTransformEditor::PopulateMapObjectFields(
            g_hZoneUnrefEdits, ZoneObjectPanel::kUnreferencedFieldCount,
            GetSelectedZoneObjectIndex(), g_zoneObjectOverrides);
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
        ZoneObjectTransformEditor::PopulateMapObjectFields(
            g_hZoneUnrefEdits, ZoneObjectPanel::kUnreferencedFieldCount,
            GetSelectedZoneObjectIndex(), g_zoneObjectOverrides);
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
        g_zoneCombinedObjectTree = !g_zoneCombinedObjectTree;
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
        MessageBoxA(g_hWnd, "Character selection is not implemented yet.",
                    "DATura", MB_OK | MB_ICONINFORMATION);
        break;

    case 2:
        MessageBoxA(g_hWnd, "Character deletion is not implemented yet.",
                    "DATura", MB_OK | MB_ICONINFORMATION);
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

static void DrawNationSelectOverlay()
{
    if (!g_nationSelectActive || !g_hWnd)
        return;

    const FFXINationSelectionRenderer::Context context =
    {
        GraphicsDevice(), g_hWnd, g_applicationSettings.enableMipMapping,
        g_titleAssets.UiModel(),
        g_gameUiConfig.nation, g_selectedNationIndex
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

static void DrawTitleScreenOverlay()
{
    if (!g_titleScreenActive || !g_hWnd)
        return;

    const FFXITitleScreenRenderer::Context context =
    {
        GraphicsDevice(), g_hWnd, g_applicationSettings.enableMipMapping,
        g_titleAssets.LogoModel(), g_titleAssets.AtlasModel(), g_titleAssets.UiModel(),
        g_gameUiConfig.title, g_input.clientMouse
    };
    FFXITitleScreenRenderer::DrawOverlay(context);
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
    if (SUCCEEDED(GraphicsDevice()->BeginScene()))
    {
        // ---- Camera setup ----
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
        const float terrainProjectionDistance = unlimitedDrawDistance ?
            BoundsDerivedZoneFarPlane(zoneModel, renderCameraEye[0],
                                      renderCameraEye[1], renderCameraEye[2]) :
            ((zoneModel && g_zoneEnvironment.valid) ?
                std::min(configuredDrawDistance, g_zoneEnvironment.drawDistance) :
                configuredDrawDistance);
        D3DMATRIX proj = D3DMath::BuildPerspectiveFovLH(
            useCreationCamera ? creationCameraFov : 3.14159265f * 0.25f,
			aspect, 0.01f, terrainProjectionDistance);
		const float skyProjectionDistance = (zoneModel && g_zoneEnvironment.valid) ?
			std::max(terrainProjectionDistance, g_zoneEnvironment.skyRadius * 1.125f) :
			terrainProjectionDistance;
        D3DMATRIX skyProj = D3DMath::BuildPerspectiveFovLH(
            3.14159265f * 0.25f, aspect, 0.01f, skyProjectionDistance);
        const D3DMATRIX viewProjection = D3DMath::Multiply(view, proj);
        ZoneRenderFrustum::Build(g_zoneRenderFrustum, view, proj);
        const float visibilityXSign = g_applicationSettings.mirrorWorldZones ? 1.0f : -1.0f;
        // Retail selects the active cell from the player, not the third-person
        // eye. In DATura the orbit target is the equivalent position; the eye
        // can be high above and outside every finite cell volume.
        g_zoneVisibilityViewerPoint[0] = renderCameraTarget[0] * visibilityXSign;
        g_zoneVisibilityViewerPoint[1] = renderCameraTarget[1];
        g_zoneVisibilityViewerPoint[2] = renderCameraTarget[2];
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
            RenderModel(zoneModel, true);
            ZoneWeatherParticles::Draw(
                GraphicsDevice(), zoneModel, g_zoneEnvironment.valid,
                g_applicationSettings.enableMipMapping, g_zoneEnvironment.weatherPath,
                gFF11LastGeneratorRecords, gFF11LastKeyframeRecords,
                renderCameraEye[0], renderCameraEye[1], renderCameraEye[2]);
            ZoneObjectHighlightRenderer::Draw(
                GraphicsDevice(), zoneModel, g_highlightedZoneObject, g_zoneObjectOverrides);
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

        if (g_applicationSettings.showCollisionGeometry)
            ZoneCollision::DrawOverlay(GraphicsDevice(), g_zoneCollisionMesh);

        for (NpcRenderAsset &asset : g_npcRenderAssets)
            asset.visibleLastFrame = false;

        for (const NpcRenderInstance &npc : g_npcRenderInstances)
        {
            if (npc.assetIndex >= g_npcRenderAssets.size() || !npc.placement.visible)
                continue;
            NpcRenderAsset &asset = g_npcRenderAssets[npc.assetIndex];
            if (!asset.resource)
                continue;
            FFXINpcPlacement::Transform transform = npc.placement.transform;
            transform.headingRadians += 3.1415926535f * 0.5f;
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
            const float npcVisibilityPoint[3] =
            {
                transform.x * visibilityXSign,
                transform.y,
                transform.z
            };
            if (!Model_FF11_IsZonePointVisible(cameraVisibilityPoint, npcVisibilityPoint))
                continue;

            asset.visibleLastFrame = true;
            GraphicsDevice()->SetTransform(D3DTS_WORLD, &npcWorld);
            RenderModel(asset.resource.Model());

            if (!npc.placement.name.empty() &&
                !FFXIPath::StringEqualsNoCase(npc.placement.name.c_str(), "blank"))
            {
                nameplate.name = npc.placement.name;
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
        }

        if (!g_titleScreenActive && !g_nationSelectActive)
            NpcNameplateRenderer::Draw(GraphicsDevice(), g_npcNameplates);
        DrawTitleScreenTextures();
        DrawNationSelectTextures();

        GraphicsDevice()->EndScene();
    }

    g_graphicsRuntime.Present();
    DrawTitleScreenOverlay();
    DrawNationSelectOverlay();
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
    case InputController::Action::Exit:
        PostQuitMessage(0);
        break;
    case InputController::Action::Back:
        if (g_nationSelectActive)
        {
            g_nationSelectActive = false;
            ShowHighPolyCreationPanel();
            InvalidateRect(window, NULL, FALSE);
        }
        break;
    case InputController::Action::Confirm:
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
    case InputController::Action::None:
        break;
    }
}

//========================================================================================
// Window procedure
//========================================================================================

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
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

    case WM_LBUTTONDOWN:
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
        InputController::BeginOrbit(
            g_input, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_RBUTTONUP:
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
        OrbitCamera::Zoom(
            g_orbitCamera,
            static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) /
                static_cast<float>(WHEEL_DELTA));
        return 0;

    // ---- Keyboard ----
    case WM_KEYDOWN:
        HandleInputAction(
            InputController::KeyDown(g_input, static_cast<unsigned int>(wParam)), hWnd);
        return 0;

    case WM_KEYUP:
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
    ApplicationMenu::Initialize(g_applicationMenu, g_hWnd, g_ffxiPath, g_themeState);
    SetMenu(g_hWnd,
        ApplicationMenu::Build(g_applicationMenu, g_applicationSettings, IsGameMode()));
    ConfigDialog::Initialize(
        g_configDialog, g_hWnd, g_ffxiPath, g_applicationSettings, g_themeState,
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
    if (deltaSeconds > 0.0f)
    {
        UpdateCameraMovement(deltaSeconds);
        UpdatePlayerAnimation(deltaSeconds);
        UpdateNpcAnimations(deltaSeconds);
        UpdateHighPolyCreationAnimation(deltaSeconds);
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
