#pragma once

#include <windows.h>

namespace Win32Application
{
enum class CreateWindowResult
{
    Success,
    RegistrationFailed,
    CreationFailed
};

struct WindowSpec
{
    HINSTANCE instance = NULL;
    WNDPROC windowProcedure = NULL;
    const wchar_t* className = NULL;
    const wchar_t* title = NULL;
    int clientWidth = 0;
    int clientHeight = 0;
    DWORD classStyle = CS_HREDRAW | CS_VREDRAW;
    DWORD windowStyle = WS_OVERLAPPEDWINDOW;
    DWORD extendedStyle = 0;
    HCURSOR cursor = NULL;
    HBRUSH backgroundBrush = NULL;
    HICON icon = NULL;
    HICON smallIcon = NULL;
    bool hasMenu = false;
};

using FrameCallback = void (*)(void* context, float deltaSeconds);

void InitializeCommonControls(DWORD classes);
HWND CreateMainWindow(const WindowSpec& spec, CreateWindowResult& result);
int Run(HWND window, int showCommand, FrameCallback frameCallback, void* context = nullptr);
}
