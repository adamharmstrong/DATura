#pragma once

#include <windows.h>

namespace Win32Theme { struct State; }

namespace FFXICompanionBrowser
{
    using LoadCallback = void (*)(void* context, const char* label, const char* datPath);

    void Show(HWND owner, const char* ffxiRoot, Win32Theme::State& theme,
              LoadCallback loadCallback,
              void* callbackContext = nullptr);
}
