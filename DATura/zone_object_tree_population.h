#pragma once

#include <windows.h>
#include <commctrl.h>

#include <string>
#include <vector>

namespace ZoneObjectTreePopulation
{
void PopulateExpandedNode(HWND tree, HTREEITEM item, LPARAM param,
                          const std::vector<std::string>& hiddenObjectNames);
}
