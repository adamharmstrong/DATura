#include "stdafx.h"
#include "ffxi_resource_browser.h"

#include "ffxi_resource.h"
#include "ffxi_zone_resources.h"
#include "resource.h"
#include "win32_panel_controls.h"
#include "win32_tool_window.h"

#include <algorithm>
#include <commctrl.h>
#include <commdlg.h>
#include <string>
#include <vector>

namespace
{
constexpr wchar_t kResourceBrowserClassName[] = L"DATuraResourceBrowserClass";
constexpr int kPathLabelId = 7240;
constexpr int kListId = 7241;
constexpr int kStatusId = 7242;
constexpr int kSeparateTypesId = 7243;
constexpr int kTypeTabsId = 7244;

struct ResourceBrowserState
{
    HWND window = nullptr;
    HWND pathLabel = nullptr;
    HWND list = nullptr;
    HWND status = nullptr;
    HWND separateTypes = nullptr;
    HWND typeTabs = nullptr;
    bool separateByType = false;
    std::vector<FFXIResource::Row> rows;
    std::vector<std::wstring> kinds;
    std::wstring baseStatus;
};

ResourceBrowserState g_browser;

std::wstring WideFromAnsi(const char* text)
{
    if (!text || !text[0])
        return {};
    const int count = MultiByteToWideChar(CP_ACP, 0, text, -1, nullptr, 0);
    if (count <= 1)
        return {};
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_ACP, 0, text, -1, result.data(), count);
    result.resize(static_cast<std::size_t>(count - 1));
    return result;
}

void Layout(const HWND window)
{
    RECT client = {};
    GetClientRect(window, &client);
    const int margin = 10;
    const int labelHeight = 24;
    const int toggleHeight = 26;
    const int tabsHeight = g_browser.separateByType ? 30 : 0;
    const int statusHeight = 38;
    const int width = std::max(100, static_cast<int>(client.right) - margin * 2);
    const int listTop = margin + labelHeight + toggleHeight + tabsHeight;
    const int statusTop = std::max(listTop + 80, static_cast<int>(client.bottom) - margin - statusHeight);
    const int listHeight = std::max(80, statusTop - margin - listTop);
    if (g_browser.pathLabel)
        MoveWindow(g_browser.pathLabel, margin, margin, width, labelHeight, TRUE);
    if (g_browser.separateTypes)
        MoveWindow(g_browser.separateTypes, margin, margin + labelHeight,
                   190, toggleHeight, TRUE);
    if (g_browser.typeTabs)
    {
        MoveWindow(g_browser.typeTabs, margin, margin + labelHeight + toggleHeight,
                   width, 30, TRUE);
        ShowWindow(g_browser.typeTabs,
                   g_browser.separateByType ? SW_SHOW : SW_HIDE);
    }
    if (g_browser.list)
        MoveWindow(g_browser.list, margin, listTop, width, listHeight, TRUE);
    if (g_browser.status)
        MoveWindow(g_browser.status, margin, statusTop,
                   width, statusHeight, TRUE);
}

std::wstring SelectedKind()
{
    if (!g_browser.separateByType || !g_browser.typeTabs)
        return {};
    const int selected = TabCtrl_GetCurSel(g_browser.typeTabs);
    return selected >= 0 && selected < static_cast<int>(g_browser.kinds.size())
        ? g_browser.kinds[static_cast<std::size_t>(selected)]
        : std::wstring();
}

