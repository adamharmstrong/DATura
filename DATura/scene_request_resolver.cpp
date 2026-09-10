#include "stdafx.h"
#include "scene_request_resolver.h"

#include "ffxi_paths.h"
#include "zone_dat_table.h"

namespace SceneRequestResolver
{
Result ResolveRelativeModel(
    const char* ffxiRoot, const char* relativePath, const char* preferredName,
    const SceneLoadContext::Options& options)
{
    Result result;
    if (!ffxiRoot || !ffxiRoot[0] || !relativePath || !relativePath[0])
    {
        result.error = Error::InvalidRequest;
        return result;
    }

    char fullPath[MAX_PATH] = {};
    FFXIPath::BuildFullPath(ffxiRoot, relativePath, fullPath, sizeof(fullPath));
    result.request.path = fullPath;
    result.request.name = preferredName ? preferredName : "";
    result.request.options = options;
    if (!FFXIPath::FileExists(fullPath))
    {
        result.error = Error::ModelFileMissing;
        return result;
    }
    return result;
}

Result ResolveZone(const char* ffxiRoot, const int zoneId)
{
    Result result;
    result.zoneId = zoneId;
    if (!ffxiRoot || !ffxiRoot[0] || zoneId < 0)
    {
        result.error = Error::InvalidRequest;
        return result;
    }

    const FFXIZoneEntry* zone = FFXIZone::FindByID(zoneId);
    char fullPath[MAX_PATH] = {};
    if (!zone || !FFXIPath::ResolveZoneModelPath(
            ffxiRoot, zoneId, fullPath, sizeof(fullPath)))
    {
        result.error = Error::ZoneModelUnavailable;
        return result;
    }

    result.request.path = fullPath;
    result.request.name = zone->name ? zone->name : "";
    return result;
}
}
