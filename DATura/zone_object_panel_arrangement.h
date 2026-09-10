#pragma once

#include <windows.h>

namespace ZoneObjectPanelArrangement
{
struct Controls
{
    HWND zoneLabel;
    HWND combineTreeButton;
    HWND placedListLabel;
    HWND unreferencedListLabel;
    HWND collisionListLabel;
    HWND drawBatchListLabel;
    HWND placedObjectList;
    HWND unreferencedObjectList;
    HWND collisionObjectList;
    HWND drawBatchList;
    HWND dataTree;
    HWND rawDataTree;
    HWND collisionDataTree;
    HWND loadingLabel;
    HWND placedShowAllButton;
    HWND placedHideAllButton;
    HWND unreferencedShowAllButton;
    HWND unreferencedHideAllButton;
    HWND showSelectedButton;
    HWND hideSelectedButton;
    HWND highlightSelectedButton;
    HWND centerSelectedButton;
    HWND* toolButtons;
    int toolButtonCount;
    HWND* unreferencedLabels;
    int unreferencedLabelCount;
    HWND* unreferencedEdits;
    int unreferencedEditCount;
    HWND unreferencedApplyButton;
    HWND collisionVisibleCheck;
    HWND statusLabel;
};

struct Context
{
    Controls controls;
    int* paneWidths;
    bool combinedObjectTree;
};

void Arrange(const Context& context, int clientWidth, int clientHeight);
}
