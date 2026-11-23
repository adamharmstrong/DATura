#pragma once

#include <vector>

// Parsed scalar channels from an FFXI SQLE motion DAT. Playback and skeleton
// application remain renderer responsibilities.
namespace FFXISqle
{
    struct MotionInfo
    {
        bool valid = false;
        bool frameChannel = false;
        bool pbChannel = false;
        float timeSeconds = 0.0f;
        int frameCount = 0;
        int channelCount = 0;
        std::vector<float> frameValues;
    };

    bool ReadMotionInfo(const char* ffxiRoot, const char* relativePath, MotionInfo* outInfo);
    float FrameValue(const MotionInfo& motion, int frameIndex, int channelIndex);
    void NormalizeQuaternion(float quaternion[4]);
    void InvertQuaternion(const float quaternion[4], float outQuaternion[4]);
    void MultiplyQuaternions(const float a[4], const float b[4], float outQuaternion[4]);
    void RotateVector(const float quaternion[4], const float vector[3], float outVector[3]);
    bool ReadTransformGroup(const MotionInfo& motion, int frameIndex, int groupIndex,
                            float translation[3], float quaternion[4]);
    int FindNearestTransformGroup(const MotionInfo& motion, const float sourcePosition[3]);
}
