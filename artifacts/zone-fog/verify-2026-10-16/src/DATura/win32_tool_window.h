#pragma once

#include <windows.h>

namespace Win32ToolWindow
{
template <typename Character>
struct BasicSpec
{
    WNDPROC windowProcedure;
    const Character* className;
    const Character* title;
    int width;
    int height;
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    HBRUSH backgroundBrush = (HBRUSH)(COLOR_BTNFACE + 1);
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    DWORD extendedStyle = WS_EX_TOOLWINDOW;
    HICON icon = NULL;
    void* createParameter = NULL;
    UINT classStyle = 0;
};

using Spec = BasicSpec<char>;
using WideSpec = BasicSpec<wchar_t>;

HWND Create(HWND owner, const Spec& spec);
HWND Create(HWND owner, const WideSpec& spec);
void Show(HWND window, bool activate, int command = SW_SHOW);
}
