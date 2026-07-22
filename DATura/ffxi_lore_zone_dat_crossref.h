#pragma once

// FFXIclopedia lore-region to DAT path cross-reference.
// Generated from tools/ffxiclopedia_lore_zone_dat_crossref.csv.

struct FFXILoreZoneDatEntry
{
    int         wikiOrder;
    const char* continent;
    const char* loreRegion;
    const char* ffxiclopediaZoneName;
    int         daturaZoneId;
    const char* daturaZoneName;
    const char* modelDatFolder;
    const char* modelDat;
    const char* dialogDat;
    const char* npcDat;
    const char* eventDat;
    const char* source;
    const char* note;
};

extern const FFXILoreZoneDatEntry kFFXILoreZoneDatTable[];
extern const int kFFXILoreZoneDatCount;

const FFXILoreZoneDatEntry* FFXILoreZoneDat_FindByZoneID(int zoneId);
const FFXILoreZoneDatEntry* FFXILoreZoneDat_FindByWikiName(const char* wikiName);
