#pragma once

struct FFXICreationEntry;

struct FFXICreationSelection
{
    int raceIndex = 0;
    int faceIndex = 0;
    int equipmentIndex = 0;
};

namespace FFXICreationSelectionState
{
const FFXICreationEntry* CurrentEntry(FFXICreationSelection& selection);
bool SetFromFlatIndex(int flatIndex, FFXICreationSelection& selection);
}
