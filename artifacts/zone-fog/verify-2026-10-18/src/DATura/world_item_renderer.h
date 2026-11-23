#pragma once

#include "inventory.h"
#include "world_item_system.h"

struct IDirect3DDevice9;

namespace WorldItemRenderer
{
void Draw(IDirect3DDevice9* device, const WorldItemSystem::State& state,
    const InventorySystem::ItemCatalog& catalog, int zoneId);
}
