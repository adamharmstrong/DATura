#pragma once

#include <cstddef>

// Helpers for working with paths relative to an installed FFXI client.
// Callers supply the install root so these routines remain independent of
// DATura's UI and application state.
namespace FFXIPath
{
    struct ZoneResourceFiles
    {
        int id = -1;
        const char* name = "";
        const char* dialogDat = "";
        const char* npcDat = "";
    };

    bool StringEqualsNoCase(const char* a, const char* b);
    bool StringStartsWithNoCase(const char* text, const char* prefix);
    void MakeRelativePath(const char* ffxiRoot, const char* path, char* outPath, std::size_t outPathSize);

    const char* FindZoneNameByModelPath(const char* ffxiRoot, const char* path);
    int FindZoneIDByModelPath(const char* ffxiRoot, const char* path);
    bool GetZoneResourceFiles(int zoneId, ZoneResourceFiles& outFiles);

    bool FileExists(const char* path);
    void BuildFullPath(const char* ffxiRoot, const char* relativePath, char* outPath, std::size_t outPathSize);
    bool ResolveFileId(const char* ffxiRoot, int datId, unsigned int* outFileId);
    bool BuildRelativePathFromFileId(unsigned int fileId, char* outPath, std::size_t outPathSize);
    bool ResolveZoneModelPath(const char* ffxiRoot, int zoneId, char* outFullPath, std::size_t outFullPathSize,
                              char* outRelativePath = nullptr, std::size_t outRelativePathSize = 0,
                              bool* outUsedFtable = nullptr);
}
