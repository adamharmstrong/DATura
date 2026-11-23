#pragma once

#include "scene_load_context.h"

namespace SceneRequestResolver
{
enum class Error
{
    None,
    InvalidRequest,
    ModelFileMissing,
    ZoneModelUnavailable,
};

struct Result
{
    SceneLoadContext::Request request;
    Error error = Error::None;
    int zoneId = -1;

    bool Succeeded() const noexcept
    {
        return error == Error::None && request.HasPath();
    }
};

Result ResolveRelativeModel(
    const char* ffxiRoot, const char* relativePath, const char* preferredName,
    const SceneLoadContext::Options& options = {});
Result ResolveZone(const char* ffxiRoot, int zoneId);
}
