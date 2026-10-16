#pragma once

#include <windows.h>

namespace ZoneObjectTreeBuilder
{
struct Context
{
    HWND placedObjectList;
    HWND unreferencedObjectList;
    HWND dataTree;
    HWND rawDataTree;
    HWND collisionDataTree;
    bool combinedObjectTree;
    const char* loadedZoneLabel;
    void (*progress)(void* context, const char* stage) = nullptr;
    void* progressContext = nullptr;
};

void Build(const Context& context);
}
