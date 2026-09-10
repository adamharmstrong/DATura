#include "stdafx.h"
#include "zone_object_tree_population.h"

#include "noesis_rapi.h"
#include "model_ff11.h"
#include "zone_object_tree_view.h"
#include "zone_object_visibility.h"

namespace
{
bool DrawBatchBelongsToMapObject(const ff11MapGeoDrawBatchDebug_t* batch,
                                 const ff11MapObjectDebug_t& object)
{
    if (!batch)
        return false;
    if (object.referencedByMap)
        return batch->mapRecordIndex == object.mapRecordIndex;
    return batch->mapRecordIndex < 0 && batch->mapGeoIndex == object.mapGeoIndex;
}

void AddDrawBatchesForMapObject(const HWND tree, const HTREEITEM parent,
                                const ff11MapObjectDebug_t& object)
{
    int count = 0;
    for (int index = 0; index < Model_FF11_GetLastMapGeoDrawBatchCount(); ++index)
    {
        if (DrawBatchBelongsToMapObject(Model_FF11_GetLastMapGeoDrawBatch(index), object))
            ++count;
    }

    char text[96] = {};
    sprintf_s(text, "Draw batches: %d", count);
    const HTREEITEM drawRoot = ZoneObjectTreeView::AddZoneTreeItem(tree, parent, text);

    for (int index = 0; index < Model_FF11_GetLastMapGeoDrawBatchCount(); ++index)
    {
        const ff11MapGeoDrawBatchDebug_t* batch = Model_FF11_GetLastMapGeoDrawBatch(index);
        if (!DrawBatchBelongsToMapObject(batch, object))
            continue;
        const HTREEITEM batchItem = ZoneObjectTreeView::AddZoneTreeItem(
            tree, drawRoot, batch->displayName,
            ZoneObjectTreeView::MakeZoneTreeParam(ZoneObjectTreeView::kZoneTreeNode_DrawBatch, index));
        ZoneObjectTreeView::AddZoneTreePlaceholder(tree, batchItem);
    }
}

void PopulateMapObjectFields(const HWND tree, const HTREEITEM item, const int index,
                             const std::vector<std::string>& hiddenObjectNames)
{
    if (index < 0 || index >= Model_FF11_GetLastMapObjectCount())
        return;

    const ff11MapObjectDebug_t& object = gFF11LastMapObjects[index];
    ZoneObjectTreeView::AddZoneTreeField(tree, item, "MapGeo name", object.objectName);
    if (object.referencedByMap)
    {
        ZoneObjectTreeView::AddZoneTreeIntField(tree, item, "Map record index", object.mapRecordIndex);
        ZoneObjectTreeView::AddZoneTreeHexField(tree, item, "Object flags 0 (map data2[0])", object.objectFlags[0]);
        ZoneObjectTreeView::AddZoneTreeHexField(tree, item, "Object flags 1 (map data2[4])", object.objectFlags[1]);
        const HTREEITEM data2Root = ZoneObjectTreeView::AddZoneTreeItem(tree, item, "Raw map object data2[0..7]");
        for (int dataIndex = 0; dataIndex < 8; ++dataIndex)
        {
            char label[64] = {};
            sprintf_s(label, "data2[%d]%s", dataIndex,
                      (dataIndex == 0) ? " - object flags 0" :
                      ((dataIndex == 4) ? " - object flags 1" : " - unknown"));
            ZoneObjectTreeView::AddZoneTreeHexField(tree, data2Root, label, object.data2[dataIndex]);
        }
        ZoneObjectTreeView::AddZoneTreeVec3Field(tree, item,
            "LOD distances: high / medium / draw (+0x38/+0x3C/+0x40)", object.vec + 1);
        ZoneObjectTreeView::AddZoneTreeIntField(tree, item, "Replacement sub-area (+0x50)", object.data2[3]);
        ZoneObjectTreeView::AddZoneTreeField(tree, item, "Geometry resident",
            object.replacedByRoom ? "No - replaced by loaded room" : "Yes");
    }
    else
    {
        ZoneObjectTreeView::AddZoneTreeIntField(tree, item, "MapGeo chunk index", object.mapGeoIndex);
    }
    ZoneObjectTreeView::AddZoneTreeVec3Field(tree, item, "Position", object.trans);
    ZoneObjectTreeView::AddZoneTreeVec3Field(tree, item, "Rotation", object.rot);
    ZoneObjectTreeView::AddZoneTreeVec3Field(tree, item, "Scale", object.scale);
    ZoneObjectTreeView::AddZoneTreeField(
        tree, item, "Visible",
        ZoneObjectVisibility::IsHidden(hiddenObjectNames, object.displayName) ? "0" : "1");
    AddDrawBatchesForMapObject(tree, item, object);
}

void PopulateDrawBatchFields(const HWND tree, const HTREEITEM item, const int index)
{
    const ff11MapGeoDrawBatchDebug_t* batch = Model_FF11_GetLastMapGeoDrawBatch(index);
    if (!batch)
        return;

    ZoneObjectTreeView::AddZoneTreeField(tree, item, "MapGeo name", batch->objectName);
    ZoneObjectTreeView::AddZoneTreeBytesField(tree, item, "MapGeo header bytes", batch->mapGeoHeaderData, 4);
    ZoneObjectTreeView::AddZoneTreeHexField(tree, item, "MapGeo unknown1", batch->mapGeoUnknown1);
    ZoneObjectTreeView::AddZoneTreeField(tree, item, "MapGeo unknown name/tag", batch->mapGeoUnknownName);
    ZoneObjectTreeView::AddZoneTreeIntField(tree, item, "MapGeo index", batch->mapGeoIndex);
    ZoneObjectTreeView::AddZoneTreeIntField(tree, item, "Map record index", batch->mapRecordIndex);
    ZoneObjectTreeView::AddZoneTreeIntField(tree, item, "Super index", batch->superIndex);
    ZoneObjectTreeView::AddZoneTreeIntField(tree, item, "Sub index", batch->subIndex);
    ZoneObjectTreeView::AddZoneTreeHexField(tree, item, "Draw offset", (unsigned int)batch->drawOffset);
    ZoneObjectTreeView::AddZoneTreeField(tree, item, "Material", batch->materialName);
    ZoneObjectTreeView::AddZoneTreeField(tree, item, "Index mode",
                                         batch->indexMode == 0 ? "Triangle list" : "Triangle strip");
    ZoneObjectTreeView::AddZoneTreeIntField(tree, item, "Vertex count", batch->vertexCount);
    ZoneObjectTreeView::AddZoneTreeIntField(tree, item, "Index count", batch->indexCount);
    ZoneObjectTreeView::AddZoneTreeIntField(tree, item, "Vertex stride", batch->vertexStride);
    ZoneObjectTreeView::AddZoneTreeHex16Field(tree, item, "Runtime mesh flags", batch->blendFlags);
    ZoneObjectTreeView::AddZoneTreeHex16Field(
        tree, item, "Uninterpreted runtime flag bits",
        batch->blendFlags & ~(0x1000u | 0x2000u | 0x4000u | 0x8000u));
    ZoneObjectTreeView::AddZoneTreeHex16Field(tree, item, "Flags 2", batch->flags2);
    ZoneObjectTreeView::AddZoneTreeField(tree, item, "DATura render mode", batch->daturaRenderMode);
    ZoneObjectTreeView::AddZoneTreeField(tree, item, "DATura render reason", batch->daturaRenderReason);
    ZoneObjectTreeView::AddZoneTreeField(tree, item, "DATura hard alpha", batch->daturaHardAlpha ? "1" : "0");
    ZoneObjectTreeView::AddZoneTreeField(tree, item, "DATura soft blend", batch->daturaSoftBlend ? "1" : "0");
    ZoneObjectTreeView::AddZoneTreeField(tree, item, "Transparent (runtime flag 0x8000)",
                                         batch->galkaReeveUseAlpha ? "1" : "0");
    ZoneObjectTreeView::AddZoneTreeField(tree, item, "Disable culling (runtime flag 0x2000)",
                                         (batch->blendFlags & 0x2000u) ? "1" : "0");
    ZoneObjectTreeView::AddZoneTreeField(tree, item, "Unknown runtime flag 0x4000",
                                         batch->runtimeFlag4000 ? "1" : "0");
    ZoneObjectTreeView::AddZoneTreeField(tree, item, "Unknown runtime flag 0x1000",
                                         batch->runtimeFlag1000 ? "1" : "0");
    ZoneObjectTreeView::AddZoneTreeField(tree, item, "DATura applies alpha blending",
                                         batch->galkaReeveWouldAlphaBlend ? "1" : "0");
    ZoneObjectTreeView::AddZoneTreeIntField(tree, item, "Super flag", batch->superFlag);
    ZoneObjectTreeView::AddZoneTreeIntField(tree, item, "Sub flag", batch->subFlag);
    ZoneObjectTreeView::AddZoneTreeHexField(tree, item, "Object flags 0", batch->objectFlags[0]);
    ZoneObjectTreeView::AddZoneTreeHexField(tree, item, "Object flags 1", batch->objectFlags[1]);
    ZoneObjectTreeView::AddZoneTreeBoundsField(tree, item, "Super bounds", batch->superBounds);
    ZoneObjectTreeView::AddZoneTreeBoundsField(tree, item, "Sub bounds", batch->subBounds);
}

void PopulateCollisionFields(const HWND tree, const HTREEITEM item, const int index)
{
    const ff11CollisionMeshDebug_t* mesh = Model_FF11_GetLastCollisionMesh(index);
    if (!mesh)
        return;

    char text[64] = {};
    sprintf_s(text, "%d, %d", mesh->gridX, mesh->gridY);
    ZoneObjectTreeView::AddZoneTreeField(tree, item, "Grid cell", text);
    ZoneObjectTreeView::AddZoneTreeHexField(tree, item, "Transform offset", (unsigned int)mesh->transformOfs);
    ZoneObjectTreeView::AddZoneTreeHexField(tree, item, "Geometry offset", (unsigned int)mesh->geometryOfs);
    ZoneObjectTreeView::AddZoneTreeHexField(tree, item, "Collision bucket flags", mesh->bucketFlags);
    ZoneObjectTreeView::AddZoneTreeHexField(tree, item, "Observed 2-bit index-flag values", mesh->indexFlagValueMask);
    ZoneObjectTreeView::AddZoneTreeIntField(tree, item, "Triangle start", mesh->triStart);
    ZoneObjectTreeView::AddZoneTreeIntField(tree, item, "Triangle count", mesh->triCount);
    ZoneObjectTreeView::AddZoneTreeVec3Field(tree, item, "Bounds min", mesh->boundsMin);
    ZoneObjectTreeView::AddZoneTreeVec3Field(tree, item, "Bounds max", mesh->boundsMax);
}
}

namespace ZoneObjectTreePopulation
{
void PopulateExpandedNode(const HWND tree, const HTREEITEM item, const LPARAM param,
                          const std::vector<std::string>& hiddenObjectNames)
{
    if (!tree || !item || !ZoneObjectTreeView::ZoneTreeNodeHasPlaceholder(tree, item))
        return;

    ZoneObjectTreeView::ZoneTreeDeleteChildren(tree, item);
    const int type = ZoneObjectTreeView::GetZoneTreeParamType(param);
    const int index = ZoneObjectTreeView::GetZoneTreeParamIndex(param);
    switch (type)
    {
    case ZoneObjectTreeView::kZoneTreeNode_MapObject:
        PopulateMapObjectFields(tree, item, index, hiddenObjectNames);
        break;
    case ZoneObjectTreeView::kZoneTreeNode_DrawBatch:
        PopulateDrawBatchFields(tree, item, index);
        break;
    case ZoneObjectTreeView::kZoneTreeNode_CollisionMesh:
        PopulateCollisionFields(tree, item, index);
        break;
    default:
        break;
    }
}
}
