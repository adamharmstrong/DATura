#include "stdafx.h"
#include "zone_environment_state.h"

#include "noesis_rapi.h"
#include "model_ff11.h"
#include "zone_environment_identity.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <ctime>

namespace ZoneEnvironmentState
{
namespace
{
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
}

void Invalidate(Cache &cache, const bool clearWeatherGroups)
{
    if (clearWeatherGroups)
        cache.weatherGroups.clear();
    cache.dirty = true;
    cache.cachedMinute = -1;
    cache.cachedWeatherIndex = -1;
}

int CurrentMinuteOfDay()
{
    // One Vana'diel day is 3456 real seconds (25x real time), with this epoch.
    constexpr long long kVanadielEpoch = 1009810800LL;
    const long long unixSeconds = (long long)std::time(nullptr);
    const long long vanadielMinutes = ((unixSeconds - kVanadielEpoch) * 25LL) / 60LL;
    int minute = (int)(vanadielMinutes % 1440LL);
    if (minute < 0)
        minute += 1440;
    return minute;
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
    if (state.valid && cache.cachedMinute == currentMinute &&
        cache.cachedWeatherIndex == weatherIndex)
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
    state.clearColor = LerpColor(before->clearColor, after->clearColor, blend);
    state.fogColor = LerpColor(before->terrainLight.fogColor, after->terrainLight.fogColor, blend);
    // fogFar == 0 is a discrete authored off sentinel; never interpolate into it.
    const ff11EnvironmentRecord_t *fogRecord = (blend < 0.5f) ? before : after;
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
}
}
