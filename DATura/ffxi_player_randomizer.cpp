#include "stdafx.h"
#include "ffxi_player_randomizer.h"

#include "character_dat_table.h"
#include "ffxi_internal_lists.h"
#include "ffxi_player_customization_catalog.h"

#include <random>
#include <vector>

namespace
{
int RandomIntExclusive(const int count)
{
    if (count <= 1)
        return 0;

    static std::mt19937 rng((unsigned int)GetTickCount());
    std::uniform_int_distribution<int> distribution(0, count - 1);
    return distribution(rng);
}

int RandomCatalogIndex(const int raceIndex, const int slot, const int minimumIndex,
                       const int maximumIndex, const bool allowZero, const int fallbackMaximum)
{
    std::vector<int> candidates;
    if (raceIndex >= 0 &&
        raceIndex < (int)(sizeof(kFFXIInternalPCLists) / sizeof(kFFXIInternalPCLists[0])) &&
        slot >= 0 && slot < 11)
    {
        const FFXIInternalList& list = kFFXIInternalPCLists[raceIndex][slot];
        for (int index = 0; index < list.count; ++index)
        {
            const int itemIndex = list.entries[index].index;
            if ((!allowZero && itemIndex <= 0) ||
                itemIndex < minimumIndex || itemIndex > maximumIndex)
            {
                continue;
            }
            if (!list.entries[index].label || !list.entries[index].label[0])
                continue;
            candidates.push_back(itemIndex);
        }
    }

    if (!candidates.empty())
        return candidates[RandomIntExclusive((int)candidates.size())];

    if (fallbackMaximum <= 0)
        return 0;
    return allowZero
        ? RandomIntExclusive(fallbackMaximum + 1)
        : 1 + RandomIntExclusive(fallbackMaximum);
}

int RandomSparseCatalogIndex(const int raceIndex, const int slot, const int noneChanceDivisor)
{
    if (noneChanceDivisor > 0 && RandomIntExclusive(noneChanceDivisor) == 0)
        return 0;

    std::vector<int> candidates;
    if (raceIndex >= 0 &&
        raceIndex < (int)(sizeof(kFFXIInternalPCLists) / sizeof(kFFXIInternalPCLists[0])) &&
        slot >= 0 && slot < 11)
    {
        const FFXIInternalList& list = kFFXIInternalPCLists[raceIndex][slot];
        for (int index = 0; index < list.count; ++index)
        {
            const int itemIndex = list.entries[index].index;
            if (itemIndex <= 0 || !list.entries[index].label || !list.entries[index].label[0])
                continue;
            candidates.push_back(itemIndex);
        }
    }

    return candidates.empty() ? 0 : candidates[RandomIntExclusive((int)candidates.size())];
}

int ValidRaceIndex(const PlayerEquipState& equipment)
{
    return equipment.raceIndex >= 0 && equipment.raceIndex < kFFXICharRaceCount
        ? equipment.raceIndex
        : 0;
}

int RandomAnimationBank(const int raceIndex, const int animationMode)
{
    const FFXIPlayerCustomizationCatalog::AnimationCategory& category =
        FFXIPlayerCustomizationCatalog::AnimationCategoryForIndex(animationMode);
    std::vector<int> candidates;
    if (category.slot >= 0 && category.slot < 11)
    {
        const FFXIInternalList& list = kFFXIInternalPCLists[raceIndex][category.slot];
        for (int index = 0; index < list.count; ++index)
        {
            if (FFXIPlayerCustomizationCatalog::AnimationCategoryMatches(
                    list.entries[index].label, category.prefix))
            {
                candidates.push_back(list.entries[index].index);
            }
        }
    }
    return candidates.empty() ? 0 : candidates[RandomIntExclusive((int)candidates.size())];
}
}

