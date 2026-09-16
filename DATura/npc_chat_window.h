#pragma once
#include <windows.h>
#include <richedit.h>
#include <commctrl.h>
#include <d3d9.h>
#include <string>
#include <vector>
#include <algorithm>

// A viewport-anchored log; the read-only Rich Edit retains selection and copying.
namespace NpcChatWindow
{
inline HWND window = nullptr;
inline HWND transcript = nullptr;
inline HWND choiceWindow = nullptr;
inline std::wstring history;
inline std::wstring choicePrompt;
inline std::vector<std::wstring> choiceOptions;
inline int choiceSelected = 0;
inline HFONT font = nullptr;
inline constexpr COLORREF background = RGB(12, 18, 50);
inline int timeoutSeconds = 15;
inline ULONGLONG hideAt = 0;
inline constexpr UINT_PTR closeTimer = 1;
inline int widthPercent = 50;
inline int heightPercent = 25;
inline int compactedLines = 0;
inline int lineHeight = 19;
inline std::wstring lastEntry;
inline void Layout(HWND owner);
inline void LayoutTranscript();
inline void LayoutChoices(HWND owner);

inline void PaintStripes(HDC dc, RECT rect)
{
    HBRUSH blue = CreateSolidBrush(RGB(17, 27, 70));
    HBRUSH dark = CreateSolidBrush(background);
    for (int y = rect.top; y < rect.bottom; y += 2)
    {
        RECT stripe{rect.left, y, rect.right, (std::min)(y + 2, (int)rect.bottom)};
        FillRect(dc, &stripe, (y / 2) % 2 ? dark : blue);
    }
    DeleteObject(blue); DeleteObject(dark);
}

inline void Hide()
{
    if (!window) return;
    KillTimer(window, closeTimer);
    hideAt = 0;
    if (GetFocus() == transcript) SetFocus(GetParent(window));
    ShowWindow(window, SW_HIDE);
}

inline void HideChoices()
{
    if (choiceWindow)
        ShowWindow(choiceWindow, SW_HIDE);
}

inline void RestartTimeout()
{
    if (!window) return;
    hideAt = GetTickCount64() + timeoutSeconds * 1000ULL;
    SetTimer(window, closeTimer, timeoutSeconds * 1000, nullptr);
}

inline void SetTimeout(int seconds)
{
    timeoutSeconds = std::clamp(seconds, 1, 300);
    if (window && IsWindowVisible(window)) RestartTimeout();
}

inline void SetSize(int width, int height)
{
    widthPercent = std::clamp(width, 20, 100);
    heightPercent = std::clamp(height, 10, 75);
    compactedLines = 0;
    if (window) Layout(GetParent(window));
}

inline void Compact(bool immediately = false)
{
    if (!window || !IsWindowVisible(window)) return;
    if (!immediately && (!hideAt || GetTickCount64() < hideAt)) return;
    RECT rect{}; GetClientRect(window, &rect);
    if (rect.bottom <= lineHeight + 16) { Hide(); return; }
    ++compactedLines;
    Layout(GetParent(window));
    // Keep the most recent lines visible as the top edge moves down.
    SendMessageW(transcript, WM_VSCROLL, SB_BOTTOM, 0);
    RestartTimeout();
}

inline bool HandleEscape(LPARAM keyData)
{
    if (!window || !IsWindowVisible(window)) return false;
    // One row per physical press; holding Escape must not drain the log.
    if (!(keyData & (1LL << 30))) Compact(true);
    return true;
}

inline void PaintFrame(HWND hwnd, HDC dc)
{
    RECT rect{}; GetClientRect(hwnd, &rect);
    PaintStripes(dc, rect);
    const int rectWidth = (int)(rect.right - rect.left);
    const int fade = (std::min)(96, (std::max)(1, rectWidth / 5));
    const COLORREF edgeRows[] = { RGB(78, 86, 118), RGB(190, 197, 220), RGB(55, 62, 92) };
    for (int row = 0; row < 3 && row < rect.bottom; ++row)
    {
        const COLORREF edge = edgeRows[row];
        const int edgeR = GetRValue(edge), edgeG = GetGValue(edge), edgeB = GetBValue(edge);
        const int bgR = GetRValue(background), bgG = GetGValue(background), bgB = GetBValue(background);
        for (int x = rect.left; x < rect.right; ++x)
        {
            const int leftDistance = x - rect.left;
            const int rightDistance = rect.right - 1 - x;
            int strength = 255;
            if (leftDistance < fade)
                strength = (std::min)(strength, leftDistance * 255 / fade);
            if (rightDistance < fade)
                strength = (std::min)(strength, rightDistance * 255 / fade);
            strength = (std::max)(0, (std::min)(255, strength));
            const COLORREF color = RGB(
                (bgR * (255 - strength) + edgeR * strength) / 255,
                (bgG * (255 - strength) + edgeG * strength) / 255,
                (bgB * (255 - strength) + edgeB * strength) / 255);
            SetPixel(dc, x, rect.top + row, color);
            SetPixel(dc, x, rect.bottom - 1 - row, color);
        }
    }
}

// Present overwrites native child windows. Composite the log into the completed
// back buffer as well, so its visibility is stable in every display mode.
inline void Draw(IDirect3DDevice9* device)
{
    if (!device || !window || !IsWindowVisible(window)) return;
    Compact();
    if (!IsWindowVisible(window)) return;
    IDirect3DSurface9* surface = nullptr;
    if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &surface))) return;
    HDC dc = nullptr;
    if (SUCCEEDED(surface->GetDC(&dc)))
    {
        RECT rect{}; GetWindowRect(window, &rect);
        MapWindowPoints(HWND_DESKTOP, GetParent(window), reinterpret_cast<POINT*>(&rect), 2);
        const int saved = SaveDC(dc);
        SetViewportOrgEx(dc, rect.left, rect.top, nullptr);
        PaintFrame(window, dc);
        RECT child{}; GetWindowRect(transcript, &child);
        MapWindowPoints(HWND_DESKTOP, GetParent(window), reinterpret_cast<POINT*>(&child), 2);
        SetViewportOrgEx(dc, child.left, child.top, nullptr);
        SendMessageW(transcript, WM_PRINT, (WPARAM)dc, PRF_CLIENT | PRF_NONCLIENT | PRF_ERASEBKGND);
        if (choiceWindow && IsWindowVisible(choiceWindow))
        {
            RECT choice{}; GetWindowRect(choiceWindow, &choice);
            MapWindowPoints(HWND_DESKTOP, GetParent(window), reinterpret_cast<POINT*>(&choice), 2);
            SetViewportOrgEx(dc, choice.left, choice.top, nullptr);
            SendMessageW(choiceWindow, WM_PRINT, (WPARAM)dc, PRF_CLIENT | PRF_NONCLIENT | PRF_ERASEBKGND);
        }
        RestoreDC(dc, saved);
        surface->ReleaseDC(dc);
    }
    surface->Release();
}

