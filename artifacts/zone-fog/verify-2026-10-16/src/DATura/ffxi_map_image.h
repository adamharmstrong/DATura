#pragma once

#include <windows.h>
#include <string>

namespace FFXIMapImage
{
struct Image
{
    HBITMAP bitmap = nullptr;
    int width = 0;
    int height = 0;
    int mapNumber = -1;
    std::string sourcePath;
};

bool Load(const char* ffxiRoot, int zoneId, Image& image);
void Release(Image& image);
}
