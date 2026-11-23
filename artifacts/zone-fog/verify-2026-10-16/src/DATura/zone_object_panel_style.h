#pragma once

#include <windows.h>

namespace ZoneObjectPanelStyle
{
struct Brushes
{
    HBRUSH panel = nullptr;
    HBRUSH control = nullptr;
    HBRUSH edit = nullptr;
};

void EnsureBrushes(Brushes& brushes);
void ApplyListColors(HWND list, bool withCheckboxes);
void ApplyTreeColors(HWND tree);

COLORREF PanelColor();
COLORREF ControlColor();
COLORREF EditColor();
COLORREF TextColor();
}
