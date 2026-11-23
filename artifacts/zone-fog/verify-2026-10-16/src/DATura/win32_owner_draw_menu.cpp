#include "stdafx.h"
#include "win32_owner_draw_menu.h"

#include "win32_theme.h"

#include <memory>
#include <string>
#include <vector>

namespace
{
struct VisualEntry
{
    std::string text;
    bool separator = false;
    bool menuBarItem = false;
};

std::vector<std::unique_ptr<VisualEntry>> g_entries;

void StyleRecursively(HMENU menu, bool menuBar)
{
    if (!menu)
        return;

    const int count = GetMenuItemCount(menu);
    for (int i = 0; i < count; ++i)
    {
        HMENU subMenu = GetSubMenu(menu, i);
        if (subMenu)
            StyleRecursively(subMenu, false);

        MENUITEMINFOA current = {};
        current.cbSize = sizeof(current);
        current.fMask = MIIM_FTYPE;
        if (!GetMenuItemInfoA(menu, i, TRUE, &current))
            continue;

        const int textLength = GetMenuStringA(menu, i, NULL, 0, MF_BYPOSITION);
        std::vector<char> text((size_t)(textLength > 0 ? textLength : 0) + 1, '\0');
        if (textLength > 0)
            GetMenuStringA(menu, i, text.data(), (int)text.size(), MF_BYPOSITION);

        std::unique_ptr<VisualEntry> entry(new VisualEntry());
        entry->text = text.data();
        entry->separator = (current.fType & MFT_SEPARATOR) != 0;
        entry->menuBarItem = menuBar;
        VisualEntry* entryPtr = entry.get();
        g_entries.push_back(std::move(entry));

        MENUITEMINFOA themed = {};
        themed.cbSize = sizeof(themed);
        themed.fMask = MIIM_FTYPE | MIIM_DATA | MIIM_STRING;
        themed.fType = current.fType | MFT_OWNERDRAW;
        themed.dwItemData = (ULONG_PTR)entryPtr;
        themed.dwTypeData = (LPSTR)entryPtr->text.c_str();
        themed.cch = (UINT)entryPtr->text.size();
        SetMenuItemInfoA(menu, i, TRUE, &themed);
    }
}
}

namespace Win32OwnerDrawMenu
{
void StyleMenuBar(HMENU menuBar)
{
    g_entries.clear();
    StyleRecursively(menuBar, true);
}

void MeasureItem(HWND owner, MEASUREITEMSTRUCT* measure, HFONT font)
{
    VisualEntry* entry = measure
        ? (VisualEntry*)measure->itemData
        : NULL;
    if (!entry)
        return;
    if (entry->separator)
    {
        measure->itemWidth = 24;
        measure->itemHeight = 9;
        return;
    }

    HDC hdc = GetDC(owner);
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    const char* tab = strchr(entry->text.c_str(), '\t');
    const int leftLength = tab
        ? (int)(tab - entry->text.c_str())
        : (int)entry->text.size();
    SIZE left = {};
    SIZE right = {};
    GetTextExtentPoint32A(hdc, entry->text.c_str(), leftLength, &left);
    if (tab && tab[1])
        GetTextExtentPoint32A(hdc, tab + 1, (int)strlen(tab + 1), &right);
    SelectObject(hdc, oldFont);
    ReleaseDC(owner, hdc);

    if (entry->menuBarItem)
    {
        measure->itemWidth = left.cx + 22;
        measure->itemHeight = 24;
    }
    else
    {
        measure->itemWidth = left.cx + right.cx + (right.cx ? 44 : 0) + 56;
        measure->itemHeight = 28;
    }
}

void DrawItem(const DRAWITEMSTRUCT* draw, const bool dark, HFONT font)
{
    VisualEntry* entry = draw
        ? (VisualEntry*)draw->itemData
        : NULL;
    if (!entry)
        return;

    const bool selected = (draw->itemState & (ODS_SELECTED | ODS_HOTLIGHT)) != 0;
    const bool disabled = (draw->itemState & (ODS_DISABLED | ODS_GRAYED)) != 0;
    const COLORREF background = entry->menuBarItem
        ? (dark ? RGB(24, 29, 34) : RGB(244, 246, 248))
        : Win32Theme::ControlColor(dark);
    HBRUSH backgroundBrush = CreateSolidBrush(background);
    FillRect(draw->hDC, &draw->rcItem, backgroundBrush);
    DeleteObject(backgroundBrush);

    RECT content = draw->rcItem;
    if (selected && !entry->separator)
    {
        InflateRect(&content, -3, -2);
        HBRUSH selectedBrush = CreateSolidBrush(
            dark ? RGB(52, 116, 181) : RGB(218, 234, 250));
        HPEN selectedPen = CreatePen(PS_SOLID, 1,
            dark ? RGB(67, 139, 211) : RGB(190, 216, 241));
        HGDIOBJ oldBrush = SelectObject(draw->hDC, selectedBrush);
        HGDIOBJ oldPen = SelectObject(draw->hDC, selectedPen);
        RoundRect(draw->hDC, content.left, content.top,
                  content.right, content.bottom, 7, 7);
        SelectObject(draw->hDC, oldPen);
        SelectObject(draw->hDC, oldBrush);
        DeleteObject(selectedPen);
        DeleteObject(selectedBrush);
    }

    if (entry->separator)
    {
        const int y = (draw->rcItem.top + draw->rcItem.bottom) / 2;
        HPEN separatorPen = CreatePen(PS_SOLID, 1,
            dark ? RGB(62, 70, 78) : RGB(214, 219, 224));
        HPEN oldPen = (HPEN)SelectObject(draw->hDC, separatorPen);
        MoveToEx(draw->hDC, draw->rcItem.left + 30, y, NULL);
        LineTo(draw->hDC, draw->rcItem.right - 10, y);
        SelectObject(draw->hDC, oldPen);
        DeleteObject(separatorPen);
        return;
    }

    const COLORREF textColor = disabled
        ? Win32Theme::MutedTextColor(dark)
        : (selected && dark ? RGB(255, 255, 255) : Win32Theme::TextColor(dark));
    SetTextColor(draw->hDC, textColor);
    SetBkMode(draw->hDC, TRANSPARENT);
    HFONT oldFont = (HFONT)SelectObject(draw->hDC, font);

    const char* tab = strchr(entry->text.c_str(), '\t');
    RECT textRect = draw->rcItem;
    if (entry->menuBarItem)
    {
        DrawTextA(draw->hDC, entry->text.c_str(),
                  tab ? (int)(tab - entry->text.c_str()) : -1,
                  &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    else
    {
        textRect.left += 30;
        textRect.right -= 22;
        DrawTextA(draw->hDC, entry->text.c_str(),
                  tab ? (int)(tab - entry->text.c_str()) : -1,
                  &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        if (tab && tab[1])
            DrawTextA(draw->hDC, tab + 1, -1, &textRect,
                      DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }
    SelectObject(draw->hDC, oldFont);

    if (!entry->menuBarItem && (draw->itemState & ODS_CHECKED))
    {
        HPEN checkPen = CreatePen(PS_SOLID, 2, textColor);
        HPEN oldPen = (HPEN)SelectObject(draw->hDC, checkPen);
        const int midY = (draw->rcItem.top + draw->rcItem.bottom) / 2;
        MoveToEx(draw->hDC, draw->rcItem.left + 9, midY, NULL);
        LineTo(draw->hDC, draw->rcItem.left + 13, midY + 4);
        LineTo(draw->hDC, draw->rcItem.left + 20, midY - 5);
        SelectObject(draw->hDC, oldPen);
        DeleteObject(checkPen);
    }
}
}
