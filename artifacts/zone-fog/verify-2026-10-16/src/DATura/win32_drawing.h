#pragma once

#include <windows.h>

namespace Win32Drawing
{
void DrawShadowText(HDC dc, HFONT font, const char* text, RECT bounds,
                    UINT format, COLORREF color, int shadowOffset);
void DrawTitleMenuButton(HDC dc, RECT bounds, const char* label, bool selected);
void DrawExpansionPlaque(HDC dc, RECT bounds, const char* label, COLORREF color);
}
