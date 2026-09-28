#include "stdafx.h"
#include "zone_environment_state.h"

#include "noesis_rapi.h"
#include "model_ff11.h"
#include "zone_environment_identity.h"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <ctime>

namespace ZoneEnvironmentState
{
namespace
{
int g_timeOverride = -1;
bool g_useLocalSystemTime = false;
DWORD LerpColor(const unsigned int a, const unsigned int b, const float t)
{
    // FFXI's 0x2F environment colors are packed R, G, B from the low byte up.
    const int ar = a & 0xff, ag = (a >> 8) & 0xff, ab = (a >> 16) & 0xff;
    const int br = b & 0xff, bg = (b >> 8) & 0xff, bb = (b >> 16) & 0xff;
    const int r = (int)(ar + (br - ar) * t + 0.5f);
    const int g = (int)(ag + (bg - ag) * t + 0.5f);
    const int bl = (int)(ab + (bb - ab) * t + 0.5f);
    return D3DCOLOR_XRGB(r, g, bl);
}

void DecodeLightDirection(const unsigned int packed, float out[3])
{
    out[0] = static_cast<float>(static_cast<int8_t>(packed & 0xff));
    out[1] = static_cast<float>(static_cast<int8_t>((packed >> 8) & 0xff));
    out[2] = static_cast<float>(static_cast<int8_t>((packed >> 16) & 0xff));
    const float length = sqrtf(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]);
    if (length > 0.0001f)
    {
        out[0] /= length;
        out[1] /= length;
        out[2] /= length;
    }
}

void InterpolateLightDirection(const unsigned int a, const unsigned int b,
                               const float t, float out[3])
{
    float from[3] = {}, to[3] = {};
    DecodeLightDirection(a, from);
    DecodeLightDirection(b, to);
    float dot = from[0] * to[0] + from[1] * to[1] + from[2] * to[2];
    if (dot < 0.0f)
    {
        to[0] = -to[0];
        to[1] = -to[1];
        to[2] = -to[2];
        dot = -dot;
    }
    dot = std::clamp(dot, -1.0f, 1.0f);
    float fromWeight = 1.0f - t;
    float toWeight = t;
    const float angle = acosf(dot);
    const float sinAngle = sinf(angle);
    if (sinAngle > 0.0001f)
    {
        fromWeight = sinf((1.0f - t) * angle) / sinAngle;
        toWeight = sinf(t * angle) / sinAngle;
    }
    out[0] = from[0] * fromWeight + to[0] * toWeight;
    out[1] = from[1] * fromWeight + to[1] * toWeight;
    out[2] = from[2] * fromWeight + to[2] * toWeight;
    const float length = sqrtf(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]);
    if (length > 0.0001f)
    {
        out[0] /= length;
        out[1] /= length;
        out[2] /= length;
    }
}

void InterpolateDirectionVectors(const float from[3], const float to[3],
                                 const float t, float out[3])
{
    float adjustedTo[3] = { to[0], to[1], to[2] };
    float dot = from[0] * adjustedTo[0] + from[1] * adjustedTo[1] + from[2] * adjustedTo[2];
    if (dot < 0.0f)
    {
        adjustedTo[0] = -adjustedTo[0];
        adjustedTo[1] = -adjustedTo[1];
        adjustedTo[2] = -adjustedTo[2];
        dot = -dot;
    }
    dot = std::clamp(dot, -1.0f, 1.0f);
    float fromWeight = 1.0f - t;
    float toWeight = t;
    const float angle = acosf(dot);
    const float sinAngle = sinf(angle);
    if (sinAngle > 0.0001f)
    {
        fromWeight = sinf((1.0f - t) * angle) / sinAngle;
        toWeight = sinf(t * angle) / sinAngle;
    }
    for (int axis = 0; axis < 3; ++axis)
        out[axis] = from[axis] * fromWeight + adjustedTo[axis] * toWeight;
}

