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
#include "query.h"
#include "ui_backbuffer.h"

// A viewport-anchored log; the read-only Rich Edit retains selection and copying.
namespace NpcChatWindow
{
inline HWND window = nullptr;
inline HWND transcript = nullptr;
inline HWND input = nullptr;
inline std::wstring history;
inline HFONT font = nullptr;
inline bool officialFont = true;
inline std::wstring fontFace = L"Arial";
inline int fontSize = 17;
inline COLORREF textColour = RGB(255, 255, 255);
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
inline std::vector<std::wstring> sentMessages;
inline int recallIndex = -1;
inline std::wstring draft;
inline int scrollLines = 0;
inline int inputScrollX = 0;
inline int InputRowHeight() { return lineHeight + 8; }
inline int InputReserve()
{
    return input && (GetWindowLongPtrW(input, GWL_STYLE) & WS_VISIBLE)
        ? InputRowHeight() + 2 : 0;
}
inline bool IsTyping() { return input && GetFocus() == input; }
inline void Layout(HWND owner);
inline void LayoutTranscript();
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

inline void ApplyTranscriptColour()
{
    if (!transcript) return;
    CHARFORMAT2W format{};
    format.cbSize = sizeof(format);
    format.dwMask = CFM_COLOR;
    format.dwEffects = 0; // Explicit color, not the system's automatic text color.
    format.crTextColor = textColour;
    SendMessageW(transcript, EM_SETCHARFORMAT, SCF_DEFAULT, (LPARAM)&format);
    SendMessageW(transcript, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&format);
}

inline void SetTextColour(COLORREF colour)
{
    textColour = colour;
    ApplyTranscriptColour();
    if (window) InvalidateRect(window, nullptr, FALSE);
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
    width = (std::max)(1, width);
    for (size_t i = 0; i < text.size(); ++i)
    {
        const wchar_t c = text[i];
        if (c == L'\r' || c == L'\n' || c == L'\x07')
        {
            lines.push_back(line);
            line.clear();
            if (c == L'\r' && i + 1 < text.size() && text[i + 1] == L'\n') ++i;
            continue;
        }
        line += c;
        if (line.size() > 1 && FFXIBitmapFont::Measure(line, fontSize).cx > width)
        {
            // Prefer the last word boundary; split an oversized word to keep
            // glyphs within the viewport. Preserve spaces rather than rebuilding words.
            size_t cut = line.find_last_of(L" \t", line.size() - 2);
            cut = cut == std::wstring::npos ? line.size() - 1 : cut + 1;
            lines.push_back(line.substr(0, cut));
            line.erase(0, cut);
        }
    }
    lines.push_back(line);
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
    parent.bottom -= InputReserve();
    const int saved = SaveDC(dc);
    POINT origin{}; GetViewportOrgEx(dc, &origin);
    SetViewportOrgEx(dc, origin.x - child.left, origin.y - child.top, nullptr);
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
    if ((input && IsWindowVisible(input)) || scrollLines > 0) return;
    if (!immediately && choiceWindow && IsWindowVisible(choiceWindow)) return;
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
    if (input && IsWindowVisible(input)) { CloseInput(); return true; }
    if (scrollLines > 0) { scrollLines = 0; RestartTimeout(); return true; }
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
    rect.bottom -= InputReserve();
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
    UiBackbuffer::Draw(device, window, [](HDC dc)
    {
        PaintFrame(window, dc);
        RECT child{}; GetWindowRect(transcript, &child);
        MapWindowPoints(HWND_DESKTOP, window, reinterpret_cast<POINT*>(&child), 2);
        SetViewportOrgEx(dc, child.left, child.top, nullptr);
        SendMessageW(transcript, WM_PRINT, (WPARAM)dc, PRF_CLIENT | PRF_NONCLIENT | PRF_ERASEBKGND);
        if (input && IsWindowVisible(input))
        {
            RECT inputRect{}; GetWindowRect(input, &inputRect);
            MapWindowPoints(HWND_DESKTOP, window, reinterpret_cast<POINT*>(&inputRect), 2);
            SetViewportOrgEx(dc, inputRect.left, inputRect.top, nullptr);
            SendMessageW(input, WM_PRINT, (WPARAM)dc, PRF_CLIENT | PRF_NONCLIENT | PRF_ERASEBKGND);
        }
    });
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
    height = (std::max)(1, (std::min)(height + InputReserve(), (int)bounds.bottom));
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
    const int inputReserve = InputReserve();
    const bool inputVisible = inputReserve > 0;
    const int inputHeight = InputRowHeight();
    const int availableHeight = (std::max)(0,
        (int)rect.bottom - 16 - inputReserve);
    int lineCount = officialFont ? static_cast<int>(WrapBitmapText(history,
        (std::max)(1, availableWidth - 4)).size()) :
        (int)SendMessageW(transcript, EM_GETLINECOUNT, 0, 0);
    if (lineCount < 1) lineCount = 1;
    const int contentHeight = lineCount * lineHeight + 4;
    const int childHeight = (std::min)(availableHeight, (std::max)(lineHeight + 4, contentHeight));
    const int childY = rect.bottom - 8 - inputReserve - childHeight;
    // Rich Edit is the interaction/copy proxy, not the bitmap scroll renderer.
    ShowScrollBar(transcript, SB_VERT, FALSE);
    MoveWindow(transcript, 8, (std::max)(8, childY), availableWidth, childHeight, FALSE);
    if (inputVisible)
        MoveWindow(input, 0, rect.bottom - inputHeight,
            rect.right, inputHeight, TRUE);
}

inline LRESULT CALLBACK TranscriptProcedure(HWND hwnd, UINT message, WPARAM w, LPARAM l,
    UINT_PTR, DWORD_PTR)
{
    if (UiBackbuffer::ConsumeNativePaint(hwnd, message)) return 0;
    if (message == WM_ERASEBKGND) return 1;
    if (officialFont && (message == WM_MOUSEWHEEL ||
        (message == WM_KEYDOWN && (w == VK_PRIOR || w == VK_NEXT))))
    {
        const int delta = message == WM_MOUSEWHEEL ?
            static_cast<short>(HIWORD(w)) / WHEEL_DELTA * 3 : (w == VK_PRIOR ? 7 : -7);
        scrollLines = (std::max)(0, scrollLines + delta);
        RestartTimeout();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    if (message == WM_PAINT || message == WM_PRINT)
    {
        PAINTSTRUCT paint{};
        HDC dc = message == WM_PAINT ? BeginPaint(hwnd, &paint) : (HDC)w;
        RECT rect{}; GetClientRect(hwnd, &rect);
        const int saved = SaveDC(dc);
        IntersectClipRect(dc, 0, 0, rect.right, rect.bottom);
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
                scrollLines = std::clamp(scrollLines, 0,
                    (std::max)(0, static_cast<int>(lines.size()) - visible));
                const int end = static_cast<int>(lines.size()) - scrollLines;
                const int first = (std::max)(0, end - visible);
                const int drawnLines = end - first;
                int y = (std::max)(2L, rect.bottom - 2 - drawnLines * lineHeight);
                for (int index = first; index < end; ++index, y += lineHeight)
                    FFXIBitmapFont::Draw(dc, lines[index], 2, y, fontSize, textColour);
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
        RestoreDC(dc, saved);
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
    ApplyTranscriptColour();
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
    if (text.find_first_not_of(L" \t\r\n") != std::wstring::npos)
    {
        if (sentMessages.empty() || sentMessages.back() != text) sentMessages.push_back(text);
        if (sentMessages.size() > 20) sentMessages.erase(sentMessages.begin());
        recallIndex = -1;
        scrollLines = 0;
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
    if (UiBackbuffer::ConsumeNativePaint(hwnd, message)) return 0;
    if (UiBackbuffer::active && message == WM_ERASEBKGND) return 1;
    if (message == WM_LBUTTONDOWN && officialFont)
    {
        SetFocus(hwnd);
        wchar_t text[512]{}; GetWindowTextW(hwnd, text, 512);
        const std::wstring value(text);
        const int x = static_cast<short>(LOWORD(l)) - 4 + inputScrollX;
        size_t index = 0;
        int previous = 0;
        for (; index < value.size(); ++index)
        {
            const int next = FFXIBitmapFont::Measure(value.substr(0, index + 1), fontSize).cx;
            if (x < (previous + next) / 2) break;
            previous = next;
        }
        SendMessageW(hwnd, EM_SETSEL, index, index);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    if (message == WM_KEYDOWN)
    {
        if (w == VK_UP || w == VK_DOWN)
        {
            if (recallIndex < 0)
            {
                wchar_t current[512]{}; GetWindowTextW(hwnd, current, 512);
                draft = current;
                recallIndex = static_cast<int>(sentMessages.size());
            }
            recallIndex = std::clamp(recallIndex + (w == VK_UP ? -1 : 1),
                0, static_cast<int>(sentMessages.size()));
            SetWindowTextW(hwnd, recallIndex == static_cast<int>(sentMessages.size()) ?
                draft.c_str() : sentMessages[recallIndex].c_str());
            SendMessageW(hwnd, EM_SETSEL, -1, -1);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (w == VK_RETURN)
        {
            if (!(l & (1LL << 30))) SubmitInput();
            return 0;
        }
        if (w == VK_ESCAPE)
        {
            CloseInput();
            return 0;
        }
    }
    if (message == WM_CHAR && (w == L'\r' || w == L'\n' || w == VK_ESCAPE))
        return 0;
    if (message == WM_ERASEBKGND)
        return 1;
    if (message == WM_PAINT || message == WM_PRINT)
    {
        PAINTSTRUCT paint{};
        HDC dc = message == WM_PAINT ? BeginPaint(hwnd, &paint) : (HDC)w;
        RECT rect{}; GetClientRect(hwnd, &rect);
        // The entry row is a separate frame below the transcript, using
        // exactly the same background and fading horizontal edges.
        PaintFrameRect(dc, rect);
        wchar_t text[512] = {};
        GetWindowTextW(hwnd, text, static_cast<int>(_countof(text)));
        const int saved = SaveDC(dc);
        IntersectClipRect(dc, 2, 3, rect.right - 2, rect.bottom - 3);
        HGDIOBJ oldFont = SelectObject(dc, font);
        const std::wstring value(text);
        auto measure = [&](int count) {
            if (officialFont) return static_cast<int>(FFXIBitmapFont::Measure(value.substr(0, count), fontSize).cx);
            SIZE size{}; GetTextExtentPoint32W(dc, value.c_str(), count, &size);
            return static_cast<int>(size.cx);
        };
        DWORD first = 0, last = 0;
        SendMessageW(hwnd, EM_GETSEL, (WPARAM)&first, (LPARAM)&last);
        const int caret = measure(static_cast<int>(last));
        const int available = (std::max)(1L, rect.right - 10);
        if (caret - inputScrollX > available) inputScrollX = caret - available;
        if (caret < inputScrollX) inputScrollX = caret;
        inputScrollX = (std::max)(0, inputScrollX);
        if (GetFocus() == hwnd && first != last)
        {
            RECT selection{4 + measure(first) - inputScrollX, 2,
                4 + measure(last) - inputScrollX, rect.bottom - 2};
            HBRUSH brush = CreateSolidBrush(RGB(60, 75, 120));
            FillRect(dc, &selection, brush); DeleteObject(brush);
        }
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, textColour);
        if (officialFont)
            FFXIBitmapFont::Draw(dc, value, 4 - inputScrollX, 3, fontSize, textColour);
        else
            TextOutW(dc, 4 - inputScrollX, 3, value.c_str(), static_cast<int>(value.size()));
        const UINT blink = GetCaretBlinkTime();
        if (GetFocus() == hwnd && (blink == INFINITE || !blink || (GetTickCount64() / blink) % 2 == 0))
        {
            RECT caretRect{4 + caret - inputScrollX, 3, 5 + caret - inputScrollX, rect.bottom - 3};
            HBRUSH brush = CreateSolidBrush(textColour);
            FillRect(dc, &caretRect, brush); DeleteObject(brush);
        }
        SelectObject(dc, oldFont);
        RestoreDC(dc, saved);
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
    if (UiBackbuffer::ConsumeNativePaint(hwnd, message)) return 0;
    switch (message)
    {
    case WM_CREATE:
    {
        transcript = CreateWindowExW(0, L"RICHEDIT50W", L"",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            0, 0, 0, 0, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!transcript) return -1;
        ApplyFont();
        HDC dc = GetDC(transcript);
        HGDIOBJ previous = SelectObject(dc, font);
        TEXTMETRICW metrics{}; GetTextMetricsW(dc, &metrics);
        lineHeight = (std::max)(1L, metrics.tmHeight);
        SelectObject(dc, previous); ReleaseDC(transcript, dc);
        SendMessageW(transcript, EM_SETBKGNDCOLOR, 0, background);
        ApplyTranscriptColour();
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
    scrollLines = 0;
    if (input) SetWindowTextW(input, L"");
    lastEntry.clear();
    choicePrompt.clear();
    choiceOptions.clear();
    choiceSelected = 0;
    choiceFirstVisible = 0;
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
    ApplyTranscriptColour();
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
    LayoutTranscript();
    inputScrollX = 0;
    recallIndex = -1;
    SendMessageW(input, EM_SETSEL, GetWindowTextLengthW(input),
        GetWindowTextLengthW(input));
    KillTimer(window, closeTimer);
    hideAt = 0;
    InvalidateRect(window, nullptr, TRUE);
    return true;
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


