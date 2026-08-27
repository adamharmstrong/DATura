#include "stdafx.h"
#include "ffxi_creation_camera.h"

#include "ffxi_creation_animation_paths.h"

#include <algorithm>
#include <cmath>

namespace FFXICreationCamera
{
void Track::Clear() { fov = {}; matrix = {}; }

bool Track::Valid() const
{
    return fov.valid && matrix.valid && fov.channelCount == 1 &&
           matrix.channelCount == 16 && matrix.frameCount > 0;
}

bool Load(const char* ffxiRoot, const int raceIndex, const int cameraIndex,
          Track& outTrack)
{
    outTrack.Clear();
    const char* fovPath = FFXICreationAnimationPaths::CameraFov(raceIndex, cameraIndex);
    const char* matrixPath = FFXICreationAnimationPaths::CameraMatrix(raceIndex, cameraIndex);
    return fovPath && matrixPath &&
           FFXISqle::ReadMotionInfo(ffxiRoot, fovPath, &outTrack.fov) &&
           FFXISqle::ReadMotionInfo(ffxiRoot, matrixPath, &outTrack.matrix) &&
           outTrack.Valid();
}

bool Sample(const Track& track, const float animationTime, float outEye[3],
            float outTarget[3], float* outFovRadians)
{
    if (!track.Valid() || !outEye || !outTarget || !outFovRadians)
        return false;

    const int frameCount = track.matrix.frameCount;
    const float framePosition = std::fmod(std::max(0.0f, animationTime) * 30.0f,
                                          static_cast<float>(frameCount));
    const int frame0 = static_cast<int>(framePosition);
    const int frame1 = (frame0 + 1) % frameCount;
    const float blend = framePosition - static_cast<float>(frame0);
    float values[16] = {};
    for (int channel = 0; channel < 16; ++channel)
    {
        const float a = FFXISqle::FrameValue(track.matrix, frame0, channel);
        const float b = FFXISqle::FrameValue(track.matrix, frame1, channel);
        values[channel] = a + (b - a) * blend;
    }

    outEye[0] = values[3];
    outEye[1] = -values[7];
    outEye[2] = values[11];
    float forward[3] = { -values[2], values[6], -values[10] };
    const float length = std::sqrt(forward[0] * forward[0] +
                                   forward[1] * forward[1] +
                                   forward[2] * forward[2]);
    if (!std::isfinite(length) || length < 1e-6f)
        return false;
    for (int axis = 0; axis < 3; ++axis)
    {
        forward[axis] /= length;
        outTarget[axis] = outEye[axis] + forward[axis];
    }

    float fovDegrees = FFXISqle::FrameValue(track.fov, frame0, 0);
    if (!std::isfinite(fovDegrees) || fovDegrees <= 1.0f || fovDegrees >= 179.0f)
        fovDegrees = 37.85f;
    *outFovRadians = fovDegrees * (3.14159265358979323846f / 180.0f);
    return true;
}
}