inline void Layout(HWND owner)
{
    if (!window) return;
    RECT bounds{};
    GetClientRect(owner, &bounds);
    const int width = (std::max)(0L, bounds.right - 16) * widthPercent / 100;
    const int baseHeight = heightPercent == 25 ?
        (std::min)((std::max)(100L, bounds.bottom / 4), 220L) : bounds.bottom * heightPercent / 100;
    const int height = (std::max)(lineHeight + 16, baseHeight - compactedLines * lineHeight);
    SetWindowPos(window, HWND_TOP, 8, (std::max)(0L, bounds.bottom - height - 8),
        width, (std::min)(height, (int)bounds.bottom), SWP_NOACTIVATE);
    LayoutTranscript();
    LayoutChoices(owner);
}

inline void LayoutTranscript()
{
    if (!window || !transcript) return;
    RECT rect{}; GetClientRect(window, &rect);
    const int availableWidth = (std::max)(0, (int)rect.right - 16);
    const int availableHeight = (std::max)(0, (int)rect.bottom - 16);
    int lineCount = (int)SendMessageW(transcript, EM_GETLINECOUNT, 0, 0);
    if (lineCount < 1) lineCount = 1;
    const int contentHeight = lineCount * lineHeight + 4;
    const int childHeight = (std::min)(availableHeight, (std::max)(lineHeight + 4, contentHeight));
    const int childY = rect.bottom - 8 - childHeight;
    MoveWindow(transcript, 8, (std::max)(8, childY), availableWidth, childHeight, TRUE);
}

