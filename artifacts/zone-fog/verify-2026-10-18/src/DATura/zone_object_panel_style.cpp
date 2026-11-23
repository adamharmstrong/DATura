#include "stdafx.h"
#include "zone_object_panel_style.h"

#include <commctrl.h>

namespace
{
const COLORREF kPanelColor = RGB(50, 54, 56);
const COLORREF kControlColor = RGB(28, 29, 30);
const COLORREF kEditColor = RGB(61, 64, 66);
const COLORREF kTextColor = RGB(238, 241, 243);
}

namespace ZoneObjectPanelStyle
{
void EnsureBrushes(Brushes& brushes)
{
    if (!brushes.panel)
        brushes.panel = CreateSolidBrush(kPanelColor);
    if (!brushes.control)
        brushes.control = CreateSolidBrush(kControlColor);
    if (!brushes.edit)
        brushes.edit = CreateSolidBrush(kEditColor);
}

void ApplyListColors(HWND list, const bool withCheckboxes)
{
    if (!list)
        return;

    ListView_SetBkColor(list, kControlColor);
    ListView_SetTextBkColor(list, kControlColor);
    ListView_SetTextColor(list, kTextColor);
    DWORD exStyle = LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER;
    if (withCheckboxes)
        exStyle |= LVS_EX_CHECKBOXES;
    ListView_SetExtendedListViewStyle(list, exStyle);
}

void ApplyTreeColors(HWND tree)
{
    if (!tree)
        return;

    TreeView_SetBkColor(tree, kControlColor);
    TreeView_SetTextColor(tree, kTextColor);
}

COLORREF PanelColor()
{
    return kPanelColor;
}

COLORREF ControlColor()
{
    return kControlColor;
}

COLORREF EditColor()
{
    return kEditColor;
}

COLORREF TextColor()
{
    return kTextColor;
}
}
