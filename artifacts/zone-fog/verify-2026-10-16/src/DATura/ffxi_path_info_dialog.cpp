#include "stdafx.h"
#include "ffxi_path_info_dialog.h"
#include "win32_panel_controls.h"
#include "win32_tool_window.h"

#include <cstdio>
#include <cstring>

namespace FFXIPathInfoDialog
{
namespace
{
const int kOkControlId = 7001;
const char* const kWindowClassName = "DATuraPathDialogClass";

LRESULT CALLBACK DialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_COMMAND:
        if (LOWORD(wParam) == kOkControlId)
        {
            DestroyWindow(hWnd);
            return 0;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;
    }

    return DefWindowProcA(hWnd, msg, wParam, lParam);
}
}

void Show(HWND owner, const char* title, const char* labelText, const char* pathText)
{
    if (!title)
        title = "FFXI Path";
    if (!labelText)
        labelText = "FFXI path:";
    if (!pathText)
        pathText = "";

    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HDC hdc = GetDC(owner);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    SIZE labelSize = {};
    SIZE pathSize = {};
    GetTextExtentPoint32A(hdc, labelText, (int)std::strlen(labelText), &labelSize);
    GetTextExtentPoint32A(hdc, pathText, (int)std::strlen(pathText), &pathSize);
    SelectObject(hdc, hOldFont);
    ReleaseDC(owner, hdc);

    RECT workArea = {};
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &workArea, 0);

    const int margin = 20;
    const int iconSize = 32;
    const int gap = 12;
    const int buttonW = 76;
    const int buttonH = 24;
    const int textW = (labelSize.cx > pathSize.cx) ? labelSize.cx : pathSize.cx;
    const int contentW = iconSize + gap + textW;
    int clientW = contentW + margin * 2;
    if (clientW < 420)
        clientW = 420;
    int maxClientW = (workArea.right - workArea.left) - 80;
    if (maxClientW < 420)
        maxClientW = 420;
    if (clientW > maxClientW)
        clientW = maxClientW;
    const int clientH = 124;

    RECT wr = { 0, 0, clientW, clientH };
    AdjustWindowRectEx(&wr, WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);
    const int windowW = wr.right - wr.left;
    const int windowH = wr.bottom - wr.top;
    const int x = workArea.left + ((workArea.right - workArea.left) - windowW) / 2;
    const int y = workArea.top + ((workArea.bottom - workArea.top) - windowH) / 2;

    EnableWindow(owner, FALSE);
    Win32ToolWindow::Spec spec =
    {
        DialogProc, kWindowClassName, title, windowW, windowH,
        WS_CAPTION | WS_SYSMENU,
        (HBRUSH)(COLOR_3DFACE + 1)
    };
    spec.x = x;
    spec.y = y;
    spec.extendedStyle = WS_EX_DLGMODALFRAME;
    HWND hDlg = Win32ToolWindow::Create(owner, spec);
    if (!hDlg)
    {
        EnableWindow(owner, TRUE);
        char msg[MAX_PATH + 128];
        sprintf_s(msg, "%s\n%s", labelText, pathText);
        MessageBoxA(owner, msg, title, MB_OK | MB_ICONINFORMATION);
        return;
    }

    HWND hIcon = Win32PanelControls::AddPanelControl(
        hDlg, "STATIC", NULL, SS_ICON, 0,
        margin, 31, iconSize, iconSize);
    SendMessageA(hIcon, STM_SETICON, (WPARAM)LoadIcon(NULL, IDI_INFORMATION), 0);
    HWND hLabel = Win32PanelControls::AddPanelControl(
        hDlg, "STATIC", labelText, 0, 0,
        margin + iconSize + gap, 26, labelSize.cx + 8, 18);
    SendMessageA(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

    int pathControlW = clientW - (margin * 2 + iconSize + gap);
    if (pathControlW < 1)
        pathControlW = 1;
    HWND hPath = Win32PanelControls::AddPanelControl(
        hDlg, "STATIC", pathText, SS_LEFTNOWORDWRAP, 0,
        margin + iconSize + gap, 44, pathControlW, 18);
    SendMessageA(hPath, WM_SETFONT, (WPARAM)hFont, TRUE);
    HWND hButton = Win32PanelControls::AddPanelControl(
        hDlg, "BUTTON", "OK", BS_DEFPUSHBUTTON, kOkControlId,
        clientW - margin - buttonW, clientH - margin - buttonH, buttonW, buttonH);
    SendMessageA(hButton, WM_SETFONT, (WPARAM)hFont, TRUE);

    Win32ToolWindow::Show(hDlg, false);
    SetFocus(hButton);
    MSG msg = {};
    while (IsWindow(hDlg) && GetMessageA(&msg, NULL, 0, 0) > 0)
    {
        if (!IsDialogMessageA(hDlg, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
}
}
