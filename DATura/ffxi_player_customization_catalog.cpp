#include "stdafx.h"
#include "ffxi_player_customization_catalog.h"

#include "ffxi_internal_lists.h"
#include "win32_combo_box.h"

#include <cstring>

namespace
{
const FFXIPlayerCustomizationCatalog::AnimationCategory kAnimationCategories[] =
{
    { "Battle",        kFFXIInternalPCSlot_Action, "Battle" },
    { "Emote",         kFFXIInternalPCSlot_Action, "Emote" },
    { "General",       kFFXIInternalPCSlot_Action, "General" },
    { "Ability",       kFFXIInternalPCSlot_Action, "Ability" },
    { "Hand-to-Hand",  kFFXIInternalPCSlot_Action, "Hand-to-Hand" },
    { "Dagger",        kFFXIInternalPCSlot_Action, "Dagger" },
    { "Sword",         kFFXIInternalPCSlot_Action, "Sword" },
    { "Club",          kFFXIInternalPCSlot_Action, "Club" },
    { "Axe",           kFFXIInternalPCSlot_Action, "Axe" },
    { "Katana",        kFFXIInternalPCSlot_Action, "Katana" },
    { "G. Sword",      kFFXIInternalPCSlot_Action, "G. Sword" },
    { "Staff",         kFFXIInternalPCSlot_Action, "Staff" },
    { "G. Axe",        kFFXIInternalPCSlot_Action, "G. Axe" },
    { "G. Katana",     kFFXIInternalPCSlot_Action, "G. Katana" },
    { "Scythe",        kFFXIInternalPCSlot_Action, "Scythe" },
    { "Polearm",       kFFXIInternalPCSlot_Action, "Polearm" },
    { "Archery",       kFFXIInternalPCSlot_Action, "Archery" },
    { "Marksmanship",  kFFXIInternalPCSlot_Action, "Marksmanship" },
    { "Mannequin",     kFFXIInternalPCSlot_Action, "Mannequin" },
    { "Others",        kFFXIInternalPCSlot_Action, "Others" },
    { "NPC WS",        kFFXIInternalPCSlot_Action, "NPC WS" },
    { "Dancer",        kFFXIInternalPCSlot_Action, "Dancer" },
    { "unknown",       kFFXIInternalPCSlot_Action, "unknown" },
    { "Motion",        kFFXIInternalPCSlot_Motion, nullptr },
    { "Weapon Skills", kFFXIInternalPCSlot_WS,     nullptr },
    { "All",           kFFXIInternalPCSlot_Action, nullptr },
};
}

