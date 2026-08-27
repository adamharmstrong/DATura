#include "stdafx.h"
#include "zone_object_panel_arrangement.h"

#include "zone_object_panel_layout.h"

namespace ZoneObjectPanelArrangement
{
void Arrange(const Context& context, const int clientWidth, const int clientHeight)
{
    if (!context.paneWidths)
        return;

    const Controls& controls = context.controls;
    const int margin = 8;
    const int headerHeight = 56;
    const int bottomHeight = 230;
    const int listBottom = clientHeight - bottomHeight;
    int placedX = 0;
    int placedWidth = 0;
    int rawX = 0;
    int rawWidth = 0;
    int collisionX = 0;
    int collisionWidth = 0;
    ZoneObjectPanelLayout::ComputeZonePaneLayout(clientWidth, context.paneWidths,
                                                  &placedX, &placedWidth, &rawX, &rawWidth,
                                                  &collisionX, &collisionWidth);
    const int treeHeight = (listBottom > headerHeight) ? listBottom - headerHeight : 120;
    const int paneButtonTop = listBottom + 6;
    const int bottomTop = listBottom + 42;
    const int toolWidth = 150;
    const int inspectorX = margin + toolWidth + 14;
    const int actionY = bottomTop + 4;
    const int transformTop = bottomTop + 42;
    const int rowLabelWidth = 70;
    const int transformColumnWidth = 80;
    const int transformEditWidth = 76;

    if (controls.zoneLabel)
        MoveWindow(controls.zoneLabel, margin, 8, clientWidth - margin * 2, 20, TRUE);
    if (controls.combineTreeButton)
    {
        SetWindowTextA(controls.combineTreeButton,
                       context.combinedObjectTree ? "Split Lists" : "Combine Lists");
        MoveWindow(controls.combineTreeButton, clientWidth - margin - 128, 32, 128, 24, TRUE);
    }
    if (controls.drawBatchListLabel) ShowWindow(controls.drawBatchListLabel, SW_HIDE);
    if (controls.placedObjectList) ShowWindow(controls.placedObjectList, SW_HIDE);
    if (controls.unreferencedObjectList) ShowWindow(controls.unreferencedObjectList, SW_HIDE);
    if (controls.collisionObjectList) ShowWindow(controls.collisionObjectList, SW_HIDE);
    if (controls.drawBatchList) ShowWindow(controls.drawBatchList, SW_HIDE);
    if (context.combinedObjectTree)
    {
        if (controls.placedListLabel)
        {
            SetWindowTextA(controls.placedListLabel, "DAT File Contents");
            ShowWindow(controls.placedListLabel, SW_SHOW);
            MoveWindow(controls.placedListLabel, margin, 36, clientWidth - margin * 2 - 140, 18, TRUE);
        }
        if (controls.unreferencedListLabel) ShowWindow(controls.unreferencedListLabel, SW_HIDE);
        if (controls.collisionListLabel) ShowWindow(controls.collisionListLabel, SW_HIDE);
        if (controls.dataTree)
        {
            ShowWindow(controls.dataTree, SW_SHOW);
            MoveWindow(controls.dataTree, margin, headerHeight, clientWidth - margin * 2, treeHeight, TRUE);
        }
        if (controls.rawDataTree) ShowWindow(controls.rawDataTree, SW_HIDE);
        if (controls.collisionDataTree) ShowWindow(controls.collisionDataTree, SW_HIDE);
        if (controls.placedShowAllButton) MoveWindow(controls.placedShowAllButton, margin, paneButtonTop, 104, 24, TRUE);
        if (controls.placedHideAllButton) MoveWindow(controls.placedHideAllButton, margin + 110, paneButtonTop, 104, 24, TRUE);
        if (controls.unreferencedShowAllButton) MoveWindow(controls.unreferencedShowAllButton, margin + 220, paneButtonTop, 90, 24, TRUE);
        if (controls.unreferencedHideAllButton) MoveWindow(controls.unreferencedHideAllButton, margin + 316, paneButtonTop, 90, 24, TRUE);
        if (controls.collisionVisibleCheck) MoveWindow(controls.collisionVisibleCheck, margin + 424, paneButtonTop, 360, 24, TRUE);
    }
    else
    {
        if (controls.placedListLabel)
        {
            SetWindowTextA(controls.placedListLabel, "Placed Geometry");
            ShowWindow(controls.placedListLabel, SW_SHOW);
            MoveWindow(controls.placedListLabel, placedX, 36, placedWidth, 18, TRUE);
        }
        if (controls.unreferencedListLabel)
        {
            ShowWindow(controls.unreferencedListLabel, SW_SHOW);
            MoveWindow(controls.unreferencedListLabel, rawX, 36, rawWidth, 18, TRUE);
        }
        if (controls.collisionListLabel)
        {
            ShowWindow(controls.collisionListLabel, SW_SHOW);
            MoveWindow(controls.collisionListLabel, collisionX, 36, collisionWidth, 18, TRUE);
        }
        if (controls.dataTree)
        {
            ShowWindow(controls.dataTree, SW_SHOW);
            MoveWindow(controls.dataTree, placedX, headerHeight, placedWidth, treeHeight, TRUE);
        }
        if (controls.rawDataTree)
        {
            ShowWindow(controls.rawDataTree, SW_SHOW);
            MoveWindow(controls.rawDataTree, rawX, headerHeight, rawWidth, treeHeight, TRUE);
        }
        if (controls.collisionDataTree)
        {
            ShowWindow(controls.collisionDataTree, SW_SHOW);
            MoveWindow(controls.collisionDataTree, collisionX, headerHeight, collisionWidth, treeHeight, TRUE);
        }
        if (controls.placedShowAllButton) MoveWindow(controls.placedShowAllButton, placedX, paneButtonTop, 104, 24, TRUE);
        if (controls.placedHideAllButton) MoveWindow(controls.placedHideAllButton, placedX + 110, paneButtonTop, 104, 24, TRUE);
        if (controls.unreferencedShowAllButton) MoveWindow(controls.unreferencedShowAllButton, rawX, paneButtonTop, 90, 24, TRUE);
        if (controls.unreferencedHideAllButton) MoveWindow(controls.unreferencedHideAllButton, rawX + 96, paneButtonTop, 90, 24, TRUE);
        if (controls.collisionVisibleCheck) MoveWindow(controls.collisionVisibleCheck, collisionX, paneButtonTop, collisionWidth, 24, TRUE);
    }
    if (controls.loadingLabel) MoveWindow(controls.loadingLabel, 18, 58, 220, 32, TRUE);

    for (int index = 0; index < controls.toolButtonCount; ++index)
    {
        if (controls.toolButtons && controls.toolButtons[index])
            MoveWindow(controls.toolButtons[index], margin, bottomTop + index * 28, toolWidth, 26, TRUE);
    }

    if (controls.showSelectedButton) MoveWindow(controls.showSelectedButton, inspectorX, actionY, 160, 26, TRUE);
    if (controls.hideSelectedButton) MoveWindow(controls.hideSelectedButton, inspectorX + 166, actionY, 110, 26, TRUE);
    if (controls.highlightSelectedButton) MoveWindow(controls.highlightSelectedButton, inspectorX + 282, actionY, 128, 26, TRUE);
    if (controls.centerSelectedButton) MoveWindow(controls.centerSelectedButton, inspectorX + 416, actionY, 112, 26, TRUE);
    for (int column = 0; column < 3 && column < controls.unreferencedLabelCount; ++column)
    {
        if (controls.unreferencedLabels && controls.unreferencedLabels[column])
        {
            const int x = inspectorX + rowLabelWidth + column * transformColumnWidth;
            MoveWindow(controls.unreferencedLabels[column], x + 32, transformTop, 20, 20, TRUE);
        }
    }
    for (int row = 0; row < 3 && row + 3 < controls.unreferencedLabelCount; ++row)
    {
        if (controls.unreferencedLabels && controls.unreferencedLabels[row + 3])
            MoveWindow(controls.unreferencedLabels[row + 3], inspectorX,
                       transformTop + 24 + row * 24 + 4, rowLabelWidth, 20, TRUE);
    }
    for (int index = 0; index < controls.unreferencedEditCount; ++index)
    {
        const int x = inspectorX + rowLabelWidth + (index % 3) * transformColumnWidth;
        const int y = transformTop + 24 + (index / 3) * 24;
        if (controls.unreferencedEdits && controls.unreferencedEdits[index])
            MoveWindow(controls.unreferencedEdits[index], x, y, transformEditWidth, 22, TRUE);
    }
    if (controls.unreferencedApplyButton)
        MoveWindow(controls.unreferencedApplyButton, inspectorX + 328, transformTop + 48, 130, 26, TRUE);
    if (controls.statusLabel)
        MoveWindow(controls.statusLabel, inspectorX, clientHeight - 24,
                   clientWidth - inspectorX - margin, 20, TRUE);
}
}
