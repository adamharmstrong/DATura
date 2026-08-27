#pragma once

#include <cstddef>
#include <windows.h>

namespace FFXIInstallPath
{
    bool DetectFromRegistry(char* outPath, std::size_t outPathSize);
    bool LoadSavedPath(const char* applicationRegistryKey, const char* valueName,
                       char* outPath, std::size_t outPathSize);
    void SavePath(const char* applicationRegistryKey, const char* valueName, const char* path);
    void ClearSavedPath(const char* applicationRegistryKey, const char* valueName);

    bool InitializePath(const char* applicationRegistryKey, const char* valueName,
                        const char* defaultPath, char* outPath, std::size_t outPathSize);
    bool BrowseForFolder(HWND owner, const char* title, char* outPath, std::size_t outPathSize);
}
