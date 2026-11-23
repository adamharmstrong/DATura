#pragma once

#include <windows.h>

namespace FFXIPathInfoDialog
{
    void Show(HWND owner, const char* title, const char* labelText, const char* pathText);
}
