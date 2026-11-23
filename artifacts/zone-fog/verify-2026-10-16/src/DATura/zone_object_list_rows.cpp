#include "stdafx.h"
#include "zone_object_list_rows.h"

#include "noesis_rapi.h"
#include "model_ff11.h"
#include "zone_object_list_view.h"
#include "zone_object_visibility.h"

#include <commctrl.h>

namespace ZoneObjectListRows
{
void InsertMapObjectRow(
    const HWND list, const int mapObjectIndex,
    const std::map<std::string, ZoneObjectTransform::DebugTransform>& overrides,
    const std::vector<std::string>& hiddenObjectNames)
{
    if (!list)
        return;
    const ff11MapObjectDebug_t* debugObject =
        (mapObjectIndex >= 0 && mapObjectIndex < (int)gFF11LastMapObjects.size()) ?
        &gFF11LastMapObjects[mapObjectIndex] : NULL;
    const char* displayName = Model_FF11_GetLastMapObjectDisplayName(mapObjectIndex);
    const char* objectName = debugObject ? debugObject->objectName : (displayName ? displayName : "");
    float trans[3] = {};
    float rot[3] = {};
    float scale[3] = {};
    Model_FF11_GetLastMapObjectTransform(mapObjectIndex, trans, scale, rot);

    const std::map<std::string, ZoneObjectTransform::DebugTransform>::const_iterator overrideIt =
        overrides.find(displayName ? displayName : "");
    if (overrideIt != overrides.end())
    {
        memcpy(trans, overrideIt->second.trans, sizeof(trans));
        memcpy(rot, overrideIt->second.rot, sizeof(rot));
        memcpy(scale, overrideIt->second.scale, sizeof(scale));
    }

    LVITEMA item = {};
    item.mask = LVIF_TEXT | LVIF_PARAM;
    item.iItem = ListView_GetItemCount(list);
    item.iSubItem = 0;
    item.pszText = const_cast<char*>("");
    item.lParam = mapObjectIndex;
    const int row = (int)SendMessageA(list, LVM_INSERTITEMA, 0, (LPARAM)&item);
    ListView_SetCheckState(list, row,
                           !ZoneObjectVisibility::IsHidden(hiddenObjectNames, displayName));

    char buffer[128];
    if (debugObject && debugObject->referencedByMap)
        sprintf_s(buffer, "%03d", debugObject->mapRecordIndex);
    else if (debugObject && debugObject->mapGeoIndex >= 0)
        sprintf_s(buffer, "%03d", debugObject->mapGeoIndex);
    else
        strcpy_s(buffer, "n/a");
    ZoneObjectListView::SetSubItem(list, row, 1, buffer);
    ZoneObjectListView::SetSubItem(list, row, 2, objectName);
    sprintf_s(buffer, "%.2f, %.2f, %.2f", trans[0], trans[1], trans[2]);
    ZoneObjectListView::SetSubItem(list, row, 3, buffer);
    sprintf_s(buffer, "%.3f, %.3f, %.3f", rot[0], rot[1], rot[2]);
    ZoneObjectListView::SetSubItem(list, row, 4, buffer);
    sprintf_s(buffer, "%.3f, %.3f, %.3f", scale[0], scale[1], scale[2]);
    ZoneObjectListView::SetSubItem(list, row, 5, buffer);
    if (debugObject && debugObject->referencedByMap)
        sprintf_s(buffer, "%08X, %08X", debugObject->objectFlags[0], debugObject->objectFlags[1]);
    else
        strcpy_s(buffer, "n/a");
    ZoneObjectListView::SetSubItem(list, row, 6, buffer);
    if (debugObject && debugObject->referencedByMap)
        sprintf_s(buffer, "%.2f, %.2f, %.2f, %.2f",
                  debugObject->vec[0], debugObject->vec[1], debugObject->vec[2], debugObject->vec[3]);
    else
        strcpy_s(buffer, "n/a");
    ZoneObjectListView::SetSubItem(list, row, 7, buffer);
    if (debugObject && debugObject->referencedByMap)
        sprintf_s(buffer, "%08X %08X %08X %08X %08X %08X %08X %08X",
                  debugObject->data2[0], debugObject->data2[1], debugObject->data2[2], debugObject->data2[3],
                  debugObject->data2[4], debugObject->data2[5], debugObject->data2[6], debugObject->data2[7]);
    else
        strcpy_s(buffer, "n/a");
    ZoneObjectListView::SetSubItem(list, row, 8, buffer);
}

void InsertCollisionRow(const HWND list, const int collisionMeshIndex)
{
    if (!list)
        return;

    const ff11CollisionMeshDebug_t* mesh = Model_FF11_GetLastCollisionMesh(collisionMeshIndex);
    if (!mesh)
        return;

    LVITEMA item = {};
    item.mask = LVIF_TEXT | LVIF_PARAM;
    item.iItem = ListView_GetItemCount(list);
    item.iSubItem = 0;
    item.pszText = const_cast<char*>(mesh->displayName);
    item.lParam = collisionMeshIndex;
    const int row = (int)SendMessageA(list, LVM_INSERTITEMA, 0, (LPARAM)&item);

    char buffer[96];
    sprintf_s(buffer, "%d,%d", mesh->gridX, mesh->gridY);
    ZoneObjectListView::SetSubItem(list, row, 1, buffer);
    sprintf_s(buffer, "T:%08X G:%08X", mesh->transformOfs, mesh->geometryOfs);
    ZoneObjectListView::SetSubItem(list, row, 2, buffer);
    sprintf_s(buffer, "%d", mesh->triCount);
    ZoneObjectListView::SetSubItem(list, row, 3, buffer);
    sprintf_s(buffer, "%.2f, %.2f, %.2f", mesh->boundsMin[0], mesh->boundsMin[1], mesh->boundsMin[2]);
    ZoneObjectListView::SetSubItem(list, row, 4, buffer);
    sprintf_s(buffer, "%.2f, %.2f, %.2f", mesh->boundsMax[0], mesh->boundsMax[1], mesh->boundsMax[2]);
    ZoneObjectListView::SetSubItem(list, row, 5, buffer);
}

void InsertDrawBatchRow(const HWND list, const int batchIndex)
{
    if (!list)
        return;

    const ff11MapGeoDrawBatchDebug_t* batch = Model_FF11_GetLastMapGeoDrawBatch(batchIndex);
    if (!batch)
        return;

    LVITEMA item = {};
    item.mask = LVIF_TEXT | LVIF_PARAM;
    item.iItem = ListView_GetItemCount(list);
    item.iSubItem = 0;
    item.pszText = const_cast<char*>(batch->displayName);
    item.lParam = batchIndex;
    const int row = (int)SendMessageA(list, LVM_INSERTITEMA, 0, (LPARAM)&item);

    char buffer[256];
    if (batch->mapRecordIndex >= 0)
        sprintf_s(buffer, "Map %03d / Geo %03d", batch->mapRecordIndex, batch->mapGeoIndex);
    else
        sprintf_s(buffer, "Geo %03d", batch->mapGeoIndex);
    ZoneObjectListView::SetSubItem(list, row, 1, buffer);
    ZoneObjectListView::SetSubItem(list, row, 2, batch->materialName);
    ZoneObjectListView::SetSubItem(list, row, 3, batch->indexMode == 0 ? "Tri List" : "Tri Strip");
    sprintf_s(buffer, "%d / %d", batch->vertexCount, batch->indexCount);
    ZoneObjectListView::SetSubItem(list, row, 4, buffer);
    sprintf_s(buffer, "%d", batch->vertexStride);
    ZoneObjectListView::SetSubItem(list, row, 5, buffer);
    ZoneObjectListView::SetSubItem(list, row, 6, batch->daturaRenderMode);
    sprintf_s(buffer, "flags:%04X flags2:%04X super:%d sub:%d obj:%08X,%08X alpha:%d cullOff:%d u4:%d u1:%d ofs:%08X",
              batch->blendFlags, batch->flags2, batch->superFlag, batch->subFlag,
              batch->objectFlags[0], batch->objectFlags[1],
              batch->galkaReeveWouldAlphaBlend ? 1 : 0,
              (batch->blendFlags & 0x2000u) ? 1 : 0,
              batch->runtimeFlag4000 ? 1 : 0,
              batch->runtimeFlag1000 ? 1 : 0,
              batch->drawOffset);
    ZoneObjectListView::SetSubItem(list, row, 7, buffer);
    sprintf_s(buffer, "%.2f, %.2f, %.2f / %.2f, %.2f, %.2f",
              batch->superBounds[0], batch->superBounds[1], batch->superBounds[2],
              batch->superBounds[3], batch->superBounds[4], batch->superBounds[5]);
    ZoneObjectListView::SetSubItem(list, row, 8, buffer);
    sprintf_s(buffer, "%.2f, %.2f, %.2f / %.2f, %.2f, %.2f",
              batch->subBounds[0], batch->subBounds[1], batch->subBounds[2],
              batch->subBounds[3], batch->subBounds[4], batch->subBounds[5]);
    ZoneObjectListView::SetSubItem(list, row, 9, buffer);
}
}
