#pragma once

#include "ffxi_sqle_motion.h"

namespace FFXICreationCamera
{
struct Track
{
    FFXISqle::MotionInfo fov;
    FFXISqle::MotionInfo matrix;
    void Clear();
    bool Valid() const;
};

bool Load(const char* ffxiRoot, int raceIndex, int cameraIndex, Track& outTrack);
bool Sample(const Track& track, float animationTime, float outEye[3],
            float outTarget[3], float* outFovRadians);
}
