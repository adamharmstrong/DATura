#include "starter_item_catalog.h"

#include <utility>

namespace StarterItemCatalog
{
using namespace InventorySystem;

namespace
{
ItemDefinition Basic(const ItemId id, const char* name, const std::uint32_t stack,
    const ItemCategory category = ItemCategory::Material, const std::uint32_t flags = ItemFlagNone)
{
    ItemDefinition item{id, name, stack};
    item.category = category;
    item.flags = flags;
    return item;
}

ItemDefinition Consumable(const ItemId id, const char* name, const std::uint32_t stack,
    const ItemCategory category, const UseEffect effect, const std::int32_t magnitude = 0,
    const std::uint32_t durationSeconds = 0)
{
    auto item = Basic(id, name, stack, category, ItemFlagUsable);
    item.use = {effect, magnitude, durationSeconds};
    return item;
}

ItemDefinition Armor(const ItemId id, const char* name, const EquipmentSlot slot,
    const std::uint16_t level, const std::uint16_t defense)
{
    auto item = Basic(id, name, 1, ItemCategory::Armor, ItemFlagEquippable);
    item.equipment = {slot, WeaponType::None, level, defense, 0, 0};
    return item;
}

ItemDefinition Weapon(const ItemId id, const char* name, const WeaponType type,
    const std::uint16_t level, const std::uint16_t damage, const std::uint16_t delay,
    const std::uint32_t extraFlags = ItemFlagNone)
{
    auto item = Basic(id, name, 1, ItemCategory::Weapon, ItemFlagEquippable | extraFlags);
    item.equipment = {EquipmentSlot::Main, type, level, 0, damage, delay};
    return item;
}
}

bool RegisterAll(ItemCatalog& catalog)
{
    const ItemDefinition items[] = {
        Basic(856, "Rabbit Hide", 12), Basic(4358, "Hare Meat", 12),
        Basic(768, "Flint Stone", 12), Basic(922, "Bat Wing", 12),
        Basic(846, "Insect Wing", 12), Basic(912, "Beehive Chip", 12),
        Basic(4370, "Honey", 12), Basic(816, "Silk Thread", 12),
        Basic(1156, "Crawler Calculus", 12), Basic(839, "Crawler Cocoon", 12),
        Basic(847, "Bird Feather", 12), Basic(505, "Sheepskin", 12),
        Basic(4387, "Wild Onion", 12), Basic(511, "Goblin Mask", 12),
        Basic(510, "Goblin Armor", 12), Basic(498, "Yagudo Necklace", 12),
        Basic(596, "Quadav Backplate", 12),
        Weapon(16656, "Orcish Axe", WeaponType::Axe, 5, 10, 288,
            ItemFlagRare | ItemFlagExclusive | ItemFlagNoAuction | ItemFlagNoDelivery),

        Consumable(4096, "Fire Crystal", 12, ItemCategory::Crystal, UseEffect::BeginSynthesis),
        Consumable(4097, "Ice Crystal", 12, ItemCategory::Crystal, UseEffect::BeginSynthesis),
        Consumable(4098, "Wind Crystal", 12, ItemCategory::Crystal, UseEffect::BeginSynthesis),
        Consumable(4099, "Earth Crystal", 12, ItemCategory::Crystal, UseEffect::BeginSynthesis),
        Consumable(4100, "Lightning Crystal", 12, ItemCategory::Crystal, UseEffect::BeginSynthesis),
        Consumable(4101, "Water Crystal", 12, ItemCategory::Crystal, UseEffect::BeginSynthesis),
        Consumable(4102, "Light Crystal", 12, ItemCategory::Crystal, UseEffect::BeginSynthesis),
        Consumable(4103, "Dark Crystal", 12, ItemCategory::Crystal, UseEffect::BeginSynthesis),

        Consumable(4112, "Potion", 1, ItemCategory::Medicine, UseEffect::RestoreHp, 50),
        Consumable(4128, "Ether", 1, ItemCategory::Medicine, UseEffect::RestoreMp, 20),
        Consumable(4148, "Antidote", 12, ItemCategory::Medicine, UseEffect::CurePoison),
        Consumable(4151, "Echo Drops", 12, ItemCategory::Medicine, UseEffect::CureSilence),
        Consumable(4150, "Eye Drops", 12, ItemCategory::Medicine, UseEffect::CureBlindness),
        Consumable(4154, "Holy Water", 12, ItemCategory::Medicine, UseEffect::CureCurse),
        Consumable(4376, "Meat Jerky", 12, ItemCategory::Food, UseEffect::FoodAttack, 0, 1800),
        Consumable(4394, "Ginger Cookie", 99, ItemCategory::Food, UseEffect::FoodMage, 0, 1800),
        Consumable(4413, "Apple Pie", 12, ItemCategory::Food, UseEffect::FoodMage, 0, 1800),
        Consumable(4410, "Roast Mushroom", 12, ItemCategory::Food, UseEffect::FoodDefense, 0, 1800),
        Consumable(4381, "Meat Mithkabob", 12, ItemCategory::Food, UseEffect::FoodAttack, 0, 1800),
        Consumable(4378, "Selbina Milk", 12, ItemCategory::Food, UseEffect::Regen, 1, 180),
        Basic(4509, "Distilled Water", 12, ItemCategory::Food),

        Weapon(16534, "Onion Sword", WeaponType::Sword, 1, 5, 228, ItemFlagNoSale),
        Weapon(16448, "Bronze Dagger", WeaponType::Dagger, 1, 3, 183),
        Weapon(16385, "Cesti", WeaponType::HandToHand, 1, 1, 528),
        Weapon(17024, "Ash Club", WeaponType::Club, 1, 4, 264),
        Weapon(17049, "Maple Wand", WeaponType::Club, 1, 3, 216),
        Armor(12448, "Bronze Cap", EquipmentSlot::Head, 1, 2),
        Armor(12576, "Bronze Harness", EquipmentSlot::Body, 1, 4),
        Armor(12704, "Bronze Mittens", EquipmentSlot::Hands, 1, 1),
        Armor(12832, "Bronze Subligar", EquipmentSlot::Legs, 1, 3),
        Armor(12960, "Bronze Leggings", EquipmentSlot::Feet, 1, 1),
        Armor(13594, "Rabbit Mantle", EquipmentSlot::Back, 4, 1),
        Armor(12440, "Leather Bandana", EquipmentSlot::Head, 7, 3),
        Armor(12568, "Leather Vest", EquipmentSlot::Body, 7, 7),
        Armor(12696, "Leather Gloves", EquipmentSlot::Hands, 7, 2),
        Armor(12824, "Leather Trousers", EquipmentSlot::Legs, 7, 5),
        Armor(12952, "Leather Highboots", EquipmentSlot::Feet, 7, 2),

        Basic(536, "Adventurer Coupon", 1, ItemCategory::Quest,
            ItemFlagRare | ItemFlagNoSale),
        Basic(513, "Linkshell", 1, ItemCategory::Communication,
            ItemFlagExclusive | ItemFlagNoSale | ItemFlagNoAuction | ItemFlagNoDelivery),
        Basic(1126, "Beastmen's Seal", 99, ItemCategory::Currency,
            ItemFlagExclusive | ItemFlagNoSale | ItemFlagNoAuction | ItemFlagNoDelivery),
        Basic(1127, "Kindred's Seal", 99, ItemCategory::Currency,
            ItemFlagExclusive | ItemFlagNoSale | ItemFlagNoAuction | ItemFlagNoDelivery)
    };

    static_assert(sizeof(items) / sizeof(items[0]) == kItemCount);
    for (const auto& item : items)
        if (catalog.Find(item.id))
            return false;
    for (const auto& item : items)
        if (!catalog.Register(item))
            return false;
    return true;
}

bool KeyItemCatalog::Register(KeyItemDefinition definition)
{
    if (definition.id == 0 || definition.name.empty())
        return false;
    return definitions_.emplace(definition.id, std::move(definition)).second;
}

const KeyItemDefinition* KeyItemCatalog::Find(const KeyItemId id) const
{
    const auto found = definitions_.find(id);
    return found == definitions_.end() ? nullptr : &found->second;
}

bool RegisterStarterKeyItems(KeyItemCatalog& catalog)
{
    const KeyItemDefinition items[] = {
        {138, "Chocobo License"},
        {385, "Map of the San d'Oria Area"},
        {386, "Map of the Bastok Area"},
        {387, "Map of the Windurst Area"}
    };
    static_assert(sizeof(items) / sizeof(items[0]) == kKeyItemCount);
    for (const auto& item : items)
        if (catalog.Find(item.id))
            return false;
    for (const auto& item : items)
        if (!catalog.Register(item))
            return false;
    return true;
}
}
