#include "stdafx.h"
#include "win32_theme.h"

#include <uxtheme.h>

#pragma comment(lib, "uxtheme.lib")

namespace
{
typedef HRESULT (WINAPI *DwmSetWindowAttributeFn)(HWND, DWORD, LPCVOID, DWORD);
typedef int (WINAPI *SetPreferredAppModeFn)(int);
typedef void (WINAPI *FlushMenuThemesFn)();

struct ChildThemeData
{
    bool dark;
    HFONT font;
    HFONT sectionFont;
};

BOOL CALLBACK ApplyThemeToChild(HWND hChild, LPARAM lParam)
{
    const ChildThemeData* data = reinterpret_cast<const ChildThemeData*>(lParam);
    if (!data)
        return TRUE;

    wchar_t className[32] = {};
    GetClassNameW(hChild, className, (int)(sizeof(className) / sizeof(className[0])));
    const bool isComboBox = _wcsicmp(className, L"ComboBox") == 0;
    const wchar_t* subApp = data->dark
        ? (isComboBox ? L"DarkMode_CFD" : L"DarkMode_Explorer")
        : L"Explorer";
    SetWindowTheme(hChild, subApp, NULL);
    const LONG_PTR style = GetWindowLongPtr(hChild, GWL_STYLE);
    const bool isGroupBox =
        _wcsicmp(className, L"Button") == 0 &&
        (style & BS_TYPEMASK) == BS_GROUPBOX;
    HFONT font = isGroupBox ? data->sectionFont : data->font;
    if (font)
        SendMessage(hChild, WM_SETFONT, (WPARAM)font, TRUE);
    InvalidateRect(hChild, NULL, TRUE);
    return TRUE;
}
}

namespace Win32Theme
{
COLORREF WindowColor(bool dark)
{
    return dark ? RGB(27, 33, 39) : RGB(244, 246, 248);
}

COLORREF ControlColor(bool dark)
{
    return dark ? RGB(34, 40, 46) : RGB(255, 255, 255);
}

COLORREF EditColor(bool dark)
{
    return dark ? RGB(42, 48, 55) : RGB(255, 255, 255);
}

COLORREF TextColor(bool dark)
{
    return dark ? RGB(226, 231, 236) : RGB(31, 36, 41);
}

COLORREF MutedTextColor(bool dark)
{
    return dark ? RGB(151, 161, 171) : RGB(91, 99, 107);
}

bool LoadDarkModePreference(const char* registryKey, const char* valueName,
                            const bool defaultDark)
{
    HKEY key = NULL;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, registryKey, 0, KEY_READ, &key) != ERROR_SUCCESS)
        return defaultDark;

    DWORD type = 0;
    DWORD value = 0;
    DWORD size = sizeof(value);
    const bool valid = RegQueryValueExA(
        key, valueName, NULL, &type, reinterpret_cast<LPBYTE>(&value), &size) == ERROR_SUCCESS &&
        type == REG_DWORD && value <= 1;
    RegCloseKey(key);

    // Preserve DATura's existing registry representation: 0 = dark, 1 = light.
    return valid ? value == 0 : defaultDark;
}

void SaveDarkModePreference(const char* registryKey, const char* valueName, const bool dark)
{
    HKEY key = NULL;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, registryKey, 0, NULL,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL,
                        &key, NULL) != ERROR_SUCCESS)
    {
        return;
    }

    const DWORD value = dark ? 0 : 1;
    RegSetValueExA(key, valueName, 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&value), sizeof(value));
    RegCloseKey(key);
}

void InitializeState(State& state, const char* registryKey, const char* valueName,
                     const bool defaultDark)
{
    state.dark = LoadDarkModePreference(registryKey, valueName, defaultDark);
    ApplyNativeMenuTheme(state.dark);
    RecreateResources(state.resources, state.dark);
}

void SetDarkMode(State& state, const char* registryKey, const char* valueName, const bool dark)
{
    state.dark = dark;
    SaveDarkModePreference(registryKey, valueName, dark);
    ApplyNativeMenuTheme(dark);
    RecreateResources(state.resources, dark);
}

void ReleaseState(State& state)
{
    ReleaseResources(state.resources);
    state.dark = true;
}

void RecreateResources(Resources& resources, bool dark)
{
    if (resources.windowBrush)
        DeleteObject(resources.windowBrush);
    if (resources.controlBrush)
        DeleteObject(resources.controlBrush);
    if (resources.editBrush)
        DeleteObject(resources.editBrush);
    resources.windowBrush = CreateSolidBrush(WindowColor(dark));
    resources.controlBrush = CreateSolidBrush(ControlColor(dark));
    resources.editBrush = CreateSolidBrush(EditColor(dark));

    if (!resources.font)
    {
        resources.font = CreateFontA(
            -15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    }
    if (!resources.sectionFont)
    {
        resources.sectionFont = CreateFontA(
            -15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    }
}

void ReleaseResources(Resources& resources)
{
    if (resources.windowBrush)
        DeleteObject(resources.windowBrush);
    if (resources.controlBrush)
        DeleteObject(resources.controlBrush);
    if (resources.editBrush)
        DeleteObject(resources.editBrush);
    if (resources.font)
        DeleteObject(resources.font);
    if (resources.sectionFont)
        DeleteObject(resources.sectionFont);
    resources = {};
}

void ApplyNativeMenuTheme(bool dark)
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
        setPreferredAppMode(dark
            ? kPreferredAppModeAllowDark
            : kPreferredAppModeForceLight);
    }
    if (flushMenuThemes)
        flushMenuThemes();

    FreeLibrary(hUxTheme);
}

