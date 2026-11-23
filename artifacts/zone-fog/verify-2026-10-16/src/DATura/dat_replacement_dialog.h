#pragma once
#include <windows.h>

namespace DatReplacementDialog
{
// Reads saved policy once, before startup assets load. Dialog changes are saved
// for the next process start to avoid mixing cached and newly replaced assets.
void Initialize(const char* installRoot, const char* registryKey);
void Show(HWND owner);
}
