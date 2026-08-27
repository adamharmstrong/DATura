#include "stdafx.h"
#include "win32_drawing.h"

#include <algorithm>

namespace Win32Drawing
{
void DrawShadowText(const HDC dc, const HFONT font, const char* text, RECT bounds,
                    const UINT format, const COLORREF color, const int shadowOffset)
{
    HFONT previousFont = (HFONT)SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);

    RECT shadowBounds = bounds;
    OffsetRect(&shadowBounds, shadowOffset, shadowOffset);
    SetTextColor(dc, RGB(0, 0, 0));
    DrawTextA(dc, text, -1, &shadowBounds, format);

    SetTextColor(dc, color);
    DrawTextA(dc, text, -1, &bounds, format);

    SelectObject(dc, previousFont);
}

void DrawTitleMenuButton(const HDC dc, const RECT bounds, const char* label, const bool selected)
{
    HBRUSH fill = CreateSolidBrush(selected ? RGB(136, 78, 35) : RGB(65, 64, 128));
    HBRUSH previousBrush = static_cast<HBRUSH>(SelectObject(dc, fill));
    HPEN pen = CreatePen(PS_SOLID, 2, selected ? RGB(235, 210, 132) : RGB(214, 214, 245));
    HPEN previousPen = static_cast<HPEN>(SelectObject(dc, pen));
    RoundRect(dc, bounds.left, bounds.top, bounds.right, bounds.bottom, 24, 24);
    SelectObject(dc, previousPen);
    SelectObject(dc, previousBrush);
    DeleteObject(pen);
    DeleteObject(fill);

    const int fontHeight = std::max<int>(18, static_cast<int>(bounds.bottom - bounds.top) - 8);
    HFONT font = CreateFontA(-fontHeight, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, "Arial");
    DrawShadowText(dc, font, label, bounds, DT_CENTER | DT_VCENTER | DT_SINGLELINE,
                   RGB(255, 255, 255), 2);
    DeleteObject(font);
}

void DrawExpansionPlaque(const HDC dc, const RECT bounds, const char* label, const COLORREF color)
{
    HBRUSH fill = CreateSolidBrush(RGB(226, 230, 226));
    HBRUSH previousBrush = static_cast<HBRUSH>(SelectObject(dc, fill));
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(205, 210, 205));
    HPEN previousPen = static_cast<HPEN>(SelectObject(dc, pen));
    Rectangle(dc, bounds.left, bounds.top, bounds.right, bounds.bottom);
    SelectObject(dc, previousPen);
    SelectObject(dc, previousBrush);
    DeleteObject(pen);
    DeleteObject(fill);

    const int fontHeight = std::max<int>(15, static_cast<int>(bounds.bottom - bounds.top) - 12);
    HFONT font = CreateFontA(-fontHeight, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_ROMAN, "Georgia");
    DrawShadowText(dc, font, label, bounds, DT_CENTER | DT_VCENTER | DT_SINGLELINE,
                   color, 1);
    DeleteObject(font);
}
}