Data BlendWeatherData(const Data& from, const Data& to, const float t)
{
    Data result = t < 0.5f ? from : to;
    result.valid = from.valid || to.valid;
    result.modelMainLightColor = LerpColor(from.modelMainLightColor, to.modelMainLightColor, t);
    result.modelSecondaryLightColor = LerpColor(
        from.modelSecondaryLightColor, to.modelSecondaryLightColor, t);
    result.modelAmbientColor = LerpColor(from.modelAmbientColor, to.modelAmbientColor, t);
    result.modelLightPower = from.modelLightPower + (to.modelLightPower - from.modelLightPower) * t;
    result.terrainMainLightColor = LerpColor(from.terrainMainLightColor, to.terrainMainLightColor, t);
    result.terrainSecondaryLightColor = LerpColor(
        from.terrainSecondaryLightColor, to.terrainSecondaryLightColor, t);
    result.terrainAmbientColor = LerpColor(from.terrainAmbientColor, to.terrainAmbientColor, t);
    result.terrainLightPower = from.terrainLightPower +
        (to.terrainLightPower - from.terrainLightPower) * t;
    if (from.authoredLightDirection && to.authoredLightDirection)
    {
        InterpolateDirectionVectors(from.modelMainLightDirection,
            to.modelMainLightDirection, t, result.modelMainLightDirection);
        InterpolateDirectionVectors(from.terrainMainLightDirection,
            to.terrainMainLightDirection, t, result.terrainMainLightDirection);
    }
    result.clearColor = LerpColor(from.clearColor, to.clearColor, t);
    result.fogColor = LerpColor(from.fogColor, to.fogColor, t);
    if (from.fogFar > 0.0f && to.fogFar > 0.0f)
    {
        result.fogNear = from.fogNear + (to.fogNear - from.fogNear) * t;
        result.fogFar = from.fogFar + (to.fogFar - from.fogFar) * t;
    }
    result.drawDistance = from.drawDistance + (to.drawDistance - from.drawDistance) * t;
    result.skyRadius = from.skyRadius + (to.skyRadius - from.skyRadius) * t;
    if (from.ringCount == to.ringCount)
    {
        result.ringCount = to.ringCount;
        for (int ring = 0; ring < result.ringCount; ++ring)
        {
            result.ringColors[ring] = LerpColor(from.ringColors[ring], to.ringColors[ring], t);
            result.ringElevations[ring] = from.ringElevations[ring] +
                (to.ringElevations[ring] - from.ringElevations[ring]) * t;
        }
    }
    return result;
}
}

void Invalidate(Cache &cache, const bool clearWeatherGroups)
{
    if (clearWeatherGroups)
        cache.weatherGroups.clear();
    cache.dirty = true;
    cache.cachedMinute = -1;
    cache.cachedWeatherIndex = -1;
    cache.transitioningWeather = false;
}

int CurrentMinuteOfDay()
{
    if (g_timeOverride >= 0)
        return g_timeOverride;
    if (g_useLocalSystemTime)
    {
        const std::time_t now = std::time(nullptr);
        std::tm local = {};
        if (localtime_s(&local, &now) == 0)
            return local.tm_hour * 60 + local.tm_min;
    }
    // One Vana'diel day is 3456 real seconds (25x real time), with this epoch.
    constexpr long long kVanadielEpoch = 1009810800LL;
    const long long unixSeconds = (long long)std::time(nullptr);
    const long long vanadielMinutes = ((unixSeconds - kVanadielEpoch) * 25LL) / 60LL;
    int minute = (int)(vanadielMinutes % 1440LL);
    if (minute < 0)
        minute += 1440;
    return minute;
}

void SetUseLocalSystemTime(const bool enabled)
{
    g_useLocalSystemTime = enabled;
}

void SetTimeOverride(const int minuteOfDay)
{
    g_timeOverride = ((minuteOfDay % 1440) + 1440) % 1440;
}

void ClearTimeOverride()
{
    g_timeOverride = -1;
}

