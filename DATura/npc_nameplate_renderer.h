#pragma once

#include <string>
#include <vector>

struct IDirect3DDevice9;

namespace NpcNameplateRenderer
{
struct DrawItem
{
    std::string name;
    float screenX = 0.0f;
    float screenY = 0.0f;
    float depth = 1.0f;
    float distanceSquared = 0.0f;
};

void Draw(IDirect3DDevice9* device, std::vector<DrawItem>& items);
void ClearCachedTextures();
}
