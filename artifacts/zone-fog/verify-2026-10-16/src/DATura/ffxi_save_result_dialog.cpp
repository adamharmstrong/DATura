#include "stdafx.h"
#include "ffxi_save_result_dialog.h"
#include "win32_panel_controls.h"
#include "win32_tool_window.h"

#include <algorithm>
#include <cstring>

namespace FFXISaveResultDialog
{
namespace
{
const int kOkControlId = 7101;
const char* const kWindowClassName = "DATuraSaveResultDialog";

struct DialogData
{
    const char* noesisPath;
    const char* datSetPath;
};

LRESULT CALLBACK DialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        {
            const CREATESTRUCTA* pCreate = (const CREATESTRUCTA*)lParam;
            const DialogData* pData = (const DialogData*)pCreate->lpCreateParams;
            HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

            HWND hIcon = Win32PanelControls::AddPanelControl(
                hWnd, "STATIC", "", SS_ICON, 0, 18, 28, 32, 32);
            SendMessageA(hIcon, STM_SETICON, (WPARAM)LoadIcon(NULL, IDI_INFORMATION), 0);

            HWND hLabel = Win32PanelControls::AddPanelControl(
                hWnd, "STATIC", "Saved character files:", 0, 0,
                64, 24, 520, 18);
            SendMessageA(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hNoesis = Win32PanelControls::AddPanelControl(
                hWnd, "EDIT", pData ? pData->noesisPath : "",
                ES_AUTOHSCROLL | ES_READONLY, 0, 64, 46, 520, 22, WS_EX_CLIENTEDGE);
            SendMessageA(hNoesis, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hDatSet = Win32PanelControls::AddPanelControl(
                hWnd, "EDIT", pData ? pData->datSetPath : "",
                ES_AUTOHSCROLL | ES_READONLY, 0, 64, 74, 520, 22, WS_EX_CLIENTEDGE);
            SendMessageA(hDatSet, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hOk = Win32PanelControls::AddPanelControl(
                hWnd, "BUTTON", "OK", WS_TABSTOP | BS_DEFPUSHBUTTON,
                kOkControlId, 510, 122, 74, 24);
            SendMessageA(hOk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SetFocus(hOk);
            return 0;
        }

    case WM_COMMAND:
        if (LOWORD(wParam) == kOkControlId && HIWORD(wParam) == BN_CLICKED)
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

int MeasureText(HWND referenceWindow, HFONT hFont, const char* text)
{
    HDC hdc = GetDC(referenceWindow);
    HGDIOBJ oldFont = SelectObject(hdc, hFont);
    SIZE size = {};
    GetTextExtentPoint32A(hdc, text ? text : "", (int)strlen(text ? text : ""), &size);
    SelectObject(hdc, oldFont);
    ReleaseDC(referenceWindow, hdc);
    return size.cx;
}
}

void Show(HWND owner, HWND positioningOwner, const char* noesisPath, const char* datSetPath)
{
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    const int textWidth = std::max(MeasureText(positioningOwner, hFont, noesisPath),
                                   MeasureText(positioningOwner, hFont, datSetPath));
    const int clientWidth = std::max(412, textWidth + 132);

    RECT workArea = {};
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &workArea, 0);
    const int workWidth = (int)(workArea.right - workArea.left);
    const int maxClientWidth = std::max(412, workWidth - 120);
    const int finalClientWidth = std::min(clientWidth, maxClientWidth);
    const int editWidth = finalClientWidth - 88;
    const int clientHeight = 162;

    RECT wr = { 0, 0, finalClientWidth, clientHeight };
    AdjustWindowRectEx(&wr, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);

    RECT ownerRect = {};
    if (owner && IsWindow(owner))
        GetWindowRect(owner, &ownerRect);
    else
        GetWindowRect(positioningOwner, &ownerRect);
    const int ownerCx = ownerRect.right - ownerRect.left;
    const int ownerCy = ownerRect.bottom - ownerRect.top;
    const int winWidth = wr.right - wr.left;
    const int winHeight = wr.bottom - wr.top;
    int x = ownerRect.left + (ownerCx - winWidth) / 2;
    int y = ownerRect.top + (ownerCy - winHeight) / 2;
    x = std::max((int)workArea.left, std::min(x, (int)workArea.right - winWidth));
    y = std::max((int)workArea.top, std::min(y, (int)workArea.bottom - winHeight));

    DialogData data = { noesisPath, datSetPath };
    Win32ToolWindow::Spec spec =
    {
        DialogProc, kWindowClassName, "Save Character", winWidth, winHeight,
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        (HBRUSH)(COLOR_BTNFACE + 1)
    };
    spec.x = x;
    spec.y = y;
    spec.extendedStyle = WS_EX_DLGMODALFRAME;
    spec.icon = LoadIcon(NULL, IDI_INFORMATION);
    spec.createParameter = &data;
    HWND hDlg = Win32ToolWindow::Create(owner, spec);
    if (!hDlg)
        return;

    SetWindowPos(GetWindow(hDlg, GW_CHILD), NULL, 18, 28, 32, 32, SWP_NOZORDER);
    HWND hChild = GetWindow(hDlg, GW_CHILD);
    hChild = GetWindow(hChild, GW_HWNDNEXT);
    SetWindowPos(hChild, NULL, 64, 24, editWidth, 18, SWP_NOZORDER);
    hChild = GetWindow(hChild, GW_HWNDNEXT);
    SetWindowPos(hChild, NULL, 64, 46, editWidth, 22, SWP_NOZORDER);
    hChild = GetWindow(hChild, GW_HWNDNEXT);
    SetWindowPos(hChild, NULL, 64, 74, editWidth, 22, SWP_NOZORDER);
    hChild = GetWindow(hChild, GW_HWNDNEXT);
    SetWindowPos(hChild, NULL, finalClientWidth - 92, 122, 74, 24, SWP_NOZORDER);

    Win32ToolWindow::Show(hDlg, false);
    if (owner)
        EnableWindow(owner, FALSE);
    MSG msg = {};
    while (IsWindow(hDlg) && GetMessageA(&msg, NULL, 0, 0) > 0)
    {
        if (!IsDialogMessageA(hDlg, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
    if (owner)
    {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }
}
}
