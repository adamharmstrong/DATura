#include "stdafx.h"
#include "config_dialog.h"

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
    IDC_CONFIG_LIGHTING_QUALITY = 8323
};

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

void CreateControls(ConfigDialog::State& state)
{
    const HWND window = state.window;
    const HFONT font = state.theme->resources.font;
    const HWND path = Win32PanelControls::AddPanelControl(
        window, "EDIT", "", ES_AUTOHSCROLL | ES_READONLY, IDC_CONFIG_PATH_TEXT,
        28, 31, 564, 24, WS_EX_CLIENTEDGE);

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
        window, "BUTTON", "Enable MIP Mapping", BS_AUTOCHECKBOX,
        IDC_CONFIG_MIP_MAPPING, 24, 224, 220, 22);
    Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Enable Bump Mapping", BS_AUTOCHECKBOX,
        IDC_CONFIG_BUMP_MAPPING, 24, 248, 220, 22);
    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Lighting / Shadows", 0, 0, 24, 304, 146, 18);
    const HWND lightingQuality = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_LIGHTING_QUALITY, 174, 300, 134, 100);
    SendMessageA(lightingQuality, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Off"));
    SendMessageA(lightingQuality, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Simplified"));
    SendMessageA(lightingQuality, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Dynamic Shadows"));
    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Vegetation Animation", 0, 0, 24, 278, 146, 18);
    const HWND environmentAnimation = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_ENV_ANIM, 174, 274, 134, 120);
    SendMessageA(environmentAnimation, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Off"));
    SendMessageA(environmentAnimation, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Simple"));
    SendMessageA(environmentAnimation, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Smooth"));
    Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Mirror world zones", BS_AUTOCHECKBOX,
        IDC_CONFIG_MIRROR_WORLD, 328, 224, 220, 22);
    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Texture Storage", 0, 0, 328, 252, 120, 18);
    const HWND textureCompression = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_TEXTURE_COMPRESSION, 454, 248, 138, 100);
    SendMessageA(textureCompression, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Compressed"));
    SendMessageA(textureCompression, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Uncompressed"));
    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Draw Distance", 0, 0, 328, 278, 120, 18);
    const HWND drawDistance = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_DRAW_DISTANCE, 454, 274, 138, 160);
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
        window, "STATIC", "Simultaneous SFX", 0, 0, 328, 372, 120, 18);
    const HWND maxSounds = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_MAX_SOUNDS, 454, 368, 138, 160);
    for (int i = 0; i < ApplicationSettings::MaxSoundOptionCount(); ++i)
    {
        SendMessageA(maxSounds, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(ApplicationSettings::MaxSoundOptionLabel(i)));
    }
    Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Enable Hardware Mouse Cursor", BS_AUTOCHECKBOX,
        IDC_CONFIG_HARDWARE_CURSOR, 328, 396, 264, 22);

    Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Game Mode", BS_AUTOCHECKBOX,
        IDC_CONFIG_EDIT_GAME, 24, 468, 160, 22);
    Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Show collision mesh", BS_AUTOCHECKBOX,
        IDC_CONFIG_COLLISION, 24, 492, 190, 22);
    Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Zone Objects...", BS_OWNERDRAW,
        IDC_CONFIG_ZONE_OBJECTS, 328, 464, 150, 28);

    Win32PanelControls::AddPanelControl(
        window, "STATIC", "Color Theme", 0, 0, 28, 562, 100, 18);
    const HWND colorTheme = Win32PanelControls::AddPanelCombo(
        window, IDC_CONFIG_COLOR_THEME, 142, 557, 170, 96);
    SendMessageA(colorTheme, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Dark"));
    SendMessageA(colorTheme, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Light"));

    Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Close", BS_OWNERDRAW,
        IDC_CONFIG_CLOSE, 480, 604, 112, 30);

    const HWND children[] =
    {
        path,
        GetDlgItem(window, IDC_CONFIG_SET_PATH),
        GetDlgItem(window, IDC_CONFIG_DETECT_PATH),
        GetDlgItem(window, IDC_CONFIG_RESET_PATH),
        GetDlgItem(window, IDC_CONFIG_SHOW_PATH),
        GetDlgItem(window, IDC_CONFIG_WINDOW_MODE),
        GetDlgItem(window, IDC_CONFIG_RESOLUTION),
        GetDlgItem(window, IDC_CONFIG_MIP_MAPPING),
        GetDlgItem(window, IDC_CONFIG_BUMP_MAPPING),
        GetDlgItem(window, IDC_CONFIG_LIGHTING_QUALITY),
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
        GetDlgItem(window, IDC_CONFIG_CLOSE)
    };
    for (const HWND child : children)
    {
        if (child)
            SendMessageA(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
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
            Win32Theme::DrawButton(draw, state->theme->dark, state->theme->resources.font,
                draw && draw->CtlID == IDC_CONFIG_CLOSE);
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
                { 14,   8, 606, 100 },
                { 14, 106, 606, 194 },
                { 14, 200, 606, 338 },
                { 14, 346, 606, 434 },
                { 14, 442, 606, 530 },
                { 14, 536, 606, 594 }
            };
            const char* titles[] =
            {
                "FFXI Installation", "Display", "Rendering",
                "Audio / Input", "Mode / Debug", "Appearance"
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
    const EventCallback eventCallback,
    const IsGameModeCallback isGameModeCallback,
    void* const callbackContext)
{
    state.owner = owner;
    state.ffxiPath = ffxiPath;
    state.settings = &settings;
    state.theme = &theme;
    state.eventCallback = eventCallback;
    state.isGameModeCallback = isGameModeCallback;
    state.callbackContext = callbackContext;
}

void Show(State& state)
{
    if (state.window && IsWindow(state.window))
    {
        Sync(state);
        Win32ToolWindow::Show(state.window, true);
        return;
    }
    if (!state.owner || !state.settings || !state.theme)
        return;

    RECT ownerRect = {};
    GetWindowRect(state.owner, &ownerRect);
    const int width = 640;
    const int height = 680;
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
        Win32ToolWindow::Show(state.window, true);
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
    SetDlgItemTextA(state.window, IDC_CONFIG_PATH_TEXT, state.ffxiPath ? state.ffxiPath : "");
    SendDlgItemMessageA(state.window, IDC_CONFIG_MIP_MAPPING, BM_SETCHECK,
        settings.enableMipMapping ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_BUMP_MAPPING, BM_SETCHECK,
        settings.enableBumpMapping ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_LIGHTING_QUALITY, CB_SETCURSEL,
        ApplicationSettings::ClampLightingQuality(settings.lightingQuality), 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_ENV_ANIM, CB_SETCURSEL,
        settings.environmentalAnimationMode, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_WINDOW_MODE, CB_SETCURSEL,
        settings.windowMode, 0);
    SendDlgItemMessageA(state.window, IDC_CONFIG_RESOLUTION, CB_SETCURSEL,
        settings.resolutionIndex, 0);
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
