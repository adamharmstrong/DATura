#include "stdafx.h"
#include "ffxi_dat_resolver.h"
#include "ffxi_paths.h"

#include "ffxi_resource.h"
#include "zone_dat_table.h"

#include <cctype>
#include <cstdio>
#include <cstring>

namespace FFXIPath
{
bool StringEqualsNoCase(const char* a, const char* b)
{
    if (!a || !b)
        return a == b;

    while (*a && *b)
    {
        const int ca = std::tolower(static_cast<unsigned char>(*a));
        const int cb = std::tolower(static_cast<unsigned char>(*b));
        if (ca != cb)
            return false;
        ++a;
        ++b;
    }

    return *a == *b;
}

bool StringStartsWithNoCase(const char* text, const char* prefix)
{
    if (!text || !prefix)
        return false;

    while (*prefix)
    {
        if (!*text)
            return false;
        const int ct = std::tolower(static_cast<unsigned char>(*text));
        const int cp = std::tolower(static_cast<unsigned char>(*prefix));
        if (ct != cp)
            return false;
        ++text;
        ++prefix;
    }

    return true;
}

void MakeRelativePath(const char* ffxiRoot, const char* path, char* outPath, const std::size_t outPathSize)
{
    if (!outPath || outPathSize == 0)
        return;

    outPath[0] = 0;
    if (!path || !path[0])
        return;

    const size_t rootLen = ffxiRoot ? std::strlen(ffxiRoot) : 0;
    if (rootLen > 0 && StringStartsWithNoCase(path, ffxiRoot))
    {
        const char* relativeStart = path + rootLen;
        while (*relativeStart == '\\' || *relativeStart == '/')
            ++relativeStart;
        strcpy_s(outPath, outPathSize, relativeStart);
    }
    else
    {
        strcpy_s(outPath, outPathSize, path);
    }

    for (char* p = outPath; *p; ++p)
    {
        if (*p == '\\')
            *p = '/';
    }
}

const char* FindZoneNameByModelPath(const char* ffxiRoot, const char* path)
{
    char relativePath[MAX_PATH] = {};
    MakeRelativePath(ffxiRoot, path, relativePath, sizeof(relativePath));

    for (int i = 0; i < kFFXIZoneCount; ++i)
    {
        const FFXIZoneEntry& zone = kFFXIZoneTable[i];
        if (zone.modelDat[0] && StringEqualsNoCase(zone.modelDat, relativePath))
            return zone.name;
    }

    return "";
}

int FindZoneIDByModelPath(const char* ffxiRoot, const char* path)
{
    char relativePath[MAX_PATH] = {};
    MakeRelativePath(ffxiRoot, path, relativePath, sizeof(relativePath));

    for (int i = 0; i < kFFXIZoneCount; ++i)
    {
        const FFXIZoneEntry& zone = kFFXIZoneTable[i];
        if (zone.modelDat[0] && StringEqualsNoCase(zone.modelDat, relativePath))
            return zone.id;
    }

    return -1;
}

bool GetZoneResourceFiles(const int zoneId, ZoneResourceFiles& outFiles)
{
    outFiles = {};
    const FFXIZoneEntry* zone = FFXIZone::FindByID(zoneId);
    if (!zone)
        return false;

    outFiles.id = zone->id;
    outFiles.name = zone->name;
    outFiles.dialogDat = zone->dialogDat;
    outFiles.npcDat = zone->npcDat;
    return true;
}

bool FileExists(const char* path)
{
    if (!path || !path[0])
        return false;
    return FFXIDatResolver::FileExists(path);
}

void BuildFullPath(const char* ffxiRoot, const char* relativePath, char* outPath, const std::size_t outPathSize)
{
    if (!outPath || outPathSize == 0)
        return;
    outPath[0] = 0;
    if (!relativePath || !relativePath[0])
        return;

    strcpy_s(outPath, outPathSize, ffxiRoot ? ffxiRoot : "");
    if (outPath[0])
    {
        const size_t len = std::strlen(outPath);
        if (len > 0 && outPath[len - 1] != '\\' && outPath[len - 1] != '/')
            strcat_s(outPath, outPathSize, "\\");
    }
    strcat_s(outPath, outPathSize, relativePath);
}

bool ResolveFileId(const char* ffxiRoot, const int datId, unsigned int* outFileId)
{
    if (outFileId)
        *outFileId = 0;
    FFXIResource::ResolvedFile resolved;
    if (!FFXIResource::ResolveFileId(ffxiRoot, datId, resolved))
        return false;
    if (outFileId)
        *outFileId = MAKELONG(resolved.packedLocation, resolved.hive);
    return true;
}

bool BuildRelativePathFromFileId(const unsigned int fileId, char* outPath, const std::size_t outPathSize)
{
    if (!outPath || outPathSize == 0)
        return false;
    outPath[0] = 0;

    const unsigned int fileNo = LOWORD(fileId);
    const unsigned int hive = HIWORD(fileId);
    if (hive < 1 || hive > 19)
        return false;

    if (hive == 1)
        sprintf_s(outPath, outPathSize, "ROM/%u/%u.DAT", fileNo / 0x80, fileNo % 0x80);
    else
        sprintf_s(outPath, outPathSize, "ROM%u/%u/%u.DAT", hive, fileNo / 0x80, fileNo % 0x80);
    return true;
}

bool ResolveZoneModelPath(const char* ffxiRoot, const int zoneId, char* outFullPath, const std::size_t outFullPathSize,
                          char* outRelativePath, const std::size_t outRelativePathSize, bool* outUsedFtable)
{
    if (outFullPath && outFullPathSize > 0)
        outFullPath[0] = 0;
    if (outRelativePath && outRelativePathSize > 0)
        outRelativePath[0] = 0;
    if (outUsedFtable)
        *outUsedFtable = false;

    const FFXIZoneEntry* zone = FFXIZone::FindByID(zoneId);
    if (zone && zone->modelDat && zone->modelDat[0])
    {
        char staticFullPath[MAX_PATH] = {};
        BuildFullPath(ffxiRoot, zone->modelDat, staticFullPath, sizeof(staticFullPath));
        if (FileExists(staticFullPath))
        {
            if (outFullPath && outFullPathSize > 0)
                strcpy_s(outFullPath, outFullPathSize, staticFullPath);
            if (outRelativePath && outRelativePathSize > 0)
                strcpy_s(outRelativePath, outRelativePathSize, zone->modelDat);
            return true;
        }
    }

    // RZN's MapLib documents this lookup path as reliable for pre-Adoulin maps.
    // Later zones have static DAT paths in DATura, but the same zoneId+100 table
    // lookup can resolve to unrelated legacy DATs, so keep it as a guarded fallback.
    if (zoneId >= 256)
        return false;

    unsigned int fileId = 0;
    char resolvedRelativePath[MAX_PATH] = {};
    if (!ResolveFileId(ffxiRoot, zoneId + 100, &fileId) ||
        !BuildRelativePathFromFileId(fileId, resolvedRelativePath, sizeof(resolvedRelativePath)))
    {
        return false;
    }

    char resolvedFullPath[MAX_PATH] = {};
    BuildFullPath(ffxiRoot, resolvedRelativePath, resolvedFullPath, sizeof(resolvedFullPath));
    if (!FileExists(resolvedFullPath))
        return false;

    if (outFullPath && outFullPathSize > 0)
        strcpy_s(outFullPath, outFullPathSize, resolvedFullPath);
    if (outRelativePath && outRelativePathSize > 0)
        strcpy_s(outRelativePath, outRelativePathSize, resolvedRelativePath);
    if (outUsedFtable)
        *outUsedFtable = true;
    return true;
}
}
