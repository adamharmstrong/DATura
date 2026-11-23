#include "world_item_system.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace WorldItemSystem
{
EntityId Spawn(State& state, const SpawnRequest& request)
{
    if (request.zoneId < 0 || request.stack.itemId == 0 || request.stack.quantity == 0 ||
        !std::isfinite(request.position[0]) || !std::isfinite(request.position[1]) ||
        !std::isfinite(request.position[2]) || state.nextEntityId == 0 ||
        state.nextEntityId == (std::numeric_limits<EntityId>::max)())
        return 0;

    Entity entity;
    entity.id = state.nextEntityId++;
    entity.zoneId = request.zoneId;
    entity.stack = request.stack;
    std::copy(std::begin(request.position), std::end(request.position), entity.position);
    state.entities.push_back(entity);
    return entity.id;
}

const Entity* Find(const State& state, const EntityId id)
{
    const auto found = std::find_if(state.entities.begin(), state.entities.end(),
        [id](const Entity& entity) { return entity.id == id; });
    return found == state.entities.end() ? nullptr : &*found;
}

std::size_t CountInZone(const State& state, const int zoneId)
{
    return static_cast<std::size_t>(std::count_if(state.entities.begin(), state.entities.end(),
        [zoneId](const Entity& entity) { return entity.zoneId == zoneId; }));
}

bool TransferToInventory(State& state, const EntityId id,
    InventorySystem::Inventory& inventory)
{
    const auto found = std::find_if(state.entities.begin(), state.entities.end(),
        [id](const Entity& entity) { return entity.id == id; });
    if (found == state.entities.end() ||
        !inventory.AddAll(found->stack.itemId, found->stack.quantity))
        return false;
    state.entities.erase(found);
    return true;
}
}
