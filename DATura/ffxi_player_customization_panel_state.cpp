#include "stdafx.h"
#include "ffxi_player_customization_panel_state.h"

#include "character_dat_table.h"
#include "ffxi_internal_lists.h"
#include "ffxi_player_customization_catalog.h"
#include "win32_combo_box.h"
#include "win32_panel_controls.h"

namespace
{
const char* const kWeaponTypeLabels[] =
{
    "(All)", "Sword", "Dagger", "Axe", "Scythe", "Polearm",
    "Katana", "Club", "Staff", "Bow", "Gun", "Shield"
};

constexpr int kWeaponTypeCount =
    (int)(sizeof(kWeaponTypeLabels) / sizeof(kWeaponTypeLabels[0]));

HWND PanelControl(const HWND panel, const int controlId)
{
    return panel ? GetDlgItem(panel, controlId) : NULL;
}
}

namespace FFXIPlayerCustomizationPanelState
{
void CreateControls(const HWND panel)
{
    if (!panel)
        return;

    Win32PanelControls::AddPanelControl(
        panel, "BUTTON", "Character", BS_GROUPBOX, -1, 8, 8, 448, 94);
    Win32PanelControls::AddPanelControl(
        panel, "STATIC", "Race:", 0, -1, 18, 36, 66, 18);
    Win32PanelControls::AddPanelCombo(panel, IDC_LP_RACE, 98, 32, 350);
    Win32PanelControls::AddPanelControl(
        panel, "STATIC", "Face:", 0, -1, 18, 68, 66, 18);
    Win32PanelControls::AddPanelCombo(panel, IDC_LP_FACE, 98, 64, 350);

    Win32PanelControls::AddPanelControl(
        panel, "BUTTON", "Weapons", BS_GROUPBOX, -1, 8, 110, 448, 158);
    Win32PanelControls::AddPanelControl(
        panel, "STATIC", "Main:", 0, -1, 18, 138, 66, 18);
    Win32PanelControls::AddPanelCombo(panel, IDC_LP_MAIN_TYPE, 18, 156, 68);
    Win32PanelControls::AddPanelCombo(panel, IDC_LP_MAIN_ITEM, 98, 156, 350);
    Win32PanelControls::AddPanelControl(
        panel, "STATIC", "Sub:", 0, -1, 18, 184, 66, 18);
    Win32PanelControls::AddPanelCombo(panel, IDC_LP_SUB_TYPE, 18, 202, 68);
    Win32PanelControls::AddPanelCombo(panel, IDC_LP_SUB_ITEM, 98, 202, 350);
    Win32PanelControls::AddPanelControl(
        panel, "STATIC", "Ranged:", 0, -1, 18, 230, 66, 18);
    Win32PanelControls::AddPanelCombo(panel, IDC_LP_RANGE_TYPE, 18, 248, 68);
    Win32PanelControls::AddPanelCombo(panel, IDC_LP_RANGE_ITEM, 98, 248, 350);

    Win32PanelControls::AddPanelControl(
        panel, "BUTTON", "Armor", BS_GROUPBOX, -1, 8, 276, 448, 176);
    Win32PanelControls::AddPanelControl(
        panel, "STATIC", "Head:", 0, -1, 18, 304, 66, 18);
    Win32PanelControls::AddPanelCombo(panel, IDC_LP_HEAD, 98, 300, 350);
    Win32PanelControls::AddPanelControl(
        panel, "STATIC", "Body:", 0, -1, 18, 332, 66, 18);
    Win32PanelControls::AddPanelCombo(panel, IDC_LP_BODY, 98, 328, 350);
    Win32PanelControls::AddPanelControl(
        panel, "STATIC", "Hands:", 0, -1, 18, 360, 66, 18);
    Win32PanelControls::AddPanelCombo(panel, IDC_LP_HANDS, 98, 356, 350);
    Win32PanelControls::AddPanelControl(
        panel, "STATIC", "Legs:", 0, -1, 18, 388, 66, 18);
    Win32PanelControls::AddPanelCombo(panel, IDC_LP_LEGS, 98, 384, 350);
    Win32PanelControls::AddPanelControl(
        panel, "STATIC", "Feet:", 0, -1, 18, 416, 66, 18);
    Win32PanelControls::AddPanelCombo(panel, IDC_LP_FEET, 98, 412, 350);

    Win32PanelControls::AddPanelControl(
        panel, "BUTTON", "Action", BS_GROUPBOX, -1, 8, 460, 448, 82);
    Win32PanelControls::AddPanelCombo(panel, IDC_LP_ANIM_MODE, 18, 488, 100);
    Win32PanelControls::AddPanelCombo(panel, IDC_LP_ANIM_BANK, 126, 488, 222);
    Win32PanelControls::AddPanelControl(
        panel, "BUTTON", "Play", BS_PUSHBUTTON, IDC_LP_PLAY, 356, 488, 92, 24);
    Win32PanelControls::AddPanelControl(
        panel, "BUTTON", "Stop", BS_PUSHBUTTON, IDC_LP_STOP, 356, 516, 44, 24);
    Win32PanelControls::AddPanelControl(
        panel, "BUTTON", "Reset", BS_PUSHBUTTON, IDC_LP_RESET, 404, 516, 44, 24);

    Win32PanelControls::AddPanelControl(
        panel, "BUTTON", "Randomize", BS_GROUPBOX, -1, 8, 550, 448, 54);
    Win32PanelControls::AddPanelControl(
        panel, "BUTTON", "Character", BS_PUSHBUTTON,
        IDC_LP_RANDOM_CHARACTER, 18, 574, 82, 24);
    Win32PanelControls::AddPanelControl(
        panel, "BUTTON", "Weapons", BS_PUSHBUTTON,
        IDC_LP_RANDOM_WEAPONS, 104, 574, 82, 24);
    Win32PanelControls::AddPanelControl(
        panel, "BUTTON", "Armor", BS_PUSHBUTTON,
        IDC_LP_RANDOM_ARMOR, 190, 574, 82, 24);
    Win32PanelControls::AddPanelControl(
        panel, "BUTTON", "Action", BS_PUSHBUTTON,
        IDC_LP_RANDOM_ACTION, 276, 574, 82, 24);
    Win32PanelControls::AddPanelControl(
        panel, "BUTTON", "All", BS_PUSHBUTTON,
        IDC_LP_RANDOM_ALL, 362, 574, 86, 24);

    Win32PanelControls::AddPanelControl(
        panel, "BUTTON", "Preset", BS_GROUPBOX, -1, 8, 612, 448, 54);
    Win32PanelControls::AddPanelControl(
        panel, "BUTTON", "Load", BS_PUSHBUTTON, IDC_LP_LOAD, 304, 634, 68, 24);
    Win32PanelControls::AddPanelControl(
        panel, "BUTTON", "Save", BS_PUSHBUTTON, IDC_LP_SAVE, 380, 634, 68, 24);
}

void SyncControls(const HWND panel, PlayerEquipState& equipment, int& faceVariant)
{
    if (!panel)
        return;

    FFXIPlayerCustomizationState::Clamp(equipment, faceVariant);
    HWND raceControl = PanelControl(panel, IDC_LP_RACE);
    SendMessageA(raceControl, CB_RESETCONTENT, 0, 0);
    for (int raceIndex = 0; raceIndex < kFFXICharRaceCount; ++raceIndex)
        Win32ComboBox::ComboAddString(raceControl, kFFXICharRaces[raceIndex].name);
    Win32ComboBox::ComboSetIndex(raceControl, equipment.raceIndex);

    FFXIPlayerCustomizationCatalog::FillFaceVariants(
        PanelControl(panel, IDC_LP_FACE), faceVariant,
        FFXIPlayerCustomizationState::FaceVariantCountForRace(equipment.raceIndex));

    FFXIPlayerCustomizationCatalog::FillFromLabels(
        PanelControl(panel, IDC_LP_MAIN_TYPE), kWeaponTypeLabels,
        kWeaponTypeCount, equipment.mainType);
    FFXIPlayerCustomizationCatalog::FillFromLabels(
        PanelControl(panel, IDC_LP_SUB_TYPE), kWeaponTypeLabels,
        kWeaponTypeCount, equipment.subType);
    FFXIPlayerCustomizationCatalog::FillFromLabels(
        PanelControl(panel, IDC_LP_RANGE_TYPE), kWeaponTypeLabels,
        kWeaponTypeCount, equipment.rangedType);
    FFXIPlayerCustomizationCatalog::FillAnimationCategory(
        PanelControl(panel, IDC_LP_ANIM_MODE), equipment.animationMode);

    const int raceIndex = equipment.raceIndex;
    FFXIPlayerCustomizationCatalog::FillCatalogSparse(
        PanelControl(panel, IDC_LP_MAIN_ITEM), "None",
        equipment.mainItem, raceIndex, kFFXIInternalPCSlot_Main);
    FFXIPlayerCustomizationCatalog::FillCatalogSparse(
        PanelControl(panel, IDC_LP_SUB_ITEM), "None",
        equipment.subItem, raceIndex, kFFXIInternalPCSlot_Sub);
    FFXIPlayerCustomizationCatalog::FillCatalogSparse(
        PanelControl(panel, IDC_LP_RANGE_ITEM), "None",
        equipment.rangedItem, raceIndex, kFFXIInternalPCSlot_Range);
    FFXIPlayerCustomizationCatalog::FillCatalogVariant(
        PanelControl(panel, IDC_LP_HEAD), "None", "Head",
        FFXIPlayerCustomizationState::ArmorVariantCount,
        equipment.headItem, raceIndex, kFFXIInternalPCSlot_Head);
    FFXIPlayerCustomizationCatalog::FillCatalogBaseVariant(
        PanelControl(panel, IDC_LP_BODY), "Body",
        FFXIPlayerCustomizationState::ArmorVariantCount,
        equipment.bodyItem, raceIndex, kFFXIInternalPCSlot_Body);
    FFXIPlayerCustomizationCatalog::FillCatalogBaseVariant(
        PanelControl(panel, IDC_LP_HANDS), "Hands",
        FFXIPlayerCustomizationState::ArmorVariantCount,
        equipment.handsItem, raceIndex, kFFXIInternalPCSlot_Hands);
    FFXIPlayerCustomizationCatalog::FillCatalogBaseVariant(
        PanelControl(panel, IDC_LP_LEGS), "Legs",
        FFXIPlayerCustomizationState::ArmorVariantCount,
        equipment.legsItem, raceIndex, kFFXIInternalPCSlot_Legs);
    FFXIPlayerCustomizationCatalog::FillCatalogBaseVariant(
        PanelControl(panel, IDC_LP_FEET), "Feet",
        FFXIPlayerCustomizationState::ArmorVariantCount,
        equipment.feetItem, raceIndex, kFFXIInternalPCSlot_Feet);

    FFXIPlayerCustomizationCatalog::FillAnimation(
        PanelControl(panel, IDC_LP_ANIM_BANK), equipment.animationBank,
        raceIndex, equipment.animationMode);
}

void PullControls(const HWND panel, PlayerEquipState& equipment, int& faceVariant)
{
    if (!panel)
        return;

    equipment.raceIndex = Win32ComboBox::ComboGetIndex(PanelControl(panel, IDC_LP_RACE));
    faceVariant = Win32ComboBox::ComboGetIndex(PanelControl(panel, IDC_LP_FACE));
    equipment.mainType = Win32ComboBox::ComboGetIndex(PanelControl(panel, IDC_LP_MAIN_TYPE));
    equipment.mainItem = Win32ComboBox::ComboGetSelectedData(
        PanelControl(panel, IDC_LP_MAIN_ITEM));
    equipment.subType = Win32ComboBox::ComboGetIndex(PanelControl(panel, IDC_LP_SUB_TYPE));
    equipment.subItem = Win32ComboBox::ComboGetSelectedData(
        PanelControl(panel, IDC_LP_SUB_ITEM));
    equipment.rangedType = Win32ComboBox::ComboGetIndex(
        PanelControl(panel, IDC_LP_RANGE_TYPE));
    equipment.rangedItem = Win32ComboBox::ComboGetSelectedData(
        PanelControl(panel, IDC_LP_RANGE_ITEM));
    equipment.headItem = Win32ComboBox::ComboGetSelectedData(
        PanelControl(panel, IDC_LP_HEAD));
    equipment.bodyItem = Win32ComboBox::ComboGetSelectedData(
        PanelControl(panel, IDC_LP_BODY));
    equipment.handsItem = Win32ComboBox::ComboGetSelectedData(
        PanelControl(panel, IDC_LP_HANDS));
    equipment.legsItem = Win32ComboBox::ComboGetSelectedData(
        PanelControl(panel, IDC_LP_LEGS));
    equipment.feetItem = Win32ComboBox::ComboGetSelectedData(
        PanelControl(panel, IDC_LP_FEET));
    equipment.animationMode = Win32ComboBox::ComboGetIndex(
        PanelControl(panel, IDC_LP_ANIM_MODE));
    equipment.animationBank = Win32ComboBox::ComboGetSelectedData(
        PanelControl(panel, IDC_LP_ANIM_BANK));
    FFXIPlayerCustomizationState::Clamp(equipment, faceVariant);
}
}
