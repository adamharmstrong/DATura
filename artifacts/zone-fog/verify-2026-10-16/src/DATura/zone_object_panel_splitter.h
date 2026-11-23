#pragma once

#include <windows.h>

namespace ZoneObjectPanelSplitter
{
bool SetResizeCursorIfOverSplitter(HWND panel, int paneWidths[3]);
bool BeginResizeDrag(HWND panel, int paneWidths[3], int mouseX, int mouseY, int& activeSplitter);
bool ResizeActiveDrag(HWND panel, int paneWidths[3], int activeSplitter, int mouseX,
                      int* outClientWidth, int* outClientHeight);
bool EndResizeDrag(int& activeSplitter);
}