inline void PaintChoices(HWND hwnd, HDC dc)
{
    RECT rect{}; GetClientRect(hwnd, &rect);
    PaintStripes(dc, rect);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(235, 235, 245));
    HFONT oldFont = font ? (HFONT)SelectObject(dc, font) : nullptr;
    int y = 8;
    if (!choicePrompt.empty())
    {
        RECT promptRect{10, y, rect.right - 10, y + lineHeight * 2};
        DrawTextW(dc, choicePrompt.c_str(), -1, &promptRect, DT_LEFT | DT_TOP | DT_WORDBREAK);
        y = promptRect.bottom + 4;
    }
    for (int i = 0; i < (int)choiceOptions.size(); ++i)
    {
        RECT row{10, y, rect.right - 10, y + lineHeight};
        std::wstring text = (i == choiceSelected ? L"\x25ba " : L"  ") + choiceOptions[(size_t)i];
        DrawTextW(dc, text.c_str(), -1, &row, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        y += lineHeight;
    }
    if (oldFont) SelectObject(dc, oldFont);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(190, 197, 220));
    HGDIOBJ oldPen = SelectObject(dc, pen);
    MoveToEx(dc, 0, 0, nullptr); LineTo(dc, rect.right, 0);
    MoveToEx(dc, 0, rect.bottom - 1, nullptr); LineTo(dc, rect.right, rect.bottom - 1);
    SelectObject(dc, oldPen); DeleteObject(pen);
}

