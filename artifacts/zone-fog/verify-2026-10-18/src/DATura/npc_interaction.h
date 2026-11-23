#pragma once
#include <cstdint>
#include <vector>

namespace NpcInteraction
{
struct Target
{
    std::uint32_t id;
    float left, top, right, bottom, depth;
};
struct State
{
    std::uint32_t selected = 0;
    std::vector<Target> targets;

    // Pick the nearest rendered target when projected bounds overlap.
    bool Click(float x, float y)
    {
        const Target* hit = nullptr;
        for (const auto& target : targets)
            if (x >= target.left && x <= target.right &&
                y >= target.top && y <= target.bottom &&
                (!hit || target.depth < hit->depth))
                hit = &target;
        const auto id = hit ? hit->id : 0;
        const bool talk = id != 0 && selected == id;
        selected = id;
        return talk;
    }
};
}
