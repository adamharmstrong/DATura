#include "application_settings.h"

#include <cstddef>

namespace
{
const ApplicationSettings::ResolutionOption kResolutionOptions[] =
{
    { 1280, 720,  "1280 x 720" },
    { 1366, 768,  "1366 x 768" },
    { 1600, 900,  "1600 x 900" },
    { 1680, 1050, "1680 x 1050" },
    { 1920, 1080, "1920 x 1080" },
    { 2560, 1440, "2560 x 1440" },
};

const int kMaxSoundOptions[] = { 8, 16, 20, 32, 64, 128, -1 };
const char* kMaxSoundOptionLabels[] =
{
    "8", "16", "20", "32", "64", "128", "Unlimited"
};

// Zero is a sentinel for the bounds-derived Unlimited mode; it is never passed
// directly to the projection matrix.
const float kDrawDistanceOptions[] = { 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 0.0f };
const char* kDrawDistanceOptionLabels[] =
{
    "500", "1000", "2000", "4000", "8000", "Unlimited"
};

template <typename T, std::size_t Size>
constexpr int ArrayCount(const T (&)[Size])
{
    return static_cast<int>(Size);
}
}

namespace ApplicationSettings
{
int ResolutionOptionCount()
{
    return ArrayCount(kResolutionOptions);
}

const ResolutionOption& ResolutionOptionAt(const int index)
{
    return kResolutionOptions[ClampResolutionIndex(index)];
}

int ClampResolutionIndex(const int index)
{
    return index >= 0 && index < ResolutionOptionCount() ? index : 0;
}

int ClampWindowMode(const int mode)
{
    return mode >= Windowed && mode <= Fullscreen ? mode : Windowed;
}

int MaxSoundOptionCount()
{
    return ArrayCount(kMaxSoundOptions);
}

int MaxSoundOptionValue(const int index)
{
    const int safeIndex = index >= 0 && index < MaxSoundOptionCount()
        ? index
        : MaxSoundOptionCount() - 1;
    return kMaxSoundOptions[safeIndex];
}

const char* MaxSoundOptionLabel(const int index)
{
    const int safeIndex = index >= 0 && index < MaxSoundOptionCount()
        ? index
        : MaxSoundOptionCount() - 1;
    return kMaxSoundOptionLabels[safeIndex];
}

int MaxSoundOptionIndex(const int value)
{
    for (int i = 0; i < MaxSoundOptionCount(); ++i)
    {
        if (kMaxSoundOptions[i] == value)
            return i;
    }
    return MaxSoundOptionCount() - 1;
}

int DrawDistanceOptionCount()
{
    return ArrayCount(kDrawDistanceOptions);
}

float DrawDistanceOptionValue(const int index)
{
    return kDrawDistanceOptions[ClampDrawDistanceIndex(index)];
}

const char* DrawDistanceOptionLabel(const int index)
{
    return kDrawDistanceOptionLabels[ClampDrawDistanceIndex(index)];
}

bool DrawDistanceOptionIsUnlimited(const int index)
{
    return ClampDrawDistanceIndex(index) == DrawDistanceOptionCount() - 1;
}

int ClampDrawDistanceIndex(const int index)
{
    return index >= 0 && index < DrawDistanceOptionCount() ? index : 4;
}

int ClampEnvironmentalAnimationMode(const int mode)
{
    if (mode < EnvironmentalAnimationOff)
        return EnvironmentalAnimationOff;
    if (mode > EnvironmentalAnimationSmooth)
        return EnvironmentalAnimationSmooth;
    return mode;
}

const char* EnvironmentalAnimationModeName(const int mode)
{
    switch (ClampEnvironmentalAnimationMode(mode))
    {
    case EnvironmentalAnimationOff:
        return "Off";
    case EnvironmentalAnimationSimple:
        return "Simple";
    case EnvironmentalAnimationSmooth:
    default:
        return "Smooth";
    }
}

int ClampLightingQuality(const int quality)
{
    if (quality < LightingOff)
        return LightingOff;
    if (quality > LightingDynamicShadows)
        return LightingDynamicShadows;
    return quality;
}

const char* LightingQualityName(const int quality)
{
    switch (ClampLightingQuality(quality))
    {
    case LightingOff: return "Off";
    case LightingSimplified: return "Simplified";
    case LightingDynamicShadows:
    default: return "Dynamic Shadows";
    }
}
}
