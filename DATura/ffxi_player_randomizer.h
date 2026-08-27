#pragma once

#include "ffxi_player_customization_state.h"

namespace FFXIPlayerRandomizer
{
void RandomizeCharacter(PlayerEquipState& equipment, int& faceVariant);
void RandomizeWeapons(PlayerEquipState& equipment);
void RandomizeArmor(PlayerEquipState& equipment);
void RandomizeAction(PlayerEquipState& equipment, int& faceVariant);
void RandomizeAll(PlayerEquipState& equipment, int& faceVariant);
}
