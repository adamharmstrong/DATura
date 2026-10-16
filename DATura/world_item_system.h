#pragma once

#include "inventory.h"

#include <cstdint>
#include <vector>

namespace WorldItemSystem
{
using EntityId = std::uint64_t;

struct Entity
{
    EntityId id = 0;
    int zoneId = -1;
    InventorySystem::ItemStack stack;
    float position[3] = {};
};

struct SpawnRequest
{
    int zoneId = -1;
    InventorySystem::ItemStack stack;
    float position[3] = {};
};

struct State
{
    EntityId nextEntityId = 1;
    std::vector<Entity> entities;
};

// Returns zero without changing state if the request is invalid.
EntityId Spawn(State& state, const SpawnRequest& request);
const Entity* Find(const State& state, EntityId id);
std::size_t CountInZone(const State& state, int zoneId);
// Transfers the complete world stack into the bag. Neither side changes if the
// inventory cannot accept the whole stack.
bool TransferToInventory(State& state, EntityId id, InventorySystem::Inventory& inventory);
}
