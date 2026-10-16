#pragma once
#include <windows.h>
#include <vector>

namespace FFXIMenuCursor
{
// CTkDrawCtrlCursol keeps location, resource offsets, scale and shape separate.
// Frames use local coordinates; the fallback is not a recovered retail asset.
struct State
{
    POINT location{};
    POINT offset{};
    float scale = 1.0f;
    bool visible = true;
    std::vector<std::vector<POINT>> frames;
    size_t frame = 0;
    unsigned int elapsedTicks = 0;
    bool animated = false;
};
void SetShape(State& state, std::vector<std::vector<POINT>> frames, bool animated);
void Locate(State& state, POINT windowOrigin, POINT buttonOrigin);
void Advance(State& state, unsigned int ticks);
void Draw(HDC dc, const State& state, const RECT& clip);
}
