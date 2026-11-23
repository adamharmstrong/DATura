#pragma once

#include <cstddef>
#include <windows.h>

#include "ffxi_player_preset.h"

namespace FFXIPlayerPresetDialog
{
    bool PromptForSavePath(HWND owner, char* outPath, std::size_t outPathSize);
    void Write(const char* path, const FFXIPlayerPreset::Data& preset);
    bool Load(HWND owner, FFXIPlayerPreset::Data* inOutPreset);
}
