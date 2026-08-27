#pragma once

#include "ffxi_player_customization_state.h"

#include <windows.h>

constexpr int IDC_LP_RACE = 8100;
constexpr int IDC_LP_MAIN_TYPE = 8101;
constexpr int IDC_LP_MAIN_ITEM = 8102;
constexpr int IDC_LP_SUB_TYPE = 8103;
constexpr int IDC_LP_SUB_ITEM = 8104;
constexpr int IDC_LP_RANGE_TYPE = 8105;
constexpr int IDC_LP_RANGE_ITEM = 8106;
constexpr int IDC_LP_FACE = 8107;
constexpr int IDC_LP_HEAD = 8108;
constexpr int IDC_LP_BODY = 8109;
constexpr int IDC_LP_HANDS = 8110;
constexpr int IDC_LP_LEGS = 8111;
constexpr int IDC_LP_FEET = 8112;
constexpr int IDC_LP_ANIM_BANK = 8113;
constexpr int IDC_LP_ANIM_MODE = 8114;
constexpr int IDC_LP_LOAD = 8115;
constexpr int IDC_LP_SAVE = 8116;
constexpr int IDC_LP_PLAY = 8117;
constexpr int IDC_LP_STOP = 8118;
constexpr int IDC_LP_RESET = 8119;
constexpr int IDC_LP_RANDOM_CHARACTER = 8120;
constexpr int IDC_LP_RANDOM_WEAPONS = 8121;
constexpr int IDC_LP_RANDOM_ARMOR = 8122;
constexpr int IDC_LP_RANDOM_ACTION = 8123;
constexpr int IDC_LP_RANDOM_ALL = 8124;

namespace FFXIPlayerCustomizationPanelState
{
void CreateControls(HWND panel);
void SyncControls(HWND panel, PlayerEquipState& equipment, int& faceVariant);
void PullControls(HWND panel, PlayerEquipState& equipment, int& faceVariant);
}
