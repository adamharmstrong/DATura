#pragma once

#include <windows.h>

namespace FFXIResourceBrowser
{
    void ShowCurrentZone(HWND owner, const char* ffxiRoot, const char* loadedZonePath);
    void ShowDatFile(HWND owner, const char* ffxiRoot);
}
