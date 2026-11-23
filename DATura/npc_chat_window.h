#pragma once
#include <windows.h>
#include <richedit.h>
#include <commctrl.h>
#include <d3d9.h>
#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include "ffxi_bitmap_font.h"
#include "ffxi_chat_assets.h"
#include "query.h"
#include "ui_backbuffer.h"

// A viewport-anchored log; the read-only Rich Edit retains selection and copying.
//
// Window behaviour follows the PS2 client's CTkDrawMessageWindow/CTkMsgWinData
// (main/tk/TkMsgWin.cpp, TkMsgWinData.cpp):
//  * The log keeps a "line budget" (m_drawStringNeckTime). Each new wrapped
//    line adds one timeout period, capped at the configured maximum, and the
//    window shows ceil(budget / timeout) rows. The budget drains in real time,
//    so the window shrinks one row per timeout period instead of collapsing
//    or re-expanding to full height when a message arrives.
//  * New lines scroll in from the bottom at 20 frames (1/3 s) per line, 20x
//    faster when more than four lines are queued, and jump when over 64.
//  * The buffer holds 100 wrapped display lines, not 100 messages.
//  * Chat continuation lines are indented; NPC and system text is not.
//  * While text input is open the log stops draining at two rows. While the
//    player reads back through the log it stays at full height and new lines
//    do not move the view.
namespace NpcChatWindow
{
struct LogEntry
{
    std::wstring text;
    bool indentContinuation = false;
};

inline HWND window = nullptr;
inline HWND transcript = nullptr;
inline HWND input = nullptr;
inline std::vector<LogEntry> entries;
inline std::wstring history; // Plain-text mirror of entries for Rich Edit copy/selection.
inline HFONT font = nullptr;
inline bool officialFont = true;
inline std::wstring fontFace = L"Arial";
inline int fontSize = 17;
inline COLORREF textColour = RGB(255, 255, 255);
inline constexpr COLORREF background = RGB(12, 18, 50);
inline int timeoutSeconds = 15;
inline constexpr UINT_PTR closeTimer = 1;
inline constexpr UINT tickIntervalMs = 50;
inline constexpr int retailMaxStoredLines = 100;
inline constexpr int retailDefaultVisibleLines = 8;
inline constexpr double retailScrollMsPerLine = 1000.0 / 3.0; // 20 frames at 60 fps.
inline constexpr int retailFastScrollBacklog = 4;   // More queued lines scroll 20x faster.
inline constexpr int retailJumpScrollBacklog = 64;  // More than this jumps to the end.
inline constexpr int retailWordBreakSearch = 24;    // Longest word moved to the next line.
inline const wchar_t* const chatContinuationIndent = L"  ";
inline int widthPercent = 50;
inline int heightPercent = 25;
inline int maxVisibleLines = retailDefaultVisibleLines;
inline double lineBudgetMs = 0.0;      // Retail m_drawStringNeckTime.
inline double pendingScrollLines = 0.0; // Retail m_scrollNeckTime / 20.
inline ULONGLONG lastTick = 0;
inline int lineHeight = 19;
inline int wrapWidth = 0;
inline float displayedHeight = 0.0f;
inline int targetHeight = 0;
inline ULONGLONG lastResizeTick = 0;
inline bool sizeAnimating = false;
inline std::wstring lastEntry;
inline std::wstring inputSpeaker = L"Adventurer";
inline std::vector<std::wstring> sentMessages;
inline int recallIndex = -1;
inline std::wstring draft;
inline int scrollLines = 0;
inline int inputScrollX = 0;
inline int InputRowHeight() { return lineHeight + 8; }
inline bool InputVisible()
{
    return input && (GetWindowLongPtrW(input, GWL_STYLE) & WS_VISIBLE);
}
inline int InputReserve() { return InputVisible() ? InputRowHeight() + 2 : 0; }
inline bool IsTyping() { return input && GetFocus() == input; }
// Where the entry row is shown, in chat-window client coordinates. The native
// Edit control itself is parked outside the window (see LayoutTranscript), so
// its own white, system-font drawing can never reach the screen.
inline RECT InputRowRect()
{
    RECT rect{};
    if (window) GetClientRect(window, &rect);
    return {0, rect.bottom - InputRowHeight(), rect.right, rect.bottom};
}
inline constexpr int parkedInputY = -30000;
inline void Layout(HWND owner);
inline void LayoutTranscript();
inline void ApplyLayout(HWND owner, int height);
inline void CloseInput(bool restoreOwnerFocus = true);
inline void Hide();

inline double EraseMs() { return (std::max)(1, timeoutSeconds) * 1000.0; }
inline int VisibleLines()
{
    if (lineBudgetMs <= 0.0) return 0;
    return static_cast<int>(std::ceil(lineBudgetMs / EraseMs() - 1e-9));
}
inline void RoundBudgetUpToLine()
{
    const double erase = EraseMs();
    lineBudgetMs = (std::max)(0.0, std::ceil(lineBudgetMs / erase - 1e-9) * erase);
}
inline void ClampBudget()
{
    lineBudgetMs = std::clamp(lineBudgetMs, 0.0, maxVisibleLines * EraseMs());
}
// Retail AddLine: one timeout period per new line, capped, rounded up to whole rows.
inline void AddBudgetLines(int lines)
{
    if (lines <= 0) return;
    lineBudgetMs = (std::max)(0.0, lineBudgetMs) + lines * EraseMs();
    ClampBudget();
    RoundBudgetUpToLine();
}
// Retail manual decrement (one row per key press).
inline void DropBudgetLine()
{
    lineBudgetMs -= EraseMs();
    if (lineBudgetMs < 0.0) lineBudgetMs = 0.0;
    RoundBudgetUpToLine();
}
// Retail recalcStringNeckTime_toMAX: an active (browsed) log is full height.
inline void FillBudget() { lineBudgetMs = maxVisibleLines * EraseMs(); }

inline void RebuildHistory()
{
    history.clear();
    for (const LogEntry& entry : entries)
    {
        if (!history.empty()) history += L"\r\n";
        history += entry.text;
    }
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

// Retail SetOneLineSqdata: break at the last space within 24 characters of the
// overflow, otherwise split the word. Continuation lines of chat messages start
// with an indent, including lines after an explicit line break.
inline std::vector<std::wstring> WrapBitmapText(const std::wstring& text, int width,
    const std::wstring& continuationIndent = std::wstring())
{
    std::vector<std::wstring> lines;
    std::wstring line;
    width = (std::max)(1, width);
    const size_t indentLength = continuationIndent.size();
    for (size_t i = 0; i < text.size(); ++i)
    {
        const wchar_t c = text[i];
        if (c == L'\r' || c == L'\n' || c == L'\x07')
        {
            lines.push_back(line);
            line = continuationIndent;
            if (c == L'\r' && i + 1 < text.size() && text[i + 1] == L'\n') ++i;
            continue;
        }
        line += c;
        const size_t prefix = lines.empty() ? 0 : indentLength;
        if (line.size() > prefix + 1 && FFXIBitmapFont::Measure(line, fontSize).cx > width)
        {
            // Spaces stay at the end of the line they follow.
            size_t cut = line.find_last_of(L" \t", line.size() - 2);
            if (cut == std::wstring::npos || cut < prefix ||
                line.size() - 1 - cut > static_cast<size_t>(retailWordBreakSearch))
                cut = line.size() - 1;
            else
                ++cut;
            const std::wstring rest = line.substr(cut);
            lines.push_back(line.substr(0, cut));
            line = continuationIndent + rest;
        }
    }
    lines.push_back(line);
    return lines;
}

inline std::vector<std::wstring> WrapEntry(const LogEntry& entry, int width)
{
    return WrapBitmapText(entry.text, width,
        entry.indentContinuation ? std::wstring(chatContinuationIndent) : std::wstring());
}

inline std::vector<std::wstring> WrapHistory(int width)
{
    std::vector<std::wstring> lines;
    for (const LogEntry& entry : entries)
    {
        std::vector<std::wstring> wrapped = WrapEntry(entry, width);
        lines.insert(lines.end(), wrapped.begin(), wrapped.end());
    }
    return lines;
}

inline int WrapWidth()
{
    if (wrapWidth > 0) return wrapWidth;
    return 400; // Before the first layout; corrected on the next one.
}

inline int WrappedLineCount(const LogEntry& entry)
{
    if (!officialFont)
    {
        int count = 1;
        for (const wchar_t c : entry.text) if (c == L'\n') ++count;
        return count;
    }
    return static_cast<int>(WrapEntry(entry, WrapWidth()).size());
}

// The retail ring buffer holds 100 display lines; drop the oldest messages
// until the wrapped total fits.
inline void TrimHistoryToRetailCapacity()
{
    int total = 0;
    std::vector<int> counts;
    counts.reserve(entries.size());
    for (const LogEntry& entry : entries)
    {
        counts.push_back(WrappedLineCount(entry));
        total += counts.back();
    }
    size_t remove = 0;
    while (total > retailMaxStoredLines && remove + 1 < entries.size())
        total -= counts[remove++];
    if (remove) entries.erase(entries.begin(), entries.begin() + remove);
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

// The scan-line pattern repeats every 4 px. Anchor it to the window's fixed
// bottom edge so it does not flicker while the top edge animates.
inline void PaintStripesBottomAnchored(HDC dc, RECT rect)
{
    const int height = (int)(rect.bottom - rect.top);
    if (height <= 0) return;
    const int saved = SaveDC(dc);
    IntersectClipRect(dc, rect.left, rect.top, rect.right, rect.bottom);
    RECT aligned = rect;
    aligned.top = rect.bottom - (height + 3) / 4 * 4;
    PaintStripes(dc, aligned);
    RestoreDC(dc, saved);
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
    PaintStripesBottomAnchored(dc, parent);
    RestoreDC(dc, saved);
}

inline void Hide()
{
    if (!window) return;
    KillTimer(window, closeTimer);
    lineBudgetMs = 0.0;
    pendingScrollLines = 0.0;
    lastTick = 0;
    displayedHeight = 0.0f;
    targetHeight = 0;
    lastResizeTick = 0;
    sizeAnimating = false;
    if (input) ShowWindow(input, SW_HIDE);
    if (GetFocus() == transcript || GetFocus() == input) SetFocus(GetParent(window));
    ShowWindow(window, SW_HIDE);
}

inline void StartTicking()
{
    if (!window) return;
    lastTick = GetTickCount64();
    SetTimer(window, closeTimer, tickIntervalMs, nullptr);
}

inline void SetTimeout(int seconds)
{
    timeoutSeconds = std::clamp(seconds, 1, 300);
    // Retail SetTimeForDECREMENTLINE recalculates the budget to the maximum.
    if (window && IsWindowVisible(window) && !entries.empty())
    {
        FillBudget();
        Layout(GetParent(window));
    }
}

inline void SetSize(int width, int height)
{
    widthPercent = std::clamp(width, 20, 100);
    heightPercent = std::clamp(height, 10, 75);
    if (window) Layout(GetParent(window));
}

inline bool DrainPaused()
{
    // Retail holds the log while an event/query window is up, and stops it
    // shrinking below two rows while the text input is open. Reading back
    // through the log keeps it open as well.
    if (choiceWindow && IsWindowVisible(choiceWindow)) return true;
    if (scrollLines > 0) return true;
    if (InputVisible() && lineBudgetMs <= 2.0 * EraseMs()) return true;
    return false;
}

// Advance the log clock by elapsedMs. Returns false once the log has hidden.
inline bool Advance(double elapsedMs)
{
    if (!window || !IsWindowVisible(window)) return false;
    const int before = VisibleLines();
    if (!DrainPaused())
        lineBudgetMs = (std::max)(0.0, lineBudgetMs - elapsedMs);
    if (pendingScrollLines > 0.0)
    {
        const double speed = std::ceil(pendingScrollLines) > retailFastScrollBacklog ? 20.0 : 1.0;
        pendingScrollLines = (std::max)(0.0,
            pendingScrollLines - speed * elapsedMs / retailScrollMsPerLine);
    }
    const int after = VisibleLines();
    if (after == 0 && !InputVisible()) { Hide(); return false; }
    if (after != before) Layout(GetParent(window));
    return true;
}

inline bool Tick()
{
    if (!window || !IsWindowVisible(window)) return false;
    const ULONGLONG now = GetTickCount64();
    const double elapsed = lastTick == 0 ? 0.0 :
        (std::min)(250.0, static_cast<double>(now - lastTick));
    lastTick = now;
    return Advance(elapsed);
}

inline bool HandleEscape(LPARAM keyData)
{
    if (!window || !IsWindowVisible(window)) return false;
    if (InputVisible()) { CloseInput(); return true; }
    if (scrollLines > 0)
    {
        scrollLines = 0;
        lastTick = GetTickCount64();
        if (transcript) InvalidateRect(transcript, nullptr, FALSE);
        return true;
    }
    // One row per physical press; holding Escape must not drain the log.
    if (keyData & (1LL << 30)) return true;
    DropBudgetLine();
    if (VisibleLines() == 0) { Hide(); return true; }
    Layout(GetParent(window));
    return true;
}

inline void PaintFrameRect(HDC dc, RECT rect, bool bottomAnchored = false)
{
    if (bottomAnchored) PaintStripesBottomAnchored(dc, rect);
    else PaintStripes(dc, rect);
    const int rectWidth = (int)(rect.right - rect.left);
    const int fade = (std::min)(96, (std::max)(1, rectWidth / 5));
    const COLORREF edgeRows[] = { RGB(78, 86, 118), RGB(190, 197, 220), RGB(55, 62, 92) };
    for (int row = 0; row < 3 && row < rect.bottom - rect.top; ++row)
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
    if (rect.bottom > rect.top) PaintFrameRect(dc, rect, true);
}

// Paint the log's surfaces with every coordinate shifted down by offsetY.
inline void PaintWindowContents(HDC dc, int offsetY)
{
    SetViewportOrgEx(dc, 0, offsetY, nullptr);
    PaintFrame(window, dc);
    RECT child{}; GetWindowRect(transcript, &child);
    MapWindowPoints(HWND_DESKTOP, window, reinterpret_cast<POINT*>(&child), 2);
    if (child.bottom > child.top)
    {
        SetViewportOrgEx(dc, child.left, child.top + offsetY, nullptr);
        SendMessageW(transcript, WM_PRINT, (WPARAM)dc, PRF_CLIENT | PRF_NONCLIENT | PRF_ERASEBKGND);
    }
    if (input && IsWindowVisible(input))
    {
        const RECT inputRect = InputRowRect();
        SetViewportOrgEx(dc, inputRect.left, inputRect.top + offsetY, nullptr);
        SendMessageW(input, WM_PRINT, (WPARAM)dc, PRF_CLIENT | PRF_NONCLIENT | PRF_ERASEBKGND);
    }
}

// Like UiBackbuffer::Draw, but the stretch from window pixels to back-buffer
// pixels never changes while the log resizes. The log is painted into a bitmap
// of its full (maximum) height, bottom-aligned, which is always stretched by
// the same amount; only the rows inside the current window are let through.
// Stretching a bitmap whose height changed every frame resampled the text rows
// differently each frame, which made the text bob up and down.
inline void CompositeBottomAnchored(IDirect3DDevice9* device)
{
    if (!device || !window || !IsWindowVisible(window)) return;
    HWND owner = GetParent(window);
    RECT client{}, rect{};
    GetClientRect(owner, &client);
    GetWindowRect(window, &rect);
    MapWindowPoints(HWND_DESKTOP, owner, reinterpret_cast<POINT*>(&rect), 2);
    const int width = rect.right - rect.left, height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0 || client.right <= 0 || client.bottom <= 0) return;
    const int fixedHeight = (std::max)(height,
        maxVisibleLines * lineHeight + 16 + InputReserve());
    const int offsetY = fixedHeight - height;
    std::unique_ptr<RendererBackend::OverlayContext> overlay(D3D9Graphics::AcquireOverlay(device));
    if (!overlay) return;
    HDC target = overlay->DeviceContext();
    HDC memory = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, width, fixedHeight);
    if (memory && bitmap)
    {
        HGDIOBJ previous = SelectObject(memory, bitmap);
        const bool ready = UiBackbuffer::PaintLocal(memory,
            [offsetY](HDC dc) { PaintWindowContents(dc, offsetY); });
        const int surfaceWidth = static_cast<int>(overlay->Width());
        const int surfaceHeight = static_cast<int>(overlay->Height());
        const int destLeft = MulDiv(rect.left, surfaceWidth, client.right);
        const int destRight = MulDiv(rect.right, surfaceWidth, client.right);
        const int destBottom = MulDiv(rect.bottom, surfaceHeight, client.bottom);
        const int destVisibleTop = MulDiv(rect.top, surfaceHeight, client.bottom);
        const int destFullHeight = MulDiv(fixedHeight, surfaceHeight, client.bottom);
        const int saved = SaveDC(target);
        if (saved)
        {
            IntersectClipRect(target, destLeft, destVisibleTop, destRight, destBottom);
            SetStretchBltMode(target, COLORONCOLOR);
            const BOOL painted = ready && StretchBlt(target, destLeft, destBottom - destFullHeight,
                destRight - destLeft, destFullHeight, memory, 0, 0, width, fixedHeight, SRCCOPY);
            if (painted) UiBackbuffer::active = true;
            RestoreDC(target, saved);
        }
        SelectObject(memory, previous);
    }
    if (bitmap) DeleteObject(bitmap);
    if (memory) DeleteDC(memory);
}

// Present overwrites native child windows. Composite the log into the completed
// back buffer as well, so its visibility is stable in every display mode.
inline void Draw(IDirect3DDevice9* device)
{
    if (!device || !window || !IsWindowVisible(window)) return;
    if (!Tick()) return;
    if (sizeAnimating)
    {
        const ULONGLONG now = GetTickCount64();
        const float elapsed = lastResizeTick == 0 ? 0.0f :
            (std::min)(0.05f, static_cast<float>(now - lastResizeTick) / 1000.0f);
        lastResizeTick = now;
        // Retail resizes 2 px/frame within one 16 px row and 8 px/frame for
        // larger changes (60 fps), scaled here to the current row height.
        const float scale = static_cast<float>(lineHeight) / 16.0f;
        const float remaining = std::fabs(static_cast<float>(targetHeight) - displayedHeight);
        const float pixelsPerSecond = (remaining > lineHeight ? 480.0f : 120.0f) * scale;
        const float step = pixelsPerSecond * elapsed;
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
    CompositeBottomAnchored(device);
}

inline void Layout(HWND owner)
{
    if (!window) return;
    RECT bounds{};
    GetClientRect(owner, &bounds);
    maxVisibleLines = heightPercent == 25 ? retailDefaultVisibleLines :
        (std::max)(1, (int)(bounds.bottom * heightPercent / 100 - 16) / (std::max)(1, lineHeight));
    ClampBudget();
    // Retail: window height = 8-row frame - (8 - rows) * row height.
    const int rows = VisibleLines();
    const int newTarget = rows > 0 ? rows * lineHeight + 16 : 0;
    const bool canAnimate = IsWindowVisible(window) && (displayedHeight > 0.0f || newTarget > 0);
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
        if (!lastResizeTick) lastResizeTick = GetTickCount64();
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
    // The log is only ever composited into the back buffer. Moving its native
    // windows must not copy their old screen pixels or repaint the uncovered
    // scene: Windows otherwise blits the last presented log image to the new
    // position every frame of a resize, and the text visibly jumps between
    // the copied and the freshly presented copy.
    SetWindowPos(window, HWND_TOP, 8, (std::max)(0L, bounds.bottom - height - 8),
        width, height, SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOREDRAW);
    LayoutTranscript();
    LayoutChoices(owner);
}

inline void LayoutTranscript()
{
    if (!window || !transcript) return;
    RECT rect{}; GetClientRect(window, &rect);
    const int availableWidth = (std::max)(0, (int)rect.right - 16);
    if (availableWidth > 4) wrapWidth = availableWidth - 4;
    const int inputReserve = InputReserve();
    const bool inputVisible = inputReserve > 0;
    const int inputHeight = InputRowHeight();
    const int availableHeight = (std::max)(0,
        (int)rect.bottom - 16 - inputReserve);
    int lineCount = officialFont ? static_cast<int>(WrapHistory(WrapWidth()).size()) :
        (int)SendMessageW(transcript, EM_GETLINECOUNT, 0, 0);
    if (lineCount < 1) lineCount = 1;
    const int contentHeight = lineCount * lineHeight + 4;
    const int childHeight = (std::min)(availableHeight, (std::max)(lineHeight + 4, contentHeight));
    const int childY = rect.bottom - 8 - inputReserve - childHeight;
    // Rich Edit is the interaction/copy proxy, not the bitmap scroll renderer.
    ShowScrollBar(transcript, SB_VERT, FALSE);
    constexpr UINT quietMove = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOREDRAW;
    SetWindowPos(transcript, nullptr, 8, (std::max)(8, childY), availableWidth, childHeight, quietMove);
    if (inputVisible)
        SetWindowPos(input, nullptr, 0, parkedInputY,
            rect.right, inputHeight, quietMove);
}

inline int VisibleTranscriptRows()
{
    if (!transcript) return 1;
    RECT rect{}; GetClientRect(transcript, &rect);
    return (std::max)(1L, (rect.bottom - 4) / (std::max)(1, lineHeight));
}

// Scroll back (positive) or forward (negative) through the stored lines.
inline void ScrollBy(int lines)
{
    if (!window || lines == 0 || !IsWindowVisible(window)) return;
    if (lines > 0 && scrollLines == 0 && maxVisibleLines > VisibleLines())
    {
        // Retail: the browsed log is the active window and opens to full height.
        FillBudget();
        Layout(GetParent(window));
    }
    pendingScrollLines = 0.0;
    scrollLines = (std::max)(0, scrollLines + lines);
    lastTick = GetTickCount64();
    if (transcript) InvalidateRect(transcript, nullptr, FALSE);
}

inline LRESULT CALLBACK TranscriptProcedure(HWND hwnd, UINT message, WPARAM w, LPARAM l,
    UINT_PTR, DWORD_PTR)
{
    if (UiBackbuffer::ConsumeNativePaint(hwnd, message)) return 0;
    if (message == WM_ERASEBKGND) return 1;
    if (officialFont && (message == WM_MOUSEWHEEL ||
        (message == WM_KEYDOWN && (w == VK_PRIOR || w == VK_NEXT || w == VK_UP || w == VK_DOWN))))
    {
        int delta = 0;
        if (message == WM_MOUSEWHEEL)
            delta = static_cast<short>(HIWORD(w)) / WHEEL_DELTA * 3;
        else if (w == VK_UP || w == VK_DOWN)
            delta = w == VK_UP ? 1 : -1;
        else // Retail pages by the window's row count.
            delta = (w == VK_PRIOR ? 1 : -1) * (std::max)(1, VisibleTranscriptRows() - 1);
        ScrollBy(delta);
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
                const auto lines = WrapHistory((std::max)(1L, rect.right - 4));
                const int visible = (std::max)(1L, (rect.bottom - 4) / lineHeight);
                scrollLines = std::clamp(scrollLines, 0,
                    (std::max)(0, static_cast<int>(lines.size()) - visible));
                // Lines still scrolling in sit below their resting row and rise
                // into place; one extra row above is revealed as they move.
                const double pending = scrollLines > 0 ? 0.0 : pendingScrollLines;
                const int extra = static_cast<int>(std::ceil(pending));
                const int offset = static_cast<int>(std::lround(pending * lineHeight));
                const int end = static_cast<int>(lines.size()) - scrollLines;
                const int first = (std::max)(0, end - visible - extra - 1);
                const int drawnLines = end - first;
                int y = rect.bottom - 2 - drawnLines * lineHeight + offset;
                for (int index = first; index < end; ++index, y += lineHeight)
                    if (y + lineHeight > 0 && y < rect.bottom)
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
    const bool wasVisible = IsWindowVisible(window) != FALSE;
    SetWindowTextW(transcript, history.c_str());
    ApplyTranscriptColour();
    if (!wasVisible)
    {
        displayedHeight = 0.0f;
        ShowWindow(window, SW_SHOWNOACTIVATE);
        StartTicking();
    }
    Layout(owner);
    LayoutTranscript();
    SendMessageW(transcript, EM_SETSEL, history.size(), history.size());
    SendMessageW(transcript, EM_SCROLLCARET, 0, 0);
    InvalidateRect(window, nullptr, TRUE);
}

// Append one message. Retail grows the window one row per wrapped line and
// scrolls the new lines in; while reading back, the view stays where it is.
inline void AppendEntry(HWND owner, const std::wstring& text, bool indentContinuation)
{
    LogEntry entry{text, indentContinuation};
    const int added = WrappedLineCount(entry);
    lastEntry = text;
    entries.push_back(std::move(entry));
    TrimHistoryToRetailCapacity();
    RebuildHistory();
    if (scrollLines > 0)
        scrollLines += added;
    else
    {
        pendingScrollLines += added;
        if (pendingScrollLines > retailJumpScrollBacklog) pendingScrollLines = 0.0;
    }
    AddBudgetLines(added);
    RefreshTranscript(owner);
}

inline void CloseInput(const bool restoreOwnerFocus)
{
    if (!input || !IsWindowVisible(input)) return;
    HWND owner = window ? GetParent(window) : nullptr;
    ShowWindow(input, SW_HIDE);
    SetWindowTextW(input, L"");
    if (window && owner)
    {
        if (VisibleLines() == 0) Hide();
        else
        {
            Layout(owner);
            lastTick = GetTickCount64();
            InvalidateRect(window, nullptr, TRUE);
        }
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
    const bool hasText = text.find_first_not_of(L" \t\r\n") != std::wstring::npos;
    if (hasText)
    {
        // Hide the input row first so the new line is measured without it.
        ShowWindow(input, SW_HIDE);
        SetWindowTextW(input, L"");
        if (sentMessages.empty() || sentMessages.back() != text) sentMessages.push_back(text);
        if (sentMessages.size() > 20) sentMessages.erase(sentMessages.begin());
        recallIndex = -1;
        scrollLines = 0;
        AppendEntry(owner, inputSpeaker + L" : " + text, true);
    }
    else
        CloseInput(false);
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
    if (message == WM_MOUSEWHEEL)
    {
        ScrollBy(static_cast<short>(HIWORD(w)) / WHEEL_DELTA * 3);
        return 0;
    }
    if (message == WM_KEYDOWN)
    {
        if (w == VK_PRIOR || w == VK_NEXT)
        {
            ScrollBy((w == VK_PRIOR ? 1 : -1) * (std::max)(1, VisibleTranscriptRows() - 1));
            return 0;
        }
        if (w == VK_UP || w == VK_DOWN)
        {
            if (sentMessages.empty()) return 0;
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

// Minimal single-line text entry that keeps EDIT's message interface
// (WM_GETTEXT/WM_SETTEXT, EM_GETSEL/EM_SETSEL, EM_SETLIMITTEXT) but draws
// nothing itself; InputProcedure paints it into the composited log.
inline DWORD inputAnchor = 0;
inline DWORD inputCaret = 0;
inline size_t inputLimit = 500;

inline std::wstring InputText(HWND hwnd)
{
    const int length = GetWindowTextLengthW(hwnd);
    std::wstring text(static_cast<size_t>((std::max)(0, length)) + 1, L'\0');
    if (length > 0) GetWindowTextW(hwnd, text.data(), length + 1);
    text.resize(static_cast<size_t>((std::max)(0, length)));
    return text;
}

inline void ReplaceInputSelection(HWND hwnd, const std::wstring& insert)
{
    std::wstring text = InputText(hwnd);
    const size_t first = (std::min)(static_cast<size_t>((std::min)(inputAnchor, inputCaret)), text.size());
    const size_t last = (std::min)(static_cast<size_t>((std::max)(inputAnchor, inputCaret)), text.size());
    std::wstring clean;
    for (const wchar_t c : insert)
        if (c >= 0x20 || c == L'\t') clean += c == L'\t' ? L' ' : c;
    const size_t room = inputLimit > text.size() - (last - first) ?
        inputLimit - (text.size() - (last - first)) : 0;
    if (clean.size() > room) clean.resize(room);
    text.replace(first, last - first, clean);
    DefWindowProcW(hwnd, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(text.c_str()));
    inputAnchor = inputCaret = static_cast<DWORD>(first + clean.size());
}

inline void CopyInputSelection(HWND hwnd)
{
    const std::wstring text = InputText(hwnd);
    const size_t first = (std::min)(static_cast<size_t>((std::min)(inputAnchor, inputCaret)), text.size());
    const size_t last = (std::min)(static_cast<size_t>((std::max)(inputAnchor, inputCaret)), text.size());
    if (first == last || !OpenClipboard(hwnd)) return;
    EmptyClipboard();
    const std::wstring part = text.substr(first, last - first);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (part.size() + 1) * sizeof(wchar_t));
    if (memory)
    {
        if (void* target = GlobalLock(memory))
        {
            memcpy(target, part.c_str(), (part.size() + 1) * sizeof(wchar_t));
            GlobalUnlock(memory);
            if (!SetClipboardData(CF_UNICODETEXT, memory)) GlobalFree(memory);
        }
        else GlobalFree(memory);
    }
    CloseClipboard();
}

inline std::wstring ClipboardText(HWND hwnd)
{
    std::wstring result;
    if (!OpenClipboard(hwnd)) return result;
    if (HANDLE data = GetClipboardData(CF_UNICODETEXT))
        if (const wchar_t* text = static_cast<const wchar_t*>(GlobalLock(data)))
        {
            for (const wchar_t* c = text; *c; ++c)
            {
                if (*c == L'\r' || *c == L'\n') break; // Single-line entry.
                result += *c;
            }
            GlobalUnlock(data);
        }
    CloseClipboard();
    return result;
}

inline LRESULT CALLBACK InputClassProcedure(HWND hwnd, UINT message, WPARAM w, LPARAM l)
{
    switch (message)
    {
    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS | DLGC_WANTCHARS;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
    {
        PAINTSTRUCT paint{}; BeginPaint(hwnd, &paint); EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_SETTEXT:
    {
        const LRESULT result = DefWindowProcW(hwnd, message, w, l);
        inputAnchor = inputCaret = static_cast<DWORD>(GetWindowTextLengthW(hwnd));
        return result;
    }
    case EM_GETSEL:
    {
        const DWORD first = (std::min)(inputAnchor, inputCaret);
        const DWORD last = (std::max)(inputAnchor, inputCaret);
        if (w) *reinterpret_cast<DWORD*>(w) = first;
        if (l) *reinterpret_cast<DWORD*>(l) = last;
        return MAKELRESULT(first, last);
    }
    case EM_SETSEL:
    {
        const DWORD length = static_cast<DWORD>(GetWindowTextLengthW(hwnd));
        const int start = static_cast<int>(w), end = static_cast<int>(l);
        if (start < 0) { inputAnchor = inputCaret; return 0; }
        inputAnchor = (std::min)(static_cast<DWORD>(start), length);
        inputCaret = end < 0 ? length : (std::min)(static_cast<DWORD>(end), length);
        return 0;
    }
    case EM_SETLIMITTEXT:
        inputLimit = w ? static_cast<size_t>(w) : 500;
        return 0;
    case WM_CHAR:
    {
        if (w == 0x01) { inputAnchor = 0; inputCaret = static_cast<DWORD>(GetWindowTextLengthW(hwnd)); return 0; }
        if (w == 0x03) { CopyInputSelection(hwnd); return 0; }
        if (w == 0x18) { CopyInputSelection(hwnd); ReplaceInputSelection(hwnd, L""); return 0; }
        if (w == 0x16) { ReplaceInputSelection(hwnd, ClipboardText(hwnd)); return 0; }
        if (w == VK_BACK)
        {
            if (inputAnchor == inputCaret && inputCaret > 0) --inputAnchor;
            ReplaceInputSelection(hwnd, L"");
            return 0;
        }
        if (w < 0x20 || w == 0x7f) return 0; // Other control characters.
        ReplaceInputSelection(hwnd, std::wstring(1, static_cast<wchar_t>(w)));
        return 0;
    }
    case WM_KEYDOWN:
    {
        const DWORD length = static_cast<DWORD>(GetWindowTextLengthW(hwnd));
        const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        const bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const std::wstring text = InputText(hwnd);
        DWORD caret = inputCaret;
        switch (w)
        {
        case VK_LEFT:
            if (!shift && inputAnchor != inputCaret) caret = (std::min)(inputAnchor, inputCaret);
            else if (caret > 0)
            {
                --caret;
                if (control)
                {
                    while (caret > 0 && text[caret] == L' ') --caret;
                    while (caret > 0 && text[caret - 1] != L' ') --caret;
                }
            }
            break;
        case VK_RIGHT:
            if (!shift && inputAnchor != inputCaret) caret = (std::max)(inputAnchor, inputCaret);
            else if (caret < length)
            {
                ++caret;
                if (control)
                {
                    while (caret < length && text[caret] != L' ') ++caret;
                    while (caret < length && text[caret] == L' ') ++caret;
                }
            }
            break;
        case VK_HOME: caret = 0; break;
        case VK_END: caret = length; break;
        case VK_DELETE:
            if (inputAnchor == inputCaret && inputCaret < length) ++inputCaret;
            ReplaceInputSelection(hwnd, L"");
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case VK_INSERT:
            if (shift) ReplaceInputSelection(hwnd, ClipboardText(hwnd));
            else if (control) CopyInputSelection(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        default:
            return 0;
        }
        inputCaret = caret;
        if (!shift) inputAnchor = caret;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, message, w, l);
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
        // The entry line is our own window class rather than a Win32 EDIT:
        // EDIT paints itself (white box, system font) whenever text changes,
        // and that leaked into the frame. This class never paints natively.
        static const bool inputClassRegistered = []
        {
            WNDCLASSW inputClass{};
            inputClass.lpfnWndProc = InputClassProcedure;
            inputClass.hInstance = GetModuleHandleW(nullptr);
            inputClass.hCursor = LoadCursor(nullptr, IDC_IBEAM);
            inputClass.lpszClassName = L"DATuraChatInput";
            return RegisterClassW(&inputClass) != 0 ||
                GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
        }();
        (void)inputClassRegistered;
        input = CreateWindowExW(0, L"DATuraChatInput", L"",
            WS_CHILD,
            0, 0, 0, 0, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!input) return -1;
        SendMessageW(input, EM_SETLIMITTEXT, 500, 0);
        SetWindowSubclass(input, InputProcedure, 2, 0);
        // Belt and braces: the native window has no visible area at all.
        SetWindowRgn(input, CreateRectRgn(0, 0, 0, 0), FALSE);
        return 0;
    }
    case WM_LBUTTONDOWN:
        // Forward clicks on the entry row to the (invisible) Edit control so
        // clicking still places the caret.
        if (InputVisible())
        {
            POINT point{static_cast<short>(LOWORD(l)), static_cast<short>(HIWORD(l))};
            const RECT row = InputRowRect();
            if (PtInRect(&row, point))
            {
                point.x -= row.left;
                point.y -= row.top;
                SendMessageW(input, WM_LBUTTONDOWN, w, MAKELPARAM(point.x, point.y));
                return 0;
            }
        }
        break;
    case WM_SIZE:
        LayoutTranscript();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_TIMER:
        // Draw() also ticks every frame; this keeps the clock running when the
        // scene is not rendering.
        if (w == closeTimer) Tick();
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
        lineBudgetMs = 0.0;
        window = transcript = input = nullptr;
        if (font) DeleteObject(font);
        font = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, message, w, l);
}

inline void Reset()
{
    entries.clear();
    history.clear();
    scrollLines = 0;
    pendingScrollLines = 0.0;
    lineBudgetMs = 0.0;
    if (input) SetWindowTextW(input, L"");
    lastEntry.clear();
    choicePrompt.clear();
    choiceOptions.clear();
    choiceSelected = 0;
    choiceFirstVisible = 0;
    if (window) { SetWindowTextW(transcript, L""); Hide(); }
    HideChoices();
}

// Bring a hidden log back at full height (an event's Confirm while it is hidden).
inline bool Reopen(HWND owner)
{
    if (!window || !owner || entries.empty() || IsWindowVisible(window))
        return false;

    FillBudget();
    pendingScrollLines = 0.0;
    scrollLines = 0;
    RefreshTranscript(owner);
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

// Retail opens the entry line without re-expanding the log; the log then stops
// shrinking at two rows until the entry line closes.
inline bool OpenInput(HWND owner, const char* speaker)
{
    if (!EnsureWindow(owner) || !input) return false;
    inputSpeaker = Utf8(speaker && *speaker ? speaker : "Adventurer");
    const bool wasVisible = IsWindowVisible(window) != FALSE;
    ShowWindow(input, SW_SHOWNOACTIVATE);
    if (!wasVisible)
    {
        displayedHeight = 0.0f;
        ShowWindow(window, SW_SHOWNOACTIVATE);
        StartTicking();
    }
    Layout(owner);
    SetFocus(input);
    LayoutTranscript();
    inputScrollX = 0;
    recallIndex = -1;
    SendMessageW(input, EM_SETSEL, GetWindowTextLengthW(input),
        GetWindowTextLengthW(input));
    InvalidateRect(window, nullptr, TRUE);
    return true;
}

inline void Show(HWND owner, const std::string& name, const std::string& dialogue)
{
    if (!EnsureWindow(owner)) return;
    const std::wstring message =
        dialogue.empty() ? L"[No dialogue is assigned to this NPC.]" : Utf8(dialogue);
    // Retail log lines read "Name : text".
    const std::wstring entry = name == "System" ? message :
        Utf8(name.empty() ? "NPC" : name) + L" : " + message;
    AppendEntry(owner, entry, false);
}
}