void PopulateListForCurrentView()
{
    if (!g_browser.list)
        return;

    const std::wstring selectedKind = SelectedKind();
    SendMessageW(g_browser.list, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(g_browser.list);
    int visibleCount = 0;
    for (const FFXIResource::Row& row : g_browser.rows)
    {
        if (!selectedKind.empty() && row.kind != selectedKind)
            continue;

        LVITEMW item = {};
        item.mask = LVIF_TEXT;
        item.iItem = visibleCount;
        item.pszText = const_cast<LPWSTR>(row.kind.c_str());
        const int inserted = ListView_InsertItem(g_browser.list, &item);
        if (inserted < 0)
            continue;
        ListView_SetItemText(g_browser.list, inserted, 1, const_cast<LPWSTR>(row.id.c_str()));
        ListView_SetItemText(g_browser.list, inserted, 2, const_cast<LPWSTR>(row.text.c_str()));
        ListView_SetItemText(g_browser.list, inserted, 3, const_cast<LPWSTR>(row.details.c_str()));
        ++visibleCount;
    }
    SendMessageW(g_browser.list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_browser.list, nullptr, TRUE);

    if (g_browser.status)
    {
        std::wstring status = g_browser.baseStatus;
        if (!selectedKind.empty())
        {
            status = selectedKind + L": " + std::to_wstring(visibleCount) +
                     L" record(s) in this tab. " + g_browser.baseStatus;
        }
        SetWindowTextW(g_browser.status, status.c_str());
    }
}

void RebuildTypeTabs()
{
    if (!g_browser.typeTabs)
        return;

    const std::wstring previousKind = SelectedKind();
    g_browser.kinds.clear();
    for (const FFXIResource::Row& row : g_browser.rows)
    {
        if (std::find(g_browser.kinds.begin(), g_browser.kinds.end(), row.kind) ==
            g_browser.kinds.end())
        {
            g_browser.kinds.push_back(row.kind);
        }
    }

    TabCtrl_DeleteAllItems(g_browser.typeTabs);
    int selectedIndex = 0;
    for (int i = 0; i < static_cast<int>(g_browser.kinds.size()); ++i)
    {
        int count = 0;
        for (const FFXIResource::Row& row : g_browser.rows)
            count += row.kind == g_browser.kinds[static_cast<std::size_t>(i)] ? 1 : 0;

        std::wstring label = g_browser.kinds[static_cast<std::size_t>(i)] +
                             L" (" + std::to_wstring(count) + L")";
        TCITEMW item = {};
        item.mask = TCIF_TEXT;
        item.pszText = const_cast<LPWSTR>(label.c_str());
        TabCtrl_InsertItem(g_browser.typeTabs, i, &item);
        if (!previousKind.empty() && g_browser.kinds[static_cast<std::size_t>(i)] == previousKind)
            selectedIndex = i;
    }
    if (!g_browser.kinds.empty())
        TabCtrl_SetCurSel(g_browser.typeTabs, selectedIndex);
}

LRESULT CALLBACK ResourceBrowserWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        g_browser.pathLabel = Win32PanelControls::AddPanelControlW(
            window, L"STATIC", L"", SS_LEFTNOWORDWRAP,
            kPathLabelId, 0, 0, 0, 0);
        g_browser.separateTypes = Win32PanelControls::AddPanelControlW(
            window, L"BUTTON", L"Separate by type", WS_TABSTOP | BS_AUTOCHECKBOX,
            kSeparateTypesId, 0, 0, 0, 0);
        g_browser.typeTabs = Win32PanelControls::AddPanelControlW(
            window, WC_TABCONTROLW, L"", WS_CLIPSIBLINGS | TCS_TABS | TCS_SINGLELINE,
            kTypeTabsId, 0, 0, 0, 0, 0, false);
        g_browser.list = Win32PanelControls::AddPanelControlW(
            window, WC_LISTVIEWW, L"", LVS_REPORT | LVS_SHOWSELALWAYS,
            kListId, 0, 0, 0, 0, WS_EX_CLIENTEDGE);
        g_browser.status = Win32PanelControls::AddPanelControlW(
            window, L"STATIC", L"", SS_LEFT,
            kStatusId, 0, 0, 0, 0);

        const HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        SendMessageW(g_browser.pathLabel, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(g_browser.separateTypes, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(g_browser.typeTabs, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(g_browser.list, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(g_browser.status, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        ListView_SetExtendedListViewStyle(g_browser.list,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);

        const wchar_t* headings[] = { L"Kind", L"ID", L"Text / Name", L"Details" };
        const int widths[] = { 120, 145, 430, 520 };
        for (int i = 0; i < 4; ++i)
        {
            LVCOLUMNW column = {};
            column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
            column.iSubItem = i;
            column.cx = widths[i];
            column.pszText = const_cast<LPWSTR>(headings[i]);
            ListView_InsertColumn(g_browser.list, i, &column);
        }
        Layout(window);
        return 0;
    }
    case WM_SIZE:
        Layout(window);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == kSeparateTypesId && HIWORD(wParam) == BN_CLICKED)
        {
            g_browser.separateByType =
                SendMessageW(g_browser.separateTypes, BM_GETCHECK, 0, 0) == BST_CHECKED;
            if (g_browser.separateByType)
                RebuildTypeTabs();
            Layout(window);
            PopulateListForCurrentView();
            return 0;
        }
        break;
    case WM_NOTIFY:
    {
        const NMHDR* header = reinterpret_cast<const NMHDR*>(lParam);
        if (header && header->idFrom == kTypeTabsId && header->code == TCN_SELCHANGE)
        {
            PopulateListForCurrentView();
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        if (window == g_browser.window)
        {
            g_browser.window = nullptr;
            g_browser.pathLabel = nullptr;
            g_browser.separateTypes = nullptr;
            g_browser.typeTabs = nullptr;
            g_browser.list = nullptr;
            g_browser.status = nullptr;
        }
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

bool EnsureWindow(const HWND owner)
{
    if (g_browser.window && IsWindow(g_browser.window))
        return true;

    const HINSTANCE instance = GetModuleHandleW(nullptr);
    Win32ToolWindow::WideSpec spec =
    {
        ResourceBrowserWndProc, kResourceBrowserClassName,
        L"DATura Resource Browser", 1240, 680,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1)
    };
    spec.extendedStyle = WS_EX_APPWINDOW;
    spec.icon = LoadIcon(instance, MAKEINTRESOURCE(IDI_DATURA));
    spec.classStyle = CS_HREDRAW | CS_VREDRAW;
    g_browser.window = Win32ToolWindow::Create(owner, spec);
    if (!g_browser.window)
        return false;
    Win32ToolWindow::Show(g_browser.window, false);
    UpdateWindow(g_browser.window);
    return true;
}

void Populate(const HWND owner, const std::wstring& title, const std::wstring& source,
              const std::vector<FFXIResource::Row>& rows, const std::wstring& status)
{
    if (!EnsureWindow(owner))
        return;

    SetWindowTextW(g_browser.window, title.c_str());
    SetWindowTextW(g_browser.pathLabel, source.c_str());
    g_browser.rows = rows;
    g_browser.baseStatus = status;
    SendMessageW(g_browser.separateTypes, BM_SETCHECK,
                 g_browser.separateByType ? BST_CHECKED : BST_UNCHECKED, 0);
    if (g_browser.separateByType)
        RebuildTypeTabs();
    Layout(g_browser.window);
    PopulateListForCurrentView();
    Win32ToolWindow::Show(g_browser.window, true, SW_RESTORE);
}
}

namespace FFXIResourceBrowser
{
void ShowCurrentZone(const HWND owner, const char* ffxiRoot, const char* loadedZonePath)
{
    FFXIZoneResources::BrowseData data;
    if (!FFXIZoneResources::BuildBrowseData(ffxiRoot, loadedZonePath, data))
    {
        MessageBoxA(owner, "Load a known zone before opening its resources.",
                    "Zone Resources", MB_OK | MB_ICONINFORMATION);
        return;
    }

    std::wstring title = L"DATura Resources - " + WideFromAnsi(data.zoneName.c_str());
    std::wstring source = L"Zone " + std::to_wstring(data.zoneId) + L": " + WideFromAnsi(data.zoneName.c_str());
    if (!data.formats.empty())
        source += L" | " + data.formats;
    std::wstring status = std::to_wstring(data.rows.size()) + L" records from " +
                          std::to_wstring(data.parsedFileCount) + L" resource file(s).";
    if (!data.warnings.empty())
        status += L" " + data.warnings;
    if (data.rows.empty())
        status = L"No supported dialog or NPC resource file could be parsed for this zone.";
    Populate(owner, title, source, data.rows, status);
}

void ShowDatFile(const HWND owner, const char* ffxiRoot)
{
    char path[MAX_PATH] = {};
    OPENFILENAMEA open = {};
    open.lStructSize = sizeof(open);
    open.hwndOwner = owner;
    open.lpstrFile = path;
    open.nMaxFile = sizeof(path);
    open.lpstrFilter = "FFXI DAT Files (*.DAT)\0*.DAT\0All Files (*.*)\0*.*\0\0";
    open.nFilterIndex = 1;
    open.lpstrInitialDir = (ffxiRoot && ffxiRoot[0]) ? ffxiRoot : nullptr;
    open.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetOpenFileNameA(&open))
        return;

    FFXIResource::ParseResult parsed;
    const bool recognized = FFXIResource::ParseFile(path, parsed);
    const std::wstring widePath = L"Logical: " + WideFromAnsi(parsed.logicalPath.c_str()) +
        L" | Source: " + WideFromAnsi(parsed.sourcePath.c_str());
    const std::wstring title = recognized ? L"DATura Resource Browser - " + parsed.format :
                                            L"DATura Resource Browser - Unrecognized";
    std::wstring status = recognized ? std::to_wstring(parsed.rows.size()) + L" records." : parsed.warning;
    if (recognized && !parsed.warning.empty())
        status += L" " + parsed.warning;
    Populate(owner, title, widePath, parsed.rows, status);
}
}
