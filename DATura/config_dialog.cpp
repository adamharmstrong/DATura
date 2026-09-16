#include "stdafx.h"
#include "config_dialog.h"
#include "ffxi_player_icons.h"
#include "ffxi_stat_system.h"
#include <commctrl.h>
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
    IDC_CONFIG_ZONE_OBJECTS = 8310,
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
    IDC_CONFIG_TAB_NAMEPLATES = 8346
};

constexpr LONG_PTR kNoConfigTab = 0;
constexpr LONG_PTR kMenuTab = 1;
constexpr LONG_PTR kChatTab = 2;
constexpr LONG_PTR kNameplatesTab = 3;

ConfigDialog::State* GetState(const HWND window)
{
    return reinterpret_cast<ConfigDialog::State*>(
        GetWindowLongPtrA(window, GWLP_USERDATA));
}

void Notify(ConfigDialog::State& state, const ConfigDialog::Command command, const int value = 0)
{
    if (state.eventCallback)
        state.eventCallback(state.callbackContext, { command, value });
    ConfigDialog::Sync(state);
}

BOOL CALLBACK ShowConfigTabChild(HWND child, LPARAM activeTab)
{
    const LONG_PTR tab = GetWindowLongPtrA(child, GWLP_USERDATA);
    if (tab >= kMenuTab && tab <= kNameplatesTab)
        ShowWindow(child, tab == activeTab ? SW_SHOW : SW_HIDE);
    return TRUE;
}

void ApplyActiveTab(ConfigDialog::State& state)
{
    if (!state.window)
        return;
    const LONG_PTR activeTab = static_cast<LONG_PTR>(std::clamp(state.activeTab, 0, 2) + 1);
    EnumChildWindows(state.window, ShowConfigTabChild, activeTab);
    InvalidateRect(state.window, nullptr, TRUE);
}

BOOL CALLBACK TagInterfaceTabChild(HWND child, LPARAM)
{
    const int id = GetDlgCtrlID(child);
    if (id == IDC_CONFIG_CLOSE || id == IDC_CONFIG_TAB_MENU ||
        id == IDC_CONFIG_TAB_CHAT || id == IDC_CONFIG_TAB_NAMEPLATES)
        return TRUE;

    HWND parent = GetParent(child);
    RECT rect = {};
    GetWindowRect(child, &rect);
    MapWindowPoints(HWND_DESKTOP, parent, reinterpret_cast<POINT*>(&rect), 2);
    if (rect.top < 548)
        return TRUE;

    LONG_PTR tab = kNameplatesTab;
    if (id == IDC_CONFIG_COLOR_THEME)
        tab = kMenuTab;
    else if (id == IDC_CONFIG_CHAT_TIMEOUT || id == IDC_CONFIG_CHAT_WIDTH ||
             id == IDC_CONFIG_CHAT_HEIGHT)
        tab = kChatTab;
    else if (id == IDC_CONFIG_PLAYER_ICON || id == IDC_CONFIG_JOB_MASTER ||
             id == IDC_CONFIG_SUBTITLE || id == IDC_CONFIG_LINKSHELL_NAME ||
             id == IDC_CONFIG_MAIN_JOB || id == IDC_CONFIG_SUB_JOB ||
             id == IDC_CONFIG_MAIN_LEVEL || id == IDC_CONFIG_SUB_LEVEL ||
             id == IDC_CONFIG_MAIN_LV_LABEL || id == IDC_CONFIG_SUB_LV_LABEL ||
             id == IDC_CONFIG_JOB_SEPARATOR || id == IDC_CONFIG_SUBTITLE_LABEL ||
             id == IDC_CONFIG_LINKSHELL_COLOR_R || id == IDC_CONFIG_LINKSHELL_COLOR_G ||
             id == IDC_CONFIG_LINKSHELL_COLOR_B)
        tab = kNameplatesTab;
    else if (id == 0)
    {
        char text[64] = {};
        GetWindowTextA(child, text, sizeof(text));
        if (strcmp(text, "Color Theme") == 0)
            tab = kMenuTab;
        else if (strcmp(text, "Display duration") == 0 || strcmp(text, "Width (%)") == 0 ||
                 strcmp(text, "Height (%)") == 0)
            tab = kChatTab;
        else
            tab = kNameplatesTab;
    }
    SetWindowLongPtrA(child, GWLP_USERDATA, tab);
    return TRUE;
}

