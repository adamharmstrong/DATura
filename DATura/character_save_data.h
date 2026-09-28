#pragma once

#include <windows.h>

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <string>
#include <vector>

namespace CharacterSaveData
{
inline constexpr int Version = 2;

struct HomePoint
{
    int zoneId = -1;
    unsigned int entityId = 0;
    std::string name;
};

struct Data
{
    int homeNationIndex = -1;
    bool hasHomePoint = false;
    int homePointZoneId = -1;
    unsigned int homePointEntityId = 0;
    std::vector<HomePoint> activatedHomePoints;
};

inline bool SameHomePoint(const HomePoint& left, const HomePoint& right)
{
    return left.zoneId == right.zoneId && left.entityId == right.entityId;
}

inline std::string HomePointIdentity(const HomePoint& point)
{
    return std::to_string(point.zoneId) + ":" + std::to_string(point.entityId);
}

inline void RegisterHomePoint(Data& data, const HomePoint& point)
{
    data.hasHomePoint = true;
    data.homePointZoneId = point.zoneId;
    data.homePointEntityId = point.entityId;
    for (HomePoint& existing : data.activatedHomePoints)
    {
        if (SameHomePoint(existing, point))
        {
            existing.name = point.name;
            return;
        }
    }
    data.activatedHomePoints.push_back(point);
}

inline bool ReadInt(const char* path, const char* section, const char* key, int& value)
{
    char text[64] = {};
    GetPrivateProfileStringA(section, key, "", text, sizeof(text), path);
    if (!text[0])
        return false;

    char* end = nullptr;
    errno = 0;
    const long parsed = std::strtol(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' || parsed < INT_MIN || parsed > INT_MAX)
        return false;
    value = static_cast<int>(parsed);
    return true;
}

inline bool Load(const char* path, Data& data)
{
    Data loaded;
    int version = 0;
    int activated = 0;
    int entityId = 0;
    if (!path || !ReadInt(path, "Character", "Version", version) ||
        (version != 1 && version != Version) ||
        !ReadInt(path, "Character", "HomeNation", loaded.homeNationIndex) ||
        loaded.homeNationIndex < 0 || loaded.homeNationIndex > 2 ||
        !ReadInt(path, "HomePoint", "Activated", activated) ||
        (activated != 0 && activated != 1))
    {
        return false;
    }

    loaded.hasHomePoint = activated != 0;
    if (loaded.hasHomePoint)
    {
        if (!ReadInt(path, "HomePoint", "ZoneId", loaded.homePointZoneId) ||
            !ReadInt(path, "HomePoint", "EntityId", entityId) ||
            loaded.homePointZoneId < 0 || entityId <= 0)
        {
            return false;
        }
        loaded.homePointEntityId = static_cast<unsigned int>(entityId);
        loaded.activatedHomePoints.push_back(
            {loaded.homePointZoneId, loaded.homePointEntityId, "Home Point"});
    }

    if (version >= 2)
    {
        int count = 0;
        if (!ReadInt(path, "ActivatedHomePoints", "Count", count) || count < 0 || count > 512)
            return false;
        loaded.activatedHomePoints.clear();
        for (int index = 0; index < count; ++index)
        {
            char section[64] = {};
            sprintf_s(section, "ActivatedHomePoint.%d", index);
            HomePoint point;
            int pointEntityId = 0;
            char pointName[128] = {};
            if (!ReadInt(path, section, "ZoneId", point.zoneId) || point.zoneId < 0 ||
                !ReadInt(path, section, "EntityId", pointEntityId) || pointEntityId <= 0)
            {
                return false;
            }
            point.entityId = static_cast<unsigned int>(pointEntityId);
            char identity[64] = {};
            GetPrivateProfileStringA(section, "Identity", "", identity, sizeof(identity), path);
            if (!identity[0] || identity != HomePointIdentity(point))
                return false;
            GetPrivateProfileStringA(section, "Name", "Home Point", pointName,
                sizeof(pointName), path);
            point.name = pointName;
            bool duplicate = false;
            for (const HomePoint& existing : loaded.activatedHomePoints)
                duplicate = duplicate || SameHomePoint(existing, point);
            if (!duplicate)
                loaded.activatedHomePoints.push_back(point);
        }
    }

    data = loaded;
    return true;
}

inline bool WriteValue(const char* path, const char* section, const char* key, int value)
{
    char text[32] = {};
    sprintf_s(text, "%d", value);
    return WritePrivateProfileStringA(section, key, text, path) != FALSE;
}

inline bool Save(const char* path, const Data& data)
{
    if (!path || !path[0])
        return false;

    bool saved = WriteValue(path, "Character", "Version", Version) &&
        WriteValue(path, "Character", "HomeNation", data.homeNationIndex) &&
        WriteValue(path, "HomePoint", "Activated", data.hasHomePoint ? 1 : 0) &&
        WriteValue(path, "HomePoint", "ZoneId", data.homePointZoneId) &&
        WriteValue(path, "HomePoint", "EntityId", static_cast<int>(data.homePointEntityId)) &&
        WriteValue(path, "ActivatedHomePoints", "Count",
            static_cast<int>(data.activatedHomePoints.size()));
    for (std::size_t index = 0; saved && index < data.activatedHomePoints.size(); ++index)
    {
        char section[64] = {};
        sprintf_s(section, "ActivatedHomePoint.%zu", index);
        const HomePoint& point = data.activatedHomePoints[index];
        saved = WriteValue(path, section, "ZoneId", point.zoneId) &&
            WriteValue(path, section, "EntityId", static_cast<int>(point.entityId)) &&
            WritePrivateProfileStringA(section, "Identity",
                HomePointIdentity(point).c_str(), path) != FALSE &&
            WritePrivateProfileStringA(section, "Name", point.name.c_str(), path) != FALSE;
    }
    return saved;
}
}
