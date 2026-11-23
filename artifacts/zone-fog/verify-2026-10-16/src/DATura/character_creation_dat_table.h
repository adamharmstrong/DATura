#pragma once

// High-poly player character DATs used by the character creation viewer.
// Most face choices are stored as sequential DMB material + RT/SHAPE mesh pairs.

struct FFXICreationEntry
{
    const char *label;
    const char *bodyMeshDat;
    const char *bodyMaterialDat;
    const char *headMeshDat;
    const char *headMaterialDat;
    int headAlphaMode;
    float headYOffset;
};

struct FFXICreationRace
{
    const char *name;
    const FFXICreationEntry *entries;
    int count;
};

#define FFXI_CREATION_ENTRY_COUNT(arr) (int)(sizeof(arr) / sizeof(arr[0]))

#define CREATION_FACE_SET(label, bodyMesh, bodyMat, headMesh, headMat, alphaMode, yOffset) \
    { "No Equipment + " label,      bodyMesh, bodyMat, headMesh, headMat, alphaMode, yOffset }, \
    { "Initial Equipment + " label, bodyMesh, bodyMat, headMesh, headMat, alphaMode, yOffset }

#define CREATION_FACE_PAIR(faceNum, bodyMesh, bodyMat, headMesh, headMatA, headMatB, alphaMode, yOffset) \
    CREATION_FACE_SET("Face " faceNum "A", bodyMesh, bodyMat, headMesh, headMatA, alphaMode, yOffset), \
    CREATION_FACE_SET("Face " faceNum "B", bodyMesh, bodyMat, headMesh, headMatB, alphaMode, yOffset)

#define CREATION_HUME_F_FACE_PAIR(faceNum, headMesh, headMatA, headMatB, yOffset) \
    CREATION_FACE_PAIR(faceNum, "ROM/63/61.dat", "ROM/63/60.dat", headMesh, headMatA, headMatB, FFXI_CREATION_ALPHA_HUMANOID_HEAD, yOffset)

#define CREATION_ELVAAN_F_FACE_PAIR(faceNum, headMesh, headMatA, headMatB, yOffset) \
    CREATION_FACE_PAIR(faceNum, "ROM/63/1.dat", "ROM/63/0.dat", headMesh, headMatA, headMatB, FFXI_CREATION_ALPHA_ELVAAN_F_HEAD, yOffset)

static const FFXICreationEntry kCreationHumeMEntries[] =
{
    CREATION_FACE_PAIR("1", "ROM/63/81.dat", "ROM/63/80.dat", "ROM/63/85.dat", "ROM/63/84.dat", "ROM/63/86.dat", FFXI_CREATION_ALPHA_HUMANOID_HEAD, 0.0f),
    CREATION_FACE_PAIR("2", "ROM/63/81.dat", "ROM/63/80.dat", "ROM/63/89.dat", "ROM/63/88.dat", "ROM/63/90.dat", FFXI_CREATION_ALPHA_HUMANOID_HEAD, 0.0f),
    CREATION_FACE_PAIR("3", "ROM/63/81.dat", "ROM/63/80.dat", "ROM/63/93.dat", "ROM/63/92.dat", "ROM/63/94.dat", FFXI_CREATION_ALPHA_HUMANOID_HEAD, 0.0f),
    CREATION_FACE_PAIR("4", "ROM/63/81.dat", "ROM/63/80.dat", "ROM/63/97.dat", "ROM/63/96.dat", "ROM/63/98.dat", FFXI_CREATION_ALPHA_HUMANOID_HEAD, 0.0f),
};

static const FFXICreationEntry kCreationHumeFEntries[] =
{
    CREATION_HUME_F_FACE_PAIR("1", "ROM/63/65.dat", "ROM/63/64.dat", "ROM/63/66.dat", -0.50f),
    CREATION_HUME_F_FACE_PAIR("2", "ROM/63/69.dat", "ROM/63/68.dat", "ROM/63/70.dat", -0.40f),
    CREATION_HUME_F_FACE_PAIR("3", "ROM/63/73.dat", "ROM/63/72.dat", "ROM/63/74.dat", -0.40f),
    CREATION_HUME_F_FACE_PAIR("4", "ROM/63/77.dat", "ROM/63/76.dat", "ROM/63/78.dat", -0.40f),
};

