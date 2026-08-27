#include "stdafx.h"
#include "ffxi_creation_animation_paths.h"

#include <cstdio>
#include <cstring>

namespace
{
struct SqlePair
{
    const char* bodyBase;
    const char* headBase;
    const char* bodyIdle;
    const char* headIdle;
    const char* bodyWalk;
    const char* headWalk;
};

constexpr SqlePair kRacePairs[] =
{
    { "ROM/66/16.dat", "ROM/66/22.dat", "ROM/66/14.dat", "ROM/66/20.dat", "ROM/66/12.dat", "ROM/66/18.dat" },
    { "ROM/65/86.dat", "ROM/65/92.dat", "ROM/65/84.dat", "ROM/65/90.dat", "ROM/65/82.dat", "ROM/65/88.dat" },
    { "ROM/64/98.dat", "ROM/64/104.dat", "ROM/64/96.dat", "ROM/64/102.dat", "ROM/64/94.dat", "ROM/64/100.dat" },
    { "ROM/64/40.dat", "ROM/64/46.dat", "ROM/64/38.dat", "ROM/64/44.dat", "ROM/64/36.dat", "ROM/64/42.dat" },
    // Tarutaru male's retail creation sequence is 67/58 (1624 frames), which
    // matches its face-1 head at 67/64. 67/4 is the 1315-frame Tarutaru female
    // sequence even though the short Tarutaru clips share the 67/0..3 cluster.
    { "ROM/67/58.dat", "ROM/67/64.dat", "ROM/67/2.dat",  "ROM/67/62.dat", "ROM/67/0.dat",  "ROM/67/60.dat" },
    { "ROM/67/4.dat",  "ROM/67/10.dat", "ROM/67/2.dat",  "ROM/67/8.dat",  "ROM/67/0.dat",  "ROM/67/6.dat"  },
    { "ROM/66/74.dat", "ROM/66/80.dat", "ROM/66/72.dat", "ROM/66/78.dat", "ROM/66/70.dat", "ROM/66/76.dat" },
    { "ROM/65/28.dat", "ROM/65/34.dat", "ROM/65/26.dat", "ROM/65/32.dat", "ROM/65/24.dat", "ROM/65/30.dat" },
};

constexpr SqlePair kFallbackPair =
    { "ROM/64/40.dat", "ROM/64/46.dat", "ROM/64/38.dat", "ROM/64/44.dat", "ROM/64/36.dat", "ROM/64/42.dat" };

struct CameraPair
{
    const char* fov[2];
    const char* matrix[2];
};

constexpr CameraPair kRaceCameras[] =
{
    { { "ROM/66/8.dat",   "ROM/66/10.dat"  }, { "ROM/66/9.dat",   "ROM/66/11.dat"  } },
    { { "ROM/65/78.dat",  "ROM/65/80.dat"  }, { "ROM/65/79.dat",  "ROM/65/81.dat"  } },
    { { "ROM/64/90.dat",  "ROM/64/92.dat"  }, { "ROM/64/91.dat",  "ROM/64/93.dat"  } },
    { { "ROM/64/32.dat",  "ROM/64/34.dat"  }, { "ROM/64/33.dat",  "ROM/64/35.dat"  } },
    { { "ROM/67/54.dat",  "ROM/67/56.dat"  }, { "ROM/67/55.dat",  "ROM/67/57.dat"  } },
    { { "ROM/66/124.dat", "ROM/66/126.dat" }, { "ROM/66/125.dat", "ROM/66/127.dat" } },
    { { "ROM/66/66.dat",  "ROM/66/68.dat"  }, { "ROM/66/67.dat",  "ROM/66/69.dat"  } },
    { { "ROM/65/20.dat",  "ROM/65/22.dat"  }, { "ROM/65/21.dat",  "ROM/65/23.dat"  } },
};


struct HeadSet
{
    const char* meshDat;
    const char* baseDat;
    const char* idleDat;
    const char* walkDat;
};

constexpr HeadSet kHeadSets[] =
{
    { "ROM/63/5.dat", "ROM/64/46.dat", "ROM/64/44.dat", "ROM/64/42.dat" }, { "ROM/63/9.dat", "ROM/64/58.dat", "ROM/64/56.dat", "ROM/64/54.dat" },
    { "ROM/63/13.dat", "ROM/64/52.dat", "ROM/64/50.dat", "ROM/64/48.dat" }, { "ROM/63/17.dat", "ROM/64/82.dat", "ROM/64/80.dat", "ROM/64/78.dat" },
    { "ROM/63/25.dat", "ROM/64/104.dat", "ROM/64/102.dat", "ROM/64/100.dat" }, { "ROM/63/29.dat", "ROM/64/116.dat", "ROM/64/114.dat", "ROM/64/112.dat" },
    { "ROM/63/33.dat", "ROM/65/0.dat", "ROM/64/126.dat", "ROM/64/124.dat" }, { "ROM/63/37.dat", "ROM/65/12.dat", "ROM/65/10.dat", "ROM/65/8.dat" },
    { "ROM/63/45.dat", "ROM/65/34.dat", "ROM/65/32.dat", "ROM/65/30.dat" }, { "ROM/63/49.dat", "ROM/65/40.dat", "ROM/65/38.dat", "ROM/65/36.dat" },
    { "ROM/63/53.dat", "ROM/65/46.dat", "ROM/65/44.dat", "ROM/65/42.dat" }, { "ROM/63/57.dat", "ROM/65/52.dat", "ROM/65/50.dat", "ROM/65/48.dat" },
    { "ROM/63/65.dat", "ROM/65/92.dat", "ROM/65/90.dat", "ROM/65/88.dat" }, { "ROM/63/69.dat", "ROM/65/104.dat", "ROM/65/102.dat", "ROM/65/100.dat" },
    { "ROM/63/73.dat", "ROM/65/116.dat", "ROM/65/114.dat", "ROM/65/112.dat" }, { "ROM/63/77.dat", "ROM/66/0.dat", "ROM/65/126.dat", "ROM/65/124.dat" },
    { "ROM/63/85.dat", "ROM/66/22.dat", "ROM/66/20.dat", "ROM/66/18.dat" }, { "ROM/63/89.dat", "ROM/66/34.dat", "ROM/66/32.dat", "ROM/66/30.dat" },
    { "ROM/63/93.dat", "ROM/66/46.dat", "ROM/66/44.dat", "ROM/66/42.dat" }, { "ROM/63/97.dat", "ROM/66/58.dat", "ROM/66/56.dat", "ROM/66/54.dat" },
    { "ROM/63/105.dat", "ROM/66/80.dat", "ROM/66/78.dat", "ROM/66/76.dat" }, { "ROM/63/109.dat", "ROM/66/92.dat", "ROM/66/90.dat", "ROM/66/88.dat" },
    { "ROM/63/113.dat", "ROM/66/104.dat", "ROM/66/102.dat", "ROM/66/100.dat" }, { "ROM/63/117.dat", "ROM/66/116.dat", "ROM/66/114.dat", "ROM/66/112.dat" },
    { "ROM/63/125.dat", "ROM/67/10.dat", "ROM/67/8.dat", "ROM/67/6.dat" }, { "ROM/64/1.dat", "ROM/67/22.dat", "ROM/67/20.dat", "ROM/67/18.dat" },
    { "ROM/64/5.dat", "ROM/67/34.dat", "ROM/67/32.dat", "ROM/67/30.dat" }, { "ROM/64/9.dat", "ROM/67/46.dat", "ROM/67/44.dat", "ROM/67/42.dat" },
    { "ROM/64/17.dat", "ROM/67/64.dat", "ROM/67/62.dat", "ROM/67/60.dat" }, { "ROM/64/21.dat", "ROM/67/76.dat", "ROM/67/74.dat", "ROM/67/72.dat" },
    { "ROM/64/25.dat", "ROM/67/88.dat", "ROM/67/86.dat", "ROM/67/84.dat" }, { "ROM/64/29.dat", "ROM/67/100.dat", "ROM/67/98.dat", "ROM/67/96.dat" },
};

const SqlePair& RacePair(int raceIndex)
{
    return raceIndex >= 0 && raceIndex < static_cast<int>(_countof(kRacePairs)) ? kRacePairs[raceIndex] : kFallbackPair;
}

const HeadSet* FindHeadSet(const char* headMeshDat)
{
    if (!headMeshDat)
        return nullptr;
    for (const HeadSet& set : kHeadSets)
        if (std::strcmp(set.meshDat, headMeshDat) == 0)
            return &set;
    return nullptr;
}
}

