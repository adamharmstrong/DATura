#pragma once

#include <d3d9.h>
#include <vector>

struct ff11EnvironmentRecord_t;

namespace ZoneEnvironmentState
{
struct Data
{
    bool valid = false;
    bool indoor = false;
    DWORD clearColor = 0;
    DWORD fogColor = 0;
    float fogNear = 0.0f;
    float fogFar = 0.0f;
    float drawDistance = 0.0f;
    float skyRadius = 0.0f;
    int sphereSpokeCount = 0;
    DWORD ringColors[8] = {};
    float ringElevations[8] = {};
    int ringCount = 0;
    char weatherPath[128] = {};
};

struct Cache
{
    std::vector<const ff11EnvironmentRecord_t *> weatherGroups;
    bool dirty = true;
    int cachedMinute = -1;
    int cachedWeatherIndex = -1;
};

void Invalidate(Cache &cache, bool clearWeatherGroups);
int CurrentMinuteOfDay();
void Update(Data &state, Cache &cache, int &weatherIndex,
            const std::vector<ff11EnvironmentRecord_t> &records, int currentMinute);
}
