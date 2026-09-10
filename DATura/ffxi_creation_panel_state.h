#pragma once

#include "ffxi_creation_selection.h"

#include <windows.h>

constexpr int IDC_HP_RACE = 8200;
constexpr int IDC_HP_FACE = 8201;
constexpr int IDC_HP_EQUIPMENT = 8202;
constexpr int IDC_HP_NAME = 8203;
constexpr int IDC_HP_SAVE = 8204;
constexpr int IDC_HP_ANIMATION = 8205;
constexpr int IDC_HP_NEXT = 8206;
constexpr int IDC_HP_TITLE = 8207;
constexpr int IDC_HP_ANIMATED_CAMERA = 8208;

namespace FFXICreationPanelState
{
void CreateControls(HWND panel, const char* characterName);
void SyncControls(HWND panel, const FFXICreationSelection& selection, int animationIndex,
                  bool animatedCamera);
void PullControls(HWND panel, FFXICreationSelection& selection, int& animationIndex,
                  bool& animatedCamera);
}
