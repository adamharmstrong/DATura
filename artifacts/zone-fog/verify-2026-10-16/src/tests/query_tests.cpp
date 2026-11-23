#include "stdafx.h"
#include "npc_chat_window.h"
#include "menu.h"
#include <cstdio>

int main(int argc, char** argv)
{
    using namespace NpcChatWindow;
    if (argc != 2) return 2;
    FFXIBitmapFont::SetRootPath(argv[1]);
    FFXIChatAssets::SetRootPath(argv[1]);
    if (!FFXIBitmapFont::Load() || !FFXIChatAssets::Load()) return 1;
    // A hidden owner prevents the smoke test from opening a visible window.
    HWND owner = CreateWindowExW(0, L"STATIC", L"Query test",
        WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, nullptr, nullptr,
        GetModuleHandleW(nullptr), nullptr);
    if (!owner) return 1;
    ShowChoices(owner, "Do you want to warp to the first floor?",
        {"Yes.", "Not now."}, 99);
    if (!choiceWindow || choiceSelected != 1 || choiceOptions.size() != 2 ||
        !(GetWindowLongPtrW(choiceWindow, GWL_STYLE) & WS_VISIBLE)) return 1;
    SetChoiceSelection(owner, -1);
    if (choiceSelected != 0) return 1;
    ShowChoices(owner, "Choose", {"0", "1", "2", "3", "4", "5"}, 0);
    SetChoiceSelection(owner, 2);
    if (choiceFirstVisible != 0) return 1;
    SetChoiceSelection(owner, 3);
    if (choiceFirstVisible != 1) return 1;
    SetChoiceSelection(owner, 5);
    if (choiceFirstVisible != 3) return 1;
    SetChoiceSelection(owner, 4);
    if (choiceFirstVisible != 3) return 1;
    SetChoiceSelection(owner, 0);
    if (choiceFirstVisible != 0) return 1;
    const std::wstring longWord(80, L'W');
    const auto wrapped = WrapBitmapText(longWord, 60);
    std::wstring restored;
    for (const auto& row : wrapped)
    {
        if (FFXIBitmapFont::Measure(row, fontSize).cx > 60) return 1;
        restored += row;
    }
    if (restored != longWord || wrapped.size() < 2) return 1;
    if (WrapBitmapText(L"a\r\nb\x07" L"c\n", 300) !=
        std::vector<std::wstring>{L"a", L"b", L"c", L""}) return 1;
    const auto spaced = WrapBitmapText(L"a  b", 300);
    if (spaced.size() != 1 || spaced[0] != L"a  b") return 1;
    const RECT mapped = UiBackbuffer::MapRect({8, 800, 1008, 992}, 2000, 1000, 2500, 1250);
    if (mapped.left != 10 || mapped.top != 1000 || mapped.bottom != 1240) return 1;
    const RECT unchanged = UiBackbuffer::MapRect({8, 800, 1008, 992}, 2000, 1000, 2000, 1000);
    if (unchanged.bottom != 992 || unchanged.left != 8) return 1;
    FFXIMenuCursor::State cursor;
    cursor.offset = {2, 3};
    FFXIMenuCursor::Locate(cursor, {10, 20}, {5, 7});
    if (cursor.location.x != 17 || cursor.location.y != 30) return 1;
    FFXIMenuCursor::SetShape(cursor,
        {{{0, 0}, {8, 4}, {0, 8}}, {{0, 0}, {10, 4}, {0, 8}}}, true);
    FFXIMenuCursor::Advance(cursor, 3);
    if (cursor.frame != 0) return 1;
    FFXIMenuCursor::Advance(cursor, 1);
    if (cursor.frame != 1) return 1;
    FFXIMenuCursor::Advance(cursor, 4);
    if (cursor.frame != 0) return 1;
    FFXIMenuCursor::SetShape(cursor, {}, false);
    const auto layout = FFXIMenu::MakeLayout({10, 20, 300, 200}, 38, 19, 3);
    if (layout.Button(0).top != 70 || layout.Button(2).top != 108 ||
        layout.Button(3).right != 0) return 1;
    RECT bounds{}; GetClientRect(choiceWindow, &bounds);
    HDC dc = GetDC(choiceWindow);
    HDC memory = CreateCompatibleDC(dc);
    HBITMAP bitmap = CreateCompatibleBitmap(dc, bounds.right, bounds.bottom);
    HGDIOBJ old = SelectObject(memory, bitmap);
    // A bottom-aligned child must not shift the source origin of the final blit.
    // Exercise several child positions, as used by one-line and collapsing logs.
    for (int offset : {0, 19, 57})
    {
        UiBackbuffer::PaintLocal(memory, [offset](HDC target) {
            SetViewportOrgEx(target, 8, offset, nullptr);
            IntersectClipRect(target, 0, 0, 20, 20);
            SetPixel(target, 0, 0, RGB(255, 255, 255));
        });
        POINT origin{}; GetViewportOrgEx(memory, &origin);
        if (origin.x || origin.y || GetPixel(memory, 8, offset) != RGB(255, 255, 255)) return 1;
    }
    // Exercise the real bitmap font and authored backdrop without a chatlog.
    SendMessageW(choiceWindow, WM_PRINT, reinterpret_cast<WPARAM>(memory), PRF_CLIENT);
    if (GetPixel(memory, 20, 5) == CLR_INVALID) return 1;
    // Verify a separately drawn cursor, and that its clipping leaves the
    // surrounding surface untouched when drawing beyond a menu boundary.
    RECT clip{0, 0, 20, 20};
    HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
    RECT clear{0, 0, bounds.right, bounds.bottom};
    FillRect(memory, &clear, black);
    DeleteObject(black);
    cursor.location = {16, 4};
    FFXIMenuCursor::Draw(memory, cursor, clip);
    if (GetPixel(memory, 18, 9) == RGB(0, 0, 0) ||
        GetPixel(memory, 22, 9) != RGB(0, 0, 0)) return 1;
    SelectObject(memory, old); DeleteObject(bitmap); DeleteDC(memory);
    ReleaseDC(choiceWindow, dc);
    const int shortHeight = bounds.bottom;
    ShowChoices(owner, std::string(200, 'W'), {"Yes.", "No.", "Later."}, 0);
    GetClientRect(choiceWindow, &bounds);
    if (bounds.bottom <= shortHeight) return 1;
    if (!EnsureWindow(owner)) return 1;
    if (GetWindowLongPtrW(transcript, GWL_STYLE) & WS_VSCROLL) return 1;
    history = L"One line";
    ApplyLayout(owner, 100);
    RECT expanded{}, expandedText{}, collapsed{}, collapsedText{};
    GetWindowRect(window, &expanded); GetWindowRect(transcript, &expandedText);
    ApplyLayout(owner, 100 - lineHeight);
    GetWindowRect(window, &collapsed); GetWindowRect(transcript, &collapsedText);
    if (expanded.bottom != collapsed.bottom || collapsed.top - expanded.top != lineHeight ||
        expandedText.bottom != collapsedText.bottom || expandedText.top != collapsedText.top) return 1;
    UiBackbuffer::active = true;
    InvalidateRect(transcript, nullptr, FALSE);
    SendMessageW(transcript, WM_PAINT, 0, 0);
    if (GetUpdateRect(transcript, nullptr, FALSE)) return 1;
    UiBackbuffer::active = false;
    ShowChoices(owner, "", {}, 0);
    if (GetWindowLongPtrW(choiceWindow, GWL_STYLE) & WS_VISIBLE) return 1;
    ShowChoices(owner, "Again?", {"Yes.", "No."}, 0);
    DestroyWindow(owner);
    if (choiceWindow) return 1;
    DrawChoices(nullptr);
    puts("Query tests passed: menu layout, cursor position/animation/clipping, assets, three-row scrolling, wrapping, prompt layout, painting, teardown.");
    return 0;
}
