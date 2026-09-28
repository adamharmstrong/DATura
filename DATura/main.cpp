/*========================================================================================
 FFXI Model Viewer
 Win32 application skeleton + backend renderer initialization
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
#include "renderer_backend.h"
#include "d3d9_device.h"
#include "d3d_ui_renderer.h"
#include "d3d_math.h"
#include "d3d_model_render_state.h"
#include "custom_texture_assets.h"
#include "model_renderer.h"
#include "zone_model_transform.h"
#include "zone_model_render_metadata.h"
#include "zone_environment_identity.h"
#include "zone_environment_render_state.h"
#include "zone_render_frustum.h"
#include "zone_weather_particles.h"
#include "zone_sky_dome.h"
#include "zone_environment_state.h"
#include "zone_object_transform.h"
#include "zone_object_highlight_renderer.h"
#include "zone_object_transform_editor.h"
#include "zone_object_list_selection.h"
#include "zone_object_picker.h"
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
#include "zone_elevator_audio.h"
#include "zone_entry_message.h"
#include "orbit_camera.h"
#include "player_controller.h"
#include "player_model_loader.h"
#include "bgw_player.h"
#include "audio_player.h"
#include "home_point_effect.h"
#include "character_save_data.h"
#include "texture_viewer.h"
#include "game_ui_config.h"
#include "npc_placement.h"
#include "ffxi_event_table.h"
#include "ffxi_event_messages.h"
#include "ffxi_lore_zone_dat_crossref.h"
#include "resource.h"
#include <commctrl.h>
#include <shellapi.h>
#include <windowsx.h>
#include <cmath>
#include <cstdio>
#include <cctype>
#include <cfloat>
#include <limits>
#include <array>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <random>
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
// Window / graphics backend state
//========================================================================================

static HWND g_hWnd = NULL;
static RendererBackend::Runtime g_graphicsRuntime;

static const int  kDefaultWidth     = 1280;
static const int  kDefaultHeight    = 720;
static const int  kCompanionViewerWidth = 720;
static const int  kCompanionViewerHeight = 540;
static const wchar_t kWindowClassName[] = L"FFXIViewerWndClass";
static const wchar_t kWindowTitle[]     = L"DATura - FFXI Model Viewer";
static const wchar_t kCompanionViewerTitle[] = L"DATura - Model Viewer";
static const wchar_t kCompanionViewerMutexName[] = L"Local\\DATuraModelViewer";
static const char kCompanionViewerArg[] = "--companion-viewer";
static const char kCompanionModelArg[] = "--model";
static const char kCompanionLabelArg[] = "--label";
static const char kCompanionOwnerPidArg[] = "--owner-pid";
static const char kPlayerRaceArg[] = "--player-race";
static const char kPlayerCustomizationArg[] = "--player-customization";
static const char kCreationModelArg[] = "--creation-model";
static const ULONG_PTR kCompanionModelCopyDataId = 0x44544343; // DTCC
static const ULONG_PTR kPlayerRaceCopyDataId = 0x44545052; // DTPR
static const ULONG_PTR kCreationModelCopyDataId = 0x4454434D; // DTCM
static bool g_companionViewerMode = false;
static std::string g_startupModelDat;
static std::string g_startupModelLabel;
static std::string g_startupPlayerCustomization;
static int g_startupPlayerRace = -1;
static int g_startupCreationModel = -1;
static float g_modelViewerLightAzimuthDegrees = 150.0f;
static float g_modelViewerLightElevationDegrees = 45.0f;
static int g_modelViewerLightOverlayCorner = 0;
static DWORD g_companionOwnerPid = 0;
static HANDLE g_companionOwnerProcess = NULL;
static HANDLE g_companionViewerMutex = NULL;

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
    float renderedHeadingRadians = 0.0f;
    bool renderedHeadingInitialized = false;
};

struct NpcEventRuntime
{
    const FFXIEventTable::Event* event = nullptr;
    std::array<std::uint32_t, 80> local = {};
    std::array<std::uint32_t, 96> zone = {};
    std::array<std::uint16_t, 8> jumps = {};
    std::size_t pc = 0;
    std::size_t jumpIndex = 0;
    std::uint32_t pendingMessage = 0xffffffffu;
    std::uint32_t pendingChoiceMessage = 0xffffffffu;
    std::uint32_t defaultChoice = 0;
    std::uint32_t hiddenChoiceMask = 0;
    bool messageOpen = false;
    bool waitingChoice = false;
    bool finished = false;
};

struct NpcConversationState
{
    std::uint32_t entityId = 0;
    std::string name;
    std::vector<std::string> frames;
    std::size_t index = 0;
    std::vector<FFXIEventTable::EntityScripts> scripts;
    std::vector<FFXIEventMessages::Entry> messages;
    NpcEventRuntime runtime;
    std::string choicePrompt;
    std::vector<std::string> choiceOptions;
    std::vector<std::uint32_t> choiceValues;
    int choiceSelected = 0;
};

struct HomePointTeleportState
{
    bool active = false;
    CharacterSaveData::HomePoint source;
    std::vector<CharacterSaveData::HomePoint> destinations;
};

static std::vector<NpcRenderAsset> g_npcRenderAssets;
static std::vector<NpcRenderInstance> g_npcRenderInstances;
static NpcConversationState g_npcConversation;
static HomePointTeleportState g_homePointTeleport;
static double g_homePointSeconds = 0.0;

static bool CompleteHomePointTeleport(HWND owner);

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
static std::set<std::string> g_highlightedZoneObjects;
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
static ZoneElevator::Audio g_metalworksElevatorAudio;
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
static bool g_zoneMapVisible = false;
static bool g_uiVisible = true;
static bool g_clickMoveActive = false;
static float g_clickMoveTarget[3] = {};
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

struct MetalworksElevatorCollisionBinding
{
    int platform = -1;
    int triangle = -1;
    ZoneCollisionTriangle closed = {};
};

static std::vector<MetalworksElevatorCollisionBinding> g_metalworksElevatorCollision;

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
static const char kAppTitleBackgroundModeValue[] = "TitleBackgroundMode";
static const char kAppTitleBackgroundZoneValue[] = "TitleBackgroundZone";
static const char kAppTitleMusicModeValue[] = "TitleMusicMode";
static const char kAppTitleMusicValue[] = "TitleMusic";
static const char kAppShowTitleUiValue[] = "ShowTitleUi";
static const char kAppEnableHdTexturesValue[] = "EnableHdTextures";
static const char kAppHdTextureFolderValue[] = "HdTextureFolder";
static const char kAppEnablePbrValue[] = "EnablePbr";
static const char kAppPbrTextureFolderValue[] = "PbrTextureFolder";
static const char kAppRenderingResolutionValue[] = "RenderingResolutionScale";
static const char kAppMapTextureCompressionValue[] = "MapTextureCompression";
static const char kAppWeatherEffectsValue[] = "WeatherEffects";
static const char kAppInvertBumpMappingValue[] = "InvertBumpMapping";
static const char kAppAntiAliasingValue[] = "AntiAliasing";
static const char kAppPostProcessAntiAliasingValue[] = "PostProcessAntiAliasing";
static const char kAppShadowPlayerValue[] = "ShadowPlayer";
static const char kAppShadowNpcsValue[] = "ShadowNpcs";
static const char kAppShadowObjectsValue[] = "ShadowObjects";
static const char kAppShadowGroundValue[] = "ShadowGroundLikeObjects";
static const char kAppShadowAlphaValue[] = "ShadowAlphaTestedObjects";
static const char kAppShadowDistanceValue[] = "ShadowMaxDistance";
static const char kAppShadowNpcLimitValue[] = "ShadowNpcLimit";
static const char kAppShadowObjectLimitValue[] = "ShadowObjectLimit";
static const char kAppShadowMinSizeValue[] = "ShadowMinimumSizePercent";
static const char kAppShadowMaxSizeValue[] = "ShadowMaximumSize";
static const char kAppShadowUpdateFramesValue[] = "ShadowReceiverUpdateFrames";
static const char kAppShadowReceiverQualityValue[] = "ShadowReceiverQuality";
static const char kAppShadowLengthValue[] = "ShadowMaximumLength";
static const char kAppShadowOpacityValue[] = "ShadowOpacityPercent";
static const char kAppShadowDebugValue[] = "ShadowDebugVisualization";
static const char kAppShadowCountersValue[] = "ShadowPerformanceCounters";
static const char* const kAppKeyBindingValues[] =
{
    "KeyForward", "KeyBackward", "KeyLeft", "KeyRight", "KeyJump",
    "KeyAutoRun", "KeyRunToggle", "KeyGameMode", "KeyZoneMap",
    "KeyUnstick", "KeyCycleWeather", "KeyCameraDebug", "KeyBumpMapping",
    "KeyMainMenu"
};

// Hard-coded default FFXI installation path.
static const char kDefaultFFXIPath[] =
    "C:\\Program Files (x86)\\PlayOnline\\SquareEnix\\FINAL FANTASY XI\\";
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

struct TitleCameraRailKey
{
    float targetOffset[3];
    float yawOffset;
    float pitch;
    float distance;
};

// The retail zones use the authored cameras recovered from ROM/0/23.DAT.
// Konschtat is DATura's twentieth, non-retail scene, so these conservative
// control points remain its custom slow looping rail (and the fallback for a
// damaged or incomplete retail title DAT).
static constexpr TitleCameraRailKey kTitleCameraRail[] =
{
    { {  0.0f,  0.0f,   0.0f }, -0.035f, 0.185f, 266.0f },
    { { -5.0f,  1.5f,   7.0f }, -0.010f, 0.178f, 258.0f },
    { { -9.0f,  2.5f,  12.0f },  0.025f, 0.172f, 250.0f },
    { { -4.0f,  1.0f,   6.0f },  0.055f, 0.182f, 255.0f },
    { {  4.0f, -1.0f,  -5.0f },  0.030f, 0.195f, 265.0f },
    { {  6.0f, -1.5f, -10.0f }, -0.010f, 0.198f, 272.0f },
};
static constexpr float kTitleCameraRailSeconds = 48.0f;
static float g_titleCameraRailTime = 0.0f;
static float g_titleCameraBaseTarget[3] =
{
    kTitleScreenCameraTarget[0], kTitleScreenCameraTarget[1], kTitleScreenCameraTarget[2]
};
static float g_titleCameraBaseYaw = kTitleScreenCameraYaw;
static float g_titleCameraBasePitch = kTitleScreenCameraPitch;
static float g_titleCameraBaseDistance = kTitleScreenCameraDistance;
static int g_titleBackgroundIndex = -1;
static int g_titleCycleIndex = -1;
static int g_titleMusicIndex = -1;
static int g_titleMusicCycleIndex = -1;
static char g_ffxiPath[MAX_PATH] = {};

struct RetailTitleCameraKey
{
    std::array<float, 3> eye = {};
    std::array<float, 3> at = {};
};

struct RetailTitleCamera
{
    unsigned int easing = 0;
    std::vector<RetailTitleCameraKey> keys;
};

struct RetailTitleShot
{
    const RetailTitleCamera* camera = nullptr;
    float startSeconds = 0.0f;
    float durationSeconds = 0.0f;
};

struct RetailTitleCameraPlayer
{
    std::map<std::string, RetailTitleCamera> cameras;
    std::vector<RetailTitleShot> shots;
    float durationSeconds = 0.0f;
    bool active = false;
};

static RetailTitleCameraPlayer g_retailTitleCamera;

template<typename T>
static bool ReadTitleDatValue(const BYTE* data, const size_t size,
                              const size_t offset, T* value)
{
    if (!data || !value || offset > size || sizeof(T) > size - offset)
        return false;
    memcpy(value, data + offset, sizeof(T));
    return true;
}

static std::string ReadTitleDatTag(const BYTE* data, const size_t size,
                                   const size_t offset)
{
    if (!data || offset > size || 4 > size - offset)
        return {};
    char tag[5] = {};
    memcpy(tag, data + offset, 4);
    return std::string(tag, strnlen_s(tag, 4));
}

static const char* RetailTitleScheduleForBackground(const int backgroundIndex)
{
    // Each retail scheduler identifies its zone with opcode 0x7b. Map those
    // zone-specific sequences into DATura's configured background order.
    // Index 8 is DATura's additional Konschtat scene and has no retail entry.
    static constexpr const char* schedules[] =
    {
        "mov1", "mov2", "mov5", "mov3", "mov4", "mov8", "mov6", "mov7",
        nullptr,
        "ex1c", "ex1a", "ex1b",
        "ex2c", "ex2a", "ex2d", "ex2b",
        "ex3a", "ex3d", "ex3c", "ex3e",
    };
    return backgroundIndex >= 0 && backgroundIndex < static_cast<int>(_countof(schedules))
        ? schedules[backgroundIndex] : nullptr;
}

static bool LoadRetailTitleCamera(const char* scheduleName)
{
    g_retailTitleCamera = {};
    if (!scheduleName || !scheduleName[0])
        return false;

    char path[MAX_PATH] = {};
    strcpy_s(path, g_ffxiPath);
    const size_t length = strlen(path);
    if (length && path[length - 1] != '\\' && path[length - 1] != '/')
        strcat_s(path, "\\");
    strcat_s(path, "ROM\\0\\23.DAT");

    BYTE* bytes = nullptr;
    DWORD byteCount = 0;
    if (!FFXIFileIO::ReadWholeFile(path, &bytes, &byteCount))
        return false;
    std::unique_ptr<BYTE[]> owned(bytes);

    struct SchedulePayload { const BYTE* data = nullptr; size_t size = 0; } schedule;
    for (size_t offset = 0; offset + 16 <= byteCount;)
    {
        unsigned int info = 0;
        if (!ReadTitleDatValue(bytes, byteCount, offset + 4, &info))
            return false;
        const size_t chunkSize = (info >> 3) & 0x7ffff0;
        const unsigned int type = info & 0x7f;
        if (chunkSize < 16 || chunkSize > byteCount - offset)
            return false;
        const std::string name = ReadTitleDatTag(bytes, byteCount, offset);
        const BYTE* payload = bytes + offset + 16;
        const size_t payloadSize = chunkSize - 16;

        if (type == 6 && payloadSize >= 32)
        {
            unsigned int mode = 0;
            // The first 16 payload bytes are the serialized resource base.
            // YmCamera's key/spline fields begin at payload offset 0x10.
            ReadTitleDatValue(payload, payloadSize, 16, &mode);
            const unsigned int keyCount = mode & 0xff;
            if (keyCount > 0 && keyCount <= 64 &&
                32 + static_cast<size_t>(keyCount) * 48 <= payloadSize)
            {
                RetailTitleCamera camera;
                ReadTitleDatValue(payload, payloadSize, 20, &camera.easing);
                for (unsigned int key = 0; key < keyCount; ++key)
                {
                    const size_t keyOffset = 32 + static_cast<size_t>(key) * 48;
                    RetailTitleCameraKey parsed;
                    memcpy(parsed.eye.data(), payload + keyOffset, sizeof(float) * 3);
                    memcpy(parsed.at.data(), payload + keyOffset + 16, sizeof(float) * 3);
                    camera.keys.push_back(parsed);
                }
                g_retailTitleCamera.cameras[name] = std::move(camera);
            }
        }
        else if (type == 7 && name == scheduleName)
        {
            schedule = { payload, payloadSize };
        }
        offset += chunkSize;
    }

    if (!schedule.data || schedule.size < 0x20)
        return false;
    unsigned int begin = 0, end = 0;
    if (!ReadTitleDatValue(schedule.data, schedule.size, 0x14, &begin) ||
        !ReadTitleDatValue(schedule.data, schedule.size, 0x18, &end) ||
        begin < 16 || end < begin || end - 16 > schedule.size)
        return false;

    float timelineFrames = 0.0f;
    for (size_t cursor = begin - 16; cursor + 4 <= end - 16;)
    {
        const BYTE opcode = schedule.data[cursor];
        const size_t commandSize = (std::max)(1, static_cast<int>(schedule.data[cursor + 1])) * 4;
        if (commandSize > end - 16 - cursor)
            return false;
        unsigned short waitFrames = 0;
        ReadTitleDatValue(schedule.data, schedule.size, cursor + 4, &waitFrames);
        if (opcode == 4 && commandSize >= 12)
        {
            unsigned short durationFrames = 0;
            ReadTitleDatValue(schedule.data, schedule.size, cursor + 6, &durationFrames);
            const std::string cameraName = ReadTitleDatTag(
                schedule.data, schedule.size, cursor + 8);
            const auto found = g_retailTitleCamera.cameras.find(cameraName);
            if (found != g_retailTitleCamera.cameras.end() && durationFrames > 0)
            {
                g_retailTitleCamera.shots.push_back({
                    &found->second, timelineFrames / 60.0f,
                    static_cast<float>(durationFrames) / 60.0f });
            }
        }
        // Scheduler waits occur after the current command has been launched.
        timelineFrames += static_cast<float>(waitFrames);
        cursor += commandSize;
    }
    g_retailTitleCamera.durationSeconds = timelineFrames / 60.0f;
    g_retailTitleCamera.active = !g_retailTitleCamera.shots.empty() &&
        g_retailTitleCamera.durationSeconds > 0.0f;
    return g_retailTitleCamera.active;
}

static float CatmullRom(const float p0, const float p1, const float p2,
                        const float p3, const float t)
{
    const float t2 = t * t;
    const float t3 = t2 * t;
    return 0.5f * ((2.0f * p1) + (-p0 + p2) * t +
        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

static float ApplyRetailCameraEasing(float t, const unsigned int mode)
{
    constexpr float halfPi = 1.5707963267948966f;
    constexpr float pi = 3.1415926535897932f;
    t = std::clamp(t, 0.0f, 1.0f);
    switch (mode)
    {
    case 1: return std::sin(t * halfPi);             // fast start, soft finish
    case 2: return 1.0f - std::cos(t * halfPi);      // soft start, fast finish
    case 3:
    case 4: return 0.5f - 0.5f * std::cos(t * pi);  // soft at both ends
    default: return t;
    }
}

static std::array<float, 3> SampleRetailCameraSpline(
    const RetailTitleCamera& camera, const bool eye, const float normalizedTime)
{
    const size_t count = camera.keys.size();
    if (count == 0)
        return {};
    const auto point = [&](const size_t index) -> const std::array<float, 3>&
    {
        return eye ? camera.keys[index].eye : camera.keys[index].at;
    };
    if (count == 1)
        return point(0);

    // YmSpline::MakeTable2 parameterizes eye and look-at curves by their
    // three-dimensional chord length. Reproduce that parameterization and a
    // natural cubic spline for each component.
    std::vector<float> parameter(count, 0.0f);
    for (size_t i = 1; i < count; ++i)
    {
        float lengthSquared = 0.0f;
        for (int axis = 0; axis < 3; ++axis)
        {
            const float difference = point(i)[axis] - point(i - 1)[axis];
            lengthSquared += difference * difference;
        }
        parameter[i] = parameter[i - 1] + std::sqrt(lengthSquared);
    }
    const float totalLength = parameter.back();
    if (totalLength <= 0.000001f)
        return point(0);
    for (float& value : parameter)
        value /= totalLength;

    const float t = std::clamp(normalizedTime, 0.0f, 1.0f);
    size_t segment = 0;
    while (segment + 2 < count && parameter[segment + 1] < t)
        ++segment;
    const float h = (std::max)(0.000001f,
        parameter[segment + 1] - parameter[segment]);
    const float a = (parameter[segment + 1] - t) / h;
    const float b = (t - parameter[segment]) / h;

    std::array<float, 3> result = {};
    for (int axis = 0; axis < 3; ++axis)
    {
        std::vector<float> second(count, 0.0f);
        if (count > 2)
        {
            std::vector<float> lower(count, 0.0f), diagonal(count, 0.0f);
            std::vector<float> upper(count, 0.0f), rhs(count, 0.0f);
            diagonal[0] = diagonal[count - 1] = 1.0f;
            for (size_t i = 1; i + 1 < count; ++i)
            {
                const float left = parameter[i] - parameter[i - 1];
                const float right = parameter[i + 1] - parameter[i];
                lower[i] = left;
                diagonal[i] = 2.0f * (left + right);
                upper[i] = right;
                rhs[i] = 6.0f * ((point(i + 1)[axis] - point(i)[axis]) / right -
                    (point(i)[axis] - point(i - 1)[axis]) / left);
            }
            for (size_t i = 1; i < count; ++i)
            {
                const float factor = lower[i] / diagonal[i - 1];
                diagonal[i] -= factor * upper[i - 1];
                rhs[i] -= factor * rhs[i - 1];
            }
            for (size_t i = count; i-- > 0;)
            {
                second[i] = (rhs[i] - (i + 1 < count ? upper[i] * second[i + 1] : 0.0f)) /
                    diagonal[i];
            }
        }
        result[axis] = a * point(segment)[axis] + b * point(segment + 1)[axis] +
            ((a * a * a - a) * second[segment] +
             (b * b * b - b) * second[segment + 1]) * h * h / 6.0f;
    }
    return result;
}

static bool SampleRetailTitleCamera()
{
    if (!g_retailTitleCamera.active || g_retailTitleCamera.shots.empty())
        return false;
    const float time = std::fmod((std::max)(0.0f, g_titleCameraRailTime),
        g_retailTitleCamera.durationSeconds);
    const RetailTitleShot* shot = &g_retailTitleCamera.shots.front();
    for (const RetailTitleShot& candidate : g_retailTitleCamera.shots)
    {
        if (candidate.startSeconds > time)
            break;
        shot = &candidate;
    }
    const float progress = shot->durationSeconds > 0.0f
        ? std::clamp((time - shot->startSeconds) / shot->durationSeconds, 0.0f, 1.0f)
        : 1.0f;
    const float eased = ApplyRetailCameraEasing(progress, shot->camera->easing);
    auto eye = SampleRetailCameraSpline(*shot->camera, true, eased);
    auto at = SampleRetailCameraSpline(*shot->camera, false, eased);
    const bool mirrorX = !g_applicationSettings.mirrorWorldZones;
    eye = FFXICoordinateFrame::NativeDatToScene(eye, mirrorX);
    at = FFXICoordinateFrame::NativeDatToScene(at, mirrorX);

    const float dx = eye[0] - at[0];
    const float dy = eye[1] - at[1];
    const float dz = eye[2] - at[2];
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!std::isfinite(distance) || distance <= 0.0001f)
        return false;
    memcpy(g_camTarget, at.data(), sizeof(float) * 3);
    g_camDist = distance;
    g_camYaw = std::atan2(dx, dz);
    g_camPitch = std::asin(std::clamp(dy / distance, -1.0f, 1.0f));
    return true;
}

static void SampleTitleCameraRail()
{
    constexpr int count = static_cast<int>(_countof(kTitleCameraRail));
    const float wrapped = std::fmod(
        std::max(0.0f, g_titleCameraRailTime), kTitleCameraRailSeconds);
    const float position = wrapped * static_cast<float>(count) /
        kTitleCameraRailSeconds;
    const int key1 = static_cast<int>(position) % count;
    const int key0 = (key1 + count - 1) % count;
    const int key2 = (key1 + 1) % count;
    const int key3 = (key1 + 2) % count;
    const float blend = position - std::floor(position);

    for (int axis = 0; axis < 3; ++axis)
    {
        g_camTarget[axis] = g_titleCameraBaseTarget[axis] + CatmullRom(
            kTitleCameraRail[key0].targetOffset[axis],
            kTitleCameraRail[key1].targetOffset[axis],
            kTitleCameraRail[key2].targetOffset[axis],
            kTitleCameraRail[key3].targetOffset[axis], blend);
    }
    g_camYaw = g_titleCameraBaseYaw + CatmullRom(
        kTitleCameraRail[key0].yawOffset, kTitleCameraRail[key1].yawOffset,
        kTitleCameraRail[key2].yawOffset, kTitleCameraRail[key3].yawOffset, blend);
    g_camPitch = g_titleCameraBasePitch - kTitleScreenCameraPitch + CatmullRom(
        kTitleCameraRail[key0].pitch, kTitleCameraRail[key1].pitch,
        kTitleCameraRail[key2].pitch, kTitleCameraRail[key3].pitch, blend);
    g_camDist = g_titleCameraBaseDistance - kTitleScreenCameraDistance + CatmullRom(
        kTitleCameraRail[key0].distance, kTitleCameraRail[key1].distance,
        kTitleCameraRail[key2].distance, kTitleCameraRail[key3].distance, blend);
}

// Active FFXI root path used to seed file browsers is declared with the title
// camera state above because the retail camera loader consumes it directly.
static bool g_titleScreenActive = false;
static int g_titleMenuSelection = 0;
static bool g_highPolyCreationActive = false;
static bool g_nationSelectActive = false;
static bool g_characterSelectActive = false;
static bool g_characterDeleteActive = false;
static int  g_selectedNationIndex = 0;

struct SavedCharacterPreview
{
    FFXIModelLifetime::OwnedModel asset;
    std::string name;
    std::string dataPath;
};
static std::vector<SavedCharacterPreview> g_savedCharacterPreviews;
static int g_selectedCharacterPreview = 0;
static std::string g_activeCharacterDataPath;
static int g_activeCharacterHomeNationIndex = 0;
static std::string g_lastSavedCharacterDataPath;

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
static void LoadPlayerRaceInModelViewer(int raceIndex);
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
static std::string NpcDialogueText(const FFXINpcPlacement::Placement& placement);

static std::string WideToUtf8(const wchar_t* const text)
{
    if (!text)
        return std::string();

    const int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1)
        return std::string();

    std::string result(static_cast<size_t>(length - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, &result[0], length, nullptr, nullptr);
    return result;
}

static std::string QuoteCommandLineArgument(const char* const argument)
{
    std::string quoted = "\"";
    unsigned int backslashCount = 0;
    for (const char* cursor = argument ? argument : ""; *cursor; ++cursor)
    {
        if (*cursor == '\\')
        {
            ++backslashCount;
            continue;
        }

        if (*cursor == '"')
        {
            quoted.append(backslashCount * 2 + 1, '\\');
            quoted.push_back('"');
            backslashCount = 0;
            continue;
        }

        quoted.append(backslashCount, '\\');
        backslashCount = 0;
        quoted.push_back(*cursor);
    }

    quoted.append(backslashCount * 2, '\\');
    quoted.push_back('"');
    return quoted;
}

struct PlayerViewerRequest
{
    PlayerEquipState equipment;
    int faceVariant;
};

static std::string SerializePlayerViewerRequest(const PlayerViewerRequest& request)
{
    char text[256] = {};
    const PlayerEquipState& e = request.equipment;
    sprintf_s(text, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        e.raceIndex, e.mainType, e.mainItem, e.subType, e.subItem,
        e.rangedType, e.rangedItem, e.headItem, e.bodyItem, e.handsItem,
        e.legsItem, e.feetItem, e.animationBank, e.animationMode,
        e.animationPlaying ? 1 : 0, request.faceVariant);
    return text;
}

static bool DeserializePlayerViewerRequest(const char* text, PlayerViewerRequest& request)
{
    int animationPlaying = 0;
    PlayerEquipState& e = request.equipment;
    const int count = sscanf_s(text ? text : "",
        "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        &e.raceIndex, &e.mainType, &e.mainItem, &e.subType, &e.subItem,
        &e.rangedType, &e.rangedItem, &e.headItem, &e.bodyItem, &e.handsItem,
        &e.legsItem, &e.feetItem, &e.animationBank, &e.animationMode,
        &animationPlaying, &request.faceVariant);
    e.animationPlaying = animationPlaying != 0;
    return count == 16;
}

static void ParseStartupArguments()
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv)
        return;

    for (int i = 1; i < argc; ++i)
    {
        const std::string argument = WideToUtf8(argv[i]);
        if (argument == kCompanionViewerArg)
        {
            g_companionViewerMode = true;
        }
        else if (argument == kCompanionModelArg && i + 1 < argc)
        {
            g_startupModelDat = WideToUtf8(argv[++i]);
        }
        else if (argument == kCompanionLabelArg && i + 1 < argc)
        {
            g_startupModelLabel = WideToUtf8(argv[++i]);
        }
        else if (argument == kCompanionOwnerPidArg && i + 1 < argc)
        {
            g_companionOwnerPid = wcstoul(argv[++i], nullptr, 10);
        }
        else if (argument == kPlayerRaceArg && i + 1 < argc)
        {
            g_startupPlayerRace = static_cast<int>(wcstol(argv[++i], nullptr, 10));
        }
        else if (argument == kPlayerCustomizationArg && i + 1 < argc)
        {
            g_startupPlayerCustomization = WideToUtf8(argv[++i]);
        }
        else if (argument == kCreationModelArg && i + 1 < argc)
        {
            g_startupCreationModel = static_cast<int>(wcstol(argv[++i], nullptr, 10));
        }
    }

    LocalFree(argv);
}

static void SetCompanionViewerOwner(const DWORD processId)
{
    if (!g_companionViewerMode || processId == 0 || processId == GetCurrentProcessId())
        return;

    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, processId);
    if (!process)
        return;

    if (g_companionOwnerProcess)
        CloseHandle(g_companionOwnerProcess);
    g_companionOwnerProcess = process;
    g_companionOwnerPid = processId;
}

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

static DWORD ReadApplicationDword(const char* valueName, const DWORD fallback)
{
    HKEY key = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, kAppRegKey, 0, KEY_READ, &key) != ERROR_SUCCESS)
        return fallback;
    DWORD value = fallback;
    DWORD type = 0;
    DWORD size = sizeof(value);
    if (RegQueryValueExA(key, valueName, nullptr, &type,
            reinterpret_cast<BYTE*>(&value), &size) != ERROR_SUCCESS ||
        type != REG_DWORD || size != sizeof(value))
        value = fallback;
    RegCloseKey(key);
    return value;
}

static void WriteApplicationDword(const char* valueName, const DWORD value)
{
    HKEY key = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, kAppRegKey, 0, nullptr, 0,
            KEY_SET_VALUE, nullptr, &key, nullptr) == ERROR_SUCCESS)
    {
        RegSetValueExA(key, valueName, 0, REG_DWORD,
            reinterpret_cast<const BYTE*>(&value), sizeof(value));
        RegCloseKey(key);
    }
}

static std::string ReadApplicationString(const char* valueName)
{
    HKEY key = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, kAppRegKey, 0, KEY_READ, &key) != ERROR_SUCCESS)
        return {};
    char value[4096] = {};
    DWORD type = 0;
    DWORD size = sizeof(value);
    const LONG result = RegQueryValueExA(key, valueName, nullptr, &type,
        reinterpret_cast<BYTE*>(value), &size);
    RegCloseKey(key);
    return result == ERROR_SUCCESS && type == REG_SZ ? std::string(value) : std::string();
}

static void WriteApplicationString(const char* valueName, const std::string& value)
{
    HKEY key = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, kAppRegKey, 0, nullptr, 0,
            KEY_SET_VALUE, nullptr, &key, nullptr) == ERROR_SUCCESS)
    {
        RegSetValueExA(key, valueName, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(value.c_str()),
            static_cast<DWORD>(value.size() + 1));
        RegCloseKey(key);
    }
}

static void InitCustomTextureSettings()
{
    g_applicationSettings.enableHdTextures =
        ReadApplicationDword(kAppEnableHdTexturesValue, 0) != 0;
    g_applicationSettings.hdTextureFolder = ReadApplicationString(kAppHdTextureFolderValue);
    g_applicationSettings.enablePbr = ReadApplicationDword(kAppEnablePbrValue, 0) != 0;
    g_applicationSettings.pbrTextureFolder = ReadApplicationString(kAppPbrTextureFolderValue);
}

static void SaveCustomTextureSettings()
{
    WriteApplicationDword(kAppEnableHdTexturesValue,
        g_applicationSettings.enableHdTextures ? 1u : 0u);
    WriteApplicationString(kAppHdTextureFolderValue, g_applicationSettings.hdTextureFolder);
    WriteApplicationDword(kAppEnablePbrValue, g_applicationSettings.enablePbr ? 1u : 0u);
    WriteApplicationString(kAppPbrTextureFolderValue, g_applicationSettings.pbrTextureFolder);
}

static void InitExtendedGraphicsSettings()
{
    g_applicationSettings.renderingResolutionScaleIndex =
        ApplicationSettings::ClampRenderingResolutionScaleIndex(static_cast<int>(
            ReadApplicationDword(kAppRenderingResolutionValue, 2)));
    g_applicationSettings.enableMapTextureCompression =
        ReadApplicationDword(kAppMapTextureCompressionValue, 1) != 0;
    g_applicationSettings.enableWeatherEffects =
        ReadApplicationDword(kAppWeatherEffectsValue, 1) != 0;
    g_applicationSettings.invertBumpMapping =
        ReadApplicationDword(kAppInvertBumpMappingValue, 0) != 0;
    g_applicationSettings.antiAliasingIndex = ApplicationSettings::ClampAntiAliasingIndex(
        static_cast<int>(ReadApplicationDword(kAppAntiAliasingValue, 0)));
    g_applicationSettings.postProcessAntiAliasingMode =
        ApplicationSettings::ClampPostProcessAntiAliasingMode(static_cast<int>(
            ReadApplicationDword(kAppPostProcessAntiAliasingValue, 0)));
    g_applicationSettings.shadowPlayer = ReadApplicationDword(kAppShadowPlayerValue, 1) != 0;
    g_applicationSettings.shadowNpcs = ReadApplicationDword(kAppShadowNpcsValue, 1) != 0;
    g_applicationSettings.shadowObjects = ReadApplicationDword(kAppShadowObjectsValue, 1) != 0;
    g_applicationSettings.shadowGroundLikeObjects = ReadApplicationDword(kAppShadowGroundValue, 0) != 0;
    g_applicationSettings.shadowAlphaTestedObjects = ReadApplicationDword(kAppShadowAlphaValue, 0) != 0;
    g_applicationSettings.shadowMaxDistance = static_cast<int>(ReadApplicationDword(kAppShadowDistanceValue, 80));
    g_applicationSettings.shadowNpcLimit = static_cast<int>(ReadApplicationDword(kAppShadowNpcLimitValue, 16));
    g_applicationSettings.shadowObjectLimit = static_cast<int>(ReadApplicationDword(kAppShadowObjectLimitValue, 64));
    g_applicationSettings.shadowMinimumSizePercent = static_cast<int>(ReadApplicationDword(kAppShadowMinSizeValue, 25));
    g_applicationSettings.shadowMaximumSize = static_cast<int>(ReadApplicationDword(kAppShadowMaxSizeValue, 40));
    g_applicationSettings.shadowReceiverUpdateFrames = static_cast<int>(ReadApplicationDword(kAppShadowUpdateFramesValue, 4));
    g_applicationSettings.shadowReceiverQuality = static_cast<int>(ReadApplicationDword(kAppShadowReceiverQualityValue, 1));
    g_applicationSettings.shadowMaximumLength = static_cast<int>(ReadApplicationDword(kAppShadowLengthValue, 30));
    g_applicationSettings.shadowOpacityPercent = static_cast<int>(ReadApplicationDword(kAppShadowOpacityValue, 32));
    g_applicationSettings.shadowDebugVisualization = ReadApplicationDword(kAppShadowDebugValue, 0) != 0;
    g_applicationSettings.shadowPerformanceCounters = ReadApplicationDword(kAppShadowCountersValue, 0) != 0;
}

static void SaveExtendedGraphicsSettings()
{
    WriteApplicationDword(kAppRenderingResolutionValue, static_cast<DWORD>(
        ApplicationSettings::ClampRenderingResolutionScaleIndex(
            g_applicationSettings.renderingResolutionScaleIndex)));
    WriteApplicationDword(kAppMapTextureCompressionValue,
        g_applicationSettings.enableMapTextureCompression ? 1u : 0u);
    WriteApplicationDword(kAppWeatherEffectsValue,
        g_applicationSettings.enableWeatherEffects ? 1u : 0u);
    WriteApplicationDword(kAppInvertBumpMappingValue,
        g_applicationSettings.invertBumpMapping ? 1u : 0u);
    WriteApplicationDword(kAppAntiAliasingValue, static_cast<DWORD>(
        ApplicationSettings::ClampAntiAliasingIndex(g_applicationSettings.antiAliasingIndex)));
    WriteApplicationDword(kAppPostProcessAntiAliasingValue, static_cast<DWORD>(
        ApplicationSettings::ClampPostProcessAntiAliasingMode(
            g_applicationSettings.postProcessAntiAliasingMode)));
    WriteApplicationDword(kAppShadowPlayerValue, g_applicationSettings.shadowPlayer);
    WriteApplicationDword(kAppShadowNpcsValue, g_applicationSettings.shadowNpcs);
    WriteApplicationDword(kAppShadowObjectsValue, g_applicationSettings.shadowObjects);
    WriteApplicationDword(kAppShadowGroundValue, g_applicationSettings.shadowGroundLikeObjects);
    WriteApplicationDword(kAppShadowAlphaValue, g_applicationSettings.shadowAlphaTestedObjects);
    WriteApplicationDword(kAppShadowDistanceValue, g_applicationSettings.shadowMaxDistance);
    WriteApplicationDword(kAppShadowNpcLimitValue, g_applicationSettings.shadowNpcLimit);
    WriteApplicationDword(kAppShadowObjectLimitValue, g_applicationSettings.shadowObjectLimit);
    WriteApplicationDword(kAppShadowMinSizeValue, g_applicationSettings.shadowMinimumSizePercent);
    WriteApplicationDword(kAppShadowMaxSizeValue, g_applicationSettings.shadowMaximumSize);
    WriteApplicationDword(kAppShadowUpdateFramesValue, g_applicationSettings.shadowReceiverUpdateFrames);
    WriteApplicationDword(kAppShadowReceiverQualityValue, g_applicationSettings.shadowReceiverQuality);
    WriteApplicationDword(kAppShadowLengthValue, g_applicationSettings.shadowMaximumLength);
    WriteApplicationDword(kAppShadowOpacityValue, g_applicationSettings.shadowOpacityPercent);
    WriteApplicationDword(kAppShadowDebugValue, g_applicationSettings.shadowDebugVisualization);
    WriteApplicationDword(kAppShadowCountersValue, g_applicationSettings.shadowPerformanceCounters);
}

static int* KeyBindingValue(ApplicationSettings::State& settings, const int index)
{
    switch (index)
    {
    case 0: return &settings.keyForward;
    case 1: return &settings.keyBackward;
    case 2: return &settings.keyLeft;
    case 3: return &settings.keyRight;
    case 4: return &settings.keyJump;
    case 5: return &settings.keyAutoRun;
    case 6: return &settings.keyRunToggle;
    case 7: return &settings.keyGameMode;
    case 8: return &settings.keyZoneMap;
    case 9: return &settings.keyUnstick;
    case 10: return &settings.keyCycleWeather;
    case 11: return &settings.keyCameraDebug;
    case 12: return &settings.keyBumpMapping;
    default: return &settings.keyMainMenu;
    }
}

static void InitKeyBindings()
{
    for (int index = 0; index < static_cast<int>(std::size(kAppKeyBindingValues)); ++index)
    {
        int* binding = KeyBindingValue(g_applicationSettings, index);
        *binding = static_cast<int>(ReadApplicationDword(
            kAppKeyBindingValues[index], static_cast<DWORD>(*binding)));
    }
}

static void SaveKeyBindings()
{
    for (int index = 0; index < static_cast<int>(std::size(kAppKeyBindingValues)); ++index)
        WriteApplicationDword(kAppKeyBindingValues[index], static_cast<DWORD>(
            *KeyBindingValue(g_applicationSettings, index)));
}

static void InitTitleBackgroundSettings()
{
    g_applicationSettings.titleBackgroundMode =
        ApplicationSettings::ClampTitleBackgroundMode(static_cast<int>(
            ReadApplicationDword(kAppTitleBackgroundModeValue,
                ApplicationSettings::TitleBackgroundRandom)));
    g_applicationSettings.titleBackgroundZoneIndex =
        ApplicationSettings::ClampTitleBackgroundZoneIndex(static_cast<int>(
            ReadApplicationDword(kAppTitleBackgroundZoneValue, 8)));
    g_applicationSettings.titleMusicMode =
        ApplicationSettings::ClampTitleMusicMode(static_cast<int>(
            ReadApplicationDword(kAppTitleMusicModeValue,
                ApplicationSettings::TitleMusicRandom)));
    g_applicationSettings.titleMusicIndex =
        ApplicationSettings::ClampTitleMusicIndex(static_cast<int>(
            ReadApplicationDword(kAppTitleMusicValue, 0)));
    g_applicationSettings.showTitleUi = ReadApplicationDword(
        kAppShowTitleUiValue, 1) != 0;
}

static void SaveTitleBackgroundSettings()
{
    WriteApplicationDword(kAppTitleBackgroundModeValue,
        static_cast<DWORD>(ApplicationSettings::ClampTitleBackgroundMode(
            g_applicationSettings.titleBackgroundMode)));
    WriteApplicationDword(kAppTitleBackgroundZoneValue,
        static_cast<DWORD>(ApplicationSettings::ClampTitleBackgroundZoneIndex(
            g_applicationSettings.titleBackgroundZoneIndex)));
    WriteApplicationDword(kAppTitleMusicModeValue,
        static_cast<DWORD>(ApplicationSettings::ClampTitleMusicMode(
            g_applicationSettings.titleMusicMode)));
    WriteApplicationDword(kAppTitleMusicValue,
        static_cast<DWORD>(ApplicationSettings::ClampTitleMusicIndex(
            g_applicationSettings.titleMusicIndex)));
    WriteApplicationDword(kAppShowTitleUiValue,
        g_applicationSettings.showTitleUi ? 1u : 0u);
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
        FFXIChatAssets::SetRootPath(g_ffxiPath);
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
    FFXIChatAssets::SetRootPath(g_ffxiPath);
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
        FFXIChatAssets::SetRootPath(g_ffxiPath);
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
        SaveExtendedGraphicsSettings();
    case ConfigDialog::Command::LightingQualityChanged:
    case ConfigDialog::Command::ShadowSettingsChanged:
    case ConfigDialog::Command::LightDirectionChanged:
    case ConfigDialog::Command::EnvironmentalAnimationChanged:
        SyncSettingsMenuChecks();
        InvalidateRect(g_hWnd, NULL, FALSE);
        break;
    case ConfigDialog::Command::CustomTextureSettingsChanged:
        SaveCustomTextureSettings();
        InvalidateRect(g_hWnd, NULL, FALSE);
        break;
    case ConfigDialog::Command::SceneClockChanged:
        ZoneEnvironmentState::ClearTimeOverride();
        ZoneEnvironmentState::SetUseLocalSystemTime(
            g_applicationSettings.sceneClockMode == ApplicationSettings::SceneClockLocalSystem);
        ZoneEnvironmentState::Invalidate(g_zoneEnvironmentCache, false);
        InvalidateRect(g_hWnd, NULL, FALSE);
        break;
    case ConfigDialog::Command::DisplayChanged:
        ApplyDisplaySettings();
        break;
    case ConfigDialog::Command::RenderingResolutionChanged:
        SaveExtendedGraphicsSettings();
        ApplyDisplaySettings();
        break;
    case ConfigDialog::Command::MapTextureCompressionChanged:
    case ConfigDialog::Command::WeatherEffectsChanged:
        SaveExtendedGraphicsSettings();
        InvalidateRect(g_hWnd, NULL, FALSE);
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
        NpcChatWindow::SetFont(g_gameUiConfig.chatLogFont, g_gameUiConfig.chatLogFontSize);
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
    case ConfigDialog::Command::MouseCursorStyleChanged:
        InputController::SetCursorStyle(
            g_input, g_applicationSettings.mouseCursorStyle, g_ffxiPath);
        break;
    case ConfigDialog::Command::MovementStyleChanged:
        InvalidateRect(g_hWnd, NULL, FALSE);
        break;
    case ConfigDialog::Command::TitleBackgroundChanged:
        SaveTitleBackgroundSettings();
        SaveExtendedGraphicsSettings();
        if (g_titleScreenActive)
            LoadTitleScreen();
        break;
    case ConfigDialog::Command::ApplySettings:
        SetColorTheme(event.value);
        SaveExtendedGraphicsSettings();
        SaveCustomTextureSettings();
        SaveKeyBindings();
        SaveTitleBackgroundSettings();
        ApplyDisplaySettings();
        ApplyTextureCompressionSetting();
        g_doors.SetPhysics(
            g_applicationSettings.doorInteractionMode == ApplicationSettings::DoorPhysics);
        ZoneEnvironmentState::SetUseLocalSystemTime(
            g_applicationSettings.sceneClockMode == ApplicationSettings::SceneClockLocalSystem);
        InputController::SetHardwareCursorEnabled(
            g_input, g_applicationSettings.enableHardwareMouseCursor);
        InputController::SetCursorStyle(
            g_input, g_applicationSettings.mouseCursorStyle, g_ffxiPath);
        InputController::SetKeyBindings(g_input, g_applicationSettings);
        SyncSettingsMenuChecks();
        SyncAppMusic();
        NpcChatWindow::SetSize(g_gameUiConfig.chatLogWidthPercent,
            g_gameUiConfig.chatLogHeightPercent);
        NpcChatWindow::SetTimeout(g_gameUiConfig.chatLogTimeoutSeconds);
        NpcChatWindow::SetFont(g_gameUiConfig.chatLogFont,
            g_gameUiConfig.chatLogFontSize);
        GameUiConfig_SaveChatLog(g_gameUiConfig);
        GameUiConfig_SavePlayerNameplate(g_gameUiConfig);
        NpcNameplateRenderer::ClearCachedTextures();
        ZoneObjectPanel::SetCollisionVisible(
            g_zoneObjectPanel, g_applicationSettings.showCollisionGeometry);
        ZoneEnvironmentState::ClearTimeOverride();
        ZoneEnvironmentState::Invalidate(g_zoneEnvironmentCache, false);
        if (g_titleScreenActive)
            LoadTitleScreen();
        InvalidateRect(g_hWnd, NULL, FALSE);
        break;
    }
}

static bool ConfigDialogIsGameMode(void*)
{
    return IsGameMode();
}

//========================================================================================
// Graphics backend helpers
//========================================================================================

static IDirect3DDevice9* GraphicsDevice()
{
    return g_graphicsRuntime.D3D9Device();
}

static RendererBackend::DisplayConfiguration CurrentDisplayConfiguration()
{
    const ApplicationSettings::ResolutionOption& resolution =
        ApplicationSettings::ResolutionOptionAt(g_applicationSettings.resolutionIndex);
    RendererBackend::DisplayConfiguration display;
    display.borderless = g_applicationSettings.windowMode == ApplicationSettings::Borderless;
    display.fullscreen = g_applicationSettings.windowMode == ApplicationSettings::Fullscreen;
    display.width = resolution.width;
    display.height = resolution.height;
    const int renderPercent = ApplicationSettings::RenderingResolutionScalePercent(
        g_applicationSettings.renderingResolutionScaleIndex);
    // Exclusive D3D9 fullscreen requires the backbuffer to match a supported
    // display mode. Windowed and borderless modes can scale an independent
    // render-sized backbuffer during presentation.
    display.renderWidth = display.fullscreen ? resolution.width :
        std::max(1, resolution.width * renderPercent / 100);
    display.renderHeight = display.fullscreen ? resolution.height :
        std::max(1, resolution.height * renderPercent / 100);
    display.antiAliasingSamples = ApplicationSettings::AntiAliasingSamples(
        g_applicationSettings.antiAliasingIndex);
    display.postProcessAntiAliasingMode =
        ApplicationSettings::ClampPostProcessAntiAliasingMode(
            g_applicationSettings.postProcessAntiAliasingMode);
    return display;
}

static void ReleaseDefaultPoolResources();
static void RecreateDefaultPoolResources();

static bool InitD3D()
{
    g_graphicsRuntime.SetDefaultPoolCallbacks(
        ReleaseDefaultPoolResources, RecreateDefaultPoolResources);
    const RendererBackend::InitializeResult result = g_graphicsRuntime.Initialize(
        g_hWnd,
        ApplicationSettings::ClampRenderingBackend(g_applicationSettings.renderingBackend),
        CurrentDisplayConfiguration());
    if (result == RendererBackend::InitializeResult::UnsupportedBackend)
    {
        MessageBoxA(g_hWnd, "The selected rendering backend is not available yet.",
                    "Rendering Backend", MB_OK | MB_ICONERROR);
        return false;
    }
    if (result == RendererBackend::InitializeResult::Direct3DUnavailable)
    {
        MessageBoxA(g_hWnd, "Direct3DCreate9 failed.", "D3D Error", MB_OK | MB_ICONERROR);
        return false;
    }
    if (result != RendererBackend::InitializeResult::Success)
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
    g_applicationSettings.renderingBackend =
        ApplicationSettings::ClampRenderingBackend(g_applicationSettings.renderingBackend);

    if (!ApplicationSettings::RenderingBackendIsAvailable(
            g_applicationSettings.renderingBackend))
    {
        g_applicationSettings.renderingBackend = ApplicationSettings::RenderingBackendDirectX9;
        ConfigDialog::Sync(g_configDialog);
    }

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
static int g_loadedZoneId = -1;
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
    g_metalworksElevatorCollision.clear();
}

static void BindMetalworksElevatorObjects()
{
    static constexpr int kRecordIndices[2] = { 422, 423 };
    static constexpr const char* kObjectNames[2] = { "liftall", "liftallb" };

    for (int platformIndex = 0; platformIndex < 2; ++platformIndex)
    {
        auto& platform = g_metalworksElevator.platforms[platformIndex];
        platform.objectName.clear();
        platform.baseY = platform.startsAtTop ? ZoneElevator::kTopY : ZoneElevator::kBottomY;

        for (size_t objectIndex = 0; objectIndex < gFF11LastMapObjects.size(); ++objectIndex)
        {
            const auto& object = gFF11LastMapObjects[objectIndex];
            std::string authoredName = object.objectName;
            authoredName.erase(authoredName.find_last_not_of(' ') + 1);
            if (object.mapRecordIndex != kRecordIndices[platformIndex] ||
                authoredName != kObjectNames[platformIndex])
                continue;

            platform.objectName = object.displayName;
            // Both lift meshes author their deck at local Y=0.
            platform.baseY = object.trans[1];
            platform.lastY = platform.baseY;
            break;
        }
    }
}

static int MetalworksElevatorPlatformForSourceTriangle(const int sourceIndex)
{
    for (int platformIndex = 0; platformIndex < 2; ++platformIndex)
    {
        const auto& platform = g_metalworksElevator.platforms[platformIndex];
        if (platform.objectName.empty())
            continue;
        for (size_t objectIndex = 0; objectIndex < gFF11LastMapObjects.size(); ++objectIndex)
        {
            const auto& object = gFF11LastMapObjects[objectIndex];
            if (platform.objectName != object.displayName)
                continue;
            const size_t source = static_cast<size_t>(sourceIndex);
            if (source >= object.visualCollisionStart &&
                source - object.visualCollisionStart < object.visualCollisionCount)
            {
                return platformIndex;
            }
        }
    }
    return -1;
}

static void ApplyMetalworksElevatorRuntimeCollision()
{
    for (const auto& binding : g_metalworksElevatorCollision)
    {
        if (binding.platform < 0 || binding.platform >= 2 ||
            binding.triangle < 0 || binding.triangle >= static_cast<int>(g_zoneCollisionTris.size()))
        {
            continue;
        }

        ZoneCollisionTriangle moved = binding.closed;
        const auto& platform = g_metalworksElevator.platforms[binding.platform];
        const float dy = platform.lastY - platform.baseY;
        for (int vertex = 0; vertex < 3; ++vertex)
            moved.p[vertex][1] += dy;
        moved.minY += dy;
        moved.maxY += dy;
        g_zoneCollisionTris[static_cast<size_t>(binding.triangle)] = moved;
    }
}

static void ApplyMetalworksElevatorRuntimeObjects()
{
    for (const auto& platform : g_metalworksElevator.platforms)
    {
        if (platform.objectName.empty())
            continue;
        // Placement is baked into zone vertices; the renderer needs only motion.
        DebugTransform transform = {};
        transform.trans[1] = platform.lastY - platform.baseY;
        transform.scale[0] = 1.0f;
        transform.scale[1] = 1.0f;
        transform.scale[2] = 1.0f;
        g_zoneRuntimeOverrides[platform.objectName] = transform;
    }
}

static void BuildZoneCollisionFromDAT(const int zoneId)
{
    ClearZoneCollision();
    if (zoneId == ZoneElevator::kMetalworksZone)
        BindMetalworksElevatorObjects();

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
        const int elevatorPlatform = zoneId == ZoneElevator::kMetalworksZone
            ? MetalworksElevatorPlatformForSourceTriangle(i) : -1;
        if (elevatorPlatform >= 0)
        {
            g_metalworksElevatorCollision.push_back(
                { elevatorPlatform, static_cast<int>(g_zoneCollisionTris.size()), tri });
        }
        g_zoneCollisionMesh.AddTriangle(tri, PlayerController::kCollisionRadius + doorPadding);
    }
    if (zoneId == ZoneElevator::kMetalworksZone)
        ApplyMetalworksElevatorRuntimeCollision();
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
    float radiusSquared = 0.0f;
    float highestVertexY = 0.0f;
    for (const noesisModel_t::Submesh &submesh : model->submeshes)
    {
        const auto includeVertex = [&](const FFXIVertex& vertex)
        {
            float vertexRadiusSquared = 0.0f;
            for (int axis = 0; axis < 3; ++axis)
                vertexRadiusSquared += vertex.pos[axis] * vertex.pos[axis];
            radiusSquared = (std::max)(radiusSquared, vertexRadiusSquared);
            highestVertexY = (std::min)(highestVertexY, vertex.pos[1]);
            haveVertex = true;
        };

        if (!submesh.cpuIndices.empty())
        {
            for (const DWORD index : submesh.cpuIndices)
                if (index < submesh.cpuVerts.size())
                    includeVertex(submesh.cpuVerts[index]);
        }
        else
        {
            for (const FFXIVertex& vertex : submesh.cpuVerts)
                includeVertex(vertex);
        }
    }
    if (!haveVertex)
        return;

    const float radius = sqrtf(radiusSquared);
    // Actor DATs use the origin near their feet. Keep that stable anchor, then
    // aim halfway toward the highest rendered vertex to center their height.
    g_camTarget[0] = g_camTarget[2] = 0.0f;
    g_camTarget[1] = highestVertexY * 0.5f;
    g_camYaw = -3.14159265f * 0.5f;
    g_camPitch = 0.0f;
    g_camDist = std::clamp(radius * 2.6f, 2.5f, 500.0f);
}

static bool SpawnPlayerAtZoneHomePoint(const unsigned int entityId = 0)
{
    // The NPC catalog includes Home Point entities at their authoritative zone
    // coordinates. Never substitute the centre of arbitrary zone geometry,
    // which is often empty space or outside the playable area. Collision is
    // used to refine the standing position when it is available, but it must
    // not reject a valid Home Point: several zones omit or incompletely export
    // collision around their Home Point object.
    for (const FFXINpcPlacement::Placement &placement : FFXINpcPlacement::Snapshot())
    {
        if (placement.name.compare(0, 10, "Home Point") != 0 ||
            (entityId != 0 && placement.entityId != entityId))
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

static bool SetClickMoveTargetFromViewportPoint(const POINT point)
{
    if (g_pickWidth <= 0 || g_pickHeight <= 0 || g_pickProjection._11 == 0.0f ||
        g_pickProjection._22 == 0.0f || g_zoneCollisionMesh.Empty())
        return false;

    const float ndcX = (2.0f * static_cast<float>(point.x) / g_pickWidth) - 1.0f;
    const float ndcY = 1.0f - (2.0f * static_cast<float>(point.y) / g_pickHeight);
    const float cameraDirection[3] =
    {
        ndcX / g_pickProjection._11,
        ndcY / g_pickProjection._22,
        1.0f
    };
    float direction[3] =
    {
        g_pickView._11 * cameraDirection[0] + g_pickView._12 * cameraDirection[1] + g_pickView._13 * cameraDirection[2],
        g_pickView._21 * cameraDirection[0] + g_pickView._22 * cameraDirection[1] + g_pickView._23 * cameraDirection[2],
        g_pickView._31 * cameraDirection[0] + g_pickView._32 * cameraDirection[1] + g_pickView._33 * cameraDirection[2]
    };
    const float length = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2]);
    if (length <= 0.0001f)
        return false;
    direction[0] /= length; direction[1] /= length; direction[2] /= length;

    float origin[3] = {};
    OrbitCamera::GetPosition(g_orbitCamera, origin[0], origin[1], origin[2]);
    float normal[3] = {};
    float pointWorld[3] = {};
    if (!ZoneCollision::Raycast(g_zoneCollisionMesh, origin, direction, 4096.0f,
                                pointWorld, normal) || std::fabs(normal[1]) < 0.25f)
        return false;
    g_clickMoveTarget[0] = pointWorld[0];
    g_clickMoveTarget[1] = pointWorld[1];
    g_clickMoveTarget[2] = pointWorld[2];
    g_clickMoveActive = true;
    return true;
}

static void DrawClickMoveTarget()
{
    if (!g_clickMoveActive ||
        g_applicationSettings.movementStyle != ApplicationSettings::MovementClickToMove ||
        !GraphicsDevice())
        return;

    struct Vertex { float x, y, z; DWORD color; };
    constexpr int segments = 20;
    const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(g_homePointSeconds) * 5.0f);
    const float outerRadius = 0.32f + pulse * 0.08f;
    const float innerRadius = outerRadius * 0.62f;
    const DWORD color = D3DCOLOR_ARGB(static_cast<int>(150.0f + pulse * 80.0f), 255, 220, 64);
    Vertex vertices[segments * 6] = {};
    constexpr float pi = 3.14159265358979323846f;
    for (int segment = 0; segment < segments; ++segment)
    {
        const float a0 = (2.0f * pi * segment) / segments;
        const float a1 = (2.0f * pi * (segment + 1)) / segments;
        const int base = segment * 6;
        const float y = g_clickMoveTarget[1] - 0.035f;
        vertices[base + 0] = {g_clickMoveTarget[0] + std::cos(a0) * innerRadius, y,
            g_clickMoveTarget[2] + std::sin(a0) * innerRadius, color};
        vertices[base + 1] = {g_clickMoveTarget[0] + std::cos(a0) * outerRadius, y,
            g_clickMoveTarget[2] + std::sin(a0) * outerRadius, color};
        vertices[base + 2] = {g_clickMoveTarget[0] + std::cos(a1) * outerRadius, y,
            g_clickMoveTarget[2] + std::sin(a1) * outerRadius, color};
        vertices[base + 3] = vertices[base + 0];
        vertices[base + 4] = vertices[base + 2];
        vertices[base + 5] = {g_clickMoveTarget[0] + std::cos(a1) * innerRadius, y,
            g_clickMoveTarget[2] + std::sin(a1) * innerRadius, color};
    }
    D3DMATRIX world = {};
    world._11 = world._22 = world._33 = world._44 = 1.0f;
    IDirect3DDevice9* device = GraphicsDevice();
    device->SetTransform(D3DTS_WORLD, &world);
    device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
    device->SetTexture(0, nullptr);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_ZENABLE, TRUE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, segments * 2, vertices, sizeof(Vertex));
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
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
    const int titleMusicIndex = (g_titleMusicIndex >= 0) ? g_titleMusicIndex : 0;
    context.titleMusicId = ApplicationSettings::TitleMusicOptionAt(
        titleMusicIndex).musicId;

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

    if (mode == InteractionController::Mode::Game)
    {
        g_highlightedZoneObjects.clear();
        ZoneObjectPanel::SetHighlightActive(g_zoneObjectPanel, false);
    }

    if (g_player.cameraActive && g_playerAsset)
        UpdatePlayerCameraTarget();

    SyncSettingsMenuChecks();
    UpdateZoneObjectEditControlState();
    SyncAppMusic();

    if (g_hWnd)
        InvalidateRect(g_hWnd, NULL, FALSE);
}

static bool EnsureGameModePlayerForCurrentZone()
{
    if (!g_zoneAsset)
        return true;
    if (g_playerAsset)
        return true;

    LoadPlayerRaceModel(g_playerEquip.raceIndex);
    if (!g_playerAsset)
        return false;

    // Player model loading initializes its pose from the editor camera. Restore
    // the zone's authoritative playable spawn after the model exists.
    SpawnPlayerAtZoneHomePoint();
    UpdatePlayerCameraTarget();
    return true;
}

static void ToggleEditGameMode()
{
    g_npcInteraction = {};
    g_doors.selected = -1;
    NpcChatWindow::Reset();
    if (IsEditMode())
    {
        if (!EnsureGameModePlayerForCurrentZone())
            return;

        // Enter gameplay at the point the editor camera is currently looking
        // at. Keep the orbit target unchanged so switching modes does not
        // move the camera to the previous player position.
        PlayerController::SetPose(
            g_player, g_camTarget[0], g_camTarget[1], g_camTarget[2], g_camYaw, false);
        PlayerController::SetRespawnPoint(g_player);
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
    PlayerController::InputSnapshot input;
    if (g_zoneMapVisible)
        return input;
    const InputController::MovementSnapshot movement = InputController::Movement(g_input);
    input.turn = -movement.right;
    input.forward = movement.forward;
    input.strafe = movement.strafe;
    input.boost = movement.running;
    input.fastRunning = movement.fastRunning;
    input.autoRun = movement.autoRun;
    input.retail = g_applicationSettings.movementStyle == ApplicationSettings::MovementRetail;
    input.cameraYaw = g_camYaw;
    input.clickToMove = g_applicationSettings.movementStyle == ApplicationSettings::MovementClickToMove &&
        g_clickMoveActive;
    input.clickTarget[0] = g_clickMoveTarget[0];
    input.clickTarget[1] = g_clickMoveTarget[1];
    input.clickTarget[2] = g_clickMoveTarget[2];
    if (input.retail)
    {
        input.strafe = movement.right;
        input.turn = 0.0f;
    }
    if (InputController::MouseForwardActive(g_input))
    {
        input.forward = 1.0f;
        input.turn = 0.0f;
        input.strafe = 0.0f;
    }
    if (input.clickToMove)
        input.forward = 1.0f;
    if (input.autoRun)
    {
        input.clickToMove = false;
        input.forward = 1.0f;
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
    if (g_applicationSettings.movementStyle == ApplicationSettings::MovementClickToMove &&
        InputController::IsMovementActive(g_input))
        g_clickMoveActive = false;
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
    if (input.clickToMove)
    {
        const float dx = g_clickMoveTarget[0] - g_player.position[0];
        const float dz = g_clickMoveTarget[2] - g_player.position[2];
        if (dx * dx + dz * dz <= 0.18f * 0.18f || movement.respawned)
            g_clickMoveActive = false;
    }
    if (!input.retail && InputController::IsMovementActive(g_input) &&
        g_input.dragMode == InputController::DragMode::None)
    {
        OrbitCamera::FollowYaw(g_orbitCamera, g_player.yaw, dt);
    }
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
    float desiredCameraTarget[3] = {};
    PlayerController::WriteCameraTarget(g_player, desiredCameraTarget);
    OrbitCamera::FollowTarget(g_orbitCamera, desiredCameraTarget, dt);
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
    // The title rail already supplied this frame's eye/target. Player follow
    // would shift both toward the gameplay actor after sampling the rail.
    if (g_titleScreenActive)
    {
        InputController::ConsumeJump(g_input);
        return;
    }
    if (g_companionViewerMode)
        return;

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

    const int minuteOfDay = ZoneEnvironmentState::CurrentMinuteOfDay();
    const bool useNight = minuteOfDay >= 18 * 60 || minuteOfDay < 6 * 60;
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
                                 gFF11LastEnvironmentRecords,
                                 ZoneEnvironmentState::CurrentMinuteOfDay());
}

static void GetRenderCameraPosition(float &x, float &y, float &z)
{
    OrbitCamera::GetPosition(g_orbitCamera, x, y, z);
}

static bool FindZoneShadowReceiverY(const float x, const float y, const float z, float* const outY)
{
    if (!outY || g_zoneCollisionTris.empty())
        return false;

    float floorY = 0.0f;
    float floorNormal[3] = {};
    const float searchRadius = g_applicationSettings.shadowReceiverQuality <= 0 ? 1.5f :
        (g_applicationSettings.shadowReceiverQuality == 1 ? 0.75f : 0.25f);
    if (!ZoneCollision::FindFloorAt(g_zoneCollisionTris, g_zoneCollisionGrid,
                                    searchRadius, x, z, y - 80.0f, y + 80.0f,
                                    &floorY, floorNormal))
    {
        return false;
    }

    *outY = floorY;
    return true;
}

static void RenderModel(noesisModel_t *pModel, bool allowObjectOverrides = false,
                        ModelRenderer::GeometryPass pass = ModelRenderer::GeometryPass::All,
                        bool drawDynamicActorShadow = true)
{
    if (!pModel || !GraphicsDevice())
        return;

    CustomTextureAssets::Configure(GraphicsDevice(),
        g_applicationSettings.enableHdTextures, g_applicationSettings.hdTextureFolder,
        g_applicationSettings.enablePbr, g_applicationSettings.pbrTextureFolder);

    ModelRenderer::Context rendererContext = {};
    rendererContext.device = GraphicsDevice();
    rendererContext.geometryPass = pass;
    rendererContext.minuteOfDay = ZoneEnvironmentState::CurrentMinuteOfDay();
    rendererContext.lightingQuality = g_applicationSettings.lightingQuality;
    rendererContext.automaticLightDirection =
        g_applicationSettings.lightDirectionMode == ApplicationSettings::LightDirectionAutomatic;
    rendererContext.lightAzimuthDegrees = static_cast<float>(
        rendererContext.automaticLightDirection
            ? g_applicationSettings.lightAzimuthOffsetDegrees
            : g_applicationSettings.lightAzimuthDegrees);
    rendererContext.lightElevationDegrees = static_cast<float>(
        rendererContext.automaticLightDirection
            ? g_applicationSettings.lightElevationOffsetDegrees
            : g_applicationSettings.lightElevationDegrees);
    if (g_companionViewerMode)
    {
        rendererContext.automaticLightDirection = false;
        rendererContext.lightAzimuthDegrees = g_modelViewerLightAzimuthDegrees;
        rendererContext.lightElevationDegrees = g_modelViewerLightElevationDegrees;
    }
    rendererContext.useAuthoredLightColor = g_zoneEnvironment.valid;
    if (allowObjectOverrides)
    {
        rendererContext.authoredMainLightColor = g_zoneEnvironment.terrainMainLightColor;
        rendererContext.authoredSecondaryLightColor = g_zoneEnvironment.terrainSecondaryLightColor;
        rendererContext.authoredAmbientLightColor = g_zoneEnvironment.terrainAmbientColor;
        rendererContext.authoredLightPower = g_zoneEnvironment.terrainLightPower;
        memcpy(rendererContext.authoredMainLightDirection,
            g_zoneEnvironment.terrainMainLightDirection,
            sizeof(rendererContext.authoredMainLightDirection));
    }
    else
    {
        rendererContext.authoredMainLightColor = g_zoneEnvironment.modelMainLightColor;
        rendererContext.authoredSecondaryLightColor = g_zoneEnvironment.modelSecondaryLightColor;
        rendererContext.authoredAmbientLightColor = g_zoneEnvironment.modelAmbientColor;
        rendererContext.authoredLightPower = g_zoneEnvironment.modelLightPower;
        memcpy(rendererContext.authoredMainLightDirection,
            g_zoneEnvironment.modelMainLightDirection,
            sizeof(rendererContext.authoredMainLightDirection));
    }
    rendererContext.useAuthoredLightDirection =
        g_zoneEnvironment.valid && g_zoneEnvironment.authoredLightDirection;
    rendererContext.useAuthoredFog = allowObjectOverrides && g_zoneEnvironment.valid;
    rendererContext.authoredFogColor = g_zoneEnvironment.fogColor;
    rendererContext.authoredFogNear = g_zoneEnvironment.fogNear;
    rendererContext.authoredFogFar = g_zoneEnvironment.fogFar;
    rendererContext.authoredGenerators = &gFF11LastGeneratorRecords;
    rendererContext.authoredKeyframes = &gFF11LastKeyframeRecords;
    rendererContext.mirrorAuthoredLightX = !g_applicationSettings.mirrorWorldZones;
    rendererContext.vegetationAnimationMode = g_applicationSettings.environmentalAnimationMode;
    rendererContext.rendersZoneObjects = allowObjectOverrides;
    rendererContext.dynamicActorShadows =
        !g_companionViewerMode && drawDynamicActorShadow &&
        g_applicationSettings.lightingQuality == ApplicationSettings::LightingDynamicShadows;
    rendererContext.dynamicObjectShadows = rendererContext.dynamicActorShadows &&
        g_applicationSettings.shadowObjects;
    rendererContext.shadowGroundLikeObjects = g_applicationSettings.shadowGroundLikeObjects;
    rendererContext.shadowAlphaTestedObjects = g_applicationSettings.shadowAlphaTestedObjects;
    rendererContext.shadowMaxDistance = static_cast<float>((std::max)(0, g_applicationSettings.shadowMaxDistance));
    rendererContext.shadowObjectLimit = (std::max)(0, g_applicationSettings.shadowObjectLimit);
    rendererContext.shadowMinimumSize = (std::max)(0, g_applicationSettings.shadowMinimumSizePercent) * 0.01f;
    rendererContext.shadowMaximumSize = static_cast<float>((std::max)(0, g_applicationSettings.shadowMaximumSize));
    rendererContext.shadowReceiverUpdateFrames = (std::max)(1, g_applicationSettings.shadowReceiverUpdateFrames);
    rendererContext.shadowMaximumLength = static_cast<float>((std::max)(1, g_applicationSettings.shadowMaximumLength));
    rendererContext.shadowOpacity = std::clamp(g_applicationSettings.shadowOpacityPercent, 0, 100) * 0.01f;
    rendererContext.shadowDebugVisualization = g_applicationSettings.shadowDebugVisualization;
    rendererContext.shadowPerformanceCounters = g_applicationSettings.shadowPerformanceCounters;
    rendererContext.enableMipMapping = g_applicationSettings.enableMipMapping;
    rendererContext.enableBumpMapping = g_applicationSettings.enableBumpMapping;
    rendererContext.bumpMappingIntensity =
        ApplicationSettings::ClampBumpMappingIntensity(
            g_applicationSettings.bumpMappingIntensityPercent) * 0.01f *
        (g_applicationSettings.invertBumpMapping ? -1.0f : 1.0f);
    rendererContext.indoorZone = g_zoneEnvironment.valid && g_zoneEnvironment.indoor;
    rendererContext.findZoneShadowReceiverY = FindZoneShadowReceiverY;
    GetRenderCameraPosition(rendererContext.cameraPosition[0],
        rendererContext.cameraPosition[1], rendererContext.cameraPosition[2]);
    ZoneModelRenderMetadata::Prepare(pModel, GraphicsDevice());
    GraphicsDevice()->SetFVF(FFXI_VERTEX_FVF);
    D3DMATRIX baseWorld = {};
    GraphicsDevice()->GetTransform(D3DTS_WORLD, &baseWorld);

    // The dynamic tier adds an inexpensive planar pass for standalone actors. Zone terrain
    // has uneven ground, so projecting an entire zone onto a single plane would be wrong.
    ModelRenderer::DrawActorPlanarShadow(rendererContext, pModel, baseWorld);
    ModelRenderer::PrepareFixedFunctionPass(rendererContext, baseWorld);

    rendererContext.visibility.hiddenNames = &g_hiddenZoneObjects;
    rendererContext.visibility.viewerPointValid = g_zoneVisibilityViewerPointValid;
    rendererContext.visibility.lodViewerPoint = g_zoneVisibilityViewerPoint;
    rendererContext.visibility.visibleMapObjects = &g_zoneVisibleMapObjects;
    rendererContext.visibility.overrides = &g_zoneRuntimeOverrides;
    rendererContext.visibility.frustum = &g_zoneRenderFrustum;
    ModelRenderer::DrawGeometry(rendererContext, pModel, baseWorld);
    if (allowObjectOverrides && pass == ModelRenderer::GeometryPass::Opaque)
        ModelRenderer::DrawZoneObjectPlanarShadows(rendererContext, pModel, baseWorld);
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
    g_metalworksElevatorAudio.Reset();
    g_loadedZoneId = -1;
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
    const int zoneId = FFXIPath::FindZoneIDByModelPath(g_ffxiPath, path);
    g_loadedZoneId = zoneId;
    g_metalworksElevator = {};
    g_metalworksElevatorAudio.Reset();
    if (zoneId == ZoneElevator::kMetalworksZone)
        g_metalworksElevatorAudio.Load(g_ffxiPath);
    g_doors.Initialize(g_zoneAsset.Model(), !g_applicationSettings.mirrorWorldZones);
    g_doors.SetPhysics(g_applicationSettings.doorInteractionMode == ApplicationSettings::DoorPhysics);
    BuildZoneCollisionFromDAT(zoneId);
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
    const auto* music = FFXIZoneMusic_FindByID(line.toZone);
    SetGameModeMusic(SelectZoneMusicId(music));
    const std::string zoneEntryMessage = ZoneEntryMessage::ForZone(line.toZone);
    if (!zoneEntryMessage.empty())
        NpcChatWindow::Show(g_hWnd, "System", zoneEntryMessage);
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

static int SelectTitleBackgroundIndex()
{
    const int count = ApplicationSettings::TitleBackgroundOptionCount();
    if (count <= 1)
        return 0;

    switch (ApplicationSettings::ClampTitleBackgroundMode(
        g_applicationSettings.titleBackgroundMode))
    {
    case ApplicationSettings::TitleBackgroundCycle:
        g_titleCycleIndex = (g_titleCycleIndex + 1) % count;
        return g_titleCycleIndex;
    case ApplicationSettings::TitleBackgroundFixed:
        return ApplicationSettings::ClampTitleBackgroundZoneIndex(
            g_applicationSettings.titleBackgroundZoneIndex);
    default:
        {
            static std::mt19937 generator(static_cast<unsigned int>(
                GetTickCount64() ^ reinterpret_cast<UINT_PTR>(&SelectTitleBackgroundIndex)));
            if (g_titleBackgroundIndex < 0)
            {
                std::uniform_int_distribution<int> firstDistribution(0, count - 1);
                return firstDistribution(generator);
            }
            std::uniform_int_distribution<int> distribution(0, count - 2);
            int selected = distribution(generator);
            // Draw from count-1 values, then skip the current entry. This
            // guarantees a visibly different scene on every return without
            // biasing any of the remaining choices.
            if (g_titleBackgroundIndex >= 0 && selected >= g_titleBackgroundIndex)
                ++selected;
            return selected;
        }
    }
}

static int SelectTitleMusicIndex()
{
    const int count = ApplicationSettings::TitleMusicOptionCount();
    if (count <= 1)
        return 0;

    switch (ApplicationSettings::ClampTitleMusicMode(
        g_applicationSettings.titleMusicMode))
    {
    case ApplicationSettings::TitleMusicCycle:
        g_titleMusicCycleIndex = (g_titleMusicCycleIndex + 1) % count;
        return g_titleMusicCycleIndex;
    case ApplicationSettings::TitleMusicFixed:
        return ApplicationSettings::ClampTitleMusicIndex(
            g_applicationSettings.titleMusicIndex);
    default:
        {
            static std::mt19937 generator(static_cast<unsigned int>(
                GetTickCount64() ^ reinterpret_cast<UINT_PTR>(&SelectTitleMusicIndex)));
            if (g_titleMusicIndex < 0)
            {
                std::uniform_int_distribution<int> firstDistribution(0, count - 1);
                return firstDistribution(generator);
            }
            std::uniform_int_distribution<int> distribution(0, count - 2);
            int selected = distribution(generator);
            if (selected >= g_titleMusicIndex)
                ++selected;
            return selected;
        }
    }
}

static void ConfigureGenericTitleCameraBase()
{
    if (!g_haveZoneCollisionBounds || g_zoneCollisionTris.empty())
        return;

    const float centerX = (g_zoneCollisionMin[0] + g_zoneCollisionMax[0]) * 0.5f;
    const float centerZ = (g_zoneCollisionMin[2] + g_zoneCollisionMax[2]) * 0.5f;
    const float extentX = (g_zoneCollisionMax[0] - g_zoneCollisionMin[0]) * 0.5f;
    const float extentZ = (g_zoneCollisionMax[2] - g_zoneCollisionMin[2]) * 0.5f;
    constexpr float ringFractions[] = { 0.0f, 0.08f, 0.16f, 0.26f, 0.38f };

    bool found = false;
    float anchorX = centerX;
    float anchorY = 0.0f;
    float anchorZ = centerZ;
    for (const float fraction : ringFractions)
    {
        const int samples = fraction == 0.0f ? 1 : 16;
        for (int sample = 0; sample < samples; ++sample)
        {
            const float angle = 6.28318530718f * static_cast<float>(sample) /
                static_cast<float>(samples);
            const float x = centerX + std::cos(angle) * extentX * fraction;
            const float z = centerZ + std::sin(angle) * extentZ * fraction;
            float floorY = 0.0f;
            float normal[3] = {};
            if (ZoneCollision::FindFloorAt(g_zoneCollisionTris, g_zoneCollisionGrid,
                    2.0f, x, z, g_zoneCollisionMin[1] - 2.0f,
                    g_zoneCollisionMax[1] + 2.0f, &floorY, normal) &&
                normal[1] >= 0.55f)
            {
                anchorX = x;
                anchorY = floorY;
                anchorZ = z;
                found = true;
                break;
            }
        }
        if (found)
            break;
    }
    if (!found)
        return;

    // Scene Y increases downward. Put the look target just above the sampled
    // floor and the camera a little higher still, instead of using the global
    // collision minimum (the tallest peak anywhere in the zone).
    g_camTarget[0] = anchorX;
    g_camTarget[1] = anchorY - 2.0f;
    g_camTarget[2] = anchorZ;
    g_camYaw = 0.0f;
    g_camPitch = -0.04f;
    g_camDist = 45.0f;
}

static void LoadTitleScreen()
{
    UnloadCreationModel();
    UnloadTitleAssets();
    InteractionController::SetGameMusicId(g_interaction, 0);
    g_highPolyCreationActive = false;
    g_nationSelectActive = false;
    g_titleCameraRailTime = 0.0f;
    g_retailTitleCamera = {};
    g_titleMusicIndex = SelectTitleMusicIndex();

    // The original PS2 lobby deliberately presents its title demo at 12:30,
    // independent of the live in-game clock.
    ZoneEnvironmentState::SetTimeOverride(12 * 60 + 30);

    SceneLoadContext::Options backdropOptions;
    backdropOptions.preserveGameScreen = true;
    g_titleBackgroundIndex = SelectTitleBackgroundIndex();
    const ApplicationSettings::TitleBackgroundOption* background =
        &ApplicationSettings::TitleBackgroundOptionAt(g_titleBackgroundIndex);
    SceneRequestResolver::Result backdrop =
        SceneRequestResolver::ResolveRelativeModel(
            g_ffxiPath, background->modelDat, background->name, backdropOptions);
    if (!backdrop.Succeeded())
    {
        // A partial FFXI installation may not contain every expansion DAT.
        // Fall back to DATura's known-good Konschtat presentation rather than
        // leaving the title screen without a backdrop.
        g_titleBackgroundIndex = 8;
        background = &ApplicationSettings::TitleBackgroundOptionAt(
            g_titleBackgroundIndex);
        backdrop = SceneRequestResolver::ResolveRelativeModel(
            g_ffxiPath, background->modelDat, background->name, backdropOptions);
        if (!backdrop.Succeeded())
        {
            g_titleScreenActive = true;
            return;
        }
    }

    // Load the selected backdrop through the regular-zone content path
    // else (environment, collision, NPC placements, and ordinary visibility).
    // Only suppress the UI transition that would otherwise leave the title
    // screen and unload the title-specific overlay assets.
    LoadDatFile(backdrop.request);

    // The logo/atlas/UI DAT loads below replace global parser diagnostics.
    // Keep the backdrop zone's authored 0x2F and 0x05 records alive.
    {
        FFXIParserDiagnostics::ScopedSnapshot parserDiagnostics;
        g_titleScreenActive = true;

        // Preserve DATura's custom Konschtat composition. Every retail zone
        // starts on its exact authored scheduler camera; collision-derived
        // framing is retained only as a safe fallback for incomplete installs.
        if (g_titleBackgroundIndex == 8)
        {
            for (int axis = 0; axis < 3; ++axis)
                g_camTarget[axis] = kTitleScreenCameraTarget[axis];
            g_camYaw = kTitleScreenCameraYaw;
            g_camPitch = kTitleScreenCameraPitch;
            g_camDist = kTitleScreenCameraDistance;
        }
        else
        {
            if (!LoadRetailTitleCamera(
                    RetailTitleScheduleForBackground(g_titleBackgroundIndex)) ||
                !SampleRetailTitleCamera())
            {
                ConfigureGenericTitleCameraBase();
            }
        }
        for (int axis = 0; axis < 3; ++axis)
            g_titleCameraBaseTarget[axis] = g_camTarget[axis];
        g_titleCameraBaseYaw = g_camYaw;
        g_titleCameraBasePitch = g_camPitch;
        g_titleCameraBaseDistance = g_camDist;

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

    // Prefer an authored sunny sky when the selected zone provides one. The
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

static void LoadCreationEntryInModelViewer(const FFXICreationEntry *pEntry)
{
    if (!pEntry)
        return;

    g_titleScreenActive = false;
    g_highPolyCreationActive = true;
    g_nationSelectActive = false;
    UnloadTitleAssets();
    UnloadZoneModel();
    UnloadPlayerModel();
    UnloadCreationModel();

    CreationModelLoader::Request request;
    request.ffxiRoot = g_ffxiPath;
    request.label = pEntry->label;
    request.bodyMeshDat = pEntry->bodyMeshDat;
    request.bodyMaterialDat = pEntry->bodyMaterialDat;
    request.headMeshDat = pEntry->headMeshDat;
    request.headMaterialDat = pEntry->headMaterialDat;
    request.bodyAnimationDat = CreationBodyAnimDatForRace(g_creationSelection.raceIndex);
    request.headAnimationDat = CreationHeadAnimDatForRace(g_creationSelection.raceIndex);
    request.headAlphaMode = pEntry->headAlphaMode;
    request.headYOffset = pEntry->headYOffset;
    request.enableTextureCompression = g_applicationSettings.enableTextureCompression;

    CreationModelLoader::Result result = CreationModelLoader::Load(GraphicsDevice(), request);
    if (!result.Succeeded())
    {
        MessageBoxA(g_hWnd, "Could not load the selected character model.",
                    "Model Viewer", MB_OK | MB_ICONWARNING);
        return;
    }

    g_creationAsset = std::move(result.asset);
    g_creationAnimationIndex = 0;
    g_creationAnimatedCamera = false;
    g_creationBodyMotion = {};
    g_creationHeadMotion = {};
    g_creationAnimTime = 0.0f;
    g_creationHorizontalPlacement[0] = 0.0f;
    g_creationHorizontalPlacement[1] = 0.0f;
    g_creationHorizontalPlacement[2] = 0.0f;
    ResetCameraForStandaloneModel(g_creationAsset.Model());
    InvalidateRect(g_hWnd, NULL, FALSE);
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

static void DumpHelmutAttachmentDiagnostics(const FFXINpcPlacement::Placement &placement,
                                            const NpcRenderAsset &asset)
{
    static std::uint32_t dumpedEntityId = 0;
    if (dumpedEntityId == placement.entityId || placement.entityId != 17748002 || !asset.resource.Model())
        return;
    dumpedEntityId = placement.entityId;

    char logPath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, logPath, MAX_PATH))
    {
        char *slash = std::strrchr(logPath, '\\');
        if (!slash)
            slash = std::strrchr(logPath, '/');
        if (slash)
            slash[1] = 0;
        else
            logPath[0] = 0;
        strcat_s(logPath, sizeof(logPath), "npc_attachment_diagnostics.log");
    }
    FILE *logFile = nullptr;
    if (logPath[0])
        fopen_s(&logFile, logPath, "ab");

    const auto writeDiagnostic = [&](const char *text)
    {
        OutputDebugStringA(text);
        if (logFile)
            std::fputs(text, logFile);
    };

    noesisModel_t *model = asset.resource.Model();
    char header[256] = {};
    std::snprintf(header, sizeof(header),
                  "[DATura NPC attach] entity=%u name=%s submeshes=%zu\n",
                  placement.entityId, placement.name.c_str(), model->submeshes.size());
    writeDiagnostic(header);

    for (size_t meshIndex = 0; meshIndex < model->submeshes.size(); ++meshIndex)
    {
        const noesisModel_t::Submesh &submesh = model->submeshes[meshIndex];
        const std::vector<FFXIVertex> &vertices =
            !submesh.cpuBindVerts.empty() ? submesh.cpuBindVerts : submesh.cpuVerts;

        float minPos[3] = {
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()
        };
        float maxPos[3] = {
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest()
        };
        for (const FFXIVertex &vertex : vertices)
        {
            for (int axis = 0; axis < 3; ++axis)
            {
                minPos[axis] = std::min(minPos[axis], vertex.pos[axis]);
                maxPos[axis] = std::max(maxPos[axis], vertex.pos[axis]);
            }
        }
        if (vertices.empty())
            memset(minPos, 0, sizeof(minPos)), memset(maxPos, 0, sizeof(maxPos));

        const float center[3] = {
            (minPos[0] + maxPos[0]) * 0.5f,
            (minPos[1] + maxPos[1]) * 0.5f,
            (minPos[2] + maxPos[2]) * 0.5f
        };

        char line[1024] = {};
        std::snprintf(line, sizeof(line),
                      "[DATura NPC attach] mesh=%zu object=\"%s\" material=\"%s\" "
                      "verts=%zu bindVerts=%zu skinVerts=%zu "
                      "min=(%.3f, %.3f, %.3f) max=(%.3f, %.3f, %.3f) "
                      "center=(%.3f, %.3f, %.3f)\n",
                      meshIndex, submesh.objectName.c_str(), submesh.materialName.c_str(),
                      submesh.cpuVerts.size(), submesh.cpuBindVerts.size(),
                      submesh.cpuSkinVerts.size(),
                      minPos[0], minPos[1], minPos[2],
                      maxPos[0], maxPos[1], maxPos[2],
                      center[0], center[1], center[2]);
        writeDiagnostic(line);
    }
    if (logFile)
        std::fclose(logFile);
}

static void UnloadNpcModels()
{
    g_homePointSeconds = 0.0;
    g_npcInteraction = {};
    g_npcConversation = {};
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
            DumpHelmutAttachmentDiagnostics(placement, g_npcRenderAssets[assetIndex]);
            const float initialHeading = scenePlacement.transform.headingRadians;
            g_npcRenderInstances.push_back(
                { std::move(scenePlacement), assetIndex, 0.0, initialHeading, true });
        }
    }
}

static float NormalizeRadians(float angle)
{
    constexpr float twoPi = 6.2831853071795864769f;
    while (angle > 3.14159265358979323846f) angle -= twoPi;
    while (angle < -3.14159265358979323846f) angle += twoPi;
    return angle;
}

static float MoveAngleToward(float current, float target, float maxStep)
{
    const float delta = NormalizeRadians(target - current);
    if (std::fabs(delta) <= maxStep)
        return target;
    return NormalizeRadians(current + (delta < 0.0f ? -maxStep : maxStep));
}

static float NpcFacingTargetHeading(const NpcRenderInstance& npc)
{
    const auto& transform = npc.placement.transform;
    if (g_npcConversation.entityId == npc.placement.entityId)
    {
        const float dx = g_player.position[0] - transform.x;
        const float dz = g_player.position[2] - transform.z;
        if (dx * dx + dz * dz > 0.0001f)
            return std::atan2f(-dz, dx);
    }
    return transform.headingRadians;
}

static void UpdateNpcFacing(float dt)
{
    if (!std::isfinite(dt) || dt <= 0.0f)
        return;
    constexpr float kNpcTurnRadiansPerSecond = 2.75f;
    const float maxStep = kNpcTurnRadiansPerSecond * dt;
    for (NpcRenderInstance& npc : g_npcRenderInstances)
    {
        if (!npc.renderedHeadingInitialized)
        {
            npc.renderedHeadingRadians = npc.placement.transform.headingRadians;
            npc.renderedHeadingInitialized = true;
        }
        npc.renderedHeadingRadians = MoveAngleToward(
            npc.renderedHeadingRadians, NpcFacingTargetHeading(npc), maxStep);
    }
}

static void UpdateNpcAnimations(float dt)
{
    if (std::isfinite(dt) && dt > 0) g_homePointSeconds += dt;
    UpdateNpcFacing(dt);
    if (!GraphicsDevice())
        return;
    for (size_t assetIndex = 0; assetIndex < g_npcRenderAssets.size(); ++assetIndex)
    {
        NpcRenderAsset &asset = g_npcRenderAssets[assetIndex];
        if (!asset.resource || !asset.visibleLastFrame)
            continue;

        noesisModel_t *model = asset.resource.Model();
        const bool isSpeaking = g_npcConversation.entityId != 0 &&
            std::any_of(g_npcRenderInstances.begin(), g_npcRenderInstances.end(),
                [assetIndex](const NpcRenderInstance &instance)
                {
                    return instance.assetIndex == assetIndex &&
                           instance.placement.visible &&
                           instance.placement.entityId == g_npcConversation.entityId;
                });
        noesisAnim_t *previousClip = model ? model->pAnim : nullptr;
        const bool haveAnimation = isSpeaking
            ? NpcRenderGeometry::SelectTalkAnimation(model)
            : NpcRenderGeometry::SelectIdleAnimation(model);
        if (!haveAnimation)
        {
            if (model)
                model->RestoreBindPose(GraphicsDevice());
            asset.animationTime = 0.0f;
            continue;
        }
        if (model && model->pAnim != previousClip)
            asset.animationTime = 0.0f;
        asset.animationTime += dt;
        model->UpdateAnimation(asset.animationTime, GraphicsDevice());
        asset.nameplateLocalY =
            NpcRenderGeometry::ComputeNameplateLocalY(model);
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

    char dataPath[MAX_PATH] = {};
    FFXIFileIO::ReplaceExtension(datSetPath, ".ini", dataPath, sizeof(dataPath));
    CharacterSaveData::Data characterData;
    characterData.homeNationIndex = 0;
    CharacterSaveData::Data existingData;
    if (CharacterSaveData::Load(dataPath, existingData))
        characterData = existingData;
    if (!CharacterSaveData::Save(dataPath, characterData))
    {
        MessageBoxA(panel ? panel : g_hWnd,
                    "The character model was saved, but its character data file could not be created.",
                    "Save Character", MB_OK | MB_ICONWARNING);
    }
    g_lastSavedCharacterDataPath = dataPath;

    FFXISaveResultDialog::Show(g_highPolyCreationActive && g_hWnd ? g_hWnd : NULL,
                               g_hWnd, noesisPath, datSetPath);
}

static bool FileExists(const char* path)
{
    const DWORD attributes = path && path[0] ? GetFileAttributesA(path) : INVALID_FILE_ATTRIBUTES;
    return attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static bool EnsureActiveCharacterModelFiles()
{
    if (g_activeCharacterDataPath.empty())
        return false;

    char datSetPath[MAX_PATH] = {};
    char noesisPath[MAX_PATH] = {};
    FFXIFileIO::ReplaceExtension(g_activeCharacterDataPath.c_str(), ".ff11datset",
        datSetPath, sizeof(datSetPath));
    FFXIFileIO::ReplaceExtension(g_activeCharacterDataPath.c_str(), ".noesis",
        noesisPath, sizeof(noesisPath));

    if (!FileExists(datSetPath))
    {
        FFXIDatSet::PlayerOptions options;
        options.raceIndex = g_playerEquip.raceIndex;
        options.faceVariant = g_playerFaceVariant;
        options.animationBank = g_playerEquip.animationBank;
        options.headItem = g_playerEquip.headItem;
        options.bodyItem = g_playerEquip.bodyItem;
        options.handsItem = g_playerEquip.handsItem;
        options.legsItem = g_playerEquip.legsItem;
        options.feetItem = g_playerEquip.feetItem;
        options.mainItem = g_playerEquip.mainItem;
        options.subItem = g_playerEquip.subItem;
        options.rangedItem = g_playerEquip.rangedItem;

        char datSetText[4096] = {};
        if (!FFXIDatSet::BuildPlayer(g_ffxiPath, options, datSetText, sizeof(datSetText)) ||
            !FFXIFileIO::WriteTextFile(datSetPath, datSetText))
            return false;
    }

    if (!FileExists(noesisPath))
    {
        const char* fileName = strrchr(datSetPath, '\\');
        fileName = fileName ? fileName + 1 : datSetPath;
        if (strchr(fileName, '"'))
            return false;

        char noesisText[1024] = {};
        sprintf_s(noesisText,
            "NOESIS_SCENE_FILE\n"
            "version 1\n"
            "physicslib\t\t\"\"\n"
            "defaultAxis\t\t\"0\"\n\n"
            "object\n"
            "{\n"
            "\tname\t\t\t\"character\"\n"
            "\tmodel\t\t\t\"%s\"\n"
            "}\n",
            fileName);
        if (!FFXIFileIO::WriteTextFile(noesisPath, noesisText))
            return false;
    }
    return true;
}

static bool LoadOrCreateActiveCharacterData(CharacterSaveData::Data& data)
{
    if (g_activeCharacterDataPath.empty())
        return false;
    if (CharacterSaveData::Load(g_activeCharacterDataPath.c_str(), data))
    {
        g_activeCharacterHomeNationIndex = data.homeNationIndex;
        return true;
    }
    if (FileExists(g_activeCharacterDataPath.c_str()))
        return false;

    data = {};
    data.homeNationIndex = std::clamp(g_activeCharacterHomeNationIndex, 0, 2);
    return true;
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

enum class NpcEventStep
{
    Done,
    Message,
    Choice,
    Blocked,
};

static std::uint16_t NpcEventRead16(const FFXIEventTable::Event& event, std::size_t at)
{
    if (at + 2 > event.code.size()) return 0;
    return static_cast<std::uint16_t>(event.code[at] | (event.code[at + 1] << 8));
}

static std::uint32_t NpcEventResolveOperand(const FFXIEventTable::Event& event, std::uint16_t operand)
{
    return (operand & 0x8000u) != 0 && (operand & 0x7fffu) < event.references.size()
        ? event.references[operand & 0x7fffu]
        : operand;
}

static std::int32_t NpcEventGetWork(NpcEventRuntime& runtime, std::size_t operandOffset, std::int32_t shift = 0)
{
    if (!runtime.event) return 0;
    const std::uint32_t operand = static_cast<std::uint32_t>(
        static_cast<std::int32_t>(NpcEventRead16(*runtime.event, runtime.pc + operandOffset)) + shift);
    if ((operand & 0x8000u) != 0)
        return static_cast<std::int32_t>(NpcEventResolveOperand(*runtime.event, static_cast<std::uint16_t>(operand)));
    if (operand >= 4096u)
    {
        const std::size_t index = operand - 4096u;
        return index < runtime.zone.size() ? static_cast<std::int32_t>(runtime.zone[index]) : 0;
    }
    return operand < runtime.local.size() ? static_cast<std::int32_t>(runtime.local[operand]) : 0;
}

static void NpcEventSetWork(NpcEventRuntime& runtime, std::size_t operandOffset, std::uint32_t value, std::int32_t shift = 0)
{
    if (!runtime.event) return;
    const std::uint32_t operand = static_cast<std::uint32_t>(
        static_cast<std::int32_t>(NpcEventRead16(*runtime.event, runtime.pc + operandOffset)) + shift);
    if (operand >= 4096u)
    {
        const std::size_t index = operand - 4096u;
        if (index < runtime.zone.size()) runtime.zone[index] = value;
    }
    else if (operand < runtime.local.size())
        runtime.local[operand] = value;
}

static std::size_t NpcEventFixedSize(std::uint8_t op)
{
    switch (op)
    {
    case 0x00: case 0x21: case 0x23: case 0x25: case 0x58: case 0x7f:
        return 1;
    case 0x43:
        return 2;
    case 0x01: case 0x05: case 0x06: case 0x0b: case 0x0c:
    case 0x1a: case 0x1c: case 0x1d: case 0x34: case 0x35: case 0x48: case 0x63:
        return 3;
    case 0x03: case 0x6f: case 0x76: case 0x80: case 0x99:
        return 5;
    case 0x07: case 0x08: case 0x09: case 0x0a: case 0x0d: case 0x0e:
    case 0x0f: case 0x10: case 0x11: case 0x14: case 0x15: case 0x19:
    case 0x24: case 0x27: case 0x28: case 0x29: case 0x2a: case 0x2b:
    case 0x3e: case 0x49: case 0x6e:
        return 7;
    case 0x02: case 0x40: case 0x41: case 0x4e: case 0x6c:
        return 9;
    case 0x2c: case 0xb0:
        return 12;
    case 0x45:
        return 17;
    case 0x46:
        return 2;
    case 0x53: case 0x54: case 0x55: case 0x5b: case 0x66:
        return 9;
    case 0x5d:
        return 5;
    case 0x70:
        return 1;
    default:
        return 0;
    }
}

static void NpcEventBinaryOp(NpcEventRuntime& runtime, std::uint8_t op)
{
    const std::uint32_t a = static_cast<std::uint32_t>(NpcEventGetWork(runtime, 1));
    const std::uint32_t b = static_cast<std::uint32_t>(NpcEventGetWork(runtime, 3));
    std::uint32_t value = 0;
    switch (op)
    {
    case 0x07: value = a + b; break;
    case 0x08: value = a - b; break;
    case 0x09: value = a | (1u << (b & 31)); break;
    case 0x0a: value = a & ~(1u << (b & 31)); break;
    case 0x0d: value = a & b; break;
    case 0x0e: value = a | b; break;
    case 0x0f: value = a ^ b; break;
    case 0x10: value = a << (b & 31); break;
    case 0x11: value = a >> (b & 31); break;
    case 0x14: value = a * b; break;
    case 0x15: value = (a && b) ? a / b : 0; break;
    }
    NpcEventSetWork(runtime, 1, value);
    runtime.pc += 5;
}

static void NpcEventIf(NpcEventRuntime& runtime)
{
    if (!runtime.event) return;
    const std::uint8_t kind = runtime.pc + 5 < runtime.event->code.size()
        ? runtime.event->code[runtime.pc + 5] & 0x0f : 0;
    const std::int32_t a = NpcEventGetWork(runtime, 1);
    const std::int32_t b = NpcEventGetWork(runtime, 3);
    const std::size_t target = NpcEventRead16(*runtime.event, runtime.pc + 6);
    bool take = true;
    switch (kind)
    {
    case 0: take = a != b; break;
    case 1: case 7: take = a == b; break;
    case 2: take = a <= b; break;
    case 3: take = a >= b; break;
    case 4: take = a < b; break;
    case 5: take = a > b; break;
    case 6: case 9: take = ((std::uint32_t)b & (std::uint32_t)a) == 0; break;
    case 8: take = ((std::uint32_t)a | (std::uint32_t)b) == 0; break;
    case 10: take = (~(std::uint32_t)a & (std::uint32_t)b) == 0; break;
    }
    runtime.pc = take ? target : runtime.pc + 8;
}

static NpcEventStep StepNpcEvent(NpcEventRuntime& runtime)
{
    if (!runtime.event || runtime.finished)
        return NpcEventStep::Done;
    for (int budget = 0; budget < 100000; ++budget)
    {
        if (runtime.pc >= runtime.event->code.size())
        {
            runtime.finished = true;
            return NpcEventStep::Done;
        }
        const std::uint8_t op = runtime.event->code[runtime.pc];
        switch (op)
        {
        case 0x00: case 0x21:
            runtime.finished = true;
            return NpcEventStep::Done;
        case 0x01:
            runtime.pc = NpcEventRead16(*runtime.event, runtime.pc + 1);
            break;
        case 0x02:
            NpcEventIf(runtime);
            break;
        case 0x03:
            NpcEventSetWork(runtime, 1, static_cast<std::uint32_t>(NpcEventGetWork(runtime, 3)));
            runtime.pc += 5;
            break;
        case 0x05:
            NpcEventSetWork(runtime, 1, 1); runtime.pc += 3; break;
        case 0x06:
            NpcEventSetWork(runtime, 1, 0); runtime.pc += 3; break;
        case 0x0b:
            NpcEventSetWork(runtime, 1, static_cast<std::uint32_t>(NpcEventGetWork(runtime, 1) + 1)); runtime.pc += 3; break;
        case 0x0c:
            NpcEventSetWork(runtime, 1, static_cast<std::uint32_t>(NpcEventGetWork(runtime, 1) - 1)); runtime.pc += 3; break;
        case 0x07: case 0x08: case 0x09: case 0x0a: case 0x0d: case 0x0e:
        case 0x0f: case 0x10: case 0x11: case 0x14: case 0x15:
            NpcEventBinaryOp(runtime, op);
            break;
        case 0x19:
        {
            const std::uint32_t a = static_cast<std::uint32_t>(NpcEventGetWork(runtime, 1));
            const std::uint32_t b = static_cast<std::uint32_t>(NpcEventGetWork(runtime, 3));
            NpcEventSetWork(runtime, 1, b);
            NpcEventSetWork(runtime, 3, a);
            runtime.pc += 5;
            break;
        }
        case 0x1a:
            if (runtime.jumpIndex >= runtime.jumps.size()) { runtime.finished = true; return NpcEventStep::Done; }
            runtime.jumps[runtime.jumpIndex++] = static_cast<std::uint16_t>(runtime.pc + 3);
            runtime.pc = NpcEventRead16(*runtime.event, runtime.pc + 1);
            break;
        case 0x1b:
            if (runtime.jumpIndex == 0) { runtime.finished = true; return NpcEventStep::Done; }
            runtime.pc = runtime.jumps[--runtime.jumpIndex];
            break;
        case 0x1d: case 0x48:
            runtime.pendingMessage = static_cast<std::uint32_t>(NpcEventGetWork(runtime, 1));
            runtime.messageOpen = true;
            runtime.pc += 3;
            break;
        case 0x2b: case 0x49:
            runtime.pendingMessage = static_cast<std::uint32_t>(NpcEventGetWork(runtime, 5));
            runtime.messageOpen = true;
            runtime.pc += 7;
            break;
        case 0xb0:
            if (runtime.pc + 12 > runtime.event->code.size() || runtime.event->code[runtime.pc + 1] != 0)
                return NpcEventStep::Blocked;
            runtime.pendingMessage = static_cast<std::uint32_t>(NpcEventGetWork(runtime, 10));
            runtime.messageOpen = true;
            runtime.pc += 12;
            break;
        case 0x23:
            if (runtime.messageOpen)
                return NpcEventStep::Message;
            runtime.pc += 1;
            break;
        case 0x24:
            runtime.pendingChoiceMessage = static_cast<std::uint32_t>(NpcEventGetWork(runtime, 1));
            runtime.defaultChoice = static_cast<std::uint32_t>(std::max<std::int32_t>(0, NpcEventGetWork(runtime, 3)));
            runtime.hiddenChoiceMask = static_cast<std::uint32_t>(NpcEventGetWork(runtime, 5));
            runtime.waitingChoice = true;
            runtime.pc += 7;
            break;
        case 0x25: case 0x7f:
            if (runtime.waitingChoice)
                return NpcEventStep::Choice;
            runtime.pc += 1;
            break;
        case 0x3e:
        {
            const std::int32_t bit = NpcEventGetWork(runtime, 3);
            const std::int32_t word = NpcEventGetWork(runtime, 1, bit >> 5);
            runtime.pc = ((word & (1 << (bit & 31))) == 0)
                ? NpcEventRead16(*runtime.event, runtime.pc + 5)
                : runtime.pc + 7;
            break;
        }
        default:
        {
            const std::size_t size = NpcEventFixedSize(op);
            if (size == 0)
                return NpcEventStep::Blocked;
            runtime.pc += size;
            break;
        }
        }
    }
    return NpcEventStep::Blocked;
}

static void DismissNpcEventMessage(NpcEventRuntime& runtime)
{
    runtime.messageOpen = false;
    runtime.pendingMessage = 0xffffffffu;
}

static void SelectNpcEventChoice(NpcEventRuntime& runtime, std::uint32_t selected)
{
    runtime.zone[0] = selected;
    runtime.waitingChoice = false;
    runtime.pendingChoiceMessage = 0xffffffffu;
}

static std::vector<std::string> SplitNpcDialogueFrames(const std::string& dialogue)
{
    std::vector<std::string> frames;
    std::size_t start = 0;
    while (start <= dialogue.size())
    {
        std::size_t end = dialogue.find('\n', start);
        std::string frame = dialogue.substr(start,
            end == std::string::npos ? std::string::npos : end - start);
        if (!frame.empty() && frame.back() == '\r')
            frame.pop_back();
        if (!frame.empty())
            frames.push_back(std::move(frame));
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    if (frames.empty())
        frames.push_back(dialogue);
    return frames;
}

static bool ShowNpcConversationFrame(HWND owner)
{
    if (!g_npcConversation.entityId)
        return false;
    if (g_npcConversation.index < g_npcConversation.frames.size())
    {
        NpcChatWindow::Show(owner, g_npcConversation.name,
            g_npcConversation.frames[g_npcConversation.index]);
        return true;
    }
    return true;
}

static void EndNpcConversation()
{
    NpcChatWindow::HideChoices();
    g_npcConversation = {};
    g_homePointTeleport = {};
}

static bool LoadRetailNpcConversation(const FFXINpcPlacement::Placement& placement)
{
    auto trace = [](const std::string& text)
    {
        OutputDebugStringA(("[DATura NPC dialogue] " + text + "\n").c_str());
    };
    const FFXILoreZoneDatEntry* zone = FFXILoreZoneDat_FindByZoneID(FFXINpcPlacement::ZoneId());
    if (!zone || !g_ffxiPath || !g_ffxiPath[0])
        return false;
    char eventPath[MAX_PATH] = {};
    char messagePath[MAX_PATH] = {};
    FFXIPath::BuildFullPath(g_ffxiPath, zone->eventDat, eventPath, sizeof(eventPath));
    FFXIPath::BuildFullPath(g_ffxiPath, zone->dialogDat, messagePath, sizeof(messagePath));
    if (!FFXIEventTable::Load(eventPath, g_npcConversation.scripts) ||
        !FFXIEventMessages::Load(messagePath, g_npcConversation.messages))
        return false;

    for (const auto& scripts : g_npcConversation.scripts)
    {
        if (scripts.entityId != placement.entityId) continue;
        for (const auto& event : scripts.events)
        {
            NpcEventRuntime runtime;
            runtime.event = &event;
            g_npcConversation.runtime = runtime;
            const NpcEventStep firstStep = StepNpcEvent(g_npcConversation.runtime);
            if (firstStep == NpcEventStep::Message || firstStep == NpcEventStep::Choice)
                return true;
        }
        trace("matched entity had no runnable retail dialogue");
        return false;
    }
    return false;
}

static bool PresentNpcEventStep(HWND owner)
{
    for (;;)
    {
        const NpcEventStep step = StepNpcEvent(g_npcConversation.runtime);
        if (step == NpcEventStep::Message)
        {
            const std::uint32_t id = g_npcConversation.runtime.pendingMessage;
            if (id < g_npcConversation.messages.size() &&
                !g_npcConversation.messages[(size_t)id].text.empty())
            {
                NpcChatWindow::HideChoices();
                NpcChatWindow::Show(owner, g_npcConversation.name,
                    g_npcConversation.messages[(size_t)id].text);
                return true;
            }
            DismissNpcEventMessage(g_npcConversation.runtime);
            continue;
        }
        if (step == NpcEventStep::Choice)
        {
            const std::uint32_t id = g_npcConversation.runtime.pendingChoiceMessage;
            std::string prompt;
            std::vector<std::string> options;
            std::vector<std::uint32_t> optionValues;
            if (id < g_npcConversation.messages.size() &&
                FFXIEventMessages::SplitChoiceText(g_npcConversation.messages[(size_t)id],
                    g_npcConversation.runtime.hiddenChoiceMask, prompt, options, optionValues))
            {
                g_npcConversation.choicePrompt = prompt;
                g_npcConversation.choiceOptions = options;
                g_npcConversation.choiceValues = optionValues;
                const auto defaultIt = std::find(optionValues.begin(), optionValues.end(),
                    g_npcConversation.runtime.defaultChoice);
                g_npcConversation.choiceSelected = defaultIt == optionValues.end()
                    ? 0 : static_cast<int>(defaultIt - optionValues.begin());
                NpcChatWindow::ShowChoices(owner, prompt, options, g_npcConversation.choiceSelected);
                return true;
            }
            SelectNpcEventChoice(g_npcConversation.runtime, 0);
            continue;
        }
        return false;
    }
}

static void StartNpcConversation(HWND owner, const FFXINpcPlacement::Placement& placement)
{
    g_npcConversation = {};
    g_npcConversation.entityId = placement.entityId;
    g_npcConversation.name = placement.name;
    if (LoadRetailNpcConversation(placement))
    {
        PresentNpcEventStep(owner);
        return;
    }
    g_npcConversation.frames = SplitNpcDialogueFrames(NpcDialogueText(placement));
    g_npcConversation.index = 0;
    ShowNpcConversationFrame(owner);
}

static bool AdvanceNpcConversation(HWND owner)
{
    if (!g_npcConversation.entityId)
        return false;
    if (g_homePointTeleport.active)
        return CompleteHomePointTeleport(owner);
    if (!g_npcConversation.choiceOptions.empty())
    {
        const std::uint32_t selectedValue =
            g_npcConversation.choiceSelected >= 0 &&
            static_cast<std::size_t>(g_npcConversation.choiceSelected) < g_npcConversation.choiceValues.size()
            ? g_npcConversation.choiceValues[static_cast<std::size_t>(g_npcConversation.choiceSelected)]
            : 0;
        SelectNpcEventChoice(g_npcConversation.runtime,
            selectedValue);
        g_npcConversation.choicePrompt.clear();
        g_npcConversation.choiceOptions.clear();
        g_npcConversation.choiceValues.clear();
        NpcChatWindow::HideChoices();
        if (!PresentNpcEventStep(owner))
            EndNpcConversation();
        return true;
    }
    if (g_npcConversation.runtime.event)
    {
        DismissNpcEventMessage(g_npcConversation.runtime);
        if (!PresentNpcEventStep(owner))
            EndNpcConversation();
        return true;
    }
    if (g_npcConversation.index + 1 < g_npcConversation.frames.size())
    {
        ++g_npcConversation.index;
        ShowNpcConversationFrame(owner);
    }
    else
    {
        EndNpcConversation();
    }
    return true;
}

static bool MoveNpcConversationChoice(HWND owner, int delta)
{
    if (!g_npcConversation.entityId || g_npcConversation.choiceOptions.empty())
        return false;
    const int count = (int)g_npcConversation.choiceOptions.size();
    g_npcConversation.choiceSelected = (g_npcConversation.choiceSelected + delta + count) % count;
    NpcChatWindow::SetChoiceSelection(owner, g_npcConversation.choiceSelected);
    return true;
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
            std::string dialogue;
            for (const std::uint16_t id : ids)
            {
                trace("event=" + std::to_string(event.id) + " message=" + std::to_string(id));
                if (id < messages.size() && !messages[id].text.empty())
                {
                    if (!dialogue.empty())
                        dialogue += "\n";
                    dialogue += messages[id].text;
                }
            }
            if (!dialogue.empty())
            {
                trace("resolved event dialogue lines=" + std::to_string(ids.size()));
                return dialogue;
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
        if (!g_companionViewerMode)
            LoadPlayerRaceInModelViewer(g_playerEquip.raceIndex);
        else if (g_playerAsset)
            g_playerAsset.Model()->RestoreBindPose(GraphicsDevice());
        break;
    case LowPolyCharacterPanel::Command::Reset:
        g_playerEquip.animationPlaying = false;
        g_playerAnimTime = 0.0f;
        if (!g_companionViewerMode)
            LoadPlayerRaceInModelViewer(g_playerEquip.raceIndex);
        else if (g_playerAsset)
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
    g_activeCharacterDataPath.clear();
    g_lastSavedCharacterDataPath.clear();

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

static bool g_selectingZoneObjectFromWorld = false;

static void HighlightZoneObject(const int mapObjectIndex)
{
    if (mapObjectIndex < 0 || mapObjectIndex >= Model_FF11_GetLastMapObjectCount())
        return;
    const char* objectName = Model_FF11_GetLastMapObjectDisplayName(mapObjectIndex);
    if (!objectName || !objectName[0])
        return;
    g_highlightedZoneObjects.clear();
    g_highlightedZoneObjects.insert(objectName);
    ZoneObjectPanel::SetHighlightActive(g_zoneObjectPanel, true);
    if (g_hWnd)
        InvalidateRect(g_hWnd, NULL, FALSE);
}

static void HighlightSelectedZoneObjectRows()
{
    g_highlightedZoneObjects.clear();
    const std::vector<int> selectedIndices =
        ZoneObjectPanel::SelectedMapObjectIndices(g_zoneObjectPanel);
    for (const int mapObjectIndex : selectedIndices)
    {
        if (mapObjectIndex < 0 ||
            mapObjectIndex >= Model_FF11_GetLastMapObjectCount())
            continue;
        const char* objectName =
            Model_FF11_GetLastMapObjectDisplayName(mapObjectIndex);
        if (objectName && objectName[0])
            g_highlightedZoneObjects.insert(objectName);
    }
    ZoneObjectPanel::SetHighlightActive(
        g_zoneObjectPanel, !g_highlightedZoneObjects.empty());
    if (g_hWnd)
        InvalidateRect(g_hWnd, NULL, FALSE);
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
        if (!g_selectingZoneObjectFromWorld)
            HighlightZoneObject(event.mapObjectIndex);
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
        if (!g_selectingZoneObjectFromWorld)
            HighlightSelectedZoneObjectRows();
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
                if (!g_highlightedZoneObjects.empty())
                    g_highlightedZoneObjects.clear();
                else
                    g_highlightedZoneObjects.insert(objectName);
                ZoneObjectPanel::SetHighlightActive(
                    g_zoneObjectPanel, !g_highlightedZoneObjects.empty());
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

static int FindZoneObjectIndex(const std::string& objectName)
{
    for (int index = 0; index < Model_FF11_GetLastMapObjectCount(); ++index)
    {
        const char* displayName = Model_FF11_GetLastMapObjectDisplayName(index);
        if (displayName && objectName == displayName)
            return index;
    }
    return -1;
}

static bool SelectZoneObjectAtClientPoint(
    const HWND window, POINT point, const bool additive)
{
    if (!GraphicsDevice() || !g_zoneAsset.Model())
        return false;

    int width = 0;
    int height = 0;
    D3D9Device::GetViewportOrClientSize(GraphicsDevice(), window, &width, &height);
    point = D3D9Device::MapClientPointToViewport(window, width, height, point);

    ZoneObjectVisibility::RenderContext visibility;
    visibility.hiddenNames = &g_hiddenZoneObjects;
    visibility.viewerPointValid = g_zoneVisibilityViewerPointValid;
    visibility.lodViewerPoint = g_zoneVisibilityViewerPoint;
    visibility.visibleMapObjects = &g_zoneVisibleMapObjects;
    visibility.overrides = &g_zoneRuntimeOverrides;
    visibility.frustum = &g_zoneRenderFrustum;
    const ZoneObjectPicker::Hit hit = ZoneObjectPicker::Pick(
        g_zoneAsset.Model(), visibility, g_pickView, g_pickProjection,
        g_pickWidth, g_pickHeight, static_cast<float>(point.x),
        static_cast<float>(point.y));
    const int mapObjectIndex = FindZoneObjectIndex(hit.objectName);
    if (mapObjectIndex < 0)
        return false;

    const HWND zoneObjectPanel = ZoneObjectPanel::Window(g_zoneObjectPanel);
    if (zoneObjectPanel && IsWindowVisible(zoneObjectPanel))
    {
        g_selectingZoneObjectFromWorld = true;
        ZoneObjectPanel::SelectMapObject(
            g_zoneObjectPanel, mapObjectIndex, additive, false);
        g_selectingZoneObjectFromWorld = false;
    }
    if (!additive)
        g_highlightedZoneObjects.clear();
    g_highlightedZoneObjects.insert(hit.objectName);
    ZoneObjectPanel::SetHighlightActive(g_zoneObjectPanel, true);
    InvalidateRect(window, NULL, FALSE);
    return true;
}

static void LoadPlayerRaceModel(int raceIndex)
{
    g_titleScreenActive = false;
    g_highPolyCreationActive = false;
    UnloadCreationModel();
    UnloadTitleAssets();
    if (g_companionViewerMode)
        UnloadZoneModel();
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
    if (g_companionViewerMode)
    {
        g_player.position[0] = 0.0f;
        g_player.position[1] = 0.0f;
        g_player.position[2] = 0.0f;
        g_player.yaw = 0.0f;
        g_player.verticalVelocity = 0.0f;
        g_player.onGround = false;
        ResetCameraForStandaloneModel(g_playerAsset.Model());
        InvalidateRect(g_hWnd, NULL, FALSE);
        return;
    }
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
    g_camDist = 10.0f;
    g_camPitch = -0.35f;
    PlayerController::SetRespawnPoint(g_player);

    SyncLowPolyControlsFromState();
}

static void ReloadPlayerModelFromControls()
{
    PullLowPolyStateFromControls();
    if (!g_companionViewerMode)
    {
        LoadPlayerRaceInModelViewer(g_playerEquip.raceIndex);
        return;
    }
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
    g_activeCharacterDataPath.clear();
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
            char dataPath[MAX_PATH] = {};
            FFXIFileIO::ReplaceExtension(path, ".ini", dataPath, sizeof(dataPath));
            preview.dataPath = dataPath;
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

static bool LoadCharacterZone(const int zoneId)
{
    const SceneRequestResolver::Result resolution =
        SceneRequestResolver::ResolveZone(g_ffxiPath, zoneId);
    if (!resolution.Succeeded())
        return false;

    LoadDatFile(resolution.request);
    SetLoadedSceneContext(resolution.request);
    return !!g_zoneAsset;
}

static std::string HomePointDisplayName(const CharacterSaveData::HomePoint& point)
{
    const FFXIZoneEntry* zone = FFXIZone::FindByID(point.zoneId);
    const std::string zoneName = zone && zone->name ? zone->name :
        "Zone " + std::to_string(point.zoneId);
    return zoneName + " - " + (point.name.empty() ? "Home Point" : point.name);
}

static void StartHomePointTeleportChoice(
    HWND owner, const CharacterSaveData::Data& data,
    const CharacterSaveData::HomePoint& source)
{
    g_homePointTeleport = {};
    g_homePointTeleport.source = source;
    for (const CharacterSaveData::HomePoint& point : data.activatedHomePoints)
    {
        if (!CharacterSaveData::SameHomePoint(point, source))
            g_homePointTeleport.destinations.push_back(point);
    }
    if (g_homePointTeleport.destinations.empty())
        return;

    g_homePointTeleport.active = true;
    g_npcConversation = {};
    g_npcConversation.entityId = source.entityId;
    g_npcConversation.name = "Home Point";
    for (std::size_t index = 0; index < g_homePointTeleport.destinations.size(); ++index)
    {
        g_npcConversation.choiceOptions.push_back(
            HomePointDisplayName(g_homePointTeleport.destinations[index]));
        g_npcConversation.choiceValues.push_back(static_cast<std::uint32_t>(index));
    }
    g_npcConversation.choiceOptions.push_back("Cancel");
    g_npcConversation.choiceValues.push_back(
        static_cast<std::uint32_t>(g_homePointTeleport.destinations.size()));
    g_npcConversation.choiceSelected = 0;
    NpcChatWindow::ShowChoices(owner, "Teleport to which Home Point?",
        g_npcConversation.choiceOptions, g_npcConversation.choiceSelected);
}

static bool CompleteHomePointTeleport(HWND owner)
{
    const int selected = g_npcConversation.choiceSelected;
    const CharacterSaveData::HomePoint source = g_homePointTeleport.source;
    const std::vector<CharacterSaveData::HomePoint> destinations =
        g_homePointTeleport.destinations;
    EndNpcConversation();
    if (selected < 0 || static_cast<std::size_t>(selected) >= destinations.size())
        return true;

    const CharacterSaveData::HomePoint destination = destinations[selected];
    if (!LoadCharacterZone(destination.zoneId) ||
        !SpawnPlayerAtZoneHomePoint(destination.entityId))
    {
        if (LoadCharacterZone(source.zoneId))
            SpawnPlayerAtZoneHomePoint(source.entityId);
        NpcChatWindow::Show(owner, "System",
            "That Home Point could not be reached. You have been returned to the source Home Point.");
        return true;
    }

    g_camDist = 10.0f;
    g_camPitch = -0.35f;
    UpdatePlayerCameraTarget();
    PlayerController::SetRespawnPoint(g_player);
    const FFXIZoneMusicEntry* music = FFXIZoneMusic_FindByID(g_loadedZoneId);
    SetGameModeMusic(SelectZoneMusicId(music));

    CharacterSaveData::Data characterData;
    if (!g_activeCharacterDataPath.empty() &&
        CharacterSaveData::Load(g_activeCharacterDataPath.c_str(), characterData))
    {
        CharacterSaveData::RegisterHomePoint(characterData, destination);
        CharacterSaveData::Save(g_activeCharacterDataPath.c_str(), characterData);
    }
    NpcChatWindow::Show(owner, "System",
        "Teleported to " + HomePointDisplayName(destination) + ".");
    return true;
}

static void LoadSelectedCharacterScene()
{
    if (g_savedCharacterPreviews.empty())
        return;

    SavedCharacterPreview& selected = g_savedCharacterPreviews[g_selectedCharacterPreview];
    CharacterSaveData::Data characterData;
    bool loadedCharacterData =
        CharacterSaveData::Load(selected.dataPath.c_str(), characterData);
    if (!loadedCharacterData)
    {
        // Legacy characters have no metadata. Give them a durable Bastok
        // fallback immediately so their first Home Point activation can save.
        characterData.homeNationIndex = 0;
        loadedCharacterData = CharacterSaveData::Save(
            selected.dataPath.c_str(), characterData);
    }
    const int nationIndex = loadedCharacterData ? characterData.homeNationIndex : 0;
    const int homeNationZone = FFXINationSelection::InfoForIndex(nationIndex).zoneId;

    bool spawned = false;
    if (loadedCharacterData && characterData.hasHomePoint &&
        LoadCharacterZone(characterData.homePointZoneId))
    {
        spawned = SpawnPlayerAtZoneHomePoint(characterData.homePointEntityId);
    }
    if (!spawned)
    {
        if (!LoadCharacterZone(homeNationZone))
        {
            MessageBoxA(g_hWnd,
                "The character's Home Point and home nation could not be loaded.",
                "Select Character", MB_OK | MB_ICONWARNING);
            return;
        }
        SpawnPlayerAtZoneHomePoint();
    }

    UnloadPlayerModel();
    g_playerAsset = std::move(selected.asset);
    g_activeCharacterDataPath = selected.dataPath;
    g_activeCharacterHomeNationIndex = nationIndex;
    g_savedCharacterPreviews.clear();
    g_characterSelectActive = false;
    g_characterDeleteActive = false;
    g_camDist = 10.0f;
    g_camPitch = -0.35f;
    UpdatePlayerCameraTarget();
    PlayerController::SetRespawnPoint(g_player);

    const FFXIZoneMusicEntry* music = FFXIZoneMusic_FindByID(g_loadedZoneId);
    SetGameModeMusic(SelectZoneMusicId(music));
    InvalidateRect(g_hWnd, nullptr, FALSE);
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
    SpawnPlayerAtZoneHomePoint();

    char dataPath[MAX_PATH] = {};
    if (!g_lastSavedCharacterDataPath.empty())
    {
        strcpy_s(dataPath, g_lastSavedCharacterDataPath.c_str());
    }
    else
    {
        char characterDir[MAX_PATH] = {};
        char safeName[64] = {};
        FFXIFileIO::GetExecutableDirectory(characterDir, sizeof(characterDir));
        strcat_s(characterDir, "Characters\\");
        CreateDirectoryA(characterDir, nullptr);
        FFXIFileIO::MakeSafeFileStem(g_creationCharacterName, safeName, sizeof(safeName));
        sprintf_s(dataPath, "%s%s.ini", characterDir, safeName);
    }
    CharacterSaveData::Data characterData;
    characterData.homeNationIndex = g_selectedNationIndex;
    CharacterSaveData::Save(dataPath, characterData);
    g_activeCharacterDataPath = dataPath;
    g_activeCharacterHomeNationIndex = g_selectedNationIndex;

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
    if (!g_titleScreenActive || !g_hWnd || !g_applicationSettings.showTitleUi)
        return;

    const FFXITitleScreenRenderer::Context context =
    {
        GraphicsDevice(), g_hWnd, g_applicationSettings.enableMipMapping,
        g_titleAssets.LogoModel(), g_titleAssets.AtlasModel(), g_titleAssets.UiModel(),
        g_gameUiConfig.title, g_input.clientMouse, g_titleMenuSelection
    };
    FFXITitleScreenRenderer::DrawTextures(context);
}

static void DrawTitleScreenOverlay(HDC overlayDc = nullptr)
{
    if (!g_titleScreenActive || !g_hWnd || !g_applicationSettings.showTitleUi)
        return;

    const FFXITitleScreenRenderer::Context context =
    {
        GraphicsDevice(), g_hWnd, g_applicationSettings.enableMipMapping,
        g_titleAssets.LogoModel(), g_titleAssets.AtlasModel(), g_titleAssets.UiModel(),
        g_gameUiConfig.title, g_input.clientMouse, g_titleMenuSelection, overlayDc
    };
    FFXITitleScreenRenderer::DrawOverlay(context);
}

static RECT CharacterRosterBackRect(int width, int height);
static RECT CharacterRosterConfirmRect(int width, int height);
static RECT CharacterRosterPanelRect();
static RECT CharacterRosterRowRect(int index);

static constexpr int kCharacterRosterRowHeight = 30;
static constexpr int kCharacterRosterPanelInset = 12;

static void DrawCharacterRosterOverlay(HDC dc)
{
    if ((!g_characterSelectActive && !g_characterDeleteActive) || !dc) return;
    const RECT panel = CharacterRosterPanelRect();
    NpcChatWindow::PaintFrameRect(dc, panel);
    HFONT font = CreateFontA(-18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, "Arial");
    for (size_t i = 0; i < g_savedCharacterPreviews.size(); ++i)
    {
        RECT row = CharacterRosterRowRect((int)i);
        if ((int)i == g_selectedCharacterPreview)
        {
            HBRUSH selected = CreateSolidBrush(
                g_characterDeleteActive ? RGB(102, 40, 58) : RGB(48, 79, 124));
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

static RECT CharacterRosterPanelRect()
{
    return { 16, 70, 262, 360 };
}

static RECT CharacterRosterRowRect(int index)
{
    const RECT panel = CharacterRosterPanelRect();
    const int top = panel.top + 14 + index * kCharacterRosterRowHeight;
    return {
        panel.left + kCharacterRosterPanelInset,
        top,
        panel.right - kCharacterRosterPanelInset,
        top + 26
    };
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

static void DrawCharacterRosterTextures(const int width, const int height)
{
    if (!g_characterSelectActive && !g_characterDeleteActive)
        return;
    noesisTex_t* buttonTexture = FFXITitleAssets::FindTitleTexture(
        g_titleAssets.UiModel(), "buttonto");
    if (!buttonTexture)
        buttonTexture = FFXITitleAssets::FindTitleTexture(
            g_titleAssets.UiModel(), "lrbutton");
    if (!buttonTexture || !buttonTexture->pD3DTex)
        return;

    const POINT mouse = D3D9Device::MapClientPointToViewport(
        g_hWnd, width, height, g_input.clientMouse);
    const RECT back = CharacterRosterBackRect(width, height);
    const RECT confirm = CharacterRosterConfirmRect(width, height);
    FFXITitleUiPrimitives::DrawButton(
        GraphicsDevice(), g_applicationSettings.enableMipMapping, buttonTexture,
        (float)back.left, (float)back.top,
        (float)(back.right - back.left), (float)(back.bottom - back.top),
        PtInRect(&back, mouse) != FALSE);
    FFXITitleUiPrimitives::DrawButton(
        GraphicsDevice(), g_applicationSettings.enableMipMapping, buttonTexture,
        (float)confirm.left, (float)confirm.top,
        (float)(confirm.right - confirm.left), (float)(confirm.bottom - confirm.top),
        PtInRect(&confirm, mouse) != FALSE);
}

static void DrawZoneMapOverlay(HDC dc)
{
    if (!g_zoneMapVisible || !dc || !IsGameMode())
        return;

    RECT client = {};
    GetClientRect(g_hWnd, &client);
    const int clientWidth = client.right - client.left;
    const int clientHeight = client.bottom - client.top;
    if (clientWidth < 240 || clientHeight < 180)
        return;

    const int margin = (std::max)(24, (std::min)(clientWidth, clientHeight) / 14);
    RECT panel = { margin, margin, clientWidth - margin, clientHeight - margin };
    HBRUSH shadow = CreateSolidBrush(RGB(3, 7, 13));
    FillRect(dc, &panel, shadow);
    DeleteObject(shadow);
    FrameRect(dc, &panel, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));

    RECT mapRect = { panel.left + 18, panel.top + 52, panel.right - 18, panel.bottom - 42 };
    HBRUSH sea = CreateSolidBrush(RGB(19, 35, 48));
    FillRect(dc, &mapRect, sea);
    DeleteObject(sea);

    if (g_haveZoneCollisionBounds && !g_zoneCollisionTris.empty())
    {
        const float minX = g_zoneCollisionMin[0], maxX = g_zoneCollisionMax[0];
        const float minZ = g_zoneCollisionMin[2], maxZ = g_zoneCollisionMax[2];
        const float worldWidth = (std::max)(1.0f, maxX - minX);
        const float worldHeight = (std::max)(1.0f, maxZ - minZ);
        const float scale = (std::min)(
            static_cast<float>(mapRect.right - mapRect.left - 8) / worldWidth,
            static_cast<float>(mapRect.bottom - mapRect.top - 8) / worldHeight);
        const float offsetX = (mapRect.left + mapRect.right - worldWidth * scale) * 0.5f;
        const float offsetY = (mapRect.top + mapRect.bottom - worldHeight * scale) * 0.5f;
        const auto project = [&](const float x, const float z)
        {
            POINT point = {
                static_cast<LONG>(offsetX + (x - minX) * scale),
                static_cast<LONG>(offsetY + (maxZ - z) * scale)
            };
            return point;
        };

        HBRUSH ground = CreateSolidBrush(RGB(92, 105, 91));
        HGDIOBJ oldBrush = SelectObject(dc, ground);
        HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
        for (const ZoneCollisionTriangle& triangle : g_zoneCollisionTris)
        {
            if (std::fabs(triangle.normal[1]) < 0.20f)
                continue;
            POINT points[3] = {
                project(triangle.p[0][0], triangle.p[0][2]),
                project(triangle.p[1][0], triangle.p[1][2]),
                project(triangle.p[2][0], triangle.p[2][2])
            };
            Polygon(dc, points, 3);
        }
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(ground);

        const POINT player = project(g_player.position[0], g_player.position[2]);
        const float arrowSize = 11.0f;
        const float forwardX = -std::sin(g_player.yaw);
        const float forwardY = -std::cos(g_player.yaw);
        const float rightX = -forwardY, rightY = forwardX;
        POINT arrow[3] = {
            { player.x + static_cast<LONG>(forwardX * arrowSize),
              player.y + static_cast<LONG>(forwardY * arrowSize) },
            { player.x - static_cast<LONG>(forwardX * arrowSize * 0.65f) + static_cast<LONG>(rightX * arrowSize * 0.65f),
              player.y - static_cast<LONG>(forwardY * arrowSize * 0.65f) + static_cast<LONG>(rightY * arrowSize * 0.65f) },
            { player.x - static_cast<LONG>(forwardX * arrowSize * 0.65f) - static_cast<LONG>(rightX * arrowSize * 0.65f),
              player.y - static_cast<LONG>(forwardY * arrowSize * 0.65f) - static_cast<LONG>(rightY * arrowSize * 0.65f) }
        };
        HBRUSH marker = CreateSolidBrush(RGB(255, 222, 76));
        oldBrush = SelectObject(dc, marker);
        oldPen = SelectObject(dc, GetStockObject(WHITE_PEN));
        Polygon(dc, arrow, 3);
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(marker);
    }

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(240, 240, 226));
    HFONT font = CreateFontA(-24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    HGDIOBJ oldFont = SelectObject(dc, font);
    TextOutA(dc, panel.left + 18, panel.top + 14, g_loadedZoneLabel.c_str(),
        static_cast<int>(g_loadedZoneLabel.size()));
    SelectObject(dc, oldFont);
    DeleteObject(font);
    SetTextColor(dc, RGB(185, 194, 203));
    const char* hint = "M or Esc: close map";
    TextOutA(dc, panel.left + 18, panel.bottom - 28, hint, static_cast<int>(strlen(hint)));
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

static void DrawModelViewerLightOverlay(HDC dc)
{
    if (!g_companionViewerMode || g_modelViewerLightOverlayCorner == 0 || !dc)
        return;

    char text[96] = {};
    sprintf_s(text, "Azimuth: %.1f deg\r\nElevation: %.1f deg",
        g_modelViewerLightAzimuthDegrees, g_modelViewerLightElevationDegrees);

    const int savedDc = SaveDC(dc);
    HFONT font = CreateFontA(
        -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    HFONT previousFont = static_cast<HFONT>(SelectObject(dc, font));
    RECT measured = {};
    static const char kMaximumLightAngleText[] =
        "Azimuth: -359.9 deg\r\nElevation: -359.9 deg";
    DrawTextA(dc, kMaximumLightAngleText, -1, &measured,
        DT_LEFT | DT_TOP | DT_NOPREFIX | DT_CALCRECT);
    SelectObject(dc, previousFont);

    RECT viewport = {};
    GetClipBox(dc, &viewport);
    constexpr int margin = 10;
    constexpr int paddingX = 8;
    constexpr int paddingY = 6;
    const int panelWidth = measured.right + paddingX * 2;
    const int panelHeight = measured.bottom + paddingY * 2;
    const bool right = g_modelViewerLightOverlayCorner == 2 ||
        g_modelViewerLightOverlayCorner == 3;
    const bool bottom = g_modelViewerLightOverlayCorner == 3 ||
        g_modelViewerLightOverlayCorner == 4;
    RECT panel = {
        right ? viewport.right - margin - panelWidth : margin,
        bottom ? viewport.bottom - margin - panelHeight : margin,
        0, 0
    };
    panel.right = panel.left + panelWidth;
    panel.bottom = panel.top + panelHeight;

    HBRUSH background = CreateSolidBrush(RGB(12, 16, 22));
    FillRect(dc, &panel, background);
    DeleteObject(background);
    RECT textBounds = {
        panel.left + paddingX, panel.top + paddingY,
        panel.right - paddingX, panel.bottom - paddingY
    };
    Win32Drawing::DrawShadowText(
        dc, font, text, textBounds, DT_LEFT | DT_TOP | DT_NOPREFIX,
        RGB(235, 240, 248), 1);
    DeleteObject(font);
    if (savedDc)
        RestoreDC(dc, savedDc);
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
    if (g_loadedZoneId == ZoneElevator::kMetalworksZone)
        ApplyMetalworksElevatorRuntimeObjects();
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
        // Normal play selects the active cell from the player-equivalent orbit
        // target. The title demo has no player, however, and its authored rails
        // often look across cell boundaries. Select title visibility from the
        // actual eye so the room surrounding the moving camera stays resident.
        const float* visibilityAnchor = g_titleScreenActive
            ? renderCameraEye : renderCameraTarget;
        FFXICoordinateFrame::SceneToNativeDat(visibilityAnchor,
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
			if (g_applicationSettings.enableWeatherEffects &&
                g_zoneEnvironment.previousWeatherPath[0])
			{
				ZoneEnvironmentRenderState::DrawCameraShells(
					GraphicsDevice(), zoneModel, g_applicationSettings.enableMipMapping,
                    g_zoneEnvironment.previousWeatherPath,
					gFF11LastGeneratorRecords, gFF11LastKeyframeRecords,
                    renderCameraEye[0], renderCameraEye[1], renderCameraEye[2],
                    1.0f - g_zoneEnvironment.weatherTransition);
			}
            if (g_applicationSettings.enableWeatherEffects)
            {
			    ZoneEnvironmentRenderState::DrawCameraShells(
				    GraphicsDevice(), zoneModel, g_applicationSettings.enableMipMapping,
                    g_zoneEnvironment.weatherPath,
				    gFF11LastGeneratorRecords, gFF11LastKeyframeRecords,
                    renderCameraEye[0], renderCameraEye[1], renderCameraEye[2],
                    g_zoneEnvironment.weatherTransition);
            }
			GraphicsDevice()->SetTransform(D3DTS_PROJECTION, &proj);
			GraphicsDevice()->SetTransform(D3DTS_WORLD, &world);
            RenderModel(zoneModel, true, ModelRenderer::GeometryPass::Opaque);
        }

        if (creationModel)
        {
            // Creation assets face +Z, while the standalone viewer begins on -X.
            D3DMATRIX creationWorld = g_companionViewerMode
                ? D3DMath::BuildYawTranslation(-3.1415926535f * 0.5f, nullptr)
                : world;
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

        int npcShadowCount = 0;
        for (const NpcRenderInstance &npc : g_npcRenderInstances)
        {
            if (npc.assetIndex >= g_npcRenderAssets.size() || !npc.placement.visible)
                continue;
            NpcRenderAsset &asset = g_npcRenderAssets[npc.assetIndex];
            if (!asset.resource && !asset.homePoint)
                continue;
            FFXINpcPlacement::Transform transform = npc.placement.transform;
            transform.headingRadians = npc.renderedHeadingRadians;
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
                const bool drawNpcShadow = g_applicationSettings.shadowNpcs &&
                    npcShadowCount < (std::max)(0, g_applicationSettings.shadowNpcLimit);
                RenderModel(asset.resource.Model(), false, ModelRenderer::GeometryPass::All,
                    drawNpcShadow);
                if (drawNpcShadow)
                    ++npcShadowCount;
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
            D3DMATRIX playerWorld = g_companionViewerMode
                ? D3DMath::BuildYawTranslation(3.1415926535f, nullptr)
                :
                D3DMath::BuildYawTranslation(
                    g_player.yaw + (3.1415926535f * 0.5f), g_player.position);
            GraphicsDevice()->SetTransform(D3DTS_WORLD, &playerWorld);
            RenderModel(playerModel, false, ModelRenderer::GeometryPass::All,
                g_applicationSettings.shadowPlayer);
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
                GraphicsDevice(), zoneModel, g_highlightedZoneObjects, g_zoneRuntimeOverrides);
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
        DrawClickMoveTarget();

        // Precipitation blends against the completed scene and depth-tests against
        // actors as well as terrain. Drawing it before actors erases foreground drops.
        if (zoneModel && g_applicationSettings.enableWeatherEffects)
        {
            if (g_zoneEnvironment.previousWeatherPath[0])
            {
                ZoneWeatherParticles::Draw(
                    GraphicsDevice(), zoneModel, g_zoneEnvironment.valid,
                    g_applicationSettings.enableMipMapping,
                    g_zoneEnvironment.previousWeatherPath,
                    gFF11LastGeneratorRecords, gFF11LastKeyframeRecords,
                    renderCameraEye[0], renderCameraEye[1], renderCameraEye[2],
                    1.0f - g_zoneEnvironment.weatherTransition);
            }
            ZoneWeatherParticles::Draw(
                GraphicsDevice(), zoneModel, g_zoneEnvironment.valid,
                g_applicationSettings.enableMipMapping, g_zoneEnvironment.weatherPath,
                gFF11LastGeneratorRecords, gFF11LastKeyframeRecords,
                renderCameraEye[0], renderCameraEye[1], renderCameraEye[2],
                g_zoneEnvironment.weatherTransition);
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
        if (g_uiVisible)
        {
            if (!g_titleScreenActive && !g_nationSelectActive)
                NpcNameplateRenderer::Draw(GraphicsDevice(), g_npcNameplates);
            DrawTitleScreenTextures();
            DrawNationSelectTextures();
            DrawCharacterRosterTextures((int)w, (int)h2);
            DrawFfxiMainMenuTextures();
            DrawZoneBoundaryDots(viewProjection, w, h2);
            DrawZoneTransitionOverlay();
        }

        GraphicsDevice()->EndScene();
    }

    // Resolve MSAA/post-processing before drawing GDI overlays into the final
    // swap-chain back buffer.
    g_graphicsRuntime.ResolveFrame();

    if (g_uiVisible)
        NpcChatWindow::Draw(GraphicsDevice());

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
            if (g_uiVisible)
            {
                DrawTitleScreenOverlay(overlayDc);
                DrawCharacterRosterOverlay(overlayDc);
                DrawNationSelectOverlay(overlayDc);
                DrawZoneMapOverlay(overlayDc);
                DrawCameraDebugOverlay(overlayDc);
                DrawModelViewerLightOverlay(overlayDc);
                DrawFfxiMainMenuOverlay(overlayDc);
                DrawDeveloperConsoleOverlay(overlayDc);
            }
            backBuffer->ReleaseDC(overlayDc);
        }
        backBuffer->Release();
    }

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

static void OpenOrUpdateModelViewer(
    const ULONG_PTR messageId, void* payload, const DWORD payloadSize,
    const std::string& selectionArguments)
{
    const HWND companionWindow = FindWindowW(kWindowClassName, kCompanionViewerTitle);
    if (companionWindow)
    {
        COPYDATASTRUCT copyData = {};
        copyData.dwData = messageId;
        copyData.cbData = payloadSize;
        copyData.lpData = payload;
        const LRESULT accepted = SendMessageA(
            companionWindow, WM_COPYDATA,
            static_cast<WPARAM>(GetCurrentProcessId()),
            reinterpret_cast<LPARAM>(&copyData));
        if (accepted)
        {
            ShowWindow(companionWindow, SW_RESTORE);
            SetForegroundWindow(companionWindow);
            return;
        }

        // A viewer from an older build does not understand this protocol.
        // Retire it before launching the current executable so stale camera
        // behavior cannot survive across debug/build cycles.
        PostMessageW(companionWindow, WM_CLOSE, 0, 0);
        for (int attempt = 0; attempt < 40 &&
             FindWindowW(kWindowClassName, kCompanionViewerTitle); ++attempt)
            Sleep(50);
    }

    char executable[MAX_PATH] = {};
    if (!GetModuleFileNameA(NULL, executable, sizeof(executable)))
    {
        MessageBoxA(g_hWnd, "Could not locate the DATura executable.", "Model Viewer",
                    MB_OK | MB_ICONERROR);
        return;
    }

    std::string parameters = QuoteCommandLineArgument(kCompanionViewerArg);
    parameters.push_back(' ');
    parameters += selectionArguments;
    parameters.push_back(' ');
    parameters += QuoteCommandLineArgument(kCompanionOwnerPidArg);
    parameters.push_back(' ');
    parameters += std::to_string(GetCurrentProcessId());

    HINSTANCE result = ShellExecuteA(
        g_hWnd, "open", executable, parameters.c_str(), nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32)
    {
        MessageBoxA(g_hWnd, "Could not open the model viewer window.",
                    "Model Viewer", MB_OK | MB_ICONERROR);
    }
}

static void LoadModelInCompanionViewer(void *, const char *label, const char *datPath)
{
    std::string payload = label ? label : "Companion";
    payload.push_back('\0');
    payload += datPath ? datPath : "";
    payload.push_back('\0');

    std::string arguments = QuoteCommandLineArgument(kCompanionModelArg);
    arguments.push_back(' ');
    arguments += QuoteCommandLineArgument(datPath);
    arguments.push_back(' ');
    arguments += QuoteCommandLineArgument(kCompanionLabelArg);
    arguments.push_back(' ');
    arguments += QuoteCommandLineArgument(label ? label : "Companion");
    OpenOrUpdateModelViewer(kCompanionModelCopyDataId, &payload[0],
                            static_cast<DWORD>(payload.size()), arguments);
}

static void LoadPlayerRaceInModelViewer(const int raceIndex)
{
    PlayerViewerRequest payload = { g_playerEquip, g_playerFaceVariant };
    payload.equipment.raceIndex = raceIndex;
    const std::string serialized = SerializePlayerViewerRequest(payload);
    std::string arguments = QuoteCommandLineArgument(kPlayerCustomizationArg);
    arguments.push_back(' ');
    arguments += QuoteCommandLineArgument(serialized.c_str());
    OpenOrUpdateModelViewer(kPlayerRaceCopyDataId, &payload, sizeof(payload), arguments);
}

static void LoadCreationModelInModelViewer(const int flatIndex)
{
    int payload = flatIndex;
    std::string arguments = QuoteCommandLineArgument(kCreationModelArg);
    arguments.push_back(' ');
    arguments += std::to_string(flatIndex);
    OpenOrUpdateModelViewer(kCreationModelCopyDataId, &payload, sizeof(payload), arguments);
}

static void ShowCompanionBrowser()
{
    FFXICompanionBrowser::Show(
        g_hWnd, g_ffxiPath, g_themeState, LoadModelInCompanionViewer);
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
        if (const FFXIStandaloneModelEntry* entry = FFXIStandaloneModel_GetEntry(
                kFFXINpcModelGroups, kFFXINpcModelGroupCount, command.selectionIndex))
            LoadModelInCompanionViewer(nullptr, entry->label, entry->dat);
        return true;

    case ApplicationMenu::SelectionType::MonsterModel:
        if (const FFXIStandaloneModelEntry* entry = FFXIStandaloneModel_GetEntry(
                kFFXIMonsterModelGroups, kFFXIMonsterModelGroupCount, command.selectionIndex))
            LoadModelInCompanionViewer(nullptr, entry->label, entry->dat);
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
                if (IsGameMode())
                    EnsureGameModePlayerForCurrentZone();
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
        LoadPlayerRaceInModelViewer(command.selectionIndex);
        return true;

    case ApplicationMenu::SelectionType::CreationModel:
        LoadCreationModelInModelViewer(command.selectionIndex);
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
        if (IsGameMode())
        {
            // An active event owns Confirm. Reopen its hidden transcript first;
            // otherwise advance it. Outside an event, Confirm opens chat input.
            if (g_npcConversation.entityId)
            {
                if (NpcChatWindow::Reopen(window))
                    return;
                if (AdvanceNpcConversation(window))
                    return;
            }
        }
        if (g_characterSelectActive && !g_savedCharacterPreviews.empty())
        {
            LoadSelectedCharacterScene();
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
                sprintf_s(path, "%sCharacters\\%s.ini", exeDir, name.c_str()); DeleteFileA(path);
                BeginCharacterDeleteScene();
            }
            return;
        }
        if (g_nationSelectActive)
        {
            LoadSelectedNationScene();
            return;
        }
        if (IsGameMode())
        {
            const char* playerName = g_gameUiConfig.playerNameplate.name[0]
                ? g_gameUiConfig.playerNameplate.name : g_creationCharacterName;
            NpcChatWindow::OpenInput(window, playerName);
        }
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
    case InputController::Action::ToggleZoneMap:
        if (IsGameMode() && g_zoneAsset && !g_titleScreenActive &&
            !g_nationSelectActive)
        {
            g_zoneMapVisible = !g_zoneMapVisible;
            if (g_zoneMapVisible)
            {
                g_ffxiMainMenu.open = false;
                g_clickMoveActive = false;
                InputController::EndDrag(g_input);
            }
            InvalidateRect(window, NULL, FALSE);
        }
        break;
    case InputController::Action::ToggleBumpMapping:
        if (IsGameMode() && !g_ffxiMainMenu.open)
        {
            g_applicationSettings.enableBumpMapping =
                !g_applicationSettings.enableBumpMapping;
            SyncSettingsMenuChecks();
            ConfigDialog::Sync(g_configDialog);
            InvalidateRect(window, NULL, FALSE);
        }
        break;
    case InputController::Action::ToggleBumpMappingInversion:
        g_applicationSettings.invertBumpMapping =
            !g_applicationSettings.invertBumpMapping;
        SaveExtendedGraphicsSettings();
        ConfigDialog::Sync(g_configDialog);
        InvalidateRect(g_hWnd, NULL, FALSE);
        break;
    case InputController::Action::DecreaseBumpMappingIntensity:
    case InputController::Action::IncreaseBumpMappingIntensity:
        if (IsGameMode() && !g_ffxiMainMenu.open)
        {
            const int direction = action ==
                InputController::Action::IncreaseBumpMappingIntensity ? 1 : -1;
            g_applicationSettings.bumpMappingIntensityPercent =
                ApplicationSettings::ClampBumpMappingIntensity(
                    g_applicationSettings.bumpMappingIntensityPercent + direction * 10);
            ConfigDialog::Sync(g_configDialog);
            InvalidateRect(window, NULL, FALSE);
        }
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
    if (msg == WM_COPYDATA && g_companionViewerMode)
    {
        const COPYDATASTRUCT* copyData = reinterpret_cast<const COPYDATASTRUCT*>(lParam);
        if (!copyData || !copyData->lpData)
            return FALSE;

        SetCompanionViewerOwner(static_cast<DWORD>(wParam));
        if (copyData->dwData == kCompanionModelCopyDataId && copyData->cbData >= 2)
        {
            const char* payload = static_cast<const char*>(copyData->lpData);
            const size_t payloadSize = copyData->cbData;
            const size_t labelLength = strnlen_s(payload, payloadSize);
            if (labelLength >= payloadSize)
                return FALSE;
            const char* datPath = payload + labelLength + 1;
            const size_t datCapacity = payloadSize - labelLength - 1;
            const size_t datLength = strnlen_s(datPath, datCapacity);
            if (datLength >= datCapacity || datLength == 0)
                return FALSE;

            g_startupModelLabel.assign(payload, labelLength);
            g_startupModelDat.assign(datPath, datLength);
            g_startupPlayerRace = -1;
            g_startupCreationModel = -1;
            if (g_graphicsRuntime.IsInitialized())
            {
                const char* label = g_startupModelLabel.empty() ? "Companion" : g_startupModelLabel.c_str();
                const FFXIStandaloneModelEntry entry = { label, g_startupModelDat.c_str() };
                LoadStandaloneModelEntry(&entry, "model");
            }
        }
        else if (copyData->dwData == kPlayerRaceCopyDataId &&
                 copyData->cbData == sizeof(PlayerViewerRequest))
        {
            const PlayerViewerRequest& request =
                *static_cast<const PlayerViewerRequest*>(copyData->lpData);
            g_playerEquip = request.equipment;
            g_playerFaceVariant = request.faceVariant;
            ClampLowPolyState();
            g_startupPlayerRace = g_playerEquip.raceIndex;
            g_startupPlayerCustomization = SerializePlayerViewerRequest(request);
            g_startupCreationModel = -1;
            g_startupModelDat.clear();
            if (g_graphicsRuntime.IsInitialized())
                LoadPlayerRaceModel(g_startupPlayerRace);
        }
        else if (copyData->dwData == kCreationModelCopyDataId && copyData->cbData == sizeof(int))
        {
            g_startupCreationModel = *static_cast<const int*>(copyData->lpData);
            g_startupPlayerRace = -1;
            g_startupModelDat.clear();
            if (g_graphicsRuntime.IsInitialized() &&
                SetHighPolyCreationSelectionFromFlatIndex(g_startupCreationModel))
                LoadCreationEntryInModelViewer(CurrentHighPolyCreationEntry());
        }
        else
        {
            return FALSE;
        }

        ShowWindow(hWnd, SW_RESTORE);
        SetForegroundWindow(hWnd);
        return TRUE;
    }

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
        if (g_companionViewerMode && g_input.dragMode == InputController::DragMode::Pan)
        {
            EndMouseLook();
            return 0;
        }
        if (IsEditMode() && !g_titleScreenActive && !g_nationSelectActive)
        {
            POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            const bool additive = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if (SelectZoneObjectAtClientPoint(hWnd, point, additive))
                return 0;
            break;
        }
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
                g_clickMoveActive = false;
                g_npcInteraction.selected = 0;
                g_doors.Click(doorHit.door, g_player.position);
                return 0;
            }
            g_doors.selected = -1;
            const bool npcTalk = g_npcInteraction.Click((float)point.x, (float)point.y);
            if (npcTalk)
                g_clickMoveActive = false;
            if (!npcTalk)
                EndNpcConversation();
            if (npcTalk)
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
                        if (!g_activeCharacterDataPath.empty() && g_loadedZoneId >= 0)
                        {
                            CharacterSaveData::Data characterData;
                            const bool haveCharacterData =
                                LoadOrCreateActiveCharacterData(characterData);
                            if (haveCharacterData && EnsureActiveCharacterModelFiles())
                            {
                                const CharacterSaveData::HomePoint point = {
                                    g_loadedZoneId, npc.placement.entityId,
                                    npc.placement.name
                                };
                                CharacterSaveData::RegisterHomePoint(characterData, point);
                                if (CharacterSaveData::Save(
                                        g_activeCharacterDataPath.c_str(), characterData))
                                {
                                    NpcChatWindow::Show(g_hWnd, "System",
                                        "Home Point registered. Character data has been saved.");
                                    StartHomePointTeleportChoice(
                                        g_hWnd, characterData, point);
                                }
                            }
                            else
                            {
                                NpcChatWindow::Show(g_hWnd, "System",
                                    "The character's save files could not be completed. The Home Point was not registered.");
                            }
                        }
                    }
                    return 0;
                }
                FFXINpcPlacement::Placement placement;
                if (FFXINpcPlacement::Find(g_npcInteraction.selected, placement))
                    StartNpcConversation(hWnd, placement);
            }
            if (!npcTalk && g_applicationSettings.movementStyle == ApplicationSettings::MovementClickToMove)
                SetClickMoveTargetFromViewportPoint(point);
            return 0;
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (g_companionViewerMode)
        {
            InputController::BeginPan(
                g_input, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        }
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
            const RECT panel = CharacterRosterPanelRect();
            if (PtInRect(&panel, point))
            {
                for (int row = 0; row < (int)g_savedCharacterPreviews.size(); ++row)
                {
                    const RECT rowRect = CharacterRosterRowRect(row);
                    if (PtInRect(&rowRect, point))
                    {
                        g_selectedCharacterPreview = row;
                        break;
                    }
                }
            }
            return 0;
        }
        if (IsGameMode() && !g_titleScreenActive && !g_nationSelectActive)
        {
            InputController::PlayerMouseButton(g_input, true, true,
                GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            if (g_applicationSettings.movementStyle == ApplicationSettings::MovementClickToMove)
            {
                int width = 0, height = 0;
                D3D9Device::GetViewportOrClientSize(GraphicsDevice(), hWnd, &width, &height);
                POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                SetClickMoveTargetFromViewportPoint(
                    D3D9Device::MapClientPointToViewport(hWnd, width, height, point));
            }
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
        if (g_companionViewerMode)
        {
            InputController::BeginOrbit(
                g_input, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        }
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
        if (g_companionViewerMode)
        {
            EndMouseLook();
            return 0;
        }
        if (IsGameMode())
        {
            InputController::PlayerMouseButton(g_input, false, false,
                GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        }
        EndMouseLook();
        return 0;

    case WM_MBUTTONDOWN:
        if (g_companionViewerMode)
        {
            InputController::BeginLightAzimuth(
                g_input, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        }
        if (g_highPolyCreationActive && !g_titleScreenActive)
        {
            InputController::BeginPan(
                g_input, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        }
        break;

    case WM_MBUTTONUP:
        if (g_input.dragMode == InputController::DragMode::Pan ||
            g_input.dragMode == InputController::DragMode::LightAzimuth)
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
            if (g_applicationSettings.movementStyle == ApplicationSettings::MovementClickToMove &&
                g_input.leftMouseHeld && !g_input.rightMouseHeld &&
                IsGameMode() && !g_titleScreenActive && !g_nationSelectActive)
            {
                int width = 0, height = 0;
                D3D9Device::GetViewportOrClientSize(GraphicsDevice(), hWnd, &width, &height);
                POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                SetClickMoveTargetFromViewportPoint(
                    D3D9Device::MapClientPointToViewport(hWnd, width, height, point));
            }
            TRACKMOUSEEVENT tme = {};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hWnd;
            TrackMouseEvent(&tme);
            if (g_titleScreenActive)
            {
                int uiW = 0, uiH = 0;
                D3D9Device::GetViewportOrClientSize(GraphicsDevice(), hWnd, &uiW, &uiH);
                POINT mouse = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                const int hovered = GameUiConfig_GetTitleMenuButtonIndex(
                    g_gameUiConfig.title, uiW, uiH,
                    D3D9Device::MapClientPointToViewport(hWnd, uiW, uiH, mouse));
                if (hovered >= 0)
                    g_titleMenuSelection = hovered;
            }
            if (delta.mode == InputController::DragMode::Pan)
            {
                OrbitCamera::Pan(g_orbitCamera, delta.x, delta.y);
            }
            else if (delta.mode == InputController::DragMode::Orbit)
            {
                OrbitCamera::Rotate(g_orbitCamera, delta.x, delta.y);
            }
            else if (delta.mode == InputController::DragMode::LightAzimuth)
            {
                g_modelViewerLightAzimuthDegrees += static_cast<float>(delta.x) * 0.5f;
                g_modelViewerLightElevationDegrees -= static_cast<float>(delta.y) * 0.5f;
                if (g_modelViewerLightAzimuthDegrees >= 360.0f ||
                    g_modelViewerLightAzimuthDegrees <= -360.0f)
                    g_modelViewerLightAzimuthDegrees = fmodf(
                        g_modelViewerLightAzimuthDegrees, 360.0f);
                if (g_modelViewerLightElevationDegrees >= 360.0f ||
                    g_modelViewerLightElevationDegrees <= -360.0f)
                    g_modelViewerLightElevationDegrees = fmodf(
                        g_modelViewerLightElevationDegrees, 360.0f);
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
        if ((GET_KEYSTATE_WPARAM(wParam) & MK_SHIFT) != 0)
        {
            OrbitCamera::RotateByTrackpadScroll(
                g_orbitCamera,
                0.0f,
                static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) /
                    static_cast<float>(WHEEL_DELTA));
            return 0;
        }
        OrbitCamera::Zoom(
            g_orbitCamera,
            static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) /
                static_cast<float>(WHEEL_DELTA));
        return 0;

    case WM_MOUSEHWHEEL:
        if (!g_developerConsoleOpen)
        {
            OrbitCamera::RotateByTrackpadScroll(
                g_orbitCamera,
                static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) /
                    static_cast<float>(WHEEL_DELTA),
                0.0f);
        }
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
        if (g_companionViewerMode && wParam == 'R')
        {
            if ((lParam & (1LL << 30)) == 0)
                g_modelViewerLightOverlayCorner =
                    (g_modelViewerLightOverlayCorner + 1) % 5;
            return 0;
        }
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
        if (wParam == '0' && (lParam & (1LL << 30)) == 0)
        {
            g_uiVisible = !g_uiVisible;
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        if (g_titleScreenActive)
        {
            if (wParam == VK_UP)
            {
                g_titleMenuSelection = (g_titleMenuSelection + 4) % 5;
                return 0;
            }
            if (wParam == VK_DOWN)
            {
                g_titleMenuSelection = (g_titleMenuSelection + 1) % 5;
                return 0;
            }
            if (wParam == VK_RETURN || wParam == VK_SPACE)
            {
                ActivateTitleButton(g_titleMenuSelection);
                return 0;
            }
            if (wParam == VK_ESCAPE)
            {
                g_titleMenuSelection = 4;
                ActivateTitleButton(g_titleMenuSelection);
                return 0;
            }
        }
        if (g_zoneMapVisible && wParam == VK_ESCAPE)
        {
            g_zoneMapVisible = false;
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }
        if (g_zoneMapVisible && wParam != 'M')
            return 0;
        if (IsGameMode() && (wParam == VK_UP || wParam == VK_DOWN) &&
            MoveNpcConversationChoice(hWnd, wParam == VK_UP ? -1 : 1))
        {
            return 0;
        }
        if (IsGameMode() && wParam == VK_ESCAPE && !g_npcConversation.choiceOptions.empty())
        {
            EndNpcConversation();
            return 0;
        }
        // FFXI consumes Escape from the chat log first.  Each press removes
        // one visible line; only a later press, after the log is hidden, may
        // open the main menu.
        if (wParam == VK_ESCAPE && IsGameMode() && NpcChatWindow::HandleEscape(lParam))
        {
            EndNpcConversation();
            return 0;
        }
        if (wParam == VK_ESCAPE && IsGameMode() &&
            (g_doors.selected >= 0 || g_npcInteraction.selected))
        {
            g_npcInteraction.selected = 0;
            EndNpcConversation();
            g_doors.selected = -1;
            return 0;
        }
        if (IsGameMode())
        {
            const bool menuKey = wParam == VK_SUBTRACT ||
                wParam == static_cast<WPARAM>(g_applicationSettings.keyMainMenu);
            const bool showMogHouse =
                g_loadedZoneLabel.find("Mog House") != std::string::npos;
            const FFXIMainMenu::Action menuAction = menuKey
                ? FFXIMainMenu::HandleKey(g_ffxiMainMenu, VK_ESCAPE, showMogHouse)
                : FFXIMainMenu::HandleKey(g_ffxiMainMenu,
                    static_cast<unsigned int>(wParam), showMogHouse);
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
    SetWindowTextW(g_hWnd, g_companionViewerMode ? kCompanionViewerTitle : kWindowTitle);
    if (g_companionViewerMode)
        SetCompanionViewerOwner(g_companionOwnerPid);
    InitFFXIPath();
    InitCustomTextureSettings();
    InitExtendedGraphicsSettings();
    InitTitleBackgroundSettings();
    InitKeyBindings();
    InputController::SetKeyBindings(g_input, g_applicationSettings);
    InputController::SetCursorStyle(
        g_input, g_applicationSettings.mouseCursorStyle, g_ffxiPath);
    FFXIBitmapFont::SetRootPath(g_ffxiPath);
    FFXIChatAssets::SetRootPath(g_ffxiPath);
    if (!g_companionViewerMode)
    {
        ApplicationMenu::Initialize(g_applicationMenu, g_hWnd, g_ffxiPath, g_themeState);
        SetMenu(g_hWnd,
            ApplicationMenu::Build(g_applicationMenu, g_applicationSettings, IsGameMode()));
        ConfigDialog::Initialize(
            g_configDialog, g_hWnd, g_ffxiPath, g_applicationSettings, g_themeState, g_gameUiConfig,
            HandleConfigDialogEvent, ConfigDialogIsGameMode);
        LowPolyCharacterPanel::Initialize(
            g_lowPolyPanel, g_hWnd, g_playerEquip, g_playerFaceVariant,
            g_themeState, HandleLowPolyPanelEvent);
        HighPolyCreationPanel::Initialize(
            g_highPolyCreationPanel, g_hWnd, g_creationSelection,
            g_creationAnimationIndex, g_creationAnimatedCamera,
            g_creationCharacterName, sizeof(g_creationCharacterName),
            HandleHighPolyCreationPanelEvent);
        ZoneObjectPanel::Initialize(
            g_zoneObjectPanel, g_hWnd, g_themeState, HandleZoneObjectPanelEvent);
    }

    // Interaction mode is application-wide. Apply its initial camera state once
    // without making any individual screen or asset load choose a mode.
    g_player.cameraActive = IsGameMode();
}

static bool InitializeGraphicsState()
{
    if (!InitD3D())
        return false;

    if (g_companionViewerMode && !g_startupPlayerCustomization.empty())
    {
        PlayerViewerRequest request = {};
        if (DeserializePlayerViewerRequest(g_startupPlayerCustomization.c_str(), request))
        {
            g_playerEquip = request.equipment;
            g_playerFaceVariant = request.faceVariant;
            ClampLowPolyState();
            LoadPlayerRaceModel(g_playerEquip.raceIndex);
        }
    }
    else if (g_companionViewerMode && g_startupPlayerRace >= 0)
    {
        LoadPlayerRaceModel(g_startupPlayerRace);
    }
    else if (g_companionViewerMode && g_startupCreationModel >= 0)
    {
        if (SetHighPolyCreationSelectionFromFlatIndex(g_startupCreationModel))
            LoadCreationEntryInModelViewer(CurrentHighPolyCreationEntry());
    }
    else if (g_companionViewerMode && !g_startupModelDat.empty())
    {
        const char* label = g_startupModelLabel.empty()
            ? "Companion"
            : g_startupModelLabel.c_str();
        const FFXIStandaloneModelEntry entry = { label, g_startupModelDat.c_str() };
        LoadStandaloneModelEntry(&entry, "model");
    }
    else
    {
        LoadTitleScreen();
    }
    return true;
}

static void RunApplicationFrame(void*, const float deltaSeconds)
{
    if (g_companionViewerMode && g_companionOwnerProcess &&
        WaitForSingleObject(g_companionOwnerProcess, 0) == WAIT_OBJECT_0)
    {
        PostMessageW(g_hWnd, WM_CLOSE, 0, 0);
        return;
    }

    UpdateAdaptivePlayDrawDistance(deltaSeconds, IsGameMode());

    if (deltaSeconds > 0.0f)
    {
        if (g_titleScreenActive)
        {
            g_titleCameraRailTime += deltaSeconds;
            if (!SampleRetailTitleCamera())
                SampleTitleCameraRail();
        }
        g_doorPhysicsUpdated = false;
        g_doors.Update(deltaSeconds, g_zoneCollisionMesh);
        if (g_loadedZoneId == ZoneElevator::kMetalworksZone &&
            ZoneElevator::Update(g_metalworksElevator, deltaSeconds,
                                 g_player.position, g_player.onGround,
                                 !g_applicationSettings.mirrorWorldZones))
        {
            ApplyMetalworksElevatorRuntimeCollision();
            g_player.verticalVelocity = 0.0f;
            g_player.jumping = false;
            PlayerController::SetLastSafePoint(g_player);
            UpdatePlayerCameraTarget();
        }
        else if (g_loadedZoneId == ZoneElevator::kMetalworksZone)
        {
            ApplyMetalworksElevatorRuntimeCollision();
        }
        const HWND soundForeground = GetForegroundWindow();
        const bool elevatorAudioAllowed = g_loadedZoneId == ZoneElevator::kMetalworksZone &&
            IsGameMode() && g_applicationSettings.enableSounds &&
            (g_applicationSettings.playSoundsInBackground || soundForeground == g_hWnd ||
             soundForeground == ConfigDialog::Window(g_configDialog) || AudioPlayer_OwnsWindow(soundForeground));
        g_metalworksElevatorAudio.Update(g_metalworksElevator, g_player.position,
            !g_applicationSettings.mirrorWorldZones, elevatorAudioAllowed);
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

    if (!g_companionViewerMode && g_hWnd && IsWindow(g_hWnd))
        SetMenu(g_hWnd, NULL);
    if (!g_companionViewerMode)
        ApplicationMenu::Release(g_applicationMenu);

    if (g_hWnd && IsWindow(g_hWnd))
        DestroyWindow(g_hWnd);
    g_hWnd = NULL;

    if (g_companionViewerMutex)
    {
        CloseHandle(g_companionViewerMutex);
        g_companionViewerMutex = NULL;
    }
    if (g_companionOwnerProcess)
    {
        CloseHandle(g_companionOwnerProcess);
        g_companionOwnerProcess = NULL;
    }

    Win32Theme::ReleaseState(g_themeState);
    UnregisterClassW(kWindowClassName, hInstance);
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE /*hPrevInstance*/,
                   _In_ LPSTR /*lpCmdLine*/, _In_ int nCmdShow)
{
    ParseStartupArguments();

    if (g_companionViewerMode)
    {
        g_companionViewerMutex = CreateMutexW(NULL, FALSE, kCompanionViewerMutexName);
        if (g_companionViewerMutex && GetLastError() == ERROR_ALREADY_EXISTS)
        {
            CloseHandle(g_companionViewerMutex);
            g_companionViewerMutex = NULL;

            HWND companionWindow = NULL;
            for (int attempt = 0; attempt < 100 && !companionWindow; ++attempt)
            {
                companionWindow = FindWindowW(kWindowClassName, kCompanionViewerTitle);
                if (!companionWindow)
                    Sleep(50);
            }

            if (companionWindow)
            {
                COPYDATASTRUCT copyData = {};
                int selectionIndex = -1;
                PlayerViewerRequest playerRequest = {};
                std::string payload;
                if (!g_startupPlayerCustomization.empty() &&
                    DeserializePlayerViewerRequest(
                        g_startupPlayerCustomization.c_str(), playerRequest))
                {
                    copyData.dwData = kPlayerRaceCopyDataId;
                    copyData.cbData = sizeof(playerRequest);
                    copyData.lpData = &playerRequest;
                }
                else if (g_startupPlayerRace >= 0)
                {
                    playerRequest.equipment = g_playerEquip;
                    playerRequest.equipment.raceIndex = g_startupPlayerRace;
                    playerRequest.faceVariant = g_playerFaceVariant;
                    copyData.dwData = kPlayerRaceCopyDataId;
                    copyData.cbData = sizeof(playerRequest);
                    copyData.lpData = &playerRequest;
                }
                else if (g_startupCreationModel >= 0)
                {
                    selectionIndex = g_startupCreationModel;
                    copyData.dwData = kCreationModelCopyDataId;
                    copyData.cbData = sizeof(selectionIndex);
                    copyData.lpData = &selectionIndex;
                }
                else
                {
                    payload = g_startupModelLabel.empty() ? "Companion" : g_startupModelLabel;
                    payload.push_back('\0');
                    payload += g_startupModelDat;
                    payload.push_back('\0');
                    copyData.dwData = kCompanionModelCopyDataId;
                    copyData.cbData = static_cast<DWORD>(payload.size());
                    copyData.lpData = &payload[0];
                }
                SendMessageA(companionWindow, WM_COPYDATA,
                             static_cast<WPARAM>(g_companionOwnerPid),
                             reinterpret_cast<LPARAM>(&copyData));
                ShowWindow(companionWindow, SW_RESTORE);
                SetForegroundWindow(companionWindow);
            }
            return 0;
        }
    }

    InitColorTheme();
    GameUiConfig_Load(g_gameUiConfig);
    NpcChatWindow::SetSize(g_gameUiConfig.chatLogWidthPercent, g_gameUiConfig.chatLogHeightPercent);
    NpcChatWindow::SetTimeout(g_gameUiConfig.chatLogTimeoutSeconds);
    NpcChatWindow::SetFont(g_gameUiConfig.chatLogFont, g_gameUiConfig.chatLogFontSize);
    Win32Application::InitializeCommonControls(
        ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES | ICC_TAB_CLASSES);

    Win32Application::WindowSpec windowSpec;
    windowSpec.instance = hInstance;
    windowSpec.windowProcedure = WndProc;
    windowSpec.className = kWindowClassName;
    windowSpec.title = g_companionViewerMode ? kCompanionViewerTitle : kWindowTitle;
    windowSpec.clientWidth = g_companionViewerMode ? kCompanionViewerWidth : kDefaultWidth;
    windowSpec.clientHeight = g_companionViewerMode ? kCompanionViewerHeight : kDefaultHeight;
    windowSpec.backgroundBrush = (HBRUSH)GetStockObject(BLACK_BRUSH);
    windowSpec.icon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DATURA));
    windowSpec.smallIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DATURA));
    windowSpec.hasMenu = !g_companionViewerMode;

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
