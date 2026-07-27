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
#include "companion_dat_table.h"
#include "ffxi_resource.h"
#include "bgw_player.h"
#include "audio_player.h"
#include "texture_viewer.h"
#include "resource.h"
#include <shlobj.h>     // SHBrowseForFolder, SHGetPathFromIDList
#include <commctrl.h>
#include <uxtheme.h>
#include <windowsx.h>
#include <cmath>
#include <cstdio>
#include <cctype>
#include <cfloat>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <algorithm>
#include <random>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "uxtheme.lib")

//========================================================================================
// Menu IDs
//========================================================================================

#define IDM_FILE_OPEN_DAT    1001
#define IDM_FILE_OPEN_DATSET 1002
#define IDM_FILE_EXIT        1003
#define IDM_FILE_RETURN_TITLE 1004

#define IDM_SETTINGS_SET_PATH      2001
#define IDM_SETTINGS_RESET_PATH    2002
#define IDM_SETTINGS_DETECT_PATH   2003
#define IDM_SETTINGS_SHOW_PATH     2004
#define IDM_SETTINGS_MIP_MAPPING   2005
#define IDM_SETTINGS_BUMP_MAPPING  2006
#define IDM_SETTINGS_ENV_ANIM      2007
#define IDM_SETTINGS_MIRROR_WORLD  2008
#define IDM_SETTINGS_CONFIG        2009

// Zone menu items: one ID per zone, offset by zone ID (0-299)
#define IDM_ZONE_BASE   3000

// High-poly character creation mesh menu items: sequential flat index across all races
#define IDM_CREATION_CHAR_BASE 4000

// Full player model menu items: one ID per race entry
#define IDM_PLAYER_BASE 5000
#define IDM_VIEW_TOGGLE_CAMERA 6001
#define IDM_VIEW_TOGGLE_EDIT_GAME_MODE 6001
#define IDM_PLAYER_CUSTOMIZE 6002
#define IDM_VIEW_ZONE_OBJECTS 6003
#define IDM_RESOURCE_CURRENT_ZONE 6101
#define IDM_RESOURCE_OPEN_DAT     6102
#define IDM_AUDIO_PLAYER          6201
#define IDM_AUDIO_STOP            6202
#define IDM_TEXTURE_VIEWER        6251
#define IDM_COMPANION_BROWSER     6261
#define IDM_PROTOTYPE_AREA_BASE   6300
#define IDM_NPC_MODEL_BASE       10000
#define IDM_MONSTER_MODEL_BASE   12000
#define IDC_PATH_DIALOG_OK 7001
#define IDC_ZONE_OBJECT_LIST 7201
#define IDC_ZONE_LABEL 7202
#define IDC_ZONE_PLACED_SHOW_ALL 7203
#define IDC_ZONE_PLACED_HIDE_ALL 7204
#define IDC_ZONE_UNREF_X 7205
#define IDC_ZONE_UNREF_Y 7206
#define IDC_ZONE_UNREF_Z 7207
#define IDC_ZONE_UNREF_RX 7208
#define IDC_ZONE_UNREF_RY 7209
#define IDC_ZONE_UNREF_RZ 7210
#define IDC_ZONE_UNREF_SX 7211
#define IDC_ZONE_UNREF_SY 7212
#define IDC_ZONE_UNREF_SZ 7213
#define IDC_ZONE_UNREF_APPLY 7214
#define IDC_ZONE_COLLISION_VISIBLE 7215
#define IDC_ZONE_LOADING_LABEL 7216
#define IDC_ZONE_SHOW_SELECTED 7217
#define IDC_ZONE_HIDE_SELECTED 7218
#define IDC_ZONE_HIGHLIGHT_SELECTED 7219
#define IDC_ZONE_CENTER_SELECTED 7220
#define IDC_ZONE_UNREF_OBJECT_LIST 7221
#define IDC_ZONE_COLLISION_OBJECT_LIST 7222
#define IDC_ZONE_UNREF_SHOW_ALL 7223
#define IDC_ZONE_UNREF_HIDE_ALL 7224
#define IDC_ZONE_TOOL_SELECT 7225
#define IDC_ZONE_TOOL_MOVE 7226
#define IDC_ZONE_TOOL_ROTATE 7227
#define IDC_ZONE_TOOL_SCALE 7228
#define IDC_ZONE_TOOL_TRANSFORM 7229
#define IDC_ZONE_STATUS_LABEL 7230
#define IDC_ZONE_DRAW_BATCH_LIST 7231
#define IDC_ZONE_DATA_TREE 7232
#define IDC_ZONE_RAW_DATA_TREE 7233
#define IDC_ZONE_COLLISION_DATA_TREE 7234
#define IDC_ZONE_COMBINE_TREE_TOGGLE 7236
#define IDC_RESOURCE_PATH_LABEL 7240
#define IDC_RESOURCE_LIST       7241
#define IDC_RESOURCE_STATUS     7242
#define IDC_RESOURCE_SEPARATE_TYPES 7243
#define IDC_RESOURCE_TYPE_TABS      7244
#define WM_ZONE_OBJECT_POPULATE (WM_APP + 31)

#define IDC_LP_RACE        8100
#define IDC_LP_MAIN_TYPE   8101
#define IDC_LP_MAIN_ITEM   8102
#define IDC_LP_SUB_TYPE    8103
#define IDC_LP_SUB_ITEM    8104
#define IDC_LP_RANGE_TYPE  8105
#define IDC_LP_RANGE_ITEM  8106
#define IDC_LP_FACE        8107
#define IDC_LP_HEAD        8108
#define IDC_LP_BODY        8109
#define IDC_LP_HANDS       8110
#define IDC_LP_LEGS        8111
#define IDC_LP_FEET        8112
#define IDC_LP_ANIM_BANK   8113
#define IDC_LP_ANIM_MODE   8114
#define IDC_LP_LOAD        8115
#define IDC_LP_SAVE        8116
#define IDC_LP_PLAY        8117
#define IDC_LP_STOP        8118
#define IDC_LP_RESET       8119
#define IDC_LP_RANDOM_CHARACTER 8120
#define IDC_LP_RANDOM_WEAPONS   8121
#define IDC_LP_RANDOM_ARMOR     8122
#define IDC_LP_RANDOM_ACTION    8123
#define IDC_LP_RANDOM_ALL       8124

#define IDC_HP_RACE        8200
#define IDC_HP_FACE        8201
#define IDC_HP_EQUIPMENT   8202
#define IDC_HP_NAME        8203
#define IDC_HP_SAVE        8204
#define IDC_HP_ANIMATION   8205
#define IDC_HP_NEXT        8206
#define IDC_HP_TITLE       8207
#define IDC_CONFIG_PATH_TEXT     8300
#define IDC_CONFIG_SET_PATH      8301
#define IDC_CONFIG_DETECT_PATH   8302
#define IDC_CONFIG_RESET_PATH    8303
#define IDC_CONFIG_SHOW_PATH     8304
#define IDC_CONFIG_MIP_MAPPING   8305
#define IDC_CONFIG_BUMP_MAPPING  8306
#define IDC_CONFIG_ENV_ANIM      8307
#define IDC_CONFIG_EDIT_GAME     8308
#define IDC_CONFIG_ZONE_OBJECTS  8310
#define IDC_CONFIG_CLOSE         8311
#define IDC_CONFIG_COLLISION     8312
#define IDC_CONFIG_WINDOW_MODE   8313
#define IDC_CONFIG_RESOLUTION    8314
#define IDC_CONFIG_ENABLE_SOUNDS 8315
#define IDC_CONFIG_BACKGROUND_SOUNDS 8316
#define IDC_CONFIG_MAX_SOUNDS    8317
#define IDC_CONFIG_HARDWARE_CURSOR 8318
#define IDC_CONFIG_MIRROR_WORLD    8319
#define IDC_CONFIG_DRAW_DISTANCE   8320
#define IDC_CONFIG_TEXTURE_COMPRESSION 8321
#define IDC_CONFIG_COLOR_THEME       8322
#define IDC_COMPANION_CATEGORY     8400
#define IDC_COMPANION_LIST         8401
#define IDC_COMPANION_PATH         8402
#define IDC_COMPANION_LOAD         8403
#define IDC_COMPANION_CLOSE        8404

//========================================================================================
// Window / D3D9 state
//========================================================================================

static HWND                  g_hWnd      = NULL;
static IDirect3D9           *g_pD3D      = NULL;
static IDirect3DDevice9     *g_pDevice   = NULL;
static IDirect3DPixelShader9 *g_pFfxiTexturePixelShader = NULL;
static bool                  g_ffxiTexturePixelShaderTried = false;
static IDirect3DPixelShader9 *g_pFfxiUiPixelShader = NULL;
static bool                  g_ffxiUiPixelShaderTried = false;
static D3DPRESENT_PARAMETERS g_d3dpp     = {};
static bool                  g_deviceLost = false;

static const int  kDefaultWidth     = 1280;
static const int  kDefaultHeight    = 720;
static const wchar_t kWindowClassName[] = L"FFXIViewerWndClass";
static const wchar_t kWindowTitle[]     = L"DATura - FFXI Model Viewer";
static const char kPathDialogClassName[] = "DATuraPathDialogClass";
static const char kConfigDialogClassName[] = "DATuraConfigDialogClass";

enum DATuraWindowMode
{
    kDATuraWindowMode_Windowed = 0,
    kDATuraWindowMode_Borderless = 1,
    kDATuraWindowMode_Fullscreen = 2
};

enum DATuraEnvironmentalAnimationMode
{
    kDATuraEnvAnim_Off = 0,
    kDATuraEnvAnim_Simple = 1,
    kDATuraEnvAnim_Smooth = 2
};

enum DATuraColorTheme
{
    kDATuraTheme_Dark = 0,
    kDATuraTheme_Light = 1
};

struct DATuraResolutionOption
{
    int width;
    int height;
    const char *label;
};

static const DATuraResolutionOption kResolutionOptions[] =
{
    { 1280, 720,  "1280 x 720" },
    { 1366, 768,  "1366 x 768" },
    { 1600, 900,  "1600 x 900" },
    { 1680, 1050, "1680 x 1050" },
    { 1920, 1080, "1920 x 1080" },
    { 2560, 1440, "2560 x 1440" },
};
static const int kResolutionOptionCount = (int)(sizeof(kResolutionOptions) / sizeof(kResolutionOptions[0]));

static const int kMaxSoundOptions[] = { 8, 16, 20, 32, 64, 128, -1 };
static const char *kMaxSoundOptionLabels[] =
{
    "8", "16", "20", "32", "64", "128", "Unlimited"
};
static const int kMaxSoundOptionCount = (int)(sizeof(kMaxSoundOptions) / sizeof(kMaxSoundOptions[0]));

static const float kDrawDistanceOptions[] = { 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f };
static const char *kDrawDistanceOptionLabels[] =
{
    "500", "1000", "2000", "4000", "8000"
};
static const int kDrawDistanceOptionCount =
    (int)(sizeof(kDrawDistanceOptions) / sizeof(kDrawDistanceOptions[0]));

// Loaded model state
static noesisModel_t *g_pZoneModel   = nullptr;
static noeRAPI_t     *g_pZoneRapi    = nullptr;
static noesisModel_t *g_pPlayerModel = nullptr;
static noeRAPI_t     *g_pPlayerRapi  = nullptr;
static noesisModel_t *g_pTitleLogoModel = nullptr;
static noeRAPI_t     *g_pTitleLogoRapi  = nullptr;
static noesisModel_t *g_pTitleLogoMarkModel = nullptr;
static noeRAPI_t     *g_pTitleLogoMarkRapi  = nullptr;
static noesisModel_t *g_pTitleUiModel   = nullptr;
static noeRAPI_t     *g_pTitleUiRapi    = nullptr;
static HWND           g_hLowPolyPanel = NULL;
static HWND           g_hHighPolyCreationPanel = NULL;
static HWND           g_hCompanionBrowser = NULL;
static HWND           g_hConfigDialog = NULL;
static HWND           g_hZoneObjectPanel = NULL;
static HWND           g_hZoneLabel = NULL;
static HWND           g_hZonePlacedListLabel = NULL;
static HWND           g_hZoneUnrefListLabel = NULL;
static HWND           g_hZoneCollisionListLabel = NULL;
static HWND           g_hZoneDrawBatchListLabel = NULL;
static HWND           g_hZoneObjectList = NULL;
static HWND           g_hZoneUnrefObjectList = NULL;
static HWND           g_hZoneCollisionObjectList = NULL;
static HWND           g_hZoneDrawBatchList = NULL;
static HWND           g_hZoneDataTree = NULL;
static HWND           g_hZoneRawDataTree = NULL;
static HWND           g_hZoneCollisionDataTree = NULL;
static HWND           g_hZoneLoadingLabel = NULL;
static HWND           g_hZonePlacedShowAllButton = NULL;
static HWND           g_hZonePlacedHideAllButton = NULL;
static HWND           g_hZoneUnrefShowAllButton = NULL;
static HWND           g_hZoneUnrefHideAllButton = NULL;
static HWND           g_hZoneShowSelectedButton = NULL;
static HWND           g_hZoneHideSelectedButton = NULL;
static HWND           g_hZoneHighlightSelectedButton = NULL;
static HWND           g_hZoneCenterSelectedButton = NULL;
static HWND           g_hZoneCombineTreeButton = NULL;
static HWND           g_hZoneToolButtons[5] = {};
static HWND           g_hZoneStatusLabel = NULL;
static HWND           g_hResourceBrowser = NULL;
static HWND           g_hResourcePathLabel = NULL;
static HWND           g_hResourceList = NULL;
static HWND           g_hResourceStatus = NULL;
static HWND           g_hResourceSeparateTypes = NULL;
static HWND           g_hResourceTypeTabs = NULL;
static bool           g_resourceSeparateByType = false;
static std::vector<FFXIResource::Row> g_resourceRows;
static std::vector<std::wstring> g_resourceKinds;
static std::wstring   g_resourceBaseStatus;
static HWND           g_hZoneUnrefEdits[9] = {};
static HWND           g_hZoneUnrefLabels[9] = {};
static HWND           g_hZoneUnrefApplyButton = NULL;
static HWND           g_hZoneCollisionVisibleCheck = NULL;
static bool           g_populatingZoneObjectList = false;
static int            g_zoneObjectColumnMode = -1;
static int            g_zoneUnrefObjectColumnMode = -1;
static int            g_zoneCollisionObjectColumnMode = -1;
static int            g_zoneDrawBatchColumnMode = -1;
static int            g_zoneTreeSelectedMapObjectIndex = -1;
static bool           g_zoneCombinedObjectTree = false;
static int            g_zonePaneWidth[3] = {};
static int            g_zoneDraggingSplitter = 0;
static char           g_loadedZoneLabel[MAX_PATH] = "No zone loaded";
static char           g_loadedZonePath[MAX_PATH] = {};
static char           g_loadedZoneName[128] = {};
static bool           g_loadedZoneUserContent = true;
static bool           g_loadedZoneRenderEnvironment = false;
static bool           g_loadedZoneRenderUnreferenced = false;
static int            g_windowMode = kDATuraWindowMode_Windowed;
static int            g_resolutionIndex = 0;
static int            g_environmentalAnimationMode = kDATuraEnvAnim_Smooth;
static bool           g_enableSounds = true;
static bool           g_playSoundsInBackground = true;
static int            g_maxSimultaneousSounds = -1;
static bool           g_enableHardwareMouseCursor = true;
static bool           g_enableMipMapping = true;
static bool           g_enableBumpMapping = false;
static bool           g_enableEnvironmentalAnimation = true;
static int            g_drawDistanceIndex = 2;
static bool           g_enableTextureCompression = true;
static int            g_colorTheme = kDATuraTheme_Dark;
static HBRUSH         g_hThemeWindowBrush = NULL;
static HBRUSH         g_hThemeControlBrush = NULL;
static HBRUSH         g_hThemeEditBrush = NULL;
static HFONT          g_hThemeFont = NULL;
static HFONT          g_hThemeSectionFont = NULL;
static HMENU          g_hMainMenu = NULL;

struct MenuVisualEntry
{
    std::string text;
    bool separator = false;
    bool hasSubmenu = false;
    bool menuBarItem = false;
};
static std::vector<std::unique_ptr<MenuVisualEntry> > g_menuVisualEntries;
// FFXI zone DAT coordinates need an X-axis flip to match the retail client.
// Leave that correction enabled when this option is off; checking the option
// deliberately displays the raw, mirrored orientation instead.
static bool           g_mirrorWorldZones = false;
static std::vector<std::string> g_hiddenZoneObjects;
static bool           g_showCollisionGeometry = false;
static std::string    g_highlightedZoneObject;
static HBRUSH         g_hZonePanelBrush = NULL;
static HBRUSH         g_hZoneControlBrush = NULL;
static HBRUSH         g_hZoneEditBrush = NULL;

struct NpcPlacementMarker
{
    char name[64];
    float pos[3];
    int confidence;
};
static std::vector<NpcPlacementMarker> g_npcPlacementMarkers;

static const COLORREF kZonePanelColor = RGB(50, 54, 56);
static const COLORREF kZoneControlColor = RGB(28, 29, 30);
static const COLORREF kZoneEditColor = RGB(61, 64, 66);
static const COLORREF kZoneTextColor = RGB(238, 241, 243);
static const COLORREF kZoneMutedTextColor = RGB(197, 204, 208);

struct ZoneDebugTransform
{
    float trans[3];
    float rot[3];
    float scale[3];
};
static std::map<std::string, ZoneDebugTransform> g_zoneObjectOverrides;

enum DATuraInteractionMode
{
    kDATuraMode_Edit = 0,
    kDATuraMode_Game = 1
};
static DATuraInteractionMode g_interactionMode = kDATuraMode_Edit;

// Orbit camera state
static float g_camYaw   =  0.0f;   // radians
static float g_camPitch =  0.25f;  // radians (positive = looking slightly down)
static float g_camDist  =  5.0f;   // distance from target
static float g_camTarget[3] = { 0.0f, 0.0f, 0.0f };
static LARGE_INTEGER g_perfFreq    = {};
static LARGE_INTEGER g_lastFrame   = {};

// Mouse drag state
static bool  g_mouseDown  = false;
static bool  g_mousePanDown = false;
static bool  g_cursorHidden = false;
static POINT g_lastMouse  = {};
static POINT g_mouseClient = { -1, -1 };

// Player/avatar camera state.  This is intentionally simple for now: it lets a
// composed character model walk through the loaded zone while the old fly/orbit
// camera remains available.
static bool  g_playerCamera = false;
static float g_playerPos[3] = { 0.0f, 0.0f, 0.0f };
static float g_playerYaw    = 0.0f;
static float g_playerGroundOffset = 0.0f;
static const float kPlayerFootContactAdjust = 0.212f;
static float g_playerAnimTime = 0.0f;
static float g_playerVelY = 0.0f;
static bool  g_playerOnGround = false;
static float g_playerRespawnPos[3] = { 0.0f, 0.0f, 0.0f };
static float g_playerRespawnYaw = 0.0f;
static bool  g_havePlayerRespawn = false;
static float g_playerLastSafePos[3] = { 0.0f, 0.0f, 0.0f };
static float g_playerLastSafeYaw = 0.0f;
static bool  g_havePlayerLastSafe = false;

struct ZoneCollisionTriangle
{
    float p[3][3];
    float normal[3];
    float minX, maxX;
    float minY, maxY;
    float minZ, maxZ;
};

static std::vector<ZoneCollisionTriangle> g_zoneCollisionTris;
static std::map<long long, std::vector<int> > g_zoneCollisionGrid;
static float g_zoneCollisionMin[3] = { 0.0f, 0.0f, 0.0f };
static float g_zoneCollisionMax[3] = { 0.0f, 0.0f, 0.0f };
static bool  g_haveZoneCollisionBounds = false;
static const float kCollisionGridCellSize = 8.0f;
static const float kPlayerCollisionRadius = 0.72f;
static const float kPlayerStepHeight = 0.85f;
static const float kPlayerCollisionHeight = 3.2f;

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
static int  g_gameModeMusicId = 0;

static int g_creationRaceIndex = 0;
static int g_creationFaceIndex = 0;
static int g_creationEquipmentIndex = 0;
static int g_creationAnimationIndex = 0;
static char g_creationCharacterName[64] = "Adventurer";
static int  g_playerFaceVariant = 0;

struct CreationSqleMotionInfo
{
    bool valid;
    bool frameChannel;
    float timeSeconds;
    int frameCount;
    int channelCount;
    std::vector<float> frameValues;
};

static CreationSqleMotionInfo g_creationBodyMotion = {};
static CreationSqleMotionInfo g_creationHeadMotion = {};
static float g_creationAnimTime = 0.0f;

struct PlayerEquipState
{
    int raceIndex;
    int mainType;
    int mainItem;
    int subType;
    int subItem;
    int rangedType;
    int rangedItem;
    int headItem;
    int bodyItem;
    int handsItem;
    int legsItem;
    int feetItem;
    int animationBank;
    int animationMode;
    bool animationPlaying;
};

static PlayerEquipState g_playerEquip =
{
    0,  // raceIndex
    1, 1, // main: Sword, first weapon DAT
    0, 0, // sub
    0, 0, // ranged
    0, 0, 0, 0, 0, // armor
    0, 0, false
};

static const char *kWeaponTypeLabels[] =
{
    "(All)", "Sword", "Dagger", "Axe", "Scythe", "Polearm", "Katana", "Club", "Staff", "Bow", "Gun", "Shield"
};
static const int kWeaponTypeCount = (int)(sizeof(kWeaponTypeLabels) / sizeof(kWeaponTypeLabels[0]));

static const char *kAnimationModeLabels[] =
{
    "Battle",
    "Emote",
    "General",
    "Ability",
    "Hand-to-Hand",
    "Dagger",
    "Sword",
    "Club",
    "Axe",
    "Katana",
    "G. Sword",
    "Staff",
    "G. Axe",
    "G. Katana",
    "Scythe",
    "Polearm",
    "Archery",
    "Marksmanship",
    "Mannequin",
    "Others",
    "NPC WS",
    "Dancer",
    "unknown",
    "Motion",
    "Weapon Skills",
    "All"
};
static const int kAnimationModeCount = (int)(sizeof(kAnimationModeLabels) / sizeof(kAnimationModeLabels[0]));

struct LowPolyAnimationCategory
{
    const char *label;
    int slot;
    const char *prefix;
};

static const LowPolyAnimationCategory kLowPolyAnimationCategories[] =
{
    { "Battle",        kFFXIInternalPCSlot_Action, "Battle" },
    { "Emote",         kFFXIInternalPCSlot_Action, "Emote" },
    { "General",       kFFXIInternalPCSlot_Action, "General" },
    { "Ability",       kFFXIInternalPCSlot_Action, "Ability" },
    { "Hand-to-Hand",  kFFXIInternalPCSlot_Action, "Hand-to-Hand" },
    { "Dagger",        kFFXIInternalPCSlot_Action, "Dagger" },
    { "Sword",         kFFXIInternalPCSlot_Action, "Sword" },
    { "Club",          kFFXIInternalPCSlot_Action, "Club" },
    { "Axe",           kFFXIInternalPCSlot_Action, "Axe" },
    { "Katana",        kFFXIInternalPCSlot_Action, "Katana" },
    { "G. Sword",      kFFXIInternalPCSlot_Action, "G. Sword" },
    { "Staff",         kFFXIInternalPCSlot_Action, "Staff" },
    { "G. Axe",        kFFXIInternalPCSlot_Action, "G. Axe" },
    { "G. Katana",     kFFXIInternalPCSlot_Action, "G. Katana" },
    { "Scythe",        kFFXIInternalPCSlot_Action, "Scythe" },
    { "Polearm",       kFFXIInternalPCSlot_Action, "Polearm" },
    { "Archery",       kFFXIInternalPCSlot_Action, "Archery" },
    { "Marksmanship",  kFFXIInternalPCSlot_Action, "Marksmanship" },
    { "Mannequin",     kFFXIInternalPCSlot_Action, "Mannequin" },
    { "Others",        kFFXIInternalPCSlot_Action, "Others" },
    { "NPC WS",        kFFXIInternalPCSlot_Action, "NPC WS" },
    { "Dancer",        kFFXIInternalPCSlot_Action, "Dancer" },
    { "unknown",       kFFXIInternalPCSlot_Action, "unknown" },
    { "Motion",        kFFXIInternalPCSlot_Motion, nullptr },
    { "Weapon Skills", kFFXIInternalPCSlot_WS,     nullptr },
    { "All",           kFFXIInternalPCSlot_Action, nullptr },
};

static const int kArmorVariantCount = 128;
static const int kWeaponVariantCount = 128;
static const int kFaceVariantCount = 16;

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
static const char *CreationPreviewWalkBodyDatForRace(int raceIndex);
static const char *CreationPreviewWalkHeadDatForRace(int raceIndex);
static bool ReadSqleMotionInfo(const char *animDat, CreationSqleMotionInfo *outInfo);
static HWND HighPolyCreationPanelControl(int id);
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
static void SetZoneObjectHidden(const char *name, bool hidden);
static bool CollisionFloorAt(float x, float z, float minY, float maxY, float *outY, float outNormal[3]);
static void LoadNpcPlacementMarkersForZonePath(const char *path);
static void DrawNpcPlacementMarkers();
static void ReloadRememberedZone();
static bool IsGameMode();
static void ToggleEditGameMode();
static const char *EnvironmentalAnimationModeName();
static void SetEnvironmentalAnimationMode(int mode);
static void ApplyDisplaySettings();
static void ApplyColorTheme(HWND hWnd);
static void SetColorTheme(int theme);
static void RefreshMainMenuTheme();
static int MaxSoundOptionIndexFromValue(int value);
static void SyncAppMusic();
static void SetCameraCursorHidden(bool hidden);
static void LoadTitleScreen();
static void BeginNationSelectScene();
static bool EnsureNationSelectAssets();
static void ReturnToTitleScreen();
static void LoadSelectedNationScene();
static void ConfirmExitFromTitleScreen();
static void ShowTitleConfigDialog();

//========================================================================================
// FFXI path management
//========================================================================================

// Registry keys for the PlayOnline installation path.
static const char kPlayOnlineRegKey[]    = "SOFTWARE\\PlayOnlineUS\\1000";
static const char kPlayOnlineRegKeyWow[] = "SOFTWARE\\WOW6432Node\\PlayOnlineUS\\1000";
static const char kPlayOnlineValue[]     = "InstallFolder";

// Try to read a REG_SZ value from HKLM. Returns true and fills outBuf on success.
static bool ReadHKLMString(const char *keyPath, const char *valueName,
                            char *outBuf, DWORD outBufSize)
{
    HKEY hKey = NULL;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, keyPath, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;

    DWORD type = 0, size = outBufSize;
    bool ok = (RegQueryValueExA(hKey, valueName, NULL, &type,
                                (LPBYTE)outBuf, &size) == ERROR_SUCCESS)
              && (type == REG_SZ || type == REG_EXPAND_SZ);
    RegCloseKey(hKey);
    return ok;
}

static bool IsDarkColorTheme()
{
    return g_colorTheme == kDATuraTheme_Dark;
}

static COLORREF ThemeWindowColor()
{
    return IsDarkColorTheme() ? RGB(27, 33, 39) : RGB(244, 246, 248);
}

static COLORREF ThemeControlColor()
{
    return IsDarkColorTheme() ? RGB(34, 40, 46) : RGB(255, 255, 255);
}

static COLORREF ThemeEditColor()
{
    return IsDarkColorTheme() ? RGB(42, 48, 55) : RGB(255, 255, 255);
}

static COLORREF ThemeTextColor()
{
    return IsDarkColorTheme() ? RGB(226, 231, 236) : RGB(31, 36, 41);
}

static COLORREF ThemeMutedTextColor()
{
    return IsDarkColorTheme() ? RGB(151, 161, 171) : RGB(91, 99, 107);
}

static void RecreateThemeResources()
{
    if (g_hThemeWindowBrush)
        DeleteObject(g_hThemeWindowBrush);
    if (g_hThemeControlBrush)
        DeleteObject(g_hThemeControlBrush);
    if (g_hThemeEditBrush)
        DeleteObject(g_hThemeEditBrush);
    g_hThemeWindowBrush = CreateSolidBrush(ThemeWindowColor());
    g_hThemeControlBrush = CreateSolidBrush(ThemeControlColor());
    g_hThemeEditBrush = CreateSolidBrush(ThemeEditColor());

    if (!g_hThemeFont)
    {
        g_hThemeFont = CreateFontA(
            -15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    }
    if (!g_hThemeSectionFont)
    {
        g_hThemeSectionFont = CreateFontA(
            -15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    }
}

typedef HRESULT (WINAPI *DwmSetWindowAttributeFn)(HWND, DWORD, LPCVOID, DWORD);
typedef int (WINAPI *SetPreferredAppModeFn)(int);
typedef void (WINAPI *FlushMenuThemesFn)();

static void ApplyNativeMenuTheme()
{
    // Native HMENU popup windows do not inherit SetWindowTheme from their
    // owner. Windows exposes these uxtheme entry points specifically so the
    // menu bar, nested popup menus, separators, arrows, and checkmarks all use
    // the same application color mode. Resolve them dynamically to retain
    // compatibility with Windows versions that predate application dark mode.
    HMODULE hUxTheme = LoadLibraryW(L"uxtheme.dll");
    if (!hUxTheme)
        return;

    SetPreferredAppModeFn setPreferredAppMode =
        (SetPreferredAppModeFn)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(135));
    FlushMenuThemesFn flushMenuThemes =
        (FlushMenuThemesFn)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(136));
    if (setPreferredAppMode)
    {
        const int kPreferredAppModeAllowDark = 1;
        const int kPreferredAppModeForceLight = 3;
        setPreferredAppMode(IsDarkColorTheme()
            ? kPreferredAppModeAllowDark
            : kPreferredAppModeForceLight);
    }
    if (flushMenuThemes)
        flushMenuThemes();

    FreeLibrary(hUxTheme);
}

static void ApplyWindowChromeTheme(HWND hWnd)
{
    if (!hWnd)
        return;

    HMODULE hDwmApi = LoadLibraryW(L"dwmapi.dll");
    DwmSetWindowAttributeFn setAttribute = hDwmApi
        ? (DwmSetWindowAttributeFn)GetProcAddress(hDwmApi, "DwmSetWindowAttribute")
        : NULL;
    if (setAttribute)
    {
        const BOOL useDark = IsDarkColorTheme() ? TRUE : FALSE;
        // Attribute 20 is DWMWA_USE_IMMERSIVE_DARK_MODE on current Windows 10/11.
        // Attribute 19 covers the earlier Windows 10 implementation.
        if (FAILED(setAttribute(hWnd, 20, &useDark, sizeof(useDark))))
            setAttribute(hWnd, 19, &useDark, sizeof(useDark));

        // Ask Windows 11 for the same softly rounded top-level corners used by
        // the CEXI reference. Older versions simply ignore this attribute.
        const DWORD roundedCorners = 2; // DWMWCP_ROUND
        setAttribute(hWnd, 33, &roundedCorners, sizeof(roundedCorners));
    }
    if (hDwmApi)
        FreeLibrary(hDwmApi);
}

static BOOL CALLBACK ApplyThemeToChild(HWND hChild, LPARAM)
{
    wchar_t className[32] = {};
    GetClassNameW(hChild, className, (int)(sizeof(className) / sizeof(className[0])));
    const bool isComboBox = _wcsicmp(className, L"ComboBox") == 0;
    const wchar_t *subApp = IsDarkColorTheme()
        ? (isComboBox ? L"DarkMode_CFD" : L"DarkMode_Explorer")
        : L"Explorer";
    SetWindowTheme(hChild, subApp, NULL);
    const LONG_PTR style = GetWindowLongPtr(hChild, GWL_STYLE);
    const bool isGroupBox =
        _wcsicmp(className, L"Button") == 0 &&
        (style & BS_TYPEMASK) == BS_GROUPBOX;
    HFONT font = isGroupBox ? g_hThemeSectionFont : g_hThemeFont;
    if (font)
        SendMessage(hChild, WM_SETFONT, (WPARAM)font, TRUE);
    InvalidateRect(hChild, NULL, TRUE);
    return TRUE;
}

static void ApplyColorTheme(HWND hWnd)
{
    if (!hWnd)
        return;
    if (!g_hThemeWindowBrush)
        RecreateThemeResources();
    SetClassLongPtr(hWnd, GCLP_HBRBACKGROUND, (LONG_PTR)g_hThemeWindowBrush);
    ApplyWindowChromeTheme(hWnd);
    SetWindowTheme(hWnd, IsDarkColorTheme() ? L"DarkMode_Explorer" : L"Explorer", NULL);
    SendMessage(hWnd, WM_THEMECHANGED, 0, 0);
    EnumChildWindows(hWnd, ApplyThemeToChild, 0);
    InvalidateRect(hWnd, NULL, TRUE);
    DrawMenuBar(hWnd);
}

static void SaveColorTheme()
{
    HKEY hKey = NULL;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, kAppRegKey, 0, NULL,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL,
                        &hKey, NULL) != ERROR_SUCCESS)
        return;
    const DWORD value = (DWORD)g_colorTheme;
    RegSetValueExA(hKey, kAppThemeValue, 0, REG_DWORD,
                   (const BYTE *)&value, sizeof(value));
    RegCloseKey(hKey);
}

static void InitColorTheme()
{
    HKEY hKey = NULL;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, kAppRegKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        DWORD type = 0;
        DWORD value = 0;
        DWORD size = sizeof(value);
        if (RegQueryValueExA(hKey, kAppThemeValue, NULL, &type,
                             (LPBYTE)&value, &size) == ERROR_SUCCESS &&
            type == REG_DWORD && value <= (DWORD)kDATuraTheme_Light)
        {
            g_colorTheme = (int)value;
        }
        RegCloseKey(hKey);
    }
    ApplyNativeMenuTheme();
    RecreateThemeResources();
}

static void SetColorTheme(int theme)
{
    g_colorTheme = (theme == kDATuraTheme_Light)
        ? kDATuraTheme_Light
        : kDATuraTheme_Dark;
    SaveColorTheme();
    ApplyNativeMenuTheme();
    RecreateThemeResources();
    ApplyColorTheme(g_hWnd);
    ApplyColorTheme(g_hConfigDialog);
    RefreshMainMenuTheme();
}

// Probe the PlayOnline registry entries (native key first, then WOW6432Node).
// Returns true and fills outBuf on success; outBuf is unchanged on failure.
static bool DetectFFXIInstallPath(char *outBuf, DWORD outBufSize)
{
    if (ReadHKLMString(kPlayOnlineRegKey,    kPlayOnlineValue, outBuf, outBufSize)) return true;
    if (ReadHKLMString(kPlayOnlineRegKeyWow, kPlayOnlineValue, outBuf, outBufSize)) return true;
    return false;
}

// Load a previously saved custom path from HKCU\Software\FFXIViewer.
// Returns true and fills outBuf on success.
static bool LoadSavedCustomPath(char *outBuf, DWORD outBufSize)
{
    HKEY hKey = NULL;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, kAppRegKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;

    DWORD type = 0;
    DWORD size = outBufSize;
    bool ok = (RegQueryValueExA(hKey, kAppPathValue, NULL, &type,
                                (LPBYTE)outBuf, &size) == ERROR_SUCCESS)
              && (type == REG_SZ);
    RegCloseKey(hKey);
    return ok;
}

// Persist a custom path to HKCU\Software\FFXIViewer so it survives restarts.
static void SaveCustomPath(const char *path)
{
    HKEY hKey = NULL;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, kAppRegKey, 0, NULL,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL,
                        &hKey, NULL) != ERROR_SUCCESS)
        return;

    RegSetValueExA(hKey, kAppPathValue, 0, REG_SZ,
                   (const BYTE *)path, (DWORD)(strlen(path) + 1));
    RegCloseKey(hKey);
}

// Delete the saved custom path from the registry, reverting to the hard-coded default.
static void ClearCustomPath()
{
    HKEY hKey = NULL;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, kAppRegKey, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS)
        return;
    RegDeleteValueA(hKey, kAppPathValue);
    RegCloseKey(hKey);
}

// Initialize g_ffxiPath at startup:
//   1. If the user previously saved a custom path, use that.
//   2. Otherwise fall back to the hard-coded default installation path.
static void InitFFXIPath()
{
    if (LoadSavedCustomPath(g_ffxiPath, sizeof(g_ffxiPath)))
        return; // user-saved override takes priority

    // Hard-coded default — no registry detection
    strcpy_s(g_ffxiPath, kDefaultFFXIPath);
}

// Open a folder-browser dialog so the user can manually choose the FFXI root.
// Saves the result and updates g_ffxiPath.
static void PromptSetFFXIPath()
{
    char displayName[MAX_PATH] = {};

    BROWSEINFOA bi    = {};
    bi.hwndOwner      = g_hWnd;
    bi.pszDisplayName = displayName;
    bi.lpszTitle      = "Select the FFXI root folder\n"
                        "(the folder that contains the ROM, ROM2, ROM3 \x85 subfolders)";
    bi.ulFlags        = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (!pidl)
        return; // user cancelled

    char chosen[MAX_PATH] = {};
    if (SHGetPathFromIDListA(pidl, chosen))
    {
        // Ensure the path ends with a backslash
        size_t len = strlen(chosen);
        if (len > 0 && chosen[len - 1] != '\\')
        {
            chosen[len]     = '\\';
            chosen[len + 1] = '\0';
        }

        strcpy_s(g_ffxiPath, chosen);
        SaveCustomPath(g_ffxiPath);
        AudioPlayer_SetRootPath(g_ffxiPath);
        TextureViewer_SetRootPath(g_ffxiPath);

        ShowFFXIPathInfoDialog("Path Saved", "FFXI path set to:", g_ffxiPath);
    }

    CoTaskMemFree(pidl);
}

// Reset to the hard-coded default path, clearing any saved custom path.
static void ResetFFXIPathToDefault()
{
    ClearCustomPath();
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
    if (DetectFFXIInstallPath(detected, sizeof(detected)))
    {
        strcpy_s(g_ffxiPath, detected);
        SaveCustomPath(g_ffxiPath);
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
    HMENU hMenu = g_hWnd ? GetMenu(g_hWnd) : NULL;
    if (!hMenu)
        return;

    CheckMenuItem(hMenu, IDM_SETTINGS_MIP_MAPPING,
                  MF_BYCOMMAND | (g_enableMipMapping ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, IDM_SETTINGS_BUMP_MAPPING,
                  MF_BYCOMMAND | (g_enableBumpMapping ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, IDM_SETTINGS_MIRROR_WORLD,
                  MF_BYCOMMAND | (g_mirrorWorldZones ? MF_CHECKED : MF_UNCHECKED));
    char envText[96] = {};
    sprintf_s(envText, "Vegetation Animation: %s", EnvironmentalAnimationModeName());
    ModifyMenuA(hMenu, IDM_SETTINGS_ENV_ANIM,
                MF_BYCOMMAND | MF_STRING |
                (g_environmentalAnimationMode != kDATuraEnvAnim_Off ? MF_CHECKED : MF_UNCHECKED),
                IDM_SETTINGS_ENV_ANIM, envText);
    CheckMenuItem(hMenu, IDM_VIEW_TOGGLE_EDIT_GAME_MODE,
                  MF_BYCOMMAND | (IsGameMode() ? MF_CHECKED : MF_UNCHECKED));
}

static const char *EnvironmentalAnimationModeName()
{
    switch (g_environmentalAnimationMode)
    {
    case kDATuraEnvAnim_Off:
        return "Off";
    case kDATuraEnvAnim_Simple:
        return "Simple";
    case kDATuraEnvAnim_Smooth:
    default:
        return "Smooth";
    }
}

static void SetEnvironmentalAnimationMode(int mode)
{
    if (mode < kDATuraEnvAnim_Off)
        mode = kDATuraEnvAnim_Off;
    if (mode > kDATuraEnvAnim_Smooth)
        mode = kDATuraEnvAnim_Smooth;
    g_environmentalAnimationMode = mode;
    g_enableEnvironmentalAnimation = (mode != kDATuraEnvAnim_Off);
}

static int MaxSoundOptionIndexFromValue(int value)
{
    for (int i = 0; i < kMaxSoundOptionCount; ++i)
    {
        if (kMaxSoundOptions[i] == value)
            return i;
    }
    return kMaxSoundOptionCount - 1;
}

static void ApplyTextureCompressionSetting()
{
    noeRAPI_t *rapis[] =
    {
        g_pZoneRapi,
        g_pPlayerRapi,
        g_pTitleLogoRapi,
        g_pTitleLogoMarkRapi,
        g_pTitleUiRapi
    };
    for (int i = 0; i < (int)(sizeof(rapis) / sizeof(rapis[0])); ++i)
    {
        if (!rapis[i])
            continue;
        bool alreadyApplied = false;
        for (int previous = 0; previous < i; ++previous)
            alreadyApplied = alreadyApplied || rapis[previous] == rapis[i];
        if (!alreadyApplied)
            rapis[i]->SetTextureCompressionEnabled(g_enableTextureCompression);
    }
}

static void ConfigureRapiTextureSettings(noeRAPI_t *pRapi)
{
    if (pRapi)
        pRapi->SetTextureCompressionEnabled(g_enableTextureCompression);
}

static void SyncConfigDialogControls(HWND hWnd)
{
    if (!hWnd)
        return;

    HWND hPath = GetDlgItem(hWnd, IDC_CONFIG_PATH_TEXT);
    if (hPath)
        SetWindowTextA(hPath, g_ffxiPath);

    SendDlgItemMessageA(hWnd, IDC_CONFIG_MIP_MAPPING, BM_SETCHECK,
                        g_enableMipMapping ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageA(hWnd, IDC_CONFIG_BUMP_MAPPING, BM_SETCHECK,
                        g_enableBumpMapping ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageA(hWnd, IDC_CONFIG_ENV_ANIM, CB_SETCURSEL, g_environmentalAnimationMode, 0);
    SendDlgItemMessageA(hWnd, IDC_CONFIG_WINDOW_MODE, CB_SETCURSEL, g_windowMode, 0);
    SendDlgItemMessageA(hWnd, IDC_CONFIG_RESOLUTION, CB_SETCURSEL, g_resolutionIndex, 0);
    SendDlgItemMessageA(hWnd, IDC_CONFIG_EDIT_GAME, BM_SETCHECK,
                        IsGameMode() ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageA(hWnd, IDC_CONFIG_COLLISION, BM_SETCHECK,
                        g_showCollisionGeometry ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageA(hWnd, IDC_CONFIG_ENABLE_SOUNDS, BM_SETCHECK,
                        g_enableSounds ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageA(hWnd, IDC_CONFIG_BACKGROUND_SOUNDS, BM_SETCHECK,
                        g_playSoundsInBackground ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageA(hWnd, IDC_CONFIG_MAX_SOUNDS, CB_SETCURSEL,
                        MaxSoundOptionIndexFromValue(g_maxSimultaneousSounds), 0);
    SendDlgItemMessageA(hWnd, IDC_CONFIG_HARDWARE_CURSOR, BM_SETCHECK,
                        g_enableHardwareMouseCursor ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageA(hWnd, IDC_CONFIG_MIRROR_WORLD, BM_SETCHECK,
                        g_mirrorWorldZones ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageA(hWnd, IDC_CONFIG_DRAW_DISTANCE, CB_SETCURSEL,
                        g_drawDistanceIndex, 0);
    SendDlgItemMessageA(hWnd, IDC_CONFIG_TEXTURE_COMPRESSION, CB_SETCURSEL,
                        g_enableTextureCompression ? 0 : 1, 0);
    SendDlgItemMessageA(hWnd, IDC_CONFIG_COLOR_THEME, CB_SETCURSEL,
                        g_colorTheme, 0);
}

static void DrawConfigPanel(HDC hdc, const RECT &panel, const char *title)
{
    HBRUSH fill = CreateSolidBrush(ThemeControlColor());
    HPEN border = CreatePen(PS_SOLID, 1,
        IsDarkColorTheme() ? RGB(57, 65, 73) : RGB(205, 211, 217));
    HGDIOBJ oldBrush = SelectObject(hdc, fill);
    HGDIOBJ oldPen = SelectObject(hdc, border);
    RoundRect(hdc, panel.left, panel.top, panel.right, panel.bottom, 12, 12);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(border);
    DeleteObject(fill);

    RECT titleRect = { panel.left + 14, panel.top + 5, panel.right - 12, panel.top + 24 };
    HFONT oldFont = (HFONT)SelectObject(hdc, g_hThemeSectionFont);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, ThemeTextColor());
    DrawTextA(hdc, title, -1, &titleRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    SelectObject(hdc, oldFont);
}

static void DrawConfigButton(const DRAWITEMSTRUCT *draw)
{
    if (!draw)
        return;

    char text[96] = {};
    GetWindowTextA(draw->hwndItem, text, sizeof(text));
    RECT rc = draw->rcItem;
    const bool accent = draw->CtlID == IDC_CONFIG_CLOSE;
    COLORREF fillColor;
    if (draw->itemState & ODS_DISABLED)
        fillColor = IsDarkColorTheme() ? RGB(43, 48, 53) : RGB(224, 227, 230);
    else if (draw->itemState & ODS_SELECTED)
        fillColor = accent ? RGB(42, 126, 211) :
            (IsDarkColorTheme() ? RGB(64, 73, 82) : RGB(215, 222, 228));
    else
        fillColor = accent ? RGB(62, 151, 245) :
            (IsDarkColorTheme() ? RGB(48, 56, 64) : RGB(231, 235, 239));

    HBRUSH fill = CreateSolidBrush(fillColor);
    HPEN border = CreatePen(PS_SOLID, 1,
        accent ? RGB(104, 180, 255) :
        (IsDarkColorTheme() ? RGB(75, 84, 93) : RGB(190, 198, 205)));
    HGDIOBJ oldBrush = SelectObject(draw->hDC, fill);
    HGDIOBJ oldPen = SelectObject(draw->hDC, border);
    RoundRect(draw->hDC, rc.left, rc.top, rc.right, rc.bottom, 8, 8);
    SelectObject(draw->hDC, oldPen);
    SelectObject(draw->hDC, oldBrush);
    DeleteObject(border);
    DeleteObject(fill);

    HFONT oldFont = (HFONT)SelectObject(draw->hDC, g_hThemeFont);
    SetBkMode(draw->hDC, TRANSPARENT);
    SetTextColor(draw->hDC,
        accent && !(draw->itemState & ODS_DISABLED)
            ? RGB(255, 255, 255)
            : ThemeTextColor());
    if (draw->itemState & ODS_SELECTED)
        OffsetRect(&rc, 0, 1);
    DrawTextA(draw->hDC, text, -1, &rc,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(draw->hDC, oldFont);
}

static LRESULT CALLBACK ConfigDialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        {
            HFONT hFont = g_hThemeFont;
            HWND hPathGroup = NULL;
            HWND hPath = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
                28, 31, 492, 24, hWnd, (HMENU)(INT_PTR)IDC_CONFIG_PATH_TEXT,
                GetModuleHandle(NULL), NULL);

            CreateWindowExA(0, "BUTTON", "Set Path...", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                28, 63, 110, 26, hWnd, (HMENU)(INT_PTR)IDC_CONFIG_SET_PATH,
                GetModuleHandle(NULL), NULL);
            CreateWindowExA(0, "BUTTON", "Auto-Detect", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                146, 63, 110, 26, hWnd, (HMENU)(INT_PTR)IDC_CONFIG_DETECT_PATH,
                GetModuleHandle(NULL), NULL);
            CreateWindowExA(0, "BUTTON", "Reset Default", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                264, 63, 110, 26, hWnd, (HMENU)(INT_PTR)IDC_CONFIG_RESET_PATH,
                GetModuleHandle(NULL), NULL);
            CreateWindowExA(0, "BUTTON", "Show Path", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                382, 63, 110, 26, hWnd, (HMENU)(INT_PTR)IDC_CONFIG_SHOW_PATH,
                GetModuleHandle(NULL), NULL);

            HWND hScreenGroup = NULL;
            CreateWindowExA(0, "STATIC", "Window Mode", WS_CHILD | WS_VISIBLE,
                24, 132, 100, 18, hWnd, NULL, GetModuleHandle(NULL), NULL);
            HWND hWindowMode = CreateWindowExA(0, "COMBOBOX", "",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                142, 128, 170, 120, hWnd, (HMENU)(INT_PTR)IDC_CONFIG_WINDOW_MODE,
                GetModuleHandle(NULL), NULL);
            SendMessageA(hWindowMode, CB_ADDSTRING, 0, (LPARAM)"Windowed");
            SendMessageA(hWindowMode, CB_ADDSTRING, 0, (LPARAM)"Borderless Window");
            SendMessageA(hWindowMode, CB_ADDSTRING, 0, (LPARAM)"Full Screen");

            CreateWindowExA(0, "STATIC", "Resolution", WS_CHILD | WS_VISIBLE,
                24, 162, 100, 18, hWnd, NULL, GetModuleHandle(NULL), NULL);
            HWND hResolution = CreateWindowExA(0, "COMBOBOX", "",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                142, 158, 170, 180, hWnd, (HMENU)(INT_PTR)IDC_CONFIG_RESOLUTION,
                GetModuleHandle(NULL), NULL);
            for (int i = 0; i < kResolutionOptionCount; ++i)
                SendMessageA(hResolution, CB_ADDSTRING, 0, (LPARAM)kResolutionOptions[i].label);

            HWND hRenderingGroup = NULL;
            CreateWindowExA(0, "BUTTON", "Enable MIP Mapping",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                24, 224, 220, 22, hWnd, (HMENU)(INT_PTR)IDC_CONFIG_MIP_MAPPING,
                GetModuleHandle(NULL), NULL);
            CreateWindowExA(0, "BUTTON", "Enable Bump Mapping",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                24, 248, 220, 22, hWnd, (HMENU)(INT_PTR)IDC_CONFIG_BUMP_MAPPING,
                GetModuleHandle(NULL), NULL);
            CreateWindowExA(0, "STATIC", "Vegetation Animation", WS_CHILD | WS_VISIBLE,
                24, 278, 120, 18, hWnd, NULL, GetModuleHandle(NULL), NULL);
            HWND hEnvAnim = CreateWindowExA(0, "COMBOBOX", "",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                160, 274, 128, 120, hWnd, (HMENU)(INT_PTR)IDC_CONFIG_ENV_ANIM,
                GetModuleHandle(NULL), NULL);
            SendMessageA(hEnvAnim, CB_ADDSTRING, 0, (LPARAM)"Off");
            SendMessageA(hEnvAnim, CB_ADDSTRING, 0, (LPARAM)"Simple");
            SendMessageA(hEnvAnim, CB_ADDSTRING, 0, (LPARAM)"Smooth");
            CreateWindowExA(0, "BUTTON", "Mirror world zones",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                300, 224, 190, 22, hWnd, (HMENU)(INT_PTR)IDC_CONFIG_MIRROR_WORLD,
                GetModuleHandle(NULL), NULL);
            CreateWindowExA(0, "STATIC", "Texture Storage", WS_CHILD | WS_VISIBLE,
                300, 252, 100, 18, hWnd, NULL, GetModuleHandle(NULL), NULL);
            HWND hTextureCompression = CreateWindowExA(0, "COMBOBOX", "",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                406, 248, 128, 100, hWnd,
                (HMENU)(INT_PTR)IDC_CONFIG_TEXTURE_COMPRESSION,
                GetModuleHandle(NULL), NULL);
            SendMessageA(hTextureCompression, CB_ADDSTRING, 0, (LPARAM)"Compressed");
            SendMessageA(hTextureCompression, CB_ADDSTRING, 0, (LPARAM)"Uncompressed");
            CreateWindowExA(0, "STATIC", "Draw Distance", WS_CHILD | WS_VISIBLE,
                300, 278, 100, 18, hWnd, NULL, GetModuleHandle(NULL), NULL);
            HWND hDrawDistance = CreateWindowExA(0, "COMBOBOX", "",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                406, 274, 128, 160, hWnd, (HMENU)(INT_PTR)IDC_CONFIG_DRAW_DISTANCE,
                GetModuleHandle(NULL), NULL);
            for (int i = 0; i < kDrawDistanceOptionCount; ++i)
                SendMessageA(hDrawDistance, CB_ADDSTRING, 0, (LPARAM)kDrawDistanceOptionLabels[i]);

            HWND hAudioGroup = NULL;
            CreateWindowExA(0, "BUTTON", "Enable Sounds",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                24, 342, 150, 22, hWnd, (HMENU)(INT_PTR)IDC_CONFIG_ENABLE_SOUNDS,
                GetModuleHandle(NULL), NULL);
            CreateWindowExA(0, "BUTTON", "Play sounds in background",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                24, 366, 210, 22, hWnd, (HMENU)(INT_PTR)IDC_CONFIG_BACKGROUND_SOUNDS,
                GetModuleHandle(NULL), NULL);
            CreateWindowExA(0, "STATIC", "Simultaneous SFX", WS_CHILD | WS_VISIBLE,
                300, 342, 120, 18, hWnd, NULL, GetModuleHandle(NULL), NULL);
            HWND hMaxSounds = CreateWindowExA(0, "COMBOBOX", "",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                426, 338, 108, 160, hWnd, (HMENU)(INT_PTR)IDC_CONFIG_MAX_SOUNDS,
                GetModuleHandle(NULL), NULL);
            for (int i = 0; i < kMaxSoundOptionCount; ++i)
                SendMessageA(hMaxSounds, CB_ADDSTRING, 0, (LPARAM)kMaxSoundOptionLabels[i]);
            CreateWindowExA(0, "BUTTON", "Enable Hardware Mouse Cursor",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                300, 366, 234, 22, hWnd, (HMENU)(INT_PTR)IDC_CONFIG_HARDWARE_CURSOR,
                GetModuleHandle(NULL), NULL);

            HWND hModeGroup = NULL;
            CreateWindowExA(0, "BUTTON", "Game Mode",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                24, 438, 160, 22, hWnd, (HMENU)(INT_PTR)IDC_CONFIG_EDIT_GAME,
                GetModuleHandle(NULL), NULL);
            CreateWindowExA(0, "BUTTON", "Show collision mesh",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                24, 462, 190, 22, hWnd, (HMENU)(INT_PTR)IDC_CONFIG_COLLISION,
                GetModuleHandle(NULL), NULL);
            CreateWindowExA(0, "BUTTON", "Zone Objects...",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                300, 434, 150, 28, hWnd, (HMENU)(INT_PTR)IDC_CONFIG_ZONE_OBJECTS,
                GetModuleHandle(NULL), NULL);

            HWND hAppearanceGroup = NULL;
            CreateWindowExA(0, "STATIC", "Color Theme", WS_CHILD | WS_VISIBLE,
                28, 532, 100, 18, hWnd, NULL, GetModuleHandle(NULL), NULL);
            HWND hColorTheme = CreateWindowExA(0, "COMBOBOX", "",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                142, 527, 170, 96, hWnd, (HMENU)(INT_PTR)IDC_CONFIG_COLOR_THEME,
                GetModuleHandle(NULL), NULL);
            SendMessageA(hColorTheme, CB_ADDSTRING, 0, (LPARAM)"Dark");
            SendMessageA(hColorTheme, CB_ADDSTRING, 0, (LPARAM)"Light");

            CreateWindowExA(0, "BUTTON", "Close",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                422, 574, 112, 30, hWnd, (HMENU)(INT_PTR)IDC_CONFIG_CLOSE,
                GetModuleHandle(NULL), NULL);

            HWND children[] =
            {
                hPath,
                GetDlgItem(hWnd, IDC_CONFIG_SET_PATH),
                GetDlgItem(hWnd, IDC_CONFIG_DETECT_PATH),
                GetDlgItem(hWnd, IDC_CONFIG_RESET_PATH),
                GetDlgItem(hWnd, IDC_CONFIG_SHOW_PATH),
                GetDlgItem(hWnd, IDC_CONFIG_WINDOW_MODE),
                GetDlgItem(hWnd, IDC_CONFIG_RESOLUTION),
                GetDlgItem(hWnd, IDC_CONFIG_MIP_MAPPING),
                GetDlgItem(hWnd, IDC_CONFIG_BUMP_MAPPING),
                GetDlgItem(hWnd, IDC_CONFIG_ENV_ANIM),
                GetDlgItem(hWnd, IDC_CONFIG_MIRROR_WORLD),
                GetDlgItem(hWnd, IDC_CONFIG_TEXTURE_COMPRESSION),
                GetDlgItem(hWnd, IDC_CONFIG_DRAW_DISTANCE),
                GetDlgItem(hWnd, IDC_CONFIG_ENABLE_SOUNDS),
                GetDlgItem(hWnd, IDC_CONFIG_BACKGROUND_SOUNDS),
                GetDlgItem(hWnd, IDC_CONFIG_MAX_SOUNDS),
                GetDlgItem(hWnd, IDC_CONFIG_HARDWARE_CURSOR),
                GetDlgItem(hWnd, IDC_CONFIG_EDIT_GAME),
                GetDlgItem(hWnd, IDC_CONFIG_COLLISION),
                GetDlgItem(hWnd, IDC_CONFIG_ZONE_OBJECTS),
                GetDlgItem(hWnd, IDC_CONFIG_COLOR_THEME),
                GetDlgItem(hWnd, IDC_CONFIG_CLOSE)
            };
            for (int i = 0; i < (int)(sizeof(children) / sizeof(children[0])); ++i)
            {
                if (children[i])
                    SendMessageA(children[i], WM_SETFONT, (WPARAM)hFont, TRUE);
            }

            ApplyColorTheme(hWnd);
            HWND groups[] =
            {
                hPathGroup, hScreenGroup, hRenderingGroup,
                hAudioGroup, hModeGroup, hAppearanceGroup
            };
            for (int i = 0; i < (int)(sizeof(groups) / sizeof(groups[0])); ++i)
                SendMessageA(groups[i], WM_SETFONT, (WPARAM)g_hThemeSectionFont, TRUE);
            SyncConfigDialogControls(hWnd);
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_CONFIG_SET_PATH:
            PromptSetFFXIPath();
            SyncConfigDialogControls(hWnd);
            return 0;
        case IDC_CONFIG_DETECT_PATH:
            AutoDetectFFXIPath();
            SyncConfigDialogControls(hWnd);
            return 0;
        case IDC_CONFIG_RESET_PATH:
            ResetFFXIPathToDefault();
            SyncConfigDialogControls(hWnd);
            return 0;
        case IDC_CONFIG_SHOW_PATH:
            ShowCurrentFFXIPathDialog();
            return 0;
        case IDC_CONFIG_MIP_MAPPING:
            g_enableMipMapping =
                SendDlgItemMessageA(hWnd, IDC_CONFIG_MIP_MAPPING, BM_GETCHECK, 0, 0) == BST_CHECKED;
            SyncSettingsMenuChecks();
            InvalidateRect(g_hWnd, NULL, FALSE);
            return 0;
        case IDC_CONFIG_BUMP_MAPPING:
            g_enableBumpMapping =
                SendDlgItemMessageA(hWnd, IDC_CONFIG_BUMP_MAPPING, BM_GETCHECK, 0, 0) == BST_CHECKED;
            SyncSettingsMenuChecks();
            InvalidateRect(g_hWnd, NULL, FALSE);
            return 0;
        case IDC_CONFIG_ENV_ANIM:
            SetEnvironmentalAnimationMode((int)SendDlgItemMessageA(hWnd, IDC_CONFIG_ENV_ANIM, CB_GETCURSEL, 0, 0));
            SyncSettingsMenuChecks();
            InvalidateRect(g_hWnd, NULL, FALSE);
            return 0;
        case IDC_CONFIG_WINDOW_MODE:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                g_windowMode = (int)SendDlgItemMessageA(hWnd, IDC_CONFIG_WINDOW_MODE, CB_GETCURSEL, 0, 0);
                if (g_windowMode < kDATuraWindowMode_Windowed || g_windowMode > kDATuraWindowMode_Fullscreen)
                    g_windowMode = kDATuraWindowMode_Windowed;
                ApplyDisplaySettings();
                SyncConfigDialogControls(hWnd);
            }
            return 0;
        case IDC_CONFIG_RESOLUTION:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                g_resolutionIndex = (int)SendDlgItemMessageA(hWnd, IDC_CONFIG_RESOLUTION, CB_GETCURSEL, 0, 0);
                if (g_resolutionIndex < 0 || g_resolutionIndex >= kResolutionOptionCount)
                    g_resolutionIndex = 0;
                ApplyDisplaySettings();
                SyncConfigDialogControls(hWnd);
            }
            return 0;
        case IDC_CONFIG_EDIT_GAME:
            ToggleEditGameMode();
            SyncConfigDialogControls(hWnd);
            return 0;
        case IDC_CONFIG_MIRROR_WORLD:
            g_mirrorWorldZones =
                SendDlgItemMessageA(hWnd, IDC_CONFIG_MIRROR_WORLD, BM_GETCHECK, 0, 0) == BST_CHECKED;
            SyncSettingsMenuChecks();
            ReloadRememberedZone();
            SyncConfigDialogControls(hWnd);
            InvalidateRect(g_hWnd, NULL, FALSE);
            return 0;
        case IDC_CONFIG_DRAW_DISTANCE:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                g_drawDistanceIndex =
                    (int)SendDlgItemMessageA(hWnd, IDC_CONFIG_DRAW_DISTANCE, CB_GETCURSEL, 0, 0);
                if (g_drawDistanceIndex < 0 || g_drawDistanceIndex >= kDrawDistanceOptionCount)
                    g_drawDistanceIndex = 2;
                SyncConfigDialogControls(hWnd);
                InvalidateRect(g_hWnd, NULL, FALSE);
            }
            return 0;
        case IDC_CONFIG_TEXTURE_COMPRESSION:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                const int selection = (int)SendDlgItemMessageA(
                    hWnd, IDC_CONFIG_TEXTURE_COMPRESSION, CB_GETCURSEL, 0, 0);
                g_enableTextureCompression = selection != 1;
                ApplyTextureCompressionSetting();
                SyncConfigDialogControls(hWnd);
                InvalidateRect(g_hWnd, NULL, FALSE);
            }
            return 0;
        case IDC_CONFIG_COLOR_THEME:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                const int theme = (int)SendDlgItemMessageA(
                    hWnd, IDC_CONFIG_COLOR_THEME, CB_GETCURSEL, 0, 0);
                SetColorTheme(theme);
                SyncConfigDialogControls(hWnd);
            }
            return 0;
        case IDC_CONFIG_COLLISION:
            g_showCollisionGeometry =
                SendDlgItemMessageA(hWnd, IDC_CONFIG_COLLISION, BM_GETCHECK, 0, 0) == BST_CHECKED;
            if (g_hZoneCollisionVisibleCheck)
            {
                SendMessageA(g_hZoneCollisionVisibleCheck, BM_SETCHECK,
                             g_showCollisionGeometry ? BST_CHECKED : BST_UNCHECKED, 0);
            }
            if (g_hWnd)
                InvalidateRect(g_hWnd, NULL, FALSE);
            return 0;
        case IDC_CONFIG_ENABLE_SOUNDS:
            g_enableSounds =
                SendDlgItemMessageA(hWnd, IDC_CONFIG_ENABLE_SOUNDS, BM_GETCHECK, 0, 0) == BST_CHECKED;
            SyncAppMusic();
            SyncConfigDialogControls(hWnd);
            return 0;
        case IDC_CONFIG_BACKGROUND_SOUNDS:
            g_playSoundsInBackground =
                SendDlgItemMessageA(hWnd, IDC_CONFIG_BACKGROUND_SOUNDS, BM_GETCHECK, 0, 0) == BST_CHECKED;
            SyncAppMusic();
            SyncConfigDialogControls(hWnd);
            return 0;
        case IDC_CONFIG_MAX_SOUNDS:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                int soundIndex = (int)SendDlgItemMessageA(hWnd, IDC_CONFIG_MAX_SOUNDS, CB_GETCURSEL, 0, 0);
                if (soundIndex < 0 || soundIndex >= kMaxSoundOptionCount)
                    soundIndex = kMaxSoundOptionCount - 1;
                g_maxSimultaneousSounds = kMaxSoundOptions[soundIndex];
                SyncConfigDialogControls(hWnd);
            }
            return 0;
        case IDC_CONFIG_HARDWARE_CURSOR:
            g_enableHardwareMouseCursor =
                SendDlgItemMessageA(hWnd, IDC_CONFIG_HARDWARE_CURSOR, BM_GETCHECK, 0, 0) == BST_CHECKED;
            if (g_enableHardwareMouseCursor)
                SetCameraCursorHidden(false);
            return 0;
        case IDC_CONFIG_ZONE_OBJECTS:
            ShowZoneObjectPanel();
            return 0;
        case IDC_CONFIG_CLOSE:
            DestroyWindow(hWnd);
            return 0;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;

    case WM_DRAWITEM:
        DrawConfigButton((const DRAWITEMSTRUCT *)lParam);
        return TRUE;

    case WM_PAINT:
        {
            PAINTSTRUCT ps = {};
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT client = {};
            GetClientRect(hWnd, &client);
            FillRect(hdc, &client, g_hThemeWindowBrush);
            const RECT panels[] =
            {
                { 14,   8, 534, 100 },
                { 14, 106, 534, 194 },
                { 14, 200, 534, 308 },
                { 14, 316, 534, 404 },
                { 14, 412, 534, 500 },
                { 14, 506, 534, 564 }
            };
            const char *titles[] =
            {
                "FFXI Installation", "Display", "Rendering",
                "Audio / Input", "Mode / Debug", "Appearance"
            };
            for (int i = 0; i < (int)(sizeof(panels) / sizeof(panels[0])); ++i)
                DrawConfigPanel(hdc, panels[i], titles[i]);
            EndPaint(hWnd, &ps);
        }
        return 0;

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, ThemeTextColor());
            SetBkColor(hdc, ThemeControlColor());
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)g_hThemeControlBrush;
        }

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, ThemeTextColor());
            SetBkColor(hdc, ThemeEditColor());
            return (LRESULT)g_hThemeEditBrush;
        }

    case WM_ERASEBKGND:
        {
            RECT rc = {};
            GetClientRect(hWnd, &rc);
            FillRect((HDC)wParam, &rc, g_hThemeWindowBrush);
        }
        return 1;

    case WM_DESTROY:
        if (g_hConfigDialog == hWnd)
            g_hConfigDialog = NULL;
        return 0;
    }

    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

static void ShowTitleConfigDialog()
{
    if (g_hConfigDialog && IsWindow(g_hConfigDialog))
    {
        SyncConfigDialogControls(g_hConfigDialog);
        ShowWindow(g_hConfigDialog, SW_SHOW);
        SetForegroundWindow(g_hConfigDialog);
        return;
    }

    HINSTANCE hInst = (HINSTANCE)GetModuleHandle(NULL);
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ConfigDialogProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = g_hThemeWindowBrush;
    wc.lpszClassName = kConfigDialogClassName;
    RegisterClassExA(&wc);

    RECT owner = {};
    GetWindowRect(g_hWnd, &owner);
    const int dlgW = 568;
    const int dlgH = 650;
    const int x = owner.left + ((owner.right - owner.left) - dlgW) / 2;
    const int y = owner.top + ((owner.bottom - owner.top) - dlgH) / 2;

    HWND hDlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME,
        kConfigDialogClassName,
        "DATura Config",
        WS_CAPTION | WS_SYSMENU | WS_POPUP | WS_VISIBLE,
        x, y, dlgW, dlgH,
        g_hWnd, NULL, hInst, NULL);

    if (hDlg)
    {
        g_hConfigDialog = hDlg;
        ShowWindow(hDlg, SW_SHOW);
        SetForegroundWindow(hDlg);
    }
}

static LRESULT CALLBACK PathDialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_PATH_DIALOG_OK)
        {
            DestroyWindow(hWnd);
            return 0;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;
    }

    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

static void ShowFFXIPathInfoDialog(const char *title, const char *labelText, const char *pathText)
{
    if (!title)
        title = "FFXI Path";
    if (!labelText)
        labelText = "FFXI path:";
    if (!pathText)
        pathText = "";

    HINSTANCE hInst = (HINSTANCE)GetModuleHandle(NULL);
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = PathDialogProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
    wc.lpszClassName = kPathDialogClassName;
    RegisterClassExA(&wc);

    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HDC hdc = GetDC(g_hWnd);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    SIZE labelSize = {};
    SIZE pathSize = {};
    GetTextExtentPoint32A(hdc, labelText, (int)strlen(labelText), &labelSize);
    GetTextExtentPoint32A(hdc, pathText, (int)strlen(pathText), &pathSize);
    SelectObject(hdc, hOldFont);
    ReleaseDC(g_hWnd, hdc);

    RECT workArea = {};
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &workArea, 0);

    const int margin = 20;
    const int iconSize = 32;
    const int gap = 12;
    const int buttonW = 76;
    const int buttonH = 24;
    const int textW = (labelSize.cx > pathSize.cx) ? labelSize.cx : pathSize.cx;
    const int contentW = iconSize + gap + textW;
    int clientW = contentW + margin * 2;
    if (clientW < 420)
        clientW = 420;
    int maxClientW = (workArea.right - workArea.left) - 80;
    if (maxClientW < 420)
        maxClientW = 420;
    if (clientW > maxClientW)
        clientW = maxClientW;
    const int clientH = 124;

    RECT wr = { 0, 0, clientW, clientH };
    AdjustWindowRectEx(&wr, WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);
    const int windowW = wr.right - wr.left;
    const int windowH = wr.bottom - wr.top;
    const int x = workArea.left + ((workArea.right - workArea.left) - windowW) / 2;
    const int y = workArea.top + ((workArea.bottom - workArea.top) - windowH) / 2;

    EnableWindow(g_hWnd, FALSE);
    HWND hDlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME,
        kPathDialogClassName,
        title,
        WS_CAPTION | WS_SYSMENU,
        x, y, windowW, windowH,
        g_hWnd, NULL, hInst, NULL);

    if (!hDlg)
    {
        EnableWindow(g_hWnd, TRUE);
        char msg[MAX_PATH + 128];
        sprintf_s(msg, "%s\n%s", labelText, pathText);
        MessageBoxA(g_hWnd, msg, title, MB_OK | MB_ICONINFORMATION);
        return;
    }

    HWND hIcon = CreateWindowExA(0, "STATIC", NULL, WS_CHILD | WS_VISIBLE | SS_ICON,
        margin, 31, iconSize, iconSize, hDlg, NULL, hInst, NULL);
    SendMessageA(hIcon, STM_SETICON, (WPARAM)LoadIcon(NULL, IDI_INFORMATION), 0);

    HWND hLabel = CreateWindowExA(0, "STATIC", labelText, WS_CHILD | WS_VISIBLE,
        margin + iconSize + gap, 26, labelSize.cx + 8, 18, hDlg, NULL, hInst, NULL);
    SendMessageA(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

    int pathControlW = clientW - (margin * 2 + iconSize + gap);
    if (pathControlW < 1)
        pathControlW = 1;
    HWND hPath = CreateWindowExA(0, "STATIC", pathText, WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
        margin + iconSize + gap, 44, pathControlW, 18, hDlg, NULL, hInst, NULL);
    SendMessageA(hPath, WM_SETFONT, (WPARAM)hFont, TRUE);

    HWND hButton = CreateWindowExA(0, "BUTTON", "OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        clientW - margin - buttonW, clientH - margin - buttonH, buttonW, buttonH,
        hDlg, (HMENU)IDC_PATH_DIALOG_OK, hInst, NULL);
    SendMessageA(hButton, WM_SETFONT, (WPARAM)hFont, TRUE);

    ShowWindow(hDlg, SW_SHOW);
    SetFocus(hButton);

    MSG msg = {};
    while (IsWindow(hDlg) && GetMessageA(&msg, NULL, 0, 0) > 0)
    {
        if (!IsDialogMessageA(hDlg, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    EnableWindow(g_hWnd, TRUE);
    SetForegroundWindow(g_hWnd);
}

static void ShowCurrentFFXIPathDialog()
{
    ShowFFXIPathInfoDialog("FFXI Path", "Current FFXI path:", g_ffxiPath);
}

//========================================================================================
// D3D9 helpers
//========================================================================================

// Fill g_d3dpp with windowed, back-buffer-unknown, depth-stencil D24S8 settings.
static void BuildPresentParams(int width, int height)
{
    if (g_windowMode != kDATuraWindowMode_Windowed &&
        g_resolutionIndex >= 0 && g_resolutionIndex < kResolutionOptionCount)
    {
        width = kResolutionOptions[g_resolutionIndex].width;
        height = kResolutionOptions[g_resolutionIndex].height;
    }

    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed               = (g_windowMode != kDATuraWindowMode_Fullscreen);
    g_d3dpp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat       = g_d3dpp.Windowed ? D3DFMT_UNKNOWN : D3DFMT_X8R8G8B8;
    g_d3dpp.BackBufferWidth        = (width  > 0) ? width  : kDefaultWidth;
    g_d3dpp.BackBufferHeight       = (height > 0) ? height : kDefaultHeight;
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
    g_d3dpp.PresentationInterval   = D3DPRESENT_INTERVAL_ONE; // vsync
}

// Try to create a D3D9 device.  Follows FFXITool's fallback chain:
//   1. HAL device, hardware vertex processing
//   2. HAL device, software vertex processing
//   3. REF device, software vertex processing  (last resort / debugging)
static bool CreateD3DDevice()
{
    struct { D3DDEVTYPE devType; DWORD vpFlags; const char *desc; } attempts[] =
    {
        { D3DDEVTYPE_HAL, D3DCREATE_HARDWARE_VERTEXPROCESSING, "HAL + HW VP" },
        { D3DDEVTYPE_HAL, D3DCREATE_SOFTWARE_VERTEXPROCESSING, "HAL + SW VP" },
        { D3DDEVTYPE_REF, D3DCREATE_SOFTWARE_VERTEXPROCESSING, "REF + SW VP" },
    };

    for (int i = 0; i < 3; ++i)
    {
        HRESULT hr = g_pD3D->CreateDevice(
            D3DADAPTER_DEFAULT,
            attempts[i].devType,
            g_hWnd,
            attempts[i].vpFlags,
            &g_d3dpp,
            &g_pDevice);

        if (SUCCEEDED(hr))
        {
            char msg[128];
            sprintf_s(msg, "D3D9 device created (%s)\n", attempts[i].desc);
            OutputDebugStringA(msg);
            return true;
        }
    }

    return false;
}

static bool InitD3D(int width, int height)
{
    g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!g_pD3D)
    {
        MessageBoxA(g_hWnd, "Direct3DCreate9 failed.", "D3D Error", MB_OK | MB_ICONERROR);
        return false;
    }

    BuildPresentParams(width, height);

    if (!CreateD3DDevice())
    {
        MessageBoxA(g_hWnd, "Failed to create a Direct3D 9 device.\n"
                            "Make sure your drivers support D3D9.",
                    "D3D Error", MB_OK | MB_ICONERROR);
        return false;
    }

    return true;
}

// Release all D3DPOOL_DEFAULT resources here before calling Reset().
// Currently empty — add resource release calls as rendering is fleshed out.
static void ReleaseDefaultPoolResources()
{
    if (g_pFfxiTexturePixelShader)
    {
        g_pFfxiTexturePixelShader->Release();
        g_pFfxiTexturePixelShader = NULL;
    }
    g_ffxiTexturePixelShaderTried = false;
    if (g_pFfxiUiPixelShader)
    {
        g_pFfxiUiPixelShader->Release();
        g_pFfxiUiPixelShader = NULL;
    }
    g_ffxiUiPixelShaderTried = false;

    // TODO: release vertex buffers, index buffers, textures, render targets
    //       that were created with D3DPOOL_DEFAULT.
}

// Re-create D3DPOOL_DEFAULT resources after a successful Reset().
static void RecreateDefaultPoolResources()
{
    // TODO: re-create vertex buffers, render targets, etc.
}

static bool ResetDevice()
{
    ReleaseDefaultPoolResources();
    HRESULT hr = g_pDevice->Reset(&g_d3dpp);
    if (FAILED(hr))
        return false;
    RecreateDefaultPoolResources();
    return true;
}

static void ApplyDisplaySettings()
{
    if (!g_hWnd)
        return;

    if (g_resolutionIndex < 0 || g_resolutionIndex >= kResolutionOptionCount)
        g_resolutionIndex = 0;
    if (g_windowMode < kDATuraWindowMode_Windowed || g_windowMode > kDATuraWindowMode_Fullscreen)
        g_windowMode = kDATuraWindowMode_Windowed;

    const DATuraResolutionOption &res = kResolutionOptions[g_resolutionIndex];
    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD exStyle = 0;
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    int w = res.width;
    int h = res.height;

    RECT workArea = {};
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &workArea, 0);
    RECT monitorArea = workArea;
    HMONITOR hMonitor = MonitorFromWindow(g_hWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    if (hMonitor && GetMonitorInfoA(hMonitor, &mi))
        monitorArea = mi.rcMonitor;

    if (g_windowMode == kDATuraWindowMode_Borderless || g_windowMode == kDATuraWindowMode_Fullscreen)
    {
        style = WS_POPUP;
        const RECT &target = (g_windowMode == kDATuraWindowMode_Fullscreen) ? monitorArea : workArea;
        x = target.left;
        y = target.top;
        w = target.right - target.left;
        h = target.bottom - target.top;
    }
    else
    {
        RECT rc = { 0, 0, res.width, res.height };
        AdjustWindowRect(&rc, style, TRUE);
        w = rc.right - rc.left;
        h = rc.bottom - rc.top;
        x = workArea.left + ((workArea.right - workArea.left) - w) / 2;
        y = workArea.top + ((workArea.bottom - workArea.top) - h) / 2;
    }

    SetWindowLongPtrA(g_hWnd, GWL_STYLE, (LONG_PTR)style);
    SetWindowLongPtrA(g_hWnd, GWL_EXSTYLE, (LONG_PTR)exStyle);
    SetWindowPos(g_hWnd, NULL, x, y, w, h,
                 SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    if (g_pDevice)
    {
        RECT client = {};
        GetClientRect(g_hWnd, &client);
        BuildPresentParams(client.right - client.left, client.bottom - client.top);
        ResetDevice();
    }

    if (g_hWnd)
        InvalidateRect(g_hWnd, NULL, TRUE);
}

static void ShutdownD3D()
{
    BGM_Stop();
    ReleaseDefaultPoolResources();
    if (g_pDevice) { g_pDevice->Release(); g_pDevice = NULL; }
    if (g_pD3D)    { g_pD3D->Release();    g_pD3D    = NULL; }
}

static const char *PathFileName(const char *path)
{
    if (!path)
        return "";
    const char *name = path;
    for (const char *p = path; *p; ++p)
    {
        if (*p == '\\' || *p == '/')
            name = p + 1;
    }
    return name;
}

static bool StringEqualsNoCase(const char *a, const char *b)
{
    if (!a || !b)
        return a == b;

    while (*a && *b)
    {
        const int ca = tolower((unsigned char)*a);
        const int cb = tolower((unsigned char)*b);
        if (ca != cb)
            return false;
        ++a;
        ++b;
    }

    return *a == *b;
}

static bool StringStartsWithNoCase(const char *text, const char *prefix)
{
    if (!text || !prefix)
        return false;

    while (*prefix)
    {
        if (!*text)
            return false;
        const int ct = tolower((unsigned char)*text);
        const int cp = tolower((unsigned char)*prefix);
        if (ct != cp)
            return false;
        ++text;
        ++prefix;
    }

    return true;
}

static void MakeRelativeFFXIPath(const char *path, char *outPath, size_t outPathSize)
{
    if (!outPath || outPathSize == 0)
        return;

    outPath[0] = 0;
    if (!path || !path[0])
        return;

    const size_t rootLen = strlen(g_ffxiPath);
    if (rootLen > 0 && StringStartsWithNoCase(path, g_ffxiPath))
    {
        const char *relativeStart = path + rootLen;
        while (*relativeStart == '\\' || *relativeStart == '/')
            ++relativeStart;
        strcpy_s(outPath, outPathSize, relativeStart);
    }
    else
    {
        strcpy_s(outPath, outPathSize, path);
    }

    for (char *p = outPath; *p; ++p)
    {
        if (*p == '\\')
            *p = '/';
    }
}

static const char *FindZoneNameByModelPath(const char *path)
{
    char relativePath[MAX_PATH] = {};
    MakeRelativeFFXIPath(path, relativePath, sizeof(relativePath));

    for (int i = 0; i < kFFXIZoneCount; ++i)
    {
        const FFXIZoneEntry &zone = kFFXIZoneTable[i];
        if (zone.modelDat[0] && StringEqualsNoCase(zone.modelDat, relativePath))
            return zone.name;
    }

	return "";
}

static int FindZoneIDByModelPath(const char *path)
{
    char relativePath[MAX_PATH] = {};
    MakeRelativeFFXIPath(path, relativePath, sizeof(relativePath));

    for (int i = 0; i < kFFXIZoneCount; ++i)
    {
        const FFXIZoneEntry &zone = kFFXIZoneTable[i];
        if (zone.modelDat[0] && StringEqualsNoCase(zone.modelDat, relativePath))
            return zone.id;
    }

    return -1;
}

static bool FileExistsAPath(const char *path)
{
    if (!path || !path[0])
        return false;
    const DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static void BuildFFXIFullPath(const char *relativePath, char *outPath, const size_t outPathSize)
{
    if (!outPath || outPathSize == 0)
        return;
    outPath[0] = 0;
    if (!relativePath || !relativePath[0])
        return;

    strcpy_s(outPath, outPathSize, g_ffxiPath);
    if (outPath[0])
    {
        const size_t len = strlen(outPath);
        if (len > 0 && outPath[len - 1] != '\\' && outPath[len - 1] != '/')
            strcat_s(outPath, outPathSize, "\\");
    }
    strcat_s(outPath, outPathSize, relativePath);
}

static bool ResolveFileIdFromTables(const int datId, unsigned int *outFileId)
{
    if (outFileId)
        *outFileId = 0;
    FFXIResource::ResolvedFile resolved;
    if (!FFXIResource::ResolveFileId(g_ffxiPath, datId, resolved))
        return false;
    if (outFileId)
        *outFileId = MAKELONG(resolved.packedLocation, resolved.hive);
    return true;
}

static bool BuildRelativePathFromFileId(const unsigned int fileId, char *outPath, const size_t outPathSize)
{
    if (!outPath || outPathSize == 0)
        return false;
    outPath[0] = 0;

    const unsigned int fileNo = LOWORD(fileId);
    const unsigned int hive = HIWORD(fileId);
    if (hive < 1 || hive > 19)
        return false;

    if (hive == 1)
        sprintf_s(outPath, outPathSize, "ROM/%u/%u.DAT", fileNo / 0x80, fileNo % 0x80);
    else
        sprintf_s(outPath, outPathSize, "ROM%u/%u/%u.DAT", hive, fileNo / 0x80, fileNo % 0x80);
    return true;
}

static bool ResolveZoneModelPath(const int zoneId, char *outFullPath, const size_t outFullPathSize,
                                 char *outRelativePath = nullptr, const size_t outRelativePathSize = 0,
                                 bool *outUsedFtable = nullptr)
{
    if (outFullPath && outFullPathSize > 0)
        outFullPath[0] = 0;
    if (outRelativePath && outRelativePathSize > 0)
        outRelativePath[0] = 0;
    if (outUsedFtable)
        *outUsedFtable = false;

    const FFXIZoneEntry *zone = FFXIZone::FindByID(zoneId);
    if (zone && zone->modelDat && zone->modelDat[0])
    {
        char staticFullPath[MAX_PATH] = {};
        BuildFFXIFullPath(zone->modelDat, staticFullPath, sizeof(staticFullPath));
        if (FileExistsAPath(staticFullPath))
        {
            if (outFullPath && outFullPathSize > 0)
                strcpy_s(outFullPath, outFullPathSize, staticFullPath);
            if (outRelativePath && outRelativePathSize > 0)
                strcpy_s(outRelativePath, outRelativePathSize, zone->modelDat);
            return true;
        }
    }

    // RZN's MapLib documents this lookup path as reliable for pre-Adoulin maps.
    // Later zones have static DAT paths in DATura, but the same zoneId+100 table
    // lookup can resolve to unrelated legacy DATs, so keep it as a guarded fallback.
    if (zoneId >= 256)
        return false;

    unsigned int fileId = 0;
    char resolvedRelativePath[MAX_PATH] = {};
    if (!ResolveFileIdFromTables(zoneId + 100, &fileId) ||
        !BuildRelativePathFromFileId(fileId, resolvedRelativePath, sizeof(resolvedRelativePath)))
    {
        return false;
    }

    char resolvedFullPath[MAX_PATH] = {};
    BuildFFXIFullPath(resolvedRelativePath, resolvedFullPath, sizeof(resolvedFullPath));
    if (!FileExistsAPath(resolvedFullPath))
        return false;

    if (outFullPath && outFullPathSize > 0)
        strcpy_s(outFullPath, outFullPathSize, resolvedFullPath);
    if (outRelativePath && outRelativePathSize > 0)
        strcpy_s(outRelativePath, outRelativePathSize, resolvedRelativePath);
    if (outUsedFtable)
        *outUsedFtable = true;
    return true;
}

static void AppendCsvField(std::vector<std::string> &fields, std::string &field)
{
    fields.push_back(field);
    field.clear();
}

static std::vector<std::string> SplitCsvLine(const char *line)
{
    std::vector<std::string> fields;
    std::string field;
    bool inQuotes = false;
    for (const char *p = line; p && *p; ++p)
    {
        const char ch = *p;
        if (ch == '\r' || ch == '\n')
            break;
        if (ch == '"')
        {
            if (inQuotes && p[1] == '"')
            {
                field.push_back('"');
                ++p;
            }
            else
            {
                inQuotes = !inQuotes;
            }
        }
        else if (ch == ',' && !inQuotes)
        {
            AppendCsvField(fields, field);
        }
        else
        {
            field.push_back(ch);
        }
    }
    AppendCsvField(fields, field);
    return fields;
}

static int CsvColumnIndex(const std::vector<std::string> &header, const char *name)
{
    for (int i = 0; i < (int)header.size(); ++i)
    {
        if (StringEqualsNoCase(header[i].c_str(), name))
            return i;
    }
    return -1;
}

static bool ResolveNpcPlacementCsvPath(char *outPath, size_t outPathSize)
{
    static const char *kRelativeCsv =
        "tools\\zone_aux_exports\\all_zone_npc_placement_candidates_conf90.csv";

    char cwdPath[MAX_PATH] = {};
    sprintf_s(cwdPath, "%s", kRelativeCsv);
    if (FileExistsAPath(cwdPath))
    {
        strcpy_s(outPath, outPathSize, cwdPath);
        return true;
    }

    char modulePath[MAX_PATH] = {};
    GetModuleFileNameA(NULL, modulePath, sizeof(modulePath));
    char *slash = strrchr(modulePath, '\\');
    if (slash)
        *slash = 0;

    char candidate[MAX_PATH] = {};
    const char *prefixes[] =
    {
        "%s\\%s",
        "%s\\..\\%s",
        "%s\\..\\..\\%s",
        "%s\\..\\..\\..\\%s",
    };
    for (int i = 0; i < (int)(sizeof(prefixes) / sizeof(prefixes[0])); ++i)
    {
        sprintf_s(candidate, prefixes[i], modulePath, kRelativeCsv);
        if (FileExistsAPath(candidate))
        {
            strcpy_s(outPath, outPathSize, candidate);
            return true;
        }
    }

    return false;
}

static void LoadNpcPlacementMarkersForZonePath(const char *path)
{
    g_npcPlacementMarkers.clear();

    const int zoneId = FindZoneIDByModelPath(path);
    if (zoneId != 230)
        return;

    char csvPath[MAX_PATH] = {};
    if (!ResolveNpcPlacementCsvPath(csvPath, sizeof(csvPath)))
        return;

    FILE *fp = nullptr;
    if (fopen_s(&fp, csvPath, "rb") != 0 || !fp)
        return;

    char line[4096] = {};
    if (!fgets(line, sizeof(line), fp))
    {
        fclose(fp);
        return;
    }

    std::vector<std::string> header = SplitCsvLine(line);
    const int zoneCol = CsvColumnIndex(header, "zone_id");
    const int nameCol = CsvColumnIndex(header, "name");
    const int xCol = CsvColumnIndex(header, "x");
    const int yCol = CsvColumnIndex(header, "y");
    const int zCol = CsvColumnIndex(header, "z");
    const int confCol = CsvColumnIndex(header, "confidence");
    if (zoneCol < 0 || nameCol < 0 || xCol < 0 || yCol < 0 || zCol < 0 || confCol < 0)
    {
        fclose(fp);
        return;
    }

    while (fgets(line, sizeof(line), fp))
    {
        std::vector<std::string> fields = SplitCsvLine(line);
        const int maxCol = std::max(confCol, std::max(zCol, std::max(yCol, std::max(xCol, std::max(nameCol, zoneCol)))));
        if ((int)fields.size() <= maxCol)
            continue;

        if (atoi(fields[zoneCol].c_str()) != zoneId)
            continue;

        const int confidence = atoi(fields[confCol].c_str());
        if (confidence < 90)
            continue;

        NpcPlacementMarker marker = {};
        strcpy_s(marker.name, fields[nameCol].c_str());
        const float csvX = (float)atof(fields[xCol].c_str());
        const float csvY = (float)atof(fields[yCol].c_str());
        const float csvZ = (float)atof(fields[zCol].c_str());
        marker.pos[0] = csvX;
        marker.pos[1] = csvZ;
        marker.pos[2] = csvY;
        if (g_haveZoneCollisionBounds)
        {
            float floorY = 0.0f;
            if (CollisionFloorAt(marker.pos[0], marker.pos[2],
                                 g_zoneCollisionMin[1] - 1.0f,
                                 g_zoneCollisionMax[1] + 1.0f,
                                 &floorY, nullptr))
            {
                marker.pos[1] = floorY - 0.08f;
            }
            else
            {
                continue;
            }
        }
        marker.confidence = confidence;
        g_npcPlacementMarkers.push_back(marker);
    }

    fclose(fp);
}

static void LoadDatFile(const char *path, bool userContentLoad = true, bool renderEnvironment = false,
                        bool renderUnreferenced = false);

static void SetLoadedZoneLabelFromNameAndPath(const char *name, const char *path)
{
    if (!path || !path[0])
    {
        strcpy_s(g_loadedZoneLabel, "No zone loaded");
    }
    else
    {
        char relativePath[MAX_PATH] = {};
        MakeRelativeFFXIPath(path, relativePath, sizeof(relativePath));

        const char *zoneName = (name && name[0]) ? name : FindZoneNameByModelPath(path);
        if (!zoneName || !zoneName[0])
            zoneName = PathFileName(path);

        sprintf_s(g_loadedZoneLabel, "Loaded zone: %s (%s)", zoneName, relativePath);
    }

    if (g_hZoneLabel)
        SetWindowTextA(g_hZoneLabel, g_loadedZoneLabel);
}

static void SetLoadedZoneLabelFromPath(const char *path)
{
    SetLoadedZoneLabelFromNameAndPath(nullptr, path);
}

static void RememberLoadedZoneContext(const char *path, const char *name,
                                      bool userContentLoad, bool renderEnvironment,
                                      bool renderUnreferenced)
{
    strcpy_s(g_loadedZonePath, path ? path : "");
    strcpy_s(g_loadedZoneName, name ? name : "");
    g_loadedZoneUserContent = userContentLoad;
    g_loadedZoneRenderEnvironment = renderEnvironment;
    g_loadedZoneRenderUnreferenced = renderUnreferenced;
}

static void ReloadRememberedZone()
{
    if (!g_loadedZonePath[0])
        return;

    char path[MAX_PATH] = {};
    char name[128] = {};
    strcpy_s(path, g_loadedZonePath);
    strcpy_s(name, g_loadedZoneName);
    const bool userContentLoad = g_loadedZoneUserContent;
    const bool renderEnvironment = g_loadedZoneRenderEnvironment;
    const bool renderUnreferenced = g_loadedZoneRenderUnreferenced;

    LoadDatFile(path, userContentLoad, renderEnvironment, renderUnreferenced);
    if (name[0])
        SetLoadedZoneLabelFromNameAndPath(name, path);
    RememberLoadedZoneContext(path, name, userContentLoad, renderEnvironment, renderUnreferenced);
}

//========================================================================================
// POLUtils-derived resource browser
//========================================================================================

static const wchar_t kResourceBrowserClassName[] = L"DATuraResourceBrowserClass";

static std::wstring WideFromAnsi(const char *text)
{
    if (!text || !text[0])
        return std::wstring();
    const int count = MultiByteToWideChar(CP_ACP, 0, text, -1, nullptr, 0);
    if (count <= 1)
        return std::wstring();
    std::wstring result((size_t)count, L'\0');
    MultiByteToWideChar(CP_ACP, 0, text, -1, result.data(), count);
    result.resize((size_t)count - 1);
    return result;
}

static void LayoutResourceBrowser(const HWND hWnd)
{
    RECT client = {};
    GetClientRect(hWnd, &client);
    const int margin = 10;
    const int labelHeight = 24;
    const int toggleHeight = 26;
    const int tabsHeight = g_resourceSeparateByType ? 30 : 0;
    const int statusHeight = 38;
    const int width = std::max(100, (int)client.right - margin * 2);
    const int listTop = margin + labelHeight + toggleHeight + tabsHeight;
    const int statusTop = std::max(listTop + 80, (int)client.bottom - margin - statusHeight);
    const int listHeight = std::max(80, statusTop - margin - listTop);
    if (g_hResourcePathLabel)
        MoveWindow(g_hResourcePathLabel, margin, margin, width, labelHeight, TRUE);
    if (g_hResourceSeparateTypes)
        MoveWindow(g_hResourceSeparateTypes, margin, margin + labelHeight,
                   190, toggleHeight, TRUE);
    if (g_hResourceTypeTabs)
    {
        MoveWindow(g_hResourceTypeTabs, margin, margin + labelHeight + toggleHeight,
                   width, 30, TRUE);
        ShowWindow(g_hResourceTypeTabs,
                   g_resourceSeparateByType ? SW_SHOW : SW_HIDE);
    }
    if (g_hResourceList)
        MoveWindow(g_hResourceList, margin, listTop, width, listHeight, TRUE);
    if (g_hResourceStatus)
        MoveWindow(g_hResourceStatus, margin, statusTop,
                   width, statusHeight, TRUE);
}

static std::wstring SelectedResourceKind()
{
    if (!g_resourceSeparateByType || !g_hResourceTypeTabs)
        return std::wstring();
    const int selected = TabCtrl_GetCurSel(g_hResourceTypeTabs);
    return selected >= 0 && selected < (int)g_resourceKinds.size()
        ? g_resourceKinds[(size_t)selected]
        : std::wstring();
}

static void PopulateResourceListForCurrentView()
{
    if (!g_hResourceList)
        return;

    const std::wstring selectedKind = SelectedResourceKind();
    SendMessageW(g_hResourceList, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(g_hResourceList);
    int visibleCount = 0;
    for (const FFXIResource::Row &row : g_resourceRows)
    {
        if (!selectedKind.empty() && row.kind != selectedKind)
            continue;

        LVITEMW item = {};
        item.mask = LVIF_TEXT;
        item.iItem = visibleCount;
        item.pszText = const_cast<LPWSTR>(row.kind.c_str());
        const int inserted = ListView_InsertItem(g_hResourceList, &item);
        if (inserted < 0)
            continue;
        ListView_SetItemText(g_hResourceList, inserted, 1, const_cast<LPWSTR>(row.id.c_str()));
        ListView_SetItemText(g_hResourceList, inserted, 2, const_cast<LPWSTR>(row.text.c_str()));
        ListView_SetItemText(g_hResourceList, inserted, 3, const_cast<LPWSTR>(row.details.c_str()));
        ++visibleCount;
    }
    SendMessageW(g_hResourceList, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_hResourceList, nullptr, TRUE);

    if (g_hResourceStatus)
    {
        std::wstring status = g_resourceBaseStatus;
        if (!selectedKind.empty())
        {
            status = selectedKind + L": " + std::to_wstring(visibleCount) +
                     L" record(s) in this tab. " + g_resourceBaseStatus;
        }
        SetWindowTextW(g_hResourceStatus, status.c_str());
    }
}

static void RebuildResourceTypeTabs()
{
    if (!g_hResourceTypeTabs)
        return;

    const std::wstring previousKind = SelectedResourceKind();
    g_resourceKinds.clear();
    for (const FFXIResource::Row &row : g_resourceRows)
    {
        if (std::find(g_resourceKinds.begin(), g_resourceKinds.end(), row.kind) ==
            g_resourceKinds.end())
        {
            g_resourceKinds.push_back(row.kind);
        }
    }

    TabCtrl_DeleteAllItems(g_hResourceTypeTabs);
    int selectedIndex = 0;
    for (int i = 0; i < (int)g_resourceKinds.size(); ++i)
    {
        int count = 0;
        for (const FFXIResource::Row &row : g_resourceRows)
            count += row.kind == g_resourceKinds[(size_t)i] ? 1 : 0;

        std::wstring label = g_resourceKinds[(size_t)i] +
                             L" (" + std::to_wstring(count) + L")";
        TCITEMW item = {};
        item.mask = TCIF_TEXT;
        item.pszText = const_cast<LPWSTR>(label.c_str());
        TabCtrl_InsertItem(g_hResourceTypeTabs, i, &item);
        if (!previousKind.empty() && g_resourceKinds[(size_t)i] == previousKind)
            selectedIndex = i;
    }
    if (!g_resourceKinds.empty())
        TabCtrl_SetCurSel(g_hResourceTypeTabs, selectedIndex);
}

static LRESULT CALLBACK ResourceBrowserWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        g_hResourcePathLabel = CreateWindowExW(0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
            0, 0, 0, 0, hWnd, (HMENU)(INT_PTR)IDC_RESOURCE_PATH_LABEL,
            GetModuleHandleW(nullptr), nullptr);
        g_hResourceSeparateTypes = CreateWindowExW(0, L"BUTTON", L"Separate by type",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            0, 0, 0, 0, hWnd, (HMENU)(INT_PTR)IDC_RESOURCE_SEPARATE_TYPES,
            GetModuleHandleW(nullptr), nullptr);
        g_hResourceTypeTabs = CreateWindowExW(0, WC_TABCONTROLW, L"",
            WS_CHILD | WS_CLIPSIBLINGS | TCS_TABS | TCS_SINGLELINE,
            0, 0, 0, 0, hWnd, (HMENU)(INT_PTR)IDC_RESOURCE_TYPE_TABS,
            GetModuleHandleW(nullptr), nullptr);
        g_hResourceList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
            0, 0, 0, 0, hWnd, (HMENU)(INT_PTR)IDC_RESOURCE_LIST,
            GetModuleHandleW(nullptr), nullptr);
        g_hResourceStatus = CreateWindowExW(0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 0, 0, hWnd, (HMENU)(INT_PTR)IDC_RESOURCE_STATUS,
            GetModuleHandleW(nullptr), nullptr);

        const HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        SendMessageW(g_hResourcePathLabel, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageW(g_hResourceSeparateTypes, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageW(g_hResourceTypeTabs, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageW(g_hResourceList, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageW(g_hResourceStatus, WM_SETFONT, (WPARAM)font, TRUE);
        ListView_SetExtendedListViewStyle(g_hResourceList,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);

        const wchar_t *headings[] = { L"Kind", L"ID", L"Text / Name", L"Details" };
        const int widths[] = { 120, 145, 430, 520 };
        for (int i = 0; i < 4; ++i)
        {
            LVCOLUMNW column = {};
            column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
            column.iSubItem = i;
            column.cx = widths[i];
            column.pszText = const_cast<LPWSTR>(headings[i]);
            ListView_InsertColumn(g_hResourceList, i, &column);
        }
        LayoutResourceBrowser(hWnd);
        return 0;
    }
    case WM_SIZE:
        LayoutResourceBrowser(hWnd);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_RESOURCE_SEPARATE_TYPES &&
            HIWORD(wParam) == BN_CLICKED)
        {
            g_resourceSeparateByType =
                SendMessageW(g_hResourceSeparateTypes, BM_GETCHECK, 0, 0) == BST_CHECKED;
            if (g_resourceSeparateByType)
                RebuildResourceTypeTabs();
            LayoutResourceBrowser(hWnd);
            PopulateResourceListForCurrentView();
            return 0;
        }
        break;
    case WM_NOTIFY:
        {
            const NMHDR *header = (const NMHDR *)lParam;
            if (header && header->idFrom == IDC_RESOURCE_TYPE_TABS &&
                header->code == TCN_SELCHANGE)
            {
                PopulateResourceListForCurrentView();
                return 0;
            }
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;
    case WM_DESTROY:
        if (hWnd == g_hResourceBrowser)
        {
            g_hResourceBrowser = NULL;
            g_hResourcePathLabel = NULL;
            g_hResourceSeparateTypes = NULL;
            g_hResourceTypeTabs = NULL;
            g_hResourceList = NULL;
            g_hResourceStatus = NULL;
        }
        return 0;
    }
    return DefWindowProcW(hWnd, message, wParam, lParam);
}

static bool EnsureResourceBrowser()
{
    if (g_hResourceBrowser && IsWindow(g_hResourceBrowser))
        return true;

    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    if (!GetClassInfoExW(instance, kResourceBrowserClassName, &windowClass))
    {
        windowClass.cbSize = sizeof(windowClass);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = ResourceBrowserWndProc;
        windowClass.hInstance = instance;
        windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
        windowClass.hIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_DATURA));
        windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        windowClass.lpszClassName = kResourceBrowserClassName;
        if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return false;
    }

    g_hResourceBrowser = CreateWindowExW(WS_EX_APPWINDOW, kResourceBrowserClassName,
        L"DATura Resource Browser", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1240, 680, g_hWnd, nullptr, instance, nullptr);
    if (!g_hResourceBrowser)
        return false;
    ShowWindow(g_hResourceBrowser, SW_SHOW);
    UpdateWindow(g_hResourceBrowser);
    return true;
}

static void PopulateResourceBrowser(const std::wstring &title, const std::wstring &source,
                                    const std::vector<FFXIResource::Row> &rows,
                                    const std::wstring &status)
{
    if (!EnsureResourceBrowser())
        return;

    SetWindowTextW(g_hResourceBrowser, title.c_str());
    SetWindowTextW(g_hResourcePathLabel, source.c_str());
    g_resourceRows = rows;
    g_resourceBaseStatus = status;
    SendMessageW(g_hResourceSeparateTypes, BM_SETCHECK,
                 g_resourceSeparateByType ? BST_CHECKED : BST_UNCHECKED, 0);
    if (g_resourceSeparateByType)
        RebuildResourceTypeTabs();
    LayoutResourceBrowser(g_hResourceBrowser);
    PopulateResourceListForCurrentView();
    ShowWindow(g_hResourceBrowser, SW_RESTORE);
    SetForegroundWindow(g_hResourceBrowser);
}

static void AddUniqueResourcePath(std::vector<std::string> &paths, const char *path)
{
    if (!path || !path[0] || !FileExistsAPath(path))
        return;
    for (const std::string &existing : paths)
        if (StringEqualsNoCase(existing.c_str(), path))
            return;
    paths.push_back(path);
}

static void AddResolvedResourcePath(std::vector<std::string> &paths, const int fileId)
{
    FFXIResource::ResolvedFile resolved;
    if (FFXIResource::ResolveFileId(g_ffxiPath, fileId, resolved))
        AddUniqueResourcePath(paths, resolved.fullPath.c_str());
}

static void ShowCurrentZoneResourceBrowser()
{
    const int zoneId = FindZoneIDByModelPath(g_loadedZonePath);
    const FFXIZoneEntry *zone = zoneId >= 0 ? FFXIZone::FindByID(zoneId) : nullptr;
    if (!zone)
    {
        MessageBoxA(g_hWnd, "Load a known zone before opening its resources.",
                    "Zone Resources", MB_OK | MB_ICONINFORMATION);
        return;
    }

    std::vector<std::string> paths;
    char fullPath[MAX_PATH] = {};
    if (zone->dialogDat && zone->dialogDat[0])
    {
        BuildFFXIFullPath(zone->dialogDat, fullPath, sizeof(fullPath));
        AddUniqueResourcePath(paths, fullPath);
    }
    if (zone->npcDat && zone->npcDat[0])
    {
        BuildFFXIFullPath(zone->npcDat, fullPath, sizeof(fullPath));
        AddUniqueResourcePath(paths, fullPath);
    }

    // Historical file-ID banks from POLUtils are retained as fallbacks for
    // installations or zones whose static table entry is incomplete.
    if (zoneId <= 255)
    {
        AddResolvedResourcePath(paths, 6420 + zoneId);
        AddResolvedResourcePath(paths, 6720 + zoneId);
        AddResolvedResourcePath(paths, 67910 + zoneId);
    }
    if (zoneId >= 255)
    {
        AddResolvedResourcePath(paths, 85590 + (zoneId - 255));
        AddResolvedResourcePath(paths, 86490 + (zoneId - 255));
    }

    std::vector<FFXIResource::Row> rows;
    std::wstring formats;
    std::wstring warnings;
    int parsedFiles = 0;
    for (const std::string &path : paths)
    {
        FFXIResource::ParseResult parsed;
        if (!FFXIResource::ParseFile(path.c_str(), parsed))
            continue;
        ++parsedFiles;
        char relativePath[MAX_PATH] = {};
        MakeRelativeFFXIPath(path.c_str(), relativePath, sizeof(relativePath));
        const std::wstring wideRelative = WideFromAnsi(relativePath);
        if (!formats.empty()) formats += L"; ";
        formats += parsed.format + L" [" + wideRelative + L"]";
        if (!parsed.warning.empty() && warnings.find(parsed.warning) == std::wstring::npos)
        {
            if (!warnings.empty()) warnings += L" ";
            warnings += parsed.warning;
        }
        for (FFXIResource::Row row : parsed.rows)
        {
            if (!row.details.empty()) row.details += L" | ";
            row.details += wideRelative;
            rows.push_back(std::move(row));
        }
    }

    std::wstring title = L"DATura Resources - " + WideFromAnsi(zone->name);
    std::wstring source = L"Zone " + std::to_wstring(zoneId) + L": " + WideFromAnsi(zone->name);
    if (!formats.empty()) source += L" | " + formats;
    std::wstring status = std::to_wstring(rows.size()) + L" records from " +
                          std::to_wstring(parsedFiles) + L" resource file(s).";
    if (!warnings.empty()) status += L" " + warnings;
    if (rows.empty())
        status = L"No supported dialog or NPC resource file could be parsed for this zone.";
    PopulateResourceBrowser(title, source, rows, status);
}

static void ShowResourceDatBrowser()
{
    char path[MAX_PATH] = {};
    OPENFILENAMEA open = {};
    open.lStructSize = sizeof(open);
    open.hwndOwner = g_hWnd;
    open.lpstrFile = path;
    open.nMaxFile = sizeof(path);
    open.lpstrFilter = "FFXI DAT Files (*.DAT)\0*.DAT\0All Files (*.*)\0*.*\0\0";
    open.nFilterIndex = 1;
    open.lpstrInitialDir = g_ffxiPath[0] ? g_ffxiPath : nullptr;
    open.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetOpenFileNameA(&open))
        return;

    FFXIResource::ParseResult parsed;
    const bool recognized = FFXIResource::ParseFile(path, parsed);
    const std::wstring widePath = WideFromAnsi(path);
    const std::wstring title = recognized ? L"DATura Resource Browser - " + parsed.format :
                                            L"DATura Resource Browser - Unrecognized";
    std::wstring status = recognized ? std::to_wstring(parsed.rows.size()) + L" records." : parsed.warning;
    if (recognized && !parsed.warning.empty()) status += L" " + parsed.warning;
    PopulateResourceBrowser(title, widePath, parsed.rows, status);
}

//========================================================================================
// Matrix math (no D3DX dependency)
//========================================================================================

static D3DMATRIX BuildIdentity()
{
    D3DMATRIX m = {};
    m._11 = m._22 = m._33 = m._44 = 1.0f;
    return m;
}

// Left-handed look-at.  Up vector is assumed to be (0,1,0).
static D3DMATRIX BuildLookAtLH(float ex, float ey, float ez,
                                 float tx, float ty, float tz)
{
    float zx = tx-ex, zy = ty-ey, zz = tz-ez;
    float zl = sqrtf(zx*zx + zy*zy + zz*zz);
    if (zl > 1e-8f) { zx/=zl; zy/=zl; zz/=zl; }

    // x = normalize(cross(worldUp, z))
    float xx = -zz, xy = 0.0f, xz = zx; // cross((0,1,0), z)
    float xl = sqrtf(xx*xx + xy*xy + xz*xz);
    if (xl > 1e-8f) { xx/=xl; xy/=xl; xz/=xl; }
    else { xx=1; xy=0; xz=0; } // degenerate — camera pointing straight up/down

    // y = cross(z, x)
    float yx = zy*xz - zz*xy;
    float yy = zz*xx - zx*xz;
    float yz = zx*xy - zy*xx;

    D3DMATRIX m = {};
    m._11=xx;  m._12=yx;  m._13=zx;  m._14=0;
    m._21=xy;  m._22=yy;  m._23=zy;  m._24=0;
    m._31=xz;  m._32=yz;  m._33=zz;  m._34=0;
    m._41 = -(xx*ex + xy*ey + xz*ez);
    m._42 = -(yx*ex + yy*ey + yz*ez);
    m._43 = -(zx*ex + zy*ey + zz*ez);
    m._44 = 1.0f;
    return m;
}

// Left-handed perspective projection.
static D3DMATRIX BuildPerspectiveFovLH(float fovY, float aspect, float zn, float zf)
{
    const float h = 1.0f / tanf(fovY * 0.5f);
    const float w = h / aspect;
    D3DMATRIX m = {};
    m._11 = w;
    m._22 = h;
    m._33 = zf / (zf - zn);
    m._34 = 1.0f;
    m._43 = -zn * zf / (zf - zn);
    // m._44 intentionally 0 (perspective divide)
    return m;
}

static D3DMATRIX BuildPlayerWorldMatrix()
{
    D3DMATRIX m = BuildIdentity();
    const float renderYaw = g_playerYaw + (3.1415926535f * 0.5f);
    const float c = cosf(renderYaw);
    const float s = sinf(renderYaw);

    // Character DATs are now built in the correct up-axis after fixing skeleton
    // quaternion conversion. Keep only the yaw offset needed to align model
    // forward with DATura's movement/camera convention.
    m._11 =  c;
    m._13 = -s;
    m._22 =  1.0f;
    m._31 =  s;
    m._33 =  c;
    m._41 = g_playerPos[0];
    m._42 = g_playerPos[1];
    m._43 = g_playerPos[2];
    return m;
}

static D3DMATRIX MultiplyMatrix(const D3DMATRIX &a, const D3DMATRIX &b)
{
    D3DMATRIX out = {};
    const float *pa = &a._11;
    const float *pb = &b._11;
    float *po = &out._11;
    for (int r = 0; r < 4; ++r)
    {
        for (int c = 0; c < 4; ++c)
        {
            po[r * 4 + c] =
                pa[r * 4 + 0] * pb[0 * 4 + c] +
                pa[r * 4 + 1] * pb[1 * 4 + c] +
                pa[r * 4 + 2] * pb[2 * 4 + c] +
                pa[r * 4 + 3] * pb[3 * 4 + c];
        }
    }
    return out;
}

static D3DMATRIX BuildRotationX(float radians)
{
    D3DMATRIX m = BuildIdentity();
    const float s = sinf(radians);
    const float c = cosf(radians);
    m._22 = c;
    m._23 = s;
    m._32 = -s;
    m._33 = c;
    return m;
}

static D3DMATRIX BuildRotationY(float radians)
{
    D3DMATRIX m = BuildIdentity();
    const float s = sinf(radians);
    const float c = cosf(radians);
    m._11 = c;
    m._13 = -s;
    m._31 = s;
    m._33 = c;
    return m;
}

static D3DMATRIX BuildRotationZ(float radians)
{
    D3DMATRIX m = BuildIdentity();
    const float s = sinf(radians);
    const float c = cosf(radians);
    m._11 = c;
    m._12 = s;
    m._21 = -s;
    m._22 = c;
    return m;
}

static D3DMATRIX BuildZoneDebugMatrix(const ZoneDebugTransform &xform)
{
    D3DMATRIX scale = BuildIdentity();
    scale._11 = xform.scale[0];
    scale._22 = xform.scale[1];
    scale._33 = xform.scale[2];

    D3DMATRIX world = MultiplyMatrix(scale, BuildRotationX(xform.rot[0]));
    world = MultiplyMatrix(world, BuildRotationY(xform.rot[1]));
    world = MultiplyMatrix(world, BuildRotationZ(xform.rot[2]));
    world._41 = xform.trans[0];
    world._42 = xform.trans[1];
    world._43 = xform.trans[2];
    return world;
}

// Move the orbit target in view-relative directions, preserving mouse orbit/zoom behavior.
static void UpdateFlyCameraMovement(float dt)
{
    if (!g_hWnd || GetForegroundWindow() != g_hWnd)
        return;

    float moveX = 0.0f;
    float moveY = 0.0f;
    float moveZ = 0.0f;

    if (GetAsyncKeyState('W') & 0x8000) moveZ += 1.0f;
    if (GetAsyncKeyState('S') & 0x8000) moveZ -= 1.0f;
    if (GetAsyncKeyState('D') & 0x8000) moveX += 1.0f;
    if (GetAsyncKeyState('A') & 0x8000) moveX -= 1.0f;
    if (GetAsyncKeyState('Q') & 0x8000) moveY += 1.0f;
    if (GetAsyncKeyState('E') & 0x8000) moveY -= 1.0f;

    if (moveX == 0.0f && moveY == 0.0f && moveZ == 0.0f)
        return;

    const float len = sqrtf(moveX*moveX + moveY*moveY + moveZ*moveZ);
    moveX /= len;
    moveY /= len;
    moveZ /= len;

    const float forwardX = -sinf(g_camYaw);
    const float forwardZ = -cosf(g_camYaw);
    const float rightX   =  cosf(g_camYaw);
    const float rightZ   = -sinf(g_camYaw);

    float speed = g_camDist * 1.5f;
    if (speed < 2.0f) speed = 2.0f;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000)   speed *= 4.0f;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) speed *= 0.25f;

    const float step = speed * dt;
    g_camTarget[0] += (rightX * moveX + forwardX * moveZ) * step;
    g_camTarget[1] += moveY * step;
    g_camTarget[2] += (rightZ * moveX + forwardZ * moveZ) * step;
}

static void PanOrbitCameraByMouseDelta(int dx, int dy)
{
    if (dx == 0 && dy == 0)
        return;

    const float forwardX = -sinf(g_camYaw) * cosf(g_camPitch);
    const float forwardY = -sinf(g_camPitch);
    const float forwardZ = -cosf(g_camYaw) * cosf(g_camPitch);
    const float rightX   =  cosf(g_camYaw);
    const float rightY   =  0.0f;
    const float rightZ   = -sinf(g_camYaw);

    float upX = rightY * forwardZ - rightZ * forwardY;
    float upY = rightZ * forwardX - rightX * forwardZ;
    float upZ = rightX * forwardY - rightY * forwardX;
    const float upLen = sqrtf(upX * upX + upY * upY + upZ * upZ);
    if (upLen > 0.0f)
    {
        upX /= upLen;
        upY /= upLen;
        upZ /= upLen;
    }

    const float scale = g_camDist * 0.0016f;
    g_camTarget[0] += (-rightX * (float)dx - upX * (float)dy) * scale;
    g_camTarget[1] += (-rightY * (float)dx - upY * (float)dy) * scale;
    g_camTarget[2] += (-rightZ * (float)dx - upZ * (float)dy) * scale;
}

static float Vec3Dot(const float a[3], const float b[3])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static void Vec3Cross(const float a[3], const float b[3], float out[3])
{
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

static float Vec3Normalize(float v[3])
{
    const float len = sqrtf(Vec3Dot(v, v));
    if (len > 0.00001f)
    {
        v[0] /= len;
        v[1] /= len;
        v[2] /= len;
    }
    return len;
}

static int CollisionGridCoord(float v)
{
    return (int)floorf(v / kCollisionGridCellSize);
}

static long long CollisionGridKey(int gx, int gz)
{
    return (((long long)gx) << 32) ^ (unsigned int)gz;
}

static void AddCollisionTriToGrid(int triIndex)
{
    const ZoneCollisionTriangle &tri = g_zoneCollisionTris[triIndex];
    const int minGX = CollisionGridCoord(tri.minX - kPlayerCollisionRadius);
    const int maxGX = CollisionGridCoord(tri.maxX + kPlayerCollisionRadius);
    const int minGZ = CollisionGridCoord(tri.minZ - kPlayerCollisionRadius);
    const int maxGZ = CollisionGridCoord(tri.maxZ + kPlayerCollisionRadius);
    for (int gz = minGZ; gz <= maxGZ; ++gz)
    {
        for (int gx = minGX; gx <= maxGX; ++gx)
            g_zoneCollisionGrid[CollisionGridKey(gx, gz)].push_back(triIndex);
    }
}

static void ClearZoneCollision()
{
    g_zoneCollisionTris.clear();
    g_zoneCollisionGrid.clear();
    g_haveZoneCollisionBounds = false;
    g_playerVelY = 0.0f;
    g_playerOnGround = false;
    g_havePlayerLastSafe = false;
}

static void BuildZoneCollisionFromDAT()
{
    ClearZoneCollision();
    const int collisionTriCount = Model_FF11_GetLastCollisionTriangleCount();
    g_zoneCollisionTris.reserve(collisionTriCount);

    for (int i = 0; i < collisionTriCount; ++i)
    {
        const float *src = Model_FF11_GetLastCollisionTrianglePoints(i);
        if (!src)
            continue;

        ZoneCollisionTriangle tri = {};
        for (int v = 0; v < 3; ++v)
        {
            for (int axis = 0; axis < 3; ++axis)
                tri.p[v][axis] = src[v * 3 + axis];
        }
        if (!g_mirrorWorldZones)
        {
            for (int v = 0; v < 3; ++v)
                tri.p[v][0] = -tri.p[v][0];
            for (int axis = 0; axis < 3; ++axis)
            {
                const float temp = tri.p[1][axis];
                tri.p[1][axis] = tri.p[2][axis];
                tri.p[2][axis] = temp;
            }
        }

        float e0[3] = { tri.p[1][0] - tri.p[0][0], tri.p[1][1] - tri.p[0][1], tri.p[1][2] - tri.p[0][2] };
        float e1[3] = { tri.p[2][0] - tri.p[0][0], tri.p[2][1] - tri.p[0][1], tri.p[2][2] - tri.p[0][2] };
        Vec3Cross(e0, e1, tri.normal);
        if (Vec3Normalize(tri.normal) <= 0.0001f)
            continue;

        tri.minX = tri.maxX = tri.p[0][0];
        tri.minY = tri.maxY = tri.p[0][1];
        tri.minZ = tri.maxZ = tri.p[0][2];
        for (int v = 1; v < 3; ++v)
        {
            tri.minX = (tri.p[v][0] < tri.minX) ? tri.p[v][0] : tri.minX;
            tri.maxX = (tri.p[v][0] > tri.maxX) ? tri.p[v][0] : tri.maxX;
            tri.minY = (tri.p[v][1] < tri.minY) ? tri.p[v][1] : tri.minY;
            tri.maxY = (tri.p[v][1] > tri.maxY) ? tri.p[v][1] : tri.maxY;
            tri.minZ = (tri.p[v][2] < tri.minZ) ? tri.p[v][2] : tri.minZ;
            tri.maxZ = (tri.p[v][2] > tri.maxZ) ? tri.p[v][2] : tri.maxZ;
        }

        const int triIndex = (int)g_zoneCollisionTris.size();
        g_zoneCollisionTris.push_back(tri);
        if (!g_haveZoneCollisionBounds)
        {
            g_zoneCollisionMin[0] = tri.minX; g_zoneCollisionMax[0] = tri.maxX;
            g_zoneCollisionMin[1] = tri.minY; g_zoneCollisionMax[1] = tri.maxY;
            g_zoneCollisionMin[2] = tri.minZ; g_zoneCollisionMax[2] = tri.maxZ;
            g_haveZoneCollisionBounds = true;
        }
        else
        {
            if (tri.minX < g_zoneCollisionMin[0]) g_zoneCollisionMin[0] = tri.minX;
            if (tri.maxX > g_zoneCollisionMax[0]) g_zoneCollisionMax[0] = tri.maxX;
            if (tri.minY < g_zoneCollisionMin[1]) g_zoneCollisionMin[1] = tri.minY;
            if (tri.maxY > g_zoneCollisionMax[1]) g_zoneCollisionMax[1] = tri.maxY;
            if (tri.minZ < g_zoneCollisionMin[2]) g_zoneCollisionMin[2] = tri.minZ;
            if (tri.maxZ > g_zoneCollisionMax[2]) g_zoneCollisionMax[2] = tri.maxZ;
        }
        AddCollisionTriToGrid(triIndex);
    }
}

static void ResetZoneCameraFromCollision()
{
    // Keep the editor fly/orbit controls at their historical defaults. The
    // collision data is only used to choose where that unchanged camera starts.
    g_camYaw = 0.0f;
    g_camPitch = 0.25f;
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
    const float initialCameraY = g_zoneCollisionMax[1] + 10.0f;
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
    g_camPitch = 0.12f;
    g_camDist = std::clamp(radius * 2.6f, 2.5f, 500.0f);
}

static void QueryCollisionTris(float x, float z, float radius, std::vector<int> &outIndices)
{
    outIndices.clear();
    const int minGX = CollisionGridCoord(x - radius);
    const int maxGX = CollisionGridCoord(x + radius);
    const int minGZ = CollisionGridCoord(z - radius);
    const int maxGZ = CollisionGridCoord(z + radius);
    for (int gz = minGZ; gz <= maxGZ; ++gz)
    {
        for (int gx = minGX; gx <= maxGX; ++gx)
        {
            std::map<long long, std::vector<int> >::const_iterator it =
                g_zoneCollisionGrid.find(CollisionGridKey(gx, gz));
            if (it == g_zoneCollisionGrid.end())
                continue;
            for (size_t i = 0; i < it->second.size(); ++i)
            {
                const int triIndex = it->second[i];
                if (std::find(outIndices.begin(), outIndices.end(), triIndex) == outIndices.end())
                    outIndices.push_back(triIndex);
            }
        }
    }
}

static bool PointInTriangleXZ(float x, float z, const ZoneCollisionTriangle &tri)
{
    const float x0 = tri.p[0][0], z0 = tri.p[0][2];
    const float x1 = tri.p[1][0], z1 = tri.p[1][2];
    const float x2 = tri.p[2][0], z2 = tri.p[2][2];
    const float denom = (z1 - z2) * (x0 - x2) + (x2 - x1) * (z0 - z2);
    if (fabsf(denom) < 0.00001f)
        return false;
    const float a = ((z1 - z2) * (x - x2) + (x2 - x1) * (z - z2)) / denom;
    const float b = ((z2 - z0) * (x - x2) + (x0 - x2) * (z - z2)) / denom;
    const float c = 1.0f - a - b;
    const float eps = -0.001f;
    return a >= eps && b >= eps && c >= eps;
}

static float PointSegmentDistSqXZ(float x, float z, const float a[3], const float b[3])
{
    const float abX = b[0] - a[0];
    const float abZ = b[2] - a[2];
    const float lenSq = abX * abX + abZ * abZ;
    if (lenSq <= 0.00001f)
    {
        const float dx = x - a[0];
        const float dz = z - a[2];
        return dx * dx + dz * dz;
    }

    float t = ((x - a[0]) * abX + (z - a[2]) * abZ) / lenSq;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    const float px = a[0] + abX * t;
    const float pz = a[2] + abZ * t;
    const float dx = x - px;
    const float dz = z - pz;
    return dx * dx + dz * dz;
}

static bool PointNearTriangleXZ(float x, float z, const ZoneCollisionTriangle &tri, float radius)
{
    if (x < tri.minX - radius || x > tri.maxX + radius ||
        z < tri.minZ - radius || z > tri.maxZ + radius)
        return false;
    if (PointInTriangleXZ(x, z, tri))
        return true;
    const float radiusSq = radius * radius;
    return PointSegmentDistSqXZ(x, z, tri.p[0], tri.p[1]) <= radiusSq ||
           PointSegmentDistSqXZ(x, z, tri.p[1], tri.p[2]) <= radiusSq ||
           PointSegmentDistSqXZ(x, z, tri.p[2], tri.p[0]) <= radiusSq;
}

static bool CollisionFloorAt(float x, float z, float minY, float maxY, float *outY, float outNormal[3])
{
    std::vector<int> candidates;
    QueryCollisionTris(x, z, kPlayerCollisionRadius, candidates);

    if (minY > maxY)
    {
        const float temp = minY;
        minY = maxY;
        maxY = temp;
    }

    bool found = false;
    float bestY = maxY;
    for (size_t i = 0; i < candidates.size(); ++i)
    {
        const ZoneCollisionTriangle &tri = g_zoneCollisionTris[candidates[i]];
        if (tri.normal[1] < 0.35f && tri.normal[1] > -0.35f)
            continue;
        if (maxY < tri.minY - 0.1f || minY > tri.maxY + 0.1f)
            continue;
        if (!PointInTriangleXZ(x, z, tri))
            continue;

        const float denom = tri.normal[1];
        if (fabsf(denom) < 0.0001f)
            continue;
        const float y = tri.p[0][1] -
            (tri.normal[0] * (x - tri.p[0][0]) + tri.normal[2] * (z - tri.p[0][2])) / denom;
        if (y >= minY && y <= maxY && (!found || y < bestY))
        {
            found = true;
            bestY = y;
            if (outNormal)
            {
                outNormal[0] = tri.normal[0];
                outNormal[1] = tri.normal[1];
                outNormal[2] = tri.normal[2];
                if (outNormal[1] < 0.0f)
                {
                    outNormal[0] = -outNormal[0];
                    outNormal[1] = -outNormal[1];
                    outNormal[2] = -outNormal[2];
                }
            }
        }
    }

    if (found && outY)
        *outY = bestY;
    return found;
}

static void SetPlayerRespawnPoint()
{
    g_playerRespawnPos[0] = g_playerPos[0];
    g_playerRespawnPos[1] = g_playerPos[1];
    g_playerRespawnPos[2] = g_playerPos[2];
    g_playerRespawnYaw = g_playerYaw;
    g_havePlayerRespawn = true;

    g_playerLastSafePos[0] = g_playerPos[0];
    g_playerLastSafePos[1] = g_playerPos[1];
    g_playerLastSafePos[2] = g_playerPos[2];
    g_playerLastSafeYaw = g_playerYaw;
    g_havePlayerLastSafe = true;
}

static void SetPlayerLastSafePoint()
{
    g_playerLastSafePos[0] = g_playerPos[0];
    g_playerLastSafePos[1] = g_playerPos[1];
    g_playerLastSafePos[2] = g_playerPos[2];
    g_playerLastSafeYaw = g_playerYaw;
    g_havePlayerLastSafe = true;
}

static void RespawnPlayer()
{
    if (!g_havePlayerRespawn)
        return;

    g_playerPos[0] = g_playerRespawnPos[0];
    g_playerPos[1] = g_playerRespawnPos[1];
    g_playerPos[2] = g_playerRespawnPos[2];
    g_playerYaw = g_playerRespawnYaw;
    g_playerVelY = 0.0f;
    g_playerOnGround = true;
    g_camTarget[0] = g_playerPos[0];
    g_camTarget[1] = g_playerPos[1] - 2.0f;
    g_camTarget[2] = g_playerPos[2];
    SetPlayerLastSafePoint();
}

static bool PlayerIsOutOfZoneBounds()
{
    if (!g_haveZoneCollisionBounds || !g_havePlayerRespawn)
        return false;

    const float horizontalMargin = 80.0f;
    const float downwardMargin = 80.0f;
    const float upwardMargin = 120.0f;
    return g_playerPos[0] < g_zoneCollisionMin[0] - horizontalMargin ||
           g_playerPos[0] > g_zoneCollisionMax[0] + horizontalMargin ||
           g_playerPos[2] < g_zoneCollisionMin[2] - horizontalMargin ||
           g_playerPos[2] > g_zoneCollisionMax[2] + horizontalMargin ||
           g_playerPos[1] > g_zoneCollisionMax[1] + downwardMargin ||
           g_playerPos[1] < g_zoneCollisionMin[1] - upwardMargin;
}

static void UpdatePlayerCameraTarget()
{
    g_camTarget[0] = g_playerPos[0];
    g_camTarget[1] = g_playerPos[1] - 2.0f;
    g_camTarget[2] = g_playerPos[2];
}

static bool IsEditMode()
{
    return g_interactionMode == kDATuraMode_Edit;
}

static bool IsGameMode()
{
    return g_interactionMode == kDATuraMode_Game;
}

static void SyncGameModeMusic()
{
    SyncAppMusic();
}

static bool CanPlaySoundsNow()
{
    if (!g_enableSounds)
        return false;
    HWND hForeground = GetForegroundWindow();
    if (!g_playSoundsInBackground && g_hWnd &&
        hForeground != g_hWnd && hForeground != g_hConfigDialog &&
        !AudioPlayer_OwnsWindow(hForeground))
        return false;
    return true;
}

static void SyncAppMusic()
{
    const bool canPlay = CanPlaySoundsNow();
    if (AudioPlayer_IsOpen())
    {
        AudioPlayer_SetPlaybackAllowed(canPlay);
        return;
    }

    if (!canPlay)
    {
        BGM_Stop();
        return;
    }

    if (g_titleScreenActive)
    {
        BGM_PlayZoneMusic(g_ffxiPath, kTitleScreenMusicId);
        return;
    }

    if (IsGameMode() && g_gameModeMusicId > 0)
    {
        BGM_PlayZoneMusic(g_ffxiPath, g_gameModeMusicId);
        return;
    }

    BGM_Stop();
}

static void SetGameModeMusic(int musicId)
{
    g_gameModeMusicId = musicId;
    SyncGameModeMusic();
}

static void SetInteractionMode(DATuraInteractionMode mode)
{
    g_interactionMode = mode;
    g_playerCamera = (mode == kDATuraMode_Game);

    if (g_playerCamera && g_pPlayerModel)
        UpdatePlayerCameraTarget();

    if (g_hWnd)
    {
        HMENU hMenu = GetMenu(g_hWnd);
        if (hMenu)
            CheckMenuItem(hMenu, IDM_VIEW_TOGGLE_EDIT_GAME_MODE,
                          MF_BYCOMMAND | (IsGameMode() ? MF_CHECKED : MF_UNCHECKED));
    }
    UpdateZoneObjectEditControlState();
    SyncGameModeMusic();

    if (g_hWnd)
        InvalidateRect(g_hWnd, NULL, FALSE);
}

static void ToggleEditGameMode()
{
    if (IsEditMode())
    {
        if (!g_pPlayerModel)
        {
            LoadPlayerRaceModel(0);
            if (!g_pPlayerModel)
                return;
        }
        SetInteractionMode(kDATuraMode_Game);
    }
    else
    {
        SetInteractionMode(kDATuraMode_Edit);
    }
}

static bool PlayerCollisionOverlapsWallAt(float x, float y, float z)
{
    std::vector<int> candidates;
    QueryCollisionTris(x, z, kPlayerCollisionRadius + 0.25f, candidates);

    const float playerTopY = y - kPlayerCollisionHeight;
    const float playerBottomY = y + kPlayerStepHeight;
    for (size_t i = 0; i < candidates.size(); ++i)
    {
        const ZoneCollisionTriangle &tri = g_zoneCollisionTris[candidates[i]];
        if (tri.normal[1] > 0.45f || tri.normal[1] < -0.45f)
            continue;
        if (playerBottomY < tri.minY || playerTopY > tri.maxY)
            continue;
        if (!PointNearTriangleXZ(x, z, tri, kPlayerCollisionRadius))
            continue;

        const float triHeight = tri.maxY - tri.minY;
        const bool shortStepFace =
            triHeight <= (kPlayerStepHeight + 0.25f) &&
            tri.minY >= y - (kPlayerStepHeight + 0.25f) &&
            tri.maxY <= y + (kPlayerStepHeight + 0.25f);
        if (!shortStepFace)
            return true;
    }
    return false;
}

static bool FindNearestSafePlayerFloor(float originX, float originY, float originZ,
                                       float *outX, float *outY, float *outZ)
{
    if (g_zoneCollisionTris.empty())
        return false;

    const float searchUp = 1.4f;
    const float searchDown = 12.0f;
    const float maxSnapUp = 1.25f;
    const float radii[] = { 0.0f, 0.35f, 0.7f, 1.1f, 1.6f, 2.3f, 3.2f, 4.5f, 6.0f };
    const int radiusCount = (int)(sizeof(radii) / sizeof(radii[0]));

    bool found = false;
    float bestX = originX;
    float bestY = originY;
    float bestZ = originZ;
    float bestDistSq = FLT_MAX;

    for (int r = 0; r < radiusCount; ++r)
    {
        const float radius = radii[r];
        const int samples = (radius <= 0.0f) ? 1 : ((radius < 2.0f) ? 12 : 24);
        for (int i = 0; i < samples; ++i)
        {
            float sampleX = originX;
            float sampleZ = originZ;
            if (radius > 0.0f)
            {
                const float angle = (6.28318530718f * (float)i) / (float)samples;
                sampleX += cosf(angle) * radius;
                sampleZ += sinf(angle) * radius;
            }

            float floorY = 0.0f;
            float floorN[3] = {};
            if (!CollisionFloorAt(sampleX, sampleZ, originY - searchUp, originY + searchDown, &floorY, floorN))
                continue;
            if (floorY < originY - maxSnapUp)
                continue;
            if (PlayerCollisionOverlapsWallAt(sampleX, floorY, sampleZ))
                continue;

            const float dx = sampleX - originX;
            const float dy = (floorY - originY) * 0.25f;
            const float dz = sampleZ - originZ;
            const float distSq = dx * dx + dy * dy + dz * dz;
            if (!found || distSq < bestDistSq)
            {
                found = true;
                bestDistSq = distSq;
                bestX = sampleX;
                bestY = floorY;
                bestZ = sampleZ;
            }
        }

        if (found)
            break;
    }

    if (found)
    {
        if (outX) *outX = bestX;
        if (outY) *outY = bestY;
        if (outZ) *outZ = bestZ;
    }
    return found;
}

static void UnstickPlayerFromGeometry()
{
    if (!IsGameMode() || g_zoneCollisionTris.empty())
        return;

    float safeX = 0.0f;
    float safeY = 0.0f;
    float safeZ = 0.0f;
    if (FindNearestSafePlayerFloor(g_playerPos[0], g_playerPos[1], g_playerPos[2], &safeX, &safeY, &safeZ))
    {
        g_playerPos[0] = safeX;
        g_playerPos[1] = safeY;
        g_playerPos[2] = safeZ;
        g_playerVelY = 0.0f;
        g_playerOnGround = true;
        SetPlayerLastSafePoint();
        UpdatePlayerCameraTarget();
        return;
    }

    if (g_havePlayerLastSafe)
    {
        g_playerPos[0] = g_playerLastSafePos[0];
        g_playerPos[1] = g_playerLastSafePos[1];
        g_playerPos[2] = g_playerLastSafePos[2];
        g_playerYaw = g_playerLastSafeYaw;
        g_playerVelY = 0.0f;
        g_playerOnGround = true;
        UpdatePlayerCameraTarget();
        return;
    }

    RespawnPlayer();
}

static void ResolveHorizontalCollision(float oldX, float oldZ, float &newX, float playerY, float &newZ)
{
    std::vector<int> candidates;
    QueryCollisionTris(newX, newZ, kPlayerCollisionRadius + 0.5f, candidates);

    for (int pass = 0; pass < 3; ++pass)
    {
        bool adjusted = false;
        for (size_t i = 0; i < candidates.size(); ++i)
        {
            const ZoneCollisionTriangle &tri = g_zoneCollisionTris[candidates[i]];
            if (tri.normal[1] > 0.45f || tri.normal[1] < -0.45f)
                continue;

            const float playerTopY = playerY - kPlayerCollisionHeight;
            const float playerBottomY = playerY + kPlayerStepHeight;
            if (playerBottomY < tri.minY || playerTopY > tri.maxY)
                continue;
            if (!PointNearTriangleXZ(newX, newZ, tri, kPlayerCollisionRadius))
                continue;

            const float triHeight = tri.maxY - tri.minY;
            const bool shortStepFace =
                g_playerOnGround &&
                triHeight <= (kPlayerStepHeight + 0.25f) &&
                tri.minY >= playerY - (kPlayerStepHeight + 0.25f) &&
                tri.maxY <= playerY + (kPlayerStepHeight + 0.25f);
            if (shortStepFace)
                continue;

            float nx = tri.normal[0];
            float nz = tri.normal[2];
            const float nLen = sqrtf(nx * nx + nz * nz);
            if (nLen < 0.0001f)
                continue;
            nx /= nLen;
            nz /= nLen;

            float oldDist = (oldX - tri.p[0][0]) * nx + (oldZ - tri.p[0][2]) * nz;
            float newDist = (newX - tri.p[0][0]) * nx + (newZ - tri.p[0][2]) * nz;
            if (fabsf(oldDist) < fabsf(newDist))
            {
                nx = -nx;
                nz = -nz;
                oldDist = -oldDist;
                newDist = -newDist;
            }

            if (newDist >= kPlayerCollisionRadius)
                continue;

            const float push = kPlayerCollisionRadius - newDist;
            if (push > 0.0f && push < kPlayerCollisionRadius * 2.5f)
            {
                newX += nx * push;
                newZ += nz * push;
                adjusted = true;
            }
        }
        if (!adjusted)
            break;
    }
}

static bool TryMovePlayerHorizontalStep(float targetX, float targetZ)
{
    const float oldX = g_playerPos[0];
    const float oldZ = g_playerPos[2];
    float newX = targetX;
    float newZ = targetZ;

    ResolveHorizontalCollision(oldX, oldZ, newX, g_playerPos[1], newZ);

    if (g_playerOnGround)
    {
        float floorY = 0.0f;
        float floorN[3] = {};
        const float maxStepUp = kPlayerStepHeight;
        const float maxStepDown = 1.25f;
        const bool haveFloor = CollisionFloorAt(newX, newZ,
                                                g_playerPos[1] - maxStepUp,
                                                g_playerPos[1] + maxStepDown,
                                                &floorY, floorN);
        if (!haveFloor)
            return false;

        if (floorY < g_playerPos[1] - maxStepUp ||
            floorY > g_playerPos[1] + maxStepDown)
            return false;

        g_playerPos[1] = floorY;
        g_playerVelY = 0.0f;
        g_playerOnGround = true;
    }

    g_playerPos[0] = newX;
    g_playerPos[2] = newZ;
    return true;
}

static void MovePlayerHorizontal(float deltaX, float deltaZ)
{
    const float distance = sqrtf(deltaX * deltaX + deltaZ * deltaZ);
    if (distance <= 0.0001f)
        return;

    const int steps = (int)ceilf(distance / 0.35f);
    const int clampedSteps = (steps < 1) ? 1 : ((steps > 24) ? 24 : steps);
    const float stepX = deltaX / (float)clampedSteps;
    const float stepZ = deltaZ / (float)clampedSteps;

    for (int i = 0; i < clampedSteps; ++i)
    {
        const float targetX = g_playerPos[0] + stepX;
        const float targetZ = g_playerPos[2] + stepZ;
        if (!TryMovePlayerHorizontalStep(targetX, targetZ))
            break;
    }
}

static void UpdatePlayerMovement(float dt)
{
    if (!g_hWnd || GetForegroundWindow() != g_hWnd)
        return;
    if (dt > 0.1f)
        dt = 0.1f;

    float turn = 0.0f;
    float move = 0.0f;

    if (GetAsyncKeyState('A') & 0x8000) turn += 1.0f;
    if (GetAsyncKeyState('D') & 0x8000) turn -= 1.0f;
    if (GetAsyncKeyState('W') & 0x8000) move += 1.0f;
    if (GetAsyncKeyState('S') & 0x8000) move -= 1.0f;

    const float turnSpeed = 2.5f;
    float moveSpeed = 8.0f;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000)   moveSpeed *= 2.0f;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) moveSpeed *= 0.35f;

    g_playerYaw += turn * turnSpeed * dt;

    const float step = move * moveSpeed * dt;
    const float deltaX = -sinf(g_playerYaw) * step;
    const float deltaZ = -cosf(g_playerYaw) * step;

    if (!g_zoneCollisionTris.empty())
    {
        MovePlayerHorizontal(deltaX, deltaZ);

        float floorY = 0.0f;
        float floorN[3] = {};
        const float searchMinY = g_playerPos[1] - (g_playerOnGround ? kPlayerStepHeight : 0.35f);
        const float searchMaxY = g_playerPos[1] + 80.0f;
        const bool haveFloor = CollisionFloorAt(g_playerPos[0], g_playerPos[2], searchMinY, searchMaxY, &floorY, floorN);

        const float gravity = 28.0f;
        g_playerVelY += gravity * dt;
        if (g_playerVelY > 80.0f)
            g_playerVelY = 80.0f;
        g_playerPos[1] += g_playerVelY * dt;

        if (haveFloor && g_playerPos[1] >= floorY - 0.04f)
        {
            g_playerPos[1] = floorY;
            g_playerVelY = 0.0f;
            g_playerOnGround = true;
            if (!PlayerCollisionOverlapsWallAt(g_playerPos[0], g_playerPos[1], g_playerPos[2]))
                SetPlayerLastSafePoint();
        }
        else
        {
            g_playerOnGround = false;
        }

        if (PlayerIsOutOfZoneBounds())
        {
            RespawnPlayer();
            return;
        }
    }
    else
    {
        float moveY = 0.0f;
        if (GetAsyncKeyState('Q') & 0x8000) moveY += 1.0f;
        if (GetAsyncKeyState('E') & 0x8000) moveY -= 1.0f;
        g_playerPos[0] += deltaX;
        g_playerPos[2] += deltaZ;
        g_playerPos[1] += moveY * moveSpeed * dt;
    }

    g_camTarget[0] = g_playerPos[0];
    g_camTarget[1] = g_playerPos[1] - 2.0f;
    g_camTarget[2] = g_playerPos[2];
}

static void UpdatePlayerAnimation(float dt)
{
    if (!g_pPlayerModel || !g_pDevice)
        return;

    const bool moving =
        (GetAsyncKeyState('W') & 0x8000) ||
        (GetAsyncKeyState('S') & 0x8000) ||
        (GetAsyncKeyState('A') & 0x8000) ||
        (GetAsyncKeyState('D') & 0x8000);

    if (!g_playerEquip.animationPlaying && (!IsGameMode() || !moving))
    {
        g_playerAnimTime = 0.0f;
        g_pPlayerModel->RestoreBindPose(g_pDevice);
        return;
    }

    g_playerAnimTime += dt;
    g_pPlayerModel->UpdateAnimation(g_playerAnimTime, g_pDevice);
}

static void UploadSubmeshVertices(noesisModel_t::Submesh &sm)
{
    if (!sm.pVB || sm.cpuVerts.empty())
        return;

    void *pVBData = nullptr;
    const UINT vbSize = (UINT)(sm.cpuVerts.size() * sizeof(FFXIVertex));
    if (SUCCEEDED(sm.pVB->Lock(0, 0, &pVBData, 0)))
    {
        memcpy(pVBData, sm.cpuVerts.data(), vbSize);
        sm.pVB->Unlock();
    }
}

static void UploadSubmeshIndices(noesisModel_t::Submesh &sm)
{
    if (!sm.pIB || sm.cpuIndices.empty())
        return;

    void *pIBData = nullptr;
    const UINT ibSize = (UINT)(sm.cpuIndices.size() * sizeof(DWORD));
    if (SUCCEEDED(sm.pIB->Lock(0, 0, &pIBData, 0)))
    {
        memcpy(pIBData, sm.cpuIndices.data(), ibSize);
        sm.pIB->Unlock();
    }
}

static void MirrorZoneModelOnX(noesisModel_t *pModel)
{
    // The normal (unchecked) view corrects the DAT's mirrored X orientation.
    // Checked exposes that mirrored orientation as requested by the UI option.
    if (!pModel || g_mirrorWorldZones)
        return;

    for (noesisModel_t::Submesh &sm : pModel->submeshes)
    {
        for (FFXIVertex &v : sm.cpuVerts)
        {
            v.pos[0] = -v.pos[0];
            v.nrm[0] = -v.nrm[0];
        }
        for (FFXIVertex &v : sm.cpuBindVerts)
        {
            v.pos[0] = -v.pos[0];
            v.nrm[0] = -v.nrm[0];
        }
        for (size_t i = 0; i + 2 < sm.cpuIndices.size(); i += 3)
            std::swap(sm.cpuIndices[i + 1], sm.cpuIndices[i + 2]);

        UploadSubmeshVertices(sm);
        UploadSubmeshIndices(sm);
    }
}

static float SqleMotionValue(const CreationSqleMotionInfo &motion, int frameIndex, int channelIndex)
{
    if (!motion.valid || channelIndex < 0 || channelIndex >= motion.channelCount)
        return 0.0f;
    if (frameIndex < 0)
        frameIndex = 0;
    if (frameIndex > motion.frameCount)
        frameIndex = motion.frameCount;

    const size_t index = (size_t)frameIndex * (size_t)motion.channelCount + (size_t)channelIndex;
    return (index < motion.frameValues.size()) ? motion.frameValues[index] : 0.0f;
}

static void SqleNormalizeQuat(float q[4])
{
    const float len = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (len <= 0.00001f)
    {
        q[0] = q[1] = q[2] = 0.0f;
        q[3] = 1.0f;
        return;
    }
    q[0] /= len;
    q[1] /= len;
    q[2] /= len;
    q[3] /= len;
}

static void SqleInvertQuat(const float q[4], float out[4])
{
    out[0] = -q[0];
    out[1] = -q[1];
    out[2] = -q[2];
    out[3] =  q[3];
}

static void SqleMulQuat(const float a[4], const float b[4], float out[4])
{
    out[0] = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
    out[1] = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
    out[2] = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
    out[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
    SqleNormalizeQuat(out);
}

static void SqleRotateVec(const float q[4], const float v[3], float out[3])
{
    const float x = q[0], y = q[1], z = q[2], w = q[3];
    const float tx = 2.0f * (y * v[2] - z * v[1]);
    const float ty = 2.0f * (z * v[0] - x * v[2]);
    const float tz = 2.0f * (x * v[1] - y * v[0]);
    out[0] = v[0] + w * tx + (y * tz - z * ty);
    out[1] = v[1] + w * ty + (z * tx - x * tz);
    out[2] = v[2] + w * tz + (x * ty - y * tx);
}

static bool SqleReadTransformGroup(const CreationSqleMotionInfo &motion, int frameIndex,
                                   int groupIndex, float trans[3], float quat[4])
{
    const int base = groupIndex * 7;
    if (!motion.frameChannel || base + 6 >= motion.channelCount)
        return false;

    trans[0] = SqleMotionValue(motion, frameIndex, base + 0);
    trans[1] = SqleMotionValue(motion, frameIndex, base + 1);
    trans[2] = SqleMotionValue(motion, frameIndex, base + 2);
    quat[0] = SqleMotionValue(motion, frameIndex, base + 3);
    quat[1] = SqleMotionValue(motion, frameIndex, base + 4);
    quat[2] = SqleMotionValue(motion, frameIndex, base + 5);
    quat[3] = SqleMotionValue(motion, frameIndex, base + 6);
    SqleNormalizeQuat(quat);
    return true;
}

static int SqleFindNearestTransformGroup(const CreationSqleMotionInfo &motion,
                                         const float sourcePos[3])
{
    const int groupCount = motion.channelCount / 7;
    int bestGroup = -1;
    float bestDistSq = FLT_MAX;
    for (int groupIndex = 0; groupIndex < groupCount; ++groupIndex)
    {
        float trans[3] = {};
        float quat[4] = {};
        if (!SqleReadTransformGroup(motion, 1, groupIndex, trans, quat))
            continue;

        const float dx = sourcePos[0] - trans[0];
        const float dy = sourcePos[1] - trans[1];
        const float dz = sourcePos[2] - trans[2];
        const float distSq = dx * dx + dy * dy + dz * dz;
        if (distSq < bestDistSq)
        {
            bestDistSq = distSq;
            bestGroup = groupIndex;
        }
    }
    return bestGroup;
}

static void ApplySqleFrameChannelToSubmesh(noesisModel_t::Submesh &sm,
                                           const CreationSqleMotionInfo &motion,
                                           int frameIndex)
{
    if (!motion.valid || !motion.frameChannel || motion.frameValues.empty() ||
        sm.cpuBindVerts.empty() || sm.cpuBindVerts.size() != sm.cpuVerts.size())
    {
        return;
    }

    sm.cpuVerts = sm.cpuBindVerts;
    for (FFXIVertex &v : sm.cpuVerts)
    {
        float sourcePos[3] = { v.pos[0], -v.pos[1], v.pos[2] };
        const int groupIndex = SqleFindNearestTransformGroup(motion, sourcePos);
        if (groupIndex < 0)
            continue;

        float bindTrans[3] = {};
        float frameTrans[3] = {};
        float bindQuat[4] = {};
        float frameQuat[4] = {};
        if (!SqleReadTransformGroup(motion, 1, groupIndex, bindTrans, bindQuat) ||
            !SqleReadTransformGroup(motion, frameIndex, groupIndex, frameTrans, frameQuat))
        {
            continue;
        }

        float invBindQuat[4] = {};
        float deltaQuat[4] = {};
        SqleInvertQuat(bindQuat, invBindQuat);
        SqleMulQuat(frameQuat, invBindQuat, deltaQuat);

        float local[3] =
        {
            sourcePos[0] - bindTrans[0],
            sourcePos[1] - bindTrans[1],
            sourcePos[2] - bindTrans[2]
        };
        float rotatedLocal[3] = {};
        SqleRotateVec(deltaQuat, local, rotatedLocal);

        sourcePos[0] += (frameTrans[0] - bindTrans[0]) + (rotatedLocal[0] - local[0]);
        sourcePos[1] += (frameTrans[1] - bindTrans[1]) + (rotatedLocal[1] - local[1]);
        sourcePos[2] += (frameTrans[2] - bindTrans[2]) + (rotatedLocal[2] - local[2]);

        v.pos[0] = sourcePos[0];
        v.pos[1] = -sourcePos[1];
        v.pos[2] = sourcePos[2];

        float sourceNrm[3] = { v.nrm[0], -v.nrm[1], v.nrm[2] };
        float rotatedNrm[3] = {};
        SqleRotateVec(deltaQuat, sourceNrm, rotatedNrm);
        v.nrm[0] = rotatedNrm[0];
        v.nrm[1] = -rotatedNrm[1];
        v.nrm[2] = rotatedNrm[2];
    }

    UploadSubmeshVertices(sm);
}

static int SqleExpectedChannelCount(const noesisModel_t *pModel, int fileIndex)
{
    int count = 0;
    if (!pModel)
        return 0;
    for (const FFXISqleBoneInfo &bone : pModel->sqleBones)
    {
        if (bone.fileIndex != fileIndex)
            continue;
        for (int group = 0; group < 5; ++group)
            count += bone.channelCounts[group];
    }
    return count;
}

static RichMat43 SqleBuildLocalMatrix(const FFXISqleBoneInfo &bone,
                                      const CreationSqleMotionInfo *pMotion,
                                      int sourceFrame, int &channelCursor)
{
    float trans[3] = { bone.bindTranslation[0], bone.bindTranslation[1], bone.bindTranslation[2] };
    float quat[4] = { bone.bindQuaternion[0], bone.bindQuaternion[1], bone.bindQuaternion[2], bone.bindQuaternion[3] };
    float scale[3] = { bone.bindScale[0], bone.bindScale[1], bone.bindScale[2] };
    float *groups[3] = { trans, quat, scale };
    const int capacities[3] = { 3, 4, 3 };
    for (int group = 0; group < 5; ++group)
    {
        const int componentCount = bone.channelCounts[group];
        for (int component = 0; component < componentCount; ++component)
        {
            if (pMotion && group < 3 && component < capacities[group])
                groups[group][component] = SqleMotionValue(*pMotion, sourceFrame, channelCursor);
            ++channelCursor;
        }
    }
    SqleNormalizeQuat(quat);

    RichMat43 local = RichQuat(quat[0], quat[1], quat[2], quat[3]).ToMat43(false);
    local[0] = local[0] * scale[0];
    local[1] = local[1] * scale[1];
    local[2] = local[2] * scale[2];
    local[3] = RichVec3(trans);

    static const float signs[3] = { 1.0f, -1.0f, 1.0f };
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col)
            local[row][col] *= signs[row] * signs[col];
    for (int axis = 0; axis < 3; ++axis)
        local[3][axis] *= signs[axis];
    if (bone.parentIndex < 0)
        for (int axis = 0; axis < 3; ++axis)
            local[3][axis] += bone.rootOffset[axis];
    return local;
}

static void BuildHighPolyCreationAnimation(noesisModel_t *pModel)
{
    if (!pModel || pModel->sqleBones.empty())
        return;

    const CreationSqleMotionInfo *motions[2] = { &g_creationBodyMotion, &g_creationHeadMotion };
    bool compatible[2] = {};
    for (int fileIndex = 0; fileIndex < 2; ++fileIndex)
    {
        const int expected = SqleExpectedChannelCount(pModel, fileIndex);
        compatible[fileIndex] = expected > 0 && motions[fileIndex]->valid &&
            motions[fileIndex]->frameChannel && expected == motions[fileIndex]->channelCount;
    }
    // A partial skeletal clip leaves most of the character motionless.  Let the
    // preview updater use the legacy FrameChannel fallback unless both pieces
    // can participate in the combined skeletal animation.
    if (!compatible[0] || !compatible[1])
        return;

    const CreationSqleMotionInfo &timing = compatible[0] ? *motions[0] : *motions[1];
    const int frameCount = std::max(1, timing.frameCount);
    const float duration = timing.timeSeconds > 0.01f ? timing.timeSeconds : (float)frameCount / 60.0f;
    noesisAnim_t *pAnim = new noesisAnim_t();
    pAnim->frameCount = frameCount;
    pAnim->fps = (float)frameCount / duration;
    pAnim->boneCount = (int)pModel->sqleBones.size();
    pAnim->frameWorldMats.resize((size_t)pAnim->frameCount * (size_t)pAnim->boneCount);

    for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex)
    {
        int channelCursors[8] = {};
        RichMat43 *pWorld = &pAnim->frameWorldMats[(size_t)frameIndex * (size_t)pAnim->boneCount];
        for (int boneIndex = 0; boneIndex < pAnim->boneCount; ++boneIndex)
        {
            const FFXISqleBoneInfo &bone = pModel->sqleBones[(size_t)boneIndex];
            const int fileIndex = bone.fileIndex;
            const CreationSqleMotionInfo *pMotion = (fileIndex >= 0 && fileIndex < 2 && compatible[fileIndex]) ?
                motions[fileIndex] : NULL;
            int sourceFrame = 1;
            if (pMotion)
            {
                const float phase = frameCount > 1 ? (float)frameIndex / (float)(frameCount - 1) : 0.0f;
                sourceFrame = 1 + (int)(phase * (float)std::max(0, pMotion->frameCount - 1));
            }
            pWorld[boneIndex] = SqleBuildLocalMatrix(bone, pMotion, sourceFrame,
                channelCursors[(fileIndex >= 0 && fileIndex < 8) ? fileIndex : 0]);
            if (bone.parentIndex >= 0 && bone.parentIndex < boneIndex)
                pWorld[boneIndex] = pWorld[boneIndex] * pWorld[bone.parentIndex];
        }
    }
    pModel->pAnim = pAnim;
}

static void UpdateHighPolyCreationAnimation(float dt)
{
    if (!g_highPolyCreationActive || !g_pZoneModel || !g_pDevice)
        return;

    if (g_creationAnimationIndex <= 0)
    {
        g_creationAnimTime = 0.0f;
        g_pZoneModel->RestoreBindPose(g_pDevice);
        return;
    }

    g_creationAnimTime += dt;
    if (g_pZoneModel->pAnim)
    {
        g_pZoneModel->UpdateAnimation(g_creationAnimTime, g_pDevice);
        return;
    }

    const CreationSqleMotionInfo &timing =
        (g_creationBodyMotion.valid && g_creationBodyMotion.frameChannel) ?
        g_creationBodyMotion : g_creationHeadMotion;
    if (!timing.valid || !timing.frameChannel || timing.frameValues.empty())
    {
        g_pZoneModel->RestoreBindPose(g_pDevice);
        return;
    }

    const float duration = (timing.timeSeconds > 0.05f) ? timing.timeSeconds : 1.0f;
    const int frameCount = (timing.frameCount > 1) ? timing.frameCount : 1;
    int frameIndex = 1 + (int)((fmodf(g_creationAnimTime, duration) / duration) *
                              (float)(frameCount - 1));
    frameIndex = std::max(1, std::min(frameIndex, frameCount));

    g_pZoneModel->RestoreBindPose(g_pDevice);
    for (noesisModel_t::Submesh &sm : g_pZoneModel->submeshes)
    {
        const bool isHead = strncmp(sm.materialName.c_str(), "creation_mat_1", 14) == 0;
        const CreationSqleMotionInfo &motion = isHead ? g_creationHeadMotion : g_creationBodyMotion;
        ApplySqleFrameChannelToSubmesh(sm, motion, frameIndex);
    }
}

static void UpdateCameraMovement(float dt)
{
    if (IsGameMode())
        UpdatePlayerMovement(dt);
    else
        UpdateFlyCameraMovement(dt);
}

static void SetCameraCursorHidden(bool hidden)
{
    if (hidden && g_enableHardwareMouseCursor)
        hidden = false;

    if (g_cursorHidden == hidden)
        return;

    g_cursorHidden = hidden;
    if (hidden)
    {
        while (ShowCursor(FALSE) >= 0) {}
    }
    else
    {
        while (ShowCursor(TRUE) < 0) {}
    }
}

static void EndMouseLook()
{
    if (g_mouseDown || g_mousePanDown)
    {
        g_mouseDown = false;
        g_mousePanDown = false;
        ReleaseCapture();
    }
    SetCameraCursorHidden(false);
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

static void ApplyD3D8StyleModelRenderState()
{
    g_pDevice->SetVertexShader(nullptr);
    g_pDevice->SetPixelShader(nullptr);
    g_pDevice->SetTexture(1, nullptr);

    // Keep the D3D9 renderer in the same fixed-function lane as FFXI's D3D8 client.
    g_pDevice->SetRenderState(D3DRS_COLORVERTEX, TRUE);
    g_pDevice->SetRenderState(D3DRS_NORMALIZENORMALS, TRUE);
    g_pDevice->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
    g_pDevice->SetRenderState(D3DRS_DITHERENABLE, TRUE);
    g_pDevice->SetRenderState(D3DRS_FOGENABLE, FALSE);
    g_pDevice->SetRenderState(D3DRS_RANGEFOGENABLE, FALSE);
    g_pDevice->SetRenderState(D3DRS_CLIPPING, TRUE);
    g_pDevice->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
    g_pDevice->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_COLOR1);
    g_pDevice->SetRenderState(D3DRS_SPECULARMATERIALSOURCE, D3DMCS_MATERIAL);
    g_pDevice->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);

    g_pDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
    g_pDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    g_pDevice->SetTextureStageState(0, D3DTSS_RESULTARG, D3DTA_CURRENT);
    g_pDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    g_pDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    g_pDevice->SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1);
    g_pDevice->SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);

    g_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    g_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
    g_pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    g_pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    g_pDevice->SetSamplerState(0, D3DSAMP_MIPFILTER,
        g_enableMipMapping ? D3DTEXF_LINEAR : D3DTEXF_NONE);
}

static void SetTextureStageForOptionalTexture(IDirect3DTexture9 *pTex, D3DTEXTUREOP alphaOp)
{
    g_pDevice->SetTexture(0, pTex);
    if (pTex)
    {
        g_pDevice->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_MODULATE2X);
        g_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        g_pDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        g_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP,   alphaOp);
        g_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        g_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    }
    else
    {
        g_pDevice->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
        g_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
        g_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
        g_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    }
}

static DWORD D3DRenderStateFloat(float value)
{
    DWORD bits = 0;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

// Native DXT textures are sampled by hardware. FFXI's authored DXT3 alpha uses
// the upper nibble as a 0..8 range, so expand that encoding before combining it
// with vertex alpha. RGBA atlases (including high-poly DMB characters) already
// carry full-range opacity and must pass through unchanged. RGB intentionally
// remains the original fixed-function 2x texture * vertex-color equation.
static bool EnsureFfxiTexturePixelShader()
{
    if (g_pFfxiTexturePixelShader)
        return true;
    if (g_ffxiTexturePixelShaderTried || !g_pDevice)
        return false;

    g_ffxiTexturePixelShaderTried = true;
    static const char kShaderSource[] =
        "sampler2D BaseTexture : register(s0);\n"
        "float4 AlphaParams : register(c0);\n"
        "float4 main(float4 diffuse : COLOR0, float2 uv : TEXCOORD0) : COLOR0\n"
        "{\n"
        "    float4 texel = tex2D(BaseTexture, uv);\n"
		"    float textureAlpha = (AlphaParams.y > 0.5) ? saturate(texel.a * 1.875) : texel.a;\n"
        "    float vertexAlpha = saturate(diffuse.a * 2.0);\n"
        "    float alpha = (AlphaParams.x > 0.5) ? textureAlpha * vertexAlpha : 1.0;\n"
        "    return float4(saturate(2.0 * texel.rgb * diffuse.rgb), alpha);\n"
        "}\n";

    ID3DBlob *pByteCode = nullptr;
    ID3DBlob *pErrors = nullptr;
    const HRESULT compileHr = D3DCompile(kShaderSource, sizeof(kShaderSource) - 1,
        "DATuraFfxiTexture", nullptr, nullptr, "main", "ps_2_0",
        D3DCOMPILE_ENABLE_STRICTNESS, 0, &pByteCode, &pErrors);
    if (FAILED(compileHr))
    {
        if (pErrors)
            OutputDebugStringA((const char *)pErrors->GetBufferPointer());
        if (pErrors)
            pErrors->Release();
        return false;
    }

    const HRESULT shaderHr = g_pDevice->CreatePixelShader(
        (const DWORD *)pByteCode->GetBufferPointer(), &g_pFfxiTexturePixelShader);
    pByteCode->Release();
    if (pErrors)
        pErrors->Release();
    if (FAILED(shaderHr))
    {
        g_pFfxiTexturePixelShader = nullptr;
        OutputDebugStringA("WARNING: Unable to create FFXI DXT texture pixel shader.\n");
        return false;
    }
    return true;
}

static bool SetFfxiTexturePixelShader(bool useAuthoredAlpha, bool expandDxt3Alpha)
{
    if (!EnsureFfxiTexturePixelShader())
    {
        g_pDevice->SetPixelShader(nullptr);
        return false;
    }

	const float alphaParams[4] =
	{
		useAuthoredAlpha ? 1.0f : 0.0f,
		expandDxt3Alpha ? 1.0f : 0.0f,
		0.0f,
		0.0f
	};
    g_pDevice->SetPixelShader(g_pFfxiTexturePixelShader);
    g_pDevice->SetPixelShaderConstantF(0, alphaParams, 1);
    return true;
}

static bool EnsureFfxiUiPixelShader()
{
    if (g_pFfxiUiPixelShader)
        return true;
    if (g_ffxiUiPixelShaderTried || !g_pDevice)
        return false;

    g_ffxiUiPixelShaderTried = true;
    static const char kShaderSource[] =
        "sampler2D BaseTexture : register(s0);\n"
        "float4 AlphaParams : register(c0);\n"
        "float4 main(float4 diffuse : COLOR0, float2 uv : TEXCOORD0) : COLOR0\n"
        "{\n"
        "    float4 texel = tex2D(BaseTexture, uv);\n"
        "    texel.a = min(saturate(texel.a * AlphaParams.x), AlphaParams.y);\n"
        "    return texel * diffuse;\n"
        "}\n";

    ID3DBlob *pByteCode = nullptr;
    ID3DBlob *pErrors = nullptr;
    const HRESULT compileHr = D3DCompile(kShaderSource, sizeof(kShaderSource) - 1,
        "DATuraFfxiUi", nullptr, nullptr, "main", "ps_2_0",
        D3DCOMPILE_ENABLE_STRICTNESS, 0, &pByteCode, &pErrors);
    if (FAILED(compileHr))
    {
        if (pErrors)
            OutputDebugStringA((const char *)pErrors->GetBufferPointer());
        if (pErrors)
            pErrors->Release();
        return false;
    }

    const HRESULT shaderHr = g_pDevice->CreatePixelShader(
        (const DWORD *)pByteCode->GetBufferPointer(), &g_pFfxiUiPixelShader);
    pByteCode->Release();
    if (pErrors)
        pErrors->Release();
    if (FAILED(shaderHr))
    {
        g_pFfxiUiPixelShader = nullptr;
        OutputDebugStringA("WARNING: Unable to create FFXI UI pixel shader.\n");
        return false;
    }
    return true;
}

static bool SetFfxiUiPixelShader(bool expandDxt3Alpha, float alphaScale = 1.0f,
                                 float maxOpacity = 1.0f)
{
    if (!EnsureFfxiUiPixelShader())
    {
        g_pDevice->SetPixelShader(nullptr);
        return false;
    }

    const float alphaParams[4] =
    {
        alphaScale * (expandDxt3Alpha ? 1.875f : 1.0f),
        maxOpacity,
        0.0f,
        0.0f
    };
    g_pDevice->SetPixelShader(g_pFfxiUiPixelShader);
    g_pDevice->SetPixelShaderConstantF(0, alphaParams, 1);
    return true;
}

static bool IsZoneObjectVisible(const std::string &objectName)
{
    if (objectName.empty())
        return true;
    return std::find(g_hiddenZoneObjects.begin(), g_hiddenZoneObjects.end(), objectName) == g_hiddenZoneObjects.end();
}

static void ApplyZoneObjectWorldIfNeeded(const std::string &objectName, bool allowObjectOverrides,
                                         const D3DMATRIX &baseWorld)
{
    if (allowObjectOverrides && !objectName.empty())
    {
        std::map<std::string, ZoneDebugTransform>::const_iterator it = g_zoneObjectOverrides.find(objectName);
        if (it != g_zoneObjectOverrides.end())
        {
            D3DMATRIX world = BuildZoneDebugMatrix(it->second);
            g_pDevice->SetTransform(D3DTS_WORLD, &world);
            return;
        }
    }

    g_pDevice->SetTransform(D3DTS_WORLD, &baseWorld);
}

static void RenderModel(noesisModel_t *pModel, bool allowObjectOverrides = false)
{
    if (!pModel || !g_pDevice) return;

    g_pDevice->SetFVF(FFXI_VERTEX_FVF);
    D3DMATRIX baseWorld = {};
    g_pDevice->GetTransform(D3DTS_WORLD, &baseWorld);
    ApplyD3D8StyleModelRenderState();

    // Baseline render states — no fixed-function lighting, depth test on
    g_pDevice->SetRenderState(D3DRS_LIGHTING,        FALSE);
    g_pDevice->SetRenderState(D3DRS_ZENABLE,         TRUE);
    g_pDevice->SetRenderState(D3DRS_ZWRITEENABLE,    TRUE);
    g_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
    g_pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);

    // Texture sampler — bilinear
    const noesisMatData_t *pMD = pModel->pMatData;

    for (size_t i = 0; i < pModel->submeshes.size(); ++i)
    {
        noesisModel_t::Submesh &sm = pModel->submeshes[i];
        if (!sm.pVB || !sm.pIB || sm.triCount == 0) continue;
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

        ApplyZoneObjectWorldIfNeeded(sm.objectName, allowObjectOverrides, baseWorld);

        // Per-submesh render states (opaque pass only — softBlend meshes are skipped above)
        // The runtime world transform reverses FFXI's original winding before
        // D3D sees it. In DATura's rendered coordinate space, exterior faces
        // are counter-clockwise, so cull clockwise triangles for a back-face
        // cull request.
        g_pDevice->SetRenderState(D3DRS_CULLMODE,
            twoSided ? D3DCULL_NONE : D3DCULL_CW);
        g_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        g_pDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
        g_pDevice->SetRenderState(D3DRS_DEPTHBIAS, 0);
		g_pDevice->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);

        if (alphaRef > 0.0f)
        {
            g_pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
            g_pDevice->SetRenderState(D3DRS_ALPHAREF,  (DWORD)(alphaRef * 255.0f));
            g_pDevice->SetRenderState(D3DRS_ALPHAFUNC,  D3DCMP_GREATER);
        }
        else
        {
            g_pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        }

		const bool shaderActive = pTex && SetFfxiTexturePixelShader(
			alphaRef > 0.0f, expandDxt3Alpha);
        SetTextureStageForOptionalTexture(pTex,
			shaderActive ? D3DTOP_SELECTARG1 :
            (alphaRef > 0.0f ? D3DTOP_MODULATE4X : D3DTOP_MODULATE2X));
        g_pDevice->SetStreamSource(0, sm.pVB, 0, (UINT)sizeof(FFXIVertex));
        g_pDevice->SetIndices(sm.pIB);
        g_pDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0,
                                         (UINT)sm.vertCount, 0, (UINT)sm.triCount);
    }

    struct SoftBlendSubmesh
    {
        size_t index;
        float distanceSq;
    };
    std::vector<SoftBlendSubmesh> softBlendSubmeshes;
    const float cameraX = g_camTarget[0] + g_camDist * sinf(g_camYaw) * cosf(g_camPitch);
    const float cameraY = g_camTarget[1] + g_camDist * sinf(g_camPitch);
    const float cameraZ = g_camTarget[2] + g_camDist * cosf(g_camYaw) * cosf(g_camPitch);

    // FFXI's 0x8000 zone layers do not write depth. Keep them in the same
    // transparent pass, but draw the farthest surfaces first so overlapping
    // crag detail layers compose instead of depending on DAT record order.
    for (size_t i = 0; i < pModel->submeshes.size(); ++i)
    {
        const noesisModel_t::Submesh &sm = pModel->submeshes[i];
        if (!sm.pVB || !sm.pIB || sm.triCount == 0) continue;
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

        ApplyZoneObjectWorldIfNeeded(sm.objectName, allowObjectOverrides, baseWorld);

        g_pDevice->SetRenderState(D3DRS_CULLMODE,
            twoSided ? D3DCULL_NONE : D3DCULL_CW);
        g_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        g_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        g_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        g_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
		g_pDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
		// 0x8000 is true source-alpha blending. Its zero-alpha texels naturally
		// contribute nothing; alpha testing here would create a second, incorrect
		// cutout rule on textured rock and decal layers.
        g_pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		// Blended zone layers only need a small coplanar nudge. A large slope-scale
		// bias lets terrain decals win depth tests against nearby 3D structures.
		g_pDevice->SetRenderState(D3DRS_DEPTHBIAS, D3DRenderStateFloat(-0.000001f));
		g_pDevice->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);

		const bool shaderActive = pTex && SetFfxiTexturePixelShader(true, expandDxt3Alpha);
		SetTextureStageForOptionalTexture(pTex, shaderActive ? D3DTOP_SELECTARG1 : D3DTOP_MODULATE4X);
        g_pDevice->SetStreamSource(0, sm.pVB, 0, (UINT)sizeof(FFXIVertex));
        g_pDevice->SetIndices(sm.pIB);
        g_pDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0,
                                         (UINT)sm.vertCount, 0, (UINT)sm.triCount);
    }

    // Clean up texture binding
    g_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    g_pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    g_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
	g_pDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    g_pDevice->SetRenderState(D3DRS_DEPTHBIAS, 0);
	g_pDevice->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);
    g_pDevice->SetTexture(0, nullptr);
	g_pDevice->SetPixelShader(nullptr);
    g_pDevice->SetTransform(D3DTS_WORLD, &baseWorld);
    g_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE2X);
    g_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE2X);
    g_pDevice->SetStreamSource(0, nullptr, 0, 0);
    g_pDevice->SetIndices(nullptr);
}

static void RenderHighlightedZoneObject(noesisModel_t *pModel)
{
    if (!pModel || !g_pDevice || g_highlightedZoneObject.empty())
        return;

    D3DMATRIX baseWorld = {};
    g_pDevice->GetTransform(D3DTS_WORLD, &baseWorld);
    g_pDevice->SetFVF(FFXI_VERTEX_FVF);
    g_pDevice->SetTexture(0, nullptr);
    g_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    g_pDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
    g_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    g_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    g_pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    g_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    g_pDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
    g_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    g_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
    g_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    g_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
    g_pDevice->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_XRGB(255, 128, 0));
    g_pDevice->SetRenderState(D3DRS_DEPTHBIAS, 0xBA83126F);

    for (size_t i = 0; i < pModel->submeshes.size(); ++i)
    {
        noesisModel_t::Submesh &sm = pModel->submeshes[i];
        if (sm.objectName != g_highlightedZoneObject || !sm.pVB || !sm.pIB || sm.triCount == 0)
            continue;
        ApplyZoneObjectWorldIfNeeded(sm.objectName, true, baseWorld);
        g_pDevice->SetStreamSource(0, sm.pVB, 0, (UINT)sizeof(FFXIVertex));
        g_pDevice->SetIndices(sm.pIB);
        g_pDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0,
                                         (UINT)sm.vertCount, 0, (UINT)sm.triCount);
    }

    g_pDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    g_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    g_pDevice->SetRenderState(D3DRS_DEPTHBIAS, 0);
    g_pDevice->SetTransform(D3DTS_WORLD, &baseWorld);
    g_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    g_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    g_pDevice->SetStreamSource(0, nullptr, 0, 0);
    g_pDevice->SetIndices(nullptr);
}

struct CollisionOverlayVertex
{
    float x, y, z;
    DWORD color;
};

#define COLLISION_OVERLAY_FVF (D3DFVF_XYZ | D3DFVF_DIFFUSE)

static void DrawCollisionGeometryOverlay()
{
    if (!g_showCollisionGeometry || !g_pDevice || g_zoneCollisionTris.empty())
        return;

    std::vector<CollisionOverlayVertex> verts;
    verts.reserve(g_zoneCollisionTris.size() * 3);
    const DWORD color = D3DCOLOR_ARGB(88, 255, 128, 24);
    for (size_t i = 0; i < g_zoneCollisionTris.size(); ++i)
    {
        const ZoneCollisionTriangle &tri = g_zoneCollisionTris[i];
        for (int v = 0; v < 3; ++v)
        {
            CollisionOverlayVertex out = {};
            out.x = tri.p[v][0];
            out.y = tri.p[v][1];
            out.z = tri.p[v][2];
            out.color = color;
            verts.push_back(out);
        }
    }

    if (verts.empty())
        return;

    D3DMATRIX world = BuildIdentity();
    g_pDevice->SetTransform(D3DTS_WORLD, &world);
    g_pDevice->SetFVF(COLLISION_OVERLAY_FVF);
    g_pDevice->SetTexture(0, nullptr);
    g_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    g_pDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
    g_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    g_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    g_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    g_pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    g_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    g_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    g_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    g_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    g_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    g_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, (UINT)g_zoneCollisionTris.size(),
                               &verts[0], sizeof(CollisionOverlayVertex));
    g_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    g_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    g_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    g_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
}

struct NpcMarkerVertex
{
    float x, y, z;
    DWORD color;
};

#define NPC_MARKER_FVF (D3DFVF_XYZ | D3DFVF_DIFFUSE)

static void DrawNpcPlacementMarkers()
{
    if (!g_pDevice || g_npcPlacementMarkers.empty())
        return;

    std::vector<NpcMarkerVertex> verts;
    verts.reserve(g_npcPlacementMarkers.size() * 6);

    for (size_t i = 0; i < g_npcPlacementMarkers.size(); ++i)
    {
        const NpcPlacementMarker &marker = g_npcPlacementMarkers[i];
        const float x = marker.pos[0];
        const float y = marker.pos[1];
        const float z = marker.pos[2];
        const float r = 1.15f;
        const float h = 3.0f;
        const DWORD color = (marker.confidence >= 105) ?
            D3DCOLOR_ARGB(230, 64, 255, 120) :
            D3DCOLOR_ARGB(220, 255, 225, 64);

        verts.push_back({ x - r, y + 0.05f, z, color });
        verts.push_back({ x + r, y + 0.05f, z, color });
        verts.push_back({ x, y + 0.05f, z - r, color });
        verts.push_back({ x, y + 0.05f, z + r, color });
        verts.push_back({ x, y, z, color });
        verts.push_back({ x, y - h, z, color });
    }

    if (verts.empty())
        return;

    D3DMATRIX world = BuildIdentity();
    g_pDevice->SetTransform(D3DTS_WORLD, &world);
    g_pDevice->SetFVF(NPC_MARKER_FVF);
    g_pDevice->SetTexture(0, nullptr);
    g_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    g_pDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
    g_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    g_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    g_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    g_pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    g_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    g_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    g_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    g_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    g_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    g_pDevice->DrawPrimitiveUP(D3DPT_LINELIST, (UINT)(verts.size() / 2),
                               &verts[0], sizeof(NpcMarkerVertex));
    g_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    g_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    g_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    g_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
}

//========================================================================================
// Model loading
//========================================================================================

static void UnloadZoneModel()
{
    ClearZoneCollision();
    g_npcPlacementMarkers.clear();
    if (g_pZoneModel)
    {
        g_pZoneModel->ReleaseD3DBuffers();
        g_pZoneModel = nullptr;
    }
    if (g_pZoneRapi)
    {
        delete g_pZoneRapi;
        g_pZoneRapi = nullptr;
    }
}

static void UnloadPlayerModel()
{
    if (g_pPlayerModel)
    {
        g_pPlayerModel->ReleaseD3DBuffers();
        g_pPlayerModel = nullptr;
    }
    if (g_pPlayerRapi)
    {
        delete g_pPlayerRapi;
        g_pPlayerRapi = nullptr;
    }
    g_playerGroundOffset = 0.0f;
    g_playerAnimTime = 0.0f;
}

static void UnloadTitleAssets()
{
    if (g_pTitleLogoModel)
    {
        g_pTitleLogoModel->ReleaseD3DBuffers();
        g_pTitleLogoModel = nullptr;
    }
    if (g_pTitleLogoRapi)
    {
        delete g_pTitleLogoRapi;
        g_pTitleLogoRapi = nullptr;
    }
    if (g_pTitleLogoMarkModel)
    {
        g_pTitleLogoMarkModel->ReleaseD3DBuffers();
        g_pTitleLogoMarkModel = nullptr;
    }
    if (g_pTitleLogoMarkRapi)
    {
        delete g_pTitleLogoMarkRapi;
        g_pTitleLogoMarkRapi = nullptr;
    }
    if (g_pTitleUiModel)
    {
        g_pTitleUiModel->ReleaseD3DBuffers();
        g_pTitleUiModel = nullptr;
    }
    if (g_pTitleUiRapi)
    {
        delete g_pTitleUiRapi;
        g_pTitleUiRapi = nullptr;
    }
}

static bool ReadWholeFile(const char *path, BYTE **ppBuf, DWORD *pFileSize)
{
    if (!path || !ppBuf || !pFileSize)
        return false;

    *ppBuf = nullptr;
    *pFileSize = 0;

    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ,
                                nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    DWORD fileSize = GetFileSize(hFile, nullptr);
    if (fileSize == 0 || fileSize == INVALID_FILE_SIZE)
    {
        CloseHandle(hFile);
        return false;
    }

    BYTE *pBuf = new BYTE[fileSize];
    DWORD bytesRead = 0;
    const BOOL readOk = ReadFile(hFile, pBuf, fileSize, &bytesRead, nullptr);
    CloseHandle(hFile);

    if (!readOk || bytesRead != fileSize)
    {
        delete[] pBuf;
        return false;
    }

    *ppBuf = pBuf;
    *pFileSize = fileSize;
    return true;
}

static noesisModel_t *LoadTextureDat(const char *relativePath, noeRAPI_t **ppRapi)
{
    if (!relativePath || !ppRapi)
        return nullptr;

    char fullPath[MAX_PATH] = {};
    sprintf_s(fullPath, "%s%s", g_ffxiPath, relativePath);

    BYTE *pBuf = nullptr;
    DWORD fileSize = 0;
    if (!ReadWholeFile(fullPath, &pBuf, &fileSize))
        return nullptr;

    noeRAPI_t *pRapi = new noeRAPI_t(g_pDevice);
    ConfigureRapiTextureSettings(pRapi);
    pRapi->SetCurrentFilePath(fullPath);

    if (!Model_FF11_CheckDAT(pBuf, (int)fileSize, pRapi))
    {
        delete[] pBuf;
        delete pRapi;
        return nullptr;
    }

    int numMdl = 0;
    noesisModel_t *pMdl = Model_FF11_LoadDAT(pBuf, (int)fileSize, numMdl, pRapi);
    delete[] pBuf;

    if (!pMdl || !pMdl->pMatData || pMdl->pMatData->texCount <= 0)
    {
        delete pRapi;
        return nullptr;
    }

    *ppRapi = pRapi;
    return pMdl;
}

static void HideUnreferencedZoneObjectsByDefault()
{
    const int mapObjectCount = Model_FF11_GetLastMapObjectCount();
    for (int i = 0; i < mapObjectCount; ++i)
    {
        const char *displayName = Model_FF11_GetLastMapObjectDisplayName(i);
        if (displayName && strncmp(displayName, "env:", 4) == 0)
            SetZoneObjectHidden(displayName, true);
    }
}

static void LoadDatFile(const char *path, bool userContentLoad, bool renderEnvironment,
                        bool renderUnreferenced)
{
    RememberLoadedZoneContext(path, nullptr, userContentLoad, renderEnvironment, renderUnreferenced);

    if (userContentLoad)
    {
        g_titleScreenActive = false;
        g_highPolyCreationActive = false;
        g_nationSelectActive = false;
        g_gameModeMusicId = 0;
        UnloadTitleAssets();
        BGM_Stop();
    }

    UnloadZoneModel();
    g_hiddenZoneObjects.clear();
    g_zoneObjectOverrides.clear();
    gFF11LastMapObjects.clear();
    gFF11LastMapGeoDrawBatches.clear();
    gFF11LastDatChunks.clear();
    memset(&gFF11LastMapHeader, 0, sizeof(gFF11LastMapHeader));
    gFF11LastCollisionTriangles.clear();
    gFF11LastCollisionMeshes.clear();
    SetLoadedZoneLabelFromPath(path);
    RefreshZoneObjectPanel();

    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ,
                                nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        MessageBoxA(g_hWnd, "Could not open file.", "Load Error", MB_OK | MB_ICONERROR);
        return;
    }

    DWORD fileSize = GetFileSize(hFile, nullptr);
    if (fileSize == 0 || fileSize == INVALID_FILE_SIZE)
    {
        CloseHandle(hFile);
        MessageBoxA(g_hWnd, "File is empty or unreadable.", "Load Error", MB_OK | MB_ICONERROR);
        return;
    }

    BYTE *pBuf = new BYTE[fileSize];
    DWORD bytesRead = 0;
    const BOOL readOk = ReadFile(hFile, pBuf, fileSize, &bytesRead, nullptr);
    CloseHandle(hFile);

    if (!readOk || bytesRead != fileSize)
    {
        delete[] pBuf;
        MessageBoxA(g_hWnd, "Read error — incomplete file.", "Load Error", MB_OK | MB_ICONERROR);
        return;
    }

    g_pZoneRapi = new noeRAPI_t(g_pDevice);
    ConfigureRapiTextureSettings(g_pZoneRapi);
    g_pZoneRapi->SetCurrentFilePath(path);

    if (Model_FF11_CheckCreationDAT(pBuf, (int)fileSize, g_pZoneRapi))
    {
        g_highPolyCreationActive = true;
        int numMdl = 0;
        noesisModel_t *pMdl = Model_FF11_LoadCreationDAT(pBuf, (int)fileSize, numMdl, g_pZoneRapi);
        delete[] pBuf;

        if (!pMdl || numMdl == 0)
        {
            delete g_pZoneRapi; g_pZoneRapi = nullptr;
            MessageBoxA(g_hWnd, "Creation DAT loaded but contained no displayable geometry.", "Load", MB_OK | MB_ICONWARNING);
            return;
        }

        g_pZoneModel = pMdl;
        g_camDist = 25.0f;
        g_camYaw = 0.0f;
        g_camPitch = 0.15f;
        g_camTarget[0] = g_camTarget[2] = 0.0f;
        g_camTarget[1] = -12.0f;
        return;
    }

    if (fileSize >= 4 && memcmp(pBuf, "DMB\0", 4) == 0)
    {
        delete[] pBuf;
        delete g_pZoneRapi; g_pZoneRapi = nullptr;
        MessageBoxA(g_hWnd,
            "This is the material/texture half of a high-poly character creation pair.\n\n"
            "Choose the matching mesh DAT next to it in the menu. DMB texture binding is not implemented yet.",
            "Creation Material DAT", MB_OK | MB_ICONINFORMATION);
        return;
    }

    if (fileSize >= 4 && memcmp(pBuf, "SQLE", 4) == 0)
    {
        delete[] pBuf;
        delete g_pZoneRapi; g_pZoneRapi = nullptr;
        MessageBoxA(g_hWnd,
            "This is an SQLE animation file for high-poly character creation models.\n\n"
            "DATura can identify it now, but SQLE animation loading is not implemented yet.",
            "SQLE Loader Pending", MB_OK | MB_ICONINFORMATION);
        return;
    }

    if (!Model_FF11_CheckDAT(pBuf, (int)fileSize, g_pZoneRapi))
    {
        delete[] pBuf;
        delete g_pZoneRapi; g_pZoneRapi = nullptr;
        MessageBoxA(g_hWnd, "Not a recognised FFXI DAT file.", "Load Error", MB_OK | MB_ICONERROR);
        return;
    }

    ff11Opts_t loadOpts = {};
    ff11Opts_t *pPrevOpts = gpFF11Opts;
    if (renderEnvironment || renderUnreferenced || userContentLoad)
    {
        loadOpts.renderUnreferenced = renderUnreferenced || userContentLoad;
        loadOpts.renderEnvironment = renderEnvironment || userContentLoad;
        loadOpts.renderEffectMeshes = g_enableEnvironmentalAnimation;
        loadOpts.collectCollision = userContentLoad;
        loadOpts.collectCollisionUnreferenced = userContentLoad;
        gpFF11Opts = &loadOpts;
    }

    int numMdl = 0;
    noesisModel_t *pMdl = Model_FF11_LoadDAT(pBuf, (int)fileSize, numMdl, g_pZoneRapi);
    gpFF11Opts = pPrevOpts;
    delete[] pBuf;

    if (!pMdl || numMdl == 0)
    {
        delete g_pZoneRapi; g_pZoneRapi = nullptr;
        MessageBoxA(g_hWnd, "DAT loaded but contained no displayable geometry.", "Load", MB_OK | MB_ICONWARNING);
        return;
    }

    g_pZoneModel  = pMdl;
    MirrorZoneModelOnX(g_pZoneModel);
    BuildZoneCollisionFromDAT();
    LoadNpcPlacementMarkersForZonePath(path);
    HideUnreferencedZoneObjectsByDefault();
    RefreshZoneObjectPanel();
    ResetZoneCameraFromCollision();
    if (userContentLoad)
        SetInteractionMode(kDATuraMode_Edit);
}

static void LoadTitleScreen()
{
    UnloadTitleAssets();
    g_gameModeMusicId = 0;
    SetInteractionMode(kDATuraMode_Edit);
    g_highPolyCreationActive = false;
    g_nationSelectActive = false;

    char fullPath[MAX_PATH] = {};
    sprintf_s(fullPath, "%s%s", g_ffxiPath, kTitleScreenZoneDat);

    DWORD attrs = GetFileAttributesA(fullPath);
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY))
    {
        g_titleScreenActive = true;
        return;
    }

    LoadDatFile(fullPath, false, true, true);
    g_titleScreenActive = true;

    g_camYaw = 0.72f;
    g_camPitch = 0.34f;
    // Preserve the title-screen framing while keeping the collision-aware
    // target chosen by LoadDatFile. Never begin the backdrop camera underground.
    if (g_camDist < 180.0f)
        g_camDist = 180.0f;

    g_pTitleLogoModel = LoadTextureDat("ROM6/0/96.DAT", &g_pTitleLogoRapi);
    g_pTitleLogoMarkModel = LoadTextureDat("ROM/119/50.DAT", &g_pTitleLogoMarkRapi);
    g_pTitleUiModel = LoadTextureDat("ROM/119/51.DAT", &g_pTitleUiRapi);

    SyncAppMusic();
}

static void LoadCreationEntry(const FFXICreationEntry *pEntry)
{
    if (!pEntry)
        return;

    SetInteractionMode(kDATuraMode_Edit);
    g_titleScreenActive = false;
    g_highPolyCreationActive = true;
    g_nationSelectActive = false;
    g_gameModeMusicId = 0;
    UnloadTitleAssets();
    BGM_Stop();
    UnloadZoneModel();

    BYTE *fileBuffers[2] = {};
    int bufferLens[2] = {};
    BYTE *materialBuffers[2] = {};
    int materialLens[2] = {};
    int materialAlphaModes[2] = {};
    float meshOffsets[2][3] = {};
    int fileCount = 0;
    int bodyFileIndex = -1;
    int headFileIndex = -1;
    char firstMeshPath[MAX_PATH] = {};

    char initialBodyMeshDat[32] = {};
    char initialBodyMaterialDat[32] = {};
    const char *bodyMeshDat = pEntry->bodyMeshDat;
    const char *bodyMaterialDat = pEntry->bodyMaterialDat;
    if (pEntry->label && strncmp(pEntry->label, "Initial Equipment", 17) == 0 &&
        pEntry->bodyMeshDat && pEntry->bodyMaterialDat)
    {
        int meshRom = 0, meshDat = 0;
        int matRom = 0, matDat = 0;
        if (sscanf_s(pEntry->bodyMeshDat, "ROM/%i/%i.dat", &meshRom, &meshDat) == 2 &&
            sscanf_s(pEntry->bodyMaterialDat, "ROM/%i/%i.dat", &matRom, &matDat) == 2)
        {
            sprintf_s(initialBodyMeshDat, "ROM/%i/%i.dat", meshRom, meshDat + 2);
            sprintf_s(initialBodyMaterialDat, "ROM/%i/%i.dat", matRom, matDat + 2);
            bodyMeshDat = initialBodyMeshDat;
            bodyMaterialDat = initialBodyMaterialDat;
        }
    }

    const char *meshPaths[2] = { bodyMeshDat, pEntry->headMeshDat };
    const char *materialPaths[2] = { bodyMaterialDat, pEntry->headMaterialDat };
    for (int meshIndex = 0; meshIndex < 2; ++meshIndex)
    {
        if (!meshPaths[meshIndex])
            continue;

        char fullPath[MAX_PATH];
        sprintf_s(fullPath, "%s%s", g_ffxiPath, meshPaths[meshIndex]);

        BYTE *pBuf = nullptr;
        DWORD fileSize = 0;
        if (!ReadWholeFile(fullPath, &pBuf, &fileSize))
        {
            for (int i = 0; i < fileCount; ++i)
            {
                delete[] fileBuffers[i];
                delete[] materialBuffers[i];
            }
            MessageBoxA(g_hWnd, "Could not open one of the character creation mesh DATs.",
                        "Creation Load", MB_OK | MB_ICONERROR);
            return;
        }

        if (firstMeshPath[0] == '\0')
            strcpy_s(firstMeshPath, fullPath);

        fileBuffers[fileCount] = pBuf;
        bufferLens[fileCount] = (int)fileSize;
        if (meshIndex == 0)
            bodyFileIndex = fileCount;
        else if (meshIndex == 1)
            headFileIndex = fileCount;

        if (materialPaths[meshIndex])
        {
            char materialPath[MAX_PATH];
            sprintf_s(materialPath, "%s%s", g_ffxiPath, materialPaths[meshIndex]);
            BYTE *pMaterialBuf = nullptr;
            DWORD materialSize = 0;
            if (ReadWholeFile(materialPath, &pMaterialBuf, &materialSize))
            {
                materialBuffers[fileCount] = pMaterialBuf;
                materialLens[fileCount] = (int)materialSize;
                if (meshIndex == 1)
                    materialAlphaModes[fileCount] = pEntry->headAlphaMode;
                else
                    materialAlphaModes[fileCount] = FFXI_CREATION_ALPHA_BODY_CUTOUT;
            }
        }

        if (meshIndex == 1 && pEntry->bodyMeshDat && pEntry->headMeshDat)
            meshOffsets[fileCount][1] = pEntry->headYOffset;

        ++fileCount;
    }

    if (fileCount == 0)
    {
        MessageBoxA(g_hWnd, "This character creation entry has no mesh DATs assigned.",
                    "Creation Load", MB_OK | MB_ICONWARNING);
        return;
    }

    g_pZoneRapi = new noeRAPI_t(g_pDevice);
    ConfigureRapiTextureSettings(g_pZoneRapi);
    g_pZoneRapi->SetCurrentFilePath(firstMeshPath[0] ? firstMeshPath : "DATura character creation model");

    if (bodyFileIndex >= 0 && headFileIndex >= 0)
    {
        char bodyAnimPath[MAX_PATH] = {};
        char headAnimPath[MAX_PATH] = {};
        sprintf_s(bodyAnimPath, "%s%s", g_ffxiPath, CreationBodyAnimDatForRace(g_creationRaceIndex));
        sprintf_s(headAnimPath, "%s%s", g_ffxiPath, CreationHeadAnimDatForRace(g_creationRaceIndex));

        BYTE *bodyAnimBuffer = nullptr;
        BYTE *headAnimBuffer = nullptr;
        DWORD bodyAnimSize = 0;
        DWORD headAnimSize = 0;
        float bodyNeck[3] = {};
        float headNeck[3] = {};
        if (ReadWholeFile(bodyAnimPath, &bodyAnimBuffer, &bodyAnimSize) &&
            ReadWholeFile(headAnimPath, &headAnimBuffer, &headAnimSize) &&
            Model_FF11_GetDATBonePosition(bodyAnimBuffer, (int)bodyAnimSize, "bone0004", bodyNeck, g_pZoneRapi) &&
            Model_FF11_GetDATBonePosition(headAnimBuffer, (int)headAnimSize, "bone0001", headNeck, g_pZoneRapi))
        {
            meshOffsets[headFileIndex][0] = bodyNeck[0] - headNeck[0];
            meshOffsets[headFileIndex][1] = -(bodyNeck[1] - headNeck[1]);
            meshOffsets[headFileIndex][2] = bodyNeck[2] - headNeck[2];
        }
        delete[] bodyAnimBuffer;
        delete[] headAnimBuffer;
    }

    int numMdl = 0;
    noesisModel_t *pMdl = Model_FF11_LoadCreationDATList(fileBuffers, bufferLens,
        materialBuffers, materialLens, materialAlphaModes, &meshOffsets[0][0], fileCount, numMdl, g_pZoneRapi);

    for (int i = 0; i < fileCount; ++i)
    {
        delete[] fileBuffers[i];
        delete[] materialBuffers[i];
    }

    if (!pMdl || numMdl == 0)
    {
        delete g_pZoneRapi; g_pZoneRapi = nullptr;
        MessageBoxA(g_hWnd, "Creation DATs loaded but contained no displayable geometry.",
                    "Creation Load", MB_OK | MB_ICONWARNING);
        return;
    }

    g_pZoneModel = pMdl;
    g_creationBodyMotion = {};
    g_creationHeadMotion = {};
    g_creationAnimTime = 0.0f;
    if (g_creationAnimationIndex == 1)
    {
        ReadSqleMotionInfo(CreationPreviewIdleBodyDatForRace(g_creationRaceIndex), &g_creationBodyMotion);
        ReadSqleMotionInfo(CreationPreviewIdleHeadDatForRace(g_creationRaceIndex), &g_creationHeadMotion);
    }
    else if (g_creationAnimationIndex == 2)
    {
        ReadSqleMotionInfo(CreationPreviewWalkBodyDatForRace(g_creationRaceIndex), &g_creationBodyMotion);
        ReadSqleMotionInfo(CreationPreviewWalkHeadDatForRace(g_creationRaceIndex), &g_creationHeadMotion);
    }
    BuildHighPolyCreationAnimation(g_pZoneModel);
    g_camDist = 25.0f;
    g_camYaw = 0.0f;
    g_camPitch = 0.15f;
    g_camTarget[0] = g_camTarget[2] = 0.0f;
    g_camTarget[1] = -12.0f;
}

static void LoadDatSetFile(const char *path)
{
    g_titleScreenActive = false;
    g_highPolyCreationActive = false;
    g_nationSelectActive = false;
    g_gameModeMusicId = 0;
    UnloadTitleAssets();
    BGM_Stop();
    UnloadZoneModel();
    g_hiddenZoneObjects.clear();
    g_zoneObjectOverrides.clear();
    gFF11LastMapObjects.clear();
    gFF11LastMapGeoDrawBatches.clear();
    gFF11LastDatChunks.clear();
    memset(&gFF11LastMapHeader, 0, sizeof(gFF11LastMapHeader));
    gFF11LastCollisionTriangles.clear();
    gFF11LastCollisionMeshes.clear();
    SetLoadedZoneLabelFromPath(path);
    RefreshZoneObjectPanel();

    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ,
                                nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        MessageBoxA(g_hWnd, "Could not open file.", "Load Error", MB_OK | MB_ICONERROR);
        return;
    }

    DWORD fileSize = GetFileSize(hFile, nullptr);
    BYTE *pBuf = new BYTE[fileSize + 1]; // +1 for null terminator (text file)
    DWORD bytesRead = 0;
    const BOOL readOk = ReadFile(hFile, pBuf, fileSize, &bytesRead, nullptr);
    CloseHandle(hFile);
    pBuf[fileSize] = '\0';

    if (!readOk || bytesRead != fileSize)
    {
        delete[] pBuf;
        MessageBoxA(g_hWnd, "Read error.", "Load Error", MB_OK | MB_ICONERROR);
        return;
    }

    g_pZoneRapi = new noeRAPI_t(g_pDevice);
    ConfigureRapiTextureSettings(g_pZoneRapi);
    g_pZoneRapi->SetCurrentFilePath(path);

    if (!Model_FF11_CheckDATSet(pBuf, (int)fileSize, g_pZoneRapi))
    {
        delete[] pBuf;
        delete g_pZoneRapi; g_pZoneRapi = nullptr;
        MessageBoxA(g_hWnd, "Not a recognised FFXI DAT Set file.", "Load Error", MB_OK | MB_ICONERROR);
        return;
    }

    int numMdl = 0;
    noesisModel_t *pMdl = Model_FF11_LoadDATSet(pBuf, (int)fileSize, numMdl, g_pZoneRapi);
    delete[] pBuf;

    if (!pMdl || numMdl == 0)
    {
        delete g_pZoneRapi; g_pZoneRapi = nullptr;
        MessageBoxA(g_hWnd, "DAT Set loaded but contained no displayable geometry.", "Load", MB_OK | MB_ICONWARNING);
        return;
    }

    g_pZoneModel  = pMdl;
    g_camDist   = 5.0f;
    g_camYaw    = 0.0f;
    g_camPitch  = 0.25f;
    g_camTarget[0] = g_camTarget[1] = g_camTarget[2] = 0.0f;
}

static void AppendDatSetLine(char *out, size_t outSize, const char *name, const char *path)
{
    strcat_s(out, outSize, "dat \"");
    strcat_s(out, outSize, name);
    strcat_s(out, outSize, "\" \"");
    strcat_s(out, outSize, path);
    strcat_s(out, outSize, "\"\n");
}

static bool MakeOffsetDatPath(const char *basePath, int offset, char *outPath, size_t outPathSize)
{
    int rom = 0;
    int dat = 0;
    if (!basePath || sscanf_s(basePath, "ROM/%i/%i.dat", &rom, &dat) != 2)
        return false;

    dat += offset;
    while (dat > 127)
    {
        dat -= 128;
        ++rom;
    }
    while (dat < 0 && rom > 0)
    {
        dat += 128;
        --rom;
    }
    if (dat < 0)
        return false;

    sprintf_s(outPath, outPathSize, "ROM/%i/%i.dat", rom, dat);
    return true;
}

static void AppendVariantDatLine(char *out, size_t outSize, const char *name,
                                 const FFXICharRace &race, int entryIndex, int variantIndex)
{
    if (entryIndex < 0 || entryIndex >= race.count || variantIndex < 0)
        return;

    char path[64] = {};
    if (MakeOffsetDatPath(race.entries[entryIndex].dat, variantIndex, path, sizeof(path)))
        AppendDatSetLine(out, outSize, name, path);
}

static bool WriteTextFile(const char *path, const char *text)
{
    HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    DWORD written = 0;
    const BOOL ok = WriteFile(hFile, text, (DWORD)strlen(text), &written, nullptr);
    CloseHandle(hFile);
    return ok && written == strlen(text);
}

static void GetExecutableDirectory(char *outDir, size_t outDirSize)
{
    if (!outDir || outDirSize == 0)
        return;

    outDir[0] = '\0';
    char exePath[MAX_PATH] = {};
    GetModuleFileNameA(NULL, exePath, sizeof(exePath));
    strcpy_s(outDir, outDirSize, exePath);
    char *lastSlash = strrchr(outDir, '\\');
    if (!lastSlash)
        lastSlash = strrchr(outDir, '/');
    if (lastSlash)
        lastSlash[1] = '\0';
}

static void MakeSafeFileStem(const char *name, char *outStem, size_t outStemSize)
{
    if (!outStem || outStemSize == 0)
        return;

    const char *src = (name && name[0]) ? name : "Adventurer";
    size_t out = 0;
    for (int i = 0; src[i] && out < outStemSize - 1; ++i)
    {
        const unsigned char c = (unsigned char)src[i];
        if (isalnum(c) || c == '-' || c == '_')
        {
            outStem[out++] = (char)c;
        }
        else if ((c == ' ' || c == '.') && out > 0 && outStem[out - 1] != '_')
        {
            outStem[out++] = '_';
        }
    }

    while (out > 0 && outStem[out - 1] == '_')
        --out;
    if (out == 0)
        strcpy_s(outStem, outStemSize, "Adventurer");
    else
        outStem[out] = '\0';
}

static void ReplaceExtension(const char *path, const char *ext, char *outPath, size_t outPathSize)
{
    strcpy_s(outPath, outPathSize, path ? path : "");
    char *lastSlash = strrchr(outPath, '\\');
    char *lastSlash2 = strrchr(outPath, '/');
    if (!lastSlash || (lastSlash2 && lastSlash2 > lastSlash))
        lastSlash = lastSlash2;
    char *dot = strrchr(outPath, '.');
    if (!dot || (lastSlash && dot < lastSlash))
        dot = outPath + strlen(outPath);
    *dot = '\0';
    strcat_s(outPath, outPathSize, ext);
}

static void MakeDefaultCharacterSavePath(char *outPath, size_t outPathSize)
{
    char exeDir[MAX_PATH] = {};
    char stem[64] = {};
    GetExecutableDirectory(exeDir, sizeof(exeDir));
    MakeSafeFileStem(g_creationCharacterName, stem, sizeof(stem));
    sprintf_s(outPath, outPathSize, "%s%s.noesis", exeDir, stem);
}

static void GetCurrentCreationModelPaths(char *bodyMesh, size_t bodyMeshSize,
                                         char *headMesh, size_t headMeshSize)
{
    if (bodyMesh && bodyMeshSize > 0)
        bodyMesh[0] = '\0';
    if (headMesh && headMeshSize > 0)
        headMesh[0] = '\0';

    const FFXICreationEntry *pEntry = CurrentHighPolyCreationEntry();
    if (!pEntry)
        return;

    const char *bodyMeshDat = pEntry->bodyMeshDat;
    char initialBodyMeshDat[32] = {};
    if (g_creationEquipmentIndex == 1 && pEntry->bodyMeshDat)
    {
        int meshRom = 0, meshDat = 0;
        if (sscanf_s(pEntry->bodyMeshDat, "ROM/%i/%i.dat", &meshRom, &meshDat) == 2)
        {
            sprintf_s(initialBodyMeshDat, "ROM/%i/%i.dat", meshRom, meshDat + 2);
            bodyMeshDat = initialBodyMeshDat;
        }
    }

    if (bodyMesh && bodyMeshSize > 0 && bodyMeshDat)
        strcpy_s(bodyMesh, bodyMeshSize, bodyMeshDat);
    if (headMesh && headMeshSize > 0 && pEntry->headMeshDat)
        strcpy_s(headMesh, headMeshSize, pEntry->headMeshDat);
}

static void MakeRelativeSqleOption(const char *animDat, char *outOption, size_t outOptionSize)
{
    outOption[0] = '\0';
    int rom = 0, dat = 0;
    if (!animDat || sscanf_s(animDat, "ROM/%i/%i.dat", &rom, &dat) != 2)
        return;
    sprintf_s(outOption, outOptionSize, "../%i/%i.dat", rom, dat);
}

struct CreationSqlePair
{
    const char *bodyBase;
    const char *headBase;
    const char *bodyIdle;
    const char *headIdle;
    const char *bodyWalk;
    const char *headWalk;
};

static const CreationSqlePair kCreationSqlePairs[] =
{
    { "ROM/66/16.dat", "ROM/66/22.dat", "ROM/66/14.dat", "ROM/66/20.dat", "ROM/66/12.dat", "ROM/66/18.dat" }, // Hume Male, face 1
    { "ROM/65/86.dat", "ROM/65/92.dat", "ROM/65/84.dat", "ROM/65/90.dat", "ROM/65/82.dat", "ROM/65/88.dat" }, // Hume Female, face 1
    { "ROM/64/98.dat", "ROM/64/104.dat", "ROM/64/96.dat", "ROM/64/102.dat", "ROM/64/94.dat", "ROM/64/100.dat" }, // Elvaan Male, face 1
    { "ROM/64/40.dat", "ROM/64/46.dat", "ROM/64/38.dat", "ROM/64/44.dat", "ROM/64/36.dat", "ROM/64/42.dat" }, // Elvaan Female, face 1
    { "ROM/67/4.dat",  "ROM/67/64.dat", "ROM/67/2.dat",  "ROM/67/62.dat", "ROM/67/0.dat",  "ROM/67/60.dat" }, // Tarutaru Male, face 1
    { "ROM/67/4.dat",  "ROM/67/10.dat", "ROM/67/2.dat",  "ROM/67/8.dat",  "ROM/67/0.dat",  "ROM/67/6.dat"  }, // Tarutaru Female, face 1
    { "ROM/66/74.dat", "ROM/66/80.dat", "ROM/66/72.dat", "ROM/66/78.dat", "ROM/66/70.dat", "ROM/66/76.dat" }, // Mithra, face 1
    { "ROM/65/28.dat", "ROM/65/34.dat", "ROM/65/26.dat", "ROM/65/32.dat", "ROM/65/24.dat", "ROM/65/30.dat" }, // Galka, face 1
};

static const CreationSqlePair &CreationSqlePairForRace(int raceIndex)
{
    static const CreationSqlePair fallback =
        { "ROM/64/40.dat", "ROM/64/46.dat", "ROM/64/38.dat", "ROM/64/44.dat", "ROM/64/36.dat", "ROM/64/42.dat" };
    if (raceIndex < 0 || raceIndex >= (int)(sizeof(kCreationSqlePairs) / sizeof(kCreationSqlePairs[0])))
        return fallback;
    return kCreationSqlePairs[raceIndex];
}

struct CreationSqleHeadSet
{
    const char *meshDat;
    const char *baseDat;
    const char *idleDat;
    const char *walkDat;
};

static const CreationSqleHeadSet kCreationSqleHeadSets[] =
{
    { "ROM/63/5.dat",   "ROM/64/46.dat",  "ROM/64/44.dat",  "ROM/64/42.dat"  },
    { "ROM/63/9.dat",   "ROM/64/58.dat",  "ROM/64/56.dat",  "ROM/64/54.dat"  },
    { "ROM/63/13.dat",  "ROM/64/52.dat",  "ROM/64/50.dat",  "ROM/64/48.dat"  },
    { "ROM/63/17.dat",  "ROM/64/82.dat",  "ROM/64/80.dat",  "ROM/64/78.dat"  },
    { "ROM/63/25.dat",  "ROM/64/104.dat", "ROM/64/102.dat", "ROM/64/100.dat" },
    { "ROM/63/29.dat",  "ROM/64/116.dat", "ROM/64/114.dat", "ROM/64/112.dat" },
    { "ROM/63/33.dat",  "ROM/65/0.dat",   "ROM/64/126.dat", "ROM/64/124.dat" },
    { "ROM/63/37.dat",  "ROM/65/12.dat",  "ROM/65/10.dat",  "ROM/65/8.dat"   },
    { "ROM/63/45.dat",  "ROM/65/34.dat",  "ROM/65/32.dat",  "ROM/65/30.dat"  },
    { "ROM/63/49.dat",  "ROM/65/40.dat",  "ROM/65/38.dat",  "ROM/65/36.dat"  },
    { "ROM/63/53.dat",  "ROM/65/46.dat",  "ROM/65/44.dat",  "ROM/65/42.dat"  },
    { "ROM/63/57.dat",  "ROM/65/52.dat",  "ROM/65/50.dat",  "ROM/65/48.dat"  },
    { "ROM/63/65.dat",  "ROM/65/92.dat",  "ROM/65/90.dat",  "ROM/65/88.dat"  },
    { "ROM/63/69.dat",  "ROM/65/104.dat", "ROM/65/102.dat", "ROM/65/100.dat" },
    { "ROM/63/73.dat",  "ROM/65/116.dat", "ROM/65/114.dat", "ROM/65/112.dat" },
    { "ROM/63/77.dat",  "ROM/66/0.dat",   "ROM/65/126.dat", "ROM/65/124.dat" },
    { "ROM/63/85.dat",  "ROM/66/22.dat",  "ROM/66/20.dat",  "ROM/66/18.dat"  },
    { "ROM/63/89.dat",  "ROM/66/34.dat",  "ROM/66/32.dat",  "ROM/66/30.dat"  },
    { "ROM/63/93.dat",  "ROM/66/46.dat",  "ROM/66/44.dat",  "ROM/66/42.dat"  },
    { "ROM/63/97.dat",  "ROM/66/58.dat",  "ROM/66/56.dat",  "ROM/66/54.dat"  },
    { "ROM/63/105.dat", "ROM/66/80.dat",  "ROM/66/78.dat",  "ROM/66/76.dat"  },
    { "ROM/63/109.dat", "ROM/66/92.dat",  "ROM/66/90.dat",  "ROM/66/88.dat"  },
    { "ROM/63/113.dat", "ROM/66/104.dat", "ROM/66/102.dat", "ROM/66/100.dat" },
    { "ROM/63/117.dat", "ROM/66/116.dat", "ROM/66/114.dat", "ROM/66/112.dat" },
    { "ROM/63/125.dat", "ROM/67/10.dat",  "ROM/67/8.dat",   "ROM/67/6.dat"   },
    { "ROM/64/1.dat",   "ROM/67/22.dat",  "ROM/67/20.dat",  "ROM/67/18.dat"  },
    { "ROM/64/5.dat",   "ROM/67/34.dat",  "ROM/67/32.dat",  "ROM/67/30.dat"  },
    { "ROM/64/9.dat",   "ROM/67/46.dat",  "ROM/67/44.dat",  "ROM/67/42.dat"  },
    { "ROM/64/17.dat",  "ROM/67/64.dat",  "ROM/67/62.dat",  "ROM/67/60.dat"  },
    { "ROM/64/21.dat",  "ROM/67/76.dat",  "ROM/67/74.dat",  "ROM/67/72.dat"  },
    { "ROM/64/25.dat",  "ROM/67/88.dat",  "ROM/67/86.dat",  "ROM/67/84.dat"  },
    { "ROM/64/29.dat",  "ROM/67/100.dat", "ROM/67/98.dat",  "ROM/67/96.dat"  },
};

static const CreationSqleHeadSet *CreationSqleHeadSetForCurrentEntry()
{
    const FFXICreationEntry *pEntry = CurrentHighPolyCreationEntry();
    if (!pEntry || !pEntry->headMeshDat)
        return NULL;
    for (const CreationSqleHeadSet &set : kCreationSqleHeadSets)
        if (strcmp(set.meshDat, pEntry->headMeshDat) == 0)
            return &set;
    return NULL;
}

static bool ReadSqleMotionInfo(const char *animDat, CreationSqleMotionInfo *outInfo)
{
    if (outInfo)
        *outInfo = {};
    if (!animDat || !outInfo)
        return false;

    char fullPath[MAX_PATH] = {};
    sprintf_s(fullPath, "%s%s", g_ffxiPath, animDat);
    BYTE *pBuf = nullptr;
    DWORD fileSize = 0;
    if (!ReadWholeFile(fullPath, &pBuf, &fileSize))
        return false;

    bool ok = false;
    if (fileSize >= 96 && memcmp(pBuf, "SQLE", 4) == 0)
    {
        char header[192] = {};
        const int copyLen = std::min<int>((int)sizeof(header) - 1, (int)fileSize);
        memcpy(header, pBuf, copyLen);
        for (int i = 0; i < copyLen; ++i)
        {
            if (header[i] == '\0')
                header[i] = ' ';
        }

        const char *motionHeader = strstr(header, "MOTION:");
        char channelName[64] = {};
        float timeSeconds = 0.0f;
        int channelCount = 0;
        int frameCount = 0;
        float step = 0.0f;
        const int parsed = motionHeader ?
            sscanf_s(motionHeader, "MOTION: %63[^,], time=%f, size=%i, frames=%i, step=%f",
                     channelName, (unsigned)_countof(channelName),
                     &timeSeconds, &channelCount, &frameCount, &step) : 0;
        if (parsed >= 4 && channelCount > 0 && frameCount > 0)
        {
            outInfo->valid = true;
            outInfo->frameChannel = (strstr(channelName, "FrameChannel") != nullptr);
            outInfo->timeSeconds = timeSeconds;
            outInfo->frameCount = frameCount;
            outInfo->channelCount = channelCount;
            ok = true;

            if (outInfo->frameChannel)
            {
                const size_t valueCount = (size_t)channelCount * (size_t)(frameCount + 1);
                const size_t byteCount = valueCount * sizeof(float);
                if (valueCount == 0 || valueCount > 10000000 || byteCount > fileSize)
                {
                    *outInfo = {};
                    ok = false;
                }
                else
                {
                    const size_t dataOfs = (size_t)fileSize - byteCount;
                    outInfo->frameValues.resize(valueCount);
                    memcpy(outInfo->frameValues.data(), pBuf + dataOfs, byteCount);
                }
            }
        }
    }

    delete[] pBuf;
    return ok;
}

static const char *CreationBodyAnimDatForRace(int raceIndex)
{
    return CreationSqlePairForRace(raceIndex).bodyBase;
}

static const char *CreationHeadAnimDatForRace(int raceIndex)
{
    const CreationSqleHeadSet *pSet = CreationSqleHeadSetForCurrentEntry();
    return pSet ? pSet->baseDat : CreationSqlePairForRace(raceIndex).headBase;
}

static const char *CreationPreviewIdleBodyDatForRace(int raceIndex)
{
    return CreationSqlePairForRace(raceIndex).bodyIdle;
}

static const char *CreationPreviewIdleHeadDatForRace(int raceIndex)
{
    const CreationSqleHeadSet *pSet = CreationSqleHeadSetForCurrentEntry();
    return pSet ? pSet->idleDat : CreationSqlePairForRace(raceIndex).headIdle;
}

static const char *CreationPreviewWalkBodyDatForRace(int raceIndex)
{
    return CreationSqlePairForRace(raceIndex).bodyWalk;
}

static const char *CreationPreviewWalkHeadDatForRace(int raceIndex)
{
    const CreationSqleHeadSet *pSet = CreationSqleHeadSetForCurrentEntry();
    return pSet ? pSet->walkDat : CreationSqlePairForRace(raceIndex).headWalk;
}

static void BuildHighPolyNoesisScene(char *out, size_t outSize)
{
    char bodyMesh[64] = {};
    char headMesh[64] = {};
    char bodyAnim[64] = {};
    char headAnim[64] = {};
    GetCurrentCreationModelPaths(bodyMesh, sizeof(bodyMesh), headMesh, sizeof(headMesh));
    MakeRelativeSqleOption(CreationBodyAnimDatForRace(g_creationRaceIndex), bodyAnim, sizeof(bodyAnim));
    MakeRelativeSqleOption(CreationHeadAnimDatForRace(g_creationRaceIndex), headAnim, sizeof(headAnim));

    sprintf_s(out, outSize,
        "NOESIS_SCENE_FILE\n"
        "version 1\n"
        "physicslib\t\t\"\"\n"
        "defaultAxis\t\t\"0\"\n"
        "\n"
        "object\n"
        "{\n"
        "\tname\t\t\t\"body\"\n"
        "\tmodel\t\t\t\"%s\"\n"
        "\tloadOptions\t\t\"-ff11sqleanim %s\"\n"
        "}\n"
        "object\n"
        "{\n"
        "\tname\t\t\t\"head\"\n"
        "\tmodel\t\t\t\"%s\"\n"
        "\tloadOptions\t\t\"-ff11sqleanim %s\"\n"
        "\t;this uses the relative positions of the neck joints on each skeleton to place the head\n"
        "\toffsetWithBones\t\"bone0001\" \"body\" \"bone0004\"\n"
        "\t;combine both objects into a single model\n"
        "\tmergeTo\t\t\t\"body\"\n"
        "}\n",
        bodyMesh, bodyAnim, headMesh, headAnim);
}

static const int IDC_SAVE_RESULT_OK = 7101;

struct SaveResultDialogData
{
    const char *noesisPath;
    const char *datSetPath;
};

static LRESULT CALLBACK SaveResultDialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        {
            const CREATESTRUCTA *pCreate = (const CREATESTRUCTA *)lParam;
            const SaveResultDialogData *pData = (const SaveResultDialogData *)pCreate->lpCreateParams;
            HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

            HWND hIcon = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE | SS_ICON,
                18, 28, 32, 32, hWnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageA(hIcon, STM_SETICON, (WPARAM)LoadIcon(NULL, IDI_INFORMATION), 0);

            HWND hLabel = CreateWindowExA(0, "STATIC", "Saved character files:",
                WS_CHILD | WS_VISIBLE, 64, 24, 520, 18, hWnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageA(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hNoesis = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", pData ? pData->noesisPath : "",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
                64, 46, 520, 22, hWnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageA(hNoesis, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hDatSet = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", pData ? pData->datSetPath : "",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
                64, 74, 520, 22, hWnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageA(hDatSet, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hOk = CreateWindowExA(0, "BUTTON", "OK",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                510, 122, 74, 24, hWnd, (HMENU)(INT_PTR)IDC_SAVE_RESULT_OK, GetModuleHandle(NULL), NULL);
            SendMessageA(hOk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SetFocus(hOk);
            return 0;
        }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_SAVE_RESULT_OK && HIWORD(wParam) == BN_CLICKED)
        {
            DestroyWindow(hWnd);
            return 0;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;
    }

    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

static int SaveResultMeasureText(HWND hOwner, HFONT hFont, const char *text)
{
    HDC hdc = GetDC(hOwner ? hOwner : g_hWnd);
    HGDIOBJ oldFont = SelectObject(hdc, hFont);
    SIZE size = {};
    GetTextExtentPoint32A(hdc, text ? text : "", (int)strlen(text ? text : ""), &size);
    SelectObject(hdc, oldFont);
    ReleaseDC(hOwner ? hOwner : g_hWnd, hdc);
    return size.cx;
}

static void ShowSaveResultDialog(HWND hOwner, const char *noesisPath, const char *datSetPath)
{
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = SaveResultDialogProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hIcon = LoadIcon(NULL, IDI_INFORMATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "DATuraSaveResultDialog";
    RegisterClassExA(&wc);

    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    const int textWidth = std::max(SaveResultMeasureText(hOwner, hFont, noesisPath),
                                   SaveResultMeasureText(hOwner, hFont, datSetPath));
    const int clientWidth = std::max(412, textWidth + 132);

    RECT workArea = {};
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &workArea, 0);
    const int workWidth = (int)(workArea.right - workArea.left);
    const int workHeight = (int)(workArea.bottom - workArea.top);
    const int maxClientWidth = std::max(412, workWidth - 120);
    const int finalClientWidth = std::min(clientWidth, maxClientWidth);
    const int editWidth = finalClientWidth - 88;
    const int clientHeight = 162;

    RECT wr = { 0, 0, finalClientWidth, clientHeight };
    AdjustWindowRectEx(&wr, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);

    RECT ownerRect = {};
    if (hOwner && IsWindow(hOwner))
        GetWindowRect(hOwner, &ownerRect);
    else
        GetWindowRect(g_hWnd, &ownerRect);
    const int ownerCx = ownerRect.right - ownerRect.left;
    const int ownerCy = ownerRect.bottom - ownerRect.top;
    const int winWidth = wr.right - wr.left;
    const int winHeight = wr.bottom - wr.top;
    int x = ownerRect.left + (ownerCx - winWidth) / 2;
    int y = ownerRect.top + (ownerCy - winHeight) / 2;
    x = std::max((int)workArea.left, std::min(x, (int)workArea.right - winWidth));
    y = std::max((int)workArea.top, std::min(y, (int)workArea.bottom - winHeight));

    SaveResultDialogData data = { noesisPath, datSetPath };
    HWND hDlg = CreateWindowExA(WS_EX_DLGMODALFRAME, "DATuraSaveResultDialog", "Save Character",
        WS_POPUP | WS_CAPTION | WS_SYSMENU, x, y, winWidth, winHeight,
        hOwner, NULL, GetModuleHandle(NULL), &data);
    if (!hDlg)
        return;

    SetWindowPos(GetWindow(hDlg, GW_CHILD), NULL, 18, 28, 32, 32, SWP_NOZORDER);
    HWND hChild = GetWindow(hDlg, GW_CHILD);
    hChild = GetWindow(hChild, GW_HWNDNEXT);
    SetWindowPos(hChild, NULL, 64, 24, editWidth, 18, SWP_NOZORDER);
    hChild = GetWindow(hChild, GW_HWNDNEXT);
    SetWindowPos(hChild, NULL, 64, 46, editWidth, 22, SWP_NOZORDER);
    hChild = GetWindow(hChild, GW_HWNDNEXT);
    SetWindowPos(hChild, NULL, 64, 74, editWidth, 22, SWP_NOZORDER);
    hChild = GetWindow(hChild, GW_HWNDNEXT);
    SetWindowPos(hChild, NULL, finalClientWidth - 92, 122, 74, 24, SWP_NOZORDER);

    ShowWindow(hDlg, SW_SHOW);
    if (hOwner)
        EnableWindow(hOwner, FALSE);
    MSG msg = {};
    while (IsWindow(hDlg) && GetMessageA(&msg, NULL, 0, 0) > 0)
    {
        if (!IsDialogMessageA(hDlg, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
    if (hOwner)
    {
        EnableWindow(hOwner, TRUE);
        SetForegroundWindow(hOwner);
    }
}

static void AppendLowPolyInferredPart(char *out, size_t outSize, const char *name,
                                      const FFXICharRace &race, int entryIndex, int offset)
{
    if (offset <= 0)
        AppendDatSetLine(out, outSize, name, race.entries[entryIndex].dat);
    else
        AppendVariantDatLine(out, outSize, name, race, entryIndex, offset);
}

static void BuildLowPolyDatSet(char *out, size_t outSize)
{
    const int raceIndex = (g_creationRaceIndex >= 0 && g_creationRaceIndex < kFFXICharRaceCount) ? g_creationRaceIndex : 0;
    const FFXICharRace &race = kFFXICharRaces[raceIndex];
    const int faceOffset = g_creationFaceIndex;
    const int equipmentOffset = (g_creationEquipmentIndex == 1) ? 1 : 0;

    strcpy_s(out, outSize,
        "NOESIS_FF11_DAT_SET\n"
        ";^ must be the first line of the file\n"
        "\n"
        ";search for dats using a path retrieved from a registry key\n"
        "setPathKey\t\"HKEY_LOCAL_MACHINE\" \"SOFTWARE\\PlayOnlineUS\\InstallFolder\" \"0001\"\n"
        "\n"
        ";search for dats on a path relative to this file \n"
        ";setPathRel\t\"./\"\n"
        "\n"
        ";search for dats on an absolute path\n"
        ";setPathAbs\t\"c:/whatever/ff11/\"\n"
        "\n");

    AppendDatSetLine(out, outSize, "__skeleton", race.entries[0].dat);
    if (race.count > 8)
        AppendDatSetLine(out, outSize, "__animation", race.entries[8].dat);

    AppendLowPolyInferredPart(out, outSize, "face", race, 1, faceOffset);
    AppendLowPolyInferredPart(out, outSize, "head", race, 2, faceOffset);
    AppendLowPolyInferredPart(out, outSize, "body", race, 3, equipmentOffset);
    AppendLowPolyInferredPart(out, outSize, "hands", race, 4, equipmentOffset);
    AppendLowPolyInferredPart(out, outSize, "waist", race, 5, equipmentOffset);
    AppendLowPolyInferredPart(out, outSize, "legs", race, 6, equipmentOffset);
    AppendDatSetLine(out, outSize, "weapon", race.entries[7].dat);
}

static void SaveHighPolyCreationCharacter()
{
    if (g_hHighPolyCreationPanel)
    {
        GetWindowTextA(HighPolyCreationPanelControl(IDC_HP_NAME),
                       g_creationCharacterName, sizeof(g_creationCharacterName));
        PullHighPolyCreationStateFromControls();
    }

    char defaultPath[MAX_PATH] = {};
    MakeDefaultCharacterSavePath(defaultPath, sizeof(defaultPath));

    char savePath[MAX_PATH] = {};
    strcpy_s(savePath, defaultPath);
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hHighPolyCreationPanel ? g_hHighPolyCreationPanel : g_hWnd;
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
    ReplaceExtension(savePath, ".noesis", noesisPath, sizeof(noesisPath));
    ReplaceExtension(savePath, ".ff11datset", datSetPath, sizeof(datSetPath));

    char noesisText[4096] = {};
    char datSetText[4096] = {};
    BuildHighPolyNoesisScene(noesisText, sizeof(noesisText));
    BuildLowPolyDatSet(datSetText, sizeof(datSetText));

    const bool wroteNoesis = WriteTextFile(noesisPath, noesisText);
    const bool wroteDatSet = WriteTextFile(datSetPath, datSetText);
    if (!wroteNoesis || !wroteDatSet)
    {
        MessageBoxA(g_hHighPolyCreationPanel ? g_hHighPolyCreationPanel : g_hWnd,
                    "Could not save one or both character files.",
                    "Save Character", MB_OK | MB_ICONERROR);
        return;
    }

    ShowSaveResultDialog(g_highPolyCreationActive && g_hWnd ? g_hWnd : NULL,
                         noesisPath, datSetPath);
}

static void BeginNationSelectScene()
{
    if (g_hHighPolyCreationPanel)
    {
        GetWindowTextA(HighPolyCreationPanelControl(IDC_HP_NAME),
                       g_creationCharacterName, sizeof(g_creationCharacterName));
        PullHighPolyCreationStateFromControls();
        ShowWindow(g_hHighPolyCreationPanel, SW_HIDE);
    }

    g_titleScreenActive = false;
    g_highPolyCreationActive = true;
    g_nationSelectActive = true;
    g_selectedNationIndex = 0;
    EnsureNationSelectAssets();
    InvalidateRect(g_hWnd, NULL, FALSE);
}

static void ReturnToTitleScreen()
{
    if (g_hHighPolyCreationPanel)
        ShowWindow(g_hHighPolyCreationPanel, SW_HIDE);
    if (g_hLowPolyPanel)
        ShowWindow(g_hLowPolyPanel, SW_HIDE);
    if (g_hZoneObjectPanel)
        ShowWindow(g_hZoneObjectPanel, SW_HIDE);

    UnloadPlayerModel();
    g_highPolyCreationActive = false;
    g_nationSelectActive = false;
    LoadTitleScreen();
    if (g_hWnd)
        InvalidateRect(g_hWnd, NULL, FALSE);
}

static void ApplyCreationStateToLowPolyPlayer()
{
    g_playerEquip.raceIndex = (g_creationRaceIndex >= 0 && g_creationRaceIndex < kFFXICharRaceCount) ? g_creationRaceIndex : 0;
    g_playerFaceVariant = (g_creationFaceIndex > 0) ? g_creationFaceIndex : 0;

    const int equipmentVariant = (g_creationEquipmentIndex == 1) ? 1 : 0;
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

static void ComboAddString(HWND hCombo, const char *text)
{
    SendMessageA(hCombo, CB_ADDSTRING, 0, (LPARAM)text);
}

static void ComboAddStringWithData(HWND hCombo, const char *text, int data)
{
    const LRESULT index = SendMessageA(hCombo, CB_ADDSTRING, 0, (LPARAM)text);
    if (index != CB_ERR)
        SendMessageA(hCombo, CB_SETITEMDATA, (WPARAM)index, (LPARAM)data);
}

static void ComboSetIndex(HWND hCombo, int index)
{
    SendMessageA(hCombo, CB_SETCURSEL, (WPARAM)index, 0);
}

static void ComboSetIndexByData(HWND hCombo, int data)
{
    const int count = (int)SendMessageA(hCombo, CB_GETCOUNT, 0, 0);
    for (int i = 0; i < count; ++i)
    {
        if ((int)SendMessageA(hCombo, CB_GETITEMDATA, (WPARAM)i, 0) == data)
        {
            ComboSetIndex(hCombo, i);
            return;
        }
    }

    ComboSetIndex(hCombo, 0);
}

static int ComboGetIndex(HWND hCombo)
{
    const LRESULT index = SendMessageA(hCombo, CB_GETCURSEL, 0, 0);
    return (index == CB_ERR) ? 0 : (int)index;
}

static int ComboGetSelectedData(HWND hCombo)
{
    const int index = ComboGetIndex(hCombo);
    const LRESULT data = SendMessageA(hCombo, CB_GETITEMDATA, (WPARAM)index, 0);
    return (data == CB_ERR) ? index : (int)data;
}

static int LowPolyFaceVariantCountForRace(int raceIndex)
{
    (void)raceIndex;
    return kFaceVariantCount;
}

static void ClampLowPolyState()
{
    if (g_playerEquip.raceIndex < 0)
        g_playerEquip.raceIndex = 0;
    if (g_playerEquip.raceIndex >= kFFXICharRaceCount)
        g_playerEquip.raceIndex = kFFXICharRaceCount - 1;

    const int faceCount = LowPolyFaceVariantCountForRace(g_playerEquip.raceIndex);
    if (g_playerFaceVariant < 0)
        g_playerFaceVariant = 0;
    if (g_playerFaceVariant >= faceCount)
        g_playerFaceVariant = faceCount - 1;

    if (g_playerEquip.animationMode < 0)
        g_playerEquip.animationMode = 0;
    if (g_playerEquip.animationMode >= kAnimationModeCount)
        g_playerEquip.animationMode = kAnimationModeCount - 1;
    if (g_playerEquip.animationBank < 0)
        g_playerEquip.animationBank = 0;
}

static HWND LowPolyPanelControl(int id)
{
    return g_hLowPolyPanel ? GetDlgItem(g_hLowPolyPanel, id) : NULL;
}

static HWND HighPolyCreationPanelControl(int id)
{
    return g_hHighPolyCreationPanel ? GetDlgItem(g_hHighPolyCreationPanel, id) : NULL;
}

static void FillComboFromLabels(HWND hCombo, const char **labels, int count, int selected)
{
    SendMessageA(hCombo, CB_RESETCONTENT, 0, 0);
    for (int i = 0; i < count; ++i)
        ComboAddString(hCombo, labels[i]);
    ComboSetIndex(hCombo, selected);
}

static void FillVariantCombo(HWND hCombo, const char *noneLabel, const char *itemPrefix,
                             int itemCount, int selected)
{
    char label[64] = {};
    SendMessageA(hCombo, CB_RESETCONTENT, 0, 0);
    ComboAddString(hCombo, noneLabel);
    for (int i = 1; i <= itemCount; ++i)
    {
        sprintf_s(label, "%s %03d", itemPrefix, i);
        ComboAddString(hCombo, label);
    }
    ComboSetIndex(hCombo, selected);
}

static void FillCatalogVariantCombo(HWND hCombo, const char *noneLabel, const char *itemPrefix,
                                    int itemCount, int selected, int raceIndex, int slot)
{
    char label[128] = {};
    SendMessageA(hCombo, CB_RESETCONTENT, 0, 0);
    ComboAddStringWithData(hCombo, noneLabel, 0);
    for (int i = 1; i <= itemCount; ++i)
    {
        const char *catalogLabel = FFXIInternal_FindPCLabel(raceIndex, slot, i);
        if (catalogLabel && catalogLabel[0])
            sprintf_s(label, "%03d - %s", i, catalogLabel);
        else
            sprintf_s(label, "%s %03d", itemPrefix, i);
        ComboAddStringWithData(hCombo, label, i);
    }
    ComboSetIndexByData(hCombo, selected);
}

static void FillCatalogSparseCombo(HWND hCombo, const char *noneLabel,
                                   int selected, int raceIndex, int slot)
{
    char label[160] = {};
    SendMessageA(hCombo, CB_RESETCONTENT, 0, 0);
    ComboAddStringWithData(hCombo, noneLabel, 0);

    bool foundSelected = (selected == 0);
    if (raceIndex >= 0 && raceIndex < (int)(sizeof(kFFXIInternalPCLists) / sizeof(kFFXIInternalPCLists[0])) &&
        slot >= 0 && slot < 11)
    {
        const FFXIInternalList &list = kFFXIInternalPCLists[raceIndex][slot];
        for (int i = 0; i < list.count; ++i)
        {
            const int itemIndex = list.entries[i].index;
            if (itemIndex <= 0 || !list.entries[i].label || !list.entries[i].label[0])
                continue;
            sprintf_s(label, "%03d - %s", itemIndex, list.entries[i].label);
            ComboAddStringWithData(hCombo, label, itemIndex);
            if (itemIndex == selected)
                foundSelected = true;
        }
    }

    if (!foundSelected && selected > 0)
    {
        sprintf_s(label, "%03d - Custom DAT offset", selected);
        ComboAddStringWithData(hCombo, label, selected);
    }

    ComboSetIndexByData(hCombo, selected);
}

static void FillCatalogBaseVariantCombo(HWND hCombo, const char *itemPrefix,
                                        int itemCount, int selected, int raceIndex, int slot)
{
    char label[128] = {};
    SendMessageA(hCombo, CB_RESETCONTENT, 0, 0);
    for (int i = 0; i < itemCount; ++i)
    {
        const char *catalogLabel = FFXIInternal_FindPCLabel(raceIndex, slot, i);
        if (catalogLabel && catalogLabel[0])
            sprintf_s(label, "%03d - %s", i, catalogLabel);
        else
            sprintf_s(label, "%s %03d", itemPrefix, i);
        ComboAddStringWithData(hCombo, label, i);
    }
    ComboSetIndexByData(hCombo, selected);
}

static bool LowPolyAnimationCategoryMatches(const char *label, const char *prefix)
{
    if (!label || !label[0])
        return false;
    if (!prefix || !prefix[0])
        return true;

    const size_t prefixLen = strlen(prefix);
    if (strncmp(label, prefix, prefixLen) != 0)
        return false;
    return label[prefixLen] == '\0' || label[prefixLen] == ':';
}

static const LowPolyAnimationCategory &LowPolyAnimationCategoryForIndex(int categoryIndex)
{
    if (categoryIndex < 0 || categoryIndex >= kAnimationModeCount)
        categoryIndex = 0;
    return kLowPolyAnimationCategories[categoryIndex];
}

static void FillLowPolyAnimationCategoryCombo(HWND hCombo, int selected)
{
    SendMessageA(hCombo, CB_RESETCONTENT, 0, 0);
    for (int i = 0; i < kAnimationModeCount; ++i)
        ComboAddString(hCombo, kLowPolyAnimationCategories[i].label);
    ComboSetIndex(hCombo, selected);
}

static void FillLowPolyAnimationCombo(HWND hCombo, int selected, int raceIndex, int categoryIndex)
{
    char label[192] = {};
    SendMessageA(hCombo, CB_RESETCONTENT, 0, 0);

    const LowPolyAnimationCategory &category = LowPolyAnimationCategoryForIndex(categoryIndex);
    bool foundSelected = false;
    if (raceIndex >= 0 && raceIndex < (int)(sizeof(kFFXIInternalPCLists) / sizeof(kFFXIInternalPCLists[0])) &&
        category.slot >= 0 && category.slot < 11)
    {
        const FFXIInternalList &list = kFFXIInternalPCLists[raceIndex][category.slot];
        for (int i = 0; i < list.count; ++i)
        {
            const int animIndex = list.entries[i].index;
            const char *animLabel = list.entries[i].label;
            if (!LowPolyAnimationCategoryMatches(animLabel, category.prefix))
                continue;

            sprintf_s(label, "%05d - %s", animIndex, animLabel ? animLabel : "Animation");
            ComboAddStringWithData(hCombo, label, animIndex);
            if (animIndex == selected)
                foundSelected = true;
        }
    }

    if (!foundSelected && selected >= 0)
    {
        sprintf_s(label, "%05d - Custom animation DAT offset", selected);
        ComboAddStringWithData(hCombo, label, selected);
    }

    if (SendMessageA(hCombo, CB_GETCOUNT, 0, 0) <= 0)
        ComboAddStringWithData(hCombo, "00000 - Animation set 0", 0);

    ComboSetIndexByData(hCombo, selected);
}

static void FillFaceVariantCombo(HWND hCombo, int selected, int raceIndex)
{
    char label[64] = {};
    SendMessageA(hCombo, CB_RESETCONTENT, 0, 0);
    const int faceCount = LowPolyFaceVariantCountForRace(raceIndex);
    for (int i = 0; i < faceCount; ++i)
    {
        sprintf_s(label, "Face %d%c", (i / 2) + 1, (i & 1) ? 'B' : 'A');
        ComboAddString(hCombo, label);
    }
    ComboSetIndex(hCombo, selected);
}

static int RandomIntExclusive(int count)
{
    if (count <= 1)
        return 0;

    static std::mt19937 rng((unsigned int)GetTickCount());
    std::uniform_int_distribution<int> dist(0, count - 1);
    return dist(rng);
}

static int RandomCatalogIndex(int raceIndex, int slot, int minIndex, int maxIndex,
                              bool allowZero, int fallbackMax)
{
    std::vector<int> candidates;
    if (raceIndex >= 0 && raceIndex < (int)(sizeof(kFFXIInternalPCLists) / sizeof(kFFXIInternalPCLists[0])) &&
        slot >= 0 && slot < 11)
    {
        const FFXIInternalList &list = kFFXIInternalPCLists[raceIndex][slot];
        for (int i = 0; i < list.count; ++i)
        {
            const int index = list.entries[i].index;
            if ((!allowZero && index <= 0) || index < minIndex || index > maxIndex)
                continue;
            if (!list.entries[i].label || !list.entries[i].label[0])
                continue;
            candidates.push_back(index);
        }
    }

    if (!candidates.empty())
        return candidates[RandomIntExclusive((int)candidates.size())];

    if (fallbackMax <= 0)
        return 0;
    return allowZero ? RandomIntExclusive(fallbackMax + 1) : (1 + RandomIntExclusive(fallbackMax));
}

static int RandomSparseCatalogIndex(int raceIndex, int slot, int noneChanceDivisor)
{
    if (noneChanceDivisor > 0 && RandomIntExclusive(noneChanceDivisor) == 0)
        return 0;

    std::vector<int> candidates;
    if (raceIndex >= 0 && raceIndex < (int)(sizeof(kFFXIInternalPCLists) / sizeof(kFFXIInternalPCLists[0])) &&
        slot >= 0 && slot < 11)
    {
        const FFXIInternalList &list = kFFXIInternalPCLists[raceIndex][slot];
        for (int i = 0; i < list.count; ++i)
        {
            const int index = list.entries[i].index;
            if (index <= 0 || !list.entries[i].label || !list.entries[i].label[0])
                continue;
            candidates.push_back(index);
        }
    }

    return candidates.empty() ? 0 : candidates[RandomIntExclusive((int)candidates.size())];
}

static void FinishLowPolyRandomize()
{
    if (g_hLowPolyPanel)
        SyncLowPolyControlsFromState();
    ReloadPlayerModelFromControls();
}

static void RandomizeLowPolyCharacterSection()
{
    if (g_hLowPolyPanel)
        PullLowPolyStateFromControls();

    g_playerEquip.raceIndex = RandomIntExclusive(kFFXICharRaceCount);
    g_playerFaceVariant = RandomIntExclusive(LowPolyFaceVariantCountForRace(g_playerEquip.raceIndex));
    g_playerEquip.animationPlaying = false;
    g_playerAnimTime = 0.0f;
    ClampLowPolyState();
    FinishLowPolyRandomize();
}

static void RandomizeLowPolyWeaponsSection()
{
    if (g_hLowPolyPanel)
        PullLowPolyStateFromControls();

    const int raceIndex = (g_playerEquip.raceIndex >= 0 && g_playerEquip.raceIndex < kFFXICharRaceCount)
        ? g_playerEquip.raceIndex
        : 0;
    g_playerEquip.mainItem = RandomSparseCatalogIndex(raceIndex, kFFXIInternalPCSlot_Main, 0);
    g_playerEquip.subItem = RandomSparseCatalogIndex(raceIndex, kFFXIInternalPCSlot_Sub, 4);
    g_playerEquip.rangedItem = RandomSparseCatalogIndex(raceIndex, kFFXIInternalPCSlot_Range, 3);
    g_playerEquip.animationPlaying = false;
    g_playerAnimTime = 0.0f;
    FinishLowPolyRandomize();
}

static void RandomizeLowPolyArmorSection()
{
    if (g_hLowPolyPanel)
        PullLowPolyStateFromControls();

    const int raceIndex = (g_playerEquip.raceIndex >= 0 && g_playerEquip.raceIndex < kFFXICharRaceCount)
        ? g_playerEquip.raceIndex
        : 0;
    g_playerEquip.headItem = RandomCatalogIndex(raceIndex, kFFXIInternalPCSlot_Head, 1, kArmorVariantCount, true, kArmorVariantCount);
    g_playerEquip.bodyItem = RandomCatalogIndex(raceIndex, kFFXIInternalPCSlot_Body, 0, kArmorVariantCount - 1, true, kArmorVariantCount - 1);
    g_playerEquip.handsItem = RandomCatalogIndex(raceIndex, kFFXIInternalPCSlot_Hands, 0, kArmorVariantCount - 1, true, kArmorVariantCount - 1);
    g_playerEquip.legsItem = RandomCatalogIndex(raceIndex, kFFXIInternalPCSlot_Legs, 0, kArmorVariantCount - 1, true, kArmorVariantCount - 1);
    g_playerEquip.feetItem = RandomCatalogIndex(raceIndex, kFFXIInternalPCSlot_Feet, 0, kArmorVariantCount - 1, true, kArmorVariantCount - 1);
    g_playerEquip.animationPlaying = false;
    g_playerAnimTime = 0.0f;
    FinishLowPolyRandomize();
}

static void RandomizeLowPolyActionSection()
{
    if (g_hLowPolyPanel)
        PullLowPolyStateFromControls();

    g_playerEquip.animationMode = RandomIntExclusive(kAnimationModeCount);
    const LowPolyAnimationCategory &category = LowPolyAnimationCategoryForIndex(g_playerEquip.animationMode);
    std::vector<int> candidates;
    const int raceIndex = (g_playerEquip.raceIndex >= 0 && g_playerEquip.raceIndex < kFFXICharRaceCount)
        ? g_playerEquip.raceIndex
        : 0;
    if (category.slot >= 0 && category.slot < 11)
    {
        const FFXIInternalList &list = kFFXIInternalPCLists[raceIndex][category.slot];
        for (int i = 0; i < list.count; ++i)
        {
            if (LowPolyAnimationCategoryMatches(list.entries[i].label, category.prefix))
                candidates.push_back(list.entries[i].index);
        }
    }
    g_playerEquip.animationBank = candidates.empty() ? 0 : candidates[RandomIntExclusive((int)candidates.size())];
    g_playerEquip.animationPlaying = false;
    g_playerAnimTime = 0.0f;
    ClampLowPolyState();
    FinishLowPolyRandomize();
}

static void RandomizeLowPolyAll()
{
    if (g_hLowPolyPanel)
        PullLowPolyStateFromControls();

    g_playerEquip.raceIndex = RandomIntExclusive(kFFXICharRaceCount);
    g_playerFaceVariant = RandomIntExclusive(LowPolyFaceVariantCountForRace(g_playerEquip.raceIndex));
    const int raceIndex = g_playerEquip.raceIndex;
    g_playerEquip.mainItem = RandomSparseCatalogIndex(raceIndex, kFFXIInternalPCSlot_Main, 0);
    g_playerEquip.subItem = RandomSparseCatalogIndex(raceIndex, kFFXIInternalPCSlot_Sub, 4);
    g_playerEquip.rangedItem = RandomSparseCatalogIndex(raceIndex, kFFXIInternalPCSlot_Range, 3);
    g_playerEquip.headItem = RandomCatalogIndex(raceIndex, kFFXIInternalPCSlot_Head, 1, kArmorVariantCount, true, kArmorVariantCount);
    g_playerEquip.bodyItem = RandomCatalogIndex(raceIndex, kFFXIInternalPCSlot_Body, 0, kArmorVariantCount - 1, true, kArmorVariantCount - 1);
    g_playerEquip.handsItem = RandomCatalogIndex(raceIndex, kFFXIInternalPCSlot_Hands, 0, kArmorVariantCount - 1, true, kArmorVariantCount - 1);
    g_playerEquip.legsItem = RandomCatalogIndex(raceIndex, kFFXIInternalPCSlot_Legs, 0, kArmorVariantCount - 1, true, kArmorVariantCount - 1);
    g_playerEquip.feetItem = RandomCatalogIndex(raceIndex, kFFXIInternalPCSlot_Feet, 0, kArmorVariantCount - 1, true, kArmorVariantCount - 1);
    g_playerEquip.animationMode = RandomIntExclusive(kAnimationModeCount);
    {
        const LowPolyAnimationCategory &category = LowPolyAnimationCategoryForIndex(g_playerEquip.animationMode);
        std::vector<int> candidates;
        if (category.slot >= 0 && category.slot < 11)
        {
            const FFXIInternalList &list = kFFXIInternalPCLists[raceIndex][category.slot];
            for (int i = 0; i < list.count; ++i)
            {
                if (LowPolyAnimationCategoryMatches(list.entries[i].label, category.prefix))
                    candidates.push_back(list.entries[i].index);
            }
        }
        g_playerEquip.animationBank = candidates.empty() ? 0 : candidates[RandomIntExclusive((int)candidates.size())];
    }
    g_playerEquip.animationPlaying = false;
    g_playerAnimTime = 0.0f;
    ClampLowPolyState();
    FinishLowPolyRandomize();
}

static void SyncLowPolyControlsFromState()
{
    if (!g_hLowPolyPanel)
        return;

    ClampLowPolyState();
    HWND hRace = LowPolyPanelControl(IDC_LP_RACE);
    SendMessageA(hRace, CB_RESETCONTENT, 0, 0);
    for (int r = 0; r < kFFXICharRaceCount; ++r)
        ComboAddString(hRace, kFFXICharRaces[r].name);
    ComboSetIndex(hRace, g_playerEquip.raceIndex);
    FillFaceVariantCombo(LowPolyPanelControl(IDC_LP_FACE), g_playerFaceVariant, g_playerEquip.raceIndex);

    FillComboFromLabels(LowPolyPanelControl(IDC_LP_MAIN_TYPE), kWeaponTypeLabels, kWeaponTypeCount, g_playerEquip.mainType);
    FillComboFromLabels(LowPolyPanelControl(IDC_LP_SUB_TYPE), kWeaponTypeLabels, kWeaponTypeCount, g_playerEquip.subType);
    FillComboFromLabels(LowPolyPanelControl(IDC_LP_RANGE_TYPE), kWeaponTypeLabels, kWeaponTypeCount, g_playerEquip.rangedType);
    FillLowPolyAnimationCategoryCombo(LowPolyPanelControl(IDC_LP_ANIM_MODE), g_playerEquip.animationMode);

    const int raceIndex = g_playerEquip.raceIndex;
    FillCatalogSparseCombo(LowPolyPanelControl(IDC_LP_MAIN_ITEM), "None",
        g_playerEquip.mainItem, raceIndex, kFFXIInternalPCSlot_Main);
    FillCatalogSparseCombo(LowPolyPanelControl(IDC_LP_SUB_ITEM), "None",
        g_playerEquip.subItem, raceIndex, kFFXIInternalPCSlot_Sub);
    FillCatalogSparseCombo(LowPolyPanelControl(IDC_LP_RANGE_ITEM), "None",
        g_playerEquip.rangedItem, raceIndex, kFFXIInternalPCSlot_Range);
    FillCatalogVariantCombo(LowPolyPanelControl(IDC_LP_HEAD), "None", "Head",
        kArmorVariantCount, g_playerEquip.headItem, raceIndex, kFFXIInternalPCSlot_Head);
    FillCatalogBaseVariantCombo(LowPolyPanelControl(IDC_LP_BODY), "Body",
        kArmorVariantCount, g_playerEquip.bodyItem, raceIndex, kFFXIInternalPCSlot_Body);
    FillCatalogBaseVariantCombo(LowPolyPanelControl(IDC_LP_HANDS), "Hands",
        kArmorVariantCount, g_playerEquip.handsItem, raceIndex, kFFXIInternalPCSlot_Hands);
    FillCatalogBaseVariantCombo(LowPolyPanelControl(IDC_LP_LEGS), "Legs",
        kArmorVariantCount, g_playerEquip.legsItem, raceIndex, kFFXIInternalPCSlot_Legs);
    FillCatalogBaseVariantCombo(LowPolyPanelControl(IDC_LP_FEET), "Feet",
        kArmorVariantCount, g_playerEquip.feetItem, raceIndex, kFFXIInternalPCSlot_Feet);

    HWND hAnimBank = LowPolyPanelControl(IDC_LP_ANIM_BANK);
    FillLowPolyAnimationCombo(hAnimBank, g_playerEquip.animationBank, raceIndex, g_playerEquip.animationMode);
}

static const FFXICreationEntry *CurrentHighPolyCreationEntry()
{
    if (g_creationRaceIndex < 0 || g_creationRaceIndex >= kFFXICreationRaceCount)
        g_creationRaceIndex = 0;

    const FFXICreationRace &race = kFFXICreationRaces[g_creationRaceIndex];
    const int faceCount = race.count / 2;
    if (g_creationFaceIndex < 0)
        g_creationFaceIndex = 0;
    if (g_creationFaceIndex >= faceCount)
        g_creationFaceIndex = faceCount - 1;
    if (g_creationEquipmentIndex < 0 || g_creationEquipmentIndex > 1)
        g_creationEquipmentIndex = 0;

    const int entryIndex = g_creationFaceIndex * 2 + g_creationEquipmentIndex;
    return (entryIndex >= 0 && entryIndex < race.count) ? &race.entries[entryIndex] : nullptr;
}

static bool SetHighPolyCreationSelectionFromFlatIndex(int flatIndex)
{
    if (flatIndex < 0)
        return false;

    for (int raceIndex = 0; raceIndex < kFFXICreationRaceCount; ++raceIndex)
    {
        const FFXICreationRace &race = kFFXICreationRaces[raceIndex];
        if (flatIndex < race.count)
        {
            g_creationRaceIndex = raceIndex;
            g_creationFaceIndex = flatIndex / 2;
            g_creationEquipmentIndex = flatIndex % 2;
            return true;
        }
        flatIndex -= race.count;
    }

    return false;
}

static void SyncHighPolyCreationControlsFromState()
{
    if (!g_hHighPolyCreationPanel)
        return;

    HWND hRace = HighPolyCreationPanelControl(IDC_HP_RACE);
    SendMessageA(hRace, CB_RESETCONTENT, 0, 0);
    for (int r = 0; r < kFFXICreationRaceCount; ++r)
        ComboAddString(hRace, kFFXICreationRaces[r].name);
    ComboSetIndex(hRace, g_creationRaceIndex);

    const FFXICreationRace &race = kFFXICreationRaces[g_creationRaceIndex];
    HWND hFace = HighPolyCreationPanelControl(IDC_HP_FACE);
    SendMessageA(hFace, CB_RESETCONTENT, 0, 0);
    for (int face = 0; face < race.count / 2; ++face)
    {
        char label[64] = {};
        sprintf_s(label, "Face %d%s", (face / 2) + 1, (face & 1) ? "B" : "A");
        ComboAddString(hFace, label);
    }
    ComboSetIndex(hFace, g_creationFaceIndex);

    HWND hEquipment = HighPolyCreationPanelControl(IDC_HP_EQUIPMENT);
    SendMessageA(hEquipment, CB_RESETCONTENT, 0, 0);
    ComboAddString(hEquipment, "No Equipment");
    ComboAddString(hEquipment, "Initial Equipment");
    ComboSetIndex(hEquipment, g_creationEquipmentIndex);

    HWND hAnimation = HighPolyCreationPanelControl(IDC_HP_ANIMATION);
    SendMessageA(hAnimation, CB_RESETCONTENT, 0, 0);
    ComboAddString(hAnimation, "A-pose");
    ComboAddString(hAnimation, "Standing idle");
    ComboAddString(hAnimation, "Walk / run");
    ComboSetIndex(hAnimation, g_creationAnimationIndex);
}

static void PullHighPolyCreationStateFromControls()
{
    if (!g_hHighPolyCreationPanel)
        return;

    const int prevRace = g_creationRaceIndex;
    g_creationRaceIndex = ComboGetIndex(HighPolyCreationPanelControl(IDC_HP_RACE));
    if (g_creationRaceIndex != prevRace)
        g_creationFaceIndex = 0;
    else
        g_creationFaceIndex = ComboGetIndex(HighPolyCreationPanelControl(IDC_HP_FACE));
    g_creationEquipmentIndex = ComboGetIndex(HighPolyCreationPanelControl(IDC_HP_EQUIPMENT));
    g_creationAnimationIndex = ComboGetIndex(HighPolyCreationPanelControl(IDC_HP_ANIMATION));
}

static void ReloadHighPolyCreationFromControls()
{
    PullHighPolyCreationStateFromControls();
    SyncHighPolyCreationControlsFromState();

    const FFXICreationEntry *pEntry = CurrentHighPolyCreationEntry();
    if (pEntry)
        LoadCreationEntry(pEntry);
}

static void PullLowPolyStateFromControls()
{
    if (!g_hLowPolyPanel)
        return;

    g_playerEquip.raceIndex = ComboGetIndex(LowPolyPanelControl(IDC_LP_RACE));
    g_playerFaceVariant = ComboGetIndex(LowPolyPanelControl(IDC_LP_FACE));
    g_playerEquip.mainType = ComboGetIndex(LowPolyPanelControl(IDC_LP_MAIN_TYPE));
    g_playerEquip.mainItem = ComboGetSelectedData(LowPolyPanelControl(IDC_LP_MAIN_ITEM));
    g_playerEquip.subType = ComboGetIndex(LowPolyPanelControl(IDC_LP_SUB_TYPE));
    g_playerEquip.subItem = ComboGetSelectedData(LowPolyPanelControl(IDC_LP_SUB_ITEM));
    g_playerEquip.rangedType = ComboGetIndex(LowPolyPanelControl(IDC_LP_RANGE_TYPE));
    g_playerEquip.rangedItem = ComboGetSelectedData(LowPolyPanelControl(IDC_LP_RANGE_ITEM));
    g_playerEquip.headItem = ComboGetSelectedData(LowPolyPanelControl(IDC_LP_HEAD));
    g_playerEquip.bodyItem = ComboGetSelectedData(LowPolyPanelControl(IDC_LP_BODY));
    g_playerEquip.handsItem = ComboGetSelectedData(LowPolyPanelControl(IDC_LP_HANDS));
    g_playerEquip.legsItem = ComboGetSelectedData(LowPolyPanelControl(IDC_LP_LEGS));
    g_playerEquip.feetItem = ComboGetSelectedData(LowPolyPanelControl(IDC_LP_FEET));
    g_playerEquip.animationMode = ComboGetIndex(LowPolyPanelControl(IDC_LP_ANIM_MODE));
    g_playerEquip.animationBank = ComboGetSelectedData(LowPolyPanelControl(IDC_LP_ANIM_BANK));
    ClampLowPolyState();
}

static void SaveLowPolyPreset()
{
    char path[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hLowPolyPanel ? g_hLowPolyPanel : g_hWnd;
    ofn.lpstrFilter = "DATura Player Preset (*.datura-player)\0*.datura-player\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = sizeof(path);
    ofn.lpstrTitle = "Save Player Preset";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = "datura-player";
    if (!GetSaveFileNameA(&ofn))
        return;

    PullLowPolyStateFromControls();
    HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return;

    char text[640] = {};
    sprintf_s(text,
        "DATURA_PLAYER_PRESET 1\n"
        "race %d\nface %d\nmainType %d\nmainItem %d\nsubType %d\nsubItem %d\n"
        "rangedType %d\nrangedItem %d\nhead %d\nbody %d\nhands %d\nlegs %d\nfeet %d\n"
        "animBank %d\nanimMode %d\n",
        g_playerEquip.raceIndex, g_playerFaceVariant, g_playerEquip.mainType, g_playerEquip.mainItem,
        g_playerEquip.subType, g_playerEquip.subItem, g_playerEquip.rangedType,
        g_playerEquip.rangedItem, g_playerEquip.headItem, g_playerEquip.bodyItem,
        g_playerEquip.handsItem, g_playerEquip.legsItem, g_playerEquip.feetItem,
        g_playerEquip.animationBank, g_playerEquip.animationMode);

    DWORD written = 0;
    WriteFile(hFile, text, (DWORD)strlen(text), &written, nullptr);
    CloseHandle(hFile);
}

static bool ReadPresetInt(const char *text, const char *key, int *outValue)
{
    const char *p = strstr(text, key);
    if (!p)
        return false;
    p += strlen(key);
    while (*p == ' ' || *p == '\t')
        ++p;
    return sscanf_s(p, "%d", outValue) == 1;
}

static void LoadLowPolyPreset()
{
    char path[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hLowPolyPanel ? g_hLowPolyPanel : g_hWnd;
    ofn.lpstrFilter = "DATura Player Preset (*.datura-player)\0*.datura-player\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = sizeof(path);
    ofn.lpstrTitle = "Load Player Preset";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameA(&ofn))
        return;

    BYTE *pBuf = nullptr;
    DWORD fileSize = 0;
    if (!ReadWholeFile(path, &pBuf, &fileSize))
        return;

    char *text = new char[fileSize + 1];
    memcpy(text, pBuf, fileSize);
    text[fileSize] = '\0';
    delete[] pBuf;

    if (strncmp(text, "DATURA_PLAYER_PRESET", 20) == 0)
    {
        ReadPresetInt(text, "race", &g_playerEquip.raceIndex);
        ReadPresetInt(text, "face", &g_playerFaceVariant);
        ReadPresetInt(text, "mainType", &g_playerEquip.mainType);
        ReadPresetInt(text, "mainItem", &g_playerEquip.mainItem);
        ReadPresetInt(text, "subType", &g_playerEquip.subType);
        ReadPresetInt(text, "subItem", &g_playerEquip.subItem);
        ReadPresetInt(text, "rangedType", &g_playerEquip.rangedType);
        ReadPresetInt(text, "rangedItem", &g_playerEquip.rangedItem);
        ReadPresetInt(text, "head", &g_playerEquip.headItem);
        ReadPresetInt(text, "body", &g_playerEquip.bodyItem);
        ReadPresetInt(text, "hands", &g_playerEquip.handsItem);
        ReadPresetInt(text, "legs", &g_playerEquip.legsItem);
        ReadPresetInt(text, "feet", &g_playerEquip.feetItem);
        ReadPresetInt(text, "animBank", &g_playerEquip.animationBank);
        ReadPresetInt(text, "animMode", &g_playerEquip.animationMode);
        ClampLowPolyState();
        SyncLowPolyControlsFromState();
        ReloadPlayerModelFromControls();
    }

    delete[] text;
}

static HWND AddPanelControl(HWND hParent, const char *className, const char *text,
                            DWORD style, int id, int x, int y, int w, int h)
{
    return CreateWindowExA(0, className, text,
        WS_CHILD | WS_VISIBLE | style, x, y, w, h, hParent, (HMENU)(INT_PTR)id,
        GetModuleHandle(NULL), NULL);
}

static HWND AddPanelCombo(HWND hParent, int id, int x, int y, int w)
{
    return CreateWindowExA(0, "COMBOBOX", "",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        x, y, w, 220, hParent, (HMENU)(INT_PTR)id, GetModuleHandle(NULL), NULL);
}

static LRESULT CALLBACK LowPolyPanelProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        AddPanelControl(hWnd, "BUTTON", "Character", BS_GROUPBOX, -1, 8, 8, 448, 94);
        AddPanelControl(hWnd, "STATIC", "Race:", 0, -1, 18, 36, 66, 18);
        AddPanelCombo(hWnd, IDC_LP_RACE, 98, 32, 350);
        AddPanelControl(hWnd, "STATIC", "Face:", 0, -1, 18, 68, 66, 18);
        AddPanelCombo(hWnd, IDC_LP_FACE, 98, 64, 350);

        AddPanelControl(hWnd, "BUTTON", "Weapons", BS_GROUPBOX, -1, 8, 110, 448, 158);
        AddPanelControl(hWnd, "STATIC", "Main:", 0, -1, 18, 138, 66, 18);
        AddPanelCombo(hWnd, IDC_LP_MAIN_TYPE, 18, 156, 68);
        AddPanelCombo(hWnd, IDC_LP_MAIN_ITEM, 98, 156, 350);
        AddPanelControl(hWnd, "STATIC", "Sub:", 0, -1, 18, 184, 66, 18);
        AddPanelCombo(hWnd, IDC_LP_SUB_TYPE, 18, 202, 68);
        AddPanelCombo(hWnd, IDC_LP_SUB_ITEM, 98, 202, 350);
        AddPanelControl(hWnd, "STATIC", "Ranged:", 0, -1, 18, 230, 66, 18);
        AddPanelCombo(hWnd, IDC_LP_RANGE_TYPE, 18, 248, 68);
        AddPanelCombo(hWnd, IDC_LP_RANGE_ITEM, 98, 248, 350);

        AddPanelControl(hWnd, "BUTTON", "Armor", BS_GROUPBOX, -1, 8, 276, 448, 176);
        AddPanelControl(hWnd, "STATIC", "Head:", 0, -1, 18, 304, 66, 18);
        AddPanelCombo(hWnd, IDC_LP_HEAD, 98, 300, 350);
        AddPanelControl(hWnd, "STATIC", "Body:", 0, -1, 18, 332, 66, 18);
        AddPanelCombo(hWnd, IDC_LP_BODY, 98, 328, 350);
        AddPanelControl(hWnd, "STATIC", "Hands:", 0, -1, 18, 360, 66, 18);
        AddPanelCombo(hWnd, IDC_LP_HANDS, 98, 356, 350);
        AddPanelControl(hWnd, "STATIC", "Legs:", 0, -1, 18, 388, 66, 18);
        AddPanelCombo(hWnd, IDC_LP_LEGS, 98, 384, 350);
        AddPanelControl(hWnd, "STATIC", "Feet:", 0, -1, 18, 416, 66, 18);
        AddPanelCombo(hWnd, IDC_LP_FEET, 98, 412, 350);

        AddPanelControl(hWnd, "BUTTON", "Action", BS_GROUPBOX, -1, 8, 460, 448, 82);
        AddPanelCombo(hWnd, IDC_LP_ANIM_MODE, 18, 488, 100);
        AddPanelCombo(hWnd, IDC_LP_ANIM_BANK, 126, 488, 222);
        AddPanelControl(hWnd, "BUTTON", "Play", BS_PUSHBUTTON, IDC_LP_PLAY, 356, 488, 92, 24);
        AddPanelControl(hWnd, "BUTTON", "Stop", BS_PUSHBUTTON, IDC_LP_STOP, 356, 516, 44, 24);
        AddPanelControl(hWnd, "BUTTON", "Reset", BS_PUSHBUTTON, IDC_LP_RESET, 404, 516, 44, 24);

        AddPanelControl(hWnd, "BUTTON", "Randomize", BS_GROUPBOX, -1, 8, 550, 448, 54);
        AddPanelControl(hWnd, "BUTTON", "Character", BS_PUSHBUTTON, IDC_LP_RANDOM_CHARACTER, 18, 574, 82, 24);
        AddPanelControl(hWnd, "BUTTON", "Weapons", BS_PUSHBUTTON, IDC_LP_RANDOM_WEAPONS, 104, 574, 82, 24);
        AddPanelControl(hWnd, "BUTTON", "Armor", BS_PUSHBUTTON, IDC_LP_RANDOM_ARMOR, 190, 574, 82, 24);
        AddPanelControl(hWnd, "BUTTON", "Action", BS_PUSHBUTTON, IDC_LP_RANDOM_ACTION, 276, 574, 82, 24);
        AddPanelControl(hWnd, "BUTTON", "All", BS_PUSHBUTTON, IDC_LP_RANDOM_ALL, 362, 574, 86, 24);

        AddPanelControl(hWnd, "BUTTON", "Preset", BS_GROUPBOX, -1, 8, 612, 448, 54);
        AddPanelControl(hWnd, "BUTTON", "Load", BS_PUSHBUTTON, IDC_LP_LOAD, 304, 634, 68, 24);
        AddPanelControl(hWnd, "BUTTON", "Save", BS_PUSHBUTTON, IDC_LP_SAVE, 380, 634, 68, 24);

        SyncLowPolyControlsFromState();
        return 0;

    case WM_COMMAND:
        {
            const WORD id = LOWORD(wParam);
            const WORD code = HIWORD(wParam);
            if (id == IDC_LP_LOAD && code == BN_CLICKED)
            {
                LoadLowPolyPreset();
                return 0;
            }
            if (id == IDC_LP_SAVE && code == BN_CLICKED)
            {
                SaveLowPolyPreset();
                return 0;
            }
            if (id == IDC_LP_RANDOM_CHARACTER && code == BN_CLICKED)
            {
                RandomizeLowPolyCharacterSection();
                return 0;
            }
            if (id == IDC_LP_RANDOM_WEAPONS && code == BN_CLICKED)
            {
                RandomizeLowPolyWeaponsSection();
                return 0;
            }
            if (id == IDC_LP_RANDOM_ARMOR && code == BN_CLICKED)
            {
                RandomizeLowPolyArmorSection();
                return 0;
            }
            if (id == IDC_LP_RANDOM_ACTION && code == BN_CLICKED)
            {
                RandomizeLowPolyActionSection();
                return 0;
            }
            if (id == IDC_LP_RANDOM_ALL && code == BN_CLICKED)
            {
                RandomizeLowPolyAll();
                return 0;
            }
            if (id == IDC_LP_PLAY && code == BN_CLICKED)
            {
                PullLowPolyStateFromControls();
                g_playerEquip.animationPlaying = true;
                ReloadPlayerModelFromControls();
                return 0;
            }
            if (id == IDC_LP_STOP && code == BN_CLICKED)
            {
                g_playerEquip.animationPlaying = false;
                if (g_pPlayerModel)
                    g_pPlayerModel->RestoreBindPose(g_pDevice);
                return 0;
            }
            if (id == IDC_LP_RESET && code == BN_CLICKED)
            {
                g_playerEquip.animationPlaying = false;
                g_playerAnimTime = 0.0f;
                if (g_pPlayerModel)
                    g_pPlayerModel->RestoreBindPose(g_pDevice);
                return 0;
            }
            if (id == IDC_LP_ANIM_MODE && code == CBN_SELCHANGE)
            {
                PullLowPolyStateFromControls();
                FillLowPolyAnimationCombo(LowPolyPanelControl(IDC_LP_ANIM_BANK),
                    -1, g_playerEquip.raceIndex, g_playerEquip.animationMode);
                g_playerEquip.animationBank = ComboGetSelectedData(LowPolyPanelControl(IDC_LP_ANIM_BANK));
                ReloadPlayerModelFromControls();
                return 0;
            }
            if (code == CBN_SELCHANGE)
            {
                PullLowPolyStateFromControls();
                ReloadPlayerModelFromControls();
                return 0;
            }
        }
        break;

    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        if (g_hLowPolyPanel == hWnd)
            g_hLowPolyPanel = NULL;
        return 0;
    }

    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

static LRESULT CALLBACK HighPolyCreationPanelProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        AddPanelControl(hWnd, "BUTTON", "Character Creation", BS_GROUPBOX, -1, 8, 8, 344, 236);
        AddPanelControl(hWnd, "STATIC", "Name:", 0, -1, 20, 36, 82, 18);
        AddPanelControl(hWnd, "EDIT", g_creationCharacterName,
                        WS_TABSTOP | ES_AUTOHSCROLL | WS_BORDER, IDC_HP_NAME, 108, 32, 232, 22);
        AddPanelControl(hWnd, "STATIC", "Race:", 0, -1, 20, 76, 82, 18);
        AddPanelCombo(hWnd, IDC_HP_RACE, 108, 72, 232);
        AddPanelControl(hWnd, "STATIC", "Face:", 0, -1, 20, 116, 82, 18);
        AddPanelCombo(hWnd, IDC_HP_FACE, 108, 112, 232);
        AddPanelControl(hWnd, "STATIC", "Equipment:", 0, -1, 20, 156, 82, 18);
        AddPanelCombo(hWnd, IDC_HP_EQUIPMENT, 108, 152, 232);
        AddPanelControl(hWnd, "STATIC", "Animation:", 0, -1, 20, 196, 82, 18);
        AddPanelCombo(hWnd, IDC_HP_ANIMATION, 108, 192, 232);
        AddPanelControl(hWnd, "BUTTON", "Save Character...", BS_PUSHBUTTON,
                        IDC_HP_SAVE, 66, 254, 132, 26);
        AddPanelControl(hWnd, "BUTTON", "Choose Nation...", BS_PUSHBUTTON,
                        IDC_HP_NEXT, 208, 254, 132, 26);
        AddPanelControl(hWnd, "BUTTON", "Return to Title", BS_PUSHBUTTON,
                        IDC_HP_TITLE, 66, 286, 274, 26);
        SyncHighPolyCreationControlsFromState();
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_HP_TITLE && HIWORD(wParam) == BN_CLICKED)
        {
            ReturnToTitleScreen();
            return 0;
        }
        if (LOWORD(wParam) == IDC_HP_NEXT && HIWORD(wParam) == BN_CLICKED)
        {
            BeginNationSelectScene();
            return 0;
        }
        if (LOWORD(wParam) == IDC_HP_SAVE && HIWORD(wParam) == BN_CLICKED)
        {
            SaveHighPolyCreationCharacter();
            return 0;
        }
        if (HIWORD(wParam) == CBN_SELCHANGE)
        {
            ReloadHighPolyCreationFromControls();
            return 0;
        }
        break;

    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        if (g_hHighPolyCreationPanel == hWnd)
            g_hHighPolyCreationPanel = NULL;
        return 0;
    }

    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

static void ShowLowPolyControlPanel()
{
    if (g_hLowPolyPanel)
    {
        ShowWindow(g_hLowPolyPanel, SW_SHOW);
        SetForegroundWindow(g_hLowPolyPanel);
        return;
    }

    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = LowPolyPanelProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "DATuraLowPolyPanelClass";
    RegisterClassExA(&wc);

    g_hLowPolyPanel = CreateWindowExA(WS_EX_TOOLWINDOW, wc.lpszClassName,
        "Low Poly Character", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 482, 704, g_hWnd, NULL, wc.hInstance, NULL);
    if (g_hLowPolyPanel)
    {
        SyncLowPolyControlsFromState();
        ShowWindow(g_hLowPolyPanel, SW_SHOW);
    }
}

static void ShowHighPolyCreationPanel()
{
    if (g_hHighPolyCreationPanel)
    {
        SyncHighPolyCreationControlsFromState();
        ShowWindow(g_hHighPolyCreationPanel, SW_SHOW);
        SetForegroundWindow(g_hHighPolyCreationPanel);
        return;
    }

    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = HighPolyCreationPanelProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "DATuraHighPolyCreationPanelClass";
    RegisterClassExA(&wc);

    g_hHighPolyCreationPanel = CreateWindowExA(WS_EX_TOOLWINDOW, wc.lpszClassName,
        "High Poly Character Creation", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 378, 374, g_hWnd, NULL, wc.hInstance, NULL);
    if (g_hHighPolyCreationPanel)
    {
        SyncHighPolyCreationControlsFromState();
        ShowWindow(g_hHighPolyCreationPanel, SW_SHOW);
    }
}

static void BeginHighPolyCreationScene()
{
    if (g_hLowPolyPanel)
        ShowWindow(g_hLowPolyPanel, SW_HIDE);

    g_nationSelectActive = false;
    g_creationRaceIndex = 0;
    g_creationFaceIndex = 0;
    g_creationEquipmentIndex = 0;

    const FFXICreationEntry *pEntry = CurrentHighPolyCreationEntry();
    if (pEntry)
        LoadCreationEntry(pEntry);
    ShowHighPolyCreationPanel();
}

static void SetZoneObjectHidden(const char *name, bool hidden)
{
    if (!name || !name[0])
        return;
    std::vector<std::string>::iterator it =
        std::find(g_hiddenZoneObjects.begin(), g_hiddenZoneObjects.end(), name);
    if (hidden)
    {
        if (it == g_hiddenZoneObjects.end())
            g_hiddenZoneObjects.push_back(name);
    }
    else if (it != g_hiddenZoneObjects.end())
    {
        g_hiddenZoneObjects.erase(it);
    }
}

static bool ZoneObjectHidden(const char *name)
{
    if (!name || !name[0])
        return false;
    return std::find(g_hiddenZoneObjects.begin(), g_hiddenZoneObjects.end(), name) != g_hiddenZoneObjects.end();
}

static bool ZoneObjectIsUnreferenced(int index)
{
    const char *displayName = Model_FF11_GetLastMapObjectDisplayName(index);
    return displayName && strncmp(displayName, "env:", 4) == 0;
}

static void EnsureZoneObjectBrushes()
{
    if (!g_hZonePanelBrush)
        g_hZonePanelBrush = CreateSolidBrush(kZonePanelColor);
    if (!g_hZoneControlBrush)
        g_hZoneControlBrush = CreateSolidBrush(kZoneControlColor);
    if (!g_hZoneEditBrush)
        g_hZoneEditBrush = CreateSolidBrush(kZoneEditColor);
}

static void ApplyZoneListColors(HWND hList, bool withCheckboxes)
{
    if (!hList)
        return;

    ListView_SetBkColor(hList, kZoneControlColor);
    ListView_SetTextBkColor(hList, kZoneControlColor);
    ListView_SetTextColor(hList, kZoneTextColor);
    DWORD exStyle = LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER;
    if (withCheckboxes)
        exStyle |= LVS_EX_CHECKBOXES;
    ListView_SetExtendedListViewStyle(hList, exStyle);
}

static void ApplyZoneTreeColors(HWND hTree)
{
    if (!hTree)
        return;

    TreeView_SetBkColor(hTree, kZoneControlColor);
    TreeView_SetTextColor(hTree, kZoneTextColor);
}

static HWND GetZoneObjectListById(UINT id)
{
    if (id == IDC_ZONE_UNREF_OBJECT_LIST)
        return g_hZoneUnrefObjectList;
    if (id == IDC_ZONE_COLLISION_OBJECT_LIST)
        return g_hZoneCollisionObjectList;
    return g_hZoneObjectList;
}

static HWND GetActiveZoneObjectList()
{
    if (g_hZoneObjectList && ListView_GetNextItem(g_hZoneObjectList, -1, LVNI_SELECTED) >= 0)
        return g_hZoneObjectList;
    if (g_hZoneUnrefObjectList && ListView_GetNextItem(g_hZoneUnrefObjectList, -1, LVNI_SELECTED) >= 0)
        return g_hZoneUnrefObjectList;
    return g_hZoneObjectList ? g_hZoneObjectList : g_hZoneUnrefObjectList;
}

static void ClearZoneObjectColumns(HWND hList)
{
    if (!hList)
        return;
    while (Header_GetItemCount(ListView_GetHeader(hList)) > 0)
        ListView_DeleteColumn(hList, 0);
}

static void AddZoneObjectColumn(HWND hList, int index, const char *text, int width)
{
    if (!hList)
        return;
    LVCOLUMNA col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    col.pszText = const_cast<char *>(text);
    col.cx = width;
    col.iSubItem = index;
    SendMessageA(hList, LVM_INSERTCOLUMNA, (WPARAM)index, (LPARAM)&col);
}

static void SetupZoneObjectColumns(HWND hList, int &columnMode, int mode)
{
    if (!hList || columnMode == mode)
        return;

    ClearZoneObjectColumns(hList);
    AddZoneObjectColumn(hList, 0, "Vis", 30);
    if (mode == 0)
    {
        AddZoneObjectColumn(hList, 1, "Map Record", 82);
        AddZoneObjectColumn(hList, 2, "MapGeo Name", 118);
    }
    else
    {
        AddZoneObjectColumn(hList, 1, "MapGeo Chunk", 92);
        AddZoneObjectColumn(hList, 2, "MapGeo Name", 118);
    }
    AddZoneObjectColumn(hList, 3, "Position", 105);
    AddZoneObjectColumn(hList, 4, "Rotation", 105);
    AddZoneObjectColumn(hList, 5, "Scale", 105);
    AddZoneObjectColumn(hList, 6, "data2[0], data2[4]", 145);
    AddZoneObjectColumn(hList, 7, "Extra Vec", 130);
    AddZoneObjectColumn(hList, 8, "Raw data2[0..7]", 230);
    columnMode = mode;
}

static void SetupCollisionObjectColumns(HWND hList, int &columnMode, int mode)
{
    if (!hList || columnMode == mode)
        return;

    ClearZoneObjectColumns(hList);
    AddZoneObjectColumn(hList, 0, "Grid Entry", 150);
    AddZoneObjectColumn(hList, 1, "Grid Cell", 70);
    AddZoneObjectColumn(hList, 2, "Offsets", 116);
    AddZoneObjectColumn(hList, 3, "Tris", 46);
    AddZoneObjectColumn(hList, 4, "Bounds Min", 110);
    AddZoneObjectColumn(hList, 5, "Bounds Max", 110);
    columnMode = mode;
}

static void SetupDrawBatchColumns(HWND hList, int &columnMode, int mode)
{
    if (!hList || columnMode == mode)
        return;

    ClearZoneObjectColumns(hList);
    AddZoneObjectColumn(hList, 0, "Batch", 165);
    AddZoneObjectColumn(hList, 1, "Source", 105);
    AddZoneObjectColumn(hList, 2, "Material", 115);
    AddZoneObjectColumn(hList, 3, "Mode", 62);
    AddZoneObjectColumn(hList, 4, "Verts/Idx", 76);
    AddZoneObjectColumn(hList, 5, "Stride", 52);
    AddZoneObjectColumn(hList, 6, "Render", 92);
    AddZoneObjectColumn(hList, 7, "Flags", 230);
    AddZoneObjectColumn(hList, 8, "Super Bounds", 190);
    AddZoneObjectColumn(hList, 9, "Sub Bounds", 190);
    columnMode = mode;
}

static void SetZoneObjectSubItem(HWND hList, int row, int col, const char *text)
{
    if (!hList)
        return;
    LVITEMA item = {};
    item.mask = LVIF_TEXT;
    item.iItem = row;
    item.iSubItem = col;
    item.pszText = const_cast<char *>(text);
    SendMessageA(hList, LVM_SETITEMA, 0, (LPARAM)&item);
}

static void InsertZoneObjectRow(HWND hList, int mapObjectIndex)
{
    if (!hList)
        return;
    const ff11MapObjectDebug_t *debugObj =
        (mapObjectIndex >= 0 && mapObjectIndex < (int)gFF11LastMapObjects.size()) ?
        &gFF11LastMapObjects[mapObjectIndex] : NULL;
    const char *displayName = Model_FF11_GetLastMapObjectDisplayName(mapObjectIndex);
    const char *objectName = debugObj ? debugObj->objectName : (displayName ? displayName : "");
    float trans[3] = {};
    float rot[3] = {};
    float scale[3] = {};
    Model_FF11_GetLastMapObjectTransform(mapObjectIndex, trans, scale, rot);

    std::map<std::string, ZoneDebugTransform>::const_iterator overrideIt =
        g_zoneObjectOverrides.find(displayName ? displayName : "");
    if (overrideIt != g_zoneObjectOverrides.end())
    {
        memcpy(trans, overrideIt->second.trans, sizeof(trans));
        memcpy(rot, overrideIt->second.rot, sizeof(rot));
        memcpy(scale, overrideIt->second.scale, sizeof(scale));
    }

    LVITEMA item = {};
    item.mask = LVIF_TEXT | LVIF_PARAM;
    item.iItem = ListView_GetItemCount(hList);
    item.iSubItem = 0;
    item.pszText = const_cast<char *>("");
    item.lParam = mapObjectIndex;
    const int row = (int)SendMessageA(hList, LVM_INSERTITEMA, 0, (LPARAM)&item);
    ListView_SetCheckState(hList, row, !ZoneObjectHidden(displayName));

    char buf[128];
    if (debugObj && debugObj->referencedByMap)
        sprintf_s(buf, "%03d", debugObj->mapRecordIndex);
    else if (debugObj && debugObj->mapGeoIndex >= 0)
        sprintf_s(buf, "%03d", debugObj->mapGeoIndex);
    else
        strcpy_s(buf, "n/a");
    SetZoneObjectSubItem(hList, row, 1, buf);
    SetZoneObjectSubItem(hList, row, 2, objectName);
    sprintf_s(buf, "%.2f, %.2f, %.2f", trans[0], trans[1], trans[2]);
    SetZoneObjectSubItem(hList, row, 3, buf);
    sprintf_s(buf, "%.3f, %.3f, %.3f", rot[0], rot[1], rot[2]);
    SetZoneObjectSubItem(hList, row, 4, buf);
    sprintf_s(buf, "%.3f, %.3f, %.3f", scale[0], scale[1], scale[2]);
    SetZoneObjectSubItem(hList, row, 5, buf);
    if (debugObj && debugObj->referencedByMap)
        sprintf_s(buf, "%08X, %08X", debugObj->objectFlags[0], debugObj->objectFlags[1]);
    else
        strcpy_s(buf, "n/a");
    SetZoneObjectSubItem(hList, row, 6, buf);
    if (debugObj && debugObj->referencedByMap)
        sprintf_s(buf, "%.2f, %.2f, %.2f, %.2f",
                  debugObj->vec[0], debugObj->vec[1], debugObj->vec[2], debugObj->vec[3]);
    else
        strcpy_s(buf, "n/a");
    SetZoneObjectSubItem(hList, row, 7, buf);
    if (debugObj && debugObj->referencedByMap)
        sprintf_s(buf, "%08X %08X %08X %08X %08X %08X %08X %08X",
                  debugObj->data2[0], debugObj->data2[1], debugObj->data2[2], debugObj->data2[3],
                  debugObj->data2[4], debugObj->data2[5], debugObj->data2[6], debugObj->data2[7]);
    else
        strcpy_s(buf, "n/a");
    SetZoneObjectSubItem(hList, row, 8, buf);
}

static void InsertCollisionObjectRow(HWND hList, int collisionMeshIndex)
{
    if (!hList)
        return;

    const ff11CollisionMeshDebug_t *mesh = Model_FF11_GetLastCollisionMesh(collisionMeshIndex);
    if (!mesh)
        return;

    LVITEMA item = {};
    item.mask = LVIF_TEXT | LVIF_PARAM;
    item.iItem = ListView_GetItemCount(hList);
    item.iSubItem = 0;
    item.pszText = const_cast<char *>(mesh->displayName);
    item.lParam = collisionMeshIndex;
    const int row = (int)SendMessageA(hList, LVM_INSERTITEMA, 0, (LPARAM)&item);

    char buf[96];
    sprintf_s(buf, "%d,%d", mesh->gridX, mesh->gridY);
    SetZoneObjectSubItem(hList, row, 1, buf);
    sprintf_s(buf, "T:%08X G:%08X", mesh->transformOfs, mesh->geometryOfs);
    SetZoneObjectSubItem(hList, row, 2, buf);
    sprintf_s(buf, "%d", mesh->triCount);
    SetZoneObjectSubItem(hList, row, 3, buf);
    sprintf_s(buf, "%.2f, %.2f, %.2f", mesh->boundsMin[0], mesh->boundsMin[1], mesh->boundsMin[2]);
    SetZoneObjectSubItem(hList, row, 4, buf);
    sprintf_s(buf, "%.2f, %.2f, %.2f", mesh->boundsMax[0], mesh->boundsMax[1], mesh->boundsMax[2]);
    SetZoneObjectSubItem(hList, row, 5, buf);
}

static void InsertDrawBatchRow(HWND hList, int batchIndex)
{
    if (!hList)
        return;

    const ff11MapGeoDrawBatchDebug_t *batch = Model_FF11_GetLastMapGeoDrawBatch(batchIndex);
    if (!batch)
        return;

    LVITEMA item = {};
    item.mask = LVIF_TEXT | LVIF_PARAM;
    item.iItem = ListView_GetItemCount(hList);
    item.iSubItem = 0;
    item.pszText = const_cast<char *>(batch->displayName);
    item.lParam = batchIndex;
    const int row = (int)SendMessageA(hList, LVM_INSERTITEMA, 0, (LPARAM)&item);

    char buf[256];
    if (batch->mapRecordIndex >= 0)
        sprintf_s(buf, "Map %03d / Geo %03d", batch->mapRecordIndex, batch->mapGeoIndex);
    else
        sprintf_s(buf, "Geo %03d", batch->mapGeoIndex);
    SetZoneObjectSubItem(hList, row, 1, buf);
    SetZoneObjectSubItem(hList, row, 2, batch->materialName);
    SetZoneObjectSubItem(hList, row, 3, batch->indexMode == 0 ? "Tri List" : "Tri Strip");
    sprintf_s(buf, "%d / %d", batch->vertexCount, batch->indexCount);
    SetZoneObjectSubItem(hList, row, 4, buf);
    sprintf_s(buf, "%d", batch->vertexStride);
    SetZoneObjectSubItem(hList, row, 5, buf);
    SetZoneObjectSubItem(hList, row, 6, batch->daturaRenderMode);
    sprintf_s(buf, "flags:%04X flags2:%04X super:%d sub:%d obj:%08X,%08X alpha:%d cullOff:%d u4:%d u1:%d ofs:%08X",
              batch->blendFlags, batch->flags2, batch->superFlag, batch->subFlag,
              batch->objectFlags[0], batch->objectFlags[1],
              batch->galkaReeveWouldAlphaBlend ? 1 : 0,
              (batch->blendFlags & 0x2000u) ? 1 : 0,
              batch->runtimeFlag4000 ? 1 : 0,
              batch->runtimeFlag1000 ? 1 : 0,
              batch->drawOffset);
    SetZoneObjectSubItem(hList, row, 7, buf);
    sprintf_s(buf, "%.2f, %.2f, %.2f / %.2f, %.2f, %.2f",
              batch->superBounds[0], batch->superBounds[1], batch->superBounds[2],
              batch->superBounds[3], batch->superBounds[4], batch->superBounds[5]);
    SetZoneObjectSubItem(hList, row, 8, buf);
    sprintf_s(buf, "%.2f, %.2f, %.2f / %.2f, %.2f, %.2f",
              batch->subBounds[0], batch->subBounds[1], batch->subBounds[2],
              batch->subBounds[3], batch->subBounds[4], batch->subBounds[5]);
    SetZoneObjectSubItem(hList, row, 9, buf);
}

static HTREEITEM AddZoneTreeItem(HWND hTree, HTREEITEM parent, const char *text, LPARAM param = 0)
{
    if (!hTree || !text)
        return NULL;

    TVINSERTSTRUCTA tvi = {};
    tvi.hParent = parent ? parent : TVI_ROOT;
    tvi.hInsertAfter = TVI_LAST;
    tvi.item.mask = TVIF_TEXT | TVIF_PARAM;
    tvi.item.pszText = const_cast<char *>(text);
    tvi.item.lParam = param;
    return (HTREEITEM)SendMessageA(hTree, TVM_INSERTITEMA, 0, (LPARAM)&tvi);
}

static bool IsZoneDataTreeId(UINT id)
{
    return id == IDC_ZONE_DATA_TREE ||
           id == IDC_ZONE_RAW_DATA_TREE ||
           id == IDC_ZONE_COLLISION_DATA_TREE;
}

enum ZoneTreeNodeType
{
    kZoneTreeNode_None = 0,
    kZoneTreeNode_MapObject = 1,
    kZoneTreeNode_DrawBatch = 2,
    kZoneTreeNode_CollisionMesh = 3,
};

static LPARAM MakeZoneTreeParam(int type, int index)
{
    return ((LPARAM)type << 24) | (LPARAM)(index + 1);
}

static int GetZoneTreeParamType(LPARAM param)
{
    return (int)((param >> 24) & 0xFF);
}

static int GetZoneTreeParamIndex(LPARAM param)
{
    return (int)(param & 0x00FFFFFF) - 1;
}

static void AddZoneTreePlaceholder(HWND hTree, HTREEITEM parent)
{
    AddZoneTreeItem(hTree, parent, "...");
}

static bool ZoneTreeNodeHasPlaceholder(HWND hTree, HTREEITEM item)
{
    HTREEITEM child = TreeView_GetChild(hTree, item);
    if (!child)
        return false;

    char text[16] = {};
    TVITEMA tvItem = {};
    tvItem.mask = TVIF_TEXT;
    tvItem.hItem = child;
    tvItem.pszText = text;
    tvItem.cchTextMax = sizeof(text);
    if (!SendMessageA(hTree, TVM_GETITEMA, 0, (LPARAM)&tvItem))
        return false;
    return strcmp(text, "...") == 0;
}

static void ZoneTreeDeleteChildren(HWND hTree, HTREEITEM item)
{
    HTREEITEM child = TreeView_GetChild(hTree, item);
    while (child)
    {
        TreeView_DeleteItem(hTree, child);
        child = TreeView_GetChild(hTree, item);
    }
}

static void AddZoneTreeField(HWND hTree, HTREEITEM parent, const char *name, const char *value)
{
    char text[512] = {};
    sprintf_s(text, "%s: %s", name ? name : "", value ? value : "");
    AddZoneTreeItem(hTree, parent, text);
}

static void AddZoneTreeIntField(HWND hTree, HTREEITEM parent, const char *name, int value)
{
    char valueText[64] = {};
    sprintf_s(valueText, "%d", value);
    AddZoneTreeField(hTree, parent, name, valueText);
}

static void AddZoneTreeHexField(HWND hTree, HTREEITEM parent, const char *name, unsigned int value)
{
    char valueText[64] = {};
    sprintf_s(valueText, "0x%08X", value);
    AddZoneTreeField(hTree, parent, name, valueText);
}

static void AddZoneTreeHex16Field(HWND hTree, HTREEITEM parent, const char *name, unsigned int value)
{
    char valueText[64] = {};
    sprintf_s(valueText, "0x%04X", value & 0xFFFF);
    AddZoneTreeField(hTree, parent, name, valueText);
}

static void AddZoneTreeBytesField(HWND hTree, HTREEITEM parent, const char *name, const unsigned char *bytes, int count)
{
    char valueText[128] = {};
    char *dst = valueText;
    size_t remaining = sizeof(valueText);
    for (int i = 0; i < count && remaining > 1; ++i)
    {
        const int written = sprintf_s(dst, remaining, "%s%02X", i ? " " : "", bytes[i]);
        if (written <= 0)
            break;
        dst += written;
        remaining -= written;
    }
    AddZoneTreeField(hTree, parent, name, valueText);
}

static void AddZoneTreeVec3Field(HWND hTree, HTREEITEM parent, const char *name, const float v[3])
{
    char valueText[128] = {};
    sprintf_s(valueText, "%.6f, %.6f, %.6f", v[0], v[1], v[2]);
    AddZoneTreeField(hTree, parent, name, valueText);
}

static void AddZoneTreeVec4Field(HWND hTree, HTREEITEM parent, const char *name, const float v[4])
{
    char valueText[160] = {};
    sprintf_s(valueText, "%.6f, %.6f, %.6f, %.6f", v[0], v[1], v[2], v[3]);
    AddZoneTreeField(hTree, parent, name, valueText);
}

static void AddZoneTreeBoundsField(HWND hTree, HTREEITEM parent, const char *name, const float bounds[6])
{
    char valueText[256] = {};
    sprintf_s(valueText, "min %.6f, %.6f, %.6f / max %.6f, %.6f, %.6f",
              bounds[0], bounds[1], bounds[2], bounds[3], bounds[4], bounds[5]);
    AddZoneTreeField(hTree, parent, name, valueText);
}

static void AddZoneTreeDrawBatchesForMapObject(HWND hTree, HTREEITEM parent, const ff11MapObjectDebug_t &obj);

static void PopulateZoneTreeMapObjectFields(HWND hTree, HTREEITEM item, int index)
{
    if (index < 0 || index >= Model_FF11_GetLastMapObjectCount())
        return;

    const ff11MapObjectDebug_t &obj = gFF11LastMapObjects[index];
    AddZoneTreeField(hTree, item, "MapGeo name", obj.objectName);
    if (obj.referencedByMap)
    {
        AddZoneTreeIntField(hTree, item, "Map record index", obj.mapRecordIndex);
        AddZoneTreeHexField(hTree, item, "Object flags 0 (map data2[0])", obj.objectFlags[0]);
        AddZoneTreeHexField(hTree, item, "Object flags 1 (map data2[4])", obj.objectFlags[1]);
        HTREEITEM data2Root = AddZoneTreeItem(hTree, item, "Raw map object data2[0..7]");
        for (int i = 0; i < 8; ++i)
        {
            char label[64] = {};
            sprintf_s(label, "data2[%d]%s", i,
                      (i == 0) ? " - object flags 0" : ((i == 4) ? " - object flags 1" : " - unknown"));
            AddZoneTreeHexField(hTree, data2Root, label, obj.data2[i]);
        }
        AddZoneTreeVec4Field(hTree, item, "Extra vector", obj.vec);
    }
    else
    {
        AddZoneTreeIntField(hTree, item, "MapGeo chunk index", obj.mapGeoIndex);
    }
    AddZoneTreeVec3Field(hTree, item, "Position", obj.trans);
    AddZoneTreeVec3Field(hTree, item, "Rotation", obj.rot);
    AddZoneTreeVec3Field(hTree, item, "Scale", obj.scale);
    AddZoneTreeField(hTree, item, "Visible", ZoneObjectHidden(obj.displayName) ? "0" : "1");
    AddZoneTreeDrawBatchesForMapObject(hTree, item, obj);
}

static void PopulateZoneTreeDrawBatchFields(HWND hTree, HTREEITEM item, int index)
{
    const ff11MapGeoDrawBatchDebug_t *batch = Model_FF11_GetLastMapGeoDrawBatch(index);
    if (!batch)
        return;

    AddZoneTreeField(hTree, item, "MapGeo name", batch->objectName);
    AddZoneTreeBytesField(hTree, item, "MapGeo header bytes", batch->mapGeoHeaderData, 4);
    AddZoneTreeHexField(hTree, item, "MapGeo unknown1", batch->mapGeoUnknown1);
    AddZoneTreeField(hTree, item, "MapGeo unknown name/tag", batch->mapGeoUnknownName);
    AddZoneTreeIntField(hTree, item, "MapGeo index", batch->mapGeoIndex);
    AddZoneTreeIntField(hTree, item, "Map record index", batch->mapRecordIndex);
    AddZoneTreeIntField(hTree, item, "Super index", batch->superIndex);
    AddZoneTreeIntField(hTree, item, "Sub index", batch->subIndex);
    AddZoneTreeHexField(hTree, item, "Draw offset", (unsigned int)batch->drawOffset);
    AddZoneTreeField(hTree, item, "Material", batch->materialName);
    AddZoneTreeField(hTree, item, "Index mode", batch->indexMode == 0 ? "Triangle list" : "Triangle strip");
    AddZoneTreeIntField(hTree, item, "Vertex count", batch->vertexCount);
    AddZoneTreeIntField(hTree, item, "Index count", batch->indexCount);
    AddZoneTreeIntField(hTree, item, "Vertex stride", batch->vertexStride);
    AddZoneTreeHex16Field(hTree, item, "Runtime mesh flags", batch->blendFlags);
    AddZoneTreeHex16Field(hTree, item, "Uninterpreted runtime flag bits",
                         batch->blendFlags & ~(0x1000u | 0x2000u | 0x4000u | 0x8000u));
    AddZoneTreeHex16Field(hTree, item, "Flags 2", batch->flags2);
    AddZoneTreeField(hTree, item, "DATura render mode", batch->daturaRenderMode);
    AddZoneTreeField(hTree, item, "DATura render reason", batch->daturaRenderReason);
    AddZoneTreeField(hTree, item, "DATura hard alpha", batch->daturaHardAlpha ? "1" : "0");
    AddZoneTreeField(hTree, item, "DATura soft blend", batch->daturaSoftBlend ? "1" : "0");
    AddZoneTreeField(hTree, item, "Transparent (runtime flag 0x8000)",
                     batch->galkaReeveUseAlpha ? "1" : "0");
    AddZoneTreeField(hTree, item, "Disable culling (runtime flag 0x2000)",
                     (batch->blendFlags & 0x2000u) ? "1" : "0");
    AddZoneTreeField(hTree, item, "Unknown runtime flag 0x4000",
                     batch->runtimeFlag4000 ? "1" : "0");
    AddZoneTreeField(hTree, item, "Unknown runtime flag 0x1000",
                     batch->runtimeFlag1000 ? "1" : "0");
    AddZoneTreeField(hTree, item, "DATura applies alpha blending",
                     batch->galkaReeveWouldAlphaBlend ? "1" : "0");
    AddZoneTreeIntField(hTree, item, "Super flag", batch->superFlag);
    AddZoneTreeIntField(hTree, item, "Sub flag", batch->subFlag);
    AddZoneTreeHexField(hTree, item, "Object flags 0", batch->objectFlags[0]);
    AddZoneTreeHexField(hTree, item, "Object flags 1", batch->objectFlags[1]);
    AddZoneTreeBoundsField(hTree, item, "Super bounds", batch->superBounds);
    AddZoneTreeBoundsField(hTree, item, "Sub bounds", batch->subBounds);
}

static bool ZoneDrawBatchBelongsToMapObject(const ff11MapGeoDrawBatchDebug_t *batch, const ff11MapObjectDebug_t &obj)
{
    if (!batch)
        return false;
    if (obj.referencedByMap)
        return batch->mapRecordIndex == obj.mapRecordIndex;
    return batch->mapRecordIndex < 0 && batch->mapGeoIndex == obj.mapGeoIndex;
}

static void AddZoneTreeDrawBatchesForMapObject(HWND hTree, HTREEITEM parent, const ff11MapObjectDebug_t &obj)
{
    int count = 0;
    for (int i = 0; i < Model_FF11_GetLastMapGeoDrawBatchCount(); ++i)
    {
        if (ZoneDrawBatchBelongsToMapObject(Model_FF11_GetLastMapGeoDrawBatch(i), obj))
            ++count;
    }

    char text[96] = {};
    sprintf_s(text, "Draw batches: %d", count);
    HTREEITEM drawRoot = AddZoneTreeItem(hTree, parent, text);

    for (int i = 0; i < Model_FF11_GetLastMapGeoDrawBatchCount(); ++i)
    {
        const ff11MapGeoDrawBatchDebug_t *batch = Model_FF11_GetLastMapGeoDrawBatch(i);
        if (!ZoneDrawBatchBelongsToMapObject(batch, obj))
            continue;
        HTREEITEM batchItem = AddZoneTreeItem(hTree, drawRoot, batch->displayName,
                                             MakeZoneTreeParam(kZoneTreeNode_DrawBatch, i));
        AddZoneTreePlaceholder(hTree, batchItem);
    }
}

static void PopulateZoneTreeCollisionFields(HWND hTree, HTREEITEM item, int index)
{
    const ff11CollisionMeshDebug_t *mesh = Model_FF11_GetLastCollisionMesh(index);
    if (!mesh)
        return;

    char text[64] = {};
    sprintf_s(text, "%d, %d", mesh->gridX, mesh->gridY);
    AddZoneTreeField(hTree, item, "Grid cell", text);
    AddZoneTreeHexField(hTree, item, "Transform offset", (unsigned int)mesh->transformOfs);
    AddZoneTreeHexField(hTree, item, "Geometry offset", (unsigned int)mesh->geometryOfs);
    AddZoneTreeHexField(hTree, item, "Collision bucket flags", mesh->bucketFlags);
    AddZoneTreeHexField(hTree, item, "Observed 2-bit index-flag values", mesh->indexFlagValueMask);
    AddZoneTreeIntField(hTree, item, "Triangle start", mesh->triStart);
    AddZoneTreeIntField(hTree, item, "Triangle count", mesh->triCount);
    AddZoneTreeVec3Field(hTree, item, "Bounds min", mesh->boundsMin);
    AddZoneTreeVec3Field(hTree, item, "Bounds max", mesh->boundsMax);
}

static void PopulateExpandedZoneTreeNode(HWND hTree, HTREEITEM item, LPARAM param)
{
    if (!hTree || !item || !ZoneTreeNodeHasPlaceholder(hTree, item))
        return;

    ZoneTreeDeleteChildren(hTree, item);
    const int type = GetZoneTreeParamType(param);
    const int index = GetZoneTreeParamIndex(param);
    switch (type)
    {
    case kZoneTreeNode_MapObject:
        PopulateZoneTreeMapObjectFields(hTree, item, index);
        break;
    case kZoneTreeNode_DrawBatch:
        PopulateZoneTreeDrawBatchFields(hTree, item, index);
        break;
    case kZoneTreeNode_CollisionMesh:
        PopulateZoneTreeCollisionFields(hTree, item, index);
        break;
    default:
        break;
    }
}

static void PopulateZoneDataTree()
{
    if (!g_hZoneDataTree || !g_hZoneRawDataTree || !g_hZoneCollisionDataTree)
        return;

    SendMessageA(g_hZoneDataTree, WM_SETREDRAW, FALSE, 0);
    SendMessageA(g_hZoneRawDataTree, WM_SETREDRAW, FALSE, 0);
    SendMessageA(g_hZoneCollisionDataTree, WM_SETREDRAW, FALSE, 0);
    TreeView_DeleteAllItems(g_hZoneDataTree);
    TreeView_DeleteAllItems(g_hZoneRawDataTree);
    TreeView_DeleteAllItems(g_hZoneCollisionDataTree);

    char text[256] = {};
    HWND hPlacedTree = g_hZoneDataTree;
    HWND hRawTree = g_zoneCombinedObjectTree ? g_hZoneDataTree : g_hZoneRawDataTree;
    HWND hCollisionTree = g_zoneCombinedObjectTree ? g_hZoneDataTree : g_hZoneCollisionDataTree;
    HTREEITEM combinedRoot = NULL;
    if (g_zoneCombinedObjectTree)
    {
        combinedRoot = AddZoneTreeItem(g_hZoneDataTree, NULL, g_loadedZoneLabel);
        sprintf_s(text, "Counts: map records %d, MapGeo draw batches %d, collision grid entries %d",
                  ListView_GetItemCount(g_hZoneObjectList),
                  Model_FF11_GetLastMapGeoDrawBatchCount(),
                  Model_FF11_GetLastCollisionMeshCount());
        AddZoneTreeItem(g_hZoneDataTree, combinedRoot, text);
    }

    HTREEITEM placedRoot = AddZoneTreeItem(hPlacedTree, combinedRoot, "Placed Geometry");
    sprintf_s(text, "Map records: %d", ListView_GetItemCount(g_hZoneObjectList));
    AddZoneTreeItem(hPlacedTree, placedRoot, text);
    HTREEITEM mapRoot = AddZoneTreeItem(hPlacedTree, placedRoot, "Map chunk 0x1C");
    if (gFF11LastMapHeader.valid)
    {
        HTREEITEM mapHeaderRoot = AddZoneTreeItem(hPlacedTree, mapRoot, "Map header metadata");
        AddZoneTreeBytesField(hPlacedTree, mapHeaderRoot, "Header bytes", gFF11LastMapHeader.headerData, 4);
        AddZoneTreeHexField(hPlacedTree, mapHeaderRoot, "Object count field (24-bit)", gFF11LastMapHeader.objectCount24);
        AddZoneTreeHexField(hPlacedTree, mapHeaderRoot, "Unknown1 (upper 8 bits after object count)", gFF11LastMapHeader.unknown1);
        for (int i = 0; i < 6; ++i)
        {
            char label[64] = {};
            sprintf_s(label, "Unknown2[%d]", i);
            AddZoneTreeHexField(hPlacedTree, mapHeaderRoot, label, gFF11LastMapHeader.unknown2[i]);
        }
        AddZoneTreeHexField(hPlacedTree, mapHeaderRoot, "Object table offset", (unsigned int)gFF11LastMapHeader.objectTableOffset);
        AddZoneTreeHexField(hPlacedTree, mapHeaderRoot, "Object table end offset", (unsigned int)gFF11LastMapHeader.objectTableEndOffset);
        AddZoneTreeIntField(hPlacedTree, mapHeaderRoot, "Parsed object count", gFF11LastMapHeader.parsedObjectCount);
        AddZoneTreeHexField(hPlacedTree, mapHeaderRoot, "Trailing data offset", (unsigned int)gFF11LastMapHeader.trailingDataOffset);
        AddZoneTreeIntField(hPlacedTree, mapHeaderRoot, "Trailing data size (unknown visibility/partitioning data)", gFF11LastMapHeader.trailingDataSize);
    }
    HTREEITEM placementRoot = AddZoneTreeItem(hPlacedTree, mapRoot, "Placement records");

    HTREEITEM chunkRoot = AddZoneTreeItem(hRawTree, combinedRoot, "DAT Chunk Table");
    sprintf_s(text, "Parsed chunks: %d", (int)gFF11LastDatChunks.size());
    AddZoneTreeItem(hRawTree, chunkRoot, text);
    for (int i = 0; i < (int)gFF11LastDatChunks.size(); ++i)
    {
        const ff11DatChunkDebug_t &chunk = gFF11LastDatChunks[i];
        sprintf_s(text, "%03d: %s type 0x%02X %s", i, chunk.name, chunk.type,
                  chunk.supported ? "handled" : "unhandled");
        HTREEITEM chunkItem = AddZoneTreeItem(hRawTree, chunkRoot, text);
        AddZoneTreeField(hRawTree, chunkItem, "Chunk name/tag", chunk.name);
        AddZoneTreeHexField(hRawTree, chunkItem, "Chunk type", (unsigned int)chunk.type);
        HTREEITEM chunkFlagsRoot = AddZoneTreeItem(hRawTree, chunkItem, "Common resource-header flags");
        AddZoneTreeField(hRawTree, chunkFlagsRoot, "Shadow resource", chunk.isShadow ? "1" : "0");
        AddZoneTreeField(hRawTree, chunkFlagsRoot, "Extracted resource", chunk.isExtracted ? "1" : "0");
        AddZoneTreeIntField(hRawTree, chunkFlagsRoot, "Resource version", chunk.version);
        AddZoneTreeField(hRawTree, chunkFlagsRoot, "Virtual resource", chunk.isVirtual ? "1" : "0");
        if (chunk.directoryPath[0])
            AddZoneTreeField(hRawTree, chunkItem, "Directory path", chunk.directoryPath);
        if (chunk.isDirectoryOpen)
            AddZoneTreeField(hRawTree, chunkItem, "Directory command", "open");
        if (chunk.isDirectoryClose)
            AddZoneTreeField(hRawTree, chunkItem, "Directory command", "close");
        AddZoneTreeHexField(hRawTree, chunkItem, "Data offset", (unsigned int)chunk.dataOffset);
        AddZoneTreeHexField(hRawTree, chunkItem, "Total chunk size", (unsigned int)chunk.size);
        AddZoneTreeField(hRawTree, chunkItem, "Handled by DATura", chunk.supported ? "1" : "0");
        if (chunk.hasEnvironmentMetadata)
        {
            HTREEITEM envRoot = AddZoneTreeItem(hRawTree, chunkItem, "Environment/light record 0x2F");
            for (int wordIndex = 0; wordIndex < 8; ++wordIndex)
            {
                char label[64] = {};
                sprintf_s(label, "Header word %d", wordIndex);
                AddZoneTreeHex16Field(hRawTree, envRoot, label, chunk.environmentHeaderWords[wordIndex]);
            }
        }
        if (chunk.hasSoundPointer)
        {
            HTREEITEM soundRoot = AddZoneTreeItem(hRawTree, chunkItem, "Sound pointer 0x3D");
            AddZoneTreeIntField(hRawTree, soundRoot, "SPW sound id", (int)chunk.soundId);
            AddZoneTreeField(hRawTree, soundRoot, "SPW path", chunk.soundPath);
        }
        if (chunk.hasEffectMetadata)
        {
            const char *effectKind = chunk.type == 0x1F ? "Effect model 0x1F" :
                                     (chunk.type == 0x21 ? "Animated effect model 0x21" : "Morph effect model 0x25");
            HTREEITEM effectRoot = AddZoneTreeItem(hRawTree, chunkItem, effectKind);
            AddZoneTreeField(hRawTree, effectRoot, "Material/image tag", chunk.effectMaterialName);
            AddZoneTreeHex16Field(hRawTree, chunkItem, "Effect header word 0", chunk.effectHeaderWords[0]);
            AddZoneTreeHex16Field(hRawTree, chunkItem, "Effect header word 1", chunk.effectHeaderWords[1]);
            AddZoneTreeHex16Field(hRawTree, chunkItem, "Effect header word 2", chunk.effectHeaderWords[2]);
            AddZoneTreeHex16Field(hRawTree, chunkItem, "Effect header word 3", chunk.effectHeaderWords[3]);
            AddZoneTreeHex16Field(hRawTree, chunkItem, "Effect header word 4", chunk.effectHeaderWords[4]);
            AddZoneTreeHex16Field(hRawTree, chunkItem, "Effect header word 5", chunk.effectHeaderWords[5]);
            AddZoneTreeHex16Field(hRawTree, chunkItem, "Effect header word 6", chunk.effectHeaderWords[6]);
            AddZoneTreeHex16Field(hRawTree, chunkItem, "Effect header word 7", chunk.effectHeaderWords[7]);
            if (chunk.type == 0x25)
            {
                AddZoneTreeIntField(hRawTree, effectRoot, "Image count", chunk.effectHeaderWords[0]);
                AddZoneTreeIntField(hRawTree, effectRoot, "Morph count", chunk.effectHeaderWords[1]);
                AddZoneTreeIntField(hRawTree, effectRoot, "Base position count", chunk.effectHeaderWords[2]);
                AddZoneTreeIntField(hRawTree, effectRoot, "Secondary position count", chunk.effectHeaderWords[3]);
                AddZoneTreeHexField(hRawTree, effectRoot, "Index block offset", chunk.effectHeaderWords[4]);
                AddZoneTreeIntField(hRawTree, effectRoot, "Triangle count", chunk.effectHeaderWords[5]);
                AddZoneTreeHexField(hRawTree, effectRoot, "Per-corner color offset", chunk.effectHeaderWords[6]);
                AddZoneTreeHexField(hRawTree, effectRoot, "Per-corner UV offset", chunk.effectHeaderWords[7]);
            }
        }
        if (chunk.hasGeneratorMetadata)
        {
            HTREEITEM generatorRoot = AddZoneTreeItem(hRawTree, chunkItem, "Generator command stream 0x05");
            unsigned int sectionStart = 0x80;
            for (int sectionIndex = 0; sectionIndex < 4; ++sectionIndex)
            {
                char label[64] = {};
                sprintf_s(label, "Section %d range", sectionIndex + 1);
                char rangeText[64] = {};
                sprintf_s(rangeText, "0x%X..0x%X", sectionStart, chunk.generatorSectionOffsets[sectionIndex]);
                AddZoneTreeField(hRawTree, generatorRoot, label, rangeText);
                sectionStart = chunk.generatorSectionOffsets[sectionIndex];
            }
        }
        if (chunk.hasKeyframeMetadata)
        {
            HTREEITEM keyframeRoot = AddZoneTreeItem(hRawTree, chunkItem, "Keyframe pairs 0x19");
            AddZoneTreeIntField(hRawTree, keyframeRoot, "Float-pair count", chunk.keyframePairCount);
        }
    }

    HTREEITEM rawRoot = AddZoneTreeItem(hRawTree, combinedRoot, "Unreferenced Geometry");
    sprintf_s(text, "Unreferenced MapGeo: %d", ListView_GetItemCount(g_hZoneUnrefObjectList));
    AddZoneTreeItem(hRawTree, rawRoot, text);
    HTREEITEM unrefRoot = AddZoneTreeItem(hRawTree, rawRoot, "MapGeo chunks 0x2E not referenced by placement records");

    for (int i = 0; i < Model_FF11_GetLastMapObjectCount(); ++i)
    {
        const ff11MapObjectDebug_t &obj = gFF11LastMapObjects[i];
        HTREEITEM parent = obj.referencedByMap ? placementRoot : unrefRoot;
        if (obj.referencedByMap)
            sprintf_s(text, "Record %03d - %s", obj.mapRecordIndex, obj.objectName);
        else
            sprintf_s(text, "MapGeo chunk %03d - %s", obj.mapGeoIndex, obj.objectName);
        HWND hTree = obj.referencedByMap ? hPlacedTree : hRawTree;
        HTREEITEM item = AddZoneTreeItem(hTree, parent, text, MakeZoneTreeParam(kZoneTreeNode_MapObject, i));
        AddZoneTreePlaceholder(hTree, item);
    }

    HTREEITEM collisionRoot = AddZoneTreeItem(hCollisionTree, combinedRoot, "Collision Meshes");
    sprintf_s(text, "Collision grid entries: %d", Model_FF11_GetLastCollisionMeshCount());
    AddZoneTreeItem(hCollisionTree, collisionRoot, text);
    HTREEITEM collisionGridRoot = AddZoneTreeItem(hCollisionTree, collisionRoot, "Collision grid");
    for (int i = 0; i < Model_FF11_GetLastCollisionMeshCount(); ++i)
    {
        const ff11CollisionMeshDebug_t *mesh = Model_FF11_GetLastCollisionMesh(i);
        if (!mesh)
            continue;

        HTREEITEM item = AddZoneTreeItem(hCollisionTree, collisionGridRoot, mesh->displayName,
                                         MakeZoneTreeParam(kZoneTreeNode_CollisionMesh, i));
        AddZoneTreePlaceholder(hCollisionTree, item);
    }

    if (combinedRoot)
        TreeView_Expand(g_hZoneDataTree, combinedRoot, TVE_EXPAND);
    TreeView_Expand(hPlacedTree, placedRoot, TVE_EXPAND);
    TreeView_Expand(hPlacedTree, mapRoot, TVE_EXPAND);
    TreeView_Expand(hPlacedTree, placementRoot, TVE_EXPAND);
    TreeView_Expand(hRawTree, chunkRoot, TVE_EXPAND);
    TreeView_Expand(hRawTree, rawRoot, TVE_EXPAND);
    TreeView_Expand(hRawTree, unrefRoot, TVE_EXPAND);
    TreeView_Expand(hCollisionTree, collisionRoot, TVE_EXPAND);
    TreeView_Expand(hCollisionTree, collisionGridRoot, TVE_EXPAND);

    SendMessageA(g_hZoneDataTree, WM_SETREDRAW, TRUE, 0);
    SendMessageA(g_hZoneRawDataTree, WM_SETREDRAW, TRUE, 0);
    SendMessageA(g_hZoneCollisionDataTree, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_hZoneDataTree, NULL, TRUE);
    InvalidateRect(g_hZoneRawDataTree, NULL, TRUE);
    InvalidateRect(g_hZoneCollisionDataTree, NULL, TRUE);
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
    SetZoneObjectSubItem(g_hZoneObjectList, row, 1, value);
}

static int GetZoneListMapObjectIndex(HWND hList, int row)
{
    if (!hList)
        return -1;
    LVITEMA item = {};
    item.mask = LVIF_PARAM;
    item.iItem = row;
    if (!SendMessageA(hList, LVM_GETITEMA, 0, (LPARAM)&item))
        return -1;
    return (int)item.lParam;
}

static int GetSelectedZoneObjectIndex()
{
    if (g_zoneTreeSelectedMapObjectIndex >= 0)
        return g_zoneTreeSelectedMapObjectIndex;

    HWND hList = GetActiveZoneObjectList();
    if (!hList)
        return -1;
    const int row = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
    return (row >= 0) ? GetZoneListMapObjectIndex(hList, row) : -1;
}

static const char *GetSelectedZoneObjectName()
{
    const int index = GetSelectedZoneObjectIndex();
    return (index >= 0) ? Model_FF11_GetLastMapObjectDisplayName(index) : "";
}

static void SetZoneListRowCheckedByIndex(int mapObjectIndex, BOOL checked)
{
    HWND lists[2] = { g_hZoneObjectList, g_hZoneUnrefObjectList };
    for (int listIndex = 0; listIndex < 2; ++listIndex)
    {
        HWND hList = lists[listIndex];
        if (!hList)
            continue;
        const int rowCount = ListView_GetItemCount(hList);
        for (int row = 0; row < rowCount; ++row)
        {
            if (GetZoneListMapObjectIndex(hList, row) == mapObjectIndex)
            {
                ListView_SetCheckState(hList, row, checked);
                return;
            }
        }
    }
}

static void SetZoneListVisibility(HWND hList, bool visible)
{
    if (!hList)
        return;

    const int rowCount = ListView_GetItemCount(hList);
    g_populatingZoneObjectList = true;
    for (int row = 0; row < rowCount; ++row)
    {
        const int mapObjectIndex = GetZoneListMapObjectIndex(hList, row);
        SetZoneObjectHidden(Model_FF11_GetLastMapObjectDisplayName(mapObjectIndex), !visible);
        ListView_SetCheckState(hList, row, visible ? TRUE : FALSE);
    }
    g_populatingZoneObjectList = false;
}

static void SetZoneUnrefEditVisible(bool visible)
{
    const int cmd = visible ? SW_SHOW : SW_HIDE;
    for (int i = 0; i < 9; ++i)
    {
        if (g_hZoneUnrefLabels[i]) ShowWindow(g_hZoneUnrefLabels[i], cmd);
        if (g_hZoneUnrefEdits[i]) ShowWindow(g_hZoneUnrefEdits[i], cmd);
    }
    if (g_hZoneUnrefApplyButton) ShowWindow(g_hZoneUnrefApplyButton, cmd);
}

static void UpdateZoneObjectEditControlState()
{
    const BOOL enabled = IsEditMode() ? TRUE : FALSE;
    HWND editControls[] =
    {
        g_hZonePlacedShowAllButton,
        g_hZonePlacedHideAllButton,
        g_hZoneUnrefShowAllButton,
        g_hZoneUnrefHideAllButton,
        g_hZoneShowSelectedButton,
        g_hZoneHideSelectedButton,
        g_hZoneHighlightSelectedButton,
        g_hZoneCenterSelectedButton,
        g_hZoneCombineTreeButton,
        g_hZoneUnrefApplyButton,
        g_hZoneCollisionVisibleCheck
    };
    for (int i = 0; i < (int)(sizeof(editControls) / sizeof(editControls[0])); ++i)
    {
        if (editControls[i])
            EnableWindow(editControls[i], enabled);
    }
    for (int i = 0; i < 5; ++i)
    {
        if (g_hZoneToolButtons[i])
            EnableWindow(g_hZoneToolButtons[i], enabled);
    }
    for (int i = 0; i < 9; ++i)
    {
        if (g_hZoneUnrefEdits[i])
            EnableWindow(g_hZoneUnrefEdits[i], enabled);
    }
}

static void UpdateZoneUnrefEditFields()
{
    const int mapObjectIndex = GetSelectedZoneObjectIndex();
    if (mapObjectIndex < 0)
        return;

    const char *objectName = Model_FF11_GetLastMapObjectDisplayName(mapObjectIndex);
    float trans[3] = {};
    float rot[3] = {};
    float scale[3] = {};
    Model_FF11_GetLastMapObjectTransform(mapObjectIndex, trans, scale, rot);
    std::map<std::string, ZoneDebugTransform>::const_iterator it =
        g_zoneObjectOverrides.find(objectName ? objectName : "");
    if (it != g_zoneObjectOverrides.end())
    {
        memcpy(trans, it->second.trans, sizeof(trans));
        memcpy(rot, it->second.rot, sizeof(rot));
        memcpy(scale, it->second.scale, sizeof(scale));
    }

    char buf[32];
    sprintf_s(buf, "%.2f", trans[0]); SetWindowTextA(g_hZoneUnrefEdits[0], buf);
    sprintf_s(buf, "%.2f", trans[1]); SetWindowTextA(g_hZoneUnrefEdits[1], buf);
    sprintf_s(buf, "%.2f", trans[2]); SetWindowTextA(g_hZoneUnrefEdits[2], buf);
    sprintf_s(buf, "%.3f", rot[0]); SetWindowTextA(g_hZoneUnrefEdits[3], buf);
    sprintf_s(buf, "%.3f", rot[1]); SetWindowTextA(g_hZoneUnrefEdits[4], buf);
    sprintf_s(buf, "%.3f", rot[2]); SetWindowTextA(g_hZoneUnrefEdits[5], buf);
    sprintf_s(buf, "%.3f", scale[0]); SetWindowTextA(g_hZoneUnrefEdits[6], buf);
    sprintf_s(buf, "%.3f", scale[1]); SetWindowTextA(g_hZoneUnrefEdits[7], buf);
    sprintf_s(buf, "%.3f", scale[2]); SetWindowTextA(g_hZoneUnrefEdits[8], buf);
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
    SetupZoneObjectColumns(g_hZoneObjectList, g_zoneObjectColumnMode, 0);
    SetupZoneObjectColumns(g_hZoneUnrefObjectList, g_zoneUnrefObjectColumnMode, 1);
    SetupCollisionObjectColumns(g_hZoneCollisionObjectList, g_zoneCollisionObjectColumnMode, 0);
    SetupDrawBatchColumns(g_hZoneDrawBatchList, g_zoneDrawBatchColumnMode, 0);
    ApplyZoneListColors(g_hZoneObjectList, true);
    ApplyZoneListColors(g_hZoneUnrefObjectList, true);
    ApplyZoneListColors(g_hZoneCollisionObjectList, false);
    ApplyZoneListColors(g_hZoneDrawBatchList, false);
    ApplyZoneTreeColors(g_hZoneDataTree);
    ApplyZoneTreeColors(g_hZoneRawDataTree);
    ApplyZoneTreeColors(g_hZoneCollisionDataTree);

    if (g_hZoneLabel)
        SetWindowTextA(g_hZoneLabel, g_loadedZoneLabel);

    const int mapObjectCount = Model_FF11_GetLastMapObjectCount();
    for (int i = 0; i < mapObjectCount; ++i)
    {
        InsertZoneObjectRow(ZoneObjectIsUnreferenced(i) ? g_hZoneUnrefObjectList : g_hZoneObjectList, i);
    }
    const int collisionMeshCount = Model_FF11_GetLastCollisionMeshCount();
    for (int i = 0; i < collisionMeshCount; ++i)
    {
        InsertCollisionObjectRow(g_hZoneCollisionObjectList, i);
    }
    const int drawBatchCount = Model_FF11_GetLastMapGeoDrawBatchCount();
    for (int i = 0; i < drawBatchCount; ++i)
    {
        InsertDrawBatchRow(g_hZoneDrawBatchList, i);
    }
    PopulateZoneDataTree();

    g_populatingZoneObjectList = false;
    SetZoneUnrefEditVisible(true);
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
        const int meshCount = g_pZoneModel ? (int)g_pZoneModel->submeshes.size() : 0;
        const int materialCount = (g_pZoneModel && g_pZoneModel->pMatData) ? g_pZoneModel->pMatData->matCount : 0;
        const int textureCount = (g_pZoneModel && g_pZoneModel->pMatData) ? g_pZoneModel->pMatData->texCount : 0;
        const int boneCount = g_pZoneModel ? g_pZoneModel->boneCount : 0;
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
    UpdateZoneUnrefEditFields();
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
    if (!g_hZoneObjectPanel || !g_hZoneObjectList || !g_hZoneUnrefObjectList || !g_hZoneCollisionObjectList ||
        !g_hZoneDrawBatchList || !g_hZoneDataTree || !g_hZoneRawDataTree || !g_hZoneCollisionDataTree)
        return;

    if (g_hZoneLabel)
        SetWindowTextA(g_hZoneLabel, g_loadedZoneLabel);
    SetupZoneObjectColumns(g_hZoneObjectList, g_zoneObjectColumnMode, 0);
    SetupZoneObjectColumns(g_hZoneUnrefObjectList, g_zoneUnrefObjectColumnMode, 1);
    SetupCollisionObjectColumns(g_hZoneCollisionObjectList, g_zoneCollisionObjectColumnMode, 0);
    SetupDrawBatchColumns(g_hZoneDrawBatchList, g_zoneDrawBatchColumnMode, 0);
    ListView_DeleteAllItems(g_hZoneObjectList);
    ListView_DeleteAllItems(g_hZoneUnrefObjectList);
    ListView_DeleteAllItems(g_hZoneCollisionObjectList);
    ListView_DeleteAllItems(g_hZoneDrawBatchList);
    TreeView_DeleteAllItems(g_hZoneDataTree);
    TreeView_DeleteAllItems(g_hZoneRawDataTree);
    TreeView_DeleteAllItems(g_hZoneCollisionDataTree);
    if (g_hZoneLoadingLabel)
    {
        SetWindowTextA(g_hZoneLoadingLabel, "Loading Object List...");
        ShowWindow(g_hZoneLoadingLabel, SW_SHOW);
        BringWindowToTop(g_hZoneLoadingLabel);
    }
    UpdateWindow(g_hZoneObjectPanel);
    PostMessageA(g_hZoneObjectPanel, WM_ZONE_OBJECT_POPULATE, 0, 0);
}

static void ComputeZonePaneLayout(int clientW, int *placedX, int *placedW,
                                  int *rawX, int *rawW, int *collisionX, int *collisionW)
{
    const int margin = 8;
    const int gap = 10;
    int totalW = clientW - margin * 2 - gap * 2;
    if (totalW < 300)
        totalW = 300;

    int minPaneW = 220;
    if (minPaneW * 3 > totalW)
        minPaneW = totalW / 3;

    if (g_zonePaneWidth[0] <= 0 || g_zonePaneWidth[1] <= 0 || g_zonePaneWidth[2] <= 0)
    {
        g_zonePaneWidth[0] = totalW / 3;
        g_zonePaneWidth[1] = totalW / 3;
        g_zonePaneWidth[2] = totalW - g_zonePaneWidth[0] - g_zonePaneWidth[1];
    }

    g_zonePaneWidth[0] = std::max(minPaneW, g_zonePaneWidth[0]);
    g_zonePaneWidth[1] = std::max(minPaneW, g_zonePaneWidth[1]);
    g_zonePaneWidth[2] = std::max(minPaneW, g_zonePaneWidth[2]);

    int currentTotal = g_zonePaneWidth[0] + g_zonePaneWidth[1] + g_zonePaneWidth[2];
    if (currentTotal != totalW)
    {
        const int delta = totalW - currentTotal;
        g_zonePaneWidth[2] += delta;
        if (g_zonePaneWidth[2] < minPaneW)
        {
            const int needed = minPaneW - g_zonePaneWidth[2];
            g_zonePaneWidth[2] = minPaneW;
            g_zonePaneWidth[1] = std::max(minPaneW, g_zonePaneWidth[1] - needed);
        }
    }

    if (placedX) *placedX = margin;
    if (placedW) *placedW = g_zonePaneWidth[0];
    if (rawX) *rawX = margin + g_zonePaneWidth[0] + gap;
    if (rawW) *rawW = g_zonePaneWidth[1];
    if (collisionX) *collisionX = margin + g_zonePaneWidth[0] + gap + g_zonePaneWidth[1] + gap;
    if (collisionW) *collisionW = std::max(minPaneW, totalW - g_zonePaneWidth[0] - g_zonePaneWidth[1]);
}

static int HitZonePaneSplitter(int clientW, int x, int y)
{
    const int headerH = 56;
    const int bottomH = 230;
    RECT rc = {};
    if (g_hZoneObjectPanel)
        GetClientRect(g_hZoneObjectPanel, &rc);
    const int listBottom = (rc.bottom > 0) ? rc.bottom - bottomH : 0;
    if (y < headerH || y > listBottom)
        return 0;

    int placedX = 0, placedW = 0, rawX = 0, rawW = 0, collisionX = 0, collisionW = 0;
    ComputeZonePaneLayout(clientW, &placedX, &placedW, &rawX, &rawW, &collisionX, &collisionW);
    const int split1 = placedX + placedW + 5;
    const int split2 = rawX + rawW + 5;
    if (abs(x - split1) <= 6)
        return 1;
    if (abs(x - split2) <= 6)
        return 2;
    return 0;
}

static LRESULT CALLBACK ZoneObjectPanelProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        EnsureZoneObjectBrushes();
        g_hZoneLabel = CreateWindowExA(0, "STATIC", g_loadedZoneLabel,
            WS_CHILD | WS_VISIBLE,
            8, 8, 540, 20, hWnd, (HMENU)(INT_PTR)IDC_ZONE_LABEL,
            GetModuleHandle(NULL), NULL);
        g_hZonePlacedListLabel = CreateWindowExA(0, "STATIC", "Placed Geometry",
            WS_CHILD | WS_VISIBLE,
            8, 36, 320, 18, hWnd, NULL, GetModuleHandle(NULL), NULL);
        g_hZoneUnrefListLabel = CreateWindowExA(0, "STATIC", "Unreferenced Geometry / Draw Batches",
            WS_CHILD | WS_VISIBLE,
            8, 260, 360, 18, hWnd, NULL, GetModuleHandle(NULL), NULL);
        g_hZoneCollisionListLabel = CreateWindowExA(0, "STATIC", "Collision Meshes",
            WS_CHILD | WS_VISIBLE,
            8, 390, 220, 18, hWnd, NULL, GetModuleHandle(NULL), NULL);
        g_hZoneDrawBatchListLabel = CreateWindowExA(0, "STATIC", "MapGeo Draw Batches (materials, flags, vertex/index data)",
            WS_CHILD | WS_VISIBLE,
            8, 500, 440, 18, hWnd, NULL, GetModuleHandle(NULL), NULL);
        g_hZonePlacedShowAllButton = CreateWindowExA(0, "BUTTON", "Show Placed",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            8, 626, 90, 24, hWnd, (HMENU)(INT_PTR)IDC_ZONE_PLACED_SHOW_ALL,
            GetModuleHandle(NULL), NULL);
        g_hZonePlacedHideAllButton = CreateWindowExA(0, "BUTTON", "Hide Placed",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            104, 626, 90, 24, hWnd, (HMENU)(INT_PTR)IDC_ZONE_PLACED_HIDE_ALL,
            GetModuleHandle(NULL), NULL);
        g_hZoneUnrefShowAllButton = CreateWindowExA(0, "BUTTON", "Show Raw",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            608, 626, 90, 24, hWnd, (HMENU)(INT_PTR)IDC_ZONE_UNREF_SHOW_ALL,
            GetModuleHandle(NULL), NULL);
        g_hZoneUnrefHideAllButton = CreateWindowExA(0, "BUTTON", "Hide Raw",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            704, 626, 90, 24, hWnd, (HMENU)(INT_PTR)IDC_ZONE_UNREF_HIDE_ALL,
            GetModuleHandle(NULL), NULL);
        g_hZoneShowSelectedButton = CreateWindowExA(0, "BUTTON", "Show Only Selected",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            8, 466, 160, 26, hWnd, (HMENU)(INT_PTR)IDC_ZONE_SHOW_SELECTED,
            GetModuleHandle(NULL), NULL);
        g_hZoneHideSelectedButton = CreateWindowExA(0, "BUTTON", "Hide Selected",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            174, 466, 110, 26, hWnd, (HMENU)(INT_PTR)IDC_ZONE_HIDE_SELECTED,
            GetModuleHandle(NULL), NULL);
        g_hZoneHighlightSelectedButton = CreateWindowExA(0, "BUTTON", "Highlight Selected",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            290, 466, 128, 26, hWnd, (HMENU)(INT_PTR)IDC_ZONE_HIGHLIGHT_SELECTED,
            GetModuleHandle(NULL), NULL);
        g_hZoneCenterSelectedButton = CreateWindowExA(0, "BUTTON", "Center Camera",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            424, 466, 112, 26, hWnd, (HMENU)(INT_PTR)IDC_ZONE_CENTER_SELECTED,
            GetModuleHandle(NULL), NULL);
        g_hZoneCombineTreeButton = CreateWindowExA(0, "BUTTON", "Combine Lists",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            1200, 34, 120, 24, hWnd, (HMENU)(INT_PTR)IDC_ZONE_COMBINE_TREE_TOGGLE,
            GetModuleHandle(NULL), NULL);
        g_hZoneObjectList = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
            8, 56, 820, 190, hWnd, (HMENU)(INT_PTR)IDC_ZONE_OBJECT_LIST,
            GetModuleHandle(NULL), NULL);
        if (g_hZoneObjectList)
        {
            ApplyZoneListColors(g_hZoneObjectList, true);
        }
        g_hZoneUnrefObjectList = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
            8, 280, 820, 178, hWnd, (HMENU)(INT_PTR)IDC_ZONE_UNREF_OBJECT_LIST,
            GetModuleHandle(NULL), NULL);
        if (g_hZoneUnrefObjectList)
        {
            ApplyZoneListColors(g_hZoneUnrefObjectList, true);
        }
        g_hZoneCollisionObjectList = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
            8, 410, 820, 80, hWnd, (HMENU)(INT_PTR)IDC_ZONE_COLLISION_OBJECT_LIST,
            GetModuleHandle(NULL), NULL);
        if (g_hZoneCollisionObjectList)
        {
            ApplyZoneListColors(g_hZoneCollisionObjectList, false);
        }
        g_hZoneDrawBatchList = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
            8, 500, 820, 90, hWnd, (HMENU)(INT_PTR)IDC_ZONE_DRAW_BATCH_LIST,
            GetModuleHandle(NULL), NULL);
        if (g_hZoneDrawBatchList)
        {
            ApplyZoneListColors(g_hZoneDrawBatchList, false);
        }
        g_hZoneDataTree = CreateWindowExA(WS_EX_CLIENTEDGE, WC_TREEVIEWA, "",
            WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS,
            8, 56, 820, 360, hWnd, (HMENU)(INT_PTR)IDC_ZONE_DATA_TREE,
            GetModuleHandle(NULL), NULL);
        if (g_hZoneDataTree)
        {
            SendMessageA(g_hZoneDataTree, TVM_SETUNICODEFORMAT, FALSE, 0);
            ApplyZoneTreeColors(g_hZoneDataTree);
        }
        g_hZoneRawDataTree = CreateWindowExA(WS_EX_CLIENTEDGE, WC_TREEVIEWA, "",
            WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS,
            8, 56, 820, 360, hWnd, (HMENU)(INT_PTR)IDC_ZONE_RAW_DATA_TREE,
            GetModuleHandle(NULL), NULL);
        if (g_hZoneRawDataTree)
        {
            SendMessageA(g_hZoneRawDataTree, TVM_SETUNICODEFORMAT, FALSE, 0);
            ApplyZoneTreeColors(g_hZoneRawDataTree);
        }
        g_hZoneCollisionDataTree = CreateWindowExA(WS_EX_CLIENTEDGE, WC_TREEVIEWA, "",
            WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS,
            8, 56, 820, 360, hWnd, (HMENU)(INT_PTR)IDC_ZONE_COLLISION_DATA_TREE,
            GetModuleHandle(NULL), NULL);
        if (g_hZoneCollisionDataTree)
        {
            SendMessageA(g_hZoneCollisionDataTree, TVM_SETUNICODEFORMAT, FALSE, 0);
            ApplyZoneTreeColors(g_hZoneCollisionDataTree);
        }
        g_hZoneLoadingLabel = CreateWindowExA(WS_EX_CLIENTEDGE, "STATIC", "Loading Object List...",
            WS_CHILD | SS_CENTER | SS_CENTERIMAGE,
            18, 58, 220, 32, hWnd, (HMENU)(INT_PTR)IDC_ZONE_LOADING_LABEL,
            GetModuleHandle(NULL), NULL);
        if (g_hZoneLoadingLabel)
            ShowWindow(g_hZoneLoadingLabel, SW_HIDE);

        {
            const char *labels[6] = { "X", "Y", "Z", "Position", "Rotation", "Scale" };
            for (int i = 0; i < 6; ++i)
            {
                g_hZoneUnrefLabels[i] = CreateWindowExA(0, "STATIC", labels[i],
                    WS_CHILD, 10, 492 + i * 20, 70, 20, hWnd, NULL, GetModuleHandle(NULL), NULL);
            }
            for (int i = 0; i < 9; ++i)
            {
                g_hZoneUnrefEdits[i] = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                    WS_CHILD | ES_AUTOHSCROLL,
                    72 + (i % 3) * 80, 516 + (i / 3) * 24, 76, 22,
                    hWnd, (HMENU)(INT_PTR)(IDC_ZONE_UNREF_X + i),
                    GetModuleHandle(NULL), NULL);
            }
            g_hZoneUnrefApplyButton = CreateWindowExA(0, "BUTTON", "Apply Transform",
                WS_CHILD | BS_PUSHBUTTON,
                510, 516, 130, 26, hWnd, (HMENU)(INT_PTR)IDC_ZONE_UNREF_APPLY,
                GetModuleHandle(NULL), NULL);
        }
        {
            const char *tools[5] = { "Select Box", "Move", "Rotate", "Scale", "Transform" };
            const int ids[5] =
            {
                IDC_ZONE_TOOL_SELECT, IDC_ZONE_TOOL_MOVE, IDC_ZONE_TOOL_ROTATE,
                IDC_ZONE_TOOL_SCALE, IDC_ZONE_TOOL_TRANSFORM
            };
            for (int i = 0; i < 5; ++i)
            {
                g_hZoneToolButtons[i] = CreateWindowExA(0, "BUTTON", tools[i],
                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    8, 670 + i * 28, 142, 26, hWnd, (HMENU)(INT_PTR)ids[i],
                    GetModuleHandle(NULL), NULL);
            }
        }
        g_hZoneStatusLabel = CreateWindowExA(0, "STATIC", "",
            WS_CHILD | WS_VISIBLE,
            168, 830, 650, 20, hWnd, (HMENU)(INT_PTR)IDC_ZONE_STATUS_LABEL,
            GetModuleHandle(NULL), NULL);
        g_hZoneCollisionVisibleCheck = CreateWindowExA(0, "BUTTON",
            "Show collision mesh",
            WS_CHILD | BS_AUTOCHECKBOX,
            10, 540, 420, 24, hWnd, (HMENU)(INT_PTR)IDC_ZONE_COLLISION_VISIBLE,
            GetModuleHandle(NULL), NULL);
        SendMessageA(g_hZoneCollisionVisibleCheck, BM_SETCHECK,
                     g_showCollisionGeometry ? BST_CHECKED : BST_UNCHECKED, 0);
        UpdateZoneObjectEditControlState();
        return 0;

    case WM_SIZE:
        {
            const int w = LOWORD(lParam);
            const int h = HIWORD(lParam);
            const int margin = 8;
            const int headerH = 56;
            const int bottomH = 230;
            const int listBottom = h - bottomH;
            int placedX = 0, paneW = 0, rawX = 0, rawW = 0, collisionPaneX = 0, collisionW = 0;
            ComputeZonePaneLayout(w, &placedX, &paneW, &rawX, &rawW, &collisionPaneX, &collisionW);
            const int treeH = (listBottom > headerH) ? listBottom - headerH : 120;
            const int paneButtonTop = listBottom + 6;
            const int bottomTop = listBottom + 42;
            const int toolW = 150;
            const int inspectorX = margin + toolW + 14;
            const int actionY = bottomTop + 4;
            const int transformTop = bottomTop + 42;
            const int rowLabelW = 70;
            const int transformColW = 80;
            const int transformEditW = 76;

            if (g_hZoneLabel) MoveWindow(g_hZoneLabel, margin, 8, w - margin * 2, 20, TRUE);
            if (g_hZoneCombineTreeButton)
            {
                SetWindowTextA(g_hZoneCombineTreeButton,
                    g_zoneCombinedObjectTree ? "Split Lists" : "Combine Lists");
                MoveWindow(g_hZoneCombineTreeButton, w - margin - 128, 32, 128, 24, TRUE);
            }
            if (g_hZoneDrawBatchListLabel) ShowWindow(g_hZoneDrawBatchListLabel, SW_HIDE);
            if (g_hZoneObjectList) ShowWindow(g_hZoneObjectList, SW_HIDE);
            if (g_hZoneUnrefObjectList) ShowWindow(g_hZoneUnrefObjectList, SW_HIDE);
            if (g_hZoneCollisionObjectList) ShowWindow(g_hZoneCollisionObjectList, SW_HIDE);
            if (g_hZoneDrawBatchList) ShowWindow(g_hZoneDrawBatchList, SW_HIDE);
            if (g_zoneCombinedObjectTree)
            {
                if (g_hZonePlacedListLabel)
                {
                    SetWindowTextA(g_hZonePlacedListLabel, "DAT File Contents");
                    ShowWindow(g_hZonePlacedListLabel, SW_SHOW);
                    MoveWindow(g_hZonePlacedListLabel, margin, 36, w - margin * 2 - 140, 18, TRUE);
                }
                if (g_hZoneUnrefListLabel) ShowWindow(g_hZoneUnrefListLabel, SW_HIDE);
                if (g_hZoneCollisionListLabel) ShowWindow(g_hZoneCollisionListLabel, SW_HIDE);
                if (g_hZoneDataTree)
                {
                    ShowWindow(g_hZoneDataTree, SW_SHOW);
                    MoveWindow(g_hZoneDataTree, margin, headerH, w - margin * 2, treeH, TRUE);
                }
                if (g_hZoneRawDataTree) ShowWindow(g_hZoneRawDataTree, SW_HIDE);
                if (g_hZoneCollisionDataTree) ShowWindow(g_hZoneCollisionDataTree, SW_HIDE);
                if (g_hZonePlacedShowAllButton) MoveWindow(g_hZonePlacedShowAllButton, margin, paneButtonTop, 104, 24, TRUE);
                if (g_hZonePlacedHideAllButton) MoveWindow(g_hZonePlacedHideAllButton, margin + 110, paneButtonTop, 104, 24, TRUE);
                if (g_hZoneUnrefShowAllButton) MoveWindow(g_hZoneUnrefShowAllButton, margin + 220, paneButtonTop, 90, 24, TRUE);
                if (g_hZoneUnrefHideAllButton) MoveWindow(g_hZoneUnrefHideAllButton, margin + 316, paneButtonTop, 90, 24, TRUE);
                if (g_hZoneCollisionVisibleCheck) MoveWindow(g_hZoneCollisionVisibleCheck, margin + 424, paneButtonTop, 360, 24, TRUE);
            }
            else
            {
                if (g_hZonePlacedListLabel)
                {
                    SetWindowTextA(g_hZonePlacedListLabel, "Placed Geometry");
                    ShowWindow(g_hZonePlacedListLabel, SW_SHOW);
                    MoveWindow(g_hZonePlacedListLabel, placedX, 36, paneW, 18, TRUE);
                }
                if (g_hZoneUnrefListLabel)
                {
                    ShowWindow(g_hZoneUnrefListLabel, SW_SHOW);
                    MoveWindow(g_hZoneUnrefListLabel, rawX, 36, rawW, 18, TRUE);
                }
                if (g_hZoneCollisionListLabel)
                {
                    ShowWindow(g_hZoneCollisionListLabel, SW_SHOW);
                    MoveWindow(g_hZoneCollisionListLabel, collisionPaneX, 36, collisionW, 18, TRUE);
                }
                if (g_hZoneDataTree)
                {
                    ShowWindow(g_hZoneDataTree, SW_SHOW);
                    MoveWindow(g_hZoneDataTree, placedX, headerH, paneW, treeH, TRUE);
                }
                if (g_hZoneRawDataTree)
                {
                    ShowWindow(g_hZoneRawDataTree, SW_SHOW);
                    MoveWindow(g_hZoneRawDataTree, rawX, headerH, rawW, treeH, TRUE);
                }
                if (g_hZoneCollisionDataTree)
                {
                    ShowWindow(g_hZoneCollisionDataTree, SW_SHOW);
                    MoveWindow(g_hZoneCollisionDataTree, collisionPaneX, headerH, collisionW, treeH, TRUE);
                }
                if (g_hZonePlacedShowAllButton) MoveWindow(g_hZonePlacedShowAllButton, placedX, paneButtonTop, 104, 24, TRUE);
                if (g_hZonePlacedHideAllButton) MoveWindow(g_hZonePlacedHideAllButton, placedX + 110, paneButtonTop, 104, 24, TRUE);
                if (g_hZoneUnrefShowAllButton) MoveWindow(g_hZoneUnrefShowAllButton, rawX, paneButtonTop, 90, 24, TRUE);
                if (g_hZoneUnrefHideAllButton) MoveWindow(g_hZoneUnrefHideAllButton, rawX + 96, paneButtonTop, 90, 24, TRUE);
                if (g_hZoneCollisionVisibleCheck) MoveWindow(g_hZoneCollisionVisibleCheck, collisionPaneX, paneButtonTop, collisionW, 24, TRUE);
            }
            if (g_hZoneLoadingLabel) MoveWindow(g_hZoneLoadingLabel, 18, 58, 220, 32, TRUE);

            for (int i = 0; i < 5; ++i)
            {
                if (g_hZoneToolButtons[i])
                    MoveWindow(g_hZoneToolButtons[i], margin, bottomTop + i * 28, toolW, 26, TRUE);
            }

            if (g_hZoneShowSelectedButton) MoveWindow(g_hZoneShowSelectedButton, inspectorX, actionY, 160, 26, TRUE);
            if (g_hZoneHideSelectedButton) MoveWindow(g_hZoneHideSelectedButton, inspectorX + 166, actionY, 110, 26, TRUE);
            if (g_hZoneHighlightSelectedButton) MoveWindow(g_hZoneHighlightSelectedButton, inspectorX + 282, actionY, 128, 26, TRUE);
            if (g_hZoneCenterSelectedButton) MoveWindow(g_hZoneCenterSelectedButton, inspectorX + 416, actionY, 112, 26, TRUE);
            for (int col = 0; col < 3; ++col)
            {
                if (g_hZoneUnrefLabels[col])
                {
                    const int x = inspectorX + rowLabelW + col * transformColW;
                    MoveWindow(g_hZoneUnrefLabels[col], x + 32, transformTop, 20, 20, TRUE);
                }
            }
            for (int row = 0; row < 3; ++row)
            {
                if (g_hZoneUnrefLabels[row + 3])
                    MoveWindow(g_hZoneUnrefLabels[row + 3], inspectorX, transformTop + 24 + row * 24 + 4, rowLabelW, 20, TRUE);
            }
            for (int i = 0; i < 9; ++i)
            {
                const int x = inspectorX + rowLabelW + (i % 3) * transformColW;
                const int y = transformTop + 24 + (i / 3) * 24;
                if (g_hZoneUnrefEdits[i]) MoveWindow(g_hZoneUnrefEdits[i], x, y, transformEditW, 22, TRUE);
            }
            if (g_hZoneUnrefApplyButton) MoveWindow(g_hZoneUnrefApplyButton, inspectorX + 328, transformTop + 48, 130, 26, TRUE);
            if (g_hZoneStatusLabel) MoveWindow(g_hZoneStatusLabel, inspectorX, h - 24, w - inspectorX - margin, 20, TRUE);
        }
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT && !g_zoneCombinedObjectTree)
        {
            POINT pt = {};
            GetCursorPos(&pt);
            ScreenToClient(hWnd, &pt);
            RECT rc = {};
            GetClientRect(hWnd, &rc);
            if (HitZonePaneSplitter(rc.right - rc.left, pt.x, pt.y))
            {
                SetCursor(LoadCursor(NULL, IDC_SIZEWE));
                return TRUE;
            }
        }
        break;

    case WM_LBUTTONDOWN:
        if (!g_zoneCombinedObjectTree)
        {
            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);
            RECT rc = {};
            GetClientRect(hWnd, &rc);
            g_zoneDraggingSplitter = HitZonePaneSplitter(rc.right - rc.left, x, y);
            if (g_zoneDraggingSplitter)
            {
                SetCapture(hWnd);
                SetCursor(LoadCursor(NULL, IDC_SIZEWE));
                return 0;
            }
        }
        break;

    case WM_MOUSEMOVE:
        if (g_zoneDraggingSplitter)
        {
            const int margin = 8;
            const int gap = 10;
            const int x = GET_X_LPARAM(lParam);
            RECT rc = {};
            GetClientRect(hWnd, &rc);
            const int clientW = (int)(rc.right - rc.left);
            const int clientH = (int)(rc.bottom - rc.top);
            const int totalW = std::max(300, clientW - margin * 2 - gap * 2);
            int minPaneW = 220;
            if (minPaneW * 3 > totalW)
                minPaneW = totalW / 3;

            if (g_zoneDraggingSplitter == 1)
            {
                const int combined12 = g_zonePaneWidth[0] + g_zonePaneWidth[1];
                int newPlaced = x - margin;
                newPlaced = std::max(minPaneW, std::min(newPlaced, combined12 - minPaneW));
                g_zonePaneWidth[0] = newPlaced;
                g_zonePaneWidth[1] = combined12 - newPlaced;
            }
            else if (g_zoneDraggingSplitter == 2)
            {
                const int rawStart = margin + g_zonePaneWidth[0] + gap;
                const int combined23 = g_zonePaneWidth[1] + g_zonePaneWidth[2];
                int newRaw = x - rawStart;
                newRaw = std::max(minPaneW, std::min(newRaw, combined23 - minPaneW));
                g_zonePaneWidth[1] = newRaw;
                g_zonePaneWidth[2] = combined23 - newRaw;
            }

            SendMessageA(hWnd, WM_SIZE, 0, MAKELPARAM(clientW, clientH));
            InvalidateRect(hWnd, NULL, TRUE);
            return 0;
        }
        break;

    case WM_LBUTTONUP:
        if (g_zoneDraggingSplitter)
        {
            g_zoneDraggingSplitter = 0;
            ReleaseCapture();
            return 0;
        }
        break;

    case WM_ZONE_OBJECT_POPULATE:
        RefreshZoneObjectPanel();
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_ZONE_PLACED_SHOW_ALL:
            SetZoneListVisibility(g_hZoneObjectList, true);
            PopulateZoneDataTree();
            if (g_hWnd) InvalidateRect(g_hWnd, NULL, FALSE);
            return 0;
        case IDC_ZONE_PLACED_HIDE_ALL:
            SetZoneListVisibility(g_hZoneObjectList, false);
            PopulateZoneDataTree();
            if (g_hWnd) InvalidateRect(g_hWnd, NULL, FALSE);
            return 0;
        case IDC_ZONE_UNREF_SHOW_ALL:
            SetZoneListVisibility(g_hZoneUnrefObjectList, true);
            PopulateZoneDataTree();
            if (g_hWnd) InvalidateRect(g_hWnd, NULL, FALSE);
            return 0;
        case IDC_ZONE_UNREF_HIDE_ALL:
            SetZoneListVisibility(g_hZoneUnrefObjectList, false);
            PopulateZoneDataTree();
            if (g_hWnd) InvalidateRect(g_hWnd, NULL, FALSE);
            return 0;
        case IDC_ZONE_COMBINE_TREE_TOGGLE:
            g_zoneCombinedObjectTree = !g_zoneCombinedObjectTree;
            PopulateZoneDataTree();
            {
                RECT rc = {};
                GetClientRect(hWnd, &rc);
                SendMessageA(hWnd, WM_SIZE, 0, MAKELPARAM(rc.right - rc.left, rc.bottom - rc.top));
            }
            return 0;
        case IDC_ZONE_SHOW_SELECTED:
            {
                const int selectedIndex = GetSelectedZoneObjectIndex();
                if (selectedIndex >= 0)
                {
                    g_hiddenZoneObjects.clear();
                    for (int i = 0; i < Model_FF11_GetLastMapObjectCount(); ++i)
                    {
                        if (i != selectedIndex)
                            SetZoneObjectHidden(Model_FF11_GetLastMapObjectDisplayName(i), true);
                    }
                    g_populatingZoneObjectList = true;
                    HWND lists[2] = { g_hZoneObjectList, g_hZoneUnrefObjectList };
                    for (int listIndex = 0; listIndex < 2; ++listIndex)
                    {
                        HWND hList = lists[listIndex];
                        if (!hList)
                            continue;
                        for (int row = 0; row < ListView_GetItemCount(hList); ++row)
                            ListView_SetCheckState(hList, row,
                                GetZoneListMapObjectIndex(hList, row) == selectedIndex);
                    }
                    g_populatingZoneObjectList = false;
                    PopulateZoneDataTree();
                    if (g_hWnd) InvalidateRect(g_hWnd, NULL, FALSE);
                }
            }
            return 0;
        case IDC_ZONE_HIDE_SELECTED:
            {
                const int selectedIndex = GetSelectedZoneObjectIndex();
                if (selectedIndex >= 0)
                {
                    SetZoneObjectHidden(Model_FF11_GetLastMapObjectDisplayName(selectedIndex), true);
                    SetZoneListRowCheckedByIndex(selectedIndex, FALSE);
                    PopulateZoneDataTree();
                    if (g_hWnd) InvalidateRect(g_hWnd, NULL, FALSE);
                }
            }
            return 0;
        case IDC_ZONE_HIGHLIGHT_SELECTED:
            {
                const char *objectName = GetSelectedZoneObjectName();
                if (objectName && objectName[0])
                {
                    if (g_highlightedZoneObject == objectName)
                        g_highlightedZoneObject.clear();
                    else
                        g_highlightedZoneObject = objectName;
                    if (g_hZoneHighlightSelectedButton)
                        SetWindowTextA(g_hZoneHighlightSelectedButton,
                            g_highlightedZoneObject.empty() ? "Highlight Selected" : "Clear Highlight");
                    if (g_hWnd) InvalidateRect(g_hWnd, NULL, FALSE);
                }
            }
            return 0;
        case IDC_ZONE_CENTER_SELECTED:
            {
                const int selectedIndex = GetSelectedZoneObjectIndex();
                if (selectedIndex >= 0)
                {
                    float trans[3] = {};
                    float rot[3] = {};
                    float scale[3] = {};
                    Model_FF11_GetLastMapObjectTransform(selectedIndex, trans, scale, rot);
                    const char *objectName = Model_FF11_GetLastMapObjectDisplayName(selectedIndex);
                    std::map<std::string, ZoneDebugTransform>::const_iterator it =
                        g_zoneObjectOverrides.find(objectName ? objectName : "");
                    if (it != g_zoneObjectOverrides.end())
                        memcpy(trans, it->second.trans, sizeof(trans));
                    g_camTarget[0] = trans[0];
                    g_camTarget[1] = trans[1];
                    g_camTarget[2] = trans[2];
                    if (g_camDist < 12.0f)
                        g_camDist = 12.0f;
                    if (g_hWnd) InvalidateRect(g_hWnd, NULL, FALSE);
                }
            }
            return 0;
        case IDC_ZONE_UNREF_APPLY:
            {
                const int mapObjectIndex = GetSelectedZoneObjectIndex();
                if (mapObjectIndex >= 0)
                {
                    ZoneDebugTransform xform = {};
                    char buf[64] = {};
                    for (int i = 0; i < 9; ++i)
                    {
                        GetWindowTextA(g_hZoneUnrefEdits[i], buf, sizeof(buf));
                        const float value = (float)atof(buf);
                        if (i < 3) xform.trans[i] = value;
                        else if (i < 6) xform.rot[i - 3] = value;
                        else xform.scale[i - 6] = value;
                    }
                    const char *objectName = Model_FF11_GetLastMapObjectDisplayName(mapObjectIndex);
                    if (objectName && objectName[0])
                        g_zoneObjectOverrides[objectName] = xform;
                    RefreshZoneObjectPanel();
                }
            }
            return 0;
        case IDC_ZONE_COLLISION_VISIBLE:
            g_showCollisionGeometry =
                SendMessageA(g_hZoneCollisionVisibleCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
            RefreshZoneObjectPanel();
            return 0;
        }
        break;

    case WM_CTLCOLORSTATIC:
        {
            EnsureZoneObjectBrushes();
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, kZoneTextColor);
            SetBkColor(hdc, kZonePanelColor);
            return (LRESULT)g_hZonePanelBrush;
        }

    case WM_CTLCOLOREDIT:
        {
            EnsureZoneObjectBrushes();
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, kZoneTextColor);
            SetBkColor(hdc, kZoneEditColor);
            return (LRESULT)g_hZoneEditBrush;
        }

    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN:
        {
            EnsureZoneObjectBrushes();
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, kZoneTextColor);
            SetBkColor(hdc, kZoneControlColor);
            return (LRESULT)g_hZoneControlBrush;
        }

    case WM_ERASEBKGND:
        {
            EnsureZoneObjectBrushes();
            RECT rc;
            GetClientRect(hWnd, &rc);
            FillRect((HDC)wParam, &rc, g_hZonePanelBrush);
            if (!g_zoneCombinedObjectTree)
            {
                int placedX = 0, placedW = 0, rawX = 0, rawW = 0, collisionX = 0, collisionW = 0;
                ComputeZonePaneLayout(rc.right - rc.left, &placedX, &placedW, &rawX, &rawW, &collisionX, &collisionW);
                HBRUSH hSplitterBrush = CreateSolidBrush(RGB(88, 94, 98));
                RECT split1 = { placedX + placedW + 4, 56, placedX + placedW + 6, rc.bottom - 230 };
                RECT split2 = { rawX + rawW + 4, 56, rawX + rawW + 6, rc.bottom - 230 };
                FillRect((HDC)wParam, &split1, hSplitterBrush);
                FillRect((HDC)wParam, &split2, hSplitterBrush);
                DeleteObject(hSplitterBrush);
            }
            return 1;
        }

    case WM_NOTIFY:
        if (!g_populatingZoneObjectList)
        {
            LPNMHDR pHdr = (LPNMHDR)lParam;
            if (pHdr && IsZoneDataTreeId((UINT)pHdr->idFrom) && pHdr->code == TVN_ITEMEXPANDINGA)
            {
                LPNMTREEVIEWA pTree = (LPNMTREEVIEWA)lParam;
                if (pTree->action == TVE_EXPAND)
                    PopulateExpandedZoneTreeNode(pHdr->hwndFrom, pTree->itemNew.hItem, pTree->itemNew.lParam);
                return 0;
            }
            if (pHdr && IsZoneDataTreeId((UINT)pHdr->idFrom) && pHdr->code == TVN_SELCHANGEDA)
            {
                LPNMTREEVIEWA pTree = (LPNMTREEVIEWA)lParam;
                const int selectedIndex =
                    (GetZoneTreeParamType(pTree->itemNew.lParam) == kZoneTreeNode_MapObject)
                    ? GetZoneTreeParamIndex(pTree->itemNew.lParam)
                    : -1;
                g_zoneTreeSelectedMapObjectIndex =
                    (selectedIndex >= 0 && selectedIndex < Model_FF11_GetLastMapObjectCount()) ? selectedIndex : -1;
                UpdateZoneUnrefEditFields();
                return 0;
            }

            LPNMLISTVIEW pNmlv = (LPNMLISTVIEW)lParam;
            const bool isZoneListNotify = pHdr &&
                (pHdr->idFrom == IDC_ZONE_OBJECT_LIST ||
                 pHdr->idFrom == IDC_ZONE_UNREF_OBJECT_LIST);
            HWND hNotifyList = pHdr ? GetZoneObjectListById((UINT)pHdr->idFrom) : NULL;
            if (isZoneListNotify &&
                pNmlv->hdr.code == LVN_ITEMCHANGED &&
                (pNmlv->uChanged & LVIF_STATE) != 0)
            {
                const UINT oldState = pNmlv->uOldState & LVIS_STATEIMAGEMASK;
                const UINT newState = pNmlv->uNewState & LVIS_STATEIMAGEMASK;
                if (oldState != newState)
                {
                    const int mapObjectIndex = GetZoneListMapObjectIndex(hNotifyList, pNmlv->iItem);
                    SetZoneObjectHidden(Model_FF11_GetLastMapObjectDisplayName(mapObjectIndex),
                                        !ListView_GetCheckState(hNotifyList, pNmlv->iItem));
                    if (g_hWnd) InvalidateRect(g_hWnd, NULL, FALSE);
                }
            }
            if (isZoneListNotify &&
                pNmlv->hdr.code == LVN_ITEMCHANGED &&
                (pNmlv->uChanged & LVIF_STATE) != 0 &&
                (pNmlv->uNewState & LVIS_SELECTED) != 0)
            {
                HWND hOtherList = (hNotifyList == g_hZoneObjectList) ? g_hZoneUnrefObjectList : g_hZoneObjectList;
                if (hOtherList)
                {
                    const int otherRow = ListView_GetNextItem(hOtherList, -1, LVNI_SELECTED);
                    if (otherRow >= 0)
                        ListView_SetItemState(hOtherList, otherRow, 0, LVIS_SELECTED | LVIS_FOCUSED);
                }
                UpdateZoneUnrefEditFields();
            }
        }
        break;

    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        if (g_hZoneObjectPanel == hWnd)
        {
            g_hZoneObjectPanel = NULL;
            g_hZoneLabel = NULL;
            g_hZonePlacedListLabel = NULL;
            g_hZoneUnrefListLabel = NULL;
            g_hZoneCollisionListLabel = NULL;
            g_hZoneDrawBatchListLabel = NULL;
            g_hZoneObjectList = NULL;
            g_hZoneUnrefObjectList = NULL;
            g_hZoneCollisionObjectList = NULL;
            g_hZoneDrawBatchList = NULL;
            g_hZoneDataTree = NULL;
            g_hZoneRawDataTree = NULL;
            g_hZoneCollisionDataTree = NULL;
            g_hZoneLoadingLabel = NULL;
            g_hZonePlacedShowAllButton = NULL;
            g_hZonePlacedHideAllButton = NULL;
            g_hZoneUnrefShowAllButton = NULL;
            g_hZoneUnrefHideAllButton = NULL;
            g_hZoneShowSelectedButton = NULL;
            g_hZoneHideSelectedButton = NULL;
            g_hZoneHighlightSelectedButton = NULL;
            g_hZoneCenterSelectedButton = NULL;
            g_hZoneCombineTreeButton = NULL;
            memset(g_hZoneToolButtons, 0, sizeof(g_hZoneToolButtons));
            g_hZoneStatusLabel = NULL;
            g_hZoneUnrefApplyButton = NULL;
            g_hZoneCollisionVisibleCheck = NULL;
            g_zoneObjectColumnMode = -1;
            g_zoneUnrefObjectColumnMode = -1;
            g_zoneCollisionObjectColumnMode = -1;
            g_zoneDrawBatchColumnMode = -1;
            g_zoneDraggingSplitter = 0;
            memset(g_hZoneUnrefEdits, 0, sizeof(g_hZoneUnrefEdits));
            memset(g_hZoneUnrefLabels, 0, sizeof(g_hZoneUnrefLabels));
        }
        return 0;
    }

    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

static void ShowZoneObjectPanel()
{
    if (g_hZoneObjectPanel)
    {
        ShowWindow(g_hZoneObjectPanel, SW_SHOW);
        SetForegroundWindow(g_hZoneObjectPanel);
        BeginZoneObjectPanelRefresh();
        return;
    }

    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ZoneObjectPanelProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    EnsureZoneObjectBrushes();
    wc.hbrBackground = g_hZonePanelBrush;
    wc.lpszClassName = "DATuraZoneObjectPanelClass";
    RegisterClassExA(&wc);

    g_hZoneObjectPanel = CreateWindowExA(WS_EX_TOOLWINDOW, wc.lpszClassName,
        "Zone Objects", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 1500, 850, g_hWnd, NULL, wc.hInstance, NULL);
    if (g_hZoneObjectPanel)
    {
        ShowWindow(g_hZoneObjectPanel, SW_SHOW);
        UpdateWindow(g_hZoneObjectPanel);
        BeginZoneObjectPanelRefresh();
    }
}

static float ComputeModelGroundOffset(const noesisModel_t *pModel)
{
    if (!pModel)
        return 0.0f;

    bool haveVertex = false;
    float minY = 0.0f;
    for (const noesisModel_t::Submesh &sm : pModel->submeshes)
    {
        for (const FFXIVertex &v : sm.cpuVerts)
        {
            if (!haveVertex || v.pos[1] < minY)
            {
                minY = v.pos[1];
                haveVertex = true;
            }
        }
    }

    return haveVertex ? (-minY + kPlayerFootContactAdjust) : 0.0f;
}

static void LoadPlayerRaceModel(int raceIndex)
{
    g_titleScreenActive = false;
    g_highPolyCreationActive = false;
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

    char datSet[4096] = {};
    strcat_s(datSet, "NOESIS_FF11_DAT_SET\n");
    strcat_s(datSet, "setPathAbs \"");
    strcat_s(datSet, g_ffxiPath);
    strcat_s(datSet, "\"\n");
    AppendDatSetLine(datSet, sizeof(datSet), "__skeleton", race.entries[0].dat);
    AppendVariantDatLine(datSet, sizeof(datSet), "__animation", race, 8, g_playerEquip.animationBank);
    if (g_playerFaceVariant <= 0)
        AppendDatSetLine(datSet, sizeof(datSet), "face", race.entries[1].dat);
    else
        AppendVariantDatLine(datSet, sizeof(datSet), "face", race, 1, g_playerFaceVariant);
    if (g_playerEquip.headItem <= 0)
        AppendDatSetLine(datSet, sizeof(datSet), "head", race.entries[2].dat);
    else
        AppendVariantDatLine(datSet, sizeof(datSet), "head", race, 2, g_playerEquip.headItem - 1);
    AppendVariantDatLine(datSet, sizeof(datSet), "body", race, 3, g_playerEquip.bodyItem);
    AppendVariantDatLine(datSet, sizeof(datSet), "hands", race, 4, g_playerEquip.handsItem);
    AppendVariantDatLine(datSet, sizeof(datSet), "legs", race, 5, g_playerEquip.legsItem);
    AppendVariantDatLine(datSet, sizeof(datSet), "feet", race, 6, g_playerEquip.feetItem);
    if (g_playerEquip.mainItem > 0)
        AppendVariantDatLine(datSet, sizeof(datSet), "main", race, 7, g_playerEquip.mainItem - 1);
    if (g_playerEquip.subItem > 0)
        AppendVariantDatLine(datSet, sizeof(datSet), "sub", race, 7, g_playerEquip.subItem - 1);
    if (g_playerEquip.rangedItem > 0)
        AppendVariantDatLine(datSet, sizeof(datSet), "ranged", race, 7, g_playerEquip.rangedItem - 1);

    g_pPlayerRapi = new noeRAPI_t(g_pDevice);
    ConfigureRapiTextureSettings(g_pPlayerRapi);
    g_pPlayerRapi->SetCurrentFilePath("DATura generated player.ff11datset");

    int numMdl = 0;
    noesisModel_t *pMdl = Model_FF11_LoadDATSet((BYTE *)datSet, (int)strlen(datSet), numMdl, g_pPlayerRapi);
    if (!pMdl || numMdl == 0)
    {
        delete g_pPlayerRapi; g_pPlayerRapi = nullptr;
        MessageBoxA(g_hWnd, "Player DAT set loaded but contained no displayable geometry.", "Player Load", MB_OK | MB_ICONWARNING);
        return;
    }

    g_pPlayerModel = pMdl;
    g_playerGroundOffset = ComputeModelGroundOffset(g_pPlayerModel);
    g_playerAnimTime = 0.0f;
    g_playerPos[0] = g_camTarget[0];
    g_playerPos[1] = g_camTarget[1];
    g_playerPos[2] = g_camTarget[2];
    if (!g_zoneCollisionTris.empty())
    {
        float floorY = 0.0f;
        float floorN[3] = {};
        if (CollisionFloorAt(g_playerPos[0], g_playerPos[2],
                             g_playerPos[1] - 40.0f, g_playerPos[1] + 240.0f,
                             &floorY, floorN))
        {
            g_playerPos[1] = floorY;
            g_playerVelY = 0.0f;
            g_playerOnGround = true;
        }
    }
    g_playerYaw = g_camYaw;
    SetInteractionMode(kDATuraMode_Game);
    g_camDist = 10.0f;
    g_camPitch = 0.35f;
    SetPlayerRespawnPoint();

    if (g_hLowPolyPanel)
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

struct TitleVertex
{
    float x, y, z, rhw;
    DWORD color;
    float u, v;
};

#define TITLE_VERTEX_FVF (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1)

static int IntMax(int a, int b)
{
    return (a > b) ? a : b;
}

static int IntMin(int a, int b)
{
    return (a < b) ? a : b;
}

static void PrepareScreenSpaceUiRenderState()
{
    // The title overlay is a separate 2D pass. It must not inherit a world
    // pixel shader, depth/cull state, or texture-stage configuration.
    g_pDevice->SetVertexShader(nullptr);
    g_pDevice->SetPixelShader(nullptr);
    g_pDevice->SetFVF(TITLE_VERTEX_FVF);
    g_pDevice->SetTexture(1, nullptr);
    g_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    g_pDevice->SetRenderState(D3DRS_FOGENABLE, FALSE);
    g_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    g_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    g_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    g_pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    g_pDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
    g_pDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    g_pDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    g_pDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    g_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    g_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
}

static void DrawShadowText(HDC hdc, HFONT hFont, const char *text, RECT rc,
                           UINT format, COLORREF color, int shadowOffset);

static void CompactAssetName(const char *src, char *dst, int dstSize)
{
    if (!src || !dst || dstSize <= 0)
        return;

    int out = 0;
    for (int i = 0; src[i] && out < dstSize - 1; ++i)
    {
        if (src[i] != ' ')
            dst[out++] = (char)tolower((unsigned char)src[i]);
    }
    dst[out] = '\0';
}

static noesisTex_t *FindTitleTexture(noesisModel_t *pModel, const char *needle)
{
    if (!pModel || !pModel->pMatData || !needle)
        return nullptr;

    char compactNeedle[64] = {};
    CompactAssetName(needle, compactNeedle, sizeof(compactNeedle));

    noesisMatData_t *pMD = pModel->pMatData;
    for (int i = 0; i < pMD->texCount; ++i)
    {
        noesisTex_t *pTex = pMD->textures[i];
        if (!pTex || !pTex->name)
            continue;

        char compactName[128] = {};
        CompactAssetName(pTex->name, compactName, sizeof(compactName));
        if (strstr(compactName, compactNeedle))
            return pTex;
    }

    return nullptr;
}

static bool UsesFfxiDxt3Alpha(const noesisTex_t *pTex)
{
    return pTex && pTex->texType == NOESISTEX_DXT3;
}

static void DrawTexturedQuadUV(IDirect3DTexture9 *pTex, float x, float y, float w, float h,
                               float u0, float v0, float u1, float v1, DWORD color,
                               bool expandDxt3Alpha, float alphaScale = 1.0f,
                               float maxOpacity = 1.0f)
{
    if (!g_pDevice || !pTex || w <= 0.0f || h <= 0.0f)
        return;

    TitleVertex verts[4] =
    {
        { x - 0.5f,     y - 0.5f,     0.0f, 1.0f, color, u0, v0 },
        { x + w - 0.5f, y - 0.5f,     0.0f, 1.0f, color, u1, v0 },
        { x - 0.5f,     y + h - 0.5f, 0.0f, 1.0f, color, u0, v1 },
        { x + w - 0.5f, y + h - 0.5f, 0.0f, 1.0f, color, u1, v1 },
    };

    PrepareScreenSpaceUiRenderState();
    g_pDevice->SetTexture(0, pTex);
    SetFfxiUiPixelShader(expandDxt3Alpha, alphaScale, maxOpacity);
    g_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    g_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    g_pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    g_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    g_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    g_pDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    g_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    g_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    g_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    g_pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    g_pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    g_pDevice->SetSamplerState(0, D3DSAMP_MIPFILTER,
        g_enableMipMapping ? D3DTEXF_LINEAR : D3DTEXF_NONE);

    g_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, verts, sizeof(TitleVertex));

    g_pDevice->SetTexture(0, nullptr);
    g_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    g_pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    g_pDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
    g_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
}

static void DrawTexturedQuad(noesisTex_t *pTex, float x, float y, float w, float h, DWORD color)
{
    if (!pTex || !pTex->pD3DTex)
        return;
    DrawTexturedQuadUV(pTex->pD3DTex, x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, color,
                       UsesFfxiDxt3Alpha(pTex));
}

static void DrawSolidQuad(float x, float y, float w, float h, DWORD color)
{
    if (!g_pDevice || w <= 0.0f || h <= 0.0f)
        return;

    TitleVertex v[4] =
    {
        { x,     y,     0.0f, 1.0f, color, 0.0f, 0.0f },
        { x + w, y,     0.0f, 1.0f, color, 0.0f, 0.0f },
        { x,     y + h, 0.0f, 1.0f, color, 0.0f, 0.0f },
        { x + w, y + h, 0.0f, 1.0f, color, 0.0f, 0.0f },
    };

    PrepareScreenSpaceUiRenderState();
    g_pDevice->SetTexture(0, nullptr);
    g_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    g_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    g_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    g_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    g_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    g_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    g_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(TitleVertex));
}

static void DrawTextureRegion(noesisTex_t *pTex, float x, float y, float w, float h,
                              float srcX, float srcY, float srcW, float srcH, DWORD color)
{
    if (!pTex || !pTex->pD3DTex || pTex->w <= 0 || pTex->h <= 0)
        return;

    DrawTexturedQuadUV(pTex->pD3DTex, x, y, w, h,
                       srcX / (float)pTex->w,
                       srcY / (float)pTex->h,
                       (srcX + srcW) / (float)pTex->w,
                       (srcY + srcH) / (float)pTex->h,
                       color, UsesFfxiDxt3Alpha(pTex));
}

static void GetTitleButtonMetrics(int w, int h, int *buttonX, int *buttonY, int *buttonW, int *buttonH)
{
    if (buttonW)
        *buttonW = IntMax(230, w / 7);
    if (buttonH)
        *buttonH = IntMax(28, h / 30);

    const int bw = IntMax(230, w / 7);
    if (buttonX)
        *buttonX = (w * 345) / 1000;
    if (buttonY)
        *buttonY = (h * 57) / 100;
}

static int GetTitleSelectedButtonIndex(int w, int h)
{
    int buttonX = 0, buttonY = 0, buttonW = 0, buttonH = 0;
    GetTitleButtonMetrics(w, h, &buttonX, &buttonY, &buttonW, &buttonH);

    for (int i = 0; i < 5; ++i)
    {
        if (g_mouseClient.x >= buttonX && g_mouseClient.x < buttonX + buttonW &&
            g_mouseClient.y >= buttonY && g_mouseClient.y < buttonY + buttonH)
        {
            return i;
        }
        buttonY += buttonH + 6;
    }

    return -1;
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
        MessageBoxA(g_hWnd, "Character selection is not implemented yet.",
                    "DATura", MB_OK | MB_ICONINFORMATION);
        break;

    case 2:
        MessageBoxA(g_hWnd, "Character deletion is not implemented yet.",
                    "DATura", MB_OK | MB_ICONINFORMATION);
        break;

    case 3:
        ShowTitleConfigDialog();
        break;
    }
}

static void DrawTitleButtonTexture(noesisTex_t *pTex, float x, float y, float w, float h, bool hover)
{
    if (!pTex || !pTex->pD3DTex || pTex->w <= 0 || pTex->h <= 0)
        return;

    // Compose the title button from atlas layers. The circular button is used as
    // the rounded end cap; the long button body supplies the center fill.
    const float rowY = hover ? 32.0f : 0.0f;
    const float rowH = 16.0f;
    const float circleX = 0.0f;
    const float circleW = 16.0f;
    const float bodyX = 16.0f;
    const float bodyW = 48.0f;
    const float bodyEndInset = 8.0f;

    const float v0 = rowY / (float)pTex->h;
    const float v1 = (rowY + rowH) / (float)pTex->h;

    const float capOuterU0 = circleX / (float)pTex->w;
    const float capMidU = (circleX + circleW * 0.5f) / (float)pTex->w;
    const float capOuterU1 = (circleX + circleW) / (float)pTex->w;
    const float bodyU0 = (bodyX + bodyEndInset) / (float)pTex->w;
    const float bodyU1 = (bodyX + bodyW - bodyEndInset) / (float)pTex->w;

    float dstCapW = h * 0.5f;
    if (dstCapW * 2.0f > w)
        dstCapW = w * 0.5f;

    const DWORD color = hover ? 0xFFFFB060 : 0xFFFFFFFF;
    const bool expandDxt3Alpha = UsesFfxiDxt3Alpha(pTex);

    // Left cap uses the left half of the circular source.
    DrawTexturedQuadUV(pTex->pD3DTex, x, y, dstCapW, h,
                       capOuterU0, v0, capMidU, v1, color, expandDxt3Alpha);

    // Center stretches only the flat middle of the long source button.
    DrawTexturedQuadUV(pTex->pD3DTex, x + dstCapW, y, w - dstCapW * 2.0f, h,
                       bodyU0, v0, bodyU1, v1, color, expandDxt3Alpha);

    // Right cap mirrors the rounded outer half of the circular source.
    DrawTexturedQuadUV(pTex->pD3DTex, x + w - dstCapW, y, dstCapW, h,
                       capMidU, v0, capOuterU0, v1, color, expandDxt3Alpha);
}

static void DrawTitleExpansionAtlas(noesisTex_t *pTex, int rows, int startRow,
                                    float x, float y, float w, float h, float gap)
{
    if (!pTex || rows <= 0)
        return;

    const float srcRowH = (float)pTex->h / (float)rows;
    for (int row = 0; row < rows; ++row)
    {
        DrawTextureRegion(pTex, x, y + (h + gap) * (float)(startRow + row), w, h,
                          0.0f, srcRowH * (float)row, (float)pTex->w, srcRowH, 0xFFFFFFFF);
    }
}

struct NationSelectInfo
{
    const char *name;
    const char *subtitle;
    const char *textureName;
    const char *description;
    int zoneId;
    COLORREF accent;
};

static const NationSelectInfo kNationSelectInfo[] =
{
    {
        "The Republic of Bastok",
        "Industry, invention, and resolve",
        "bcre",
        "A nation of industry and invention, Bastok rose from the rugged lands of Gustaberg through mining, engineering, and sheer determination. Its citizens value progress, discipline, and hard work, though old tensions still linger beneath the smoke of its forges.",
        235,
        RGB(80, 96, 210)
    },
    {
        "The Federation of Windurst",
        "Magic, scholarship, and the stars",
        "wcre",
        "A lush and mystical nation guided by magic, scholarship, and the wisdom of the Star Sibyl. Windurst is home to brilliant Tarutaru mages and proud Mithra hunters, where ancient traditions and playful curiosity shape daily life.",
        241,
        RGB(90, 150, 48)
    },
    {
        "The Kingdom of San d'Oria",
        "Knighthood, faith, and honor",
        "scre",
        "A proud kingdom of knights, faith, and noble houses, San d'Oria stands beneath the forests of Ronfaure as a bastion of honor and tradition. Its people revere courage, loyalty, and duty, though pride can be as sharp as any blade.",
        230,
        RGB(185, 32, 36)
    },
};

static const int kNationSelectCount = (int)(sizeof(kNationSelectInfo) / sizeof(kNationSelectInfo[0]));

static void LoadSelectedNationScene()
{
    if (g_hHighPolyCreationPanel)
    {
        GetWindowTextA(HighPolyCreationPanelControl(IDC_HP_NAME),
                       g_creationCharacterName, sizeof(g_creationCharacterName));
        PullHighPolyCreationStateFromControls();
        ShowWindow(g_hHighPolyCreationPanel, SW_HIDE);
    }

    if (g_selectedNationIndex < 0 || g_selectedNationIndex >= kNationSelectCount)
        g_selectedNationIndex = 0;

    const NationSelectInfo &nation = kNationSelectInfo[g_selectedNationIndex];
    const FFXIZoneEntry *pZone = FFXIZone::FindByID(nation.zoneId);
    char fullPath[MAX_PATH] = {};
    if (!pZone || !ResolveZoneModelPath(nation.zoneId, fullPath, sizeof(fullPath)))
    {
        MessageBoxA(g_hWnd, "The selected nation zone has no model DAT on record and could not be resolved from FTABLE/VTABLE.",
                    "Nation Select", MB_OK | MB_ICONWARNING);
        return;
    }

    LoadDatFile(fullPath);
    SetLoadedZoneLabelFromNameAndPath(pZone->name, fullPath);
    RememberLoadedZoneContext(fullPath, pZone->name, true, false, false);
    if (!g_pZoneModel)
        return;

    ApplyCreationStateToLowPolyPlayer();
    LoadPlayerRaceModel(g_playerEquip.raceIndex);

    const FFXIZoneMusicEntry *pMusic = FFXIZoneMusic_FindByID(nation.zoneId);
    if (pMusic)
        SetGameModeMusic(SelectZoneMusicId(pMusic));

    if (g_hLowPolyPanel)
        SyncLowPolyControlsFromState();
    if (g_hWnd)
        InvalidateRect(g_hWnd, NULL, FALSE);
}

static bool EnsureNationSelectAssets()
{
    if (g_pTitleUiModel && g_pTitleUiRapi)
        return true;

    g_pTitleUiModel = LoadTextureDat("ROM/119/51.DAT", &g_pTitleUiRapi);
    return g_pTitleUiModel != nullptr;
}

static RECT GetNationCardRect(int clientW, int clientH, int index)
{
    const int marginX = IntMax(56, clientW / 18);
    const int cardGap = IntMax(18, clientW / 70);
    const int cardW = IntMax(80, (clientW - marginX * 2 - cardGap * 2) / 3);
    const int cardH = IntMax(230, clientH / 3);
    const int cardY = IntMax(118, clientH / 7);
    const int cardX = marginX + (cardW + cardGap) * index;
    RECT rc = { cardX, cardY, cardX + cardW, cardY + cardH };
    return rc;
}

static int GetNationCardAtPoint(int clientW, int clientH, POINT pt)
{
    for (int i = 0; i < kNationSelectCount; ++i)
    {
        RECT rc = GetNationCardRect(clientW, clientH, i);
        if (PtInRect(&rc, pt))
            return i;
    }
    return -1;
}

static void DrawNationSelectTextures()
{
    if (!g_nationSelectActive || !g_hWnd)
        return;

    EnsureNationSelectAssets();
    RECT rc = {};
    GetClientRect(g_hWnd, &rc);
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0)
        return;

    for (int i = 0; i < kNationSelectCount; ++i)
    {
        RECT card = GetNationCardRect(w, h, i);
        const bool selected = (i == g_selectedNationIndex);
        DrawSolidQuad((float)card.left, (float)card.top,
                      (float)(card.right - card.left), (float)(card.bottom - card.top),
                      selected ? 0xB02E2A24 : 0xA0202230);

        noesisTex_t *pCrest = FindTitleTexture(g_pTitleUiModel, kNationSelectInfo[i].textureName);
        if (!pCrest || !pCrest->pD3DTex)
            continue;

        const int crestAvail = IntMax(32, card.right - card.left - 48);
        const float crestSize = (float)IntMin(crestAvail, IntMax(96, h / 6));
        const float crestX = (float)(card.left + card.right) * 0.5f - crestSize * 0.5f;
        const float crestY = (float)card.top + (float)IntMax(26, h / 38);
        DrawTextureRegion(pCrest, crestX, crestY, crestSize, crestSize,
                          0.0f, 0.0f, (float)pCrest->w, (float)pCrest->h, 0xFFFFFFFF);
    }
}

static void DrawNationSelectOverlay()
{
    if (!g_nationSelectActive || !g_hWnd)
        return;

    RECT rc = {};
    GetClientRect(g_hWnd, &rc);
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0)
        return;

    HDC hdc = GetDC(g_hWnd);
    if (!hdc)
        return;

    HFONT hTitleFont = CreateFontA(-IntMax(28, h / 20), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_ROMAN, "Georgia");
    RECT titleRc = { 0, IntMax(20, h / 35), w, IntMax(82, h / 9) };
    DrawShadowText(hdc, hTitleFont, "Choose Your Starting Nation", titleRc,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(245, 241, 220), 2);
    DeleteObject(hTitleFont);

    HFONT hNameFont = CreateFontA(-IntMax(18, h / 38), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_ROMAN, "Georgia");
    HFONT hSubFont = CreateFontA(-IntMax(14, h / 52), 0, 0, 0, FW_NORMAL, TRUE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, "Arial");
    HFONT hBodyFont = CreateFontA(-IntMax(14, h / 55), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, "Arial");

    for (int i = 0; i < kNationSelectCount; ++i)
    {
        RECT card = GetNationCardRect(w, h, i);
        const bool selected = (i == g_selectedNationIndex);
        HBRUSH hCard = (HBRUSH)GetStockObject(HOLLOW_BRUSH);
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hCard);
        HPEN hPen = CreatePen(PS_SOLID, selected ? 3 : 1,
                              selected ? kNationSelectInfo[i].accent : RGB(112, 116, 128));
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
        RoundRect(hdc, card.left, card.top, card.right, card.bottom, 8, 8);
        SelectObject(hdc, hOldPen);
        SelectObject(hdc, hOldBrush);
        DeleteObject(hPen);

        RECT nameRc = { card.left + 16, card.top + IntMax(132, h / 5),
                        card.right - 16, card.top + IntMax(182, h / 4) };
        DrawShadowText(hdc, hNameFont, kNationSelectInfo[i].name, nameRc,
                       DT_CENTER | DT_WORDBREAK, RGB(255, 255, 255), 1);

        RECT subRc = { card.left + 18, nameRc.bottom + 2, card.right - 18, nameRc.bottom + IntMax(34, h / 28) };
        DrawShadowText(hdc, hSubFont, kNationSelectInfo[i].subtitle, subRc,
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE, kNationSelectInfo[i].accent, 1);
    }

    RECT descRc = { IntMax(80, w / 12), h - IntMax(170, h / 4),
                    w - IntMax(80, w / 12), h - IntMax(62, h / 12) };
    HBRUSH hDesc = CreateSolidBrush(RGB(20, 22, 34));
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hDesc);
    HPEN hDescPen = CreatePen(PS_SOLID, 1, RGB(120, 126, 155));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hDescPen);
    RoundRect(hdc, descRc.left, descRc.top, descRc.right, descRc.bottom, 8, 8);
    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hDescPen);
    DeleteObject(hDesc);

    RECT bodyRc = { descRc.left + 24, descRc.top + 18, descRc.right - 24, descRc.bottom - 18 };
    DrawShadowText(hdc, hBodyFont, kNationSelectInfo[g_selectedNationIndex].description, bodyRc,
                   DT_CENTER | DT_VCENTER | DT_WORDBREAK, RGB(235, 238, 245), 1);

    HFONT hStatusFont = CreateFontA(-IntMax(13, h / 60), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_MODERN, "Consolas");
    RECT bottomRc = { 0, h - IntMax(28, h / 30), w, h };
    HBRUSH hBottom = CreateSolidBrush(RGB(42, 42, 92));
    FillRect(hdc, &bottomRc, hBottom);
    DeleteObject(hBottom);
    DrawShadowText(hdc, hStatusFont, "Click a nation to select it. Press Enter to confirm, or Backspace to return.",
                   bottomRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(255, 255, 255), 1);
    DeleteObject(hStatusFont);

    DeleteObject(hBodyFont);
    DeleteObject(hSubFont);
    DeleteObject(hNameFont);
    ReleaseDC(g_hWnd, hdc);
}

static void DrawTitleScreenTextures()
{
    if (!g_titleScreenActive || !g_hWnd)
        return;

    RECT rc = {};
    GetClientRect(g_hWnd, &rc);
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0)
        return;

    noesisTex_t *pLogoTextTex = FindTitleTexture(g_pTitleLogoModel, "ffxi");
    noesisTex_t *pTitleAtlasTex = FindTitleTexture(g_pTitleLogoMarkModel, "titlwin");
    noesisTex_t *pLogoGraphicTex = FindTitleTexture(g_pTitleLogoMarkModel, "xilogo");
    if (pTitleAtlasTex && pTitleAtlasTex->pD3DTex)
    {
        const float srcX = 36.0f;
        const float srcY = 310.0f;
        const float srcW = 935.0f;
        const float srcH = 520.0f;
        const float logoW = (float)IntMax(760, (w * 58) / 100);
        const float logoH = logoW * (srcH / srcW);
        const float logoX = (float)(w / 9);
        const float logoY = (float)(h / 3) - logoH * 0.60f;
        DrawTexturedQuadUV(pTitleAtlasTex->pD3DTex, logoX, logoY, logoW, logoH,
                           srcX / (float)pTitleAtlasTex->w,
                           srcY / (float)pTitleAtlasTex->h,
                           (srcX + srcW) / (float)pTitleAtlasTex->w,
                           (srcY + srcH) / (float)pTitleAtlasTex->h,
                           0xFFFFFFFF, UsesFfxiDxt3Alpha(pTitleAtlasTex), 1.5f, 0.90f);
    }
    else if (pLogoGraphicTex && pLogoGraphicTex->pD3DTex)
    {
        const float graphicW = (float)IntMax(650, (w * 55) / 100);
        const float graphicH = graphicW * ((float)pLogoGraphicTex->h / (float)pLogoGraphicTex->w);
        const float graphicX = (float)(w / 7) - graphicW * 0.02f;
        const float graphicY = (float)(h / 3) - graphicH * 0.70f;
        DrawTexturedQuad(pLogoGraphicTex, graphicX, graphicY, graphicW, graphicH, 0xFFFFFFFF);
    }

    if (!pTitleAtlasTex && pLogoTextTex && pLogoTextTex->pD3DTex)
    {
        const float logoW = (float)IntMax(560, (w * 48) / 100);
        const float logoH = logoW * ((float)pLogoTextTex->h / (float)pLogoTextTex->w);
        const float logoX = (float)(w / 7);
        const float logoY = (float)(h / 3) - logoH * 0.05f;
        DrawTexturedQuad(pLogoTextTex, logoX, logoY, logoW, logoH, 0xFFFFFFFF);
    }

    const float plaqueW = (float)IntMax(260, (w * 3) / 16);
    const float plaqueH = (float)IntMax(38, h / 20);
    const float plaqueGap = (float)IntMax(7, h / 105);
    const float plaqueX = (float)(w - (int)plaqueW - IntMax(40, w / 16));
    const float plaqueY = (float)IntMax(55, h / 14);
    noesisTex_t *pExpansion1Tex = FindTitleTexture(g_pTitleLogoMarkModel, "ex1us");
    noesisTex_t *pExpansion2Tex = FindTitleTexture(g_pTitleLogoMarkModel, "ex2us");
    noesisTex_t *pExpansion5Tex = FindTitleTexture(g_pTitleLogoMarkModel, "ex5us");
    DrawTitleExpansionAtlas(pExpansion1Tex, 5, 0, plaqueX, plaqueY, plaqueW, plaqueH, plaqueGap);
    DrawTitleExpansionAtlas(pExpansion2Tex, 5, 5, plaqueX, plaqueY, plaqueW, plaqueH, plaqueGap);
    if (pExpansion5Tex)
    {
        DrawTextureRegion(pExpansion5Tex, plaqueX, plaqueY + (plaqueH + plaqueGap) * 10.0f,
                          plaqueW, plaqueH, 0.0f, 0.0f,
                          (float)pExpansion5Tex->w, (float)pExpansion5Tex->h, 0xFFFFFFFF);
    }

    noesisTex_t *pSecuredTex = FindTitleTexture(g_pTitleLogoMarkModel, "otp");
    if (!pSecuredTex)
        pSecuredTex = FindTitleTexture(g_pTitleUiModel, "otp");
    if (pSecuredTex)
    {
        const float iconW = (float)IntMax(78, w / 19);
        const float iconH = iconW * ((float)pSecuredTex->h / (float)pSecuredTex->w);
        DrawTextureRegion(pSecuredTex, (float)IntMax(52, w / 16), (float)(h - IntMax(145, h / 6)),
                          iconW, iconH, 0.0f, 0.0f,
                          (float)pSecuredTex->w, (float)pSecuredTex->h, 0xFFFFFFFF);
    }

    noesisTex_t *pButtonTex = FindTitleTexture(g_pTitleUiModel, "buttonto");
    if (!pButtonTex)
        pButtonTex = FindTitleTexture(g_pTitleUiModel, "lrbutton");
    if (pButtonTex && pButtonTex->pD3DTex)
    {
        int buttonX = 0, buttonY = 0, buttonW = 0, buttonH = 0;
        GetTitleButtonMetrics(w, h, &buttonX, &buttonY, &buttonW, &buttonH);
        const int selectedButton = GetTitleSelectedButtonIndex(w, h);
        for (int i = 0; i < 5; ++i)
        {
            DrawTitleButtonTexture(pButtonTex, (float)buttonX, (float)buttonY,
                                   (float)buttonW, (float)buttonH, i == selectedButton);
            buttonY += buttonH + 6;
        }
    }

    if (pTitleAtlasTex && pTitleAtlasTex->pD3DTex)
    {
        int buttonX = 0, buttonY = 0, buttonW = 0, buttonH = 0;
        GetTitleButtonMetrics(w, h, &buttonX, &buttonY, &buttonW, &buttonH);
        buttonY += (buttonH + 6) * 5;

        const float srcX = 670.0f;
        const float srcY = 20.0f;
        const float srcW = 295.0f;
        const float srcH = 32.0f;
        const float copyrightW = (float)IntMax(235, (buttonW * 11) / 10);
        const float copyrightH = copyrightW * (srcH / srcW);
        const float copyrightX = (float)buttonX + ((float)buttonW - copyrightW) * 0.5f;
        const float copyrightY = (float)buttonY + (float)IntMax(12, h / 80);
        DrawTextureRegion(pTitleAtlasTex, copyrightX, copyrightY, copyrightW, copyrightH,
                          srcX, srcY, srcW, srcH, 0xFFFFFFFF);
    }
}

static void DrawShadowText(HDC hdc, HFONT hFont, const char *text, RECT rc,
                           UINT format, COLORREF color, int shadowOffset)
{
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    SetBkMode(hdc, TRANSPARENT);

    RECT shadowRc = rc;
    OffsetRect(&shadowRc, shadowOffset, shadowOffset);
    SetTextColor(hdc, RGB(0, 0, 0));
    DrawTextA(hdc, text, -1, &shadowRc, format);

    SetTextColor(hdc, color);
    DrawTextA(hdc, text, -1, &rc, format);

    SelectObject(hdc, hOldFont);
}

static void DrawTitleButton(HDC hdc, RECT rc, const char *label, bool selected)
{
    HBRUSH hFill = CreateSolidBrush(selected ? RGB(136, 78, 35) : RGB(65, 64, 128));
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hFill);
    HPEN hPen = CreatePen(PS_SOLID, 2, selected ? RGB(235, 210, 132) : RGB(214, 214, 245));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 24, 24);
    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hPen);
    DeleteObject(hFill);

    const int fontH = IntMax(18, (rc.bottom - rc.top) - 8);
    HFONT hFont = CreateFontA(-fontH, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, "Arial");
    DrawShadowText(hdc, hFont, label, rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE,
                   RGB(255, 255, 255), 2);
    DeleteObject(hFont);
}

static void DrawExpansionPlaque(HDC hdc, RECT rc, const char *label, COLORREF color)
{
    HBRUSH hFill = CreateSolidBrush(RGB(226, 230, 226));
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hFill);
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(205, 210, 205));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hPen);
    DeleteObject(hFill);

    const int fontH = IntMax(15, (rc.bottom - rc.top) - 12);
    HFONT hFont = CreateFontA(-fontH, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_ROMAN, "Georgia");
    DrawShadowText(hdc, hFont, label, rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE,
                   color, 1);
    DeleteObject(hFont);
}

static void DrawTitleScreenOverlay()
{
    if (!g_titleScreenActive || !g_hWnd)
        return;

    RECT rc = {};
    GetClientRect(g_hWnd, &rc);
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0)
        return;

    HDC hdc = GetDC(g_hWnd);
    if (!hdc)
        return;

    const bool haveRealLogo = (FindTitleTexture(g_pTitleLogoModel, "ffxi") != nullptr) ||
                              (FindTitleTexture(g_pTitleLogoMarkModel, "titlwin") != nullptr);
    const bool haveRealButton = (FindTitleTexture(g_pTitleUiModel, "buttonto") != nullptr) ||
                                (FindTitleTexture(g_pTitleUiModel, "lrbutton") != nullptr);
    const bool haveRealCopyright = (FindTitleTexture(g_pTitleLogoMarkModel, "titlwin") != nullptr);
    const bool haveRealExpansions = (FindTitleTexture(g_pTitleLogoMarkModel, "ex1us") != nullptr) &&
                                    (FindTitleTexture(g_pTitleLogoMarkModel, "ex2us") != nullptr) &&
                                    (FindTitleTexture(g_pTitleLogoMarkModel, "ex5us") != nullptr);

    const int logoTop = h / 3;
    RECT logoRc = { w / 12, logoTop, (w * 7) / 10, logoTop + IntMax(76, h / 7) };
    if (!haveRealLogo)
    {
        HFONT hLogoFont = CreateFontA(-IntMax(64, h / 8), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_ROMAN, "Times New Roman");
        DrawShadowText(hdc, hLogoFont, "FINAL FANTASY XI", logoRc,
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(248, 248, 248), 3);
        DeleteObject(hLogoFont);

        RECT onlineRc = { logoRc.left, logoRc.bottom - 10, logoRc.right, logoRc.bottom + IntMax(28, h / 32) };
        HFONT hOnlineFont = CreateFontA(-IntMax(18, h / 36), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_ROMAN, "Georgia");
        DrawShadowText(hdc, hOnlineFont, "O N L I N E", onlineRc,
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(255, 255, 255), 2);
        DeleteObject(hOnlineFont);
    }

    int buttonX = 0, buttonY = 0, buttonW = 0, buttonH = 0;
    GetTitleButtonMetrics(w, h, &buttonX, &buttonY, &buttonW, &buttonH);
    const int selectedButton = GetTitleSelectedButtonIndex(w, h);
    const char *buttons[] = { "Select Character", "Create Character", "Delete Character", "Config", "Back" };
    const char *helpText[] =
    {
        "Select a character to log on with.",
        "Create a new character.",
        "Delete a character.",
        "Change the game's options.",
        "Return to the previous screen.",
    };
    HFONT hButtonFont = CreateFontA(-IntMax(18, buttonH - 8), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, "Arial");
    for (int i = 0; i < 5; ++i)
    {
        RECT br = { buttonX, buttonY, buttonX + buttonW, buttonY + buttonH };
        if (haveRealButton)
        {
            const COLORREF textColor = (i == selectedButton) ? RGB(255, 226, 102) : RGB(255, 255, 255);
            DrawShadowText(hdc, hButtonFont, buttons[i], br, DT_CENTER | DT_VCENTER | DT_SINGLELINE,
                           textColor, 2);
        }
        else
        {
            DrawTitleButton(hdc, br, buttons[i], i == selectedButton);
        }
        buttonY += buttonH + 6;
    }
    DeleteObject(hButtonFont);

    if (!haveRealCopyright)
    {
        RECT copyrightRc = { buttonX - 20, buttonY + 12, buttonX + buttonW + 20, buttonY + 40 };
        HFONT hCopyrightFont = CreateFontA(-IntMax(18, h / 34), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS, "Arial");
        DrawShadowText(hdc, hCopyrightFont, "(C) SQUARE ENIX", copyrightRc,
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(255, 255, 255), 2);
        DeleteObject(hCopyrightFont);
    }

    if (!haveRealExpansions)
    {
        const int plaqueW = IntMax(260, (w * 3) / 16);
        const int plaqueH = IntMax(34, h / 20);
        const int plaqueX = w - plaqueW - IntMax(40, w / 16);
        int plaqueY = IntMax(55, h / 14);
        struct Plaque { const char *label; COLORREF color; };
        const Plaque plaques[] =
        {
            { "Rise of the Zilart", RGB(84, 56, 26) },
            { "Chains of Promathia", RGB(73, 58, 84) },
            { "Treasures of Aht Urhgan", RGB(110, 78, 28) },
            { "Wings of the Goddess", RGB(41, 61, 92) },
            { "A Crystalline Prophecy", RGB(22, 87, 180) },
            { "A Moogle Kupo d'Etat", RGB(181, 26, 42) },
            { "A Shantotto Ascension", RGB(98, 54, 31) },
            { "Vision of Abyssea", RGB(0, 124, 179) },
            { "Scars of Abyssea", RGB(98, 64, 142) },
            { "Heroes of Abyssea", RGB(142, 36, 46) },
            { "Seekers of Adoulin", RGB(72, 132, 52) },
        };
        for (int i = 0; i < (int)(sizeof(plaques) / sizeof(plaques[0])); ++i)
        {
            RECT pr = { plaqueX, plaqueY, plaqueX + plaqueW, plaqueY + plaqueH };
            DrawExpansionPlaque(hdc, pr, plaques[i].label, plaques[i].color);
            plaqueY += plaqueH + IntMax(6, h / 120);
        }
    }

    RECT bottomRc = { 0, h - IntMax(26, h / 36), w, h };
    HBRUSH hBottom = CreateSolidBrush(RGB(42, 38, 74));
    FillRect(hdc, &bottomRc, hBottom);
    DeleteObject(hBottom);

    HFONT hStatusFont = CreateFontA(-IntMax(14, h / 48), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_MODERN, "Consolas");
    const char *statusText = (selectedButton >= 0) ? helpText[selectedButton] : helpText[0];
    DrawShadowText(hdc, hStatusFont, statusText, bottomRc,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(255, 255, 255), 1);
    DeleteObject(hStatusFont);

    ReleaseDC(g_hWnd, hdc);
}

static void Render()
{
    if (!g_pDevice)
        return;

    // Handle lost device
    HRESULT hr = g_pDevice->TestCooperativeLevel();
    if (hr == D3DERR_DEVICELOST)
    {
        g_deviceLost = true;
        Sleep(10);
        return;
    }
    if (hr == D3DERR_DEVICENOTRESET)
    {
        g_deviceLost = false;
        if (!ResetDevice())
        {
            Sleep(10);
            return;
        }
    }

    // Clear back buffer and depth/stencil
    g_pDevice->Clear(0, NULL,
        D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
        D3DCOLOR_XRGB(25, 32, 38),
        1.0f, 0);

    if (SUCCEEDED(g_pDevice->BeginScene()))
    {
        // ---- Camera setup ----
        // Orbit camera: position = target + spherical offset at (yaw, pitch, dist)
        const float cx = g_camTarget[0] + g_camDist * sinf(g_camYaw)  * cosf(g_camPitch);
        const float cy = g_camTarget[1] + g_camDist * sinf(g_camPitch);
        const float cz = g_camTarget[2] + g_camDist * cosf(g_camYaw)  * cosf(g_camPitch);

        D3DMATRIX view = BuildLookAtLH(cx, cy, cz,
                                        g_camTarget[0], g_camTarget[1], g_camTarget[2]);

        RECT rc = {};
        GetClientRect(g_hWnd, &rc);
        const float w = (float)(rc.right  - rc.left);
        const float h2 = (float)(rc.bottom - rc.top);
        const float aspect = (h2 > 0.0f) ? (w / h2) : (16.0f / 9.0f);

        const float drawDistance =
            (g_drawDistanceIndex >= 0 && g_drawDistanceIndex < kDrawDistanceOptionCount)
                ? kDrawDistanceOptions[g_drawDistanceIndex]
                : 2000.0f;
        D3DMATRIX proj  = BuildPerspectiveFovLH(3.14159265f * 0.25f, aspect, 0.01f, drawDistance);
        g_pDevice->SetTransform(D3DTS_VIEW,       &view);
        g_pDevice->SetTransform(D3DTS_PROJECTION, &proj);

        D3DMATRIX world = BuildIdentity();
        g_pDevice->SetTransform(D3DTS_WORLD,      &world);

        if (g_pZoneModel)
        {
            RenderModel(g_pZoneModel, true);
            RenderHighlightedZoneObject(g_pZoneModel);
            DrawNpcPlacementMarkers();
        }

        DrawCollisionGeometryOverlay();

        if (g_pPlayerModel)
        {
            D3DMATRIX playerWorld = BuildPlayerWorldMatrix();
            g_pDevice->SetTransform(D3DTS_WORLD, &playerWorld);
            RenderModel(g_pPlayerModel);
            g_pDevice->SetTransform(D3DTS_WORLD, &world);
        }

        DrawTitleScreenTextures();
        DrawNationSelectTextures();

        g_pDevice->EndScene();
    }

    g_pDevice->Present(NULL, NULL, NULL, NULL);
    DrawTitleScreenOverlay();
    DrawNationSelectOverlay();
}

//========================================================================================
// File open helpers
//========================================================================================

// Open a standard Windows file-open dialog filtered for .dat files.
// Returns true and fills outPath on success; returns false if the user cancelled.
static bool BrowseForDatFile(char *outPath, int outPathSize, const char *title)
{
    OPENFILENAMEA ofn = {};
    ofn.lStructSize     = sizeof(ofn);
    ofn.hwndOwner       = g_hWnd;
    ofn.lpstrFilter     = "DAT Files (*.dat)\0*.dat\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile       = outPath;
    ofn.nMaxFile        = outPathSize;
    ofn.lpstrTitle      = title;
    ofn.Flags           = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt     = "dat";
    // Seed the browser in the FFXI ROM folder if we know the install path
    ofn.lpstrInitialDir = (g_ffxiPath[0] != '\0') ? g_ffxiPath : NULL;
    outPath[0]          = '\0';
    return GetOpenFileNameA(&ofn) != 0;
}

// Open a file-open dialog filtered for the text-format DAT set files.
static bool BrowseForDatSetFile(char *outPath, int outPathSize)
{
    OPENFILENAMEA ofn = {};
    ofn.lStructSize     = sizeof(ofn);
    ofn.hwndOwner       = g_hWnd;
    ofn.lpstrFilter     = "FFXI DAT Set Files (*.ff11datset;*.txt)\0*.ff11datset;*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile       = outPath;
    ofn.nMaxFile        = outPathSize;
    ofn.lpstrTitle      = "Open FFXI DAT Set";
    ofn.Flags           = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrInitialDir = (g_ffxiPath[0] != '\0') ? g_ffxiPath : NULL;
    outPath[0]          = '\0';
    return GetOpenFileNameA(&ofn) != 0;
}

//========================================================================================
// Menu
//========================================================================================

static HMENU BuildStandaloneModelMenu(const FFXIStandaloneModelGroup *groups,
                                      int groupCount, UINT commandBase)
{
    HMENU catalogMenu = CreatePopupMenu();
    int flatIndex = 0;
    for (int groupIndex = 0; groupIndex < groupCount; ++groupIndex)
    {
        const FFXIStandaloneModelGroup &group = groups[groupIndex];
        HMENU groupMenu = CreatePopupMenu();
        for (int entryIndex = 0; entryIndex < group.count; ++entryIndex, ++flatIndex)
        {
            const FFXIStandaloneModelEntry &entry = group.entries[entryIndex];
            char fullPath[MAX_PATH] = {};
            BuildFFXIFullPath(entry.dat, fullPath, sizeof(fullPath));
            const UINT flags = FileExistsAPath(fullPath) ? MF_STRING : (MF_STRING | MF_GRAYED);
            AppendMenuA(groupMenu, flags, commandBase + flatIndex, entry.label);
        }
        AppendMenuA(catalogMenu, MF_POPUP, (UINT_PTR)groupMenu, group.name);
    }
    return catalogMenu;
}

static void LoadStandaloneModelEntry(const FFXIStandaloneModelEntry *entry, const char *kind)
{
    if (!entry)
        return;

    char fullPath[MAX_PATH] = {};
    BuildFFXIFullPath(entry->dat, fullPath, sizeof(fullPath));
    if (!FileExistsAPath(fullPath))
    {
        char message[256] = {};
        sprintf_s(message, "The selected %s DAT was not found under the configured FFXI path.",
                  kind ? kind : "model");
        MessageBoxA(g_hWnd, message, "Model Missing", MB_OK | MB_ICONWARNING);
        return;
    }

    UnloadPlayerModel();
    LoadDatFile(fullPath);
    if (!g_pZoneModel)
        return;

    SetLoadedZoneLabelFromNameAndPath(entry->label, fullPath);
    RememberLoadedZoneContext(fullPath, entry->label, true, false, false);
    SetGameModeMusic(0);
    ResetCameraForStandaloneModel(g_pZoneModel);
    InvalidateRect(g_hWnd, NULL, FALSE);
}

static int CompanionBrowserSelectedGroup(HWND hWnd)
{
    const LRESULT selection = SendDlgItemMessageA(
        hWnd, IDC_COMPANION_CATEGORY, CB_GETCURSEL, 0, 0);
    return selection >= 0 && selection < kFFXICompanionGroupCount ? (int)selection : -1;
}

static const FFXIStandaloneModelEntry *CompanionBrowserSelectedEntry(HWND hWnd)
{
    const int groupIndex = CompanionBrowserSelectedGroup(hWnd);
    if (groupIndex < 0)
        return nullptr;

    const FFXIStandaloneModelGroup &group = kFFXICompanionGroups[groupIndex];
    const LRESULT selection = SendDlgItemMessageA(
        hWnd, IDC_COMPANION_LIST, LB_GETCURSEL, 0, 0);
    if (selection < 0 || selection >= group.count)
        return nullptr;
    return &group.entries[selection];
}

static void UpdateCompanionBrowserSelection(HWND hWnd)
{
    const FFXIStandaloneModelEntry *entry = CompanionBrowserSelectedEntry(hWnd);
    char text[384] = "Select a companion to view its model.";
    bool available = false;
    if (entry)
    {
        char fullPath[MAX_PATH] = {};
        BuildFFXIFullPath(entry->dat, fullPath, sizeof(fullPath));
        available = FileExistsAPath(fullPath);
        sprintf_s(text, "%s\r\nDAT: %s%s", entry->label, entry->dat,
                  available ? "" : "  (not installed)");
    }
    SetDlgItemTextA(hWnd, IDC_COMPANION_PATH, text);
    EnableWindow(GetDlgItem(hWnd, IDC_COMPANION_LOAD), available ? TRUE : FALSE);
}

static void PopulateCompanionBrowserList(HWND hWnd)
{
    HWND hList = GetDlgItem(hWnd, IDC_COMPANION_LIST);
    if (!hList)
        return;

    SendMessageA(hList, LB_RESETCONTENT, 0, 0);
    const int groupIndex = CompanionBrowserSelectedGroup(hWnd);
    if (groupIndex >= 0)
    {
        const FFXIStandaloneModelGroup &group = kFFXICompanionGroups[groupIndex];
        for (int i = 0; i < group.count; ++i)
            SendMessageA(hList, LB_ADDSTRING, 0, (LPARAM)group.entries[i].label);
        if (group.count > 0)
            SendMessageA(hList, LB_SETCURSEL, 0, 0);
    }
    UpdateCompanionBrowserSelection(hWnd);
}

static void LoadSelectedCompanion(HWND hWnd)
{
    const FFXIStandaloneModelEntry *entry = CompanionBrowserSelectedEntry(hWnd);
    if (!entry)
        return;
    LoadStandaloneModelEntry(entry, "companion model");
}

static LRESULT CALLBACK CompanionBrowserProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        {
            HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HWND hTitle = AddPanelControl(hWnd, "STATIC",
                "Browse pets, mounts, and summoned companions.", 0, -1,
                14, 14, 390, 20);
            HWND hCategoryLabel = AddPanelControl(hWnd, "STATIC", "Category:", 0, -1,
                14, 44, 72, 18);
            HWND hCategory = AddPanelCombo(hWnd, IDC_COMPANION_CATEGORY, 88, 40, 316);
            HWND hList = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY,
                14, 76, 390, 292, hWnd, (HMENU)(INT_PTR)IDC_COMPANION_LIST,
                GetModuleHandle(NULL), NULL);
            HWND hPath = AddPanelControl(hWnd, "STATIC", "", SS_LEFT, IDC_COMPANION_PATH,
                14, 378, 390, 42);
            HWND hLoad = AddPanelControl(hWnd, "BUTTON", "Load in Viewer",
                BS_DEFPUSHBUTTON | WS_TABSTOP, IDC_COMPANION_LOAD, 178, 430, 128, 28);
            HWND hClose = AddPanelControl(hWnd, "BUTTON", "Close",
                BS_PUSHBUTTON | WS_TABSTOP, IDC_COMPANION_CLOSE, 314, 430, 90, 28);

            HWND controls[] = { hTitle, hCategoryLabel, hCategory, hList, hPath, hLoad, hClose };
            for (HWND control : controls)
            {
                if (control)
                    SendMessageA(control, WM_SETFONT, (WPARAM)hFont, TRUE);
            }

            for (int i = 0; i < kFFXICompanionGroupCount; ++i)
                SendMessageA(hCategory, CB_ADDSTRING, 0, (LPARAM)kFFXICompanionGroups[i].name);
            SendMessageA(hCategory, CB_SETCURSEL, 0, 0);
            PopulateCompanionBrowserList(hWnd);
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_COMPANION_CATEGORY && HIWORD(wParam) == CBN_SELCHANGE)
        {
            PopulateCompanionBrowserList(hWnd);
            return 0;
        }
        if (LOWORD(wParam) == IDC_COMPANION_LIST && HIWORD(wParam) == LBN_SELCHANGE)
        {
            UpdateCompanionBrowserSelection(hWnd);
            return 0;
        }
        if ((LOWORD(wParam) == IDC_COMPANION_LIST && HIWORD(wParam) == LBN_DBLCLK) ||
            (LOWORD(wParam) == IDC_COMPANION_LOAD && HIWORD(wParam) == BN_CLICKED))
        {
            LoadSelectedCompanion(hWnd);
            return 0;
        }
        if (LOWORD(wParam) == IDC_COMPANION_CLOSE && HIWORD(wParam) == BN_CLICKED)
        {
            ShowWindow(hWnd, SW_HIDE);
            return 0;
        }
        break;

    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        if (g_hCompanionBrowser == hWnd)
            g_hCompanionBrowser = NULL;
        return 0;
    }

    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

static void ShowCompanionBrowser()
{
    if (g_hCompanionBrowser && IsWindow(g_hCompanionBrowser))
    {
        ShowWindow(g_hCompanionBrowser, SW_SHOW);
        SetForegroundWindow(g_hCompanionBrowser);
        return;
    }

    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = CompanionBrowserProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "DATuraCompanionBrowserClass";
    RegisterClassExA(&wc);

    RECT owner = {};
    GetWindowRect(g_hWnd, &owner);
    const int width = 438;
    const int height = 512;
    const int x = owner.left + 32;
    const int y = owner.top + 64;
    g_hCompanionBrowser = CreateWindowExA(WS_EX_TOOLWINDOW, wc.lpszClassName,
        "Companion Browser", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        x, y, width, height, g_hWnd, NULL, wc.hInstance, NULL);
    if (g_hCompanionBrowser)
        ShowWindow(g_hCompanionBrowser, SW_SHOW);
}

static void ApplyMenuBackgroundRecursive(HMENU hMenu)
{
    if (!hMenu)
        return;

    MENUINFO menuInfo = {};
    menuInfo.cbSize = sizeof(menuInfo);
    menuInfo.fMask = MIM_BACKGROUND;
    menuInfo.hbrBack = g_hThemeControlBrush;
    SetMenuInfo(hMenu, &menuInfo);

    const int count = GetMenuItemCount(hMenu);
    for (int i = 0; i < count; ++i)
        ApplyMenuBackgroundRecursive(GetSubMenu(hMenu, i));
}

static void RefreshMainMenuTheme()
{
    if (!g_hMainMenu)
        return;
    ApplyMenuBackgroundRecursive(g_hMainMenu);
    if (g_hWnd)
    {
        DrawMenuBar(g_hWnd);
        RedrawWindow(g_hWnd, NULL, NULL,
                     RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW);
    }
}

static void StyleOwnerDrawMenuRecursive(HMENU hMenu, bool menuBar)
{
    if (!hMenu)
        return;

    const int count = GetMenuItemCount(hMenu);
    for (int i = 0; i < count; ++i)
    {
        HMENU hSubMenu = GetSubMenu(hMenu, i);
        if (hSubMenu)
            StyleOwnerDrawMenuRecursive(hSubMenu, false);

        MENUITEMINFOA current = {};
        current.cbSize = sizeof(current);
        current.fMask = MIIM_FTYPE;
        if (!GetMenuItemInfoA(hMenu, i, TRUE, &current))
            continue;

        const int textLength = GetMenuStringA(hMenu, i, NULL, 0, MF_BYPOSITION);
        std::vector<char> text((size_t)(textLength > 0 ? textLength : 0) + 1, '\0');
        if (textLength > 0)
            GetMenuStringA(hMenu, i, text.data(), (int)text.size(), MF_BYPOSITION);

        std::unique_ptr<MenuVisualEntry> entry(new MenuVisualEntry());
        entry->text = text.data();
        entry->separator = (current.fType & MFT_SEPARATOR) != 0;
        entry->hasSubmenu = hSubMenu != NULL;
        entry->menuBarItem = menuBar;
        MenuVisualEntry *entryPtr = entry.get();
        g_menuVisualEntries.push_back(std::move(entry));

        MENUITEMINFOA themed = {};
        themed.cbSize = sizeof(themed);
        themed.fMask = MIIM_FTYPE | MIIM_DATA | MIIM_STRING;
        themed.fType = current.fType | MFT_OWNERDRAW;
        themed.dwItemData = (ULONG_PTR)entryPtr;
        themed.dwTypeData = (LPSTR)entryPtr->text.c_str();
        themed.cch = (UINT)entryPtr->text.size();
        SetMenuItemInfoA(hMenu, i, TRUE, &themed);
    }
}

static void MeasureOwnerDrawMenuItem(MEASUREITEMSTRUCT *measure)
{
    MenuVisualEntry *entry = measure
        ? (MenuVisualEntry *)measure->itemData
        : NULL;
    if (!entry)
        return;
    if (entry->separator)
    {
        measure->itemWidth = 24;
        measure->itemHeight = 9;
        return;
    }

    HDC hdc = GetDC(g_hWnd);
    HFONT oldFont = (HFONT)SelectObject(hdc, g_hThemeFont);
    const char *tab = strchr(entry->text.c_str(), '\t');
    const int leftLength = tab
        ? (int)(tab - entry->text.c_str())
        : (int)entry->text.size();
    SIZE left = {};
    SIZE right = {};
    GetTextExtentPoint32A(hdc, entry->text.c_str(), leftLength, &left);
    if (tab && tab[1])
        GetTextExtentPoint32A(hdc, tab + 1, (int)strlen(tab + 1), &right);
    SelectObject(hdc, oldFont);
    ReleaseDC(g_hWnd, hdc);

    if (entry->menuBarItem)
    {
        measure->itemWidth = left.cx + 22;
        measure->itemHeight = 24;
    }
    else
    {
        measure->itemWidth = left.cx + right.cx + (right.cx ? 44 : 0) + 56;
        measure->itemHeight = 28;
    }
}

static void DrawOwnerDrawMenuItem(const DRAWITEMSTRUCT *draw)
{
    MenuVisualEntry *entry = draw
        ? (MenuVisualEntry *)draw->itemData
        : NULL;
    if (!entry)
        return;

    const bool dark = IsDarkColorTheme();
    const bool selected = (draw->itemState & (ODS_SELECTED | ODS_HOTLIGHT)) != 0;
    const bool disabled = (draw->itemState & (ODS_DISABLED | ODS_GRAYED)) != 0;
    const COLORREF background = entry->menuBarItem
        ? (dark ? RGB(24, 29, 34) : RGB(244, 246, 248))
        : ThemeControlColor();
    HBRUSH backgroundBrush = CreateSolidBrush(background);
    FillRect(draw->hDC, &draw->rcItem, backgroundBrush);
    DeleteObject(backgroundBrush);

    RECT content = draw->rcItem;
    if (selected && !entry->separator)
    {
        InflateRect(&content, -3, -2);
        HBRUSH selectedBrush = CreateSolidBrush(
            dark ? RGB(52, 116, 181) : RGB(218, 234, 250));
        HPEN selectedPen = CreatePen(PS_SOLID, 1,
            dark ? RGB(67, 139, 211) : RGB(190, 216, 241));
        HGDIOBJ oldBrush = SelectObject(draw->hDC, selectedBrush);
        HGDIOBJ oldPen = SelectObject(draw->hDC, selectedPen);
        RoundRect(draw->hDC, content.left, content.top,
                  content.right, content.bottom, 7, 7);
        SelectObject(draw->hDC, oldPen);
        SelectObject(draw->hDC, oldBrush);
        DeleteObject(selectedPen);
        DeleteObject(selectedBrush);
    }

    if (entry->separator)
    {
        const int y = (draw->rcItem.top + draw->rcItem.bottom) / 2;
        HPEN separatorPen = CreatePen(PS_SOLID, 1,
            dark ? RGB(62, 70, 78) : RGB(214, 219, 224));
        HPEN oldPen = (HPEN)SelectObject(draw->hDC, separatorPen);
        MoveToEx(draw->hDC, draw->rcItem.left + 30, y, NULL);
        LineTo(draw->hDC, draw->rcItem.right - 10, y);
        SelectObject(draw->hDC, oldPen);
        DeleteObject(separatorPen);
        return;
    }

    const COLORREF textColor = disabled
        ? ThemeMutedTextColor()
        : (selected && dark ? RGB(255, 255, 255) : ThemeTextColor());
    SetTextColor(draw->hDC, textColor);
    SetBkMode(draw->hDC, TRANSPARENT);
    HFONT oldFont = (HFONT)SelectObject(draw->hDC, g_hThemeFont);

    const char *tab = strchr(entry->text.c_str(), '\t');
    RECT textRect = draw->rcItem;
    if (entry->menuBarItem)
    {
        DrawTextA(draw->hDC, entry->text.c_str(),
                  tab ? (int)(tab - entry->text.c_str()) : -1,
                  &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    else
    {
        textRect.left += 30;
        textRect.right -= 22;
        DrawTextA(draw->hDC, entry->text.c_str(),
                  tab ? (int)(tab - entry->text.c_str()) : -1,
                  &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        if (tab && tab[1])
            DrawTextA(draw->hDC, tab + 1, -1, &textRect,
                      DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }
    SelectObject(draw->hDC, oldFont);

    if (!entry->menuBarItem && (draw->itemState & ODS_CHECKED))
    {
        HPEN checkPen = CreatePen(PS_SOLID, 2, textColor);
        HPEN oldPen = (HPEN)SelectObject(draw->hDC, checkPen);
        const int midY = (draw->rcItem.top + draw->rcItem.bottom) / 2;
        MoveToEx(draw->hDC, draw->rcItem.left + 9, midY, NULL);
        LineTo(draw->hDC, draw->rcItem.left + 13, midY + 4);
        LineTo(draw->hDC, draw->rcItem.left + 20, midY - 5);
        SelectObject(draw->hDC, oldPen);
        DeleteObject(checkPen);
    }

}

static HMENU BuildMenuBar()
{
    HMENU hMenuBar      = CreateMenu();
    HMENU hFileMenu     = CreatePopupMenu();
    HMENU hSettingsMenu = CreatePopupMenu();
    HMENU hViewMenu     = CreatePopupMenu();
    HMENU hResourceMenu = CreatePopupMenu();
    HMENU hImageMenu    = CreatePopupMenu();
    HMENU hAudioMenu    = CreatePopupMenu();
    HMENU hCompanionMenu = CreatePopupMenu();

    AppendMenuA(hFileMenu, MF_STRING,    IDM_FILE_OPEN_DAT,    "Open DAT...\tCtrl+O");
    AppendMenuA(hFileMenu, MF_STRING,    IDM_FILE_OPEN_DATSET, "Open DAT Set...");
    AppendMenuA(hFileMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(hFileMenu, MF_STRING,    IDM_FILE_RETURN_TITLE, "Return to Title Screen");
    AppendMenuA(hFileMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(hFileMenu, MF_STRING,    IDM_FILE_EXIT,        "Exit\tAlt+F4");

    AppendMenuA(hSettingsMenu, MF_STRING, IDM_SETTINGS_CONFIG,      "Config...");
    AppendMenuA(hSettingsMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(hSettingsMenu, MF_STRING, IDM_SETTINGS_SET_PATH,    "Set FFXI Path...");
    AppendMenuA(hSettingsMenu, MF_STRING, IDM_SETTINGS_DETECT_PATH, "Auto-Detect Path");
    AppendMenuA(hSettingsMenu, MF_STRING, IDM_SETTINGS_RESET_PATH,  "Reset to Default Path");
    AppendMenuA(hSettingsMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(hSettingsMenu, MF_STRING, IDM_SETTINGS_SHOW_PATH,   "Show Current Path");
    AppendMenuA(hSettingsMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(hSettingsMenu,
                MF_STRING | (g_enableMipMapping ? MF_CHECKED : MF_UNCHECKED),
                IDM_SETTINGS_MIP_MAPPING,
                "Enable MIP Mapping");
    AppendMenuA(hSettingsMenu,
                MF_STRING | (g_enableBumpMapping ? MF_CHECKED : MF_UNCHECKED),
                IDM_SETTINGS_BUMP_MAPPING,
                "Enable Bump Mapping");
    AppendMenuA(hSettingsMenu,
                MF_STRING | (g_mirrorWorldZones ? MF_CHECKED : MF_UNCHECKED),
                IDM_SETTINGS_MIRROR_WORLD,
                "Mirror World Zones");
    AppendMenuA(hSettingsMenu,
                MF_STRING | (g_environmentalAnimationMode != kDATuraEnvAnim_Off ? MF_CHECKED : MF_UNCHECKED),
                IDM_SETTINGS_ENV_ANIM,
                "Vegetation Animation: Smooth");

    AppendMenuA(hViewMenu,
                MF_STRING | (IsGameMode() ? MF_CHECKED : MF_UNCHECKED),
                IDM_VIEW_TOGGLE_EDIT_GAME_MODE,
                "Toggle Edit/Game Mode\tF");
    AppendMenuA(hViewMenu, MF_STRING, IDM_VIEW_ZONE_OBJECTS, "Zone Objects...");

    AppendMenuA(hResourceMenu, MF_STRING, IDM_RESOURCE_CURRENT_ZONE, "Current Zone Dialog / NPCs...");
    AppendMenuA(hResourceMenu, MF_STRING, IDM_RESOURCE_OPEN_DAT, "Open Resource DAT...");

    AppendMenuA(hImageMenu, MF_STRING, IDM_TEXTURE_VIEWER, "Image / Texture Viewer...");

    AppendMenuA(hAudioMenu, MF_STRING, IDM_AUDIO_PLAYER, "Music / SFX Player...");
    AppendMenuA(hAudioMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(hAudioMenu, MF_STRING, IDM_AUDIO_STOP, "Stop Playback");

    AppendMenuA(hCompanionMenu, MF_STRING, IDM_COMPANION_BROWSER, "Companion Browser...");

    // ---- Zones menu ----
    HMENU hZonesMenu = CreatePopupMenu();
    struct ZoneMenuCategory
    {
        std::string name;
        HMENU       menu;
    };
    std::vector<ZoneMenuCategory> zoneCategories;

    for (int g = 0; g < kFFXIZoneGroupCount; ++g)
    {
        const FFXIZoneGroup& grp = kFFXIZoneGroups[g];
        const std::string groupName = grp.name ? grp.name : "";
        const size_t slash = groupName.find(" / ");
        const std::string categoryName = (slash != std::string::npos) ? groupName.substr(0, slash) : "Other";
        const std::string regionName = (slash != std::string::npos) ? groupName.substr(slash + 3) : groupName;

        HMENU hCategory = NULL;
        for (size_t c = 0; c < zoneCategories.size(); ++c)
        {
            if (zoneCategories[c].name == categoryName)
            {
                hCategory = zoneCategories[c].menu;
                break;
            }
        }
        if (!hCategory)
        {
            hCategory = CreatePopupMenu();
            zoneCategories.push_back({ categoryName, hCategory });
            AppendMenuA(hZonesMenu, MF_POPUP, (UINT_PTR)hCategory, categoryName.c_str());
        }

        HMENU hSub = CreatePopupMenu();
        for (int z = 0; z < grp.count; ++z)
        {
            const int id = grp.ids[z];
            const FFXIZoneEntry* pZone = FFXIZone::FindByID(id);
            if (!pZone) continue;

            char label[128];
            UINT flags = MF_STRING;
            char resolvedFullPath[MAX_PATH] = {};
            const bool hasResolvableModel = FFXIZone::HasModel(pZone) ||
                                            ResolveZoneModelPath(id, resolvedFullPath, sizeof(resolvedFullPath));
            if (!hasResolvableModel) flags |= MF_GRAYED;
            sprintf_s(label, "[%d] %s", id, pZone->name);
            AppendMenuA(hSub, flags, IDM_ZONE_BASE + id, label);
        }
        AppendMenuA(hCategory, MF_POPUP, (UINT_PTR)hSub, regionName.c_str());
    }

    // Prototype maps have no stable retail zone IDs, so expose them through
    // direct DAT paths rather than forcing them into kFFXIZoneTable.
    HMENU hInternalCategory = NULL;
    for (size_t c = 0; c < zoneCategories.size(); ++c)
    {
        if (zoneCategories[c].name == "Internal")
        {
            hInternalCategory = zoneCategories[c].menu;
            break;
        }
    }
    if (!hInternalCategory)
    {
        hInternalCategory = CreatePopupMenu();
        zoneCategories.push_back({ "Internal", hInternalCategory });
        AppendMenuA(hZonesMenu, MF_POPUP, (UINT_PTR)hInternalCategory, "Internal");
    }

    HMENU hPrototypeAreas = CreatePopupMenu();
    for (int i = 0; i < kFFXIPrototypeAreaCount; ++i)
    {
        const FFXIPrototypeAreaEntry& area = kFFXIPrototypeAreas[i];
        // Keep direct-path entries selectable. The command handler validates
        // the DAT and reports its precise failure instead of silently greying
        // an entry while the menu bar is being constructed.
        AppendMenuA(hPrototypeAreas, MF_STRING, IDM_PROTOTYPE_AREA_BASE + i, area.name);
    }
    AppendMenuA(hInternalCategory, MF_POPUP, (UINT_PTR)hPrototypeAreas, "Prototype Areas");

    // ---- PC creation models menu (high-poly character creation DATs) ----
    HMENU hCreationMenu = CreatePopupMenu();
    int flatIdx = 0;
    for (int r = 0; r < kFFXICreationRaceCount; ++r)
    {
        const FFXICreationRace& race = kFFXICreationRaces[r];
        HMENU hRace = CreatePopupMenu();
        for (int e = 0; e < race.count; ++e)
        {
            const char *entryLabel = race.entries[e].label ? race.entries[e].label : "Face";
            const char *faceLabel = strstr(entryLabel, " + ");
            faceLabel = faceLabel ? faceLabel + 3 : entryLabel;

            HMENU hFace = CreatePopupMenu();
            for (int variant = 0; variant < 2 && e + variant < race.count; ++variant)
            {
                const FFXICreationEntry& entry = race.entries[e + variant];
                const char *equipmentLabel =
                    (entry.label && strncmp(entry.label, "Initial Equipment", 17) == 0)
                    ? "Initial Equipment"
                    : "No Equipment";
                AppendMenuA(hFace, MF_STRING, IDM_CREATION_CHAR_BASE + flatIdx, equipmentLabel);
                ++flatIdx;
            }

            AppendMenuA(hRace, MF_POPUP, (UINT_PTR)hFace, faceLabel);
            ++e;
        }
        AppendMenuA(hCreationMenu, MF_POPUP, (UINT_PTR)hRace, race.name);
    }

    // ---- PC in-game models menu (low-poly composed player models) ----
    HMENU hPlayerMenu = CreatePopupMenu();
    AppendMenuA(hPlayerMenu, MF_STRING, IDM_PLAYER_CUSTOMIZE, "Equipment / Animation...");
    AppendMenuA(hPlayerMenu, MF_SEPARATOR, 0, NULL);
    for (int r = 0; r < kFFXICharRaceCount; ++r)
    {
        AppendMenuA(hPlayerMenu, MF_STRING, IDM_PLAYER_BASE + r, kFFXICharRaces[r].name);
    }

    HMENU hNpcMenu = BuildStandaloneModelMenu(
        kFFXINpcModelGroups, kFFXINpcModelGroupCount, IDM_NPC_MODEL_BASE);
    HMENU hMonsterMenu = BuildStandaloneModelMenu(
        kFFXIMonsterModelGroups, kFFXIMonsterModelGroupCount, IDM_MONSTER_MODEL_BASE);

    HMENU hAssetsMenu = CreatePopupMenu();
    HMENU hPlayerModelsMenu = CreatePopupMenu();
    HMENU hToolsMenu = CreatePopupMenu();

    AppendMenuA(hPlayerModelsMenu, MF_POPUP, (UINT_PTR)hCreationMenu, "High Poly");
    AppendMenuA(hPlayerModelsMenu, MF_POPUP, (UINT_PTR)hPlayerMenu,   "Low Poly");

    AppendMenuA(hAssetsMenu, MF_POPUP, (UINT_PTR)hZonesMenu,        "Zones");
    AppendMenuA(hAssetsMenu, MF_POPUP, (UINT_PTR)hPlayerModelsMenu, "Player Models");
    AppendMenuA(hAssetsMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(hAssetsMenu, MF_POPUP, (UINT_PTR)hNpcMenu,          "NPCs");
    AppendMenuA(hAssetsMenu, MF_POPUP, (UINT_PTR)hMonsterMenu,      "Monsters");
    AppendMenuA(hAssetsMenu, MF_POPUP, (UINT_PTR)hCompanionMenu,    "Companions");

    AppendMenuA(hToolsMenu, MF_POPUP, (UINT_PTR)hResourceMenu, "Resources");
    AppendMenuA(hToolsMenu, MF_POPUP, (UINT_PTR)hImageMenu,    "Images / Textures");
    AppendMenuA(hToolsMenu, MF_POPUP, (UINT_PTR)hAudioMenu,    "Audio");

    AppendMenuA(hMenuBar, MF_POPUP, (UINT_PTR)hFileMenu,     "File");
    AppendMenuA(hMenuBar, MF_POPUP, (UINT_PTR)hAssetsMenu,   "Assets");
    AppendMenuA(hMenuBar, MF_POPUP, (UINT_PTR)hToolsMenu,    "Tools");
    AppendMenuA(hMenuBar, MF_POPUP, (UINT_PTR)hViewMenu,     "View");
    AppendMenuA(hMenuBar, MF_POPUP, (UINT_PTR)hSettingsMenu, "Settings");
    g_menuVisualEntries.clear();
    StyleOwnerDrawMenuRecursive(hMenuBar, true);
    g_hMainMenu = hMenuBar;
    ApplyMenuBackgroundRecursive(g_hMainMenu);
    return hMenuBar;
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
                MeasureOwnerDrawMenuItem(measure);
                return TRUE;
            }
        }
        break;

    case WM_DRAWITEM:
        {
            const DRAWITEMSTRUCT *draw = (const DRAWITEMSTRUCT *)lParam;
            if (draw && draw->CtlType == ODT_MENU)
            {
                DrawOwnerDrawMenuItem(draw);
                return TRUE;
            }
        }
        break;

    case WM_ACTIVATEAPP:
        SyncAppMusic();
        return 0;

    case WM_DATURA_AUDIO_PLAYER_CLOSED:
        SyncAppMusic();
        return 0;

    case WM_SIZE:
        // Ignore if minimized or if the device doesn't exist yet
        if (g_pDevice && wParam != SIZE_MINIMIZED)
        {
            const int w = LOWORD(lParam);
            const int h = HIWORD(lParam);
            if (w > 0 && h > 0)
            {
                BuildPresentParams(w, h);
                ResetDevice();
            }
        }
        return 0;

    case WM_COMMAND:
        {
            const WORD cmd = LOWORD(wParam);

            const int npcModelCount = FFXIStandaloneModel_TotalEntries(
                kFFXINpcModelGroups, kFFXINpcModelGroupCount);
            if (cmd >= IDM_NPC_MODEL_BASE && cmd < IDM_NPC_MODEL_BASE + npcModelCount)
            {
                const FFXIStandaloneModelEntry *entry = FFXIStandaloneModel_GetEntry(
                    kFFXINpcModelGroups, kFFXINpcModelGroupCount, cmd - IDM_NPC_MODEL_BASE);
                LoadStandaloneModelEntry(entry, "NPC model");
                return 0;
            }

            const int monsterModelCount = FFXIStandaloneModel_TotalEntries(
                kFFXIMonsterModelGroups, kFFXIMonsterModelGroupCount);
            if (cmd >= IDM_MONSTER_MODEL_BASE &&
                cmd < IDM_MONSTER_MODEL_BASE + monsterModelCount)
            {
                const FFXIStandaloneModelEntry *entry = FFXIStandaloneModel_GetEntry(
                    kFFXIMonsterModelGroups, kFFXIMonsterModelGroupCount,
                    cmd - IDM_MONSTER_MODEL_BASE);
                LoadStandaloneModelEntry(entry, "monster model");
                return 0;
            }

            // ---- Direct-path prototype area menu ----
            if (cmd >= IDM_PROTOTYPE_AREA_BASE &&
                cmd < IDM_PROTOTYPE_AREA_BASE + kFFXIPrototypeAreaCount)
            {
                const FFXIPrototypeAreaEntry& area =
                    kFFXIPrototypeAreas[cmd - IDM_PROTOTYPE_AREA_BASE];
                char fullPath[MAX_PATH] = {};
                BuildFFXIFullPath(area.modelDat, fullPath, sizeof(fullPath));
                if (FileExistsAPath(fullPath))
                {
                    LoadDatFile(fullPath);
                    SetLoadedZoneLabelFromNameAndPath(area.name, fullPath);
                    RememberLoadedZoneContext(fullPath, area.name, true, false, false);
                    SetGameModeMusic(0);
                }
                else
                {
                    MessageBoxA(g_hWnd, "This prototype DAT was not found under the configured FFXI path.",
                                "Prototype Area Missing", MB_OK | MB_ICONWARNING);
                }
                return 0;
            }

            // ---- Zone menu ----
            if (cmd >= IDM_ZONE_BASE && cmd < IDM_ZONE_BASE + kFFXIZoneCount)
            {
                const int zoneId = cmd - IDM_ZONE_BASE;
                const FFXIZoneEntry* pZone = FFXIZone::FindByID(zoneId);
                char fullPath[MAX_PATH] = {};
                if (pZone && ResolveZoneModelPath(zoneId, fullPath, sizeof(fullPath)))
                {
                    LoadDatFile(fullPath);
                    SetLoadedZoneLabelFromNameAndPath(pZone->name, fullPath);
                    RememberLoadedZoneContext(fullPath, pZone->name, true, false, false);

                    const FFXIZoneMusicEntry* pMusic = FFXIZoneMusic_FindByID(zoneId);
                    if (pMusic)
                    {
                        SetGameModeMusic(SelectZoneMusicId(pMusic));
                    }
                    else
                    {
                        SetGameModeMusic(0);
                    }
                }
                else
                {
                    MessageBoxA(g_hWnd, "This zone has no model DAT on record and could not be resolved from FTABLE/VTABLE.",
                                "No Model", MB_OK | MB_ICONWARNING);
                }
                return 0;
            }

            // ---- Full player model menu ----
            if (cmd >= IDM_PLAYER_BASE && cmd < IDM_PLAYER_BASE + kFFXICharRaceCount)
            {
                LoadPlayerRaceModel(cmd - IDM_PLAYER_BASE);
                ShowLowPolyControlPanel();
                return 0;
            }

            // ---- High-poly character creation mesh menu ----
            if (cmd >= IDM_CREATION_CHAR_BASE && cmd < IDM_CREATION_CHAR_BASE + FFXICreation_TotalEntries())
            {
                const int charIdx = cmd - IDM_CREATION_CHAR_BASE;
                if (SetHighPolyCreationSelectionFromFlatIndex(charIdx))
                {
                    if (g_hLowPolyPanel)
                        ShowWindow(g_hLowPolyPanel, SW_HIDE);

                    const FFXICreationEntry *pEntry = CurrentHighPolyCreationEntry();
                    if (pEntry)
                        LoadCreationEntry(pEntry);
                    ShowHighPolyCreationPanel();
                }
                return 0;
            }
        }
        switch (LOWORD(wParam))
        {
        case IDM_FILE_OPEN_DAT:
            {
                char path[MAX_PATH] = {};
                if (BrowseForDatFile(path, sizeof(path), "Open FFXI DAT"))
                    LoadDatFile(path);
            }
            return 0;

        case IDM_FILE_OPEN_DATSET:
            {
                char path[MAX_PATH] = {};
                if (BrowseForDatSetFile(path, sizeof(path)))
                    LoadDatSetFile(path);
            }
            return 0;

        case IDM_FILE_RETURN_TITLE:
            ReturnToTitleScreen();
            return 0;

        case IDM_FILE_EXIT:
            PostQuitMessage(0);
            return 0;

        case IDM_SETTINGS_CONFIG:
            ShowTitleConfigDialog();
            return 0;

        case IDM_SETTINGS_SET_PATH:
            PromptSetFFXIPath();
            SyncConfigDialogControls(g_hConfigDialog);
            return 0;

        case IDM_SETTINGS_DETECT_PATH:
            AutoDetectFFXIPath();
            SyncConfigDialogControls(g_hConfigDialog);
            return 0;

        case IDM_SETTINGS_RESET_PATH:
            ResetFFXIPathToDefault();
            SyncConfigDialogControls(g_hConfigDialog);
            return 0;

        case IDM_SETTINGS_SHOW_PATH:
            ShowCurrentFFXIPathDialog();
            return 0;

        case IDM_SETTINGS_MIP_MAPPING:
            g_enableMipMapping = !g_enableMipMapping;
            SyncSettingsMenuChecks();
            SyncConfigDialogControls(g_hConfigDialog);
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;

        case IDM_SETTINGS_BUMP_MAPPING:
            g_enableBumpMapping = !g_enableBumpMapping;
            SyncSettingsMenuChecks();
            SyncConfigDialogControls(g_hConfigDialog);
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;

        case IDM_SETTINGS_ENV_ANIM:
            SetEnvironmentalAnimationMode((g_environmentalAnimationMode + 1) % 3);
            SyncSettingsMenuChecks();
            SyncConfigDialogControls(g_hConfigDialog);
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;

        case IDM_SETTINGS_MIRROR_WORLD:
            g_mirrorWorldZones = !g_mirrorWorldZones;
            SyncSettingsMenuChecks();
            SyncConfigDialogControls(g_hConfigDialog);
            ReloadRememberedZone();
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;

        case IDM_VIEW_TOGGLE_EDIT_GAME_MODE:
            ToggleEditGameMode();
            SyncConfigDialogControls(g_hConfigDialog);
            return 0;

        case IDM_VIEW_ZONE_OBJECTS:
            ShowZoneObjectPanel();
            return 0;

        case IDM_RESOURCE_CURRENT_ZONE:
            ShowCurrentZoneResourceBrowser();
            return 0;

        case IDM_RESOURCE_OPEN_DAT:
            ShowResourceDatBrowser();
            return 0;

        case IDM_TEXTURE_VIEWER:
            TextureViewer_Show(g_hWnd, g_ffxiPath);
            return 0;

        case IDM_COMPANION_BROWSER:
            ShowCompanionBrowser();
            return 0;

        case IDM_AUDIO_PLAYER:
            AudioPlayer_Show(g_hWnd, g_ffxiPath);
            SyncAppMusic();
            return 0;

        case IDM_AUDIO_STOP:
            AudioPlayer_StopPlayback();
            return 0;

        case IDM_PLAYER_CUSTOMIZE:
            ShowLowPolyControlPanel();
            if (!g_pPlayerModel)
                ReloadPlayerModelFromControls();
            return 0;
        }
        break;

    case WM_LBUTTONDOWN:
        if (g_nationSelectActive)
        {
            RECT rc = {};
            GetClientRect(hWnd, &rc);
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            const int nationIndex = GetNationCardAtPoint(rc.right - rc.left, rc.bottom - rc.top, pt);
            if (nationIndex >= 0)
            {
                g_selectedNationIndex = nationIndex;
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
        }
        if (g_titleScreenActive)
        {
            RECT rc = {};
            GetClientRect(hWnd, &rc);
            const int buttonIndex = GetTitleSelectedButtonIndex(rc.right - rc.left, rc.bottom - rc.top);
            if (buttonIndex >= 0)
            {
                ActivateTitleButton(buttonIndex);
                return 0;
            }
        }
        break;

    // ---- Mouse camera orbit ----
    case WM_RBUTTONDOWN:
        g_mouseDown    = true;
        g_lastMouse.x  = (short)LOWORD(lParam);
        g_lastMouse.y  = (short)HIWORD(lParam);
        SetCapture(hWnd);
        SetCameraCursorHidden(true);
        return 0;

    case WM_RBUTTONUP:
        EndMouseLook();
        return 0;

    case WM_MBUTTONDOWN:
        if (g_highPolyCreationActive && !g_titleScreenActive)
        {
            g_mousePanDown = true;
            g_lastMouse.x  = (short)LOWORD(lParam);
            g_lastMouse.y  = (short)HIWORD(lParam);
            SetCapture(hWnd);
            return 0;
        }
        break;

    case WM_MBUTTONUP:
        if (g_mousePanDown)
        {
            EndMouseLook();
            return 0;
        }
        break;

    case WM_CAPTURECHANGED:
        if ((HWND)lParam != hWnd)
        {
            g_mouseDown = false;
            g_mousePanDown = false;
            SetCameraCursorHidden(false);
        }
        return 0;

    case WM_KILLFOCUS:
        EndMouseLook();
        break;

    case WM_MOUSEMOVE:
        g_mouseClient.x = (short)LOWORD(lParam);
        g_mouseClient.y = (short)HIWORD(lParam);
        {
            TRACKMOUSEEVENT tme = {};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hWnd;
            TrackMouseEvent(&tme);
        }
        if (g_mousePanDown)
        {
            const int mx = g_mouseClient.x;
            const int my = g_mouseClient.y;
            PanOrbitCameraByMouseDelta(mx - g_lastMouse.x, my - g_lastMouse.y);
            g_lastMouse.x = mx;
            g_lastMouse.y = my;
        }
        else if (g_mouseDown)
        {
            const int mx = g_mouseClient.x;
            const int my = g_mouseClient.y;
            const float dx = (float)(mx - g_lastMouse.x) * 0.005f;
            const float dy = (float)(my - g_lastMouse.y) * 0.005f;
            g_camYaw   -= dx;
            g_camPitch -= dy;
            if (g_camPitch >  1.55f) g_camPitch =  1.55f;
            if (g_camPitch < -1.55f) g_camPitch = -1.55f;
            g_lastMouse.x = mx;
            g_lastMouse.y = my;
        }
        return 0;

    case WM_MOUSELEAVE:
        g_mouseClient.x = -1;
        g_mouseClient.y = -1;
        return 0;

    case WM_MOUSEWHEEL:
        {
            const float delta = (float)(short)HIWORD(wParam) / (float)WHEEL_DELTA;
            g_camDist -= delta * g_camDist * 0.12f;
            if (g_camDist <    0.05f) g_camDist =    0.05f;
            if (g_camDist > 2000.0f)  g_camDist = 2000.0f;
        }
        return 0;

    // ---- Keyboard ----
    case WM_KEYDOWN:
        switch (wParam)
        {
        case VK_ESCAPE:
            PostQuitMessage(0);
            break;
        case VK_BACK:
            if (g_nationSelectActive)
            {
                g_nationSelectActive = false;
                ShowHighPolyCreationPanel();
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;
        case VK_RETURN:
            if (g_nationSelectActive)
            {
                LoadSelectedNationScene();
            }
            break;
        case 'O':
            // Ctrl+O — open DAT
            if (GetKeyState(VK_CONTROL) & 0x8000)
                SendMessageA(hWnd, WM_COMMAND, IDM_FILE_OPEN_DAT, 0);
            break;
        case 'F':
            SendMessageA(hWnd, WM_COMMAND, IDM_VIEW_TOGGLE_EDIT_GAME_MODE, 0);
            break;
        case 'G':
            UnstickPlayerFromGeometry();
            break;
        }
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

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE /*hPrevInstance*/,
                   _In_ LPSTR /*lpCmdLine*/, _In_ int nCmdShow)
{
    InitColorTheme();

    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES | ICC_TAB_CLASSES;
    InitCommonControlsEx(&icc);

    // Register window class
    WNDCLASSEXW wc   = {};
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = kWindowClassName;
    wc.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DATURA));
    wc.hIconSm       = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DATURA));

    if (!RegisterClassExW(&wc))
    {
        MessageBoxA(NULL, "RegisterClassExW failed.", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Calculate window size that gives us the desired client area
    RECT rc = { 0, 0, kDefaultWidth, kDefaultHeight };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, TRUE); // TRUE = has menu

    // Create window
    g_hWnd = CreateWindowExW(
        0,
        kWindowClassName,
        kWindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left,
        rc.bottom - rc.top,
        NULL, NULL, hInstance, NULL);

    if (!g_hWnd)
    {
        MessageBoxA(NULL, "CreateWindowEx failed.", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    ApplyColorTheme(g_hWnd);

    // Attach menu bar
    SetMenu(g_hWnd, BuildMenuBar());
    SetWindowTextW(g_hWnd, kWindowTitle);

    // Detect / load the FFXI install path before opening any file browsers
    InitFFXIPath();

    // Initialize Direct3D 9
    if (!InitD3D(kDefaultWidth, kDefaultHeight))
        return 1;

    LoadTitleScreen();

    QueryPerformanceFrequency(&g_perfFreq);
    QueryPerformanceCounter(&g_lastFrame);

    // Show window
    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    // Message + render loop
    MSG msg = {};
    for (;;)
    {
        // Drain the Windows message queue
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                goto cleanup;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        LARGE_INTEGER now = {};
        QueryPerformanceCounter(&now);
        float dt = (float)(now.QuadPart - g_lastFrame.QuadPart) / (float)g_perfFreq.QuadPart;
        g_lastFrame = now;
        if (dt > 0.1f) dt = 0.1f;
        if (dt > 0.0f)
        {
            UpdateCameraMovement(dt);
            UpdatePlayerAnimation(dt);
            UpdateHighPolyCreationAnimation(dt);
        }

        Render();
    }

cleanup:
    UnloadTitleAssets();
    UnloadPlayerModel();
    UnloadZoneModel();
    ShutdownD3D();
    if (g_hThemeWindowBrush)
        DeleteObject(g_hThemeWindowBrush);
    if (g_hThemeControlBrush)
        DeleteObject(g_hThemeControlBrush);
    if (g_hThemeEditBrush)
        DeleteObject(g_hThemeEditBrush);
    if (g_hThemeFont)
        DeleteObject(g_hThemeFont);
    if (g_hThemeSectionFont)
        DeleteObject(g_hThemeSectionFont);
    UnregisterClassW(kWindowClassName, hInstance);
    return static_cast<int>(msg.wParam);
}
