#pragma once

#include <windows.h>

namespace FFXINationSelection
{
struct Info
{
    const char* name;
    const char* subtitle;
    const char* textureName;
    const char* description;
    int zoneId;
    COLORREF accent;
};

int Count();
const Info& InfoForIndex(int index);
}
