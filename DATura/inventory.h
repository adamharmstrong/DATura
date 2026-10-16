#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace InventorySystem
{
using ItemId = std::uint32_t;

enum class ItemCategory
{
    Material,
    Crystal,
    Medicine,
    Food,
    Weapon,
    Armor,
    Quest,
    Currency,
    Communication
};

enum class EquipmentSlot
{
    None,
    Main,
    Head,
    Body,
    Hands,
    Legs,
    Feet,
    Back,
    Count
};

inline constexpr std::size_t EquipmentSlotCount =
    static_cast<std::size_t>(EquipmentSlot::Count) - 1;

inline constexpr std::size_t EquipmentSlotIndex(const EquipmentSlot slot)
{
    return slot == EquipmentSlot::None ? EquipmentSlotCount
        : static_cast<std::size_t>(slot) - 1;
}

enum class WeaponType
{
    None,
    HandToHand,
    Dagger,
    Sword,
    Axe,
    Club
};

enum class UseEffect
{
    None,
    BeginSynthesis,
    RestoreHp,
    RestoreMp,
    CurePoison,
    CureBlindness,
    CureSilence,
    CureCurse,
    FoodAttack,
    FoodMage,
    FoodDefense,
    Regen
};

enum ItemFlag : std::uint32_t
{
    ItemFlagNone = 0,
    ItemFlagUsable = 1u << 0,
    ItemFlagEquippable = 1u << 1,
    ItemFlagRare = 1u << 2,
    ItemFlagExclusive = 1u << 3,
    ItemFlagNoSale = 1u << 4,
    ItemFlagNoAuction = 1u << 5,
    ItemFlagNoDelivery = 1u << 6
};

struct EquipmentInfo
{
    EquipmentSlot slot = EquipmentSlot::None;
    WeaponType weaponType = WeaponType::None;
    std::uint16_t level = 0;
    std::uint16_t defense = 0;
    std::uint16_t damage = 0;
    std::uint16_t delay = 0;
};

struct UseInfo
{
    UseEffect effect = UseEffect::None;
    std::int32_t magnitude = 0;
    std::uint32_t durationSeconds = 0;
};

struct ItemDefinition
{
    ItemId id = 0;
    std::string name;
    std::uint32_t stackLimit = 1;
    ItemCategory category = ItemCategory::Material;
    std::uint32_t flags = ItemFlagNone;
    EquipmentInfo equipment;
    UseInfo use;
};

struct ItemStack
{
    ItemId itemId = 0;
    std::uint32_t quantity = 0;

    bool operator==(const ItemStack&) const = default;
};

class ItemCatalog
{
public:
    bool Register(ItemDefinition definition);
    const ItemDefinition* Find(ItemId id) const;
    const ItemDefinition* FindByName(const std::string& name) const;
    std::size_t Size() const { return definitions_.size(); }

private:
    std::unordered_map<ItemId, ItemDefinition> definitions_;
};

enum class AddStatus
{
    Complete,
    Partial,
    Full,
    UnknownItem,
    InvalidQuantity
};

struct AddResult
{
    AddStatus status = AddStatus::InvalidQuantity;
    std::uint32_t added = 0;
    std::uint32_t remainder = 0;
};

// Fixed-slot inventory with FFXI-style stack limits. Mutating operations preserve
// the invariant that empty slots contain no stack and occupied slots contain a
// known item with a non-zero quantity no greater than its catalog stack limit.
class Inventory
{
public:
    using TransferReceiver = std::function<bool(const ItemStack&)>;

    Inventory(std::size_t slotCount, const ItemCatalog& catalog);

    std::size_t SlotCount() const { return slots_.size(); }
    std::size_t OccupiedSlotCount() const;
    const std::optional<ItemStack>& Slot(std::size_t index) const;
    const std::vector<std::optional<ItemStack>>& Slots() const { return slots_; }
    std::uint64_t Count(ItemId itemId) const;
    bool CanAdd(ItemId itemId, std::uint32_t quantity) const;

    // Restores a complete per-character slot snapshot after validating every
    // stack against this inventory's catalog. Failure leaves the inventory unchanged.
    bool Restore(const std::vector<std::optional<ItemStack>>& slots);

    AddResult Add(ItemId itemId, std::uint32_t quantity);
    bool AddAll(ItemId itemId, std::uint32_t quantity);
    bool Remove(ItemId itemId, std::uint32_t quantity);

    // Offers a stack from one slot to another system (for example, a world-item
    // spawner). The inventory changes only if the receiver accepts the transfer.
    bool TransferFromSlot(std::size_t slotIndex, std::uint32_t quantity,
        const TransferReceiver& receiver);

private:
    const ItemCatalog& catalog_;
    std::vector<std::optional<ItemStack>> slots_;
};
}
