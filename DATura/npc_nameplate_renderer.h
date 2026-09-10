#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct IDirect3DDevice9;

namespace NpcNameplateRenderer
{
struct DrawItem
{
    std::string name;
    std::string roleTitle;
    float screenX = 0.0f;
    float screenY = 0.0f;
    float depth = 1.0f;
    float distanceSquared = 0.0f;
    // One optional retail icon; IDs are defined by FFXIPlayerIcons::Catalog.
    std::string icon;
    std::uint32_t nameColor = 0xFFA4FFA4;
    std::uint32_t iconColor = 0xFFFFFFFF;
    bool jobMaster = false;
    bool starterQuestAvailable = false;
};

void Draw(IDirect3DDevice9* device, std::vector<DrawItem>& items);
void ClearCachedTextures();
}
