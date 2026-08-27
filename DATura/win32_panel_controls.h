#pragma once

#include <windows.h>

namespace Win32PanelControls
{
HWND AddPanelControl(HWND parent, const char *className, const char *text,
                     DWORD style, int id, int x, int y, int width, int height,
                     DWORD extendedStyle = 0, bool visible = true);
HWND AddPanelControlW(HWND parent, const wchar_t *className, const wchar_t *text,
                      DWORD style, int id, int x, int y, int width, int height,
                      DWORD extendedStyle = 0, bool visible = true);
HWND AddPanelCombo(HWND parent, int id, int x, int y, int width, int height = 220);
}
