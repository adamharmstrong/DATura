#include "stdafx.h"
#include "config_dialog.h"
#include "ffxi_player_icons.h"
#include "ffxi_stat_system.h"
#include "ffxi_install_path.h"
#include <commctrl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "win32_panel_controls.h"
#include "win32_tool_window.h"

namespace
{
const char kWindowClassName[] = "DATuraConfigDialogClass";

enum ControlId
{
    IDC_CONFIG_PATH_TEXT = 8300,
    IDC_CONFIG_SET_PATH = 8301,
    IDC_CONFIG_DETECT_PATH = 8302,
    IDC_CONFIG_RESET_PATH = 8303,
    IDC_CONFIG_SHOW_PATH = 8304,
    IDC_CONFIG_MIP_MAPPING = 8305,
    IDC_CONFIG_BUMP_MAPPING = 8306,
    IDC_CONFIG_ENV_ANIM = 8307,
    IDC_CONFIG_EDIT_GAME = 8308,
    IDC_CONFIG_CLOSE = 8311,
    IDC_CONFIG_COLLISION = 8312,
    IDC_CONFIG_WINDOW_MODE = 8313,
    IDC_CONFIG_RESOLUTION = 8314,
    IDC_CONFIG_ENABLE_SOUNDS = 8315,
    IDC_CONFIG_BACKGROUND_SOUNDS = 8316,
    IDC_CONFIG_MAX_SOUNDS = 8317,
    IDC_CONFIG_HARDWARE_CURSOR = 8318,
    IDC_CONFIG_MIRROR_WORLD = 8319,
    IDC_CONFIG_DRAW_DISTANCE = 8320,
    IDC_CONFIG_TEXTURE_COMPRESSION = 8321,
    IDC_CONFIG_COLOR_THEME = 8322,
    IDC_CONFIG_LIGHTING_QUALITY = 8323,
    IDC_CONFIG_DOOR_INTERACTION = 8324,
    IDC_CONFIG_PLAYER_ICON = 8325,
    IDC_CONFIG_JOB_MASTER = 8326,
    IDC_CONFIG_SUBTITLE = 8327,
    IDC_CONFIG_SUBTITLE_LABEL = 8328,
    IDC_CONFIG_LINKSHELL_NAME = 8329,
    IDC_CONFIG_MAIN_JOB = 8330,
    IDC_CONFIG_MAIN_LEVEL = 8331,
    IDC_CONFIG_SUB_JOB = 8332,
    IDC_CONFIG_SUB_LEVEL = 8333,
    IDC_CONFIG_MAIN_LV_LABEL = 8334,
    IDC_CONFIG_SUB_LV_LABEL = 8335,
    IDC_CONFIG_JOB_SEPARATOR = 8336,
    IDC_CONFIG_CHAT_TIMEOUT = 8337,
    IDC_CONFIG_CHAT_WIDTH = 8338,
    IDC_CONFIG_CHAT_HEIGHT = 8339,
    IDC_CONFIG_RENDERING_BACKEND = 8340,
    IDC_CONFIG_LINKSHELL_COLOR_R = 8341,
    IDC_CONFIG_LINKSHELL_COLOR_G = 8342,
    IDC_CONFIG_LINKSHELL_COLOR_B = 8343,
    IDC_CONFIG_TAB_MENU = 8344,
    IDC_CONFIG_TAB_CHAT = 8345,
    IDC_CONFIG_TAB_NAMEPLATES = 8346,
    IDC_CONFIG_LIGHT_DIRECTION_MODE = 8347,
    IDC_CONFIG_LIGHT_AZIMUTH = 8348,
    IDC_CONFIG_LIGHT_ELEVATION = 8349,
    IDC_CONFIG_SCENE_CLOCK = 8350,
    IDC_CONFIG_CATEGORY_GENERAL = 8351,
    IDC_CONFIG_CATEGORY_GRAPHICS = 8352,
    IDC_CONFIG_CATEGORY_AUDIO = 8353,
    IDC_CONFIG_CATEGORY_INTERFACE = 8354,
    IDC_CONFIG_ENV_ANIM_LABEL = 8355,
    IDC_CONFIG_LIGHTING_LABEL = 8356,
    IDC_CONFIG_AZIMUTH_LABEL = 8357,
    IDC_CONFIG_ELEVATION_LABEL = 8358,
    IDC_CONFIG_TEXTURE_STORAGE_LABEL = 8359,
    IDC_CONFIG_DRAW_DISTANCE_LABEL = 8360,
    IDC_CONFIG_CURSOR_STYLE = 8361,
    IDC_CONFIG_CURSOR_STYLE_LABEL = 8362,
    IDC_CONFIG_BUMP_INTENSITY = 8363,
    IDC_CONFIG_BUMP_INTENSITY_LABEL = 8364,
    IDC_CONFIG_BUMP_INTENSITY_VALUE = 8365,
    IDC_CONFIG_CHAT_FONT = 8366,
    IDC_CONFIG_CHAT_FONT_SIZE = 8369,
    IDC_CONFIG_MOVEMENT_STYLE = 8367,
    IDC_CONFIG_MOVEMENT_STYLE_LABEL = 8368,
    IDC_CONFIG_TITLE_BACKGROUND_MODE = 8370,
    IDC_CONFIG_TITLE_BACKGROUND_ZONE = 8371,
    IDC_CONFIG_TITLE_BACKGROUND_LABEL = 8372,
    IDC_CONFIG_SHOW_TITLE_UI = 8390,
    IDC_CONFIG_TITLE_MUSIC_MODE = 8391,
    IDC_CONFIG_TITLE_MUSIC = 8392,
    IDC_CONFIG_TITLE_MUSIC_LABEL = 8393,
    IDC_CONFIG_APPLY = 8373,
    IDC_CONFIG_PLAYER_ICON_LABEL = 8374,
    IDC_CONFIG_LINKSHELL_COLOR_LABEL = 8375,
    IDC_CONFIG_COLOR_R_LABEL = 8376,
    IDC_CONFIG_COLOR_G_LABEL = 8377,
    IDC_CONFIG_COLOR_B_LABEL = 8378,
    IDC_CONFIG_CHAT_TIMEOUT_LABEL = 8379,
    IDC_CONFIG_CHAT_WIDTH_LABEL = 8380,
    IDC_CONFIG_CHAT_HEIGHT_LABEL = 8381,
    IDC_CONFIG_CHAT_FONT_LABEL = 8382,
    IDC_CONFIG_CHAT_FONT_SIZE_LABEL = 8383,
    IDC_CONFIG_COLOR_THEME_LABEL = 8384,
    IDC_CONFIG_SUBTITLE_FIELD_LABEL = 8385,
    IDC_CONFIG_SCENE_CLOCK_LABEL = 8386,
    IDC_CONFIG_DOOR_INTERACTION_LABEL = 8387,
    IDC_CONFIG_GAME_MODE_LABEL = 8388,
    IDC_CONFIG_EDIT_MODE_LABEL = 8389,
    IDC_CONFIG_WINDOW_MODE_LABEL = 8394,
    IDC_CONFIG_RESOLUTION_LABEL = 8395,
    IDC_CONFIG_RENDERING_BACKEND_LABEL = 8396
    ,IDC_CONFIG_HD_TEXTURES = 8397
    ,IDC_CONFIG_HD_TEXTURE_FOLDER = 8398
    ,IDC_CONFIG_PBR = 8399
    ,IDC_CONFIG_PBR_TEXTURE_FOLDER = 8400
    ,IDC_CONFIG_RENDER_RESOLUTION = 8401
    ,IDC_CONFIG_RENDER_RESOLUTION_LABEL = 8402
    ,IDC_CONFIG_MAP_TEXTURE_COMPRESSION = 8403
    ,IDC_CONFIG_WEATHER_EFFECTS = 8404
    ,IDC_CONFIG_INVERT_BUMP_MAPPING = 8405
    ,IDC_CONFIG_ANTI_ALIASING = 8406
    ,IDC_CONFIG_ANTI_ALIASING_LABEL = 8407
    ,IDC_CONFIG_POST_PROCESS_AA = 8408
    ,IDC_CONFIG_POST_PROCESS_AA_LABEL = 8409
    ,IDC_CONFIG_SHADOW_PLAYER = 8410
    ,IDC_CONFIG_SHADOW_NPCS = 8411
    ,IDC_CONFIG_SHADOW_OBJECTS = 8412
    ,IDC_CONFIG_SHADOW_GROUND = 8413
    ,IDC_CONFIG_SHADOW_ALPHA = 8414
    ,IDC_CONFIG_SHADOW_DISTANCE = 8415
    ,IDC_CONFIG_SHADOW_NPC_LIMIT = 8416
    ,IDC_CONFIG_SHADOW_OBJECT_LIMIT = 8417
    ,IDC_CONFIG_SHADOW_MIN_SIZE = 8418
    ,IDC_CONFIG_SHADOW_MAX_SIZE = 8419
    ,IDC_CONFIG_SHADOW_UPDATE = 8420
    ,IDC_CONFIG_SHADOW_RECEIVER_QUALITY = 8421
    ,IDC_CONFIG_SHADOW_LENGTH = 8422
    ,IDC_CONFIG_SHADOW_OPACITY = 8423
    ,IDC_CONFIG_SHADOW_DEBUG = 8424
    ,IDC_CONFIG_SHADOW_COUNTERS = 8425
    ,IDC_CONFIG_SHADOW_DISTANCE_LABEL = 8426
    ,IDC_CONFIG_SHADOW_NPC_LIMIT_LABEL = 8427
    ,IDC_CONFIG_SHADOW_OBJECT_LIMIT_LABEL = 8428
    ,IDC_CONFIG_SHADOW_UPDATE_LABEL = 8429
    ,IDC_CONFIG_SHADOW_MIN_SIZE_LABEL = 8430
    ,IDC_CONFIG_SHADOW_MAX_SIZE_LABEL = 8431
    ,IDC_CONFIG_SHADOW_LENGTH_LABEL = 8432
    ,IDC_CONFIG_SHADOW_OPACITY_LABEL = 8433
    ,IDC_CONFIG_SHADOW_RECEIVER_LABEL = 8434
    ,IDC_CONFIG_CATEGORY_KEYS = 8435
    ,IDC_CONFIG_CATEGORY_DEBUG = 8436
    ,IDC_CONFIG_KEY_FORWARD = 8440
    ,IDC_CONFIG_KEY_BACKWARD = 8441
    ,IDC_CONFIG_KEY_LEFT = 8442
    ,IDC_CONFIG_KEY_RIGHT = 8443
    ,IDC_CONFIG_KEY_JUMP = 8444
    ,IDC_CONFIG_KEY_AUTORUN = 8445
    ,IDC_CONFIG_KEY_RUN_TOGGLE = 8446
    ,IDC_CONFIG_KEY_GAME_MODE = 8447
    ,IDC_CONFIG_KEY_ZONE_MAP = 8448
    ,IDC_CONFIG_KEY_UNSTICK = 8449
    ,IDC_CONFIG_KEY_WEATHER = 8450
    ,IDC_CONFIG_KEY_CAMERA_DEBUG = 8451
    ,IDC_CONFIG_KEY_BUMP_MAPPING = 8452
    ,IDC_CONFIG_KEY_MAIN_MENU = 8453
};

constexpr LONG_PTR kNoConfigTab = 0;
constexpr LONG_PTR kMenuTab = 1;
constexpr LONG_PTR kChatTab = 2;
constexpr LONG_PTR kNameplatesTab = 3;
constexpr int kGeneralPage = 1;
constexpr int kGraphicsPage = 2;
constexpr int kAudioPage = 3;
constexpr int kInterfacePage = 4;
constexpr int kKeysPage = 5;
constexpr int kDebugPage = 6;
constexpr int kBindingCount = 14;

const char* const kBindingLabels[kBindingCount] =
{
    "Move forward", "Move backward", "Move left", "Move right", "Jump",
    "Auto-run", "Run toggle", "Game / Edit mode", "Zone map",
    "Unstick player", "Cycle weather", "Camera debug", "Toggle bump mapping",
    "Open main menu"
};

int* BindingValue(ApplicationSettings::State& settings, const int index)
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

const char* KeyName(const int key, char (&text)[64])
{
    switch (key)
    {
    case VK_SPACE: return "Space";
    case VK_SHIFT: return "Shift";
    case VK_CONTROL: return "Ctrl";
    case VK_MENU: return "Alt";
    case VK_RETURN: return "Enter";
    case VK_ESCAPE: return "Escape";
    case VK_BACK: return "Backspace";
    case VK_SUBTRACT: return "Numpad -";
    }
    const UINT scan = MapVirtualKeyA(static_cast<UINT>(key), MAPVK_VK_TO_VSC);
    if (GetKeyNameTextA(static_cast<LONG>(scan << 16), text, 64) > 0)
        return text;
    std::snprintf(text, 64, "Key %d", key);
    return text;
}

LONG_PTR ControlPageTag(const int page, const LONG_PTR interfaceTab = 0)
{
    return static_cast<LONG_PTR>(page) | (interfaceTab << 8);
}

ConfigDialog::State* GetState(const HWND window)
{
    return reinterpret_cast<ConfigDialog::State*>(
        GetWindowLongPtrA(window, GWLP_USERDATA));
}

void Notify(ConfigDialog::State& state, const ConfigDialog::Command command, const int value = 0)
{
    const bool immediate = command == ConfigDialog::Command::SetPath ||
        command == ConfigDialog::Command::AutoDetectPath ||
        command == ConfigDialog::Command::ResetPath ||
        command == ConfigDialog::Command::ShowPath ||
        command == ConfigDialog::Command::ApplySettings;
    if (immediate && state.eventCallback)
        state.eventCallback(state.callbackContext, { command, value });
    ConfigDialog::Sync(state);
}

void BeginEditing(ConfigDialog::State& state)
{
    if (state.editing || !state.liveSettings)
        return;
    state.pendingSettings = *state.liveSettings;
    state.settings = &state.pendingSettings;
    if (state.liveGameUi)
    {
        state.pendingGameUi = *state.liveGameUi;
        state.gameUi = &state.pendingGameUi;
    }
    state.pendingDarkTheme = state.theme ? state.theme->dark : true;
    state.pendingGameMode = state.isGameModeCallback &&
        state.isGameModeCallback(state.callbackContext);
    state.editing = true;
}

void CommitPending(ConfigDialog::State& state)
{
    if (!state.editing || !state.liveSettings)
        return;
    *state.liveSettings = state.pendingSettings;
    if (state.liveGameUi && state.gameUi)
        *state.liveGameUi = state.pendingGameUi;
}

void EndEditing(ConfigDialog::State& state)
{
    state.settings = state.liveSettings;
    state.gameUi = state.liveGameUi;
    state.editing = false;
}

int ReadSignedInteger(const HWND window, const int controlId, const int fallback)
{
    char text[32] = {};
    GetDlgItemTextA(window, controlId, text, sizeof(text));
    char* end = nullptr;
    const long value = std::strtol(text, &end, 10);
    return end != text && *end == '\0' ? static_cast<int>(value) : fallback;
}

BOOL CALLBACK ShowConfigTabChild(HWND child, LPARAM activeTab)
{
    const LONG_PTR tag = GetWindowLongPtrA(child, GWLP_USERDATA);
    const int page = static_cast<int>(tag & 0xff);
    if (page < kGeneralPage || page > kDebugPage)
        return TRUE;
    const int activePage = static_cast<int>(activeTab & 0xff);
    const LONG_PTR subTab = tag >> 8;
    const LONG_PTR activeSubTab = activeTab >> 8;
    ShowWindow(child, page == activePage && (!subTab || subTab == activeSubTab) ? SW_SHOW : SW_HIDE);
    return TRUE;
}

void ApplyActiveTab(ConfigDialog::State& state)
{
    if (!state.window)
        return;
    const LONG_PTR activePage = static_cast<LONG_PTR>(std::clamp(state.categoryTab, 0, 5) + 1);
    const LONG_PTR activeSubTab = static_cast<LONG_PTR>(std::clamp(state.activeTab, 0, 2) + 1);
    EnumChildWindows(state.window, ShowConfigTabChild, activePage | (activeSubTab << 8));
    InvalidateRect(state.window, nullptr, TRUE);
}

int PageContentBottom(const int categoryTab)
{
    switch (categoryTab)
    {
    case 0: return 620;
    case 1: return 500;
    case 2: return 170;
    case 3: return 438;
    case 4: return 510;
    default: return 390;
    }
}

struct ScrollPageControlsContext
{
    int page;
    int delta;
};

BOOL CALLBACK ScrollPageControl(HWND child, LPARAM parameter)
{
    const auto* context = reinterpret_cast<const ScrollPageControlsContext*>(parameter);
    const LONG_PTR tag = GetWindowLongPtrA(child, GWLP_USERDATA);
    if (static_cast<int>(tag & 0xff) != context->page)
        return TRUE;
    RECT rect = {};
    GetWindowRect(child, &rect);
    MapWindowPoints(HWND_DESKTOP, GetParent(child), reinterpret_cast<POINT*>(&rect), 2);
    SetWindowPos(child, nullptr, rect.left, rect.top + context->delta,
        rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE);
    return TRUE;
}

void UpdatePageScroll(ConfigDialog::State& state, const int requestedOffset)
{
    if (!state.window)
        return;
    RECT client = {};
    GetClientRect(state.window, &client);
    constexpr int contentTop = 76;
    const int contentBottom = (std::max)(contentTop + 1, static_cast<int>(client.bottom) - 58);
    const int viewportHeight = contentBottom - contentTop;
    const int contentHeight = (std::max)(viewportHeight,
        PageContentBottom(state.categoryTab) - contentTop + 12);
    const int maximumOffset = (std::max)(0, contentHeight - viewportHeight);
    const int newOffset = std::clamp(requestedOffset, 0, maximumOffset);
    int& currentOffset = state.scrollOffsets[std::clamp(state.categoryTab, 0, 5)];
    if (newOffset != currentOffset)
    {
        const ScrollPageControlsContext context = {state.categoryTab + 1,
            currentOffset - newOffset};
        EnumChildWindows(state.window, ScrollPageControl,
            reinterpret_cast<LPARAM>(&context));
        currentOffset = newOffset;
    }
    SCROLLINFO scroll = {sizeof(scroll)};
    scroll.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    scroll.nMin = 0;
    scroll.nMax = contentHeight - 1;
    scroll.nPage = viewportHeight;
    scroll.nPos = currentOffset;
    SetScrollInfo(state.window, SB_VERT, &scroll, TRUE);
    InvalidateRect(state.window, nullptr, TRUE);
}

BOOL CALLBACK TagInterfaceTabChild(HWND child, LPARAM)
{
    const int id = GetDlgCtrlID(child);
    if (id == IDC_CONFIG_CLOSE || id == IDC_CONFIG_APPLY ||
        (id >= IDC_CONFIG_CATEGORY_GENERAL &&
        id <= IDC_CONFIG_CATEGORY_INTERFACE) || id == IDC_CONFIG_CATEGORY_KEYS ||
        id == IDC_CONFIG_CATEGORY_DEBUG)
        return TRUE;

    HWND parent = GetParent(child);
    RECT rect = {};
    GetWindowRect(child, &rect);
    MapWindowPoints(HWND_DESKTOP, parent, reinterpret_cast<POINT*>(&rect), 2);
    int page = kGeneralPage;
    int offsetY = 64;
    if (id == IDC_CONFIG_TITLE_BACKGROUND_MODE ||
        id == IDC_CONFIG_TITLE_BACKGROUND_ZONE ||
        id == IDC_CONFIG_TITLE_BACKGROUND_LABEL ||
        id == IDC_CONFIG_SHOW_TITLE_UI ||
        id == IDC_CONFIG_TITLE_MUSIC_MODE ||
        id == IDC_CONFIG_TITLE_MUSIC ||
        id == IDC_CONFIG_TITLE_MUSIC_LABEL)
    {
        page = kGeneralPage;
        offsetY = 96;
    }
    else if (rect.top >= 194 && rect.top < 342)
    {
        page = kGraphicsPage;
        offsetY = -118;
    }
    else if (rect.top >= 342 && rect.top < 438)
    {
        page = kAudioPage;
        offsetY = -264;
    }
    else if (rect.top >= 438 && rect.top < 534)
    {
        page = kGeneralPage;
        offsetY = -66;
    }
    else if (rect.top >= 534)
    {
        page = kInterfacePage;
        offsetY = -454;
    }
    else if (rect.top >= 100)
        offsetY = 138;

    const LONG_PTR tab = kNoConfigTab;
    SetWindowLongPtrA(child, GWLP_USERDATA, ControlPageTag(page, tab));
    SetWindowPos(child, nullptr, rect.left, rect.top + offsetY,
        rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE);
    return TRUE;
}

void CreateControls(ConfigDialog::State& state)
{
    const HWND window = state.window;
    const HFONT font = state.theme->resources.font;
    Win32PanelControls::AddPanelControl(window, "BUTTON", "General", BS_OWNERDRAW,
        IDC_CONFIG_CATEGORY_GENERAL, 18, 12, 112, 30);
    Win32PanelControls::AddPanelControl(window, "BUTTON", "Graphics", BS_OWNERDRAW,
        IDC_CONFIG_CATEGORY_GRAPHICS, 136, 12, 112, 30);
    Win32PanelControls::AddPanelControl(window, "BUTTON", "Audio", BS_OWNERDRAW,
        IDC_CONFIG_CATEGORY_AUDIO, 254, 12, 102, 30);
    Win32PanelControls::AddPanelControl(window, "BUTTON", "Interface", BS_OWNERDRAW,
        IDC_CONFIG_CATEGORY_INTERFACE, 362, 12, 112, 30);
    Win32PanelControls::AddPanelControl(window, "BUTTON", "Key Bindings", BS_OWNERDRAW,
        IDC_CONFIG_CATEGORY_KEYS, 480, 12, 134, 30);
    Win32PanelControls::AddPanelControl(window, "BUTTON", "Debug", BS_OWNERDRAW,
        IDC_CONFIG_CATEGORY_DEBUG, 620, 12, 112, 30);

    for (int index = 0; index < kBindingCount; ++index)
    {
        const int column = index / 7;
        const int row = index % 7;
        const int x = column ? 390 : 28;
        const int y = 112 + row * 50;
        Win32PanelControls::AddPanelControl(window, "STATIC", kBindingLabels[index], 0,
            -1, x, y, 180, 22);
        Win32PanelControls::AddPanelControl(window, "BUTTON", "", BS_OWNERDRAW | WS_TABSTOP,
            IDC_CONFIG_KEY_FORWARD + index, x + 184, y - 4, 142, 30);
    }
    const HWND path = Win32PanelControls::AddPanelControl(
        window, "EDIT", "", ES_AUTOHSCROLL | ES_READONLY, IDC_CONFIG_PATH_TEXT,
        28, 31, 704, 24, WS_EX_CLIENTEDGE);

    Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Set Path...", BS_OWNERDRAW, IDC_CONFIG_SET_PATH,
        28, 63, 110, 26);
    Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Auto-Detect", BS_OWNERDRAW, IDC_CONFIG_DETECT_PATH,
        146, 63, 110, 26);
    Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Reset Default", BS_OWNERDRAW, IDC_CONFIG_RESET_PATH,
        264, 63, 110, 26);
    Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Show Path", BS_OWNERDRAW, IDC_CONFIG_SHOW_PATH,
        382, 63, 110, 26);

    Win32PanelControls::AddPanelControl(window, "STATIC", "Mode", 0,
        IDC_CONFIG_TITLE_BACKGROUND_LABEL, 24, 100, 118, 22);
    const HWND titleBackgroundMode = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_TITLE_BACKGROUND_MODE, 142, 96, 150, 110);
    for (int mode = ApplicationSettings::TitleBackgroundRandom;
         mode <= ApplicationSettings::TitleBackgroundFixed; ++mode)
        SendMessageA(titleBackgroundMode, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(ApplicationSettings::TitleBackgroundModeName(mode)));
    const HWND titleBackgroundZone = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_TITLE_BACKGROUND_ZONE, 304, 96, 260, 360);
    for (int i = 0; i < ApplicationSettings::TitleBackgroundOptionCount(); ++i)
        SendMessageA(titleBackgroundZone, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(
            ApplicationSettings::TitleBackgroundOptionAt(i).name));
    Win32PanelControls::AddPanelControl(window, "BUTTON", "Hide Title UI", BS_AUTOCHECKBOX,
        IDC_CONFIG_SHOW_TITLE_UI, 574, 96, 158, 22);
    Win32PanelControls::AddPanelControl(window, "STATIC", "Title Music", 0,
        IDC_CONFIG_TITLE_MUSIC_LABEL, 24, 126, 118, 22);
    const HWND titleMusicMode = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_TITLE_MUSIC_MODE, 142, 122, 150, 140);
    for (int mode = ApplicationSettings::TitleMusicRandom;
         mode <= ApplicationSettings::TitleMusicFixed; ++mode)
        SendMessageA(titleMusicMode, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(ApplicationSettings::TitleMusicModeName(mode)));
    const HWND titleMusic = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_TITLE_MUSIC, 304, 122, 260, 180);
    for (int i = 0; i < ApplicationSettings::TitleMusicOptionCount(); ++i)
        SendMessageA(titleMusic, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(
            ApplicationSettings::TitleMusicOptionAt(i).name));

    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Window Mode", 0, IDC_CONFIG_WINDOW_MODE_LABEL, 24, 132, 100, 18);
    const HWND windowMode = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_WINDOW_MODE, 142, 128, 170, 120);
    SendMessageA(windowMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Windowed"));
    SendMessageA(windowMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Borderless Window"));
    SendMessageA(windowMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Full Screen"));

    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Resolution", 0, IDC_CONFIG_RESOLUTION_LABEL, 24, 162, 100, 18);
    const HWND resolution = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_RESOLUTION, 142, 158, 170, 180);
    for (int i = 0; i < ApplicationSettings::ResolutionOptionCount(); ++i)
    {
        SendMessageA(resolution, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(ApplicationSettings::ResolutionOptionAt(i).label));
    }
    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Drawing Precision", 0,
        IDC_CONFIG_RENDER_RESOLUTION_LABEL, 340, 162, 124, 18);
    const HWND renderingResolution = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_RENDER_RESOLUTION, 450, 158, 170, 180);
    for (int i = 0; i < ApplicationSettings::RenderingResolutionScaleOptionCount(); ++i)
        SendMessageA(renderingResolution, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(
            ApplicationSettings::RenderingResolutionScaleLabel(i)));
    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Multisample Anti-Aliasing (MSAA)", 0,
        IDC_CONFIG_ANTI_ALIASING_LABEL, 340, 190, 260, 18);
    const HWND antiAliasing = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_ANTI_ALIASING, 450, 186, 170, 150);
    for (int i = 0; i < ApplicationSettings::AntiAliasingOptionCount(); ++i)
        SendMessageA(antiAliasing, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(
            ApplicationSettings::AntiAliasingLabel(i)));
    Win32PanelControls::AddPanelControl(window, "STATIC",
        "Subpixel Morphological Anti-Aliasing (SMAA)", 0,
        IDC_CONFIG_POST_PROCESS_AA_LABEL, 340, 180, 330, 18);
    const HWND postProcessAa = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_POST_PROCESS_AA, 450, 176, 170, 120);
    for (int i = 0; i < ApplicationSettings::PostProcessAntiAliasingOptionCount(); ++i)
        SendMessageA(postProcessAa, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(
            ApplicationSettings::PostProcessAntiAliasingLabel(i)));

    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Rendering Backend", 0, IDC_CONFIG_RENDERING_BACKEND_LABEL, 428, 132, 140, 22);
    const HWND renderingBackend = Win32PanelControls::AddPanelControl(
        window, "COMBOBOX", "", CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED |
            CBS_HASSTRINGS | WS_VSCROLL, IDC_CONFIG_RENDERING_BACKEND,
        574, 128, 158, 180);
    for (int i = 0; i < ApplicationSettings::RenderingBackendOptionCount(); ++i)
    {
        SendMessageA(renderingBackend, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(ApplicationSettings::RenderingBackendName(i)));
    }

    Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Enable MIP Mapping", BS_AUTOCHECKBOX,
        IDC_CONFIG_MIP_MAPPING, 24, 224, 220, 22);
    Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Enable Bump Mapping", BS_AUTOCHECKBOX,
        IDC_CONFIG_BUMP_MAPPING, 24, 248, 220, 22);
    Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Invert Bump Mapping (O)", BS_AUTOCHECKBOX,
        IDC_CONFIG_INVERT_BUMP_MAPPING, 24, 270, 220, 22);
    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Bump Intensity", 0,
        IDC_CONFIG_BUMP_INTENSITY_LABEL, 250, 248, 108, 22);
    const HWND bumpIntensity = Win32PanelControls::AddPanelControl(
        window, "msctls_trackbar32", "", TBS_AUTOTICKS | TBS_HORZ | WS_TABSTOP,
        IDC_CONFIG_BUMP_INTENSITY, 360, 244, 260, 28);
    SendMessageA(bumpIntensity, TBM_SETRANGE, TRUE, MAKELPARAM(0, 300));
    SendMessageA(bumpIntensity, TBM_SETTICFREQ, 50, 0);
    SendMessageA(bumpIntensity, TBM_SETPAGESIZE, 0, 25);
    Win32PanelControls::AddPanelControl(
        window, "STATIC", "100%", SS_RIGHT,
        IDC_CONFIG_BUMP_INTENSITY_VALUE, 630, 248, 54, 22);
    Win32PanelControls::AddPanelControl(window, "BUTTON", "Enable HD Textures", BS_AUTOCHECKBOX,
        IDC_CONFIG_HD_TEXTURES, 24, 278, 190, 22);
    Win32PanelControls::AddPanelControl(window, "EDIT", "", ES_AUTOHSCROLL | ES_READONLY,
        IDC_CONFIG_HD_TEXTURE_FOLDER, 220, 274, 484, 24, WS_EX_CLIENTEDGE);
    Win32PanelControls::AddPanelControl(window, "BUTTON", "Enable Physically Based Rendering (PBR)",
        BS_AUTOCHECKBOX, IDC_CONFIG_PBR, 24, 308, 290, 22);
    Win32PanelControls::AddPanelControl(window, "EDIT", "", ES_AUTOHSCROLL | ES_READONLY,
        IDC_CONFIG_PBR_TEXTURE_FOLDER, 320, 304, 384, 24, WS_EX_CLIENTEDGE);
    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Vegetation Animation", 0, IDC_CONFIG_ENV_ANIM_LABEL, 24, 278, 146, 22);
    const HWND environmentAnimation = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_ENV_ANIM, 188, 274, 178, 120);
    SendMessageA(environmentAnimation, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Off"));
    SendMessageA(environmentAnimation, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Simple"));
    SendMessageA(environmentAnimation, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Smooth"));
    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Lighting", 0, IDC_CONFIG_LIGHTING_LABEL, 24, 312, 72, 22);
    const HWND lightingQuality = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_LIGHTING_QUALITY, 96, 308, 140, 100);
    SendMessageA(lightingQuality, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Off"));
    SendMessageA(lightingQuality, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Simplified"));
    SendMessageA(lightingQuality, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Dynamic Shadows"));
    const HWND lightMode = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_LIGHT_DIRECTION_MODE, 244, 308, 112, 90);
    SendMessageA(lightMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Automatic"));
    SendMessageA(lightMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Manual"));
    Win32PanelControls::AddPanelControl(window, "STATIC", "Azimuth", 0, IDC_CONFIG_AZIMUTH_LABEL,
        370, 312, 58, 22);
    const HWND lightAzimuth = Win32PanelControls::AddPanelControl(window, "EDIT", "0",
        ES_AUTOHSCROLL | WS_TABSTOP, IDC_CONFIG_LIGHT_AZIMUTH, 430, 308, 54, 24);
    Win32PanelControls::AddPanelControl(window, "STATIC", "Elevation", 0, IDC_CONFIG_ELEVATION_LABEL,
        500, 312, 64, 22);
    const HWND lightElevation = Win32PanelControls::AddPanelControl(window, "EDIT", "0",
        ES_AUTOHSCROLL | WS_TABSTOP, IDC_CONFIG_LIGHT_ELEVATION, 566, 308, 54, 24);
    SendMessageA(lightAzimuth, EM_SETLIMITTEXT, 4, 0);
    SendMessageA(lightElevation, EM_SETLIMITTEXT, 3, 0);
    Win32PanelControls::AddPanelControl(window, "BUTTON", "Player shadows", BS_AUTOCHECKBOX,
        IDC_CONFIG_SHADOW_PLAYER, 24, 278, 160, 22);
    Win32PanelControls::AddPanelControl(window, "BUTTON", "NPC shadows", BS_AUTOCHECKBOX,
        IDC_CONFIG_SHADOW_NPCS, 190, 278, 150, 22);
    Win32PanelControls::AddPanelControl(window, "BUTTON", "Object shadows", BS_AUTOCHECKBOX,
        IDC_CONFIG_SHADOW_OBJECTS, 350, 278, 160, 22);
    Win32PanelControls::AddPanelControl(window, "BUTTON", "Terrain-like casters", BS_AUTOCHECKBOX,
        IDC_CONFIG_SHADOW_GROUND, 24, 302, 180, 22);
    Win32PanelControls::AddPanelControl(window, "BUTTON", "Alpha-tested casters", BS_AUTOCHECKBOX,
        IDC_CONFIG_SHADOW_ALPHA, 210, 302, 190, 22);
    Win32PanelControls::AddPanelControl(window, "BUTTON", "Debug colors", BS_AUTOCHECKBOX,
        IDC_CONFIG_SHADOW_DEBUG, 410, 302, 140, 22);
    Win32PanelControls::AddPanelControl(window, "BUTTON", "Performance counters", BS_AUTOCHECKBOX,
        IDC_CONFIG_SHADOW_COUNTERS, 556, 302, 176, 22);
    const struct { const char* label; int id; int labelId; int x; } shadowEdits[] =
    {
        { "Distance", IDC_CONFIG_SHADOW_DISTANCE, IDC_CONFIG_SHADOW_DISTANCE_LABEL, 24 },
        { "NPC limit", IDC_CONFIG_SHADOW_NPC_LIMIT, IDC_CONFIG_SHADOW_NPC_LIMIT_LABEL, 200 },
        { "Object limit", IDC_CONFIG_SHADOW_OBJECT_LIMIT, IDC_CONFIG_SHADOW_OBJECT_LIMIT_LABEL, 376 },
        { "Update frames", IDC_CONFIG_SHADOW_UPDATE, IDC_CONFIG_SHADOW_UPDATE_LABEL, 552 },
        { "Min size x100", IDC_CONFIG_SHADOW_MIN_SIZE, IDC_CONFIG_SHADOW_MIN_SIZE_LABEL, 24 },
        { "Max size", IDC_CONFIG_SHADOW_MAX_SIZE, IDC_CONFIG_SHADOW_MAX_SIZE_LABEL, 200 },
        { "Max length", IDC_CONFIG_SHADOW_LENGTH, IDC_CONFIG_SHADOW_LENGTH_LABEL, 376 },
        { "Opacity %", IDC_CONFIG_SHADOW_OPACITY, IDC_CONFIG_SHADOW_OPACITY_LABEL, 552 }
    };
    for (int i = 0; i < 8; ++i)
    {
        const int y = 326;
        Win32PanelControls::AddPanelControl(window, "STATIC", shadowEdits[i].label, 0,
            shadowEdits[i].labelId, shadowEdits[i].x, y, 104, 22);
        const HWND edit = Win32PanelControls::AddPanelControl(window, "EDIT", "0",
            ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP, shadowEdits[i].id,
            shadowEdits[i].x + 108, y - 4, 54, 24);
        SendMessageA(edit, EM_SETLIMITTEXT, 4, 0);
    }
    Win32PanelControls::AddPanelControl(window, "STATIC", "Receiver quality", 0,
        IDC_CONFIG_SHADOW_RECEIVER_LABEL, 24, 326, 120, 22);
    const HWND receiverQuality = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_SHADOW_RECEIVER_QUALITY, 150, 322, 160, 100);
    SendMessageA(receiverQuality, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Fast"));
    SendMessageA(receiverQuality, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Balanced"));
    SendMessageA(receiverQuality, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Precise"));
    Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Mirror world zones", BS_AUTOCHECKBOX,
        IDC_CONFIG_MIRROR_WORLD, 428, 224, 220, 22);
    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Texture Storage", 0, IDC_CONFIG_TEXTURE_STORAGE_LABEL, 428, 256, 120, 22);
    const HWND textureCompression = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_TEXTURE_COMPRESSION, 574, 252, 158, 100);
    SendMessageA(textureCompression, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Compressed"));
    SendMessageA(textureCompression, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Uncompressed"));
    Win32PanelControls::AddPanelControl(window, "BUTTON", "Compress on-screen map textures",
        BS_AUTOCHECKBOX, IDC_CONFIG_MAP_TEXTURE_COMPRESSION, 390, 256, 300, 22);
    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Draw Distance", 0, IDC_CONFIG_DRAW_DISTANCE_LABEL, 428, 290, 120, 22);
    const HWND drawDistance = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_DRAW_DISTANCE, 574, 286, 158, 160);
    for (int i = 0; i < ApplicationSettings::DrawDistanceOptionCount(); ++i)
    {
        SendMessageA(drawDistance, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(ApplicationSettings::DrawDistanceOptionLabel(i)));
    }
    Win32PanelControls::AddPanelControl(window, "BUTTON", "Enable weather effects",
        BS_AUTOCHECKBOX, IDC_CONFIG_WEATHER_EFFECTS, 428, 320, 220, 22);

    Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Enable Sounds", BS_AUTOCHECKBOX,
        IDC_CONFIG_ENABLE_SOUNDS, 24, 372, 150, 22);
    Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Play sounds in background", BS_AUTOCHECKBOX,
        IDC_CONFIG_BACKGROUND_SOUNDS, 24, 396, 210, 22);
    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Simultaneous SFX", 0, 0, 428, 372, 120, 18);
    const HWND maxSounds = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_MAX_SOUNDS, 574, 368, 158, 160);
    for (int i = 0; i < ApplicationSettings::MaxSoundOptionCount(); ++i)
    {
        SendMessageA(maxSounds, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(ApplicationSettings::MaxSoundOptionLabel(i)));
    }
    Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Enable Hardware Mouse Cursor", BS_AUTOCHECKBOX,
        IDC_CONFIG_HARDWARE_CURSOR, 428, 396, 264, 22);

    Win32PanelControls::AddPanelControl(window, "STATIC", "Game Mode", 0,
        IDC_CONFIG_GAME_MODE_LABEL, 24, 468, 88, 22);
    Win32PanelControls::AddPanelControl(window, "BUTTON", "", BS_OWNERDRAW,
        IDC_CONFIG_EDIT_GAME, 116, 464, 56, 26);
    Win32PanelControls::AddPanelControl(window, "STATIC", "Edit Mode", 0,
        IDC_CONFIG_EDIT_MODE_LABEL, 180, 468, 84, 22);
    Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Show collision mesh", BS_AUTOCHECKBOX,
        IDC_CONFIG_COLLISION, 24, 492, 190, 22);
    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Control Style", 0, IDC_CONFIG_MOVEMENT_STYLE_LABEL,
        428, 450, 140, 22);
    const HWND movementStyle = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_MOVEMENT_STYLE, 574, 446, 158, 100);
    for (int i = 0; i < 3; ++i)
        SendMessageA(movementStyle, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(ApplicationSettings::MovementStyleName(i)));
    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Scene Clock", 0, IDC_CONFIG_SCENE_CLOCK_LABEL,
        428, 469, 120, 18);
    const HWND sceneClock = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_SCENE_CLOCK, 574, 465, 158, 100);
    SendMessageA(sceneClock, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Vana'diel (Retail)"));
    SendMessageA(sceneClock, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Local System Time"));
    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Door Interaction", 0, IDC_CONFIG_DOOR_INTERACTION_LABEL,
        428, 499, 120, 18);
    const HWND doorInteraction = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_DOOR_INTERACTION, 574, 495, 158, 100);
    SendMessageA(doorInteraction, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Classic"));
    SendMessageA(doorInteraction, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Physics"));

    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Color Theme", 0, IDC_CONFIG_COLOR_THEME_LABEL, 28, 606, 100, 22);
    const HWND colorTheme = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_COLOR_THEME, 142, 601, 180, 96);
    SendMessageA(colorTheme, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Dark"));
    SendMessageA(colorTheme, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Light"));
    Win32PanelControls::AddPanelControl(window, "STATIC", "Mouse Cursor", 0,
        IDC_CONFIG_CURSOR_STYLE_LABEL,
        370, 606, 110, 22);
    const HWND cursorStyle = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_CURSOR_STYLE, 486, 601, 246, 140);
    for (const char* label : {"Windows", "FFXI animated", "FFXI static", "FFXI interaction"})
        SendMessageA(cursorStyle, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));

    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Player icon", 0, IDC_CONFIG_PLAYER_ICON_LABEL, 28, 606, 100, 22);
    const HWND playerIcon = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_PLAYER_ICON, 142, 601, 260, 280);
    SendMessageA(playerIcon, CB_SETDROPPEDWIDTH, 450, 0);
    SendMessageA(playerIcon, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("None"));
    for (const auto& icon : FFXIPlayerIcons::Catalog)
        SendMessageA(playerIcon, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(icon.label));
    SendMessageA(playerIcon, CB_SETMINVISIBLE, 12, 0);
    Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Show Job Master stars", BS_AUTOCHECKBOX,
        IDC_CONFIG_JOB_MASTER, 428, 601, 264, 22);

    Win32PanelControls::AddPanelControl(window, "STATIC", "Subtitle", 0,
        IDC_CONFIG_SUBTITLE_FIELD_LABEL, 28, 640, 100, 22);
    const HWND subtitle = Win32PanelControls::AddPanelCombo(window, IDC_CONFIG_SUBTITLE, 142, 635, 260, 120);
    for (const char* label : {"None", "Linkshell name", "Jobs + levels"})
        SendMessageA(subtitle, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
    Win32PanelControls::AddPanelControl(window, "STATIC", "Linkshell", 0,
        IDC_CONFIG_SUBTITLE_LABEL, 28, 674, 110, 22);
    const HWND linkshell = Win32PanelControls::AddPanelControl(window, "EDIT", "",
        ES_AUTOHSCROLL | WS_TABSTOP, IDC_CONFIG_LINKSHELL_NAME, 142, 669, 260, 24);
    SendMessageA(linkshell, EM_SETLIMITTEXT, 127, 0);
    const HWND mainJob = Win32PanelControls::AddPanelCombo(window, IDC_CONFIG_MAIN_JOB, 142, 669, 88, 240);
    const HWND subJob = Win32PanelControls::AddPanelCombo(window, IDC_CONFIG_SUB_JOB, 342, 669, 88, 240);
    for (int job = 0; job < FFXIStats::kJob_Count; ++job)
    {
        const LPARAM label = reinterpret_cast<LPARAM>(FFXIStats::JobName(static_cast<FFXIStats::Job>(job)));
        if (job) SendMessageA(mainJob, CB_ADDSTRING, 0, label);
        SendMessageA(subJob, CB_ADDSTRING, 0, label);
    }
    Win32PanelControls::AddPanelControl(window, "STATIC", "Lv.", 0, IDC_CONFIG_MAIN_LV_LABEL, 238, 674, 25, 22);
    Win32PanelControls::AddPanelControl(window, "STATIC", "Lv.", 0, IDC_CONFIG_SUB_LV_LABEL, 438, 674, 25, 22);
    Win32PanelControls::AddPanelControl(window, "STATIC", "/", 0, IDC_CONFIG_JOB_SEPARATOR, 322, 674, 20, 22);
    for (const auto& pair : {std::pair{IDC_CONFIG_MAIN_LEVEL, 264}, std::pair{IDC_CONFIG_SUB_LEVEL, 464}})
    {
        const HWND level = Win32PanelControls::AddPanelControl(window, "EDIT", "1",
            ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP, pair.first, pair.second, 669, 50, 24);
        SendMessageA(level, EM_SETLIMITTEXT, 2, 0);
    }

    Win32PanelControls::AddPanelControl(window, "STATIC", "Linkshell icon color", 0,
        IDC_CONFIG_LINKSHELL_COLOR_LABEL,
        560, 640, 160, 22);
    Win32PanelControls::AddPanelControl(window, "STATIC", "R", 0, IDC_CONFIG_COLOR_R_LABEL, 560, 674, 12, 22);
    Win32PanelControls::AddPanelControl(window, "STATIC", "G", 0, IDC_CONFIG_COLOR_G_LABEL, 616, 674, 12, 22);
    Win32PanelControls::AddPanelControl(window, "STATIC", "B", 0, IDC_CONFIG_COLOR_B_LABEL, 672, 674, 12, 22);
    const int colorY = 669;
    for (const auto& channel : {std::pair{IDC_CONFIG_LINKSHELL_COLOR_R, 576},
                               std::pair{IDC_CONFIG_LINKSHELL_COLOR_G, 632},
                               std::pair{IDC_CONFIG_LINKSHELL_COLOR_B, 688}})
    {
        const HWND color = Win32PanelControls::AddPanelControl(window, "EDIT", "0",
            ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP, channel.first, channel.second, colorY, 40, 24);
        SendMessageA(color, EM_SETLIMITTEXT, 3, 0);
    }

    Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Apply", BS_OWNERDRAW,
        IDC_CONFIG_APPLY, 500, 646, 112, 30);
    Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Okay", BS_OWNERDRAW,
        IDC_CONFIG_CLOSE, 620, 646, 112, 30);

    Win32PanelControls::AddPanelControl(window, "STATIC", "Display duration", 0,
        IDC_CONFIG_CHAT_TIMEOUT_LABEL, 28, 606, 120, 22);
    const HWND timeout = Win32PanelControls::AddPanelControl(window, "EDIT", "15",
        ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP, IDC_CONFIG_CHAT_TIMEOUT, 158, 601, 56, 24);
    SendMessageA(timeout, EM_SETLIMITTEXT, 3, 0);

    Win32PanelControls::AddPanelControl(window, "STATIC", "Width (%)", 0, IDC_CONFIG_CHAT_WIDTH_LABEL, 250, 606, 68, 22);
    Win32PanelControls::AddPanelControl(window, "EDIT", "50", ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP, IDC_CONFIG_CHAT_WIDTH, 328, 601, 50, 24);
    Win32PanelControls::AddPanelControl(window, "STATIC", "Height (%)", 0, IDC_CONFIG_CHAT_HEIGHT_LABEL, 414, 606, 70, 22);
    Win32PanelControls::AddPanelControl(window, "EDIT", "25", ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP, IDC_CONFIG_CHAT_HEIGHT, 494, 601, 50, 24);
    Win32PanelControls::AddPanelControl(window, "STATIC", "Font", 0, IDC_CONFIG_CHAT_FONT_LABEL, 28, 640, 120, 22);
    const HWND chatFont = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_CHAT_FONT, 158, 635, 220, 180);
    for (const char* name : {"FFXI (Official)", "Arial", "Segoe UI", "Tahoma", "Verdana", "Consolas"})
        SendMessageA(chatFont, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name));
    Win32PanelControls::AddPanelControl(window, "STATIC", "Font size", 0, IDC_CONFIG_CHAT_FONT_SIZE_LABEL, 414, 640, 70, 22);
    const HWND chatFontSize = Win32PanelControls::AddPanelControl(window, "EDIT", "17",
        ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP, IDC_CONFIG_CHAT_FONT_SIZE, 494, 635, 50, 24);
    SendMessageA(chatFontSize, EM_SETLIMITTEXT, 2, 0);

    const HWND children[] =
    {
        path,
        timeout,
        GetDlgItem(window, IDC_CONFIG_CHAT_WIDTH),
        GetDlgItem(window, IDC_CONFIG_CHAT_HEIGHT),
        chatFont,
        chatFontSize,
        GetDlgItem(window, IDC_CONFIG_SET_PATH),
        GetDlgItem(window, IDC_CONFIG_DETECT_PATH),
        GetDlgItem(window, IDC_CONFIG_RESET_PATH),
        GetDlgItem(window, IDC_CONFIG_SHOW_PATH),
        GetDlgItem(window, IDC_CONFIG_TAB_MENU),
        GetDlgItem(window, IDC_CONFIG_TAB_CHAT),
        GetDlgItem(window, IDC_CONFIG_TAB_NAMEPLATES),
        GetDlgItem(window, IDC_CONFIG_CATEGORY_GENERAL),
        GetDlgItem(window, IDC_CONFIG_CATEGORY_GRAPHICS),
        GetDlgItem(window, IDC_CONFIG_CATEGORY_AUDIO),
        GetDlgItem(window, IDC_CONFIG_CATEGORY_INTERFACE),
        GetDlgItem(window, IDC_CONFIG_CATEGORY_KEYS),
        GetDlgItem(window, IDC_CONFIG_CATEGORY_DEBUG),
        GetDlgItem(window, IDC_CONFIG_WINDOW_MODE),
        GetDlgItem(window, IDC_CONFIG_RESOLUTION),
        GetDlgItem(window, IDC_CONFIG_RENDER_RESOLUTION),
        GetDlgItem(window, IDC_CONFIG_RENDER_RESOLUTION_LABEL),
        GetDlgItem(window, IDC_CONFIG_ANTI_ALIASING),
        GetDlgItem(window, IDC_CONFIG_ANTI_ALIASING_LABEL),
        GetDlgItem(window, IDC_CONFIG_POST_PROCESS_AA),
        GetDlgItem(window, IDC_CONFIG_POST_PROCESS_AA_LABEL),
        GetDlgItem(window, IDC_CONFIG_RENDERING_BACKEND),
        GetDlgItem(window, IDC_CONFIG_MIP_MAPPING),
        GetDlgItem(window, IDC_CONFIG_BUMP_MAPPING),
        GetDlgItem(window, IDC_CONFIG_INVERT_BUMP_MAPPING),
        GetDlgItem(window, IDC_CONFIG_BUMP_INTENSITY),
        GetDlgItem(window, IDC_CONFIG_BUMP_INTENSITY_LABEL),
        GetDlgItem(window, IDC_CONFIG_BUMP_INTENSITY_VALUE),
        GetDlgItem(window, IDC_CONFIG_HD_TEXTURES),
        GetDlgItem(window, IDC_CONFIG_HD_TEXTURE_FOLDER),
        GetDlgItem(window, IDC_CONFIG_PBR),
        GetDlgItem(window, IDC_CONFIG_PBR_TEXTURE_FOLDER),
        GetDlgItem(window, IDC_CONFIG_LIGHTING_QUALITY),
        GetDlgItem(window, IDC_CONFIG_LIGHT_DIRECTION_MODE),
        GetDlgItem(window, IDC_CONFIG_LIGHT_AZIMUTH),
        GetDlgItem(window, IDC_CONFIG_LIGHT_ELEVATION),
        GetDlgItem(window, IDC_CONFIG_SCENE_CLOCK),
        GetDlgItem(window, IDC_CONFIG_DOOR_INTERACTION),
        GetDlgItem(window, IDC_CONFIG_ENV_ANIM),
        GetDlgItem(window, IDC_CONFIG_MIRROR_WORLD),
        GetDlgItem(window, IDC_CONFIG_TEXTURE_COMPRESSION),
        GetDlgItem(window, IDC_CONFIG_MAP_TEXTURE_COMPRESSION),
        GetDlgItem(window, IDC_CONFIG_WEATHER_EFFECTS),
        GetDlgItem(window, IDC_CONFIG_DRAW_DISTANCE),
        GetDlgItem(window, IDC_CONFIG_ENABLE_SOUNDS),
        GetDlgItem(window, IDC_CONFIG_BACKGROUND_SOUNDS),
        GetDlgItem(window, IDC_CONFIG_MAX_SOUNDS),
        GetDlgItem(window, IDC_CONFIG_HARDWARE_CURSOR),
        GetDlgItem(window, IDC_CONFIG_EDIT_GAME),
        GetDlgItem(window, IDC_CONFIG_COLLISION),
        GetDlgItem(window, IDC_CONFIG_COLOR_THEME),
        GetDlgItem(window, IDC_CONFIG_CURSOR_STYLE),
        GetDlgItem(window, IDC_CONFIG_PLAYER_ICON),
        GetDlgItem(window, IDC_CONFIG_JOB_MASTER),
        GetDlgItem(window, IDC_CONFIG_SUBTITLE),
        GetDlgItem(window, IDC_CONFIG_LINKSHELL_NAME),
        GetDlgItem(window, IDC_CONFIG_MAIN_JOB),
        GetDlgItem(window, IDC_CONFIG_SUB_JOB),
        GetDlgItem(window, IDC_CONFIG_MAIN_LEVEL),
        GetDlgItem(window, IDC_CONFIG_SUB_LEVEL),
        GetDlgItem(window, IDC_CONFIG_LINKSHELL_COLOR_R),
        GetDlgItem(window, IDC_CONFIG_LINKSHELL_COLOR_G),
        GetDlgItem(window, IDC_CONFIG_LINKSHELL_COLOR_B),
        GetDlgItem(window, IDC_CONFIG_CLOSE)
    };
    for (const HWND child : children)
    {
        if (child)
            SendMessageA(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
    EnumChildWindows(window, TagInterfaceTabChild, 0);
    const int debugShadowControls[] =
    {
        IDC_CONFIG_SHADOW_GROUND, IDC_CONFIG_SHADOW_ALPHA,
        IDC_CONFIG_SHADOW_DISTANCE, IDC_CONFIG_SHADOW_NPC_LIMIT,
        IDC_CONFIG_SHADOW_OBJECT_LIMIT, IDC_CONFIG_SHADOW_MIN_SIZE,
        IDC_CONFIG_SHADOW_MAX_SIZE, IDC_CONFIG_SHADOW_UPDATE,
        IDC_CONFIG_SHADOW_RECEIVER_QUALITY, IDC_CONFIG_SHADOW_LENGTH,
        IDC_CONFIG_SHADOW_OPACITY, IDC_CONFIG_SHADOW_DEBUG,
        IDC_CONFIG_SHADOW_COUNTERS, IDC_CONFIG_SHADOW_DISTANCE_LABEL,
        IDC_CONFIG_SHADOW_NPC_LIMIT_LABEL, IDC_CONFIG_SHADOW_OBJECT_LIMIT_LABEL,
        IDC_CONFIG_SHADOW_UPDATE_LABEL, IDC_CONFIG_SHADOW_MIN_SIZE_LABEL,
        IDC_CONFIG_SHADOW_MAX_SIZE_LABEL, IDC_CONFIG_SHADOW_LENGTH_LABEL,
        IDC_CONFIG_SHADOW_OPACITY_LABEL, IDC_CONFIG_SHADOW_RECEIVER_LABEL
    };
    for (const int id : debugShadowControls)
        if (const HWND control = GetDlgItem(window, id))
            SetWindowLongPtrA(control, GWLP_USERDATA, ControlPageTag(kDebugPage));
    for (int index = 0; index < kBindingCount; ++index)
    {
        const HWND button = GetDlgItem(window, IDC_CONFIG_KEY_FORWARD + index);
        const int column = index / 7;
        const int row = index % 7;
        const int x = column ? 390 : 28;
        const int y = 112 + row * 50;
        SetWindowLongPtrA(button, GWLP_USERDATA, ControlPageTag(kKeysPage));
        HWND label = GetWindow(button, GW_HWNDPREV);
        if (label)
        {
            SetWindowLongPtrA(label, GWLP_USERDATA, ControlPageTag(kKeysPage));
            SetWindowPos(label, nullptr, x, y, 180, 22, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        SetWindowPos(button, nullptr, x + 184, y - 4, 142, 30,
            SWP_NOZORDER | SWP_NOACTIVATE);
    }

    const auto place = [window](const int id, const int x, const int y, const int width, const int height)
    {
        if (const HWND control = GetDlgItem(window, id))
            SetWindowPos(control, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    };

    // Graphics: reserve a complete row for bump controls and separate environment from lighting.
    place(IDC_CONFIG_MIP_MAPPING, 24, 106, 220, 22);
    place(IDC_CONFIG_MIRROR_WORLD, 390, 106, 220, 22);
    place(IDC_CONFIG_BUMP_MAPPING, 24, 136, 190, 22);
    place(IDC_CONFIG_BUMP_INTENSITY_LABEL, 220, 136, 108, 22);
    place(IDC_CONFIG_BUMP_INTENSITY, 330, 132, 300, 28);
    place(IDC_CONFIG_BUMP_INTENSITY_VALUE, 650, 136, 54, 22);
    place(IDC_CONFIG_INVERT_BUMP_MAPPING, 24, 166, 220, 22);
    place(IDC_CONFIG_HD_TEXTURES, 24, 196, 190, 22);
    place(IDC_CONFIG_HD_TEXTURE_FOLDER, 220, 192, 484, 24);
    place(IDC_CONFIG_PBR, 24, 226, 290, 22);
    place(IDC_CONFIG_PBR_TEXTURE_FOLDER, 320, 222, 384, 24);
    place(IDC_CONFIG_TEXTURE_STORAGE_LABEL, 24, 262, 138, 22);
    place(IDC_CONFIG_TEXTURE_COMPRESSION, 170, 258, 194, 100);
    place(IDC_CONFIG_MAP_TEXTURE_COMPRESSION, 390, 262, 300, 22);

    place(IDC_CONFIG_ENV_ANIM_LABEL, 24, 340, 150, 22);
    place(IDC_CONFIG_ENV_ANIM, 188, 336, 210, 120);
    place(IDC_CONFIG_DRAW_DISTANCE_LABEL, 428, 340, 120, 22);
    place(IDC_CONFIG_DRAW_DISTANCE, 574, 336, 158, 160);
    place(IDC_CONFIG_WEATHER_EFFECTS, 428, 368, 220, 22);

    place(IDC_CONFIG_LIGHTING_LABEL, 24, 438, 72, 22);
    place(IDC_CONFIG_LIGHTING_QUALITY, 96, 434, 180, 100);
    place(IDC_CONFIG_LIGHT_DIRECTION_MODE, 286, 434, 132, 90);
    place(IDC_CONFIG_AZIMUTH_LABEL, 438, 438, 58, 22);
    place(IDC_CONFIG_LIGHT_AZIMUTH, 496, 434, 54, 24);
    place(IDC_CONFIG_ELEVATION_LABEL, 566, 438, 64, 22);
    place(IDC_CONFIG_LIGHT_ELEVATION, 636, 434, 54, 24);

    place(IDC_CONFIG_SHADOW_PLAYER, 24, 468, 160, 22);
    place(IDC_CONFIG_SHADOW_NPCS, 190, 468, 150, 22);
    place(IDC_CONFIG_SHADOW_OBJECTS, 350, 468, 160, 22);

    place(IDC_CONFIG_SHADOW_GROUND, 28, 112, 190, 22);
    place(IDC_CONFIG_SHADOW_ALPHA, 234, 112, 200, 22);
    place(IDC_CONFIG_SHADOW_DEBUG, 450, 112, 126, 22);
    place(IDC_CONFIG_SHADOW_COUNTERS, 584, 112, 150, 22);
    const int shadowEditIds[] = { IDC_CONFIG_SHADOW_DISTANCE, IDC_CONFIG_SHADOW_NPC_LIMIT,
        IDC_CONFIG_SHADOW_OBJECT_LIMIT, IDC_CONFIG_SHADOW_UPDATE, IDC_CONFIG_SHADOW_MIN_SIZE,
        IDC_CONFIG_SHADOW_MAX_SIZE, IDC_CONFIG_SHADOW_LENGTH, IDC_CONFIG_SHADOW_OPACITY };
    for (int i = 0; i < 8; ++i)
    {
        place(IDC_CONFIG_SHADOW_DISTANCE_LABEL + i, 28 + (i % 4) * 176,
            154 + (i / 4) * 38, 104, 22);
        place(shadowEditIds[i], 136 + (i % 4) * 176, 150 + (i / 4) * 38, 54, 24);
    }
    place(IDC_CONFIG_SHADOW_RECEIVER_LABEL, 28, 234, 120, 22);
    place(IDC_CONFIG_SHADOW_RECEIVER_QUALITY, 154, 230, 180, 100);

    // General: keep each panel's controls on separate, padded rows.
    place(IDC_CONFIG_TITLE_BACKGROUND_LABEL, 24, 202, 118, 22);
    place(IDC_CONFIG_TITLE_BACKGROUND_MODE, 142, 198, 150, 110);
    place(IDC_CONFIG_TITLE_BACKGROUND_ZONE, 304, 198, 260, 360);
    place(IDC_CONFIG_SHOW_TITLE_UI, 574, 198, 158, 22);
    place(IDC_CONFIG_TITLE_MUSIC_LABEL, 24, 232, 118, 22);
    place(IDC_CONFIG_TITLE_MUSIC_MODE, 142, 228, 150, 140);
    place(IDC_CONFIG_TITLE_MUSIC, 304, 228, 260, 180);
    place(IDC_CONFIG_WINDOW_MODE, 142, 302, 170, 120);
    place(IDC_CONFIG_RESOLUTION, 142, 338, 170, 180);
    place(IDC_CONFIG_RENDERING_BACKEND, 188, 374, 176, 180);
    place(IDC_CONFIG_RENDER_RESOLUTION_LABEL, 390, 306, 138, 22);
    place(IDC_CONFIG_RENDER_RESOLUTION, 536, 302, 196, 180);
    place(IDC_CONFIG_ANTI_ALIASING_LABEL, 390, 338, 342, 22);
    place(IDC_CONFIG_ANTI_ALIASING, 390, 360, 142, 150);
    place(IDC_CONFIG_POST_PROCESS_AA_LABEL, 390, 392, 342, 22);
    place(IDC_CONFIG_POST_PROCESS_AA, 390, 414, 142, 120);
    place(IDC_CONFIG_WINDOW_MODE_LABEL, 24, 306, 100, 18);
    place(IDC_CONFIG_RESOLUTION_LABEL, 24, 342, 100, 18);
    place(IDC_CONFIG_RENDERING_BACKEND_LABEL, 24, 378, 156, 22);
    place(IDC_CONFIG_GAME_MODE_LABEL, 24, 496, 88, 22);
    place(IDC_CONFIG_EDIT_GAME, 116, 492, 56, 26);
    place(IDC_CONFIG_EDIT_MODE_LABEL, 180, 496, 84, 22);
    place(IDC_CONFIG_COLLISION, 24, 530, 190, 22);
    place(IDC_CONFIG_MOVEMENT_STYLE_LABEL, 390, 496, 138, 22);
    place(IDC_CONFIG_MOVEMENT_STYLE, 536, 492, 196, 120);
    place(IDC_CONFIG_SCENE_CLOCK_LABEL, 390, 532, 138, 22);
    place(IDC_CONFIG_SCENE_CLOCK, 536, 528, 196, 100);
    place(IDC_CONFIG_DOOR_INTERACTION_LABEL, 390, 568, 138, 22);
    place(IDC_CONFIG_DOOR_INTERACTION, 536, 564, 196, 100);

    // Interface: all three groups share one page instead of hiding behind
    // sub-tabs. The taller dialog leaves each group enough room to breathe.
    if (const HWND hardwareCursor = GetDlgItem(window, IDC_CONFIG_HARDWARE_CURSOR))
        SetWindowLongPtrA(hardwareCursor, GWLP_USERDATA, ControlPageTag(kInterfacePage));
    place(IDC_CONFIG_COLOR_THEME_LABEL, 28, 112, 100, 22);
    place(IDC_CONFIG_COLOR_THEME, 142, 108, 180, 96);
    place(IDC_CONFIG_CURSOR_STYLE_LABEL, 370, 112, 110, 22);
    place(IDC_CONFIG_CURSOR_STYLE, 486, 108, 246, 140);
    place(IDC_CONFIG_HARDWARE_CURSOR, 28, 140, 264, 22);

    place(IDC_CONFIG_CHAT_TIMEOUT_LABEL, 28, 216, 120, 22);
    place(IDC_CONFIG_CHAT_TIMEOUT, 158, 212, 56, 24);
    place(IDC_CONFIG_CHAT_WIDTH_LABEL, 250, 216, 68, 22);
    place(IDC_CONFIG_CHAT_WIDTH, 328, 212, 50, 24);
    place(IDC_CONFIG_CHAT_HEIGHT_LABEL, 414, 216, 70, 22);
    place(IDC_CONFIG_CHAT_HEIGHT, 494, 212, 50, 24);
    place(IDC_CONFIG_CHAT_FONT_LABEL, 28, 250, 120, 22);
    place(IDC_CONFIG_CHAT_FONT, 158, 246, 220, 180);
    place(IDC_CONFIG_CHAT_FONT_SIZE_LABEL, 414, 250, 70, 22);
    place(IDC_CONFIG_CHAT_FONT_SIZE, 494, 246, 50, 24);

    place(IDC_CONFIG_PLAYER_ICON_LABEL, 28, 326, 100, 22);
    place(IDC_CONFIG_PLAYER_ICON, 142, 322, 260, 280);
    place(IDC_CONFIG_JOB_MASTER, 428, 326, 264, 22);
    place(IDC_CONFIG_SUBTITLE_FIELD_LABEL, 28, 360, 100, 22);
    place(IDC_CONFIG_SUBTITLE, 142, 356, 260, 120);
    place(IDC_CONFIG_SUBTITLE_LABEL, 28, 398, 110, 22);
    place(IDC_CONFIG_LINKSHELL_NAME, 142, 393, 260, 24);
    place(IDC_CONFIG_MAIN_JOB, 142, 393, 88, 240);
    place(IDC_CONFIG_MAIN_LV_LABEL, 238, 398, 25, 22);
    place(IDC_CONFIG_MAIN_LEVEL, 264, 393, 50, 24);
    place(IDC_CONFIG_JOB_SEPARATOR, 322, 398, 20, 22);
    place(IDC_CONFIG_SUB_JOB, 342, 393, 88, 240);
    place(IDC_CONFIG_SUB_LV_LABEL, 438, 398, 25, 22);
    place(IDC_CONFIG_SUB_LEVEL, 464, 393, 50, 24);
    place(IDC_CONFIG_LINKSHELL_COLOR_LABEL, 548, 360, 170, 22);
    place(IDC_CONFIG_COLOR_R_LABEL, 548, 398, 12, 22);
    place(IDC_CONFIG_LINKSHELL_COLOR_R, 564, 393, 40, 24);
    place(IDC_CONFIG_COLOR_G_LABEL, 612, 398, 12, 22);
    place(IDC_CONFIG_LINKSHELL_COLOR_G, 628, 393, 40, 24);
    place(IDC_CONFIG_COLOR_B_LABEL, 676, 398, 12, 22);
    place(IDC_CONFIG_LINKSHELL_COLOR_B, 692, 393, 40, 24);
}

LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NCCREATE)
    {
        const CREATESTRUCTA* create = reinterpret_cast<const CREATESTRUCTA*>(lParam);
        SetWindowLongPtrA(window, GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(create ? create->lpCreateParams : nullptr));
    }

    ConfigDialog::State* state = GetState(window);
    switch (message)
    {
    case WM_CREATE:
        if (!state || !state->settings || !state->theme)
            return -1;
        state->window = window;
        CreateControls(*state);
        Win32Theme::ApplyWindowTheme(window, *state->theme);
        ApplyActiveTab(*state);
        ConfigDialog::Sync(*state);
        state->categoryTab = std::clamp(state->categoryTab, 0, 5);
        UpdatePageScroll(*state, state->scrollOffsets[state->categoryTab]);
        return 0;

    case WM_HSCROLL:
        if (state && state->settings &&
            reinterpret_cast<HWND>(lParam) == GetDlgItem(window, IDC_CONFIG_BUMP_INTENSITY))
        {
            state->settings->bumpMappingIntensityPercent =
                ApplicationSettings::ClampBumpMappingIntensity(static_cast<int>(
                    SendDlgItemMessageA(window, IDC_CONFIG_BUMP_INTENSITY,
                        TBM_GETPOS, 0, 0)));
            Notify(*state, ConfigDialog::Command::BumpMappingChanged);
            return 0;
        }
        break;

    case WM_KEYDOWN:
        if (state && state->capturingBinding >= 0 && state->settings)
        {
            const int index = state->capturingBinding;
            state->capturingBinding = -1;
            if (wParam != VK_ESCAPE)
            {
                int* target = BindingValue(*state->settings, index);
                const int previous = *target;
                for (int other = 0; other < kBindingCount; ++other)
                {
                    if (other != index && *BindingValue(*state->settings, other) ==
                        static_cast<int>(wParam))
                    {
                        *BindingValue(*state->settings, other) = previous;
                        break;
                    }
                }
                *target = static_cast<int>(wParam);
            }
            ConfigDialog::Sync(*state);
            return 0;
        }
        break;

    case WM_COMMAND:
        if (!state || !state->settings)
            break;
        switch (LOWORD(wParam))
        {
        case IDC_CONFIG_KEY_FORWARD:
        case IDC_CONFIG_KEY_BACKWARD:
        case IDC_CONFIG_KEY_LEFT:
        case IDC_CONFIG_KEY_RIGHT:
        case IDC_CONFIG_KEY_JUMP:
        case IDC_CONFIG_KEY_AUTORUN:
        case IDC_CONFIG_KEY_RUN_TOGGLE:
        case IDC_CONFIG_KEY_GAME_MODE:
        case IDC_CONFIG_KEY_ZONE_MAP:
        case IDC_CONFIG_KEY_UNSTICK:
        case IDC_CONFIG_KEY_WEATHER:
        case IDC_CONFIG_KEY_CAMERA_DEBUG:
        case IDC_CONFIG_KEY_BUMP_MAPPING:
        case IDC_CONFIG_KEY_MAIN_MENU:
            state->capturingBinding = LOWORD(wParam) - IDC_CONFIG_KEY_FORWARD;
            SetWindowTextA(GetDlgItem(window, LOWORD(wParam)), "Press a key...");
            SetFocus(window);
            return 0;
        case IDC_CONFIG_SET_PATH:
            Notify(*state, ConfigDialog::Command::SetPath);
            return 0;
        case IDC_CONFIG_DETECT_PATH:
            Notify(*state, ConfigDialog::Command::AutoDetectPath);
            return 0;
        case IDC_CONFIG_RESET_PATH:
            Notify(*state, ConfigDialog::Command::ResetPath);
            return 0;
        case IDC_CONFIG_SHOW_PATH:
            Notify(*state, ConfigDialog::Command::ShowPath);
            return 0;
        case IDC_CONFIG_MIP_MAPPING:
            state->settings->enableMipMapping =
                SendDlgItemMessageA(window, IDC_CONFIG_MIP_MAPPING, BM_GETCHECK, 0, 0) == BST_CHECKED;
            Notify(*state, ConfigDialog::Command::MipMappingChanged);
            return 0;
        case IDC_CONFIG_BUMP_MAPPING:
            state->settings->enableBumpMapping =
                SendDlgItemMessageA(window, IDC_CONFIG_BUMP_MAPPING, BM_GETCHECK, 0, 0) == BST_CHECKED;
            Notify(*state, ConfigDialog::Command::BumpMappingChanged);
            return 0;
        case IDC_CONFIG_INVERT_BUMP_MAPPING:
            state->settings->invertBumpMapping = SendDlgItemMessageA(
                window, IDC_CONFIG_INVERT_BUMP_MAPPING, BM_GETCHECK, 0, 0) == BST_CHECKED;
            Notify(*state, ConfigDialog::Command::BumpMappingChanged);
            return 0;
        case IDC_CONFIG_HD_TEXTURES:
            state->settings->enableHdTextures =
                SendDlgItemMessageA(window, IDC_CONFIG_HD_TEXTURES, BM_GETCHECK, 0, 0) == BST_CHECKED;
            Notify(*state, ConfigDialog::Command::CustomTextureSettingsChanged);
            return 0;
        case IDC_CONFIG_PBR:
            state->settings->enablePbr =
                SendDlgItemMessageA(window, IDC_CONFIG_PBR, BM_GETCHECK, 0, 0) == BST_CHECKED;
            Notify(*state, ConfigDialog::Command::CustomTextureSettingsChanged);
            return 0;
        case IDC_CONFIG_HD_TEXTURE_FOLDER:
        case IDC_CONFIG_PBR_TEXTURE_FOLDER:
            if (HIWORD(wParam) == EN_SETFOCUS)
            {
                char folder[MAX_PATH] = {};
                const bool hd = LOWORD(wParam) == IDC_CONFIG_HD_TEXTURE_FOLDER;
                const std::string& current = hd ? state->settings->hdTextureFolder :
                    state->settings->pbrTextureFolder;
                strcpy_s(folder, current.c_str());
                if (FFXIInstallPath::BrowseForFolder(window,
                        hd ? "Select the HD texture folder" : "Select the PBR texture folder",
                        folder, sizeof(folder)))
                {
                    (hd ? state->settings->hdTextureFolder : state->settings->pbrTextureFolder) = folder;
                    Notify(*state, ConfigDialog::Command::CustomTextureSettingsChanged);
                }
                SetFocus(window);
            }
            return 0;
        case IDC_CONFIG_DOOR_INTERACTION:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                state->settings->doorInteractionMode = SendDlgItemMessageA(
                    window, IDC_CONFIG_DOOR_INTERACTION, CB_GETCURSEL, 0, 0) == 1 ?
                    ApplicationSettings::DoorPhysics : ApplicationSettings::DoorClassic;
                Notify(*state, ConfigDialog::Command::DoorInteractionChanged);
            }
            return 0;
        case IDC_CONFIG_LIGHTING_QUALITY:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                state->settings->lightingQuality = ApplicationSettings::ClampLightingQuality(static_cast<int>(
                    SendDlgItemMessageA(window, IDC_CONFIG_LIGHTING_QUALITY, CB_GETCURSEL, 0, 0)));
                Notify(*state, ConfigDialog::Command::LightingQualityChanged);
            }
            return 0;
        case IDC_CONFIG_LIGHT_DIRECTION_MODE:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                state->settings->lightDirectionMode = ApplicationSettings::ClampLightDirectionMode(
                    static_cast<int>(SendDlgItemMessageA(
                        window, IDC_CONFIG_LIGHT_DIRECTION_MODE, CB_GETCURSEL, 0, 0)));
                Notify(*state, ConfigDialog::Command::LightDirectionChanged);
            }
            return 0;
        case IDC_CONFIG_LIGHT_AZIMUTH:
        case IDC_CONFIG_LIGHT_ELEVATION:
            if (HIWORD(wParam) == EN_KILLFOCUS)
            {
                if (LOWORD(wParam) == IDC_CONFIG_LIGHT_AZIMUTH)
                {
                    int& azimuth = state->settings->lightDirectionMode ==
                        ApplicationSettings::LightDirectionAutomatic
                        ? state->settings->lightAzimuthOffsetDegrees
                        : state->settings->lightAzimuthDegrees;
                    azimuth = ApplicationSettings::ClampLightAzimuth(
                        ReadSignedInteger(window, IDC_CONFIG_LIGHT_AZIMUTH,
                            azimuth));
                }
                else
                {
                    int& elevation = state->settings->lightDirectionMode ==
                        ApplicationSettings::LightDirectionAutomatic
                        ? state->settings->lightElevationOffsetDegrees
                        : state->settings->lightElevationDegrees;
                    elevation = ApplicationSettings::ClampLightElevation(
                        ReadSignedInteger(window, IDC_CONFIG_LIGHT_ELEVATION,
                            elevation));
                }
                Notify(*state, ConfigDialog::Command::LightDirectionChanged);
            }
            return 0;
        case IDC_CONFIG_SCENE_CLOCK:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                state->settings->sceneClockMode = ApplicationSettings::ClampSceneClockMode(
                    static_cast<int>(SendDlgItemMessageA(
                        window, IDC_CONFIG_SCENE_CLOCK, CB_GETCURSEL, 0, 0)));
                Notify(*state, ConfigDialog::Command::SceneClockChanged);
            }
            return 0;
        case IDC_CONFIG_TITLE_BACKGROUND_MODE:
        case IDC_CONFIG_TITLE_BACKGROUND_ZONE:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                state->settings->titleBackgroundMode =
                    ApplicationSettings::ClampTitleBackgroundMode(static_cast<int>(
                        SendDlgItemMessageA(window, IDC_CONFIG_TITLE_BACKGROUND_MODE,
                            CB_GETCURSEL, 0, 0)));
                state->settings->titleBackgroundZoneIndex =
                    ApplicationSettings::ClampTitleBackgroundZoneIndex(static_cast<int>(
                        SendDlgItemMessageA(window, IDC_CONFIG_TITLE_BACKGROUND_ZONE,
                            CB_GETCURSEL, 0, 0)));
                Notify(*state, ConfigDialog::Command::TitleBackgroundChanged);
            }
            return 0;
        case IDC_CONFIG_TITLE_MUSIC_MODE:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                state->settings->titleMusicMode = ApplicationSettings::ClampTitleMusicMode(
                    static_cast<int>(SendDlgItemMessageA(window, IDC_CONFIG_TITLE_MUSIC_MODE,
                        CB_GETCURSEL, 0, 0)));
                Notify(*state, ConfigDialog::Command::TitleBackgroundChanged);
            }
            return 0;
        case IDC_CONFIG_SHADOW_PLAYER:
        case IDC_CONFIG_SHADOW_NPCS:
        case IDC_CONFIG_SHADOW_OBJECTS:
        case IDC_CONFIG_SHADOW_GROUND:
        case IDC_CONFIG_SHADOW_ALPHA:
        case IDC_CONFIG_SHADOW_DEBUG:
        case IDC_CONFIG_SHADOW_COUNTERS:
        {
            const auto checked = [window](const int id) {
                return SendDlgItemMessageA(window, id, BM_GETCHECK, 0, 0) == BST_CHECKED;
            };
            state->settings->shadowPlayer = checked(IDC_CONFIG_SHADOW_PLAYER);
            state->settings->shadowNpcs = checked(IDC_CONFIG_SHADOW_NPCS);
            state->settings->shadowObjects = checked(IDC_CONFIG_SHADOW_OBJECTS);
            state->settings->shadowGroundLikeObjects = checked(IDC_CONFIG_SHADOW_GROUND);
            state->settings->shadowAlphaTestedObjects = checked(IDC_CONFIG_SHADOW_ALPHA);
            state->settings->shadowDebugVisualization = checked(IDC_CONFIG_SHADOW_DEBUG);
            state->settings->shadowPerformanceCounters = checked(IDC_CONFIG_SHADOW_COUNTERS);
            Notify(*state, ConfigDialog::Command::ShadowSettingsChanged);
            return 0;
        }
        case IDC_CONFIG_SHADOW_DISTANCE:
        case IDC_CONFIG_SHADOW_NPC_LIMIT:
        case IDC_CONFIG_SHADOW_OBJECT_LIMIT:
        case IDC_CONFIG_SHADOW_MIN_SIZE:
        case IDC_CONFIG_SHADOW_MAX_SIZE:
        case IDC_CONFIG_SHADOW_UPDATE:
        case IDC_CONFIG_SHADOW_LENGTH:
        case IDC_CONFIG_SHADOW_OPACITY:
            if (HIWORD(wParam) == EN_KILLFOCUS)
            {
                state->settings->shadowMaxDistance = std::clamp(ReadSignedInteger(window, IDC_CONFIG_SHADOW_DISTANCE, 80), 0, 9999);
                state->settings->shadowNpcLimit = std::clamp(ReadSignedInteger(window, IDC_CONFIG_SHADOW_NPC_LIMIT, 16), 0, 9999);
                state->settings->shadowObjectLimit = std::clamp(ReadSignedInteger(window, IDC_CONFIG_SHADOW_OBJECT_LIMIT, 64), 0, 9999);
                state->settings->shadowMinimumSizePercent = std::clamp(ReadSignedInteger(window, IDC_CONFIG_SHADOW_MIN_SIZE, 25), 0, 9999);
                state->settings->shadowMaximumSize = std::clamp(ReadSignedInteger(window, IDC_CONFIG_SHADOW_MAX_SIZE, 40), 0, 9999);
                state->settings->shadowReceiverUpdateFrames = std::clamp(ReadSignedInteger(window, IDC_CONFIG_SHADOW_UPDATE, 4), 1, 9999);
                state->settings->shadowMaximumLength = std::clamp(ReadSignedInteger(window, IDC_CONFIG_SHADOW_LENGTH, 30), 1, 9999);
                state->settings->shadowOpacityPercent = std::clamp(ReadSignedInteger(window, IDC_CONFIG_SHADOW_OPACITY, 32), 0, 100);
                Notify(*state, ConfigDialog::Command::ShadowSettingsChanged);
            }
            return 0;
        case IDC_CONFIG_SHADOW_RECEIVER_QUALITY:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                state->settings->shadowReceiverQuality = std::clamp(static_cast<int>(
                    SendDlgItemMessageA(window, IDC_CONFIG_SHADOW_RECEIVER_QUALITY,
                        CB_GETCURSEL, 0, 0)), 0, 2);
                Notify(*state, ConfigDialog::Command::ShadowSettingsChanged);
            }
            return 0;
        case IDC_CONFIG_TITLE_MUSIC:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                state->settings->titleMusicIndex = ApplicationSettings::ClampTitleMusicIndex(
                    static_cast<int>(SendDlgItemMessageA(window, IDC_CONFIG_TITLE_MUSIC,
                        CB_GETCURSEL, 0, 0)));
                Notify(*state, ConfigDialog::Command::TitleBackgroundChanged);
            }
            return 0;
        case IDC_CONFIG_SHOW_TITLE_UI:
            if (HIWORD(wParam) == BN_CLICKED)
            {
                state->settings->showTitleUi = SendDlgItemMessageA(
                    window, IDC_CONFIG_SHOW_TITLE_UI, BM_GETCHECK, 0, 0) != BST_CHECKED;
                Notify(*state, ConfigDialog::Command::TitleBackgroundChanged);
            }
            return 0;
        case IDC_CONFIG_ENV_ANIM:
            state->settings->environmentalAnimationMode =
                ApplicationSettings::ClampEnvironmentalAnimationMode(static_cast<int>(
                    SendDlgItemMessageA(window, IDC_CONFIG_ENV_ANIM, CB_GETCURSEL, 0, 0)));
            Notify(*state, ConfigDialog::Command::EnvironmentalAnimationChanged);
            return 0;
        case IDC_CONFIG_WINDOW_MODE:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                state->settings->windowMode = ApplicationSettings::ClampWindowMode(static_cast<int>(
                    SendDlgItemMessageA(window, IDC_CONFIG_WINDOW_MODE, CB_GETCURSEL, 0, 0)));
                Notify(*state, ConfigDialog::Command::DisplayChanged);
            }
            return 0;
        case IDC_CONFIG_RESOLUTION:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                state->settings->resolutionIndex = ApplicationSettings::ClampResolutionIndex(static_cast<int>(
                    SendDlgItemMessageA(window, IDC_CONFIG_RESOLUTION, CB_GETCURSEL, 0, 0)));
                Notify(*state, ConfigDialog::Command::DisplayChanged);
            }
            return 0;
        case IDC_CONFIG_RENDER_RESOLUTION:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                state->settings->renderingResolutionScaleIndex =
                    ApplicationSettings::ClampRenderingResolutionScaleIndex(static_cast<int>(
                        SendDlgItemMessageA(window, IDC_CONFIG_RENDER_RESOLUTION,
                            CB_GETCURSEL, 0, 0)));
                Notify(*state, ConfigDialog::Command::RenderingResolutionChanged);
            }
            return 0;
        case IDC_CONFIG_ANTI_ALIASING:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                state->settings->antiAliasingIndex = ApplicationSettings::ClampAntiAliasingIndex(
                    static_cast<int>(SendDlgItemMessageA(
                        window, IDC_CONFIG_ANTI_ALIASING, CB_GETCURSEL, 0, 0)));
                Notify(*state, ConfigDialog::Command::RenderingResolutionChanged);
            }
            return 0;
        case IDC_CONFIG_POST_PROCESS_AA:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                state->settings->postProcessAntiAliasingMode =
                    ApplicationSettings::ClampPostProcessAntiAliasingMode(static_cast<int>(
                        SendDlgItemMessageA(window, IDC_CONFIG_POST_PROCESS_AA,
                            CB_GETCURSEL, 0, 0)));
                Notify(*state, ConfigDialog::Command::RenderingResolutionChanged);
            }
            return 0;
        case IDC_CONFIG_EDIT_GAME:
            state->pendingGameMode = !state->pendingGameMode;
            InvalidateRect(GetDlgItem(window, IDC_CONFIG_EDIT_GAME), nullptr, TRUE);
            return 0;
        case IDC_CONFIG_MOVEMENT_STYLE:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                state->settings->movementStyle = ApplicationSettings::ClampMovementStyle(
                    static_cast<int>(SendDlgItemMessageA(
                        window, IDC_CONFIG_MOVEMENT_STYLE, CB_GETCURSEL, 0, 0)));
                Notify(*state, ConfigDialog::Command::MovementStyleChanged);
            }
            return 0;
        case IDC_CONFIG_MIRROR_WORLD:
            state->settings->mirrorWorldZones =
                SendDlgItemMessageA(window, IDC_CONFIG_MIRROR_WORLD, BM_GETCHECK, 0, 0) == BST_CHECKED;
            Notify(*state, ConfigDialog::Command::MirrorWorldChanged);
            return 0;
        case IDC_CONFIG_DRAW_DISTANCE:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                state->settings->drawDistanceIndex =
                    ApplicationSettings::ClampDrawDistanceIndex(static_cast<int>(
                        SendDlgItemMessageA(window, IDC_CONFIG_DRAW_DISTANCE, CB_GETCURSEL, 0, 0)));
                Notify(*state, ConfigDialog::Command::DrawDistanceChanged);
            }
            return 0;
        case IDC_CONFIG_TEXTURE_COMPRESSION:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                state->settings->enableTextureCompression =
                    SendDlgItemMessageA(window, IDC_CONFIG_TEXTURE_COMPRESSION, CB_GETCURSEL, 0, 0) != 1;
                Notify(*state, ConfigDialog::Command::TextureCompressionChanged);
            }
            return 0;
        case IDC_CONFIG_MAP_TEXTURE_COMPRESSION:
            state->settings->enableMapTextureCompression = SendDlgItemMessageA(
                window, IDC_CONFIG_MAP_TEXTURE_COMPRESSION, BM_GETCHECK, 0, 0) == BST_CHECKED;
            Notify(*state, ConfigDialog::Command::MapTextureCompressionChanged);
            return 0;
        case IDC_CONFIG_WEATHER_EFFECTS:
            state->settings->enableWeatherEffects = SendDlgItemMessageA(
                window, IDC_CONFIG_WEATHER_EFFECTS, BM_GETCHECK, 0, 0) == BST_CHECKED;
            Notify(*state, ConfigDialog::Command::WeatherEffectsChanged);
            return 0;
        case IDC_CONFIG_COLOR_THEME:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                state->pendingDarkTheme = SendDlgItemMessageA(
                    window, IDC_CONFIG_COLOR_THEME, CB_GETCURSEL, 0, 0) !=
                    ApplicationSettings::LightTheme;
            }
            return 0;
        case IDC_CONFIG_TAB_MENU:
        case IDC_CONFIG_TAB_CHAT:
        case IDC_CONFIG_TAB_NAMEPLATES:
            state->activeTab = LOWORD(wParam) == IDC_CONFIG_TAB_CHAT ? 1 :
                (LOWORD(wParam) == IDC_CONFIG_TAB_NAMEPLATES ? 2 : 0);
            ApplyActiveTab(*state);
            ConfigDialog::Sync(*state);
            return 0;
        case IDC_CONFIG_CURSOR_STYLE:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                state->settings->mouseCursorStyle = std::clamp(static_cast<int>(
                    SendDlgItemMessageA(window, IDC_CONFIG_CURSOR_STYLE, CB_GETCURSEL, 0, 0)), 0, 3);
                Notify(*state, ConfigDialog::Command::MouseCursorStyleChanged);
            }
            return 0;
        case IDC_CONFIG_CATEGORY_GENERAL:
        case IDC_CONFIG_CATEGORY_GRAPHICS:
        case IDC_CONFIG_CATEGORY_AUDIO:
        case IDC_CONFIG_CATEGORY_INTERFACE:
        case IDC_CONFIG_CATEGORY_KEYS:
        case IDC_CONFIG_CATEGORY_DEBUG:
            switch (LOWORD(wParam))
            {
            case IDC_CONFIG_CATEGORY_GRAPHICS: state->categoryTab = 1; break;
            case IDC_CONFIG_CATEGORY_AUDIO: state->categoryTab = 2; break;
            case IDC_CONFIG_CATEGORY_INTERFACE: state->categoryTab = 3; break;
            case IDC_CONFIG_CATEGORY_KEYS: state->categoryTab = 4; break;
            case IDC_CONFIG_CATEGORY_DEBUG: state->categoryTab = 5; break;
            default: state->categoryTab = 0; break;
            }
            ApplyActiveTab(*state);
            ConfigDialog::Sync(*state);
            state->categoryTab = std::clamp(state->categoryTab, 0, 5);
            UpdatePageScroll(*state, state->scrollOffsets[state->categoryTab]);
            return 0;
        case IDC_CONFIG_RENDERING_BACKEND:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                state->settings->renderingBackend =
                    ApplicationSettings::ClampRenderingBackend(static_cast<int>(
                        SendDlgItemMessageA(window, IDC_CONFIG_RENDERING_BACKEND, CB_GETCURSEL, 0, 0)));
                if (!ApplicationSettings::RenderingBackendIsAvailable(
                        state->settings->renderingBackend))
                {
                    MessageBoxA(window,
                        "That rendering backend is selectable for planning, but is not implemented yet.\n"
                        "DATura will keep using DirectX 9 for rendering.",
                        "Rendering Backend", MB_OK | MB_ICONINFORMATION);
                }
                Notify(*state, ConfigDialog::Command::DisplayChanged);
            }
            return 0;
        case IDC_CONFIG_PLAYER_ICON:
            if (HIWORD(wParam) == CBN_SELCHANGE && state->gameUi)
            {
                const int selected = static_cast<int>(SendDlgItemMessageA(
                    window, IDC_CONFIG_PLAYER_ICON, CB_GETCURSEL, 0, 0));
                if (selected >= 0 && selected <= static_cast<int>(std::size(FFXIPlayerIcons::Catalog)))
                {
                    strcpy_s(state->gameUi->playerNameplate.icon, selected == 0 ? "none" :
                        FFXIPlayerIcons::Catalog[selected - 1].id);
                    Notify(*state, ConfigDialog::Command::PlayerNameplateChanged);
                }
            }
            return 0;
        case IDC_CONFIG_JOB_MASTER:
            if (state->gameUi)
            {
                state->gameUi->playerNameplate.jobMaster = SendDlgItemMessageA(
                    window, IDC_CONFIG_JOB_MASTER, BM_GETCHECK, 0, 0) == BST_CHECKED;
                Notify(*state, ConfigDialog::Command::PlayerNameplateChanged);
            }
            return 0;
        case IDC_CONFIG_SUBTITLE:
        case IDC_CONFIG_MAIN_JOB:
        case IDC_CONFIG_SUB_JOB:
            if (HIWORD(wParam) == CBN_SELCHANGE && state->gameUi)
            {
                const int id = LOWORD(wParam);
                const int selected = static_cast<int>(SendDlgItemMessageA(window, id, CB_GETCURSEL, 0, 0));
                auto& player = state->gameUi->playerNameplate;
                if (selected < 0) return 0;
                if (id == IDC_CONFIG_SUBTITLE && selected <= 2)
                    player.subtitleMode = static_cast<PlayerSubtitleMode>(selected);
                else if (id == IDC_CONFIG_MAIN_JOB && selected < FFXIStats::kJob_Count - 1)
                    player.mainJob = selected + 1;
                else if (id == IDC_CONFIG_SUB_JOB && selected < FFXIStats::kJob_Count)
                    player.subJob = selected;
                Notify(*state, ConfigDialog::Command::PlayerNameplateChanged);
            }
            return 0;
        case IDC_CONFIG_CHAT_WIDTH:
        case IDC_CONFIG_CHAT_HEIGHT:
        case IDC_CONFIG_CHAT_TIMEOUT:
        case IDC_CONFIG_CHAT_FONT_SIZE:
            if (HIWORD(wParam) == EN_KILLFOCUS && state->gameUi)
            {
                BOOL valid = FALSE;
                const int id = LOWORD(wParam);
                const UINT value = GetDlgItemInt(window, id, &valid, FALSE);
                if (valid && value <= 2147483647U)
                {
                    if (id == IDC_CONFIG_CHAT_WIDTH) state->gameUi->chatLogWidthPercent = std::clamp((int)value, 20, 100);
                    else if (id == IDC_CONFIG_CHAT_HEIGHT) state->gameUi->chatLogHeightPercent = std::clamp((int)value, 10, 75);
                    else if (id == IDC_CONFIG_CHAT_FONT_SIZE) state->gameUi->chatLogFontSize = std::clamp((int)value, 10, 32);
                    else state->gameUi->chatLogTimeoutSeconds = std::clamp((int)value, 1, 300);
                }
                Notify(*state, ConfigDialog::Command::ChatLogChanged);
            }
            return 0;
        case IDC_CONFIG_CHAT_FONT:
            if (HIWORD(wParam) == CBN_SELCHANGE && state->gameUi)
            {
                const int selected = static_cast<int>(SendDlgItemMessageA(
                    window, IDC_CONFIG_CHAT_FONT, CB_GETCURSEL, 0, 0));
                static const char* fonts[] = {"FFXI", "Arial", "Segoe UI", "Tahoma", "Verdana", "Consolas"};
                if (selected >= 0 && selected < static_cast<int>(std::size(fonts)))
                {
                    strcpy_s(state->gameUi->chatLogFont, fonts[selected]);
                    Notify(*state, ConfigDialog::Command::ChatLogChanged);
                }
            }
            return 0;
        case IDC_CONFIG_LINKSHELL_NAME:
        case IDC_CONFIG_MAIN_LEVEL:
        case IDC_CONFIG_SUB_LEVEL:
        case IDC_CONFIG_LINKSHELL_COLOR_R:
        case IDC_CONFIG_LINKSHELL_COLOR_G:
        case IDC_CONFIG_LINKSHELL_COLOR_B:
            if (HIWORD(wParam) == EN_KILLFOCUS && state->gameUi)
            {
                auto& player = state->gameUi->playerNameplate;
                const int id = LOWORD(wParam);
                if (id == IDC_CONFIG_LINKSHELL_NAME)
                    GetDlgItemTextA(window, id, player.linkshellName, sizeof(player.linkshellName));
                else if (id == IDC_CONFIG_LINKSHELL_COLOR_R || id == IDC_CONFIG_LINKSHELL_COLOR_G ||
                         id == IDC_CONFIG_LINKSHELL_COLOR_B)
                {
                    const int r = std::clamp((int)GetDlgItemInt(window, IDC_CONFIG_LINKSHELL_COLOR_R, nullptr, FALSE), 0, 255);
                    const int g = std::clamp((int)GetDlgItemInt(window, IDC_CONFIG_LINKSHELL_COLOR_G, nullptr, FALSE), 0, 255);
                    const int b = std::clamp((int)GetDlgItemInt(window, IDC_CONFIG_LINKSHELL_COLOR_B, nullptr, FALSE), 0, 255);
                    player.linkshellColor = RGB(r, g, b);
                }
                else
                {
                    const int level = std::clamp(static_cast<int>(GetDlgItemInt(window, id, nullptr, FALSE)), 1, 99);
                    (id == IDC_CONFIG_MAIN_LEVEL ? player.mainLevel : player.subLevel) = level;
                }
                Notify(*state, ConfigDialog::Command::PlayerNameplateChanged);
            }
            return 0;
        case IDC_CONFIG_COLLISION:
            state->settings->showCollisionGeometry =
                SendDlgItemMessageA(window, IDC_CONFIG_COLLISION, BM_GETCHECK, 0, 0) == BST_CHECKED;
            Notify(*state, ConfigDialog::Command::CollisionVisibilityChanged);
            return 0;
        case IDC_CONFIG_ENABLE_SOUNDS:
            state->settings->enableSounds =
                SendDlgItemMessageA(window, IDC_CONFIG_ENABLE_SOUNDS, BM_GETCHECK, 0, 0) == BST_CHECKED;
            Notify(*state, ConfigDialog::Command::SoundSettingsChanged);
            return 0;
        case IDC_CONFIG_BACKGROUND_SOUNDS:
            state->settings->playSoundsInBackground =
                SendDlgItemMessageA(window, IDC_CONFIG_BACKGROUND_SOUNDS, BM_GETCHECK, 0, 0) == BST_CHECKED;
            Notify(*state, ConfigDialog::Command::SoundSettingsChanged);
            return 0;
        case IDC_CONFIG_MAX_SOUNDS:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                state->settings->maxSimultaneousSounds =
                    ApplicationSettings::MaxSoundOptionValue(static_cast<int>(
                        SendDlgItemMessageA(window, IDC_CONFIG_MAX_SOUNDS, CB_GETCURSEL, 0, 0)));
                Notify(*state, ConfigDialog::Command::SoundSettingsChanged);
            }
            return 0;
        case IDC_CONFIG_HARDWARE_CURSOR:
            state->settings->enableHardwareMouseCursor =
                SendDlgItemMessageA(window, IDC_CONFIG_HARDWARE_CURSOR, BM_GETCHECK, 0, 0) == BST_CHECKED;
            Notify(*state, ConfigDialog::Command::HardwareCursorChanged);
            return 0;
        case IDC_CONFIG_APPLY:
            CommitPending(*state);
            Notify(*state, ConfigDialog::Command::ApplySettings,
                state->pendingDarkTheme ? ApplicationSettings::DarkTheme :
                    ApplicationSettings::LightTheme);
            if (state->isGameModeCallback &&
                state->pendingGameMode != state->isGameModeCallback(state->callbackContext))
                state->eventCallback(state->callbackContext,
                    { ConfigDialog::Command::ToggleGameMode, 0 });
            return 0;
        case IDC_CONFIG_CLOSE:
            CommitPending(*state);
            Notify(*state, ConfigDialog::Command::ApplySettings,
                state->pendingDarkTheme ? ApplicationSettings::DarkTheme :
                    ApplicationSettings::LightTheme);
            if (state->isGameModeCallback &&
                state->pendingGameMode != state->isGameModeCallback(state->callbackContext))
                state->eventCallback(state->callbackContext,
                    { ConfigDialog::Command::ToggleGameMode, 0 });
            DestroyWindow(window);
            return 0;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(window);
        return 0;

    case WM_VSCROLL:
        if (state)
        {
            int offset = state->scrollOffsets[std::clamp(state->categoryTab, 0, 5)];
            switch (LOWORD(wParam))
            {
            case SB_LINEUP: offset -= 24; break;
            case SB_LINEDOWN: offset += 24; break;
            case SB_PAGEUP: offset -= 160; break;
            case SB_PAGEDOWN: offset += 160; break;
            case SB_THUMBPOSITION:
            case SB_THUMBTRACK:
            {
                SCROLLINFO info = {sizeof(info)};
                info.fMask = SIF_TRACKPOS;
                GetScrollInfo(window, SB_VERT, &info);
                offset = info.nTrackPos;
                break;
            }
            case SB_TOP: offset = 0; break;
            case SB_BOTTOM: offset = 100000; break;
            default: return 0;
            }
            UpdatePageScroll(*state, offset);
        }
        return 0;

    case WM_MOUSEWHEEL:
        if (state)
        {
            const int offset = state->scrollOffsets[std::clamp(state->categoryTab, 0, 5)] -
                GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA * 48;
            UpdatePageScroll(*state, offset);
        }
        return 0;

    case WM_SIZE:
        if (state)
        {
            const int clientWidth = LOWORD(lParam);
            const int clientHeight = HIWORD(lParam);
            SetWindowPos(GetDlgItem(window, IDC_CONFIG_APPLY), nullptr,
                clientWidth - 252, clientHeight - 40, 112, 30,
                SWP_NOZORDER | SWP_NOACTIVATE);
            SetWindowPos(GetDlgItem(window, IDC_CONFIG_CLOSE), nullptr,
                clientWidth - 132, clientHeight - 40, 112, 30,
                SWP_NOZORDER | SWP_NOACTIVATE);
            UpdatePageScroll(*state,
                state->scrollOffsets[std::clamp(state->categoryTab, 0, 5)]);
        }
        return 0;

    case WM_GETMINMAXINFO:
        if (lParam)
        {
            MINMAXINFO* limits = reinterpret_cast<MINMAXINFO*>(lParam);
            // Preserve the designed two-column width while still allowing the
            // viewport to become shorter and rely on its vertical scrollbar.
            limits->ptMinTrackSize.x = 780;
            limits->ptMinTrackSize.y = 480;
        }
        return 0;

    case WM_DRAWITEM:
        if (state && state->theme)
        {
            const DRAWITEMSTRUCT* draw = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            if (draw && draw->CtlID == IDC_CONFIG_RENDERING_BACKEND)
            {
                const bool selected = (draw->itemState & ODS_SELECTED) != 0;
                const bool active = ApplicationSettings::RenderingBackendIsAvailable(
                    static_cast<int>(draw->itemID));
                const COLORREF background = selected ? RGB(55, 70, 84) : RGB(31, 38, 44);
                const COLORREF text = active ? RGB(235, 235, 235) : RGB(125, 130, 135);
                const HBRUSH brush = CreateSolidBrush(background);
                FillRect(draw->hDC, &draw->rcItem, brush);
                DeleteObject(brush);
                SetTextColor(draw->hDC, text);
                SetBkMode(draw->hDC, TRANSPARENT);
                char label[64] = {};
                SendDlgItemMessageA(window, IDC_CONFIG_RENDERING_BACKEND,
                    CB_GETLBTEXT, draw->itemID, reinterpret_cast<LPARAM>(label));
                RECT textRect = draw->rcItem;
                textRect.left += 4;
                DrawTextA(draw->hDC, label, -1, &textRect, DT_SINGLELINE | DT_VCENTER);
                return TRUE;
            }
            if (draw && draw->CtlID == IDC_CONFIG_EDIT_GAME)
            {
                const bool gameMode = state->pendingGameMode;
                RECT track = draw->rcItem;
                InflateRect(&track, -2, -4);
                const COLORREF trackColor = state->theme->dark ? RGB(52, 62, 72) : RGB(185, 190, 195);
                const COLORREF activeColor = RGB(52, 146, 235);
                const HBRUSH trackBrush = CreateSolidBrush(trackColor);
                const HPEN trackPen = CreatePen(PS_SOLID, 1,
                    state->theme->dark ? RGB(92, 104, 116) : RGB(135, 140, 145));
                const HGDIOBJ oldBrush = SelectObject(draw->hDC, trackBrush);
                const HGDIOBJ oldPen = SelectObject(draw->hDC, trackPen);
                RoundRect(draw->hDC, track.left, track.top, track.right, track.bottom, 18, 18);

                const int knobSize = (track.bottom - track.top) - 4;
                RECT knob = {gameMode ? track.left + 2 : track.right - knobSize - 2,
                    track.top + 2, 0, track.top + 2 + knobSize};
                knob.right = knob.left + knobSize;
                const HBRUSH knobBrush = CreateSolidBrush(gameMode ? activeColor : RGB(225, 228, 232));
                SelectObject(draw->hDC, knobBrush);
                SelectObject(draw->hDC, GetStockObject(NULL_PEN));
                Ellipse(draw->hDC, knob.left, knob.top, knob.right, knob.bottom);
                SelectObject(draw->hDC, oldBrush);
                SelectObject(draw->hDC, oldPen);
                DeleteObject(knobBrush);
                DeleteObject(trackBrush);
                DeleteObject(trackPen);
                return TRUE;
            }
            Win32Theme::DrawButton(draw, state->theme->dark, state->theme->resources.font,
                draw && (draw->CtlID == IDC_CONFIG_CLOSE ||
                    draw->CtlID == IDC_CONFIG_APPLY ||
                    (draw->CtlID == IDC_CONFIG_CATEGORY_GENERAL && state->categoryTab == 0) ||
                    (draw->CtlID == IDC_CONFIG_CATEGORY_GRAPHICS && state->categoryTab == 1) ||
                    (draw->CtlID == IDC_CONFIG_CATEGORY_AUDIO && state->categoryTab == 2) ||
                    (draw->CtlID == IDC_CONFIG_CATEGORY_INTERFACE && state->categoryTab == 3) ||
                    (draw->CtlID == IDC_CONFIG_CATEGORY_KEYS && state->categoryTab == 4) ||
                    (draw->CtlID == IDC_CONFIG_CATEGORY_DEBUG && state->categoryTab == 5) ||
                    (draw->CtlID == IDC_CONFIG_TAB_MENU && state->activeTab == 0) ||
                    (draw->CtlID == IDC_CONFIG_TAB_CHAT && state->activeTab == 1) ||
                    (draw->CtlID == IDC_CONFIG_TAB_NAMEPLATES && state->activeTab == 2)));
        }
        return TRUE;

    case WM_PAINT:
        if (state && state->theme)
        {
            PAINTSTRUCT paint = {};
            const HDC deviceContext = BeginPaint(window, &paint);
            RECT client = {};
            GetClientRect(window, &client);
            FillRect(deviceContext, &client, state->theme->resources.windowBrush);
            RECT panels[4] = {};
            const char* titles[4] = {};
            int panelCount = 0;
            switch (state->categoryTab)
            {
            case 0:
                panels[0] = {14, 72, 746, 170}; titles[0] = "FFXI Installation";
                panels[1] = {14, 176, 746, 266}; titles[1] = "Title Background";
                panels[2] = {14, 272, 746, 454}; titles[2] = "Display";
                panels[3] = {14, 464, 746, 620}; titles[3] = "Mode / Input";
                panelCount = 4;
                break;
            case 1:
                panels[0] = {14, 82, 746, 302}; titles[0] = "Textures / Geometry";
                panels[1] = {14, 310, 746, 402}; titles[1] = "Environment";
                panels[2] = {14, 410, 746, 500}; titles[2] = "Lighting";
                panelCount = 3;
                break;
            case 2:
                panels[0] = {14, 82, 746, 170}; titles[0] = "Audio";
                panelCount = 1;
                break;
            case 3:
                panels[0] = {14, 82, 746, 170}; titles[0] = "Menu";
                panels[1] = {14, 184, 746, 282}; titles[1] = "Chat Log";
                panels[2] = {14, 296, 746, 438}; titles[2] = "Nameplates";
                panelCount = 3;
                break;
            case 4:
                panels[0] = {14, 82, 746, 486}; titles[0] = "Keyboard Controls";
                panelCount = 1;
                break;
            default:
                panels[0] = {14, 82, 746, 278}; titles[0] = "Dynamic Shadow Diagnostics";
                panelCount = 1;
                break;
            }
            const int scrollOffset = state->scrollOffsets[std::clamp(state->categoryTab, 0, 5)];
            for (int i = 0; i < panelCount; ++i)
                OffsetRect(&panels[i], 0, -scrollOffset);
            const int savedDc = SaveDC(deviceContext);
            IntersectClipRect(deviceContext, 0, 76, client.right, client.bottom - 58);
            for (int i = 0; i < panelCount; ++i)
                Win32Theme::DrawPanel(deviceContext, panels[i], titles[i], state->theme->dark,
                    state->theme->resources.sectionFont);
            RestoreDC(deviceContext, savedDc);

            RECT footer = {0, client.bottom - 52, client.right, client.bottom};
            FillRect(deviceContext, &footer, state->theme->resources.controlBrush);
            const HPEN separator = CreatePen(PS_SOLID, 1,
                state->theme->dark ? RGB(70, 78, 86) : RGB(170, 175, 180));
            const HGDIOBJ oldPen = SelectObject(deviceContext, separator);
            MoveToEx(deviceContext, footer.left, footer.top, nullptr);
            LineTo(deviceContext, footer.right, footer.top);
            SelectObject(deviceContext, oldPen);
            DeleteObject(separator);
            EndPaint(window, &paint);
        }
        return 0;

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        if (state && state->theme)
        {
            const HDC deviceContext = reinterpret_cast<HDC>(wParam);
            SetTextColor(deviceContext, Win32Theme::TextColor(state->theme->dark));
            SetBkColor(deviceContext, Win32Theme::ControlColor(state->theme->dark));
            SetBkMode(deviceContext, TRANSPARENT);
            return reinterpret_cast<LRESULT>(state->theme->resources.controlBrush);
        }
        break;

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        if (state && state->theme)
        {
            const HDC deviceContext = reinterpret_cast<HDC>(wParam);
            SetTextColor(deviceContext, Win32Theme::TextColor(state->theme->dark));
            SetBkColor(deviceContext, Win32Theme::EditColor(state->theme->dark));
            return reinterpret_cast<LRESULT>(state->theme->resources.editBrush);
        }
        break;

    case WM_ERASEBKGND:
        if (state && state->theme)
        {
            RECT client = {};
            GetClientRect(window, &client);
            FillRect(reinterpret_cast<HDC>(wParam), &client, state->theme->resources.windowBrush);
            return 1;
        }
        break;

    case WM_DESTROY:
        if (state)
        {
            EndEditing(*state);
            if (state->window == window)
                state->window = NULL;
        }
        return 0;
    }

    return DefWindowProcA(window, message, wParam, lParam);
}
}

