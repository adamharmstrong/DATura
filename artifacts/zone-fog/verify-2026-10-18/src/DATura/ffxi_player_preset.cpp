#include "stdafx.h"
#include "ffxi_player_preset.h"

#include <cstdio>
#include <cstring>

namespace
{
bool ReadInt(const char* text, const char* key, int* outValue)
{
    const char* p = std::strstr(text, key);
    if (!p)
        return false;
    p += std::strlen(key);
    while (*p == ' ' || *p == '\t')
        ++p;
    return sscanf_s(p, "%d", outValue) == 1;
}
}

namespace FFXIPlayerPreset
{
void BuildText(const Data& preset, char* outText, std::size_t outTextSize)
{
    sprintf_s(outText, outTextSize,
        "DATURA_PLAYER_PRESET 1\n"
        "race %d\nface %d\nmainType %d\nmainItem %d\nsubType %d\nsubItem %d\n"
        "rangedType %d\nrangedItem %d\nhead %d\nbody %d\nhands %d\nlegs %d\nfeet %d\n"
        "animBank %d\nanimMode %d\n",
        preset.raceIndex, preset.faceVariant, preset.mainType, preset.mainItem,
        preset.subType, preset.subItem, preset.rangedType, preset.rangedItem,
        preset.headItem, preset.bodyItem, preset.handsItem, preset.legsItem, preset.feetItem,
        preset.animationBank, preset.animationMode);
}

bool ParseText(const char* text, Data* inOutPreset)
{
    if (!text || !inOutPreset || std::strncmp(text, "DATURA_PLAYER_PRESET", 20) != 0)
        return false;

    ReadInt(text, "race", &inOutPreset->raceIndex);
    ReadInt(text, "face", &inOutPreset->faceVariant);
    ReadInt(text, "mainType", &inOutPreset->mainType);
    ReadInt(text, "mainItem", &inOutPreset->mainItem);
    ReadInt(text, "subType", &inOutPreset->subType);
    ReadInt(text, "subItem", &inOutPreset->subItem);
    ReadInt(text, "rangedType", &inOutPreset->rangedType);
    ReadInt(text, "rangedItem", &inOutPreset->rangedItem);
    ReadInt(text, "head", &inOutPreset->headItem);
    ReadInt(text, "body", &inOutPreset->bodyItem);
    ReadInt(text, "hands", &inOutPreset->handsItem);
    ReadInt(text, "legs", &inOutPreset->legsItem);
    ReadInt(text, "feet", &inOutPreset->feetItem);
    ReadInt(text, "animBank", &inOutPreset->animationBank);
    ReadInt(text, "animMode", &inOutPreset->animationMode);
    return true;
}
}
