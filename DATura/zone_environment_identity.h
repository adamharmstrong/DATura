#pragma once

#include <string>

namespace ZoneEnvironmentIdentity
{
bool IsEnvironmentObjectName(const std::string &name);
bool ContainsLowerToken(const std::string &text, const char *token);
bool EnvironmentMeshMatchesWeather(const std::string &objectName, const char *weatherPath);
bool ParseEnvironmentMeshIdentity(const std::string &objectName, std::string &directoryPath,
                                  std::string &resourceName, std::string &generatorName);
bool EnvironmentWeatherRootsMatch(const char *a, const char *b);
bool EnvironmentNamesMatch(const char *a, const char *b);
}
