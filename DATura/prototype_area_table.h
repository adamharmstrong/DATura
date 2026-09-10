#pragma once

// Model-facing prototype areas that are not represented by stable retail zone IDs.
// These entries are loaded directly from their DAT paths.
struct FFXIPrototypeAreaEntry
{
    const char* name;
    const char* modelDat;
};

// The environment shown during retail character selection. It is distinct
// from the similarly themed Sel Phiner prototype terrain in ROM/0/28.DAT.
static const char kFFXICharacterCreationZoneDat[] = "ROM/1/5.DAT";
static const char kFFXISelPhinerExteriorDat[] = "ROM/0/28.DAT";

static const FFXIPrototypeAreaEntry kFFXIPrototypeAreas[] =
{
    { "Sel Phiner - Exterior / Monorail", kFFXISelPhinerExteriorDat },
    { "Sel Phiner - Town",                "ROM/0/33.DAT" },

    { "Character Selection / Creation",    kFFXICharacterCreationZoneDat },

    // The Last Stand occupied the slot later released as zone 183,
    // Maquette Abdhaljs-Legion. Its old logical resource ID (6603) no longer
    // resolves to this model in a current client, while ROM/26/120.DAT is an
    // auxiliary zone DAT rather than displayable geometry.
    { "The Last Stand (Legacy / Legion)",  "ROM/1/12.DAT" },
};

static const int kFFXIPrototypeAreaCount =
    (int)(sizeof(kFFXIPrototypeAreas) / sizeof(kFFXIPrototypeAreas[0]));
