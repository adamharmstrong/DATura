#pragma once

#include <windows.h>

namespace ZoneObjectPanelLoading
{
struct Context
{
    HWND panel = NULL;
    HWND zoneLabel = NULL;
    HWND placedList = NULL;
    HWND unreferencedList = NULL;
    HWND collisionList = NULL;
    HWND drawBatchList = NULL;
    HWND dataTree = NULL;
    HWND rawDataTree = NULL;
    HWND collisionDataTree = NULL;
    HWND loadingLabel = NULL;
    const char *loadedZoneLabel = "";
    int *placedColumnMode = nullptr;
    int *unreferencedColumnMode = nullptr;
    int *collisionColumnMode = nullptr;
    int *drawBatchColumnMode = nullptr;
    UINT populateMessage = 0;
};

bool Begin(Context &context);
}
