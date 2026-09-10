#include "stdafx.h"
#include "scene_load_context.h"

#include <cctype>
#include <utility>

namespace
{
bool EqualsNoCase(const char* left, const char* right) noexcept
{
    if (!left || !right)
        return left == right;

    while (*left && *right)
    {
        const int leftCharacter = std::tolower(static_cast<unsigned char>(*left));
        const int rightCharacter = std::tolower(static_cast<unsigned char>(*right));
        if (leftCharacter != rightCharacter)
            return false;
        ++left;
        ++right;
    }

    return *left == *right;
}

const char* FileName(const char* path) noexcept
{
    if (!path)
        return "";

    const char* name = path;
    for (const char* cursor = path; *cursor; ++cursor)
    {
        if (*cursor == '\\' || *cursor == '/')
            name = cursor + 1;
    }
    return name;
}
}

namespace SceneLoadContext
{
bool Request::HasPath() const noexcept
{
    return !path.empty();
}

bool State::HasRememberedRequest() const noexcept
{
    return remembered_.HasPath();
}

Request State::Snapshot() const
{
    return remembered_;
}

const std::string& State::Path() const noexcept
{
    return remembered_.path;
}

bool State::MatchesPath(const char* path) const noexcept
{
    return path && remembered_.HasPath() && EqualsNoCase(remembered_.path.c_str(), path);
}

void State::Remember(const char* path, const char* name, const Options& options)
{
    Request request;
    request.path = path ? path : "";
    request.name = name ? name : "";
    request.options = options;
    Remember(std::move(request));
}

void State::Remember(Request request)
{
    remembered_ = std::move(request);
}

void State::Clear() noexcept
{
    remembered_ = {};
}

std::string FormatLoadedLabel(
    const char* preferredName, const char* discoveredName,
    const char* path, const char* relativePath)
{
    if (!path || !path[0])
        return "No zone loaded";

    const char* name = preferredName && preferredName[0] ? preferredName : discoveredName;
    if (!name || !name[0])
        name = FileName(path);

    const char* displayPath = relativePath && relativePath[0] ? relativePath : path;
    return std::string("Loaded zone: ") + name + " (" + displayPath + ")";
}
}
