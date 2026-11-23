#include "stdafx.h"
#include "win32_tool_window.h"

namespace Win32ToolWindow
{
HWND Create(const HWND owner, const Spec& spec)
{
    if (!spec.windowProcedure || !spec.className || !spec.title)
        return NULL;

    HINSTANCE instance = GetModuleHandle(NULL);
    WNDCLASSEXA windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = spec.classStyle;
    windowClass.lpfnWndProc = spec.windowProcedure;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.hIcon = spec.icon;
    windowClass.hbrBackground = spec.backgroundBrush;
    windowClass.lpszClassName = spec.className;
    RegisterClassExA(&windowClass);

    return CreateWindowExA(
        spec.extendedStyle, spec.className, spec.title,
        spec.style,
        spec.x, spec.y, spec.width, spec.height,
        owner, NULL, instance, spec.createParameter);
}

HWND Create(const HWND owner, const WideSpec& spec)
{
    if (!spec.windowProcedure || !spec.className || !spec.title)
        return NULL;

    HINSTANCE instance = GetModuleHandleW(NULL);
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = spec.classStyle;
    windowClass.lpfnWndProc = spec.windowProcedure;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.hIcon = spec.icon;
    windowClass.hbrBackground = spec.backgroundBrush;
    windowClass.lpszClassName = spec.className;
    RegisterClassExW(&windowClass);

    return CreateWindowExW(
        spec.extendedStyle, spec.className, spec.title,
        spec.style,
        spec.x, spec.y, spec.width, spec.height,
        owner, NULL, instance, spec.createParameter);
}

void Show(const HWND window, const bool activate, const int command)
{
    if (!window)
        return;

    ShowWindow(window, command);
    if (activate)
        SetForegroundWindow(window);
}
}
