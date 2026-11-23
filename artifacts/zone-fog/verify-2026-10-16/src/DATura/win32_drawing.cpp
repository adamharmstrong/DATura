#include "stdafx.h"
#include "win32_drawing.h"
#include "ffxi_bitmap_font.h"

#include <algorithm>

namespace Win32Drawing
{
void DrawShadowText(const HDC dc, const HFONT font, const char* text, RECT bounds,
                    const UINT format, const COLORREF color, const int shadowOffset)
{
    const std::string narrow = text ? text : "";
    const std::wstring wide(narrow.begin(), narrow.end());
    if (FFXIBitmapFont::Supports(wide))
    {
        LOGFONTA description = {};
        GetObjectA(font, sizeof(description), &description);
        const int height = std::max<int>(1, std::abs(description.lfHeight));
        std::vector<std::wstring> lines(1);
        for (size_t i = 0; i < wide.size();)
        {
            if (wide[i] == '\r') { ++i; continue; }
            if (wide[i] == '\n') { lines.emplace_back(); ++i; continue; }
            size_t end = i + 1;
            while (end < wide.size() && wide[end] != ' ' && wide[end] != '\n' && wide[end] != '\r') ++end;
            std::wstring word = wide.substr(i, end - i);
            if ((format & DT_WORDBREAK) && !lines.back().empty() &&
                FFXIBitmapFont::Measure(lines.back() + word, height).cx > bounds.right - bounds.left)
            {
                lines.emplace_back();
                word.erase(0, word.find_first_not_of(L' '));
            }
            lines.back() += word;
            i = end;
        }
        int y = bounds.top;
        if (format & DT_VCENTER) y += (bounds.bottom - bounds.top - int(lines.size()) * height) / 2;
        const int saved = SaveDC(dc);
        IntersectClipRect(dc, bounds.left, bounds.top, bounds.right, bounds.bottom);
        for (const auto& line : lines)
        {
            const int width = FFXIBitmapFont::Measure(line, height).cx;
            int x = bounds.left;
            if (format & DT_CENTER) x += (bounds.right - bounds.left - width) / 2;
            else if (format & DT_RIGHT) x = bounds.right - width;
            FFXIBitmapFont::Draw(dc, line, x + shadowOffset, y + shadowOffset, height, RGB(0, 0, 0));
            FFXIBitmapFont::Draw(dc, line, x, y, height, color);
            y += height;
        }
        if (saved) RestoreDC(dc, saved);
        return;
    }
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