static const FFXICreationEntry kCreationElvaanMEntries[] =
{
    CREATION_FACE_PAIR("1", "ROM/63/21.dat", "ROM/63/20.dat", "ROM/63/25.dat", "ROM/63/24.dat", "ROM/63/26.dat", FFXI_CREATION_ALPHA_HUMANOID_HEAD, 0.0f),
    CREATION_FACE_PAIR("2", "ROM/63/21.dat", "ROM/63/20.dat", "ROM/63/29.dat", "ROM/63/28.dat", "ROM/63/30.dat", FFXI_CREATION_ALPHA_HUMANOID_HEAD, 0.0f),
    CREATION_FACE_PAIR("3", "ROM/63/21.dat", "ROM/63/20.dat", "ROM/63/33.dat", "ROM/63/32.dat", "ROM/63/34.dat", FFXI_CREATION_ALPHA_HUMANOID_HEAD, 0.0f),
    CREATION_FACE_PAIR("4", "ROM/63/21.dat", "ROM/63/20.dat", "ROM/63/37.dat", "ROM/63/36.dat", "ROM/63/38.dat", FFXI_CREATION_ALPHA_HUMANOID_HEAD, 0.0f),
};

static const FFXICreationEntry kCreationElvaanFEntries[] =
{
    CREATION_ELVAAN_F_FACE_PAIR("1", "ROM/63/5.dat",  "ROM/63/4.dat",  "ROM/63/6.dat",  -0.50f),
    CREATION_ELVAAN_F_FACE_PAIR("2", "ROM/63/9.dat",  "ROM/63/8.dat",  "ROM/63/10.dat", -0.40f),
    CREATION_ELVAAN_F_FACE_PAIR("3", "ROM/63/13.dat", "ROM/63/12.dat", "ROM/63/14.dat", -0.50f),
    CREATION_ELVAAN_F_FACE_PAIR("4", "ROM/63/17.dat", "ROM/63/16.dat", "ROM/63/18.dat", -0.40f),
};

static const FFXICreationEntry kCreationTaruMEntries[] =
{
    CREATION_FACE_PAIR("1", "ROM/64/13.dat", "ROM/64/12.dat", "ROM/64/17.dat", "ROM/64/16.dat", "ROM/64/18.dat", FFXI_CREATION_ALPHA_TARU_HEAD, 0.0f),
    CREATION_FACE_PAIR("2", "ROM/64/13.dat", "ROM/64/12.dat", "ROM/64/21.dat", "ROM/64/20.dat", "ROM/64/22.dat", FFXI_CREATION_ALPHA_TARU_HEAD, 0.0f),
    CREATION_FACE_PAIR("3", "ROM/64/13.dat", "ROM/64/12.dat", "ROM/64/25.dat", "ROM/64/24.dat", "ROM/64/26.dat", FFXI_CREATION_ALPHA_TARU_HEAD, 0.0f),
    CREATION_FACE_PAIR("4", "ROM/64/13.dat", "ROM/64/12.dat", "ROM/64/29.dat", "ROM/64/28.dat", "ROM/64/30.dat", FFXI_CREATION_ALPHA_TARU_HEAD, 0.0f),
};

static const FFXICreationEntry kCreationTaruFEntries[] =
{
    CREATION_FACE_PAIR("1", "ROM/63/121.dat", "ROM/63/120.dat", "ROM/63/125.dat", "ROM/63/124.dat", "ROM/63/126.dat", FFXI_CREATION_ALPHA_TARU_HEAD, 0.0f),
    CREATION_FACE_PAIR("2", "ROM/63/121.dat", "ROM/63/120.dat", "ROM/64/1.dat",   "ROM/64/0.dat",   "ROM/64/2.dat",   FFXI_CREATION_ALPHA_TARU_HEAD, 0.0f),
    CREATION_FACE_PAIR("3", "ROM/63/121.dat", "ROM/63/120.dat", "ROM/64/5.dat",   "ROM/64/4.dat",   "ROM/64/6.dat",   FFXI_CREATION_ALPHA_TARU_HEAD, 0.0f),
    CREATION_FACE_PAIR("4", "ROM/63/121.dat", "ROM/63/120.dat", "ROM/64/9.dat",   "ROM/64/8.dat",   "ROM/64/10.dat",  FFXI_CREATION_ALPHA_TARU_HEAD, 0.0f),
};

