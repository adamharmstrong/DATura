#include "stdafx.h"
#include "ffxi_zone_resources.h"

#include "ffxi_paths.h"

#include <string>
#include <vector>

namespace
{
std::wstring WideFromAnsi(const char* text)
{
    if (!text || !text[0])
        return {};
    const int count = MultiByteToWideChar(CP_ACP, 0, text, -1, nullptr, 0);
    if (count <= 1)
        return {};
    std::wstring result(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(CP_ACP, 0, text, -1, result.data(), count);
    result.resize(static_cast<size_t>(count - 1));
    return result;
}

void AddUniqueExistingPath(std::vector<std::string>& paths, const char* path)
{
    if (!path || !path[0] || !FFXIPath::FileExists(path))
        return;
    for (const std::string& existing : paths)
    {
        if (FFXIPath::StringEqualsNoCase(existing.c_str(), path))
            return;
    }
    paths.emplace_back(path);
}

void AddResolvedPath(std::vector<std::string>& paths, const char* ffxiRoot, const int fileId)
{
    FFXIResource::ResolvedFile resolved;
    if (FFXIResource::ResolveFileId(ffxiRoot, fileId, resolved))
        AddUniqueExistingPath(paths, resolved.fullPath.c_str());
}
}

namespace FFXIZoneResources
{
bool BuildBrowseData(const char* ffxiRoot, const char* zoneModelPath, BrowseData& outData)
{
    outData = {};
    const int zoneId = FFXIPath::FindZoneIDByModelPath(ffxiRoot, zoneModelPath);
    FFXIPath::ZoneResourceFiles zone;
    if (zoneId < 0 || !FFXIPath::GetZoneResourceFiles(zoneId, zone))
        return false;

    outData.zoneId = zone.id;
    outData.zoneName = zone.name ? zone.name : "";
    std::vector<std::string> paths;
    char fullPath[MAX_PATH] = {};
    if (zone.dialogDat && zone.dialogDat[0])
    {
        FFXIPath::BuildFullPath(ffxiRoot, zone.dialogDat, fullPath, sizeof(fullPath));
        AddUniqueExistingPath(paths, fullPath);
    }
    if (zone.npcDat && zone.npcDat[0])
    {
        FFXIPath::BuildFullPath(ffxiRoot, zone.npcDat, fullPath, sizeof(fullPath));
        AddUniqueExistingPath(paths, fullPath);
    }

    // Historical file-ID banks from POLUtils are retained as fallbacks for
    // installations or zones whose static table entry is incomplete.
    if (zoneId <= 255)
    {
        AddResolvedPath(paths, ffxiRoot, 6420 + zoneId);
        AddResolvedPath(paths, ffxiRoot, 6720 + zoneId);
        AddResolvedPath(paths, ffxiRoot, 67910 + zoneId);
    }
    if (zoneId >= 255)
    {
        AddResolvedPath(paths, ffxiRoot, 85590 + (zoneId - 255));
        AddResolvedPath(paths, ffxiRoot, 86490 + (zoneId - 255));
    }

    for (const std::string& path : paths)
    {
        FFXIResource::ParseResult parsed;
        if (!FFXIResource::ParseFile(path.c_str(), parsed))
            continue;
        ++outData.parsedFileCount;

        char relativePath[MAX_PATH] = {};
        FFXIPath::MakeRelativePath(ffxiRoot, path.c_str(), relativePath, sizeof(relativePath));
        const std::wstring wideRelative = WideFromAnsi(relativePath);
        if (!outData.formats.empty())
            outData.formats += L"; ";
        outData.formats += parsed.format + L" [" + wideRelative + L"]";
        if (!parsed.warning.empty() && outData.warnings.find(parsed.warning) == std::wstring::npos)
        {
            if (!outData.warnings.empty())
                outData.warnings += L" ";
            outData.warnings += parsed.warning;
        }
        for (FFXIResource::Row row : parsed.rows)
        {
            if (!row.details.empty())
                row.details += L" | ";
            row.details += wideRelative;
            if (!FFXIPath::StringEqualsNoCase(parsed.logicalPath.c_str(), parsed.sourcePath.c_str()))
                row.details += L" | Source: " + WideFromAnsi(parsed.sourcePath.c_str());
            outData.rows.push_back(std::move(row));
        }
    }
    return true;
}
}