namespace FFXIPlayerCustomizationCatalog
{
int AnimationCategoryCount()
{
    return static_cast<int>(sizeof(kAnimationCategories) / sizeof(kAnimationCategories[0]));
}

const AnimationCategory& AnimationCategoryForIndex(int categoryIndex)
{
    if (categoryIndex < 0 || categoryIndex >= AnimationCategoryCount())
        categoryIndex = 0;
    return kAnimationCategories[categoryIndex];
}

bool AnimationCategoryMatches(const char *label, const char *prefix)
{
    if (!label || !label[0])
        return false;
    if (!prefix || !prefix[0])
        return true;

    const size_t prefixLength = std::strlen(prefix);
    if (std::strncmp(label, prefix, prefixLength) != 0)
        return false;
    return label[prefixLength] == '\0' || label[prefixLength] == ':';
}

void FillFromLabels(HWND comboBox, const char *const *labels, const int count, const int selected)
{
    SendMessageA(comboBox, CB_RESETCONTENT, 0, 0);
    for (int index = 0; index < count; ++index)
        Win32ComboBox::ComboAddString(comboBox, labels[index]);
    Win32ComboBox::ComboSetIndex(comboBox, selected);
}

void FillCatalogVariant(HWND comboBox, const char *noneLabel, const char *itemPrefix,
                        const int itemCount, const int selected, const int raceIndex, const int slot)
{
    char label[128] = {};
    SendMessageA(comboBox, CB_RESETCONTENT, 0, 0);
    Win32ComboBox::ComboAddStringWithData(comboBox, noneLabel, 0);
    for (int index = 1; index <= itemCount; ++index)
    {
        const char *catalogLabel = FFXIInternal_FindPCLabel(raceIndex, slot, index);
        if (catalogLabel && catalogLabel[0])
            sprintf_s(label, "%03d - %s", index, catalogLabel);
        else
            sprintf_s(label, "%s %03d", itemPrefix, index);
        Win32ComboBox::ComboAddStringWithData(comboBox, label, index);
    }
    Win32ComboBox::ComboSetIndexByData(comboBox, selected);
}

void FillCatalogSparse(HWND comboBox, const char *noneLabel,
                       const int selected, const int raceIndex, const int slot)
{
    char label[160] = {};
    SendMessageA(comboBox, CB_RESETCONTENT, 0, 0);
    Win32ComboBox::ComboAddStringWithData(comboBox, noneLabel, 0);

    bool foundSelected = selected == 0;
    if (raceIndex >= 0 && raceIndex < static_cast<int>(sizeof(kFFXIInternalPCLists) / sizeof(kFFXIInternalPCLists[0])) &&
        slot >= 0 && slot < 11)
    {
        const FFXIInternalList &list = kFFXIInternalPCLists[raceIndex][slot];
        for (int index = 0; index < list.count; ++index)
        {
            const int itemIndex = list.entries[index].index;
            if (itemIndex <= 0 || !list.entries[index].label || !list.entries[index].label[0])
                continue;
            sprintf_s(label, "%03d - %s", itemIndex, list.entries[index].label);
            Win32ComboBox::ComboAddStringWithData(comboBox, label, itemIndex);
            if (itemIndex == selected)
                foundSelected = true;
        }
    }

    if (!foundSelected && selected > 0)
    {
        sprintf_s(label, "%03d - Custom DAT offset", selected);
        Win32ComboBox::ComboAddStringWithData(comboBox, label, selected);
    }

    Win32ComboBox::ComboSetIndexByData(comboBox, selected);
}

void FillCatalogBaseVariant(HWND comboBox, const char *itemPrefix,
                            const int itemCount, const int selected,
                            const int raceIndex, const int slot)
{
    char label[128] = {};
    SendMessageA(comboBox, CB_RESETCONTENT, 0, 0);
    for (int index = 0; index < itemCount; ++index)
    {
        const char *catalogLabel = FFXIInternal_FindPCLabel(raceIndex, slot, index);
        if (catalogLabel && catalogLabel[0])
            sprintf_s(label, "%03d - %s", index, catalogLabel);
        else
            sprintf_s(label, "%s %03d", itemPrefix, index);
        Win32ComboBox::ComboAddStringWithData(comboBox, label, index);
    }
    Win32ComboBox::ComboSetIndexByData(comboBox, selected);
}

void FillAnimationCategory(HWND comboBox, const int selected)
{
    SendMessageA(comboBox, CB_RESETCONTENT, 0, 0);
    for (int index = 0; index < AnimationCategoryCount(); ++index)
        Win32ComboBox::ComboAddString(comboBox, kAnimationCategories[index].label);
    Win32ComboBox::ComboSetIndex(comboBox, selected);
}

void FillAnimation(HWND comboBox, const int selected, const int raceIndex, const int categoryIndex)
{
    char label[192] = {};
    SendMessageA(comboBox, CB_RESETCONTENT, 0, 0);

    const AnimationCategory &category = AnimationCategoryForIndex(categoryIndex);
    bool foundSelected = false;
    if (raceIndex >= 0 && raceIndex < static_cast<int>(sizeof(kFFXIInternalPCLists) / sizeof(kFFXIInternalPCLists[0])) &&
        category.slot >= 0 && category.slot < 11)
    {
        const FFXIInternalList &list = kFFXIInternalPCLists[raceIndex][category.slot];
        for (int index = 0; index < list.count; ++index)
        {
            const int animationIndex = list.entries[index].index;
            const char *animationLabel = list.entries[index].label;
            if (!AnimationCategoryMatches(animationLabel, category.prefix))
                continue;

            sprintf_s(label, "%05d - %s", animationIndex,
                      animationLabel ? animationLabel : "Animation");
            Win32ComboBox::ComboAddStringWithData(comboBox, label, animationIndex);
            if (animationIndex == selected)
                foundSelected = true;
        }
    }

    if (!foundSelected && selected >= 0)
    {
        sprintf_s(label, "%05d - Custom animation DAT offset", selected);
        Win32ComboBox::ComboAddStringWithData(comboBox, label, selected);
    }

    if (SendMessageA(comboBox, CB_GETCOUNT, 0, 0) <= 0)
        Win32ComboBox::ComboAddStringWithData(comboBox, "00000 - Animation set 0", 0);

    Win32ComboBox::ComboSetIndexByData(comboBox, selected);
}

void FillFaceVariants(HWND comboBox, const int selected, const int faceCount)
{
    char label[64] = {};
    SendMessageA(comboBox, CB_RESETCONTENT, 0, 0);
    for (int index = 0; index < faceCount; ++index)
    {
        sprintf_s(label, "Face %d%c", (index / 2) + 1, (index & 1) ? 'B' : 'A');
        Win32ComboBox::ComboAddString(comboBox, label);
    }
    Win32ComboBox::ComboSetIndex(comboBox, selected);
}
}
