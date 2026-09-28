#pragma once
#include <windows.h>
#include <richedit.h>
#include <commctrl.h>
#include <d3d9.h>
#include <string>
#include <vector>
#include <algorithm>
#include "ffxi_bitmap_font.h"
#include "ffxi_chat_assets.h"

// A viewport-anchored log; the read-only Rich Edit retains selection and copying.
namespace NpcChatWindow
{
inline HWND window = nullptr;
inline HWND transcript = nullptr;
inline HWND input = nullptr;
inline HWND choiceWindow = nullptr;
inline std::wstring history;
inline std::wstring choicePrompt;
inline std::vector<std::wstring> choiceOptions;
inline int choiceSelected = 0;
inline HFONT font = nullptr;
inline bool officialFont = true;
inline std::wstring fontFace = L"Arial";
inline int fontSize = 17;
inline constexpr COLORREF background = RGB(12, 18, 50);
inline int timeoutSeconds = 15;
inline ULONGLONG hideAt = 0;
inline constexpr UINT_PTR closeTimer = 1;
inline constexpr int retailMaxStoredLines = 100;
inline constexpr int retailDefaultVisibleLines = 8;
inline int widthPercent = 50;
inline int heightPercent = 25;
inline int compactedLines = 0;
inline int lineHeight = 19;
inline float displayedHeight = 0.0f;
inline int targetHeight = 0;
inline ULONGLONG lastResizeTick = 0;
inline bool sizeAnimating = false;
inline constexpr float resizePixelsPerSecond = 120.0f;
inline std::wstring lastEntry;
inline std::wstring inputSpeaker = L"Adventurer";
inline void Layout(HWND owner);
inline void LayoutTranscript();
inline void LayoutChoices(HWND owner);
inline void ApplyLayout(HWND owner, int height);
inline void CloseInput(bool restoreOwnerFocus = true);

inline void TrimHistoryToRetailCapacity()
{
    int lineCount = 1;
    for (const wchar_t c : history)
        if (c == L'\n') ++lineCount;
    if (lineCount <= retailMaxStoredLines) return;

    int removeLines = lineCount - retailMaxStoredLines;
    std::size_t cut = 0;
    while (removeLines-- > 0)
    {
        cut = history.find(L'\n', cut);
        if (cut == std::wstring::npos) { history.clear(); return; }
        ++cut;
    }
    history.erase(0, cut);
}

inline void ApplyFont()
{
    if (font) DeleteObject(font);
    font = CreateFontW(-fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, fontFace.c_str());
    if (transcript) SendMessageW(transcript, WM_SETFONT, (WPARAM)font, TRUE);
    if (input) SendMessageW(input, WM_SETFONT, (WPARAM)font, TRUE);
    lineHeight = fontSize + 2;
    if (window)
    {
        Layout(GetParent(window));
        InvalidateRect(window, nullptr, TRUE);
        if (transcript) InvalidateRect(transcript, nullptr, TRUE);
        if (input) InvalidateRect(input, nullptr, TRUE);
        if (choiceWindow) InvalidateRect(choiceWindow, nullptr, TRUE);
    }
}

inline void SetFont(const char* name, int size)
{
    fontSize = std::clamp(size, 10, 32);
    officialFont = !name || !*name || _stricmp(name, "FFXI") == 0;
    if (officialFont)
        fontFace = L"Arial"; // Rich Edit layout proxy; painting uses the DAT atlas.
    else
    {
        const int count = MultiByteToWideChar(CP_UTF8, 0, name, -1, nullptr, 0);
        fontFace.assign((std::max)(1, count), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, name, -1, fontFace.data(), (int)fontFace.size());
        if (!fontFace.empty() && fontFace.back() == L'\0') fontFace.pop_back();
    }
    ApplyFont();
}

inline std::vector<std::wstring> WrapBitmapText(const std::wstring& text, int width)
{
    std::vector<std::wstring> lines;
    std::wstring line;
    size_t cursor = 0;
    while (cursor <= text.size())
    {
        const size_t end = text.find_first_of(L" \r\n", cursor);
        const size_t wordEnd = end == std::wstring::npos ? text.size() : end;
        std::wstring word = text.substr(cursor, wordEnd - cursor);
        if (!word.empty() && !line.empty() &&
            FFXIBitmapFont::Measure(line + L" " + word, fontSize).cx > width)
        {
            lines.push_back(line);
            line.clear();
        }
        if (!word.empty())
        {
            if (!line.empty()) line += L' ';
            line += word;
        }
        if (end == std::wstring::npos) break;
        const bool newline = text[end] == L'\r' || text[end] == L'\n';
        if (newline) { lines.push_back(line); line.clear(); }
        cursor = end + 1;
        if (text[end] == L'\r' && cursor < text.size() && text[cursor] == L'\n') ++cursor;
    }
    if (!line.empty() || lines.empty()) lines.push_back(line);
    return lines;
}

inline void PaintStripes(HDC dc, RECT rect)
{
    if (FFXIChatAssets::PaintWindow(dc, rect)) return;
    HBRUSH blue = CreateSolidBrush(RGB(17, 27, 70));
    HBRUSH dark = CreateSolidBrush(background);
    for (int y = rect.top; y < rect.bottom; y += 2)
    {
        RECT stripe{rect.left, y, rect.right, (std::min)(y + 2, (int)rect.bottom)};
        FillRect(dc, &stripe, (y / 2) % 2 ? dark : blue);
    }
    DeleteObject(blue); DeleteObject(dark);
}

inline void PaintTranscriptBackdrop(HWND hwnd, HDC dc)
{
    if (!window) return;
    RECT child{};
    GetWindowRect(hwnd, &child);
    MapWindowPoints(HWND_DESKTOP, window, reinterpret_cast<POINT*>(&child), 2);
    RECT parent{};
    GetClientRect(window, &parent);
    const int saved = SaveDC(dc);
    SetViewportOrgEx(dc, -child.left, -child.top, nullptr);
    PaintStripes(dc, parent);
    RestoreDC(dc, saved);
}

inline void Hide()
{
    if (!window) return;
    KillTimer(window, closeTimer);
    hideAt = 0;
    displayedHeight = 0.0f;
    targetHeight = 0;
    lastResizeTick = 0;
    sizeAnimating = false;
    if (input) ShowWindow(input, SW_HIDE);
    if (GetFocus() == transcript || GetFocus() == input) SetFocus(GetParent(window));
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
    if (input && IsWindowVisible(input)) return;
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

inline void PaintFrameRect(HDC dc, RECT rect)
{
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

inline void PaintFrame(HWND hwnd, HDC dc)
{
    RECT rect{}; GetClientRect(hwnd, &rect);
    PaintFrameRect(dc, rect);
}

// Present overwrites native child windows. Composite the log into the completed
// back buffer as well, so its visibility is stable in every display mode.
inline void Draw(IDirect3DDevice9* device)
{
    if (!device || !window || !IsWindowVisible(window)) return;
    Compact();
    if (!IsWindowVisible(window)) return;
    if (sizeAnimating)
    {
        const ULONGLONG now = GetTickCount64();
        const float elapsed = lastResizeTick == 0 ? 0.0f :
            (std::min)(0.05f, static_cast<float>(now - lastResizeTick) / 1000.0f);
        lastResizeTick = now;
        const float step = resizePixelsPerSecond * elapsed;
        if (displayedHeight < targetHeight)
            displayedHeight = (std::min)(static_cast<float>(targetHeight), displayedHeight + step);
        else
            displayedHeight = (std::max)(static_cast<float>(targetHeight), displayedHeight - step);
        if (displayedHeight == static_cast<float>(targetHeight))
        {
            sizeAnimating = false;
            lastResizeTick = 0;
        }
        ApplyLayout(GetParent(window), static_cast<int>(displayedHeight + 0.5f));
    }
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
        if (input && IsWindowVisible(input))
        {
            RECT inputRect{}; GetWindowRect(input, &inputRect);
            MapWindowPoints(HWND_DESKTOP, GetParent(window),
                reinterpret_cast<POINT*>(&inputRect), 2);
            SetViewportOrgEx(dc, inputRect.left, inputRect.top, nullptr);
            SendMessageW(input, WM_PRINT, (WPARAM)dc,
                PRF_CLIENT | PRF_NONCLIENT | PRF_ERASEBKGND);
        }
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
    const int baseHeight = heightPercent == 25 ?
        retailDefaultVisibleLines * lineHeight + 16 : bounds.bottom * heightPercent / 100;
    const int newTarget = (std::max)(lineHeight + 16, baseHeight - compactedLines * lineHeight);
    const bool canAnimate = IsWindowVisible(window) && displayedHeight > 0.0f;
    targetHeight = newTarget;
    if (!canAnimate)
    {
        displayedHeight = static_cast<float>(targetHeight);
        sizeAnimating = false;
        lastResizeTick = 0;
    }
    else if (static_cast<int>(displayedHeight + 0.5f) != targetHeight)
    {
        sizeAnimating = true;
        lastResizeTick = GetTickCount64();
    }
    ApplyLayout(owner, static_cast<int>(displayedHeight + 0.5f));
}

inline void ApplyLayout(HWND owner, int height)
{
    if (!window || !owner) return;
    RECT bounds{};
    GetClientRect(owner, &bounds);
    const int width = (std::max)(0L, bounds.right - 16) * widthPercent / 100;
    height = (std::max)(1, (std::min)(height, (int)bounds.bottom));
    SetWindowPos(window, HWND_TOP, 8, (std::max)(0L, bounds.bottom - height - 8),
        width, height, SWP_NOACTIVATE);
    LayoutTranscript();
    LayoutChoices(owner);
}

inline void LayoutTranscript()
{
    if (!window || !transcript) return;
    RECT rect{}; GetClientRect(window, &rect);
    const int availableWidth = (std::max)(0, (int)rect.right - 16);
    const bool inputVisible = input && IsWindowVisible(input);
    const int inputHeight = lineHeight + 6;
    const int inputReserve = inputVisible ? inputHeight + 2 : 0;
    const int availableHeight = (std::max)(0,
        (int)rect.bottom - 16 - inputReserve);
    int lineCount = (int)SendMessageW(transcript, EM_GETLINECOUNT, 0, 0);
    if (lineCount < 1) lineCount = 1;
    const int contentHeight = lineCount * lineHeight + 4;
    const int childHeight = (std::min)(availableHeight, (std::max)(lineHeight + 4, contentHeight));
    const int childY = rect.bottom - 8 - inputReserve - childHeight;
    MoveWindow(transcript, 8, (std::max)(8, childY), availableWidth, childHeight, TRUE);
    if (inputVisible)
        MoveWindow(input, 8, rect.bottom - 8 - inputHeight,
            availableWidth, inputHeight, TRUE);
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
        if (officialFont)
        {
            const auto lines = WrapBitmapText(choicePrompt, promptRect.right - promptRect.left);
            for (size_t line = 0; line < lines.size() && line < 2; ++line)
                FFXIBitmapFont::Draw(dc, lines[line], promptRect.left,
                    promptRect.top + static_cast<int>(line) * lineHeight, fontSize, RGB(235, 235, 245));
        }
        else DrawTextW(dc, choicePrompt.c_str(), -1, &promptRect, DT_LEFT | DT_TOP | DT_WORDBREAK);
        y = promptRect.bottom + 4;
    }
    constexpr int maxVisibleChoices = 12;
    const int visibleCount = (std::min)(maxVisibleChoices, (int)choiceOptions.size());
    const int firstVisible = (std::clamp)(choiceSelected - visibleCount / 2, 0,
        (std::max)(0, (int)choiceOptions.size() - visibleCount));
    const int lastVisible = firstVisible + visibleCount;
    for (int i = firstVisible; i < lastVisible; ++i)
    {
        RECT row{10, y, rect.right - 10, y + lineHeight};
        std::wstring text = (i == choiceSelected ? L"> " : L"  ") + choiceOptions[(size_t)i];
        if (officialFont)
            FFXIBitmapFont::Draw(dc, text, row.left, row.top + 1, fontSize, RGB(235, 235, 245));
        else DrawTextW(dc, text.c_str(), -1, &row, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
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
    const int visibleChoices = (std::min)(12, (int)choiceOptions.size());
    const int height = 16 + (promptLines + visibleChoices) * lineHeight;
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
            // WM_PRINT is composited after the parent frame and needs only the
            // glyphs. Native child painting receives the matching slice of the
            // one continuous parent surface.
            if (message == WM_PAINT)
                PaintTranscriptBackdrop(hwnd, dc);
            if (officialFont)
            {
                const auto lines = WrapBitmapText(history, (std::max)(1L, rect.right - 4));
                const int visible = (std::max)(1L, (rect.bottom - 4) / lineHeight);
                const int first = (std::max)(0, static_cast<int>(lines.size()) - visible);
                const int drawnLines = static_cast<int>(lines.size()) - first;
                int y = (std::max)(2L, rect.bottom - 2 - drawnLines * lineHeight);
                for (int index = first; index < static_cast<int>(lines.size()); ++index, y += lineHeight)
                    FFXIBitmapFont::Draw(dc, lines[index], 2, y, fontSize, RGB(235, 235, 245));
            }
            else
            {
                HDC buffer = CreateCompatibleDC(dc);
                HBITMAP bitmap = CreateCompatibleBitmap(dc, rect.right, rect.bottom);
                HGDIOBJ old = SelectObject(buffer, bitmap);
                DefSubclassProc(hwnd, WM_PRINT, (WPARAM)buffer, PRF_CLIENT | PRF_ERASEBKGND);
                TransparentBlt(dc, 0, 0, rect.right, rect.bottom, buffer, 0, 0,
                    rect.right, rect.bottom, background);
                SelectObject(buffer, old); DeleteObject(bitmap); DeleteDC(buffer);
            }
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

inline void RefreshTranscript(HWND owner)
{
    if (!window || !transcript) return;
    compactedLines = 0;
    Layout(owner);
    SetWindowTextW(transcript, history.c_str());
    LayoutTranscript();
    SendMessageW(transcript, EM_SETSEL, history.size(), history.size());
    SendMessageW(transcript, EM_SCROLLCARET, 0, 0);
    ShowWindow(window, SW_SHOWNOACTIVATE);
    RestartTimeout();
    InvalidateRect(window, nullptr, TRUE);
}

inline void CloseInput(const bool restoreOwnerFocus)
{
    if (!input || !IsWindowVisible(input)) return;
    HWND owner = window ? GetParent(window) : nullptr;
    ShowWindow(input, SW_HIDE);
    SetWindowTextW(input, L"");
    if (window && owner)
    {
        Layout(owner);
        RestartTimeout();
        InvalidateRect(window, nullptr, TRUE);
    }
    if (restoreOwnerFocus && owner)
        SetFocus(owner);
}

inline void SubmitInput()
{
    if (!input || !window) return;
    const int length = GetWindowTextLengthW(input);
    std::wstring text(static_cast<size_t>((std::max)(0, length)) + 1, L'\0');
    if (length > 0)
        GetWindowTextW(input, text.data(), length + 1);
    text.resize(static_cast<size_t>((std::max)(0, length)));
    HWND owner = GetParent(window);
    CloseInput(false);
    if (!text.empty())
    {
        const std::wstring entry = inputSpeaker + L": " + text;
        lastEntry = entry;
        if (!history.empty()) history += L"\r\n";
        history += entry;
        TrimHistoryToRetailCapacity();
        RefreshTranscript(owner);
    }
    if (owner) SetFocus(owner);
}

inline LRESULT CALLBACK InputProcedure(HWND hwnd, UINT message, WPARAM w, LPARAM l,
    UINT_PTR, DWORD_PTR)
{
    if (message == WM_KEYDOWN)
    {
        if (w == VK_RETURN)
        {
            SubmitInput();
            return 0;
        }
        if (w == VK_ESCAPE)
        {
            CloseInput();
            return 0;
        }
    }
    if (message == WM_CHAR && (w == L'\r' || w == L'\n'))
        return 0;
    if (message == WM_ERASEBKGND)
        return 1;
    if (message == WM_PAINT || message == WM_PRINT)
    {
        PAINTSTRUCT paint{};
        HDC dc = message == WM_PAINT ? BeginPaint(hwnd, &paint) : (HDC)w;
        RECT rect{}; GetClientRect(hwnd, &rect);
        // Back-buffer composition has already painted the complete parent
        // surface. Native child painting needs the matching local slice.
        if (message == WM_PAINT)
            PaintTranscriptBackdrop(hwnd, dc);
        wchar_t text[512] = {};
        GetWindowTextW(hwnd, text, static_cast<int>(_countof(text)));
        SetBkMode(dc, TRANSPARENT);
        if (officialFont)
            FFXIBitmapFont::Draw(dc, text, 4, 3, fontSize, RGB(235, 235, 245));
        else
        {
            HGDIOBJ oldFont = font ? SelectObject(dc, font) : nullptr;
            SetTextColor(dc, RGB(235, 235, 245));
            RECT textRect{4, 1, rect.right - 4, rect.bottom - 1};
            DrawTextW(dc, text, -1, &textRect,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            if (oldFont) SelectObject(dc, oldFont);
        }
        if (GetFocus() == hwnd)
        {
            DWORD selection = static_cast<DWORD>(SendMessageW(hwnd, EM_GETSEL, 0, 0));
            const int caretCharacter = LOWORD(selection);
            std::wstring prefix(text, text + (std::min)(caretCharacter,
                static_cast<int>(wcslen(text))));
            const int caretX = 4 + (officialFont
                ? FFXIBitmapFont::Measure(prefix, fontSize).cx
                : LOWORD(SendMessageW(hwnd, EM_POSFROMCHAR, caretCharacter, 0)));
            HPEN caretPen = CreatePen(PS_SOLID, 1, RGB(235, 235, 245));
            HGDIOBJ oldPen = SelectObject(dc, caretPen);
            MoveToEx(dc, (std::min)(caretX, (int)rect.right - 2), 3, nullptr);
            LineTo(dc, (std::min)(caretX, (int)rect.right - 2), rect.bottom - 3);
            SelectObject(dc, oldPen);
            DeleteObject(caretPen);
        }
        if (message == WM_PAINT) EndPaint(hwnd, &paint);
        return 0;
    }
    const LRESULT result = DefSubclassProc(hwnd, message, w, l);
    if (message == WM_CHAR || message == WM_KEYUP || message == WM_LBUTTONUP)
        InvalidateRect(hwnd, nullptr, FALSE);
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
        ApplyFont();
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
        input = CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | ES_AUTOHSCROLL,
            0, 0, 0, 0, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!input) return -1;
        SendMessageW(input, WM_SETFONT, (WPARAM)font, FALSE);
        SendMessageW(input, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
            MAKELPARAM(3, 3));
        SendMessageW(input, EM_SETLIMITTEXT, 500, 0);
        SetWindowSubclass(input, InputProcedure, 2, 0);
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
        window = transcript = input = nullptr;
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

inline bool Reopen(HWND owner)
{
    if (!window || !owner || history.empty() || IsWindowVisible(window))
        return false;

    compactedLines = 0;
    Layout(owner);
    SetWindowTextW(transcript, history.c_str());
    LayoutTranscript();
    SendMessageW(transcript, EM_SETSEL, history.size(), history.size());
    SendMessageW(transcript, EM_SCROLLCARET, 0, 0);
    ShowWindow(window, SW_SHOWNOACTIVATE);
    RestartTimeout();
    InvalidateRect(window, nullptr, TRUE);
    return true;
}

inline bool EnsureWindow(HWND owner)
{
    if (window) return true;
    if (!owner) return false;
    static HMODULE richEdit = LoadLibraryW(L"Msftedit.dll");
    if (!richEdit) return false;
    WNDCLASSW cls = {};
    cls.lpfnWndProc = Procedure;
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.hCursor = LoadCursor(nullptr, IDC_ARROW);
    cls.lpszClassName = L"DATuraNpcChat";
    RegisterClassW(&cls);
    window = CreateWindowExW(0, cls.lpszClassName, L"Chat log",
        WS_CHILD | WS_CLIPCHILDREN,
        0, 0, 0, 0, owner, nullptr, cls.hInstance, nullptr);
    return window != nullptr;
}

inline bool OpenInput(HWND owner, const char* speaker)
{
    if (!EnsureWindow(owner) || !input) return false;
    inputSpeaker = Utf8(speaker && *speaker ? speaker : "Adventurer");
    compactedLines = 0;
    ShowWindow(input, SW_SHOWNOACTIVATE);
    Layout(owner);
    ShowWindow(window, SW_SHOWNOACTIVATE);
    SetFocus(input);
    SendMessageW(input, EM_SETSEL, GetWindowTextLengthW(input),
        GetWindowTextLengthW(input));
    KillTimer(window, closeTimer);
    hideAt = 0;
    InvalidateRect(window, nullptr, TRUE);
    return true;
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
    if (!EnsureWindow(owner)) return;
    const std::wstring message =
        dialogue.empty() ? L"[No dialogue is assigned to this NPC.]" : Utf8(dialogue);
    const std::wstring entry = name == "System" ? message :
        Utf8(name.empty() ? "NPC" : name) + L": " + message;
    lastEntry = entry;
    if (!history.empty()) history += L"\r\n";
    history += entry;
    TrimHistoryToRetailCapacity();
    RefreshTranscript(owner);
}
}