inline LRESULT CALLBACK ChoiceProcedure(HWND hwnd, UINT message, WPARAM w, LPARAM l)
{
    switch (message)
    {
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

inline void EnsureChoiceWindow(HWND owner)
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

inline void LayoutChoices(HWND owner)
{
    if (!choiceWindow || choiceOptions.empty()) return;
    RECT bounds{}; GetClientRect(owner, &bounds);
    RECT chat{};
    if (window)
    {
        GetWindowRect(window, &chat);
        MapWindowPoints(HWND_DESKTOP, owner, reinterpret_cast<POINT*>(&chat), 2);
    }
    const int width = (std::max)(220, (std::max)(0, (int)bounds.right - 16) * widthPercent / 100);
    const int promptLines = choicePrompt.empty() ? 0 : 2;
    const int height = 16 + (promptLines + (int)choiceOptions.size()) * lineHeight;
    const int y = window && IsWindowVisible(window)
        ? (std::max)(8, (int)chat.top - height - 6)
        : (std::max)(8, (int)bounds.bottom - height - 8);
    SetWindowPos(choiceWindow, HWND_TOP, 8, y, width, height, SWP_NOACTIVATE);
}

inline LRESULT CALLBACK TranscriptProcedure(HWND hwnd, UINT message, WPARAM w, LPARAM l,
    UINT_PTR, DWORD_PTR)
{
    if (message == WM_PAINT || message == WM_PRINT)
    {
        PAINTSTRUCT paint{};
        HDC dc = message == WM_PAINT ? BeginPaint(hwnd, &paint) : (HDC)w;
        RECT rect{}; GetClientRect(hwnd, &rect);
        if (rect.right > 0 && rect.bottom > 0)
        {
            HDC buffer = CreateCompatibleDC(dc);
            HBITMAP bitmap = CreateCompatibleBitmap(dc, rect.right, rect.bottom);
            HGDIOBJ old = SelectObject(buffer, bitmap);
            DefSubclassProc(hwnd, WM_PRINT, (WPARAM)buffer, PRF_CLIENT | PRF_ERASEBKGND);
            PaintStripes(dc, rect);
            TransparentBlt(dc, 0, 0, rect.right, rect.bottom, buffer, 0, 0,
                rect.right, rect.bottom, background);
            SelectObject(buffer, old); DeleteObject(bitmap); DeleteDC(buffer);
        }
        if (message == WM_PAINT) EndPaint(hwnd, &paint);
        return 0;
    }
    if (message == WM_KEYDOWN && w == VK_ESCAPE)
    {
        HandleEscape(l);
        return 0;
    }
    return DefSubclassProc(hwnd, message, w, l);
}

inline std::wstring Utf8(const std::string& text)
{
    const int count = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (count <= 0) return {};
    std::wstring result(count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &result[0], count);
    result.pop_back();
    return result;
}

inline LRESULT CALLBACK Procedure(HWND hwnd, UINT message, WPARAM w, LPARAM l)
{
    switch (message)
    {
    case WM_CREATE:
    {
        transcript = CreateWindowExW(0, L"RICHEDIT50W", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            0, 0, 0, 0, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!transcript) return -1;
        font = CreateFontW(-17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Arial");
        SendMessageW(transcript, WM_SETFONT, (WPARAM)font, TRUE);
        HDC dc = GetDC(transcript);
        HGDIOBJ previous = SelectObject(dc, font);
        TEXTMETRICW metrics{}; GetTextMetricsW(dc, &metrics);
        lineHeight = (std::max)(1L, metrics.tmHeight);
        SelectObject(dc, previous); ReleaseDC(transcript, dc);
        SendMessageW(transcript, EM_SETBKGNDCOLOR, 0, background);
        CHARFORMAT2W format{};
        format.cbSize = sizeof(format);
        format.dwMask = CFM_COLOR;
        format.crTextColor = RGB(235, 235, 245);
        SendMessageW(transcript, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&format);
        SendMessageW(transcript, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(2, 2));
        SendMessageW(transcript, EM_EXLIMITTEXT, 0, 60000);
        SetWindowSubclass(transcript, TranscriptProcedure, 1, 0);
        return 0;
    }
    case WM_SIZE:
        LayoutTranscript();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_TIMER:
        if (w == closeTimer) Compact();
        return 0;
    case WM_PAINT:
    {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd, &paint);
        PaintFrame(hwnd, dc);
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_CLOSE:
        Hide();
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, closeTimer);
        hideAt = 0;
        window = transcript = nullptr;
        if (font) DeleteObject(font);
        font = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, message, w, l);
}

inline void Reset()
{
    history.clear();
    lastEntry.clear();
    choicePrompt.clear();
    choiceOptions.clear();
    choiceSelected = 0;
    compactedLines = 0;
    if (window) { SetWindowTextW(transcript, L""); Hide(); }
    HideChoices();
}

inline void ShowChoices(HWND owner, const std::string& prompt, const std::vector<std::string>& options, int selected)
{
    EnsureChoiceWindow(owner);
    if (!choiceWindow) return;
    choicePrompt = Utf8(prompt);
    choiceOptions.clear();
    for (const std::string& option : options)
        choiceOptions.push_back(Utf8(option));
    choiceSelected = std::clamp(selected, 0, (std::max)(0, (int)choiceOptions.size() - 1));
    LayoutChoices(owner);
    ShowWindow(choiceWindow, SW_SHOWNOACTIVATE);
    InvalidateRect(choiceWindow, nullptr, TRUE);
}

inline void SetChoiceSelection(HWND owner, int selected)
{
    if (choiceOptions.empty()) return;
    choiceSelected = std::clamp(selected, 0, (int)choiceOptions.size() - 1);
    LayoutChoices(owner);
    if (choiceWindow) InvalidateRect(choiceWindow, nullptr, TRUE);
}

inline void Show(HWND owner, const std::string& name, const std::string& dialogue)
{
    if (!window)
    {
        static HMODULE richEdit = LoadLibraryW(L"Msftedit.dll");
        if (!richEdit) return;
        WNDCLASSW cls = {};
        cls.lpfnWndProc = Procedure;
        cls.hInstance = GetModuleHandleW(nullptr);
        cls.hCursor = LoadCursor(nullptr, IDC_ARROW);
        cls.lpszClassName = L"DATuraNpcChat";
        RegisterClassW(&cls);
        window = CreateWindowExW(0, cls.lpszClassName, L"Chat log",
            WS_CHILD | WS_CLIPCHILDREN,
            0, 0, 0, 0, owner, nullptr, cls.hInstance, nullptr);
    }
    if (!window) return;
    const std::wstring entry = Utf8(name.empty() ? "NPC" : name) + L": " +
        (dialogue.empty() ? L"[No dialogue is assigned to this NPC.]" : Utf8(dialogue));
    lastEntry = entry;
    if (!history.empty()) history += L"\r\n";
    history += entry;
    if (history.size() > 60000)
    {
        size_t cut = history.find(L'\n', history.size() - 60000);
        if (cut != std::wstring::npos && cut + 1 < history.size()) ++cut;
        else cut = history.size() - 60000;
        if (cut < history.size() && history[cut] >= 0xdc00 && history[cut] <= 0xdfff) ++cut;
        history.erase(0, cut);
    }
    compactedLines = 0;
    Layout(owner);
    SetWindowTextW(transcript, history.c_str());
    LayoutTranscript();
    SendMessageW(transcript, EM_SETSEL, history.size(), history.size());
    SendMessageW(transcript, EM_SCROLLCARET, 0, 0);
    ShowWindow(window, SW_SHOWNOACTIVATE);
    RestartTimeout();
}
}


