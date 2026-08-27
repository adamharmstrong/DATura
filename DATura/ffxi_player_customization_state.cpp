#include "stdafx.h"
#include "ffxi_player_customization_state.h"

#include "character_dat_table.h"
#include "ffxi_player_customization_catalog.h"
#include "ffxi_player_preset.h"

namespace FFXIPlayerCustomizationState
{
int FaceVariantCountForRace(const int raceIndex)
{
    (void)raceIndex;
    return FaceVariantCount;
}

void Clamp(PlayerEquipState& equipment, int& faceVariant)
{
    if (equipment.raceIndex < 0)
        equipment.raceIndex = 0;
    if (equipment.raceIndex >= kFFXICharRaceCount)
        equipment.raceIndex = kFFXICharRaceCount - 1;

    const int faceCount = FaceVariantCountForRace(equipment.raceIndex);
    if (faceVariant < 0)
        faceVariant = 0;
    if (faceVariant >= faceCount)
        faceVariant = faceCount - 1;

    if (equipment.animationMode < 0)
        equipment.animationMode = 0;
    if (equipment.animationMode >= FFXIPlayerCustomizationCatalog::AnimationCategoryCount())
        equipment.animationMode = FFXIPlayerCustomizationCatalog::AnimationCategoryCount() - 1;
    if (equipment.animationBank < 0)
        equipment.animationBank = 0;
}

FFXIPlayerPreset::Data CapturePreset(const PlayerEquipState& equipment,
                                     const int faceVariant)
{
    return
    {
        equipment.raceIndex, faceVariant,
        equipment.mainType, equipment.mainItem,
        equipment.subType, equipment.subItem,
        equipment.rangedType, equipment.rangedItem,
        equipment.headItem, equipment.bodyItem,
        equipment.handsItem, equipment.legsItem, equipment.feetItem,
        equipment.animationBank, equipment.animationMode,
    };
}

void ApplyPreset(const FFXIPlayerPreset::Data& preset,
                 PlayerEquipState& equipment, int& faceVariant)
{
    equipment.raceIndex = preset.raceIndex;
    faceVariant = preset.faceVariant;
    equipment.mainType = preset.mainType;
    equipment.mainItem = preset.mainItem;
    equipment.subType = preset.subType;
    equipment.subItem = preset.subItem;
    equipment.rangedType = preset.rangedType;
    equipment.rangedItem = preset.rangedItem;
    equipment.headItem = preset.headItem;
    equipment.bodyItem = preset.bodyItem;
    equipment.handsItem = preset.handsItem;
    equipment.legsItem = preset.legsItem;
    equipment.feetItem = preset.feetItem;
    equipment.animationBank = preset.animationBank;
    equipment.animationMode = preset.animationMode;
}
}
