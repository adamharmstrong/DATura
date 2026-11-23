#include "stdafx.h"
#include "win32_panel_controls.h"

namespace Win32PanelControls
{
HWND AddPanelControl(HWND parent, const char *className, const char *text,
                     DWORD style, int id, int x, int y, int width, int height,
                     DWORD extendedStyle, bool visible)
{
    return CreateWindowExA(extendedStyle, className, text,
        WS_CHILD | (visible ? WS_VISIBLE : 0) | style, x, y, width, height, parent,
        (HMENU)(INT_PTR)id, GetModuleHandle(NULL), NULL);
}

HWND AddPanelControlW(HWND parent, const wchar_t *className, const wchar_t *text,
                      DWORD style, int id, int x, int y, int width, int height,
                      DWORD extendedStyle, bool visible)
{
    return CreateWindowExW(extendedStyle, className, text,
        WS_CHILD | (visible ? WS_VISIBLE : 0) | style, x, y, width, height, parent,
        (HMENU)(INT_PTR)id, GetModuleHandle(NULL), NULL);
}

HWND AddPanelCombo(HWND parent, int id, int x, int y, int width, int height)
{
    return AddPanelControl(parent, "COMBOBOX", "",
        CBS_DROPDOWNLIST | WS_VSCROLL, id, x, y, width, height);
}
}
