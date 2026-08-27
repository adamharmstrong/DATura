#include "stdafx.h"
#include "zone_object_panel_loading.h"

#include "zone_object_list_columns.h"

#include <commctrl.h>

namespace ZoneObjectPanelLoading
{
bool Begin(Context &context)
{
    if (!context.panel || !context.placedList || !context.unreferencedList ||
        !context.collisionList || !context.drawBatchList || !context.dataTree ||
        !context.rawDataTree || !context.collisionDataTree || !context.placedColumnMode ||
        !context.unreferencedColumnMode || !context.collisionColumnMode ||
        !context.drawBatchColumnMode || context.populateMessage == 0)
    {
        return false;
    }

    if (context.zoneLabel)
        SetWindowTextA(context.zoneLabel, context.loadedZoneLabel);
    ZoneObjectListColumns::SetupMapObjectColumns(context.placedList, *context.placedColumnMode, 0);
    ZoneObjectListColumns::SetupMapObjectColumns(
        context.unreferencedList, *context.unreferencedColumnMode, 1);
    ZoneObjectListColumns::SetupCollisionColumns(context.collisionList, *context.collisionColumnMode, 0);
    ZoneObjectListColumns::SetupDrawBatchColumns(context.drawBatchList, *context.drawBatchColumnMode, 0);
    ListView_DeleteAllItems(context.placedList);
    ListView_DeleteAllItems(context.unreferencedList);
    ListView_DeleteAllItems(context.collisionList);
    ListView_DeleteAllItems(context.drawBatchList);
    TreeView_DeleteAllItems(context.dataTree);
    TreeView_DeleteAllItems(context.rawDataTree);
    TreeView_DeleteAllItems(context.collisionDataTree);
    if (context.loadingLabel)
    {
        SetWindowTextA(context.loadingLabel, "Loading Object List...");
        ShowWindow(context.loadingLabel, SW_SHOW);
        BringWindowToTop(context.loadingLabel);
    }
    UpdateWindow(context.panel);
    PostMessageA(context.panel, context.populateMessage, 0, 0);
    return true;
}
}
