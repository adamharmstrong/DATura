#pragma once

#include <string>

namespace SceneLoadContext
{
struct Options
{
    bool userContentLoad = true;
    bool renderEnvironment = false;
    bool renderUnreferenced = false;
    bool preserveGameScreen = false;
};

struct Request
{
    std::string path;
    std::string name;
    Options options;

    bool HasPath() const noexcept;
};

class State final
{
public:
    bool HasRememberedRequest() const noexcept;
    Request Snapshot() const;
    const std::string& Path() const noexcept;
    bool MatchesPath(const char* path) const noexcept;

    void Remember(const char* path, const char* name, const Options& options);
    void Remember(Request request);
    void Clear() noexcept;

private:
    Request remembered_;
};

std::string FormatLoadedLabel(
    const char* preferredName, const char* discoveredName,
    const char* path, const char* relativePath);
}
