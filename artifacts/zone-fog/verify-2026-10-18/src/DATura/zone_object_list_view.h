#pragma once

#include <windows.h>

namespace ZoneObjectListView
{
void ClearColumns(HWND list);
void AddColumn(HWND list, int index, const char* text, int width);
void SetSubItem(HWND list, int row, int column, const char* text);
}