namespace FFXIPlayerRandomizer
{
void RandomizeCharacter(PlayerEquipState& equipment, int& faceVariant)
{
    equipment.raceIndex = RandomIntExclusive(kFFXICharRaceCount);
    faceVariant = RandomIntExclusive(
        FFXIPlayerCustomizationState::FaceVariantCountForRace(equipment.raceIndex));
    equipment.animationPlaying = false;
    FFXIPlayerCustomizationState::Clamp(equipment, faceVariant);
}

void RandomizeWeapons(PlayerEquipState& equipment)
{
    const int raceIndex = ValidRaceIndex(equipment);
    equipment.mainItem = RandomSparseCatalogIndex(raceIndex, kFFXIInternalPCSlot_Main, 0);
    equipment.subItem = RandomSparseCatalogIndex(raceIndex, kFFXIInternalPCSlot_Sub, 4);
    equipment.rangedItem = RandomSparseCatalogIndex(raceIndex, kFFXIInternalPCSlot_Range, 3);
    equipment.animationPlaying = false;
}

void RandomizeArmor(PlayerEquipState& equipment)
{
    const int raceIndex = ValidRaceIndex(equipment);
    equipment.headItem = RandomCatalogIndex(
        raceIndex, kFFXIInternalPCSlot_Head, 1,
        FFXIPlayerCustomizationState::ArmorVariantCount, true,
        FFXIPlayerCustomizationState::ArmorVariantCount);
    equipment.bodyItem = RandomCatalogIndex(
        raceIndex, kFFXIInternalPCSlot_Body, 0,
        FFXIPlayerCustomizationState::ArmorVariantCount - 1, true,
        FFXIPlayerCustomizationState::ArmorVariantCount - 1);
    equipment.handsItem = RandomCatalogIndex(
        raceIndex, kFFXIInternalPCSlot_Hands, 0,
        FFXIPlayerCustomizationState::ArmorVariantCount - 1, true,
        FFXIPlayerCustomizationState::ArmorVariantCount - 1);
    equipment.legsItem = RandomCatalogIndex(
        raceIndex, kFFXIInternalPCSlot_Legs, 0,
        FFXIPlayerCustomizationState::ArmorVariantCount - 1, true,
        FFXIPlayerCustomizationState::ArmorVariantCount - 1);
    equipment.feetItem = RandomCatalogIndex(
        raceIndex, kFFXIInternalPCSlot_Feet, 0,
        FFXIPlayerCustomizationState::ArmorVariantCount - 1, true,
        FFXIPlayerCustomizationState::ArmorVariantCount - 1);
    equipment.animationPlaying = false;
}

void RandomizeAction(PlayerEquipState& equipment, int& faceVariant)
{
    equipment.animationMode = RandomIntExclusive(
        FFXIPlayerCustomizationCatalog::AnimationCategoryCount());
    equipment.animationBank = RandomAnimationBank(
        ValidRaceIndex(equipment), equipment.animationMode);
    equipment.animationPlaying = false;
    FFXIPlayerCustomizationState::Clamp(equipment, faceVariant);
}

void RandomizeAll(PlayerEquipState& equipment, int& faceVariant)
{
    equipment.raceIndex = RandomIntExclusive(kFFXICharRaceCount);
    faceVariant = RandomIntExclusive(
        FFXIPlayerCustomizationState::FaceVariantCountForRace(equipment.raceIndex));
    const int raceIndex = equipment.raceIndex;
    equipment.mainItem = RandomSparseCatalogIndex(raceIndex, kFFXIInternalPCSlot_Main, 0);
    equipment.subItem = RandomSparseCatalogIndex(raceIndex, kFFXIInternalPCSlot_Sub, 4);
    equipment.rangedItem = RandomSparseCatalogIndex(raceIndex, kFFXIInternalPCSlot_Range, 3);
    equipment.headItem = RandomCatalogIndex(
        raceIndex, kFFXIInternalPCSlot_Head, 1,
        FFXIPlayerCustomizationState::ArmorVariantCount, true,
        FFXIPlayerCustomizationState::ArmorVariantCount);
    equipment.bodyItem = RandomCatalogIndex(
        raceIndex, kFFXIInternalPCSlot_Body, 0,
        FFXIPlayerCustomizationState::ArmorVariantCount - 1, true,
        FFXIPlayerCustomizationState::ArmorVariantCount - 1);
    equipment.handsItem = RandomCatalogIndex(
        raceIndex, kFFXIInternalPCSlot_Hands, 0,
        FFXIPlayerCustomizationState::ArmorVariantCount - 1, true,
        FFXIPlayerCustomizationState::ArmorVariantCount - 1);
    equipment.legsItem = RandomCatalogIndex(
        raceIndex, kFFXIInternalPCSlot_Legs, 0,
        FFXIPlayerCustomizationState::ArmorVariantCount - 1, true,
        FFXIPlayerCustomizationState::ArmorVariantCount - 1);
    equipment.feetItem = RandomCatalogIndex(
        raceIndex, kFFXIInternalPCSlot_Feet, 0,
        FFXIPlayerCustomizationState::ArmorVariantCount - 1, true,
        FFXIPlayerCustomizationState::ArmorVariantCount - 1);
    equipment.animationMode = RandomIntExclusive(
        FFXIPlayerCustomizationCatalog::AnimationCategoryCount());
    equipment.animationBank = RandomAnimationBank(raceIndex, equipment.animationMode);
    equipment.animationPlaying = false;
    FFXIPlayerCustomizationState::Clamp(equipment, faceVariant);
}
}
