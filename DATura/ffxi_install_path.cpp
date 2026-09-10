#include "stdafx.h"
#include "ffxi_install_path.h"

#include <cstring>
#include <shlobj.h>

namespace
{
constexpr char kPlayOnlineRegKey[] = "SOFTWARE\\PlayOnlineUS\\1000";
constexpr char kPlayOnlineRegKeyWow[] = "SOFTWARE\\WOW6432Node\\PlayOnlineUS\\1000";
constexpr char kPlayOnlineValue[] = "InstallFolder";

bool ReadLocalMachineString(const char* keyPath, const char* valueName,
                            char* outPath, const std::size_t outPathSize)
{
    if (!outPath || outPathSize == 0 || outPathSize > MAXDWORD)
        return false;

    HKEY key = nullptr;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, keyPath, 0, KEY_READ, &key) != ERROR_SUCCESS)
        return false;

    DWORD type = 0;
    DWORD size = static_cast<DWORD>(outPathSize);
    const bool success = RegQueryValueExA(key, valueName, nullptr, &type,
        reinterpret_cast<LPBYTE>(outPath), &size) == ERROR_SUCCESS &&
        (type == REG_SZ || type == REG_EXPAND_SZ);
    RegCloseKey(key);
    return success;
}
}

namespace FFXIInstallPath
{
bool DetectFromRegistry(char* outPath, const std::size_t outPathSize)
{
    return ReadLocalMachineString(kPlayOnlineRegKey, kPlayOnlineValue, outPath, outPathSize) ||
           ReadLocalMachineString(kPlayOnlineRegKeyWow, kPlayOnlineValue, outPath, outPathSize);
}

bool LoadSavedPath(const char* applicationRegistryKey, const char* valueName,
                   char* outPath, const std::size_t outPathSize)
{
    if (!applicationRegistryKey || !valueName || !outPath || outPathSize == 0 || outPathSize > MAXDWORD)
        return false;

    HKEY key = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, applicationRegistryKey, 0, KEY_READ, &key) != ERROR_SUCCESS)
        return false;

    DWORD type = 0;
    DWORD size = static_cast<DWORD>(outPathSize);
    const bool success = RegQueryValueExA(key, valueName, nullptr, &type,
        reinterpret_cast<LPBYTE>(outPath), &size) == ERROR_SUCCESS && type == REG_SZ;
    RegCloseKey(key);
    return success;
}

void SavePath(const char* applicationRegistryKey, const char* valueName, const char* path)
{
    if (!applicationRegistryKey || !valueName || !path)
        return;

    HKEY key = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, applicationRegistryKey, 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr,
                        &key, nullptr) != ERROR_SUCCESS)
    {
        return;
    }

    RegSetValueExA(key, valueName, 0, REG_SZ,
                   reinterpret_cast<const BYTE*>(path),
                   static_cast<DWORD>(std::strlen(path) + 1));
    RegCloseKey(key);
}

void ClearSavedPath(const char* applicationRegistryKey, const char* valueName)
{
    if (!applicationRegistryKey || !valueName)
        return;

    HKEY key = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, applicationRegistryKey, 0, KEY_WRITE, &key) != ERROR_SUCCESS)
        return;
    RegDeleteValueA(key, valueName);
    RegCloseKey(key);
}

bool InitializePath(const char* applicationRegistryKey, const char* valueName,
                    const char* defaultPath, char* outPath, const std::size_t outPathSize)
{
    if (!defaultPath || !outPath || outPathSize == 0)
        return false;

    if (LoadSavedPath(applicationRegistryKey, valueName, outPath, outPathSize))
        return true;

    return strcpy_s(outPath, outPathSize, defaultPath) == 0;
}

bool BrowseForFolder(const HWND owner, const char* title, char* outPath, const std::size_t outPathSize)
{
    if (!title || !outPath || outPathSize == 0)
        return false;

    char displayName[MAX_PATH] = {};
    BROWSEINFOA browseInfo = {};
    browseInfo.hwndOwner = owner;
    browseInfo.pszDisplayName = displayName;
    browseInfo.lpszTitle = title;
    browseInfo.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST itemIdList = SHBrowseForFolderA(&browseInfo);
    if (!itemIdList)
        return false;

    char selectedPath[MAX_PATH] = {};
    const bool hasSelectedPath = SHGetPathFromIDListA(itemIdList, selectedPath) != FALSE;
    CoTaskMemFree(itemIdList);
    if (!hasSelectedPath)
        return false;

    const std::size_t length = std::strlen(selectedPath);
    if (length > 0 && selectedPath[length - 1] != '\\')
    {
        if (length + 1 >= sizeof(selectedPath))
            return false;
        selectedPath[length] = '\\';
        selectedPath[length + 1] = '\0';
    }

    return strcpy_s(outPath, outPathSize, selectedPath) == 0;
}
}