static const FFXICreationEntry kCreationMithraEntries[] =
{
    CREATION_FACE_PAIR("1", "ROM/63/101.dat", "ROM/63/100.dat", "ROM/63/105.dat", "ROM/63/104.dat", "ROM/63/106.dat", FFXI_CREATION_ALPHA_MITHRA_HEAD, 0.0f),
    CREATION_FACE_PAIR("2", "ROM/63/101.dat", "ROM/63/100.dat", "ROM/63/109.dat", "ROM/63/108.dat", "ROM/63/110.dat", FFXI_CREATION_ALPHA_MITHRA_HEAD, 0.0f),
    CREATION_FACE_PAIR("3", "ROM/63/101.dat", "ROM/63/100.dat", "ROM/63/113.dat", "ROM/63/112.dat", "ROM/63/114.dat", FFXI_CREATION_ALPHA_MITHRA_HEAD, 0.0f),
    CREATION_FACE_PAIR("4", "ROM/63/101.dat", "ROM/63/100.dat", "ROM/63/117.dat", "ROM/63/116.dat", "ROM/63/118.dat", FFXI_CREATION_ALPHA_MITHRA_HEAD, 0.0f),
};

static const FFXICreationEntry kCreationGalkaEntries[] =
{
    CREATION_FACE_PAIR("1", "ROM/63/41.dat", "ROM/63/40.dat", "ROM/63/45.dat", "ROM/63/44.dat", "ROM/63/46.dat", FFXI_CREATION_ALPHA_GALKA_HEAD, 0.0f),
    CREATION_FACE_PAIR("2", "ROM/63/41.dat", "ROM/63/40.dat", "ROM/63/49.dat", "ROM/63/48.dat", "ROM/63/50.dat", FFXI_CREATION_ALPHA_GALKA_HEAD, 0.0f),
    CREATION_FACE_PAIR("3", "ROM/63/41.dat", "ROM/63/40.dat", "ROM/63/53.dat", "ROM/63/52.dat", "ROM/63/54.dat", FFXI_CREATION_ALPHA_GALKA_HEAD, 0.0f),
    CREATION_FACE_PAIR("4", "ROM/63/41.dat", "ROM/63/40.dat", "ROM/63/57.dat", "ROM/63/56.dat", "ROM/63/58.dat", FFXI_CREATION_ALPHA_GALKA_HEAD, 0.0f),
};

static const FFXICreationRace kFFXICreationRaces[] =
{
    { "Hume Male",       kCreationHumeMEntries,   FFXI_CREATION_ENTRY_COUNT(kCreationHumeMEntries)   },
    { "Hume Female",     kCreationHumeFEntries,   FFXI_CREATION_ENTRY_COUNT(kCreationHumeFEntries)   },
    { "Elvaan Male",     kCreationElvaanMEntries, FFXI_CREATION_ENTRY_COUNT(kCreationElvaanMEntries) },
    { "Elvaan Female",   kCreationElvaanFEntries, FFXI_CREATION_ENTRY_COUNT(kCreationElvaanFEntries) },
    { "Tarutaru Male",   kCreationTaruMEntries,   FFXI_CREATION_ENTRY_COUNT(kCreationTaruMEntries)   },
    { "Tarutaru Female", kCreationTaruFEntries,   FFXI_CREATION_ENTRY_COUNT(kCreationTaruFEntries)   },
    { "Mithra",          kCreationMithraEntries,  FFXI_CREATION_ENTRY_COUNT(kCreationMithraEntries)  },
    { "Galka",           kCreationGalkaEntries,   FFXI_CREATION_ENTRY_COUNT(kCreationGalkaEntries)   },
};

static const int kFFXICreationRaceCount = (int)(sizeof(kFFXICreationRaces) / sizeof(kFFXICreationRaces[0]));

static inline int FFXICreation_TotalEntries()
{
    int total = 0;
    for (int r = 0; r < kFFXICreationRaceCount; ++r)
        total += kFFXICreationRaces[r].count;
    return total;
}

static inline const FFXICreationEntry *FFXICreation_GetEntry(int flatIndex)
{
    if (flatIndex < 0)
        return nullptr;

    for (int r = 0; r < kFFXICreationRaceCount; ++r)
    {
        const FFXICreationRace &race = kFFXICreationRaces[r];
        if (flatIndex < race.count)
            return &race.entries[flatIndex];
        flatIndex -= race.count;
    }

    return nullptr;
}

#undef CREATION_HUME_F_FACE_PAIR
#undef CREATION_ELVAAN_F_FACE_PAIR
#undef CREATION_FACE_PAIR
#undef CREATION_FACE_SET
