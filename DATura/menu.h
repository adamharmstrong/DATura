#pragma once
#include "menu_cursor.h"

namespace FFXIMenu
{
// Modern viewport layout corresponding to the PS2 menu window/button rectangles.
// Resource decoding and event answer ownership remain outside this module.
struct Layout
{
    RECT window{};
    RECT question{};
    int firstRowY = 0;
    int rowHeight = 0;
    int cursorWidth = 0;
    int rows = 0;
    RECT Button(int visibleIndex) const;
};
Layout MakeLayout(const RECT& bounds, int questionHeight, int rowHeight, int rows);
void DrawWindow(HDC dc, const RECT& bounds);
void DrawCursor(HDC dc, const Layout& layout, int selectedVisibleRow,
    FFXIMenuCursor::State& cursor);
}