namespace ConfigDialog
{
void Initialize(
    State& state,
    const HWND owner,
    const char* const ffxiPath,
    ApplicationSettings::State& settings,
    Win32Theme::State& theme,
    GameUiConfig& gameUi,
    const EventCallback eventCallback,
    const IsGameModeCallback isGameModeCallback,
    void* const callbackContext)
{
    state.owner = owner;
    state.ffxiPath = ffxiPath;
    state.settings = &settings;
    state.liveSettings = &settings;
    state.theme = &theme;
    state.gameUi = &gameUi;
    state.liveGameUi = &gameUi;
    state.eventCallback = eventCallback;
    state.isGameModeCallback = isGameModeCallback;
    state.callbackContext = callbackContext;
}

void Show(State& state)
{
    state.categoryTab = std::clamp(state.categoryTab, 0, 5);
    if (state.window && IsWindow(state.window))
    {
        ApplyActiveTab(state);
        Sync(state);
        UpdatePageScroll(state, state.scrollOffsets[state.categoryTab]);
        Win32ToolWindow::Show(state.window, true);
        return;
    }
    if (!state.owner || !state.settings || !state.theme)
        return;

    BeginEditing(state);

    RECT ownerRect = {};
    GetWindowRect(state.owner, &ownerRect);
    const int width = 780;
    const int height = 700;
    Win32ToolWindow::Spec spec =
    {
        WindowProcedure, kWindowClassName, "DATura Config", width, height,
        WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_POPUP | WS_VISIBLE | WS_VSCROLL,
        state.theme->resources.windowBrush
    };
    spec.x = ownerRect.left + ((ownerRect.right - ownerRect.left) - width) / 2;
    spec.y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - height) / 2;
    spec.extendedStyle = WS_EX_DLGMODALFRAME;
    spec.createParameter = &state;
    state.window = Win32ToolWindow::Create(state.owner, spec);
    if (state.window)
    {
        ApplyActiveTab(state);
        Sync(state);
        state.categoryTab = std::clamp(state.categoryTab, 0, 5);
        UpdatePageScroll(state, state.scrollOffsets[state.categoryTab]);
        Win32ToolWindow::Show(state.window, true);
    }
}