namespace FFXICreationAnimationPaths
{
void MakeRelativeOption(const char* animationDat, char* outOption, std::size_t outOptionSize)
{
    if (!outOption || outOptionSize == 0)
        return;

    outOption[0] = '\0';
    int rom = 0;
    int dat = 0;
    if (!animationDat || sscanf_s(animationDat, "ROM/%i/%i.dat", &rom, &dat) != 2)
        return;
    sprintf_s(outOption, outOptionSize, "../%i/%i.dat", rom, dat);
}

const char* BodyBase(int raceIndex) { return RacePair(raceIndex).bodyBase; }
const char* HeadBase(int raceIndex, const char* headMeshDat)
{
    const HeadSet* set = FindHeadSet(headMeshDat);
    return set ? set->baseDat : RacePair(raceIndex).headBase;
}
const char* BodyIdle(int raceIndex) { return RacePair(raceIndex).bodyIdle; }
const char* HeadIdle(int raceIndex, const char* headMeshDat)
{
    const HeadSet* set = FindHeadSet(headMeshDat);
    return set ? set->idleDat : RacePair(raceIndex).headIdle;
}

bool SequenceUsesInitialEquipment(int raceIndex)
{
    // These races add/change cloth controls between the two body variants.
    // Their PB sequence channel counts match the +2 initial-equipment body;
    // their short FrameChannel clips match the no-equipment body.
    return raceIndex >= 4 && raceIndex <= 7;
}

const char* CameraFov(const int raceIndex, const int cameraIndex)
{
    if (raceIndex < 0 || raceIndex >= static_cast<int>(_countof(kRaceCameras)) ||
        cameraIndex < 0 || cameraIndex > 1)
        return nullptr;
    return kRaceCameras[raceIndex].fov[cameraIndex];
}

const char* CameraMatrix(const int raceIndex, const int cameraIndex)
{
    if (raceIndex < 0 || raceIndex >= static_cast<int>(_countof(kRaceCameras)) ||
        cameraIndex < 0 || cameraIndex > 1)
        return nullptr;
    return kRaceCameras[raceIndex].matrix[cameraIndex];
}
}