void ApplyWindowTheme(HWND hWnd, bool dark, Resources& resources)
{
    if (!hWnd)
        return;
    if (!resources.windowBrush)
        RecreateResources(resources, dark);

    SetClassLongPtr(hWnd, GCLP_HBRBACKGROUND, (LONG_PTR)resources.windowBrush);

    HMODULE hDwmApi = LoadLibraryW(L"dwmapi.dll");
    DwmSetWindowAttributeFn setAttribute = hDwmApi
        ? (DwmSetWindowAttributeFn)GetProcAddress(hDwmApi, "DwmSetWindowAttribute")
        : NULL;
    if (setAttribute)
    {
        const BOOL useDark = dark ? TRUE : FALSE;
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

    SetWindowTheme(hWnd, dark ? L"DarkMode_Explorer" : L"Explorer", NULL);
    SendMessage(hWnd, WM_THEMECHANGED, 0, 0);
    const ChildThemeData childData = { dark, resources.font, resources.sectionFont };
    EnumChildWindows(hWnd, ApplyThemeToChild, (LPARAM)&childData);
    InvalidateRect(hWnd, NULL, TRUE);
    DrawMenuBar(hWnd);
}

void ApplyWindowTheme(HWND hWnd, State& state)
{
    ApplyWindowTheme(hWnd, state.dark, state.resources);
}

void DrawPanel(HDC hdc, const RECT& panel, const char* title,
               bool dark, HFONT sectionFont)
{
    HBRUSH fill = CreateSolidBrush(ControlColor(dark));
    HPEN border = CreatePen(PS_SOLID, 1,
        dark ? RGB(57, 65, 73) : RGB(205, 211, 217));
    HGDIOBJ oldBrush = SelectObject(hdc, fill);
    HGDIOBJ oldPen = SelectObject(hdc, border);
    RoundRect(hdc, panel.left, panel.top, panel.right, panel.bottom, 12, 12);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(border);
    DeleteObject(fill);

    RECT titleRect = { panel.left + 14, panel.top + 5, panel.right - 12, panel.top + 24 };
    HFONT oldFont = (HFONT)SelectObject(hdc, sectionFont);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, TextColor(dark));
    DrawTextA(hdc, title, -1, &titleRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    SelectObject(hdc, oldFont);
}

void DrawButton(const DRAWITEMSTRUCT* draw, bool dark, HFONT font, bool accent)
{
    if (!draw)
        return;

    char text[96] = {};
    GetWindowTextA(draw->hwndItem, text, sizeof(text));
    RECT rc = draw->rcItem;
    COLORREF fillColor;
    if (draw->itemState & ODS_DISABLED)
        fillColor = dark ? RGB(43, 48, 53) : RGB(224, 227, 230);
    else if (draw->itemState & ODS_SELECTED)
        fillColor = accent ? RGB(42, 126, 211) :
            (dark ? RGB(64, 73, 82) : RGB(215, 222, 228));
    else
        fillColor = accent ? RGB(62, 151, 245) :
            (dark ? RGB(48, 56, 64) : RGB(231, 235, 239));

    HBRUSH fill = CreateSolidBrush(fillColor);
    HPEN border = CreatePen(PS_SOLID, 1,
        accent ? RGB(104, 180, 255) :
        (dark ? RGB(75, 84, 93) : RGB(190, 198, 205)));
    HGDIOBJ oldBrush = SelectObject(draw->hDC, fill);
    HGDIOBJ oldPen = SelectObject(draw->hDC, border);
    RoundRect(draw->hDC, rc.left, rc.top, rc.right, rc.bottom, 8, 8);
    SelectObject(draw->hDC, oldPen);
    SelectObject(draw->hDC, oldBrush);
    DeleteObject(border);
    DeleteObject(fill);

    HFONT oldFont = (HFONT)SelectObject(draw->hDC, font);
    SetBkMode(draw->hDC, TRANSPARENT);
    SetTextColor(draw->hDC,
        accent && !(draw->itemState & ODS_DISABLED)
            ? RGB(255, 255, 255)
            : TextColor(dark));
    if (draw->itemState & ODS_SELECTED)
        OffsetRect(&rc, 0, 1);
    DrawTextA(draw->hDC, text, -1, &rc,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(draw->hDC, oldFont);
}
}
