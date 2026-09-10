#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace ZoneWater
{
struct Curve
{
    std::vector<std::pair<float, float>> keys;

    float Evaluate(float time, float fallback = 1.0f) const
    {
        if (keys.empty() || !std::isfinite(time)) return fallback;
        float previous = -1.0f;
        for (const auto& key : keys)
        {
            if (!std::isfinite(key.first) || !std::isfinite(key.second) ||
                key.first < previous || key.first < 0.0f || key.first > 1.0f)
                return fallback;
            previous = key.first;
        }
        if (time <= keys.front().first) return keys.front().second;
        for (size_t i = 1; i < keys.size(); ++i)
        {
            if (time > keys[i].first) continue;
            const float width = keys[i].first - keys[i - 1].first;
            if (width <= 0.0f) return keys[i].second;
            const float t = (time - keys[i - 1].first) / width;
            return keys[i - 1].second + (keys[i].second - keys[i - 1].second) * t;
        }
        return keys.back().second;
    }
};

// Owned by the loaded model. Parser globals may change when another DAT is read.
struct Surface
{
    std::string directory;
    std::string resource;
    std::string generator;
    unsigned int sourceOffset = 0;
    float colorScale[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    Curve colorCurves[4];
    float uvVelocity[2] = {};
    unsigned short blendMode = 0x44;
    bool twoSided = false; // a collapsed axis has no reliable determinant winding
    float cullDistance = 0.0f;
};

struct State
{
    float colorScale[3] = { 1.0f, 1.0f, 1.0f };
    float opacity = 1.0f;
    float uvOffset[2] = {};
};

inline State Evaluate(const Surface& surface, int minuteOfDay, double seconds)
{
    State result;
    const float day = ((minuteOfDay % 1440 + 1440) % 1440) / 1440.0f;
    for (int channel = 0; channel < 4; ++channel)
    {
        const float value = surface.colorScale[channel] *
            surface.colorCurves[channel].Evaluate(day);
        const float safe = std::isfinite(value) ? (std::max)(value, 0.0f) : 0.0f;
        if (channel == 3) result.opacity = (std::min)(safe, 1.0f);
        else result.colorScale[channel] = safe;
    }
    for (int axis = 0; axis < 2; ++axis)
    {
        if (std::isfinite(seconds) && std::isfinite(surface.uvVelocity[axis]))
            result.uvOffset[axis] = static_cast<float>(
                std::fmod(seconds * surface.uvVelocity[axis], 1.0));
    }
    return result;
}
}
