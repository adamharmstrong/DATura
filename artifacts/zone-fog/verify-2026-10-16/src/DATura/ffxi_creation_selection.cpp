#include "stdafx.h"
#include "ffxi_creation_selection.h"

#include "noesis_rapi.h"
#include "model_ff11.h"
#include "character_creation_dat_table.h"

namespace FFXICreationSelectionState
{
const FFXICreationEntry* CurrentEntry(FFXICreationSelection& selection)
{
    if (selection.raceIndex < 0 || selection.raceIndex >= kFFXICreationRaceCount)
        selection.raceIndex = 0;

    const FFXICreationRace& race = kFFXICreationRaces[selection.raceIndex];
    const int faceCount = race.count / 2;
    if (selection.faceIndex < 0)
        selection.faceIndex = 0;
    if (selection.faceIndex >= faceCount)
        selection.faceIndex = faceCount - 1;
    if (selection.equipmentIndex < 0 || selection.equipmentIndex > 1)
        selection.equipmentIndex = 0;

    const int entryIndex = selection.faceIndex * 2 + selection.equipmentIndex;
    return (entryIndex >= 0 && entryIndex < race.count) ? &race.entries[entryIndex] : nullptr;
}

bool SetFromFlatIndex(int flatIndex, FFXICreationSelection& selection)
{
    if (flatIndex < 0)
        return false;

    for (int raceIndex = 0; raceIndex < kFFXICreationRaceCount; ++raceIndex)
    {
        const FFXICreationRace& race = kFFXICreationRaces[raceIndex];
        if (flatIndex < race.count)
        {
            selection.raceIndex = raceIndex;
            selection.faceIndex = flatIndex / 2;
            selection.equipmentIndex = flatIndex % 2;
            return true;
        }
        flatIndex -= race.count;
    }

    return false;
}
}
