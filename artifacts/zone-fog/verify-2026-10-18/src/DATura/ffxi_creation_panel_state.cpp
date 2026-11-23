#include "stdafx.h"
#include "ffxi_creation_panel_state.h"

#include "noesis_rapi.h"
#include "model_ff11.h"
#include "character_creation_dat_table.h"
#include "win32_combo_box.h"
#include "win32_panel_controls.h"

namespace
{
HWND PanelControl(const HWND panel, const int controlId)
{
    return panel ? GetDlgItem(panel, controlId) : NULL;
}
}

namespace FFXICreationPanelState
{
void CreateControls(const HWND panel, const char* characterName)
{
    if (!panel)
        return;

    Win32PanelControls::AddPanelControl(
        panel, "BUTTON", "Character Creation", BS_GROUPBOX, -1, 8, 8, 344, 268);
    Win32PanelControls::AddPanelControl(
        panel, "STATIC", "Name:", 0, -1, 20, 36, 82, 18);
    Win32PanelControls::AddPanelControl(
        panel, "EDIT", characterName,
        WS_TABSTOP | ES_AUTOHSCROLL | WS_BORDER, IDC_HP_NAME, 108, 32, 232, 22);
    Win32PanelControls::AddPanelControl(
        panel, "STATIC", "Race:", 0, -1, 20, 76, 82, 18);
    Win32PanelControls::AddPanelCombo(panel, IDC_HP_RACE, 108, 72, 232);
    Win32PanelControls::AddPanelControl(
        panel, "STATIC", "Face:", 0, -1, 20, 116, 82, 18);
    Win32PanelControls::AddPanelCombo(panel, IDC_HP_FACE, 108, 112, 232);
    Win32PanelControls::AddPanelControl(
        panel, "STATIC", "Equipment:", 0, -1, 20, 156, 82, 18);
    Win32PanelControls::AddPanelCombo(panel, IDC_HP_EQUIPMENT, 108, 152, 232);
    Win32PanelControls::AddPanelControl(
        panel, "STATIC", "Animation:", 0, -1, 20, 196, 82, 18);
    Win32PanelControls::AddPanelCombo(panel, IDC_HP_ANIMATION, 108, 192, 232);
    Win32PanelControls::AddPanelControl(
        panel, "BUTTON", "Use animated camera", BS_AUTOCHECKBOX | WS_TABSTOP,
        IDC_HP_ANIMATED_CAMERA, 108, 228, 232, 22);
    Win32PanelControls::AddPanelControl(
        panel, "BUTTON", "Save Character...", BS_PUSHBUTTON,
        IDC_HP_SAVE, 66, 286, 132, 26);
    Win32PanelControls::AddPanelControl(
        panel, "BUTTON", "Choose Nation...", BS_PUSHBUTTON,
        IDC_HP_NEXT, 208, 286, 132, 26);
    Win32PanelControls::AddPanelControl(
        panel, "BUTTON", "Return to Title", BS_PUSHBUTTON,
        IDC_HP_TITLE, 66, 318, 274, 26);
}

void SyncControls(const HWND panel, const FFXICreationSelection& selection,
                  const int animationIndex, const bool animatedCamera)
{
    if (!panel)
        return;

    HWND raceControl = PanelControl(panel, IDC_HP_RACE);
    SendMessageA(raceControl, CB_RESETCONTENT, 0, 0);
    for (int raceIndex = 0; raceIndex < kFFXICreationRaceCount; ++raceIndex)
        Win32ComboBox::ComboAddString(raceControl, kFFXICreationRaces[raceIndex].name);
    Win32ComboBox::ComboSetIndex(raceControl, selection.raceIndex);

    const FFXICreationRace& race = kFFXICreationRaces[selection.raceIndex];
    HWND faceControl = PanelControl(panel, IDC_HP_FACE);
    SendMessageA(faceControl, CB_RESETCONTENT, 0, 0);
    for (int faceIndex = 0; faceIndex < race.count / 2; ++faceIndex)
    {
        char label[64] = {};
        sprintf_s(label, "Face %d%s", (faceIndex / 2) + 1,
                  (faceIndex & 1) ? "B" : "A");
        Win32ComboBox::ComboAddString(faceControl, label);
    }
    Win32ComboBox::ComboSetIndex(faceControl, selection.faceIndex);

    HWND equipmentControl = PanelControl(panel, IDC_HP_EQUIPMENT);
    SendMessageA(equipmentControl, CB_RESETCONTENT, 0, 0);
    Win32ComboBox::ComboAddString(equipmentControl, "No Equipment");
    Win32ComboBox::ComboAddString(equipmentControl, "Initial Equipment");
    Win32ComboBox::ComboSetIndex(equipmentControl, selection.equipmentIndex);

    HWND animationControl = PanelControl(panel, IDC_HP_ANIMATION);
    SendMessageA(animationControl, CB_RESETCONTENT, 0, 0);
    Win32ComboBox::ComboAddString(animationControl, "A-pose");
    Win32ComboBox::ComboAddString(animationControl, "Standing idle");
    Win32ComboBox::ComboAddString(animationControl, "Character creation sequence");
    Win32ComboBox::ComboSetIndex(animationControl, animationIndex);

    HWND cameraControl = PanelControl(panel, IDC_HP_ANIMATED_CAMERA);
    SendMessageA(cameraControl, BM_SETCHECK,
                 animatedCamera ? BST_CHECKED : BST_UNCHECKED, 0);
    EnableWindow(cameraControl, animationIndex == 2);
}

void PullControls(const HWND panel, FFXICreationSelection& selection, int& animationIndex,
                  bool& animatedCamera)
{
    if (!panel)
        return;

    const int previousRace = selection.raceIndex;
    selection.raceIndex = Win32ComboBox::ComboGetIndex(PanelControl(panel, IDC_HP_RACE));
    if (selection.raceIndex != previousRace)
        selection.faceIndex = 0;
    else
        selection.faceIndex = Win32ComboBox::ComboGetIndex(PanelControl(panel, IDC_HP_FACE));
    selection.equipmentIndex = Win32ComboBox::ComboGetIndex(
        PanelControl(panel, IDC_HP_EQUIPMENT));
    animationIndex = Win32ComboBox::ComboGetIndex(PanelControl(panel, IDC_HP_ANIMATION));
    animatedCamera = SendMessageA(PanelControl(panel, IDC_HP_ANIMATED_CAMERA),
                                  BM_GETCHECK, 0, 0) == BST_CHECKED;
}
}
