#pragma once

#include "application_settings.h"
#include <cmath>

namespace ZoneVegetationAnimation
{
// DATs author the displacement. The runtime supplies a four-second wind cycle;
// this timing follows Xim, rather than a confirmed retail timing record.
inline float Weight(const int mode, const double seconds)
{
    if (mode == ApplicationSettings::EnvironmentalAnimationOff || !std::isfinite(seconds))
        return 0.0f;
    const double phase = seconds / 4.0 - std::floor(seconds / 4.0);
    if (mode == ApplicationSettings::EnvironmentalAnimationSimple)
        return static_cast<float>(1.0 - std::abs(2.0 * phase - 1.0));
    return static_cast<float>(0.5 - 0.5 * std::cos(phase * 6.283185307179586));
}
}
