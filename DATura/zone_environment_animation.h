#pragma once

#include <string>
#include <vector>

struct ff11GeneratorRecord_t;
struct ff11KeyframeRecord_t;

namespace ZoneEnvironmentAnimation
{
const ff11GeneratorRecord_t *FindMeshGenerator(
    const std::string &objectName, const std::vector<ff11GeneratorRecord_t> &generators);
const ff11KeyframeRecord_t *FindKeyframe(const ff11GeneratorRecord_t &generator,
                                         const char *keyframeName,
                                         const std::vector<ff11KeyframeRecord_t> &keyframes);
float EvaluateKeyframe(const ff11KeyframeRecord_t *keyframe, float dayFraction, float fallback);
}
