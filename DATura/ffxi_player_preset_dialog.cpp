#include "stdafx.h"
#include "ffxi_player_preset_dialog.h"

#include "ffxi_file_io.h"

#include <cstring>

namespace
{
const char kPresetFilter[] = "DATura Player Preset (*.datura-player)\0*.datura-player\0All Files (*.*)\0*.*\0";
}

namespace FFXIPlayerPresetDialog
{
bool PromptForSavePath(HWND owner, char* outPath, std::size_t outPathSize)
{
    if (!outPath || outPathSize == 0)
        return false;
    outPath[0] = '\0';

    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = kPresetFilter;
    ofn.lpstrFile = outPath;
    ofn.nMaxFile = (DWORD)outPathSize;
    ofn.lpstrTitle = "Save Player Preset";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = "datura-player";
    return GetSaveFileNameA(&ofn) != FALSE;
}

void Write(const char* path, const FFXIPlayerPreset::Data& preset)
{
    if (!path || !path[0])
        return;
    HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return;

    char text[640] = {};
    FFXIPlayerPreset::BuildText(preset, text, sizeof(text));
    DWORD written = 0;
    WriteFile(hFile, text, (DWORD)std::strlen(text), &written, nullptr);
    CloseHandle(hFile);
}

bool Load(HWND owner, FFXIPlayerPreset::Data* inOutPreset)
{
    if (!inOutPreset)
        return false;

    char path[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = kPresetFilter;
    ofn.lpstrFile = path;
    ofn.nMaxFile = sizeof(path);
    ofn.lpstrTitle = "Load Player Preset";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameA(&ofn))
        return false;

    char* text = nullptr;
    if (!FFXIFileIO::ReadWholeTextFile(path, &text))
        return false;

    const bool parsed = FFXIPlayerPreset::ParseText(text, inOutPreset);
    delete[] text;
    return parsed;
}
}
