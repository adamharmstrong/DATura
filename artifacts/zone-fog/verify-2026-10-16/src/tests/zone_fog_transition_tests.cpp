#include "stdafx.h"
#include "noesis_rapi.h"
#include "model_ff11.h"
#include "zone_environment_state.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

// Weather and time-of-day transitions of authored zone fog. Weather blends are
// timed by the real tick clock, so each check derives its expectation from the
// blend factor Update reports (Data::weatherTransition) instead of from sleep
// durations. Pass/fail therefore does not depend on scheduling.

namespace
{
int failures = 0;
int assertions = 0;

void Check(const bool condition, const char* message)
{
    ++assertions;
    if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}

using Records = std::vector<ff11EnvironmentRecord_t>;
using State = ZoneEnvironmentState::Data;
using Cache = ZoneEnvironmentState::Cache;

struct Fog { float nearDistance, farDistance; unsigned int color; };

ff11EnvironmentRecord_t Record(const char* directory, const int minute, const Fog fog,
                               const unsigned int light = 0x00808080)
{
    ff11EnvironmentRecord_t record = {};
    strcpy_s(record.directoryPath, directory);
    record.minuteOfDay = minute;
    record.terrainLight.fogNear = fog.nearDistance;
    record.terrainLight.fogFar = fog.farDistance;
    record.terrainLight.fogColor = fog.color;
    record.terrainLight.sunColor = light;
    record.modelLight.sunColor = light;
    record.terrainLight.diffuseMultiplier = 1.0f;
    record.modelLight.diffuseMultiplier = 1.0f;
    record.clearColor = fog.color;
    record.drawDistance = 400.0f;
    record.skyBoxRadius = 500.0f;
    record.skyDomeRingCount = 1;
    record.skyDomeRingColors[0] = fog.color;
    return record;
}

int Channel(const DWORD color, const int shift) { return static_cast<int>((color >> shift) & 0xff); }

// Packed 0x2F colors are R, G, B from the low byte up; state colors are D3D XRGB.
DWORD ExpectedColor(const unsigned int from, const unsigned int to, const float t)
{
    const auto lerp = [&](int shift)
    {
        const int a = (from >> shift) & 0xff, b = (to >> shift) & 0xff;
        return static_cast<int>(a + (b - a) * t + 0.5f);
    };
    return D3DCOLOR_XRGB(lerp(0), lerp(8), lerp(16));
}

bool ColorNear(const DWORD actual, const DWORD expected)
{
    for (int shift = 0; shift <= 16; shift += 8)
        if (std::abs(Channel(actual, shift) - Channel(expected, shift)) > 1) return false;
    return true;
}

bool FogValid(const State& state)
{
    if (!std::isfinite(state.fogNear) || !std::isfinite(state.fogFar)) return false;
    return state.fogFar == 0.0f || (state.fogFar > 0.0f && state.fogNear < state.fogFar);
}

// Selects a weather group by its directory; groups are ordered fine/sunny first.
int WeatherIndex(const Cache& cache, const char* directory)
{
    for (size_t index = 0; index < cache.weatherGroups.size(); ++index)
        if (strcmp(cache.weatherGroups[index]->directoryPath, directory) == 0)
            return static_cast<int>(index);
    return -1;
}

// Polls until the active weather blend reaches [low, high] and returns that state.
State SampleTransition(State& state, Cache& cache, int& weather, const Records& records,
                       const int minute, const float low, const float high)
{
    const ULONGLONG deadline = GetTickCount64() + 10000;
    while (GetTickCount64() < deadline)
    {
        ZoneEnvironmentState::Update(state, cache, weather, records, minute);
        if (state.weatherTransition >= low && state.weatherTransition <= high) return state;
        if (state.weatherTransition > high) break;
        Sleep(5);
    }
    return state;
}

void FinishTransition(State& state, Cache& cache, int& weather, const Records& records, const int minute)
{
    const ULONGLONG deadline = GetTickCount64() + 10000;
    do
    {
        ZoneEnvironmentState::Update(state, cache, weather, records, minute);
        if (state.weatherTransition >= 1.0f) return;
        Sleep(20);
    } while (GetTickCount64() < deadline);
}

constexpr Fog kFair = { 50.0f, 400.0f, 0x00c0a080 };
constexpr Fog kMist = { 10.0f, 100.0f, 0x00606060 };
constexpr Fog kRain = { 20.0f, 200.0f, 0x00403020 };
constexpr Fog kOff = { 0.0f, 0.0f, 0x00101010 };

Records WeatherRecords()
{
    return {
        Record("zone/weat/mist", 0, kMist), Record("zone/weat/mist", 720, kMist),
        Record("zone/weat/fine", 0, kFair), Record("zone/weat/fine", 720, kFair),
        Record("zone/weat/rain", 0, kRain), Record("zone/weat/rain", 720, kRain),
        Record("zone/weat/none", 0, kOff), Record("zone/weat/none", 720, kOff),
    };
}

void ExpectBlend(const State& state, const Fog from, const Fog to, const char* what)
{
    const float t = state.weatherTransition;
    char message[256];
    sprintf_s(message, "%s at t=%.3f: fog distances follow the reported weather blend", what, t);
    Check(std::abs(state.fogNear - (from.nearDistance + (to.nearDistance - from.nearDistance) * t)) < .01f &&
          std::abs(state.fogFar - (from.farDistance + (to.farDistance - from.farDistance) * t)) < .01f, message);
    sprintf_s(message, "%s at t=%.3f: fog color follows the same blend factor", what, t);
    Check(ColorNear(state.fogColor, ExpectedColor(from.color, to.color, t)), message);
    sprintf_s(message, "%s at t=%.3f: sky ring and clear color move with fog", what, t);
    Check(ColorNear(state.ringColors[0], ExpectedColor(from.color, to.color, t)) &&
          ColorNear(state.clearColor, ExpectedColor(from.color, to.color, t)), message);
    sprintf_s(message, "%s at t=%.3f: fog stays finite and ordered", what, t);
    Check(FogValid(state), message);
}

void ExpectSettled(const State& state, const Fog fog, const char* directory, const char* what)
{
    char message[256];
    sprintf_s(message, "%s: completed transition lands exactly on the target fog", what);
    Check(state.fogNear == fog.nearDistance && state.fogFar == fog.farDistance &&
          state.fogColor == ExpectedColor(fog.color, fog.color, 0), message);
    sprintf_s(message, "%s: completed transition clears the previous weather", what);
    Check(state.weatherTransition == 1.0f && state.previousWeatherPath[0] == '\0' &&
          strcmp(state.weatherPath, directory) == 0, message);
}

void TestFairMistTransitions()
{
    const Records records = WeatherRecords();
    State state; Cache cache; int weather = 0;
    ZoneEnvironmentState::Update(state, cache, weather, records, 600);
    Check(strcmp(state.weatherPath, "zone/weat/fine") == 0,
          "fair weather is the default group even when authored after mist");
    ExpectSettled(state, kFair, "zone/weat/fine", "initial fair weather");

    weather = WeatherIndex(cache, "zone/weat/mist");
    ZoneEnvironmentState::Update(state, cache, weather, records, 600);
    Check(state.weatherTransition < .1f, "fair-to-mist starts at the fair end without a fog pop");
    Check(strcmp(state.previousWeatherPath, "zone/weat/fine") == 0 &&
          strcmp(state.weatherPath, "zone/weat/mist") == 0, "fair-to-mist reports both weather groups");
    ExpectBlend(state, kFair, kMist, "fair-to-mist start");
    ExpectBlend(SampleTransition(state, cache, weather, records, 600, .3f, .7f), kFair, kMist,
                "fair-to-mist midpoint");
    Check(state.weatherTransition > .0f && state.weatherTransition < 1.0f,
          "fair-to-mist was sampled at an intermediate point");
    FinishTransition(state, cache, weather, records, 600);
    ExpectSettled(state, kMist, "zone/weat/mist", "fair-to-mist");

    weather = WeatherIndex(cache, "zone/weat/fine");
    ZoneEnvironmentState::Update(state, cache, weather, records, 600);
    ExpectBlend(state, kMist, kFair, "mist-to-fair start");
    ExpectBlend(SampleTransition(state, cache, weather, records, 600, .3f, .7f), kMist, kFair,
                "mist-to-fair midpoint");
    FinishTransition(state, cache, weather, records, 600);
    ExpectSettled(state, kFair, "zone/weat/fine", "mist-to-fair");
}

// fogFar == 0 is an authored off switch. A weather blend into or out of it must
// switch once rather than interpolate toward a spuriously short fog range.
void TestOffSentinelTransitions()
{
    const Records records = WeatherRecords();
    State state; Cache cache; int weather = 0;
    ZoneEnvironmentState::Update(state, cache, weather, records, 600);
    const auto run = [&](const char* target, const Fog from, const Fog to, const char* what)
    {
        weather = WeatherIndex(cache, target);
        bool sawFrom = false, sawTo = false, discrete = true, ordered = true;
        float lastTransition = -1.0f;
        const ULONGLONG deadline = GetTickCount64() + 10000;
        do
        {
            ZoneEnvironmentState::Update(state, cache, weather, records, 600);
            const float t = state.weatherTransition;
            const bool atFrom = state.fogNear == from.nearDistance && state.fogFar == from.farDistance;
            const bool atTo = state.fogNear == to.nearDistance && state.fogFar == to.farDistance;
            discrete = discrete && (atFrom || atTo);
            if (t < 1.0f) { ordered = ordered && (t < .5f ? atFrom : atTo); sawFrom |= atFrom; sawTo |= atTo; }
            lastTransition = t;
            Sleep(5);
        } while (lastTransition < 1.0f && GetTickCount64() < deadline);
        char message[256];
        sprintf_s(message, "%s: fog distances are only ever the enabled or disabled authored range", what);
        Check(discrete, message);
        sprintf_s(message, "%s: fog switches at the weather midpoint", what);
        Check(ordered && sawFrom && sawTo, message);
        sprintf_s(message, "%s: transition completes", what);
        Check(lastTransition >= 1.0f && state.fogFar == to.farDistance, message);
    };
    run("zone/weat/none", kFair, kOff, "fair-to-off weather");
    run("zone/weat/fine", kOff, kFair, "off-to-fair weather");
}

// A weather change during an active blend restarts from the visible blended
// state, so there is no flash back to either authored endpoint.
void TestRapidWeatherChanges()
{
    const Records records = WeatherRecords();
    State state; Cache cache; int weather = 0;
    ZoneEnvironmentState::Update(state, cache, weather, records, 600);
    weather = WeatherIndex(cache, "zone/weat/mist");
    const State mid = SampleTransition(state, cache, weather, records, 600, .4f, .6f);
    const float midTransition = mid.weatherTransition;
    weather = WeatherIndex(cache, "zone/weat/rain");
    ZoneEnvironmentState::Update(state, cache, weather, records, 600);
    Check(midTransition > .0f && midTransition < 1.0f, "rapid change was issued during an active blend");
    // A few milliseconds elapse between the two updates; allow 10% of the span.
    Check(std::abs(state.fogFar - mid.fogFar) < 30.0f && std::abs(state.fogNear - mid.fogNear) < 5.0f,
          "changing weather mid-blend continues from the visible fog, not an endpoint");
    Check(strcmp(state.weatherPath, "zone/weat/rain") == 0, "mid-blend change targets the new weather");
    Check(FogValid(state), "mid-blend change keeps fog finite and ordered");
    // The restarted blend is between the captured mid state and rain.
    const Fog midFog = { mid.fogNear, mid.fogFar, 0 };
    const float t = state.weatherTransition;
    Check(std::abs(state.fogFar - (midFog.farDistance + (kRain.farDistance - midFog.farDistance) * t)) < .5f,
          "restarted blend interpolates from the captured mid-blend fog");
    FinishTransition(state, cache, weather, records, 600);
    ExpectSettled(state, kRain, "zone/weat/rain", "rapid mist-to-rain change");

    // Three changes in quick succession must also settle on the last request.
    for (const char* target : { "zone/weat/fine", "zone/weat/mist", "zone/weat/fine" })
    {
        weather = WeatherIndex(cache, target);
        ZoneEnvironmentState::Update(state, cache, weather, records, 600);
        Check(FogValid(state), "back-to-back weather requests keep fog finite and ordered");
    }
    FinishTransition(state, cache, weather, records, 600);
    ExpectSettled(state, kFair, "zone/weat/fine", "back-to-back weather requests");
}

void TestTimeOfDay()
{
    // Sunrise 06:00, noon, sunset 18:00, and midnight records in one group.
    const Fog night = { 5.0f, 60.0f, 0x00201008 };
    const Fog dawn = { 20.0f, 160.0f, 0x00406080 };
    const Fog noon = { 60.0f, 400.0f, 0x00c0c0c0 };
    const Fog dusk = { 30.0f, 200.0f, 0x00205080 };
    const Records records = {
        Record("zone/weat/fine", 1260, night), Record("zone/weat/fine", 360, dawn),
        Record("zone/weat/fine", 720, noon), Record("zone/weat/fine", 1080, dusk),
    };
    State state; Cache cache; int weather = 0;
    const auto at = [&](const int minute) { ZoneEnvironmentState::Update(state, cache, weather, records, minute); };
    const auto expect = [&](const Fog from, const Fog to, const float t, const char* message)
    {
        Check(std::abs(state.fogNear - (from.nearDistance + (to.nearDistance - from.nearDistance) * t)) < .001f &&
              std::abs(state.fogFar - (from.farDistance + (to.farDistance - from.farDistance) * t)) < .001f &&
              state.fogColor == ExpectedColor(from.color, to.color, t), message);
    };
    at(360); expect(dawn, dawn, 0, "sunrise record applies exactly at 06:00");
    at(540); expect(dawn, noon, .5f, "morning interpolates between sunrise and noon");
    at(1080); expect(dusk, dusk, 0, "sunset record applies exactly at 18:00");
    at(1170); expect(dusk, night, .5f, "evening interpolates between sunset and night");
    at(0); expect(night, dawn, 180.0f / 540.0f, "midnight lies inside the night-to-sunrise interval");
    at(1439); const float before = state.fogFar;
    at(0); const float after = state.fogFar;
    Check(std::abs(after - before) < 1.0f, "fog is continuous across the midnight wrap");
    bool valid = true;
    for (int minute = 0; minute < 1440; ++minute) { at(minute); valid = valid && FogValid(state); }
    Check(valid, "every minute of the day yields finite, ordered fog");

    // Weather blends while the clock advances: the target follows the new minute.
    const Records weatherRecords = WeatherRecords();
    State live; Cache liveCache; int liveWeather = 0;
    ZoneEnvironmentState::Update(live, liveCache, liveWeather, weatherRecords, 600);
    liveWeather = WeatherIndex(liveCache, "zone/weat/mist");
    int minute = 600;
    bool liveValid = true;
    do
    {
        ZoneEnvironmentState::Update(live, liveCache, liveWeather, weatherRecords, ++minute % 1440);
        liveValid = liveValid && FogValid(live);
        Sleep(2);
    } while (live.weatherTransition < 1.0f && minute < 600 + 5000);
    Check(liveValid, "weather blend while the clock advances keeps fog finite and ordered");
    ExpectSettled(live, kMist, "zone/weat/mist", "weather blend while the clock advances");
}

void TestZoneChanges()
{
    const Records dense = WeatherRecords();
    const Records haze = { Record("other/weat/fine", 0, { 200.0f, 900.0f, 0x00e0e0f0 }) };
    State state; Cache cache; int weather = 0;
    ZoneEnvironmentState::Update(state, cache, weather, dense, 600);
    weather = WeatherIndex(cache, "zone/weat/mist");
    SampleTransition(state, cache, weather, dense, 600, .3f, .7f);

    // LoadDatFile and zone NPC loading invalidate the cache before the next update.
    weather = 0;
    ZoneEnvironmentState::Invalidate(cache, true);
    ZoneEnvironmentState::Update(state, cache, weather, haze, 600);
    Check(state.fogNear == 200.0f && state.fogFar == 900.0f && state.weatherTransition == 1.0f &&
          state.previousWeatherPath[0] == '\0',
          "zone change mid-blend shows the new zone's fog with no carried-over transition");

    ZoneEnvironmentState::Invalidate(cache, true);
    ZoneEnvironmentState::Update(state, cache, weather, Records{}, 600);
    Check(!state.valid && state.fogFar == 0.0f, "zone without environment records has no active fog");

    ZoneEnvironmentState::Invalidate(cache, true);
    ZoneEnvironmentState::Update(state, cache, weather, dense, 600);
    ExpectSettled(state, kFair, "zone/weat/fine", "returning to a dense-fog zone");

    // Reloading the same records into new storage must not read stale pointers.
    Records reloaded = dense;
    reloaded[2].terrainLight.fogFar = reloaded[3].terrainLight.fogFar = 333.0f;
    ZoneEnvironmentState::Invalidate(cache, false);
    ZoneEnvironmentState::Update(state, cache, weather, reloaded, 600);
    Check(state.fogFar == 333.0f, "zone reload rebuilds weather groups from the new records");
}

// Records becoming empty without an Invalidate (not a path DATura currently
// takes) should still not let a stale blend carry the previous zone's fog.
void TestEmptyRecordsMidBlend()
{
    const Records dense = WeatherRecords();
    const Records haze = { Record("other/weat/fine", 0, { 200.0f, 900.0f, 0x00e0e0f0 }) };
    State state; Cache cache; int weather = 0;
    ZoneEnvironmentState::Update(state, cache, weather, dense, 600);
    weather = WeatherIndex(cache, "zone/weat/mist");
    SampleTransition(state, cache, weather, dense, 600, .3f, .7f);
    ZoneEnvironmentState::Update(state, cache, weather, Records{}, 600);
    weather = 0;
    ZoneEnvironmentState::Update(state, cache, weather, haze, 600);
    Check(state.fogNear == 200.0f && state.fogFar == 900.0f,
          "un-invalidated empty-records gap does not carry the previous zone's fog");
}
}

int main()
{
    TestFairMistTransitions();
    TestOffSentinelTransitions();
    TestRapidWeatherChanges();
    TestTimeOfDay();
    TestZoneChanges();
    TestEmptyRecordsMidBlend();
    std::cout << "Zone fog transitions: " << assertions << " assertions, " << failures << " failures\n";
    return failures ? 1 : 0;
}
