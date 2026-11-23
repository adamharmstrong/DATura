#include "stdafx.h"
#include "zone_environment_identity.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace ZoneEnvironmentIdentity
{
bool IsEnvironmentObjectName(const std::string &name)
{
    return name.rfind("env:", 0) == 0;
}

bool ContainsLowerToken(const std::string &text, const char *token)
{
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](const unsigned char c) { return (char)tolower(c); });
    return lower.find(token) != std::string::npos;
}

bool EnvironmentMeshMatchesWeather(const std::string &objectName, const char *weatherPath)
{
    // Environment submeshes carry their DAT directory after "env: ". Match
    // that directory to the selected 0x2F group so duplicate clod_a01 shells
    // under Clouds, Mist, and Thunder remain distinct and only the active set
    // is drawn. Nested star/moon resources belong to the same active group.
    static const char kEnvironmentPrefix[] = "env: ";
    const char *activeWeatherPath = weatherPath ? weatherPath : "";
    if (objectName.rfind(kEnvironmentPrefix, 0) == 0 && activeWeatherPath[0])
    {
        const char *assetPath = objectName.c_str() + sizeof(kEnvironmentPrefix) - 1;
        const size_t weatherPathLength = strlen(activeWeatherPath);
        return _strnicmp(assetPath, activeWeatherPath, weatherPathLength) == 0 &&
               assetPath[weatherPathLength] == '/';
    }

    // Compatibility fallback for models built before directory-qualified
    // environment names were introduced.
    const char *slash = strrchr(activeWeatherPath, '/');
    const char *weather = slash ? slash + 1 : activeWeatherPath;
    if (!weather[0])
        return true;
    if (ContainsLowerToken(objectName, weather))
        return true;
    if ((!strcmp(weather, "mist") || !strcmp(weather, "clod")) &&
        ContainsLowerToken(objectName, "clod"))
    {
        return true;
    }
    return false;
}

bool ParseEnvironmentMeshIdentity(const std::string &objectName, std::string &directoryPath,
                                  std::string &resourceName, std::string &generatorName)
{
    static const char kEnvironmentPrefix[] = "env: ";
    if (objectName.rfind(kEnvironmentPrefix, 0) != 0)
        return false;
    const size_t marker = objectName.find("/@", sizeof(kEnvironmentPrefix) - 1);
    if (marker == std::string::npos)
        return false;
    const size_t resourceStart = marker + 2;
    const size_t resourceEnd = objectName.find('/', resourceStart);
    if (resourceEnd == std::string::npos)
        return false;
    const size_t generatorStart = resourceEnd + 1;
    const size_t generatorEnd = objectName.find('/', generatorStart);
    if (generatorEnd == std::string::npos)
        return false;

    directoryPath = objectName.substr(sizeof(kEnvironmentPrefix) - 1,
                                      marker - (sizeof(kEnvironmentPrefix) - 1));
    resourceName = objectName.substr(resourceStart, resourceEnd - resourceStart);
    generatorName = objectName.substr(generatorStart, generatorEnd - generatorStart);
    return !directoryPath.empty() && !resourceName.empty() && !generatorName.empty();
}

static int EnvironmentWeatherRootLength(const char *path)
{
    if (!path)
        return 0;
    for (const char *cursor = path; *cursor; ++cursor)
    {
        if (cursor[0] == '/' && _strnicmp(cursor, "/weat/", 6) == 0)
        {
            const char *rootEnd = cursor + 6;
            while (*rootEnd && *rootEnd != '/')
                ++rootEnd;
            return (int)(rootEnd - path);
        }
    }
    return 0;
}

bool EnvironmentWeatherRootsMatch(const char *a, const char *b)
{
    const int aLength = EnvironmentWeatherRootLength(a);
    const int bLength = EnvironmentWeatherRootLength(b);
    return aLength > 0 && aLength == bLength && _strnicmp(a, b, aLength) == 0;
}

bool EnvironmentNamesMatch(const char *a, const char *b)
{
    if (!a || !b)
        return false;
    const size_t aLength = strlen(a);
    return aLength == strlen(b) && _strnicmp(a, b, aLength) == 0;
}
}
