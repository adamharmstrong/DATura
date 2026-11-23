#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "menu.h"
#include "ffxi_chat_assets.h"
#include <algorithm>

namespace FFXIMenu
{
RECT Layout::Button(int visibleIndex) const
{
    if (visibleIndex < 0 || visibleIndex >= rows) return {};
    const int y = firstRowY + visibleIndex * rowHeight;
    return {window.left + 10 + cursorWidth, y, window.right - 10, y + rowHeight};
}
Layout MakeLayout(const RECT& bounds, int questionHeight, int rowHeight, int rows)
{
    Layout layout;
    layout.window = bounds;
    layout.rowHeight = (std::max)(1, rowHeight);
    layout.rows = (std::max)(0, rows);
    layout.cursorWidth = (std::max)(12, layout.rowHeight);
    layout.question = {bounds.left + 10, bounds.top + 8,
        bounds.right - 10, bounds.top + 8 + (std::max)(0, questionHeight)};
    layout.firstRowY = layout.question.bottom + (questionHeight > 0 ? 4 : 0);
    return layout;
}
void DrawWindow(HDC dc, const RECT& bounds)
{
    if (!dc || bounds.right <= bounds.left || bounds.bottom <= bounds.top) return;
    const int saved = SaveDC(dc);
    if (!saved) return;
    IntersectClipRect(dc, bounds.left, bounds.top, bounds.right, bounds.bottom);
    if (!FFXIChatAssets::PaintWindow(dc, bounds))
    {
        HBRUSH background = CreateSolidBrush(RGB(12, 18, 50));
        FillRect(dc, &bounds, background);
        DeleteObject(background);
    }
    // Shared fallback frame; the original menu-shape frame is not yet decoded.
    HBRUSH border = CreateSolidBrush(RGB(190, 197, 220));
    FrameRect(dc, &bounds, border);
    DeleteObject(border);
    RestoreDC(dc, saved);
}
void DrawCursor(HDC dc, const Layout& layout, int selectedVisibleRow,
    FFXIMenuCursor::State& cursor)
{
    if (selectedVisibleRow < 0 || selectedVisibleRow >= layout.rows) return;
    const RECT button = layout.Button(selectedVisibleRow);
    FFXIMenuCursor::Locate(cursor, {layout.window.left, layout.window.top},
        {button.left - layout.window.left - layout.cursorWidth,
         button.top - layout.window.top});
    FFXIMenuCursor::Draw(dc, cursor, layout.window);
}
}
