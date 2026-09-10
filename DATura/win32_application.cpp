#include "stdafx.h"
#include "win32_application.h"

#include <commctrl.h>

namespace Win32Application
{
void InitializeCommonControls(const DWORD classes)
{
    INITCOMMONCONTROLSEX commonControls = {};
    commonControls.dwSize = sizeof(commonControls);
    commonControls.dwICC = classes;
    InitCommonControlsEx(&commonControls);
}

HWND CreateMainWindow(const WindowSpec& spec, CreateWindowResult& result)
{
    result = CreateWindowResult::RegistrationFailed;
    if (!spec.instance || !spec.windowProcedure || !spec.className || !spec.title)
        return NULL;

    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = spec.classStyle;
    windowClass.lpfnWndProc = spec.windowProcedure;
    windowClass.hInstance = spec.instance;
    windowClass.hCursor = spec.cursor ? spec.cursor : LoadCursor(NULL, IDC_ARROW);
    windowClass.hbrBackground = spec.backgroundBrush;
    windowClass.lpszClassName = spec.className;
    windowClass.hIcon = spec.icon;
    windowClass.hIconSm = spec.smallIcon;

    if (!RegisterClassExW(&windowClass))
        return NULL;

    RECT windowRect = { 0, 0, spec.clientWidth, spec.clientHeight };
    AdjustWindowRectEx(&windowRect, spec.windowStyle, spec.hasMenu, spec.extendedStyle);

    HWND window = CreateWindowExW(
        spec.extendedStyle,
        spec.className,
        spec.title,
        spec.windowStyle,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        NULL,
        NULL,
        spec.instance,
        NULL);
    if (!window)
    {
        result = CreateWindowResult::CreationFailed;
        UnregisterClassW(spec.className, spec.instance);
        return NULL;
    }

    result = CreateWindowResult::Success;
    return window;
}

int Run(
    const HWND window,
    const int showCommand,
    const FrameCallback frameCallback,
    void* const context)
{
    LARGE_INTEGER performanceFrequency = {};
    LARGE_INTEGER lastFrame = {};
    const bool hasPerformanceTimer =
        QueryPerformanceFrequency(&performanceFrequency) && performanceFrequency.QuadPart > 0;
    if (hasPerformanceTimer)
        QueryPerformanceCounter(&lastFrame);

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG message = {};
    for (;;)
    {
        while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
                return static_cast<int>(message.wParam);

            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        float deltaSeconds = 0.0f;
        if (hasPerformanceTimer)
        {
            LARGE_INTEGER now = {};
            QueryPerformanceCounter(&now);
            deltaSeconds = static_cast<float>(now.QuadPart - lastFrame.QuadPart)
                / static_cast<float>(performanceFrequency.QuadPart);
            lastFrame = now;
            if (deltaSeconds > 0.1f)
                deltaSeconds = 0.1f;
        }

        if (frameCallback)
            frameCallback(context, deltaSeconds);
    }
}
}
