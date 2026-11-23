#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "menu_cursor.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace FFXIMenuCursor
{
void SetShape(State& state, std::vector<std::vector<POINT>> frames, bool animated)
{
    state.frames = std::move(frames);
    state.frame = 0;
    state.elapsedTicks = 0;
    state.animated = animated;
}
void Locate(State& state, POINT windowOrigin, POINT buttonOrigin)
{
    // TkMenuCtrlData.cpp: CTkDrawCtrlCursol::LocateCalc.
    state.location = {windowOrigin.x + buttonOrigin.x + state.offset.x,
        windowOrigin.y + buttonOrigin.y + state.offset.y};
}
void Advance(State& state, unsigned int ticks)
{
    if (!state.animated || state.frames.empty()) return;
    // ShapeCalc resets a countdown to 3 and advances when it becomes negative.
    // Use explicit 60 Hz UI ticks instead of tying animation to paint frequency.
    const unsigned long long total = static_cast<unsigned long long>(state.elapsedTicks) + ticks;
    state.frame = (state.frame + total / 4) % state.frames.size();
    state.elapsedTicks = static_cast<unsigned int>(total % 4);
}
void Draw(HDC dc, const State& state, const RECT& clip)
{
    if (!dc || !state.visible || !std::isfinite(state.scale) || state.scale <= 0) return;
    // A geometric pointer until the native menu shape/texture is resolved.
    const std::vector<POINT> fallback{{0, 0}, {9, 6}, {0, 12}, {2, 6}};
    const auto& shape = state.frames.empty() ? fallback : state.frames[state.frame % state.frames.size()];
    if (shape.size() < 3) return;
    std::vector<POINT> points;
    for (const POINT& point : shape)
        points.push_back({state.location.x + static_cast<LONG>(std::lround(point.x * state.scale)),
            state.location.y + static_cast<LONG>(std::lround(point.y * state.scale))});
    const int saved = SaveDC(dc);
    if (!saved) return;
    IntersectClipRect(dc, clip.left, clip.top, clip.right, clip.bottom);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(65, 75, 100));
    HBRUSH brush = CreateSolidBrush(RGB(245, 245, 255));
    if (pen && brush)
    {
        SelectObject(dc, pen);
        SelectObject(dc, brush);
        Polygon(dc, points.data(), static_cast<int>(points.size()));
    }
    RestoreDC(dc, saved);
    if (pen) DeleteObject(pen);
    if (brush) DeleteObject(brush);
}
}
