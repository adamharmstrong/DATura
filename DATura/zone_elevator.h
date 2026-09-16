#pragma once

#include "ffxi_coordinate_frame.h"

#include <array>
#include <cmath>
#include <string>

namespace ZoneElevator
{
inline constexpr int kMetalworksZone = 237;
inline constexpr float kLegPeriod = 18.25f;
inline constexpr float kTravelTime = 8.0f;
inline constexpr float kBottomY = -13.10f;
inline constexpr float kTopY = 2.00f;
inline constexpr float kPlatformX = -56.0f;
inline constexpr float kPlatformHalfWidth = 3.8f;
inline constexpr float kPlatformHalfDepth = 3.8f;
inline constexpr float kRideHeightTolerance = 1.5f;

struct Platform
{
    float z = 0.0f;
    bool startsAtTop = false;
    float lastY = kBottomY;
    std::string objectName;
    float baseY = kBottomY;
};

struct State
{
    float elapsed = 0.0f;
    Platform platforms[2] =
    {
        { 12.014f, true, kTopY, {}, kTopY },
        { -12.020f, false, kBottomY, {}, kBottomY },
    };
};

inline float PlatformY(const float elapsed, const bool startsAtTop)
{
    const int legIndex = static_cast<int>(elapsed / kLegPeriod);
    const float legTime = elapsed - static_cast<float>(legIndex) * kLegPeriod;
    const bool legStartsAtTop = (legIndex % 2 == 0) ? startsAtTop : !startsAtTop;
    const float startY = legStartsAtTop ? kTopY : kBottomY;
    const float endY = legStartsAtTop ? kBottomY : kTopY;

    if (legIndex == 0 || legTime >= kTravelTime)
        return legTime >= kTravelTime && legIndex != 0 ? endY : startY;

    const float travel = legTime / kTravelTime;
    return startY + (endY - startY) * travel;
}

inline bool ContainsPlayer(const Platform& platform, const std::array<float, 3>& nativePosition)
{
    return nativePosition[0] >= kPlatformX - kPlatformHalfWidth &&
           nativePosition[0] <= kPlatformX + kPlatformHalfWidth &&
           nativePosition[2] >= platform.z - kPlatformHalfDepth &&
           nativePosition[2] <= platform.z + kPlatformHalfDepth &&
           std::fabs(nativePosition[1] - platform.lastY) <= kRideHeightTolerance;
}

inline bool Update(State& state, const float dt, float position[3],
                   bool& onGround, const bool mirrorX)
{
    if (!position || dt <= 0.0f)
        return false;

    state.elapsed += dt;
    const float fullCycle = kLegPeriod * 2.0f;
    while (state.elapsed >= fullCycle)
        state.elapsed -= fullCycle;

    const auto nativePosition = FFXICoordinateFrame::SceneToNativeDat(
        { position[0], position[1], position[2] }, mirrorX);
    bool movedPlayer = false;

    for (Platform& platform : state.platforms)
    {
        const float y = PlatformY(state.elapsed, platform.startsAtTop);
        const float dy = y - platform.lastY;
        if (dy != 0.0f && ContainsPlayer(platform, nativePosition))
        {
            position[1] += dy;
            onGround = true;
            movedPlayer = true;
        }
        platform.lastY = y;
    }

    return movedPlayer;
}
}
