#include "world_item_hover.h"

#include <limits>

namespace WorldItemHover
{
WorldItemSystem::EntityId PickNearest(const std::vector<Candidate>& candidates,
    const float mouseX, const float mouseY)
{
    WorldItemSystem::EntityId picked = 0;
    float nearestDepth = (std::numeric_limits<float>::max)();
    for (const Candidate& candidate : candidates)
    {
        if (candidate.entityId == 0 || candidate.depth < 0.0f ||
            mouseX < candidate.left || mouseX > candidate.right ||
            mouseY < candidate.top || mouseY > candidate.bottom ||
            candidate.depth >= nearestDepth)
            continue;
        picked = candidate.entityId;
        nearestDepth = candidate.depth;
    }
    return picked;
}
}
