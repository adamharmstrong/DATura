#pragma once

#include "inventory.h"
#include "starter_item_catalog.h"

#include <cstddef>
#include <vector>
#include <windows.h>

namespace FFXIInventoryMenu
{
enum class View { Closed, Items, Equipment, KeyItems, Actions, DropQuantity };
enum class EventType { None, Closed, Use, Equip, Drop };

struct Event
{
    EventType type = EventType::None;
    std::size_t slotIndex = 0;
    InventorySystem::ItemStack stack;
};

struct State
{
    View view = View::Closed;
    View returnView = View::Closed;
    int selected = 0;
    int actionSelected = 0;
    std::uint32_t dropQuantity = 1;
};

bool IsOpen(const State& state);
void OpenItems(State& state);
void OpenEquipment(State& state);
void OpenKeyItems(State& state);

Event HandleKey(State& state, unsigned int key,
    const InventorySystem::Inventory& inventory,
    const InventorySystem::ItemCatalog& catalog,
    const std::vector<StarterItemCatalog::KeyItemId>& ownedKeyItems);

void Draw(HDC dc, const State& state, int width, int height,
    const InventorySystem::Inventory& inventory,
    const InventorySystem::ItemCatalog& catalog,
    const StarterItemCatalog::KeyItemCatalog& keyItemCatalog,
    const std::vector<StarterItemCatalog::KeyItemId>& ownedKeyItems);
}
