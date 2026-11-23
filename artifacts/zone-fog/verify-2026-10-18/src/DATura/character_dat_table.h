#pragma once

// FFXI Character DAT Table — corrected paths
//
// Paths derived from VTABLE.DAT / FTABLE.DAT file-ID resolution, matching the
// pcdat[] table in FFXI Tool's Krypton.cpp.  The previous table used ROM/63/
// which contains "DMB\0" formatted files unrelated to character geometry.
//
// Column semantics from pcdat[race][col]:
//   col 0  = Skeleton + base textures  (contains type 0x29 skeleton + 0x20 textures)
//   col 1  = Face mesh
//   col 2  = Head mesh
//   col 3  = Body slot  (naked body; +256=hands, +512=waist, +768=legs in file-ID space)
//   col 4  = Weapon
//   col 5  = Animation set 0
//   col 6  = Animation set 1
//
// Race order: 0=Hume M, 1=Hume F, 2=Elvaan M, 3=Elvaan F,
//             4=Tarutaru M, 5=Tarutaru F, 6=Mithra, 7=Galka

struct FFXICharEntry
{
    const char* label;
    const char* dat;   // path relative to FFXI root (forward slashes)
};

struct FFXICharRace
{
    const char*           name;
    const FFXICharEntry*  entries;
    int                   count;
};

// ---------------------------------------------------------------------------
// Hume Male
// ---------------------------------------------------------------------------
static const FFXICharEntry kHumeMEntries[] =
{
    { "Skeleton + Base Tex",  "ROM/27/82.dat"  },
    { "Face",                 "ROM/27/87.dat"  },
    { "Head",                 "ROM/27/103.dat" },
    { "Body (naked)",         "ROM/28/7.dat"   },
    { "Hands",                "ROM/28/52.dat"  },
    { "Waist",                "ROM/28/84.dat"  },
    { "Legs",                 "ROM/28/116.dat" },
    { "Weapon",               "ROM/29/20.dat"  },
    { "Animation set 0",      "ROM/32/13.dat"  },
    { "Animation set 1",      "ROM/32/40.dat"  },
};

// ---------------------------------------------------------------------------
// Hume Female
// ---------------------------------------------------------------------------
static const FFXICharEntry kHumeFEntries[] =
{
    { "Skeleton + Base Tex",  "ROM/32/58.dat"  },
    { "Face",                 "ROM/32/63.dat"  },
    { "Head",                 "ROM/32/79.dat"  },
    { "Body (naked)",         "ROM/32/111.dat" },
    { "Hands",                "ROM/33/28.dat"  },
    { "Waist",                "ROM/33/60.dat"  },
    { "Legs",                 "ROM/33/92.dat"  },
    { "Weapon",               "ROM/33/124.dat" },
    { "Animation set 0",      "ROM/36/117.dat" },
    { "Animation set 1",      "ROM/37/13.dat"  },
};

// ---------------------------------------------------------------------------
// Elvaan Male
// ---------------------------------------------------------------------------
static const FFXICharEntry kElvaanMEntries[] =
{
    { "Skeleton + Base Tex",  "ROM/37/31.dat"  },
    { "Face",                 "ROM/37/36.dat"  },
    { "Head",                 "ROM/37/52.dat"  },
    { "Body (naked)",         "ROM/37/83.dat"  },
    { "Hands",                "ROM/37/127.dat" },
    { "Waist",                "ROM/38/30.dat"  },
    { "Legs",                 "ROM/38/61.dat"  },
    { "Weapon",               "ROM/38/92.dat"  },
    { "Animation set 0",      "ROM/41/84.dat"  },
    { "Animation set 1",      "ROM/41/114.dat" },
};

// ---------------------------------------------------------------------------
// Elvaan Female
// ---------------------------------------------------------------------------
static const FFXICharEntry kElvaanFEntries[] =
{
    { "Skeleton + Base Tex",  "ROM/42/4.dat"   },
    { "Face",                 "ROM/42/9.dat"   },
    { "Head",                 "ROM/42/25.dat"  },
    { "Body (naked)",         "ROM/42/56.dat"  },
    { "Hands",                "ROM/42/100.dat" },
    { "Waist",                "ROM/43/3.dat"   },
    { "Legs",                 "ROM/43/34.dat"  },
    { "Weapon",               "ROM/43/65.dat"  },
    { "Animation set 0",      "ROM/46/57.dat"  },
    { "Animation set 1",      "ROM/46/75.dat"  },
};

// ---------------------------------------------------------------------------
// Tarutaru Male
// ---------------------------------------------------------------------------
static const FFXICharEntry kTarutaruMEntries[] =
{
    { "Skeleton + Base Tex",  "ROM/46/93.dat"  },
    { "Face",                 "ROM/46/98.dat"  },
    { "Head",                 "ROM/46/114.dat" },
    { "Body (naked)",         "ROM/47/17.dat"  },
    { "Hands",                "ROM/47/61.dat"  },
    { "Waist",                "ROM/47/92.dat"  },
    { "Legs",                 "ROM/47/123.dat" },
    { "Weapon",               "ROM/48/26.dat"  },
    { "Animation set 0",      "ROM/51/19.dat"  },
    { "Animation set 1",      "ROM/51/37.dat"  },
};

