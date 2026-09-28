#include "stdafx.h"
#include "ffxi_companion_browser.h"

#include "ffxi_paths.h"
#include "npc_monster_dat_table.h"
#include "companion_dat_table.h"
#include "win32_panel_controls.h"
#include "win32_theme.h"
#include "win32_tool_window.h"

namespace
{
constexpr char kWindowClassName[] = "DATuraCompanionBrowserClass";
constexpr int kCategoryId = 8400;
constexpr int kListId = 8401;
constexpr int kPathId = 8402;
constexpr int kLoadId = 8403;
constexpr int kCloseId = 8404;

struct State
{
    HWND window = nullptr;
    HWND owner = nullptr;
    const char* ffxiRoot = nullptr;
    Win32Theme::State* theme = nullptr;
    FFXICompanionBrowser::LoadCallback loadCallback = nullptr;
    void* callbackContext = nullptr;
};

State g_state;

int SelectedGroup(const HWND window)
{
    const LRESULT selection = SendDlgItemMessageA(window, kCategoryId, CB_GETCURSEL, 0, 0);
    return selection >= 0 && selection < kFFXICompanionGroupCount
        ? static_cast<int>(selection)
        : -1;
}

const FFXIStandaloneModelEntry* SelectedEntry(const HWND window)
{
    const int groupIndex = SelectedGroup(window);
    if (groupIndex < 0)
        return nullptr;

    const FFXIStandaloneModelGroup& group = kFFXICompanionGroups[groupIndex];
    const LRESULT selection = SendDlgItemMessageA(window, kListId, LB_GETCURSEL, 0, 0);
    if (selection < 0 || selection >= group.count)
        return nullptr;
    return &group.entries[selection];
}

void UpdateSelection(const HWND window)
{
    const FFXIStandaloneModelEntry* entry = SelectedEntry(window);
    char text[384] = "Select a companion to view its model.";
    bool available = false;
    if (entry)
    {
        char fullPath[MAX_PATH] = {};
        FFXIPath::BuildFullPath(g_state.ffxiRoot, entry->dat, fullPath, sizeof(fullPath));
        available = FFXIPath::FileExists(fullPath);
        sprintf_s(text, "%s\r\nDAT: %s%s", entry->label, entry->dat,
                  available ? "" : "  (not installed)");
    }
    SetDlgItemTextA(window, kPathId, text);
    EnableWindow(GetDlgItem(window, kLoadId), available ? TRUE : FALSE);
}

void PopulateList(const HWND window)
{
    HWND list = GetDlgItem(window, kListId);
    if (!list)
        return;

    SendMessageA(list, LB_RESETCONTENT, 0, 0);
    const int groupIndex = SelectedGroup(window);
    if (groupIndex >= 0)
    {
        const FFXIStandaloneModelGroup& group = kFFXICompanionGroups[groupIndex];
        for (int i = 0; i < group.count; ++i)
            SendMessageA(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(group.entries[i].label));
        if (group.count > 0)
            SendMessageA(list, LB_SETCURSEL, 0, 0);
    }
    UpdateSelection(window);
}

void LoadSelected(const HWND window)
{
    const FFXIStandaloneModelEntry* entry = SelectedEntry(window);
    if (entry && g_state.loadCallback)
        g_state.loadCallback(g_state.callbackContext, entry->label, entry->dat);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        if (!g_state.theme)
            return -1;
        HWND title = Win32PanelControls::AddPanelControl(window, "STATIC",
            "Browse pets, mounts, and summoned companions.", 0, -1,
            22, 34, 374, 20);
        HWND categoryLabel = Win32PanelControls::AddPanelControl(window, "STATIC", "Category:", 0, -1,
            22, 64, 72, 18);
        HWND category = Win32PanelControls::AddPanelCombo(window, kCategoryId, 96, 60, 300);
        HWND list = Win32PanelControls::AddPanelControl(window, "LISTBOX", "",
            WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY, kListId,
            22, 94, 374, 272, WS_EX_CLIENTEDGE);
        HWND path = Win32PanelControls::AddPanelControl(window, "STATIC", "", SS_LEFT, kPathId,
            22, 378, 374, 42);
        HWND load = Win32PanelControls::AddPanelControl(window, "BUTTON", "Load in Viewer",
            BS_OWNERDRAW | WS_TABSTOP, kLoadId, 178, 442, 128, 30);
        HWND close = Win32PanelControls::AddPanelControl(window, "BUTTON", "Close",
            BS_OWNERDRAW | WS_TABSTOP, kCloseId, 314, 442, 82, 30);

        Win32Theme::ApplyWindowTheme(window, *g_state.theme);

        for (int i = 0; i < kFFXICompanionGroupCount; ++i)
            SendMessageA(category, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kFFXICompanionGroups[i].name));
        SendMessageA(category, CB_SETCURSEL, 0, 0);
        PopulateList(window);
        return 0;
    }
    case WM_DRAWITEM:
        if (g_state.theme)
        {
            const DRAWITEMSTRUCT* draw = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            Win32Theme::DrawButton(draw, g_state.theme->dark,
                g_state.theme->resources.font, LOWORD(wParam) == kLoadId);
            return TRUE;
        }
        break;
    case WM_CTLCOLORSTATIC:
        if (g_state.theme)
        {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, Win32Theme::TextColor(g_state.theme->dark));
            return reinterpret_cast<LRESULT>(g_state.theme->resources.controlBrush);
        }
        break;
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLOREDIT:
        if (g_state.theme)
        {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetBkColor(dc, Win32Theme::EditColor(g_state.theme->dark));
            SetTextColor(dc, Win32Theme::TextColor(g_state.theme->dark));
            return reinterpret_cast<LRESULT>(g_state.theme->resources.editBrush);
        }
        break;
    case WM_ERASEBKGND:
        if (g_state.theme)
        {
            RECT client = {};
            GetClientRect(window, &client);
            FillRect(reinterpret_cast<HDC>(wParam), &client,
                g_state.theme->resources.windowBrush);
            return 1;
        }
        break;
    case WM_PAINT:
        if (g_state.theme)
        {
            PAINTSTRUCT paint = {};
            HDC dc = BeginPaint(window, &paint);
            FillRect(dc, &paint.rcPaint, g_state.theme->resources.windowBrush);
            const RECT panel = { 8, 8, 412, 430 };
            Win32Theme::DrawPanel(dc, panel, "Companions", g_state.theme->dark,
                g_state.theme->resources.sectionFont);
            EndPaint(window, &paint);
            return 0;
        }
        break;
    case WM_COMMAND:
        if (LOWORD(wParam) == kCategoryId && HIWORD(wParam) == CBN_SELCHANGE)
        {
            PopulateList(window);
            return 0;
        }
        if (LOWORD(wParam) == kListId && HIWORD(wParam) == LBN_SELCHANGE)
        {
            UpdateSelection(window);
            return 0;
        }
        if ((LOWORD(wParam) == kListId && HIWORD(wParam) == LBN_DBLCLK) ||
            (LOWORD(wParam) == kLoadId && HIWORD(wParam) == BN_CLICKED))
        {
            LoadSelected(window);
            return 0;
        }
        if (LOWORD(wParam) == kCloseId && HIWORD(wParam) == BN_CLICKED)
        {
            ShowWindow(window, SW_HIDE);
            return 0;
        }
        break;
    case WM_CLOSE:
        ShowWindow(window, SW_HIDE);
        return 0;
    case WM_DESTROY:
        if (g_state.window == window)
            g_state.window = nullptr;
        return 0;
    }

    return DefWindowProcA(window, message, wParam, lParam);
}
}

namespace FFXICompanionBrowser
{
void Show(const HWND owner, const char* ffxiRoot, Win32Theme::State& theme,
          const LoadCallback loadCallback,
          void* callbackContext)
{
    g_state.owner = owner;
    g_state.ffxiRoot = ffxiRoot;
    g_state.theme = &theme;
    g_state.loadCallback = loadCallback;
    g_state.callbackContext = callbackContext;

    if (g_state.window && IsWindow(g_state.window))
    {
        Win32ToolWindow::Show(g_state.window, true);
        return;
    }

    RECT ownerRect = {};
    GetWindowRect(owner, &ownerRect);
    Win32ToolWindow::Spec spec = {
        WindowProc,
        kWindowClassName,
        "Companion Browser",
        438,
        526
    };
    spec.backgroundBrush = theme.resources.windowBrush;
    spec.x = ownerRect.left + 32;
    spec.y = ownerRect.top + 64;
    g_state.window = Win32ToolWindow::Create(owner, spec);
    if (g_state.window)
        Win32ToolWindow::Show(g_state.window, false);
}
}
