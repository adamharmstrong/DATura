#pragma once

// Model-facing prototype areas that are not represented by stable retail zone IDs.
// These entries are loaded directly from their DAT paths.
struct FFXIPrototypeAreaEntry
{
    const char* name;
    const char* modelDat;
};

static const FFXIPrototypeAreaEntry kFFXIPrototypeAreas[] =
{
    { "Sel Phiner - Exterior / Monorail", "ROM/0/28.DAT" },
    { "Sel Phiner - Town",                "ROM/0/33.DAT" },

    // Historical aliases intentionally use direct model paths. This keeps the
    // character-creation association out of the retail zone table and avoids
    // assigning a second zone ID to the same Sel Phiner environment.
    { "Character Creation (Sel Phiner)",   "ROM/0/28.DAT" },

    // The Last Stand occupied the slot later released as zone 183,
    // Maquette Abdhaljs-Legion. Its old logical resource ID (6603) no longer
    // resolves to this model in a current client, while ROM/26/120.DAT is an
    // auxiliary zone DAT rather than displayable geometry.
    { "The Last Stand (Legacy / Legion)",  "ROM/1/12.DAT" },
};

static const int kFFXIPrototypeAreaCount =
    (int)(sizeof(kFFXIPrototypeAreas) / sizeof(kFFXIPrototypeAreas[0]));
