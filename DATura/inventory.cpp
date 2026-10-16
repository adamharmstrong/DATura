#include "inventory.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace InventorySystem
{
bool ItemCatalog::Register(ItemDefinition definition)
{
    if (definition.id == 0 || definition.name.empty() || definition.stackLimit == 0)
        return false;

    return definitions_.emplace(definition.id, std::move(definition)).second;
}

const ItemDefinition* ItemCatalog::Find(const ItemId id) const
{
    const auto found = definitions_.find(id);
    return found == definitions_.end() ? nullptr : &found->second;
}

const ItemDefinition* ItemCatalog::FindByName(const std::string& name) const
{
    const auto found = std::find_if(definitions_.begin(), definitions_.end(),
        [&name](const auto& entry) { return entry.second.name == name; });
    return found == definitions_.end() ? nullptr : &found->second;
}

Inventory::Inventory(const std::size_t slotCount, const ItemCatalog& catalog)
    : catalog_(catalog), slots_(slotCount)
{
}

std::size_t Inventory::OccupiedSlotCount() const
{
    return static_cast<std::size_t>(std::count_if(slots_.begin(), slots_.end(),
        [](const auto& slot) { return slot.has_value(); }));
}

const std::optional<ItemStack>& Inventory::Slot(const std::size_t index) const
{
    if (index >= slots_.size())
        throw std::out_of_range("inventory slot index");
    return slots_[index];
}

std::uint64_t Inventory::Count(const ItemId itemId) const
{
    std::uint64_t total = 0;
    for (const auto& slot : slots_)
        if (slot && slot->itemId == itemId)
            total += slot->quantity;
    return total;
}

bool Inventory::CanAdd(const ItemId itemId, const std::uint32_t quantity) const
{
    if (quantity == 0)
        return false;
    const ItemDefinition* definition = catalog_.Find(itemId);
    if (!definition)
        return false;

    std::uint64_t capacity = 0;
    for (const auto& slot : slots_)
    {
        if (!slot)
            capacity += definition->stackLimit;
        else if (slot->itemId == itemId && slot->quantity < definition->stackLimit)
            capacity += definition->stackLimit - slot->quantity;
        if (capacity >= quantity)
            return true;
    }
    return false;
}

bool Inventory::Restore(const std::vector<std::optional<ItemStack>>& slots)
{
    if (slots.size() != slots_.size())
        return false;

    for (const auto& slot : slots)
    {
        if (!slot)
            continue;
        const ItemDefinition* definition = catalog_.Find(slot->itemId);
        if (!definition || slot->quantity == 0 || slot->quantity > definition->stackLimit)
            return false;
    }

    slots_ = slots;
    return true;
}

AddResult Inventory::Add(const ItemId itemId, const std::uint32_t quantity)
{
    if (quantity == 0)
        return {AddStatus::InvalidQuantity, 0, 0};

    const ItemDefinition* definition = catalog_.Find(itemId);
    if (!definition)
        return {AddStatus::UnknownItem, 0, quantity};

    std::uint32_t remaining = quantity;
    for (auto& slot : slots_)
    {
        if (!slot || slot->itemId != itemId || slot->quantity >= definition->stackLimit)
            continue;

        const std::uint32_t room = definition->stackLimit - slot->quantity;
        const std::uint32_t moved = std::min(room, remaining);
        slot->quantity += moved;
        remaining -= moved;
        if (remaining == 0)
            break;
    }

    for (auto& slot : slots_)
    {
        if (remaining == 0)
            break;
        if (slot)
            continue;

        const std::uint32_t moved = std::min(definition->stackLimit, remaining);
        slot = ItemStack{itemId, moved};
        remaining -= moved;
    }

    const std::uint32_t added = quantity - remaining;
    const AddStatus status = remaining == 0 ? AddStatus::Complete
        : added == 0 ? AddStatus::Full : AddStatus::Partial;
    return {status, added, remaining};
}

bool Inventory::AddAll(const ItemId itemId, const std::uint32_t quantity)
{
    if (!CanAdd(itemId, quantity))
        return false;
    return Add(itemId, quantity).status == AddStatus::Complete;
}

bool Inventory::Remove(const ItemId itemId, const std::uint32_t quantity)
{
    if (quantity == 0 || Count(itemId) < quantity)
        return false;

    std::uint32_t remaining = quantity;
    for (auto& slot : slots_)
    {
        if (!slot || slot->itemId != itemId)
            continue;

        const std::uint32_t removed = std::min(slot->quantity, remaining);
        slot->quantity -= removed;
        remaining -= removed;
        if (slot->quantity == 0)
            slot.reset();
        if (remaining == 0)
            return true;
    }
    return true;
}

bool Inventory::TransferFromSlot(const std::size_t slotIndex, const std::uint32_t quantity,
    const TransferReceiver& receiver)
{
    if (slotIndex >= slots_.size() || quantity == 0 || !receiver)
        return false;

    auto& slot = slots_[slotIndex];
    if (!slot || quantity > slot->quantity)
        return false;

    const ItemStack transfer{slot->itemId, quantity};
    if (!receiver(transfer))
        return false;

    slot->quantity -= quantity;
    if (slot->quantity == 0)
        slot.reset();
    return true;
}
}
