#include "stdafx.h"
#include "zone_object_list_columns.h"

#include "zone_object_list_view.h"

namespace ZoneObjectListColumns
{
void SetupMapObjectColumns(const HWND list, int& columnMode, const int mode)
{
    if (!list || columnMode == mode)
        return;

    ZoneObjectListView::ClearColumns(list);
    ZoneObjectListView::AddColumn(list, 0, "Vis", 30);
    if (mode == 0)
    {
        ZoneObjectListView::AddColumn(list, 1, "Map Record", 82);
        ZoneObjectListView::AddColumn(list, 2, "MapGeo Name", 118);
    }
    else
    {
        ZoneObjectListView::AddColumn(list, 1, "MapGeo Chunk", 92);
        ZoneObjectListView::AddColumn(list, 2, "MapGeo Name", 118);
    }
    ZoneObjectListView::AddColumn(list, 3, "Position", 105);
    ZoneObjectListView::AddColumn(list, 4, "Rotation", 105);
    ZoneObjectListView::AddColumn(list, 5, "Scale", 105);
    ZoneObjectListView::AddColumn(list, 6, "data2[0], data2[4]", 145);
    ZoneObjectListView::AddColumn(list, 7, "Extra Vec", 130);
    ZoneObjectListView::AddColumn(list, 8, "Raw data2[0..7]", 230);
    columnMode = mode;
}

void SetupCollisionColumns(const HWND list, int& columnMode, const int mode)
{
    if (!list || columnMode == mode)
        return;

    ZoneObjectListView::ClearColumns(list);
    ZoneObjectListView::AddColumn(list, 0, "Grid Entry", 150);
    ZoneObjectListView::AddColumn(list, 1, "Grid Cell", 70);
    ZoneObjectListView::AddColumn(list, 2, "Offsets", 116);
    ZoneObjectListView::AddColumn(list, 3, "Tris", 46);
    ZoneObjectListView::AddColumn(list, 4, "Bounds Min", 110);
    ZoneObjectListView::AddColumn(list, 5, "Bounds Max", 110);
    columnMode = mode;
}

void SetupDrawBatchColumns(const HWND list, int& columnMode, const int mode)
{
    if (!list || columnMode == mode)
        return;

    ZoneObjectListView::ClearColumns(list);
    ZoneObjectListView::AddColumn(list, 0, "Batch", 165);
    ZoneObjectListView::AddColumn(list, 1, "Source", 105);
    ZoneObjectListView::AddColumn(list, 2, "Material", 115);
    ZoneObjectListView::AddColumn(list, 3, "Mode", 62);
    ZoneObjectListView::AddColumn(list, 4, "Verts/Idx", 76);
    ZoneObjectListView::AddColumn(list, 5, "Stride", 52);
    ZoneObjectListView::AddColumn(list, 6, "Render", 92);
    ZoneObjectListView::AddColumn(list, 7, "Flags", 230);
    ZoneObjectListView::AddColumn(list, 8, "Super Bounds", 190);
    ZoneObjectListView::AddColumn(list, 9, "Sub Bounds", 190);
    columnMode = mode;
}
}
