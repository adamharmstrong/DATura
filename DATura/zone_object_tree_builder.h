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
};

void Build(const Context& context);
}