void Close(State& state)
{
    if (state.window && IsWindow(state.window))
        DestroyWindow(state.window);
    state.window = NULL;
}

void Sync(State& state)
{
    if (!state.window || !IsWindow(state.window) || !state.settings || !state.theme)
        return;

    const ApplicationSettings::State& settings = *state.settings;
    if (state.gameUi)
    {
        SetDlgItemInt(state.window, IDC_CONFIG_CHAT_TIMEOUT, state.gameUi->chatLogTimeoutSeconds, FALSE);
        SetDlgItemInt(state.window, IDC_CONFIG_CHAT_WIDTH, state.gameUi->chatLogWidthPercent, FALSE);
        SetDlgItemInt(state.window, IDC_CONFIG_CHAT_HEIGHT, state.gameUi->chatLogHeightPercent, FALSE);
        SetDlgItemInt(state.window, IDC_CONFIG_CHAT_FONT_SIZE, state.gameUi->chatLogFontSize, FALSE);
        static const char* fonts[] = {"FFXI", "Arial", "Segoe UI", "Tahoma", "Verdana", "Consolas"};
        int chatFontIndex = 0;
        for (int i = 0; i < static_cast<int>(std::size(fonts)); ++i)
            if (_stricmp(state.gameUi->chatLogFont, fonts[i]) == 0) { chatFontIndex = i; break; }
        SendDlgItemMessageA(state.window, IDC_CONFIG_CHAT_FONT, CB_SETCURSEL, chatFontIndex, 0);
        const auto* icon = FFXIPlayerIcons::Find(state.gameUi->playerNameplate.icon);
        const int selected = icon ? static_cast<int>(icon - FFXIPlayerIcons::Catalog) + 1 : 0;
        SendDlgItemMessageA(state.window, IDC_CONFIG_PLAYER_ICON, CB_SETCURSEL, selected, 0);
        SendDlgItemMessageA(state.window, IDC_CONFIG_JOB_MASTER, BM_SETCHECK,
            state.gameUi->playerNameplate.jobMaster ? BST_CHECKED : BST_UNCHECKED, 0);
        const auto& player = state.gameUi->playerNameplate;
        SendDlgItemMessageA(state.window, IDC_CONFIG_SUBTITLE, CB_SETCURSEL, static_cast<int>(player.subtitleMode), 0);
        const bool linkshell = player.subtitleMode == PlayerSubtitleMode::Linkshell;
        const bool jobs = player.subtitleMode == PlayerSubtitleMode::Jobs;
        const bool nameplatesVisible = state.categoryTab == 3;
        SetDlgItemTextA(state.window, IDC_CONFIG_SUBTITLE_LABEL, jobs ? "Jobs / levels" : "Linkshell");
        ShowWindow(GetDlgItem(state.window, IDC_CONFIG_SUBTITLE_LABEL),
            nameplatesVisible && (linkshell || jobs) ? SW_SHOW : SW_HIDE);
        ShowWindow(GetDlgItem(state.window, IDC_CONFIG_LINKSHELL_NAME),
            nameplatesVisible && linkshell ? SW_SHOW : SW_HIDE);
        SetDlgItemTextA(state.window, IDC_CONFIG_LINKSHELL_NAME, player.linkshellName);
        for (const int id : {IDC_CONFIG_MAIN_JOB, IDC_CONFIG_SUB_JOB, IDC_CONFIG_MAIN_LEVEL,
            IDC_CONFIG_SUB_LEVEL, IDC_CONFIG_MAIN_LV_LABEL, IDC_CONFIG_SUB_LV_LABEL, IDC_CONFIG_JOB_SEPARATOR})
            ShowWindow(GetDlgItem(state.window, id), nameplatesVisible && jobs ? SW_SHOW : SW_HIDE);
        SendDlgItemMessageA(state.window, IDC_CONFIG_MAIN_JOB, CB_SETCURSEL, player.mainJob - 1, 0);
        SendDlgItemMessageA(state.window, IDC_CONFIG_SUB_JOB, CB_SETCURSEL, player.subJob, 0);
        SetDlgItemInt(state.window, IDC_CONFIG_MAIN_LEVEL, player.mainLevel, FALSE);
        SetDlgItemInt(state.window, IDC_CONFIG_SUB_LEVEL, player.subLevel, FALSE);
        EnableWindow(GetDlgItem(state.window, IDC_CONFIG_SUB_LEVEL), player.subJob != FFXIStats::kJob_None);
        SetDlgItemInt(state.window, IDC_CONFIG_LINKSHELL_COLOR_R, GetRValue(player.linkshellColor), FALSE);
        SetDlgItemInt(state.window, IDC_CONFIG_LINKSHELL_COLOR_G, GetGValue(player.linkshellColor), FALSE);
        SetDlgItemInt(state.window, IDC_CONFIG_LINKSHELL_COLOR_B, GetBValue(player.linkshellColor), FALSE);
    }
    SetDlgItemTextA(state.window, IDC_CONFIG_PATH_TEXT, state.ffxiPath ? state.ffxiPath : "");
    SendDlgItemMessageA(state.window, IDC_CONFIG_MIP_MAPPING, BM_SETCHECK,
        settings.enableMipMapping ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_BUMP_MAPPING, BM_SETCHECK,
        settings.enableBumpMapping ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_INVERT_BUMP_MAPPING, BM_SETCHECK,
        settings.invertBumpMapping ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_HD_TEXTURES, BM_SETCHECK,
        settings.enableHdTextures ? BST_CHECKED : BST_UNCHECKED, 0);
    SetDlgItemTextA(state.window, IDC_CONFIG_HD_TEXTURE_FOLDER, settings.hdTextureFolder.c_str());
    SendDlgItemMessageA(state.window, IDC_CONFIG_PBR, BM_SETCHECK,
        settings.enablePbr ? BST_CHECKED : BST_UNCHECKED, 0);
    SetDlgItemTextA(state.window, IDC_CONFIG_PBR_TEXTURE_FOLDER, settings.pbrTextureFolder.c_str());
    const int bumpIntensity = ApplicationSettings::ClampBumpMappingIntensity(
        settings.bumpMappingIntensityPercent);
    SendDlgItemMessageA(state.window, IDC_CONFIG_BUMP_INTENSITY, TBM_SETPOS,
        TRUE, bumpIntensity);
    char bumpIntensityText[16] = {};
    std::snprintf(bumpIntensityText, sizeof(bumpIntensityText), "%d%%", bumpIntensity);
    SetDlgItemTextA(state.window, IDC_CONFIG_BUMP_INTENSITY_VALUE, bumpIntensityText);
    EnableWindow(GetDlgItem(state.window, IDC_CONFIG_BUMP_INTENSITY),
        settings.enableBumpMapping ? TRUE : FALSE);
    EnableWindow(GetDlgItem(state.window, IDC_CONFIG_INVERT_BUMP_MAPPING),
        settings.enableBumpMapping ? TRUE : FALSE);
    SendDlgItemMessageA(state.window, IDC_CONFIG_DOOR_INTERACTION, CB_SETCURSEL,
        settings.doorInteractionMode == ApplicationSettings::DoorPhysics ? 1 : 0, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_LIGHTING_QUALITY, CB_SETCURSEL,
        ApplicationSettings::ClampLightingQuality(settings.lightingQuality), 0);
    const struct { int id; bool value; } shadowChecks[] =
    {
        { IDC_CONFIG_SHADOW_PLAYER, settings.shadowPlayer },
        { IDC_CONFIG_SHADOW_NPCS, settings.shadowNpcs },
        { IDC_CONFIG_SHADOW_OBJECTS, settings.shadowObjects },
        { IDC_CONFIG_SHADOW_GROUND, settings.shadowGroundLikeObjects },
        { IDC_CONFIG_SHADOW_ALPHA, settings.shadowAlphaTestedObjects },
        { IDC_CONFIG_SHADOW_DEBUG, settings.shadowDebugVisualization },
        { IDC_CONFIG_SHADOW_COUNTERS, settings.shadowPerformanceCounters }
    };
    for (const auto& check : shadowChecks)
        SendDlgItemMessageA(state.window, check.id, BM_SETCHECK,
            check.value ? BST_CHECKED : BST_UNCHECKED, 0);
    SetDlgItemInt(state.window, IDC_CONFIG_SHADOW_DISTANCE, settings.shadowMaxDistance, FALSE);
    SetDlgItemInt(state.window, IDC_CONFIG_SHADOW_NPC_LIMIT, settings.shadowNpcLimit, FALSE);
    SetDlgItemInt(state.window, IDC_CONFIG_SHADOW_OBJECT_LIMIT, settings.shadowObjectLimit, FALSE);
    SetDlgItemInt(state.window, IDC_CONFIG_SHADOW_MIN_SIZE, settings.shadowMinimumSizePercent, FALSE);
    SetDlgItemInt(state.window, IDC_CONFIG_SHADOW_MAX_SIZE, settings.shadowMaximumSize, FALSE);
    SetDlgItemInt(state.window, IDC_CONFIG_SHADOW_UPDATE, settings.shadowReceiverUpdateFrames, FALSE);
    SetDlgItemInt(state.window, IDC_CONFIG_SHADOW_LENGTH, settings.shadowMaximumLength, FALSE);
    SetDlgItemInt(state.window, IDC_CONFIG_SHADOW_OPACITY, settings.shadowOpacityPercent, FALSE);
    SendDlgItemMessageA(state.window, IDC_CONFIG_SHADOW_RECEIVER_QUALITY, CB_SETCURSEL,
        std::clamp(settings.shadowReceiverQuality, 0, 2), 0);
    const bool dynamicShadows = settings.lightingQuality == ApplicationSettings::LightingDynamicShadows;
    for (int id = IDC_CONFIG_SHADOW_PLAYER; id <= IDC_CONFIG_SHADOW_RECEIVER_QUALITY; ++id)
        EnableWindow(GetDlgItem(state.window, id), dynamicShadows ? TRUE : FALSE);
    SendDlgItemMessageA(state.window, IDC_CONFIG_LIGHT_DIRECTION_MODE, CB_SETCURSEL,
        ApplicationSettings::ClampLightDirectionMode(settings.lightDirectionMode), 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_SCENE_CLOCK, CB_SETCURSEL,
        ApplicationSettings::ClampSceneClockMode(settings.sceneClockMode), 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_TITLE_BACKGROUND_MODE, CB_SETCURSEL,
        ApplicationSettings::ClampTitleBackgroundMode(settings.titleBackgroundMode), 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_TITLE_BACKGROUND_ZONE, CB_SETCURSEL,
        ApplicationSettings::ClampTitleBackgroundZoneIndex(settings.titleBackgroundZoneIndex), 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_SHOW_TITLE_UI, BM_SETCHECK,
        settings.showTitleUi ? BST_UNCHECKED : BST_CHECKED, 0);
    EnableWindow(GetDlgItem(state.window, IDC_CONFIG_TITLE_BACKGROUND_ZONE),
        settings.titleBackgroundMode == ApplicationSettings::TitleBackgroundFixed);
    SendDlgItemMessageA(state.window, IDC_CONFIG_TITLE_MUSIC_MODE, CB_SETCURSEL,
        ApplicationSettings::ClampTitleMusicMode(settings.titleMusicMode), 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_TITLE_MUSIC, CB_SETCURSEL,
        ApplicationSettings::ClampTitleMusicIndex(settings.titleMusicIndex), 0);
    EnableWindow(GetDlgItem(state.window, IDC_CONFIG_TITLE_MUSIC),
        settings.titleMusicMode == ApplicationSettings::TitleMusicFixed);
    const bool automaticLight = settings.lightDirectionMode ==
        ApplicationSettings::LightDirectionAutomatic;
    SetDlgItemInt(state.window, IDC_CONFIG_LIGHT_AZIMUTH, ApplicationSettings::ClampLightAzimuth(
        automaticLight ? settings.lightAzimuthOffsetDegrees : settings.lightAzimuthDegrees), TRUE);
    SetDlgItemInt(state.window, IDC_CONFIG_LIGHT_ELEVATION, ApplicationSettings::ClampLightElevation(
        automaticLight ? settings.lightElevationOffsetDegrees : settings.lightElevationDegrees), TRUE);
    SendDlgItemMessageA(state.window, IDC_CONFIG_ENV_ANIM, CB_SETCURSEL,
        settings.environmentalAnimationMode, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_WINDOW_MODE, CB_SETCURSEL,
        settings.windowMode, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_RESOLUTION, CB_SETCURSEL,
        settings.resolutionIndex, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_RENDER_RESOLUTION, CB_SETCURSEL,
        settings.renderingResolutionScaleIndex, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_ANTI_ALIASING, CB_SETCURSEL,
        settings.antiAliasingIndex, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_POST_PROCESS_AA, CB_SETCURSEL,
        settings.postProcessAntiAliasingMode, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_RENDERING_BACKEND, CB_SETCURSEL,
        ApplicationSettings::ClampRenderingBackend(settings.renderingBackend), 0);
    InvalidateRect(GetDlgItem(state.window, IDC_CONFIG_EDIT_GAME), nullptr, TRUE);
    SendDlgItemMessageA(state.window, IDC_CONFIG_COLLISION, BM_SETCHECK,
        settings.showCollisionGeometry ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_MOVEMENT_STYLE, CB_SETCURSEL,
        ApplicationSettings::ClampMovementStyle(settings.movementStyle), 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_ENABLE_SOUNDS, BM_SETCHECK,
        settings.enableSounds ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_BACKGROUND_SOUNDS, BM_SETCHECK,
        settings.playSoundsInBackground ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_MAX_SOUNDS, CB_SETCURSEL,
        ApplicationSettings::MaxSoundOptionIndex(settings.maxSimultaneousSounds), 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_HARDWARE_CURSOR, BM_SETCHECK,
        settings.enableHardwareMouseCursor ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_CURSOR_STYLE, CB_SETCURSEL,
        std::clamp(settings.mouseCursorStyle, 0, 3), 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_MIRROR_WORLD, BM_SETCHECK,
        settings.mirrorWorldZones ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_DRAW_DISTANCE, CB_SETCURSEL,
        settings.drawDistanceIndex, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_TEXTURE_COMPRESSION, CB_SETCURSEL,
        settings.enableTextureCompression ? 0 : 1, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_MAP_TEXTURE_COMPRESSION, BM_SETCHECK,
        settings.enableMapTextureCompression ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_WEATHER_EFFECTS, BM_SETCHECK,
        settings.enableWeatherEffects ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_COLOR_THEME, CB_SETCURSEL,
        state.pendingDarkTheme ? ApplicationSettings::DarkTheme : ApplicationSettings::LightTheme, 0);
    for (int index = 0; index < kBindingCount; ++index)
    {
        if (state.capturingBinding == index)
            continue;
        char keyText[64] = {};
        SetDlgItemTextA(state.window, IDC_CONFIG_KEY_FORWARD + index,
            KeyName(*BindingValue(*state.settings, index), keyText));
    }
}

void ApplyTheme(State& state)
{
    if (state.window && IsWindow(state.window) && state.theme)
        Win32Theme::ApplyWindowTheme(state.window, *state.theme);
}

HWND Window(const State& state)
{
    return state.window && IsWindow(state.window) ? state.window : NULL;
}
}

