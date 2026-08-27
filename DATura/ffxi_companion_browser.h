#pragma once

#include <windows.h>

namespace FFXICompanionBrowser
{
    using LoadCallback = void (*)(void* context, const char* label, const char* datPath);

    void Show(HWND owner, const char* ffxiRoot, LoadCallback loadCallback,
              void* callbackContext = nullptr);
}
