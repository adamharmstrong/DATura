#include "stdafx.h"
#include "zone_environment_animation.h"

#include "noesis_rapi.h"
#include "model_ff11.h"
#include "zone_environment_identity.h"

#include <algorithm>
#include <cmath>

namespace ZoneEnvironmentAnimation
{
const ff11GeneratorRecord_t *FindMeshGenerator(
    const std::string &objectName, const std::vector<ff11GeneratorRecord_t> &generators)
{
    std::string directoryPath;
    std::string resourceName;
    std::string generatorName;
    if (!ZoneEnvironmentIdentity::ParseEnvironmentMeshIdentity(
            objectName, directoryPath, resourceName, generatorName))
    {
        return nullptr;
    }

    for (const ff11GeneratorRecord_t &generator : generators)
    {
        if (ZoneEnvironmentIdentity::EnvironmentNamesMatch(generator.name, generatorName.c_str()) &&
            ZoneEnvironmentIdentity::EnvironmentNamesMatch(generator.linkedResource, resourceName.c_str()) &&
            ZoneEnvironmentIdentity::EnvironmentWeatherRootsMatch(
                generator.directoryPath, directoryPath.c_str()))
        {
            return &generator;
        }
    }
    return nullptr;
}

const ff11KeyframeRecord_t *FindKeyframe(
    const ff11GeneratorRecord_t &generator, const char *keyframeName,
    const std::vector<ff11KeyframeRecord_t> &keyframes)
{
    if (!keyframeName || !keyframeName[0])
        return nullptr;

    const ff11KeyframeRecord_t *weatherMatch = nullptr;
    for (const ff11KeyframeRecord_t &keyframe : keyframes)
    {
        if (!ZoneEnvironmentIdentity::EnvironmentNamesMatch(keyframe.name, keyframeName))
            continue;
        if (ZoneEnvironmentIdentity::EnvironmentNamesMatch(
                keyframe.directoryPath, generator.directoryPath))
        {
            return &keyframe;
        }
        if (!weatherMatch && ZoneEnvironmentIdentity::EnvironmentWeatherRootsMatch(
                keyframe.directoryPath, generator.directoryPath))
        {
            weatherMatch = &keyframe;
        }
    }
    return weatherMatch;
}

float EvaluateKeyframe(const ff11KeyframeRecord_t *keyframe, float dayFraction, const float fallback)
{
    if (!keyframe || keyframe->pairCount <= 0)
        return fallback;
    dayFraction = std::clamp(dayFraction, 0.0f, 1.0f);
    int previous = 0;
    for (int next = 1; next < keyframe->pairCount; ++next)
    {
        const float nextTime = keyframe->times[next];
        if (!std::isfinite(nextTime) || !std::isfinite(keyframe->values[next]))
            break;
        if (nextTime >= dayFraction)
        {
            const float previousTime = keyframe->times[previous];
            if (nextTime <= previousTime)
                return keyframe->values[next];
            const float blend = (dayFraction - previousTime) / (nextTime - previousTime);
            return keyframe->values[previous] +
                   (keyframe->values[next] - keyframe->values[previous]) * blend;
        }
        previous = next;
        if (nextTime >= 1.0f)
            break;
    }
    return keyframe->values[previous];
}
}