// ---------------------------------------------------------------------------
// Tarutaru Female
// ---------------------------------------------------------------------------
static const FFXICharEntry kTarutaruFEntries[] =
{
    { "Skeleton + Base Tex",  "ROM/46/93.dat"  },  // shared with Taru Male
    { "Face",                 "ROM/51/55.dat"  },
    { "Head",                 "ROM/46/114.dat" },  // shared with Taru Male
    { "Body (naked)",         "ROM/47/17.dat"  },  // shared with Taru Male
    { "Hands",                "ROM/47/61.dat"  },
    { "Waist",                "ROM/47/92.dat"  },
    { "Legs",                 "ROM/47/123.dat" },
    { "Weapon",               "ROM/48/26.dat"  },
    { "Animation set 0",      "ROM/51/19.dat"  },
    { "Animation set 1",      "ROM/51/71.dat"  },
};

// ---------------------------------------------------------------------------
// Mithra
// ---------------------------------------------------------------------------
static const FFXICharEntry kMithraEntries[] =
{
    { "Skeleton + Base Tex",  "ROM/51/89.dat"  },
    { "Face",                 "ROM/51/94.dat"  },
    { "Head",                 "ROM/51/94.dat"  },  // same as face for Mithra
    { "Body (naked)",         "ROM/52/13.dat"  },
    { "Hands",                "ROM/52/57.dat"  },
    { "Waist",                "ROM/52/88.dat"  },
    { "Legs",                 "ROM/52/119.dat" },
    { "Weapon",               "ROM/53/22.dat"  },
    { "Animation set 0",      "ROM/56/14.dat"  },
    { "Animation set 1",      "ROM/56/41.dat"  },
};

// ---------------------------------------------------------------------------
// Galka
// ---------------------------------------------------------------------------
static const FFXICharEntry kGalkaEntries[] =
{
    { "Skeleton + Base Tex",  "ROM/56/59.dat"  },
    { "Face",                 "ROM/56/64.dat"  },
    { "Head",                 "ROM/56/80.dat"  },
    { "Body (naked)",         "ROM/56/111.dat" },
    { "Hands",                "ROM/57/27.dat"  },
    { "Waist",                "ROM/57/58.dat"  },
    { "Legs",                 "ROM/57/89.dat"  },
    { "Weapon",               "ROM/57/120.dat" },
    { "Animation set 0",      "ROM/60/112.dat" },
    { "Animation set 1",      "ROM/61/8.dat"   },
};

// ---------------------------------------------------------------------------
// Master race table
// ---------------------------------------------------------------------------
#define FFXI_CHAR_ENTRY_COUNT(arr) (int)(sizeof(arr)/sizeof(arr[0]))

static const FFXICharRace kFFXICharRaces[] =
{
    { "Hume Male",      kHumeMEntries,     FFXI_CHAR_ENTRY_COUNT(kHumeMEntries)     },
    { "Hume Female",    kHumeFEntries,     FFXI_CHAR_ENTRY_COUNT(kHumeFEntries)     },
    { "Elvaan Male",    kElvaanMEntries,   FFXI_CHAR_ENTRY_COUNT(kElvaanMEntries)   },
    { "Elvaan Female",  kElvaanFEntries,   FFXI_CHAR_ENTRY_COUNT(kElvaanFEntries)   },
    { "Tarutaru Male",  kTarutaruMEntries, FFXI_CHAR_ENTRY_COUNT(kTarutaruMEntries) },
    { "Tarutaru Female",kTarutaruFEntries, FFXI_CHAR_ENTRY_COUNT(kTarutaruFEntries) },
    { "Mithra",         kMithraEntries,    FFXI_CHAR_ENTRY_COUNT(kMithraEntries)    },
    { "Galka",          kGalkaEntries,     FFXI_CHAR_ENTRY_COUNT(kGalkaEntries)     },
};
static const int kFFXICharRaceCount = (int)(sizeof(kFFXICharRaces)/sizeof(kFFXICharRaces[0]));

static inline int FFXIChar_TotalEntries()
{
    int total = 0;
    for (int r = 0; r < kFFXICharRaceCount; ++r)
    {
        total += kFFXICharRaces[r].count;
    }
    return total;
}

static inline const FFXICharEntry* FFXIChar_GetEntry(int flatIndex)
{
    if (flatIndex < 0)
        return nullptr;

    for (int r = 0; r < kFFXICharRaceCount; ++r)
    {
        const FFXICharRace& race = kFFXICharRaces[r];
        if (flatIndex < race.count)
            return &race.entries[flatIndex];

        flatIndex -= race.count;
    }

    return nullptr;
}