void CreateControls(ConfigDialog::State& state)
{
    const HWND window = state.window;
    const HFONT font = state.theme->resources.font;
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

    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Window Mode", 0, 0, 24, 132, 100, 18);
    const HWND windowMode = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_WINDOW_MODE, 142, 128, 170, 120);
    SendMessageA(windowMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Windowed"));
    SendMessageA(windowMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Borderless Window"));
    SendMessageA(windowMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Full Screen"));

    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Resolution", 0, 0, 24, 162, 100, 18);
    const HWND resolution = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_RESOLUTION, 142, 158, 170, 180);
    for (int i = 0; i < ApplicationSettings::ResolutionOptionCount(); ++i)
    {
        SendMessageA(resolution, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(ApplicationSettings::ResolutionOptionAt(i).label));
    }

    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Rendering Backend", 0, 0, 428, 132, 140, 22);
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
        window, "STATIC", "Vegetation Animation", 0, 0, 24, 278, 146, 22);
    const HWND environmentAnimation = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_ENV_ANIM, 188, 274, 178, 120);
    SendMessageA(environmentAnimation, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Off"));
    SendMessageA(environmentAnimation, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Simple"));
    SendMessageA(environmentAnimation, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Smooth"));
    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Lighting / Shadows", 0, 0, 24, 312, 146, 22);
    const HWND lightingQuality = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_LIGHTING_QUALITY, 188, 308, 178, 100);
    SendMessageA(lightingQuality, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Off"));
    SendMessageA(lightingQuality, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Simplified"));
    SendMessageA(lightingQuality, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Dynamic Shadows"));
    Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Mirror world zones", BS_AUTOCHECKBOX,
        IDC_CONFIG_MIRROR_WORLD, 428, 224, 220, 22);
    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Texture Storage", 0, 0, 428, 256, 120, 22);
    const HWND textureCompression = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_TEXTURE_COMPRESSION, 574, 252, 158, 100);
    SendMessageA(textureCompression, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Compressed"));
    SendMessageA(textureCompression, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Uncompressed"));
    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Draw Distance", 0, 0, 428, 290, 120, 22);
    const HWND drawDistance = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_DRAW_DISTANCE, 574, 286, 158, 160);
    for (int i = 0; i < ApplicationSettings::DrawDistanceOptionCount(); ++i)
    {
        SendMessageA(drawDistance, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(ApplicationSettings::DrawDistanceOptionLabel(i)));
    }

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

    Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Game Mode", BS_AUTOCHECKBOX,
        IDC_CONFIG_EDIT_GAME, 24, 468, 160, 22);
    Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Show collision mesh", BS_AUTOCHECKBOX,
        IDC_CONFIG_COLLISION, 24, 492, 190, 22);
    Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Zone Objects...", BS_OWNERDRAW,
        IDC_CONFIG_ZONE_OBJECTS, 428, 464, 150, 28);

    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Door Interaction", 0, 0, 428, 499, 120, 18);
    const HWND doorInteraction = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_DOOR_INTERACTION, 574, 495, 158, 100);
    SendMessageA(doorInteraction, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Classic"));
    SendMessageA(doorInteraction, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Physics"));

    Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Menu", BS_OWNERDRAW,
        IDC_CONFIG_TAB_MENU, 28, 558, 110, 26);
    Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Chat Log", BS_OWNERDRAW,
        IDC_CONFIG_TAB_CHAT, 146, 558, 110, 26);
    Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Nameplates", BS_OWNERDRAW,
        IDC_CONFIG_TAB_NAMEPLATES, 264, 558, 110, 26);

    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Color Theme", 0, 0, 28, 606, 100, 22);
    const HWND colorTheme = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_COLOR_THEME, 142, 601, 180, 96);
    SendMessageA(colorTheme, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Dark"));
    SendMessageA(colorTheme, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Light"));

    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Player icon", 0, 0, 28, 606, 100, 22);
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

    Win32PanelControls::AddPanelControl(window, "STATIC", "Subtitle", 0, 0, 28, 640, 100, 22);
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

    Win32PanelControls::AddPanelControl(window, "STATIC", "Linkshell icon color", 0, 0,
        560, 640, 160, 22);
    Win32PanelControls::AddPanelControl(window, "STATIC", "R", 0, 0, 560, 674, 12, 22);
    Win32PanelControls::AddPanelControl(window, "STATIC", "G", 0, 0, 616, 674, 12, 22);
    Win32PanelControls::AddPanelControl(window, "STATIC", "B", 0, 0, 672, 674, 12, 22);
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
        window, "BUTTON", "Close", BS_OWNERDRAW,
        IDC_CONFIG_CLOSE, 620, 732, 112, 30);

    Win32PanelControls::AddPanelControl(window, "STATIC", "Display duration", 0, 0, 28, 606, 120, 22);
    const HWND timeout = Win32PanelControls::AddPanelControl(window, "EDIT", "15",
        ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP, IDC_CONFIG_CHAT_TIMEOUT, 158, 601, 56, 24);
    SendMessageA(timeout, EM_SETLIMITTEXT, 3, 0);

    Win32PanelControls::AddPanelControl(window, "STATIC", "Width (%)", 0, 0, 250, 606, 68, 22);
    Win32PanelControls::AddPanelControl(window, "EDIT", "50", ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP, IDC_CONFIG_CHAT_WIDTH, 328, 601, 50, 24);
    Win32PanelControls::AddPanelControl(window, "STATIC", "Height (%)", 0, 0, 414, 606, 70, 22);
    Win32PanelControls::AddPanelControl(window, "EDIT", "25", ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP, IDC_CONFIG_CHAT_HEIGHT, 494, 601, 50, 24);

    const HWND children[] =
    {
        path,
        timeout,
        GetDlgItem(window, IDC_CONFIG_CHAT_WIDTH),
        GetDlgItem(window, IDC_CONFIG_CHAT_HEIGHT),
        GetDlgItem(window, IDC_CONFIG_SET_PATH),
        GetDlgItem(window, IDC_CONFIG_DETECT_PATH),
        GetDlgItem(window, IDC_CONFIG_RESET_PATH),
        GetDlgItem(window, IDC_CONFIG_SHOW_PATH),
        GetDlgItem(window, IDC_CONFIG_TAB_MENU),
        GetDlgItem(window, IDC_CONFIG_TAB_CHAT),
        GetDlgItem(window, IDC_CONFIG_TAB_NAMEPLATES),
        GetDlgItem(window, IDC_CONFIG_WINDOW_MODE),
        GetDlgItem(window, IDC_CONFIG_RESOLUTION),
        GetDlgItem(window, IDC_CONFIG_RENDERING_BACKEND),
        GetDlgItem(window, IDC_CONFIG_MIP_MAPPING),
        GetDlgItem(window, IDC_CONFIG_BUMP_MAPPING),
        GetDlgItem(window, IDC_CONFIG_LIGHTING_QUALITY),
        GetDlgItem(window, IDC_CONFIG_DOOR_INTERACTION),
        GetDlgItem(window, IDC_CONFIG_ENV_ANIM),
        GetDlgItem(window, IDC_CONFIG_MIRROR_WORLD),
        GetDlgItem(window, IDC_CONFIG_TEXTURE_COMPRESSION),
        GetDlgItem(window, IDC_CONFIG_DRAW_DISTANCE),
        GetDlgItem(window, IDC_CONFIG_ENABLE_SOUNDS),
        GetDlgItem(window, IDC_CONFIG_BACKGROUND_SOUNDS),
        GetDlgItem(window, IDC_CONFIG_MAX_SOUNDS),
        GetDlgItem(window, IDC_CONFIG_HARDWARE_CURSOR),
        GetDlgItem(window, IDC_CONFIG_EDIT_GAME),
        GetDlgItem(window, IDC_CONFIG_COLLISION),
        GetDlgItem(window, IDC_CONFIG_ZONE_OBJECTS),
        GetDlgItem(window, IDC_CONFIG_COLOR_THEME),
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
        return 0;

    case WM_COMMAND:
        if (!state || !state->settings)
            break;
        switch (LOWORD(wParam))
        {
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
        case IDC_CONFIG_EDIT_GAME:
            Notify(*state, ConfigDialog::Command::ToggleGameMode);
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
        case IDC_CONFIG_COLOR_THEME:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                Notify(*state, ConfigDialog::Command::ColorThemeChanged, static_cast<int>(
                    SendDlgItemMessageA(window, IDC_CONFIG_COLOR_THEME, CB_GETCURSEL, 0, 0)));
            }
            return 0;
        case IDC_CONFIG_TAB_MENU:
        case IDC_CONFIG_TAB_CHAT:
        case IDC_CONFIG_TAB_NAMEPLATES:
            state->activeTab = LOWORD(wParam) == IDC_CONFIG_TAB_CHAT ? 1 :
                (LOWORD(wParam) == IDC_CONFIG_TAB_NAMEPLATES ? 2 : 0);
            ApplyActiveTab(*state);
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
            if (HIWORD(wParam) == EN_KILLFOCUS && state->gameUi)
            {
                BOOL valid = FALSE;
                const int id = LOWORD(wParam);
                const UINT value = GetDlgItemInt(window, id, &valid, FALSE);
                if (valid && value <= 2147483647U)
                {
                    if (id == IDC_CONFIG_CHAT_WIDTH) state->gameUi->chatLogWidthPercent = std::clamp((int)value, 20, 100);
                    else if (id == IDC_CONFIG_CHAT_HEIGHT) state->gameUi->chatLogHeightPercent = std::clamp((int)value, 10, 75);
                    else state->gameUi->chatLogTimeoutSeconds = std::clamp((int)value, 1, 300);
                }
                Notify(*state, ConfigDialog::Command::ChatLogChanged);
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
        case IDC_CONFIG_ZONE_OBJECTS:
            Notify(*state, ConfigDialog::Command::ShowZoneObjects);
            return 0;
        case IDC_CONFIG_CLOSE:
            DestroyWindow(window);
            return 0;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(window);
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
            Win32Theme::DrawButton(draw, state->theme->dark, state->theme->resources.font,
                draw && (draw->CtlID == IDC_CONFIG_CLOSE ||
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
            const RECT panels[] =
            {
                { 14,   8, 746, 100 },
                { 14, 106, 746, 194 },
                { 14, 200, 746, 338 },
                { 14, 346, 746, 434 },
                { 14, 442, 746, 530 },
                { 14, 536, 746, 724 },
            };
            const char* titles[] =
            {
                "FFXI Installation", "Display", "Rendering",
                "Audio / Input", "Mode / Debug", "Interface"
            };
            for (int i = 0; i < static_cast<int>(sizeof(panels) / sizeof(panels[0])); ++i)
            {
                Win32Theme::DrawPanel(deviceContext, panels[i], titles[i], state->theme->dark,
                    state->theme->resources.sectionFont);
            }
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
        if (state && state->window == window)
            state->window = NULL;
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
    state.theme = &theme;
    state.gameUi = &gameUi;
    state.eventCallback = eventCallback;
    state.isGameModeCallback = isGameModeCallback;
    state.callbackContext = callbackContext;
}

void Show(State& state)
{
    if (state.window && IsWindow(state.window))
    {
        ApplyActiveTab(state);
        Sync(state);
        Win32ToolWindow::Show(state.window, true);
        return;
    }
    if (!state.owner || !state.settings || !state.theme)
        return;

    RECT ownerRect = {};
    GetWindowRect(state.owner, &ownerRect);
    const int width = 780;
    const int height = 806;
    Win32ToolWindow::Spec spec =
    {
        WindowProcedure, kWindowClassName, "DATura Config", width, height,
        WS_CAPTION | WS_SYSMENU | WS_POPUP | WS_VISIBLE,
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
        const auto* icon = FFXIPlayerIcons::Find(state.gameUi->playerNameplate.icon);
        const int selected = icon ? static_cast<int>(icon - FFXIPlayerIcons::Catalog) + 1 : 0;
        SendDlgItemMessageA(state.window, IDC_CONFIG_PLAYER_ICON, CB_SETCURSEL, selected, 0);
        SendDlgItemMessageA(state.window, IDC_CONFIG_JOB_MASTER, BM_SETCHECK,
            state.gameUi->playerNameplate.jobMaster ? BST_CHECKED : BST_UNCHECKED, 0);
        const auto& player = state.gameUi->playerNameplate;
        SendDlgItemMessageA(state.window, IDC_CONFIG_SUBTITLE, CB_SETCURSEL, static_cast<int>(player.subtitleMode), 0);
        const bool linkshell = player.subtitleMode == PlayerSubtitleMode::Linkshell;
        const bool jobs = player.subtitleMode == PlayerSubtitleMode::Jobs;
        SetDlgItemTextA(state.window, IDC_CONFIG_SUBTITLE_LABEL, jobs ? "Jobs / levels" : "Linkshell");
        ShowWindow(GetDlgItem(state.window, IDC_CONFIG_SUBTITLE_LABEL), linkshell || jobs ? SW_SHOW : SW_HIDE);
        ShowWindow(GetDlgItem(state.window, IDC_CONFIG_LINKSHELL_NAME), linkshell ? SW_SHOW : SW_HIDE);
        SetDlgItemTextA(state.window, IDC_CONFIG_LINKSHELL_NAME, player.linkshellName);
        for (const int id : {IDC_CONFIG_MAIN_JOB, IDC_CONFIG_SUB_JOB, IDC_CONFIG_MAIN_LEVEL,
            IDC_CONFIG_SUB_LEVEL, IDC_CONFIG_MAIN_LV_LABEL, IDC_CONFIG_SUB_LV_LABEL, IDC_CONFIG_JOB_SEPARATOR})
            ShowWindow(GetDlgItem(state.window, id), jobs ? SW_SHOW : SW_HIDE);
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
    SendDlgItemMessageA(state.window, IDC_CONFIG_DOOR_INTERACTION, CB_SETCURSEL,
        settings.doorInteractionMode == ApplicationSettings::DoorPhysics ? 1 : 0, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_LIGHTING_QUALITY, CB_SETCURSEL,
        ApplicationSettings::ClampLightingQuality(settings.lightingQuality), 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_ENV_ANIM, CB_SETCURSEL,
        settings.environmentalAnimationMode, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_WINDOW_MODE, CB_SETCURSEL,
        settings.windowMode, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_RESOLUTION, CB_SETCURSEL,
        settings.resolutionIndex, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_RENDERING_BACKEND, CB_SETCURSEL,
        ApplicationSettings::ClampRenderingBackend(settings.renderingBackend), 0);
    const bool gameMode = state.isGameModeCallback &&
        state.isGameModeCallback(state.callbackContext);
    SendDlgItemMessageA(state.window, IDC_CONFIG_EDIT_GAME, BM_SETCHECK,
        gameMode ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_COLLISION, BM_SETCHECK,
        settings.showCollisionGeometry ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_ENABLE_SOUNDS, BM_SETCHECK,
        settings.enableSounds ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_BACKGROUND_SOUNDS, BM_SETCHECK,
        settings.playSoundsInBackground ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_MAX_SOUNDS, CB_SETCURSEL,
        ApplicationSettings::MaxSoundOptionIndex(settings.maxSimultaneousSounds), 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_HARDWARE_CURSOR, BM_SETCHECK,
        settings.enableHardwareMouseCursor ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_MIRROR_WORLD, BM_SETCHECK,
        settings.mirrorWorldZones ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_DRAW_DISTANCE, CB_SETCURSEL,
        settings.drawDistanceIndex, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_TEXTURE_COMPRESSION, CB_SETCURSEL,
        settings.enableTextureCompression ? 0 : 1, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_COLOR_THEME, CB_SETCURSEL,
        state.theme->dark ? ApplicationSettings::DarkTheme : ApplicationSettings::LightTheme, 0);
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

