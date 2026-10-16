#pragma once

#include "world_item_system.h"

#include <vector>

namespace WorldItemHover
{
struct Candidate
{
    WorldItemSystem::EntityId entityId = 0;
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    float depth = 0.0f;
};

WorldItemSystem::EntityId PickNearest(const std::vector<Candidate>& candidates,
    float mouseX, float mouseY);
}
