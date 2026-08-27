#pragma once

#include <windows.h>

namespace ZoneObjectListColumns
{
void SetupMapObjectColumns(HWND list, int& columnMode, int mode);
void SetupCollisionColumns(HWND list, int& columnMode, int mode);
void SetupDrawBatchColumns(HWND list, int& columnMode, int mode);
}
