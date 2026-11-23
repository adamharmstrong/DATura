#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "query.h"
#include "menu.h"
#include "npc_chat_window.h"

// PS2 XiEvent::CodeQUERY feeds a question and choices to CTkQueryControl;
// the caller resumes the event only after confirming/cancelling a selection.
// Reuse DATura's existing bitmap font and windowps chat backdrop. The native
// query layout and cursor asset have not yet been resolved; these are fallback
// layout/cursor visuals, not a pixel-exact reproduction of TkQuery.cpp.
namespace NpcChatWindow
{
HWND choiceWindow = nullptr;
std::wstring choicePrompt;
std::vector<std::wstring> choiceOptions;
int choiceSelected = 0;
int choiceFirstVisible = 0;

int QueryPromptHeight(int width)
{
    if (choicePrompt.empty()) return 0;
    if (officialFont)
        return static_cast<int>(WrapBitmapText(choicePrompt, (std::max)(1, width)).size()) * lineHeight;
    HDC dc = GetDC(choiceWindow);
    HFONT old = font ? (HFONT)SelectObject(dc, font) : nullptr;
    RECT bounds{0, 0, (std::max)(1, width), 0};
    DrawTextW(dc, choicePrompt.c_str(), -1, &bounds, DT_LEFT | DT_WORDBREAK | DT_CALCRECT);
    if (old) SelectObject(dc, old);
    ReleaseDC(choiceWindow, dc);
    return (std::max)(lineHeight, static_cast<int>(bounds.bottom));
}

void PaintChoices(HWND hwnd, HDC dc)
{
    RECT rect{}; GetClientRect(hwnd, &rect);
    FFXIMenu::DrawWindow(dc, rect);
    const auto menu = FFXIMenu::MakeLayout(rect, QueryPromptHeight(rect.right - 20),
        lineHeight, (std::min)(queryVisibleRows, static_cast<int>(choiceOptions.size())));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(235, 235, 245));
    HFONT oldFont = font ? (HFONT)SelectObject(dc, font) : nullptr;
    if (!choicePrompt.empty())
    {
        RECT promptRect = menu.question;
        if (officialFont)
        {
            const auto lines = WrapBitmapText(choicePrompt, promptRect.right - promptRect.left);
            for (size_t line = 0; line < lines.size(); ++line)
                FFXIBitmapFont::Draw(dc, lines[line], promptRect.left,
                    promptRect.top + static_cast<int>(line) * lineHeight, fontSize, RGB(235, 235, 245));
        }
        else DrawTextW(dc, choicePrompt.c_str(), -1, &promptRect, DT_LEFT | DT_TOP | DT_WORDBREAK);
    }
    constexpr int maxVisibleChoices = queryVisibleRows;
    const int visibleCount = (std::min)(maxVisibleChoices, (int)choiceOptions.size());
    const int firstVisible = choiceFirstVisible;
    const int lastVisible = firstVisible + visibleCount;
    for (int i = firstVisible; i < lastVisible; ++i)
    {
        RECT row = menu.Button(i - firstVisible);
        const std::wstring& text = choiceOptions[(size_t)i];
        if (officialFont)
            FFXIBitmapFont::Draw(dc, text, row.left, row.top + 1, fontSize, RGB(235, 235, 245));
        else DrawTextW(dc, text.c_str(), -1, &row, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    if (oldFont) SelectObject(dc, oldFont);
    FFXIMenuCursor::State cursor;
    cursor.scale = static_cast<float>(lineHeight) / 16.0f;
    cursor.offset.y = (std::max)(0, (lineHeight - static_cast<int>(12 * cursor.scale)) / 2);
    FFXIMenu::DrawCursor(dc, menu, choiceSelected - firstVisible, cursor);
}

LRESULT CALLBACK ChoiceProcedure(HWND hwnd, UINT message, WPARAM w, LPARAM l)
{
    if (UiBackbuffer::ConsumeNativePaint(hwnd, message)) return 0;
    switch (message)
    {
    case WM_NCDESTROY:
        if (choiceWindow == hwnd) choiceWindow = nullptr;
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
    {
        PAINTSTRUCT paint{}; HDC dc = BeginPaint(hwnd, &paint);
        PaintChoices(hwnd, dc);
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_PRINT:
        PaintChoices(hwnd, (HDC)w);
        return 0;
    }
    return DefWindowProcW(hwnd, message, w, l);
}

void EnsureChoiceWindow(HWND owner)
{
    if (choiceWindow) return;
    WNDCLASSW cls = {};
    cls.lpfnWndProc = ChoiceProcedure;
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.hCursor = LoadCursor(nullptr, IDC_ARROW);
    cls.lpszClassName = L"DATuraNpcChoice";
    RegisterClassW(&cls);
    choiceWindow = CreateWindowExW(0, cls.lpszClassName, L"Dialogue choices",
        WS_CHILD, 0, 0, 0, 0, owner, nullptr, cls.hInstance, nullptr);
}

void LayoutChoices(HWND owner)
{
    if (!owner || !choiceWindow || choiceOptions.empty()) return;
    RECT bounds{}; GetClientRect(owner, &bounds);
    RECT chat{};
    if (window)
    {
        GetWindowRect(window, &chat);
        MapWindowPoints(HWND_DESKTOP, owner, reinterpret_cast<POINT*>(&chat), 2);
    }
    const int availableWidth = (std::max)(1, (int)bounds.right - 16);
    const int width = (std::min)(availableWidth, (std::max)(220, availableWidth * widthPercent / 100));
    const int promptHeight = QueryPromptHeight(width - 20);
    const int visibleChoices = (std::min)(queryVisibleRows, (int)choiceOptions.size());
    const int height = 16 + promptHeight + (choicePrompt.empty() ? 0 : 4) + visibleChoices * lineHeight;
    const int y = window && IsWindowVisible(window)
        ? (std::max)(8, (int)chat.top - height - 6)
        : (std::max)(8, (int)bounds.bottom - height - 8);
    SetWindowPos(choiceWindow, HWND_TOP, 8, y, width, height, SWP_NOACTIVATE);
}

void ShowChoices(HWND owner, const std::string& prompt, const std::vector<std::string>& options, int selected)
{
    if (!owner || options.empty()) { HideChoices(); return; }
    EnsureChoiceWindow(owner);
    if (!choiceWindow) return;
    choicePrompt = Utf8(prompt);
    choiceOptions.clear();
    for (const std::string& option : options)
        choiceOptions.push_back(Utf8(option));
    choiceSelected = std::clamp(selected, 0, (std::max)(0, (int)choiceOptions.size() - 1));
    choiceFirstVisible = (std::max)(0, choiceSelected - queryVisibleRows + 1);
    LayoutChoices(owner);
    ShowWindow(choiceWindow, SW_SHOWNOACTIVATE);
    InvalidateRect(choiceWindow, nullptr, TRUE);
}

void SetChoiceSelection(HWND owner, int selected)
{
    if (choiceOptions.empty()) return;
    choiceSelected = std::clamp(selected, 0, (int)choiceOptions.size() - 1);
    if (choiceSelected < choiceFirstVisible) choiceFirstVisible = choiceSelected;
    if (choiceSelected >= choiceFirstVisible + queryVisibleRows)
        choiceFirstVisible = choiceSelected - queryVisibleRows + 1;
    LayoutChoices(owner);
    if (choiceWindow) InvalidateRect(choiceWindow, nullptr, TRUE);
}

void HideChoices()
{
    if (choiceWindow)
        ShowWindow(choiceWindow, SW_HIDE);
}


void DrawChoices(IDirect3DDevice9* device)
{
    UiBackbuffer::Draw(device, choiceWindow, [](HDC dc)
    {
        PaintChoices(choiceWindow, dc);
    });
}
}
