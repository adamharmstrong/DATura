#pragma once

#include <cstddef>

// Fixed SQLE animation paths for high-poly character-creation assets.
// headMeshDat may be null; in that case the race-default head animation is
// returned.
namespace FFXICreationAnimationPaths
{
    void MakeRelativeOption(const char* animationDat, char* outOption, std::size_t outOptionSize);
    const char* BodyBase(int raceIndex);
    const char* HeadBase(int raceIndex, const char* headMeshDat);
    const char* BodyIdle(int raceIndex);
    const char* HeadIdle(int raceIndex, const char* headMeshDat);
    bool SequenceUsesInitialEquipment(int raceIndex);
    const char* CameraFov(int raceIndex, int cameraIndex);
    const char* CameraMatrix(int raceIndex, int cameraIndex);
}
