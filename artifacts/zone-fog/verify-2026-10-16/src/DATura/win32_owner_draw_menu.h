#pragma once

#include <windows.h>

namespace Win32OwnerDrawMenu
{
void StyleMenuBar(HMENU menuBar);
void MeasureItem(HWND owner, MEASUREITEMSTRUCT* measure, HFONT font);
void DrawItem(const DRAWITEMSTRUCT* draw, bool dark, HFONT font);
}
