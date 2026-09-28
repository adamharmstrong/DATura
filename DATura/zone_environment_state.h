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
    DWORD modelMainLightColor = 0;
    DWORD modelSecondaryLightColor = 0;
    DWORD modelAmbientColor = 0;
    float modelLightPower = 1.0f;
    DWORD terrainMainLightColor = 0;
    DWORD terrainSecondaryLightColor = 0;
    DWORD terrainAmbientColor = 0;
    float terrainLightPower = 1.0f;
    bool authoredLightDirection = false;
    float modelMainLightDirection[3] = {};
    float terrainMainLightDirection[3] = {};
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
    char previousWeatherPath[128] = {};
    float weatherTransition = 1.0f;
};

struct Cache
{
    std::vector<const ff11EnvironmentRecord_t *> weatherGroups;
    bool dirty = true;
    int cachedMinute = -1;
    int cachedWeatherIndex = -1;
    bool transitioningWeather = false;
    unsigned long long weatherTransitionStartMs = 0;
    Data weatherTransitionFrom;
};

void Invalidate(Cache &cache, bool clearWeatherGroups);
int CurrentMinuteOfDay();
void SetUseLocalSystemTime(bool enabled);
void SetTimeOverride(int minuteOfDay);
void ClearTimeOverride();
void Update(Data &state, Cache &cache, int &weatherIndex,
            const std::vector<ff11EnvironmentRecord_t> &records, int currentMinute);
}