void Update(Data &state, Cache &cache, int &weatherIndex,
            const std::vector<ff11EnvironmentRecord_t> &records, const int currentMinute)
{
    if (records.empty())
    {
        state = {};
        cache.weatherGroups.clear();
        cache.cachedMinute = -1;
        cache.cachedWeatherIndex = -1;
        return;
    }

    if (cache.dirty)
    {
        cache.weatherGroups.clear();
        for (const ff11EnvironmentRecord_t &record : records)
        {
            if (!ZoneEnvironmentIdentity::ContainsLowerToken(record.directoryPath, "/weat/"))
                continue;
            bool knownGroup = false;
            for (const ff11EnvironmentRecord_t *known : cache.weatherGroups)
                if (strcmp(known->directoryPath, record.directoryPath) == 0)
                    knownGroup = true;
            if (!knownGroup)
                cache.weatherGroups.push_back(&record);
        }
        std::stable_sort(cache.weatherGroups.begin(), cache.weatherGroups.end(),
            [](const ff11EnvironmentRecord_t *a, const ff11EnvironmentRecord_t *b)
            {
                const bool aFine = ZoneEnvironmentIdentity::ContainsLowerToken(a->directoryPath, "/fine") ||
                                   ZoneEnvironmentIdentity::ContainsLowerToken(a->directoryPath, "/suny");
                const bool bFine = ZoneEnvironmentIdentity::ContainsLowerToken(b->directoryPath, "/fine") ||
                                   ZoneEnvironmentIdentity::ContainsLowerToken(b->directoryPath, "/suny");
                return aFine && !bFine;
            });
        cache.dirty = false;
        cache.cachedMinute = -1;
        cache.cachedWeatherIndex = -1;
    }

    if (!cache.weatherGroups.empty())
    {
        if (weatherIndex < 0)
            weatherIndex = 0;
        weatherIndex %= (int)cache.weatherGroups.size();
    }
    const bool weatherChanged = state.valid && cache.cachedWeatherIndex >= 0 &&
        cache.cachedWeatherIndex != weatherIndex;
    if (weatherChanged)
    {
        cache.weatherTransitionFrom = state;
        cache.weatherTransitionStartMs = GetTickCount64();
        cache.transitioningWeather = true;
    }
    if (state.valid && cache.cachedMinute == currentMinute &&
        cache.cachedWeatherIndex == weatherIndex && !cache.transitioningWeather)
    {
        return;
    }

    state = {};
    cache.cachedMinute = currentMinute;
    cache.cachedWeatherIndex = weatherIndex;

    // A directory contains one weather state's per-hour records. Keep the
    // first authored state as the default instead of blending across weather
    // directories that happen to use identical HHMM tags.
    const ff11EnvironmentRecord_t *first = &records.front();
    if (!cache.weatherGroups.empty())
        first = cache.weatherGroups[(size_t)weatherIndex];
    const char *group = first->directoryPath;
    const ff11EnvironmentRecord_t *before = nullptr;
    const ff11EnvironmentRecord_t *after = nullptr;
    int beforeDelta = 1441;
    int afterDelta = 1441;
    for (const ff11EnvironmentRecord_t &record : records)
    {
        if (strcmp(record.directoryPath, group) != 0 || record.minuteOfDay < 0)
            continue;
        const int back = (currentMinute - record.minuteOfDay + 1440) % 1440;
        const int forward = (record.minuteOfDay - currentMinute + 1440) % 1440;
        if (back < beforeDelta) { beforeDelta = back; before = &record; }
        if (forward < afterDelta) { afterDelta = forward; after = &record; }
    }
    if (!before) before = first;
    if (!after) after = before;
    const float blend = (before == after || beforeDelta + afterDelta == 0) ? 0.0f :
        (float)beforeDelta / (float)(beforeDelta + afterDelta);

    state.valid = true;
    strcpy_s(state.weatherPath, first->directoryPath);
    state.modelMainLightColor = LerpColor(
        before->modelLight.sunColor, after->modelLight.sunColor, blend);
    state.modelSecondaryLightColor = LerpColor(
        before->modelLight.moonColor, after->modelLight.moonColor, blend);
    state.modelAmbientColor = LerpColor(
        before->modelLight.ambientColor, after->modelLight.ambientColor, blend);
    state.modelLightPower = before->modelLight.diffuseMultiplier +
        (after->modelLight.diffuseMultiplier - before->modelLight.diffuseMultiplier) * blend;
    if (!std::isfinite(state.modelLightPower) || state.modelLightPower < 0.0f)
        state.modelLightPower = 1.0f;
    state.terrainMainLightColor = LerpColor(
        before->terrainLight.sunColor, after->terrainLight.sunColor, blend);
    state.terrainSecondaryLightColor = LerpColor(
        before->terrainLight.moonColor, after->terrainLight.moonColor, blend);
    state.terrainAmbientColor = LerpColor(
        before->terrainLight.ambientColor, after->terrainLight.ambientColor, blend);
    state.terrainLightPower = before->terrainLight.diffuseMultiplier +
        (after->terrainLight.diffuseMultiplier - before->terrainLight.diffuseMultiplier) * blend;
    if (!std::isfinite(state.terrainLightPower) || state.terrainLightPower < 0.0f)
        state.terrainLightPower = 1.0f;
    const ff11EnvironmentRecord_t *fogRecord = (blend < 0.5f) ? before : after;
    state.authoredLightDirection = (fogRecord->indoorFlag & 1u) != 0;
    if (state.authoredLightDirection)
    {
        InterpolateLightDirection(before->modelLight.moonColor,
            after->modelLight.moonColor, blend, state.modelMainLightDirection);
        InterpolateLightDirection(before->terrainLight.moonColor,
            after->terrainLight.moonColor, blend, state.terrainMainLightDirection);
    }
    state.clearColor = LerpColor(before->clearColor, after->clearColor, blend);
    state.fogColor = LerpColor(before->terrainLight.fogColor, after->terrainLight.fogColor, blend);
    // fogFar == 0 is a discrete authored off sentinel; never interpolate into it.
    state.indoor = fogRecord->indoorFlag != 0;
    if (before->terrainLight.fogFar == 0.0f || after->terrainLight.fogFar == 0.0f)
    {
        state.fogNear = fogRecord->terrainLight.fogNear;
        state.fogFar = fogRecord->terrainLight.fogFar;
    }
    else
    {
        state.fogNear = before->terrainLight.fogNear +
            (after->terrainLight.fogNear - before->terrainLight.fogNear) * blend;
        state.fogFar = before->terrainLight.fogFar +
            (after->terrainLight.fogFar - before->terrainLight.fogFar) * blend;
    }
    state.drawDistance = before->drawDistance + (after->drawDistance - before->drawDistance) * blend;
    if (!(state.drawDistance > 1.0f) || !std::isfinite(state.drawDistance))
        state.drawDistance = 2000.0f;
    state.skyRadius = before->skyBoxRadius + (after->skyBoxRadius - before->skyBoxRadius) * blend;
    if (!(state.skyRadius > 1.0f) || !std::isfinite(state.skyRadius))
        state.skyRadius = 2029.5f;
    state.sphereSpokeCount = fogRecord->sphereSpokeCount;
    state.ringCount = std::min(before->skyDomeRingCount, after->skyDomeRingCount);
    for (int ring = 0; ring < state.ringCount; ++ring)
    {
        state.ringColors[ring] = LerpColor(before->skyDomeRingColors[ring],
                                            after->skyDomeRingColors[ring], blend);
        state.ringElevations[ring] = before->skyDomeElevations[ring] +
            (after->skyDomeElevations[ring] - before->skyDomeElevations[ring]) * blend;
    }
    if (cache.transitioningWeather)
    {
        constexpr float kWeatherTransitionMilliseconds = 3000.0f;
        const float transition = std::clamp(
            static_cast<float>(GetTickCount64() - cache.weatherTransitionStartMs) /
                kWeatherTransitionMilliseconds,
            0.0f, 1.0f);
        const Data target = state;
        state = BlendWeatherData(cache.weatherTransitionFrom, target, transition);
        strcpy_s(state.previousWeatherPath, cache.weatherTransitionFrom.weatherPath);
        strcpy_s(state.weatherPath, target.weatherPath);
        state.weatherTransition = transition;
        if (transition >= 1.0f)
        {
            cache.transitioningWeather = false;
            state.previousWeatherPath[0] = '\0';
        }
    }
}
}
