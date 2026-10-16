#pragma once

#include "ffxi_stat_system.h"
#include "inventory.h"

#include <windows.h>

#include <array>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

namespace CharacterSaveData
{
inline constexpr int Version = 3;

struct HomePoint
{
    int zoneId = -1;
    unsigned int entityId = 0;
    std::string name;
};

struct Data
{
    int homeNationIndex = -1;
    FFXIStats::CharacterBuild stats;
    std::vector<std::optional<InventorySystem::ItemStack>> inventorySlots;
    std::array<InventorySystem::ItemId, InventorySystem::EquipmentSlotCount> equippedItems{};
    bool hasHomePoint = false;
    int homePointZoneId = -1;
    unsigned int homePointEntityId = 0;
    std::vector<HomePoint> activatedHomePoints;
};

inline bool ValidEquipmentOwnership(const Data& data)
{
    for (const InventorySystem::ItemId itemId : data.equippedItems)
    {
        if (itemId == 0)
            continue;
        bool owned = false;
        for (const auto& inventorySlot : data.inventorySlots)
            owned = owned || (inventorySlot && inventorySlot->itemId == itemId);
        if (!owned)
            return false;
    }
    return true;
}

inline bool ValidStats(const FFXIStats::CharacterBuild& stats)
{
    return stats.race >= FFXIStats::kRace_Hume && stats.race < FFXIStats::kRace_Count &&
        stats.mainJob > FFXIStats::kJob_None && stats.mainJob < FFXIStats::kJob_Count &&
        stats.mainLevel >= 1 && stats.mainLevel <= 99 &&
        stats.supportJob >= FFXIStats::kJob_None && stats.supportJob < FFXIStats::kJob_Count &&
        stats.supportLevel >= 0 && stats.supportLevel <= 99 &&
        ((stats.supportJob == FFXIStats::kJob_None) == (stats.supportLevel == 0));
}

inline bool SameHomePoint(const HomePoint& left, const HomePoint& right)
{
    return left.zoneId == right.zoneId && left.entityId == right.entityId;
}

inline std::string HomePointIdentity(const HomePoint& point)
{
    return std::to_string(point.zoneId) + ":" + std::to_string(point.entityId);
}

inline void RegisterHomePoint(Data& data, const HomePoint& point)
{
    for (HomePoint& existing : data.activatedHomePoints)
    {
        if (SameHomePoint(existing, point))
        {
            existing.name = point.name;
            return;
        }
    }
    data.activatedHomePoints.push_back(point);
}

inline void SetHomePoint(Data& data, const HomePoint& point)
{
    RegisterHomePoint(data, point);
    data.hasHomePoint = true;
    data.homePointZoneId = point.zoneId;
    data.homePointEntityId = point.entityId;
}

inline bool ReadInt(const char* path, const char* section, const char* key, int& value)
{
    char text[64] = {};
    GetPrivateProfileStringA(section, key, "", text, sizeof(text), path);
    if (!text[0])
        return false;

    char* end = nullptr;
    errno = 0;
    const long parsed = std::strtol(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' || parsed < INT_MIN || parsed > INT_MAX)
        return false;
    value = static_cast<int>(parsed);
    return true;
}

inline bool ReadAttributes(
    const char* path, const char* section, FFXIStats::AttributeBlock& attributes)
{
    static constexpr const char* Keys[FFXIStats::kAttr_Count] =
        {"STR", "DEX", "VIT", "AGI", "INT", "MND", "CHR"};
    for (int index = 0; index < FFXIStats::kAttr_Count; ++index)
    {
        const auto attribute = static_cast<FFXIStats::Attribute>(index);
        if (!ReadInt(path, section, Keys[index], attributes[attribute]))
            return false;
    }
    return true;
}

inline const char* EquipmentKey(const std::size_t index)
{
    static constexpr const char* Keys[] =
        {"Main", "Head", "Body", "Hands", "Legs", "Feet", "Back"};
    return index < sizeof(Keys) / sizeof(Keys[0]) ? Keys[index] : "";
}

inline const char* ElementKey(const std::size_t index)
{
    static constexpr const char* Keys[] =
        {"Fire", "Ice", "Wind", "Earth", "Lightning", "Water", "Light", "Dark"};
    return index < sizeof(Keys) / sizeof(Keys[0]) ? Keys[index] : "";
}

inline const char* SkillKey(const std::size_t index)
{
    static constexpr const char* Keys[] = {"HandToHand", "Dagger", "Sword", "GreatSword",
        "Axe", "GreatAxe", "Scythe", "Polearm", "Katana", "GreatKatana", "Club", "Staff",
        "Archery", "Marksmanship", "Throwing", "Guard", "Evasion", "Shield", "Parrying",
        "DivineMagic", "HealingMagic", "EnhancingMagic", "EnfeeblingMagic", "ElementalMagic",
        "DarkMagic", "SummoningMagic", "Ninjutsu", "Singing", "StringInstrument",
        "WindInstrument", "BlueMagic", "Geomancy", "Handbell"};
    return index < sizeof(Keys) / sizeof(Keys[0]) ? Keys[index] : "";
}

inline const char* ModifierKey(const std::size_t index)
{
    static constexpr const char* Keys[] = {"MaxHP", "MaxMP", "Attack", "RangedAttack",
        "Defense", "Accuracy", "RangedAccuracy", "Evasion", "MagicAttack", "MagicDefense",
        "MagicAccuracy", "MagicEvasion", "Haste", "DualWield", "MartialArts", "FastCast",
        "StoreTP", "SubtleBlow", "Regain", "Regen", "Refresh", "DoubleAttack",
        "TripleAttack", "QuadrupleAttack", "CriticalHitRate", "CriticalHitDamage",
        "DamageTaken", "PhysicalDamageTaken", "MagicDamageTaken", "BreathDamageTaken",
        "Enmity", "CurePotency", "SpellInterruptionRate", "MovementSpeed"};
    return index < sizeof(Keys) / sizeof(Keys[0]) ? Keys[index] : "";
}

inline void ReadOptional(const char* path, const char* section, const char* key,
    int& value, bool& needsRewrite)
{
    if (!ReadInt(path, section, key, value))
    {
        needsRewrite = true;
    }
}

inline void ReadExtendedStats(const char* path, FFXIStats::CharacterBuild& stats,
    bool& needsRewrite)
{
    ReadOptional(path, "Resources", "CurrentHP", stats.resources.currentHp, needsRewrite);
    ReadOptional(path, "Resources", "CurrentMP", stats.resources.currentMp, needsRewrite);
    ReadOptional(path, "Resources", "TP", stats.resources.tp, needsRewrite);
    ReadOptional(path, "Progression", "Experience", stats.progression.experience, needsRewrite);
    ReadOptional(path, "Progression", "LimitPoints", stats.progression.limitPoints, needsRewrite);
    ReadOptional(path, "Progression", "MeritPoints", stats.progression.meritPoints, needsRewrite);
    ReadOptional(path, "Progression", "JobPoints", stats.progression.jobPoints, needsRewrite);
    ReadOptional(path, "Progression", "CapacityPoints", stats.progression.capacityPoints, needsRewrite);
    ReadOptional(path, "Progression", "MasterLevel", stats.progression.masterLevel, needsRewrite);
    for (std::size_t i = 0; i < stats.skills.value.size(); ++i)
        ReadOptional(path, "Skills", SkillKey(i), stats.skills.value[i], needsRewrite);
    for (std::size_t i = 0; i < stats.elementalResistance.value.size(); ++i)
        ReadOptional(path, "ElementalResistance", ElementKey(i),
            stats.elementalResistance.value[i], needsRewrite);
    const char* sections[] = {"GearModifiers", "TraitModifiers", "EffectModifiers"};
    FFXIStats::ModifierBlock* blocks[] =
        {&stats.gearModifiers, &stats.traitModifiers, &stats.effectModifiers};
    for (std::size_t block = 0; block < 3; ++block)
        for (std::size_t i = 0; i < blocks[block]->value.size(); ++i)
            ReadOptional(path, sections[block], ModifierKey(i), blocks[block]->value[i], needsRewrite);
}

inline bool Save(const char* path, const Data& data);

inline bool Load(const char* path, Data& data)
{
    Data loaded;
    bool needsRewrite = false;
    int version = 0;
    int activated = 0;
    int entityId = 0;
    if (!path || !ReadInt(path, "Character", "Version", version) ||
        (version < 1 || version > Version) ||
        !ReadInt(path, "Character", "HomeNation", loaded.homeNationIndex) ||
        loaded.homeNationIndex < 0 || loaded.homeNationIndex > 2 ||
        !ReadInt(path, "HomePoint", "Activated", activated) ||
        (activated != 0 && activated != 1))
    {
        return false;
    }

    if (version >= 3)
    {
        int race = 0;
        int mainJob = 0;
        int supportJob = 0;
        if (!ReadInt(path, "Stats", "Race", race) ||
            !ReadInt(path, "Stats", "MainJob", mainJob) ||
            !ReadInt(path, "Stats", "MainLevel", loaded.stats.mainLevel) ||
            !ReadInt(path, "Stats", "SupportJob", supportJob) ||
            !ReadInt(path, "Stats", "SupportLevel", loaded.stats.supportLevel) ||
            !ReadAttributes(path, "StatMerits", loaded.stats.merits) ||
            !ReadAttributes(path, "StatGear", loaded.stats.gear) ||
            !ReadAttributes(path, "StatBuffs", loaded.stats.buffs))
        {
            return false;
        }
        loaded.stats.race = static_cast<FFXIStats::Race>(race);
        loaded.stats.mainJob = static_cast<FFXIStats::Job>(mainJob);
        loaded.stats.supportJob = static_cast<FFXIStats::Job>(supportJob);
        if (!ValidStats(loaded.stats))
            return false;
    }
    ReadExtendedStats(path, loaded.stats, needsRewrite);
    if (loaded.stats.resources.currentHp < 0 || loaded.stats.resources.currentMp < 0 ||
        loaded.stats.resources.tp < 0 || loaded.stats.resources.tp > 3000 ||
        loaded.stats.progression.experience < 0 || loaded.stats.progression.masterLevel < 0)
        return false;


    int inventorySlotCount = 0;
    if (!ReadInt(path, "Inventory", "SlotCount", inventorySlotCount))
    {
        needsRewrite = true;
        inventorySlotCount = 0;
    }
    if (inventorySlotCount < 0 || inventorySlotCount > 1024)
        return false;
    loaded.inventorySlots.resize(static_cast<std::size_t>(inventorySlotCount));
    for (int index = 0; index < inventorySlotCount; ++index)
    {
        char section[64] = {};
        sprintf_s(section, "InventorySlot.%d", index);
        int itemId = 0;
        int quantity = 0;
        const bool hasItem = ReadInt(path, section, "ItemId", itemId);
        const bool hasQuantity = ReadInt(path, section, "Quantity", quantity);
        if (!hasItem || !hasQuantity || itemId < 0 || quantity < 0 ||
            ((itemId == 0) != (quantity == 0)))
        {
            return false;
        }
        if (itemId != 0)
        {
            loaded.inventorySlots[static_cast<std::size_t>(index)] = {
                static_cast<InventorySystem::ItemId>(itemId),
                static_cast<std::uint32_t>(quantity)};
        }
    }

    for (std::size_t index = 0; index < loaded.equippedItems.size(); ++index)
    {
        int itemId = 0;
        if (!ReadInt(path, "Equipment", EquipmentKey(index), itemId))
        {
            needsRewrite = true;
            itemId = 0;
        }
        if (itemId < 0)
            return false;
        loaded.equippedItems[index] = static_cast<InventorySystem::ItemId>(itemId);
    }
    if (!ValidEquipmentOwnership(loaded))
        return false;

    loaded.hasHomePoint = activated != 0;
    if (loaded.hasHomePoint)
    {
        if (!ReadInt(path, "HomePoint", "ZoneId", loaded.homePointZoneId) ||
            !ReadInt(path, "HomePoint", "EntityId", entityId) ||
            loaded.homePointZoneId < 0 || entityId <= 0)
        {
            return false;
        }
        loaded.homePointEntityId = static_cast<unsigned int>(entityId);
        loaded.activatedHomePoints.push_back(
            {loaded.homePointZoneId, loaded.homePointEntityId, "Home Point"});
    }

    if (version >= 2)
    {
        int count = 0;
        if (!ReadInt(path, "ActivatedHomePoints", "Count", count) || count < 0 || count > 512)
            return false;
        loaded.activatedHomePoints.clear();
        for (int index = 0; index < count; ++index)
        {
            char section[64] = {};
            sprintf_s(section, "ActivatedHomePoint.%d", index);
            HomePoint point;
            int pointEntityId = 0;
            char pointName[128] = {};
            if (!ReadInt(path, section, "ZoneId", point.zoneId) || point.zoneId < 0 ||
                !ReadInt(path, section, "EntityId", pointEntityId) || pointEntityId <= 0)
            {
                return false;
            }
            point.entityId = static_cast<unsigned int>(pointEntityId);
            char identity[64] = {};
            GetPrivateProfileStringA(section, "Identity", "", identity, sizeof(identity), path);
            if (!identity[0] || identity != HomePointIdentity(point))
                return false;
            GetPrivateProfileStringA(section, "Name", "Home Point", pointName,
                sizeof(pointName), path);
            point.name = pointName;
            bool duplicate = false;
            for (const HomePoint& existing : loaded.activatedHomePoints)
                duplicate = duplicate || SameHomePoint(existing, point);
            if (!duplicate)
                loaded.activatedHomePoints.push_back(point);
        }
    }

    data = loaded;
    needsRewrite = needsRewrite || version != Version;
    return !needsRewrite || Save(path, data);
}

inline bool WriteValue(const char* path, const char* section, const char* key, int value)
{
    char text[32] = {};
    sprintf_s(text, "%d", value);
    return WritePrivateProfileStringA(section, key, text, path) != FALSE;
}

inline bool WriteAttributes(
    const char* path, const char* section, const FFXIStats::AttributeBlock& attributes)
{
    static constexpr const char* Keys[FFXIStats::kAttr_Count] =
        {"STR", "DEX", "VIT", "AGI", "INT", "MND", "CHR"};
    for (int index = 0; index < FFXIStats::kAttr_Count; ++index)
    {
        const auto attribute = static_cast<FFXIStats::Attribute>(index);
        if (!WriteValue(path, section, Keys[index], attributes[attribute]))
            return false;
    }
    return true;
}

inline bool WriteExtendedStats(const char* path, const FFXIStats::CharacterBuild& stats)
{
    bool saved = WriteValue(path, "Resources", "CurrentHP", stats.resources.currentHp) &&
        WriteValue(path, "Resources", "CurrentMP", stats.resources.currentMp) &&
        WriteValue(path, "Resources", "TP", stats.resources.tp) &&
        WriteValue(path, "Progression", "Experience", stats.progression.experience) &&
        WriteValue(path, "Progression", "LimitPoints", stats.progression.limitPoints) &&
        WriteValue(path, "Progression", "MeritPoints", stats.progression.meritPoints) &&
        WriteValue(path, "Progression", "JobPoints", stats.progression.jobPoints) &&
        WriteValue(path, "Progression", "CapacityPoints", stats.progression.capacityPoints) &&
        WriteValue(path, "Progression", "MasterLevel", stats.progression.masterLevel);
    for (std::size_t i = 0; saved && i < stats.skills.value.size(); ++i)
        saved = WriteValue(path, "Skills", SkillKey(i), stats.skills.value[i]);
    for (std::size_t i = 0; saved && i < stats.elementalResistance.value.size(); ++i)
        saved = WriteValue(path, "ElementalResistance", ElementKey(i),
            stats.elementalResistance.value[i]);
    const char* sections[] = {"GearModifiers", "TraitModifiers", "EffectModifiers"};
    const FFXIStats::ModifierBlock* blocks[] =
        {&stats.gearModifiers, &stats.traitModifiers, &stats.effectModifiers};
    for (std::size_t block = 0; saved && block < 3; ++block)
        for (std::size_t i = 0; saved && i < blocks[block]->value.size(); ++i)
            saved = WriteValue(path, sections[block], ModifierKey(i), blocks[block]->value[i]);
    return saved;
}

inline bool Save(const char* path, const Data& data)
{
    if (!path || !path[0])
        return false;

    if (!ValidStats(data.stats))
        return false;
    if (data.stats.resources.currentHp < 0 || data.stats.resources.currentMp < 0 ||
        data.stats.resources.tp < 0 || data.stats.resources.tp > 3000 ||
        data.stats.progression.experience < 0 || data.stats.progression.masterLevel < 0)
        return false;
    if (data.inventorySlots.size() > 1024)
        return false;
    for (const auto& slot : data.inventorySlots)
    {
        if (slot && (slot->itemId == 0 || slot->itemId > INT_MAX ||
            slot->quantity == 0 || slot->quantity > INT_MAX))
        {
            return false;
        }
    }
    for (const InventorySystem::ItemId itemId : data.equippedItems)
    {
        if (itemId > INT_MAX)
            return false;
    }
    if (!ValidEquipmentOwnership(data))
        return false;

    bool saved = WriteValue(path, "Character", "Version", Version) &&
        WriteValue(path, "Character", "HomeNation", data.homeNationIndex) &&
        WriteValue(path, "Stats", "Race", static_cast<int>(data.stats.race)) &&
        WriteValue(path, "Stats", "MainJob", static_cast<int>(data.stats.mainJob)) &&
        WriteValue(path, "Stats", "MainLevel", data.stats.mainLevel) &&
        WriteValue(path, "Stats", "SupportJob", static_cast<int>(data.stats.supportJob)) &&
        WriteValue(path, "Stats", "SupportLevel", data.stats.supportLevel) &&
        WriteAttributes(path, "StatMerits", data.stats.merits) &&
        WriteAttributes(path, "StatGear", data.stats.gear) &&
        WriteAttributes(path, "StatBuffs", data.stats.buffs) &&
        WriteExtendedStats(path, data.stats) &&
        WriteValue(path, "Inventory", "SlotCount",
            static_cast<int>(data.inventorySlots.size())) &&
        WriteValue(path, "HomePoint", "Activated", data.hasHomePoint ? 1 : 0) &&
        WriteValue(path, "HomePoint", "ZoneId", data.homePointZoneId) &&
        WriteValue(path, "HomePoint", "EntityId", static_cast<int>(data.homePointEntityId)) &&
        WriteValue(path, "ActivatedHomePoints", "Count",
            static_cast<int>(data.activatedHomePoints.size()));
    for (std::size_t index = 0; saved && index < data.equippedItems.size(); ++index)
    {
        saved = WriteValue(path, "Equipment", EquipmentKey(index),
            static_cast<int>(data.equippedItems[index]));
    }
    for (std::size_t index = 0; saved && index < data.inventorySlots.size(); ++index)
    {
        char section[64] = {};
        sprintf_s(section, "InventorySlot.%zu", index);
        const auto& slot = data.inventorySlots[index];
        saved = WriteValue(path, section, "ItemId",
                    slot ? static_cast<int>(slot->itemId) : 0) &&
            WriteValue(path, section, "Quantity",
                    slot ? static_cast<int>(slot->quantity) : 0);
    }
    for (std::size_t index = 0; saved && index < data.activatedHomePoints.size(); ++index)
    {
        char section[64] = {};
        sprintf_s(section, "ActivatedHomePoint.%zu", index);
        const HomePoint& point = data.activatedHomePoints[index];
        saved = WriteValue(path, section, "ZoneId", point.zoneId) &&
            WriteValue(path, section, "EntityId", static_cast<int>(point.entityId)) &&
            WritePrivateProfileStringA(section, "Identity",
                HomePointIdentity(point).c_str(), path) != FALSE &&
            WritePrivateProfileStringA(section, "Name", point.name.c_str(), path) != FALSE;
    }
    return saved;
}
}
