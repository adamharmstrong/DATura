#include "stdafx.h"
#include "zone_object_tree_builder.h"

#include "noesis_rapi.h"
#include "model_ff11.h"
#include "zone_object_tree_view.h"

#include <commctrl.h>

namespace ZoneObjectTreeBuilder
{
void Build(const Context& context)
{
    if (!context.dataTree || !context.rawDataTree || !context.collisionDataTree)
        return;

    SendMessageA(context.dataTree, WM_SETREDRAW, FALSE, 0);
    SendMessageA(context.rawDataTree, WM_SETREDRAW, FALSE, 0);
    SendMessageA(context.collisionDataTree, WM_SETREDRAW, FALSE, 0);
    TreeView_DeleteAllItems(context.dataTree);
    TreeView_DeleteAllItems(context.rawDataTree);
    TreeView_DeleteAllItems(context.collisionDataTree);

    char text[256] = {};
    const HWND placedTree = context.dataTree;
    const HWND rawTree = context.combinedObjectTree ? context.dataTree : context.rawDataTree;
    const HWND collisionTree = context.combinedObjectTree ? context.dataTree : context.collisionDataTree;
    HTREEITEM combinedRoot = NULL;
    if (context.combinedObjectTree)
    {
        combinedRoot = ZoneObjectTreeView::AddZoneTreeItem(context.dataTree, NULL, context.loadedZoneLabel);
        sprintf_s(text, "Counts: map records %d, MapGeo draw batches %d, collision grid entries %d",
                  ListView_GetItemCount(context.placedObjectList),
                  Model_FF11_GetLastMapGeoDrawBatchCount(),
                  Model_FF11_GetLastCollisionMeshCount());
        ZoneObjectTreeView::AddZoneTreeItem(context.dataTree, combinedRoot, text);
    }

    const HTREEITEM placedRoot = ZoneObjectTreeView::AddZoneTreeItem(placedTree, combinedRoot, "Placed Geometry");
    sprintf_s(text, "Map records: %d", ListView_GetItemCount(context.placedObjectList));
    ZoneObjectTreeView::AddZoneTreeItem(placedTree, placedRoot, text);
    const HTREEITEM mapRoot = ZoneObjectTreeView::AddZoneTreeItem(placedTree, placedRoot, "Map chunk 0x1C");
    if (gFF11LastMapHeader.valid)
    {
        const HTREEITEM mapHeaderRoot = ZoneObjectTreeView::AddZoneTreeItem(placedTree, mapRoot, "Map header metadata");
        ZoneObjectTreeView::AddZoneTreeBytesField(placedTree, mapHeaderRoot, "Header bytes", gFF11LastMapHeader.headerData, 4);
        ZoneObjectTreeView::AddZoneTreeHexField(placedTree, mapHeaderRoot, "Object count field (24-bit)", gFF11LastMapHeader.objectCount24);
        ZoneObjectTreeView::AddZoneTreeHexField(placedTree, mapHeaderRoot, "Unknown1 (upper 8 bits after object count)", gFF11LastMapHeader.unknown1);
        for (int index = 0; index < 6; ++index)
        {
            char label[64] = {};
            sprintf_s(label, "Unknown2[%d]", index);
            ZoneObjectTreeView::AddZoneTreeHexField(placedTree, mapHeaderRoot, label, gFF11LastMapHeader.unknown2[index]);
        }
        ZoneObjectTreeView::AddZoneTreeHexField(placedTree, mapHeaderRoot, "Object table offset", (unsigned int)gFF11LastMapHeader.objectTableOffset);
        ZoneObjectTreeView::AddZoneTreeHexField(placedTree, mapHeaderRoot, "Object table end offset", (unsigned int)gFF11LastMapHeader.objectTableEndOffset);
        ZoneObjectTreeView::AddZoneTreeIntField(placedTree, mapHeaderRoot, "Parsed object count", gFF11LastMapHeader.parsedObjectCount);
        ZoneObjectTreeView::AddZoneTreeHexField(placedTree, mapHeaderRoot, "Trailing data offset", (unsigned int)gFF11LastMapHeader.trailingDataOffset);
        ZoneObjectTreeView::AddZoneTreeIntField(placedTree, mapHeaderRoot,
                                                "Trailing data size (unknown visibility/partitioning data)",
                                                gFF11LastMapHeader.trailingDataSize);
    }
    const HTREEITEM placementRoot = ZoneObjectTreeView::AddZoneTreeItem(placedTree, mapRoot, "Placement records");

    const HTREEITEM chunkRoot = ZoneObjectTreeView::AddZoneTreeItem(rawTree, combinedRoot, "DAT Chunk Table");
    sprintf_s(text, "Parsed chunks: %d", (int)gFF11LastDatChunks.size());
    ZoneObjectTreeView::AddZoneTreeItem(rawTree, chunkRoot, text);
    for (int index = 0; index < (int)gFF11LastDatChunks.size(); ++index)
    {
        const ff11DatChunkDebug_t& chunk = gFF11LastDatChunks[index];
        sprintf_s(text, "%03d: %s type 0x%02X %s", index, chunk.name, chunk.type,
                  chunk.supported ? "handled" : "unhandled");
        const HTREEITEM chunkItem = ZoneObjectTreeView::AddZoneTreeItem(rawTree, chunkRoot, text);
        ZoneObjectTreeView::AddZoneTreeField(rawTree, chunkItem, "Chunk name/tag", chunk.name);
        ZoneObjectTreeView::AddZoneTreeHexField(rawTree, chunkItem, "Chunk type", (unsigned int)chunk.type);
        const HTREEITEM chunkFlagsRoot = ZoneObjectTreeView::AddZoneTreeItem(
            rawTree, chunkItem, "Common resource-header flags");
        ZoneObjectTreeView::AddZoneTreeField(rawTree, chunkFlagsRoot, "Shadow resource", chunk.isShadow ? "1" : "0");
        ZoneObjectTreeView::AddZoneTreeField(rawTree, chunkFlagsRoot, "Extracted resource", chunk.isExtracted ? "1" : "0");
        ZoneObjectTreeView::AddZoneTreeIntField(rawTree, chunkFlagsRoot, "Resource version", chunk.version);
        ZoneObjectTreeView::AddZoneTreeField(rawTree, chunkFlagsRoot, "Virtual resource", chunk.isVirtual ? "1" : "0");
        if (chunk.directoryPath[0])
            ZoneObjectTreeView::AddZoneTreeField(rawTree, chunkItem, "Directory path", chunk.directoryPath);
        if (chunk.isDirectoryOpen)
            ZoneObjectTreeView::AddZoneTreeField(rawTree, chunkItem, "Directory command", "open");
        if (chunk.isDirectoryClose)
            ZoneObjectTreeView::AddZoneTreeField(rawTree, chunkItem, "Directory command", "close");
        ZoneObjectTreeView::AddZoneTreeHexField(rawTree, chunkItem, "Data offset", (unsigned int)chunk.dataOffset);
        ZoneObjectTreeView::AddZoneTreeHexField(rawTree, chunkItem, "Total chunk size", (unsigned int)chunk.size);
        ZoneObjectTreeView::AddZoneTreeField(rawTree, chunkItem, "Handled by DATura", chunk.supported ? "1" : "0");
        if (chunk.hasEnvironmentMetadata)
        {
            const HTREEITEM environmentRoot = ZoneObjectTreeView::AddZoneTreeItem(
                rawTree, chunkItem, "Environment/light record 0x2F");
            for (int wordIndex = 0; wordIndex < 8; ++wordIndex)
            {
                char label[64] = {};
                sprintf_s(label, "Header word %d", wordIndex);
                ZoneObjectTreeView::AddZoneTreeHex16Field(rawTree, environmentRoot, label,
                                                          chunk.environmentHeaderWords[wordIndex]);
            }
        }
        if (chunk.hasSoundPointer)
        {
            const HTREEITEM soundRoot = ZoneObjectTreeView::AddZoneTreeItem(rawTree, chunkItem, "Sound pointer 0x3D");
            ZoneObjectTreeView::AddZoneTreeIntField(rawTree, soundRoot, "SPW sound id", (int)chunk.soundId);
            ZoneObjectTreeView::AddZoneTreeField(rawTree, soundRoot, "SPW path", chunk.soundPath);
        }
        if (chunk.hasEffectMetadata)
        {
            const char* effectKind = chunk.type == 0x1F ? "Effect model 0x1F" :
                                     (chunk.type == 0x21 ? "Animated effect model 0x21" : "Morph effect model 0x25");
            const HTREEITEM effectRoot = ZoneObjectTreeView::AddZoneTreeItem(rawTree, chunkItem, effectKind);
            ZoneObjectTreeView::AddZoneTreeField(rawTree, effectRoot, "Material/image tag", chunk.effectMaterialName);
            ZoneObjectTreeView::AddZoneTreeHex16Field(rawTree, chunkItem, "Effect header word 0", chunk.effectHeaderWords[0]);
            ZoneObjectTreeView::AddZoneTreeHex16Field(rawTree, chunkItem, "Effect header word 1", chunk.effectHeaderWords[1]);
            ZoneObjectTreeView::AddZoneTreeHex16Field(rawTree, chunkItem, "Effect header word 2", chunk.effectHeaderWords[2]);
            ZoneObjectTreeView::AddZoneTreeHex16Field(rawTree, chunkItem, "Effect header word 3", chunk.effectHeaderWords[3]);
            ZoneObjectTreeView::AddZoneTreeHex16Field(rawTree, chunkItem, "Effect header word 4", chunk.effectHeaderWords[4]);
            ZoneObjectTreeView::AddZoneTreeHex16Field(rawTree, chunkItem, "Effect header word 5", chunk.effectHeaderWords[5]);
            ZoneObjectTreeView::AddZoneTreeHex16Field(rawTree, chunkItem, "Effect header word 6", chunk.effectHeaderWords[6]);
            ZoneObjectTreeView::AddZoneTreeHex16Field(rawTree, chunkItem, "Effect header word 7", chunk.effectHeaderWords[7]);
            if (chunk.type == 0x25)
            {
                ZoneObjectTreeView::AddZoneTreeIntField(rawTree, effectRoot, "Image count", chunk.effectHeaderWords[0]);
                ZoneObjectTreeView::AddZoneTreeIntField(rawTree, effectRoot, "Morph count", chunk.effectHeaderWords[1]);
                ZoneObjectTreeView::AddZoneTreeIntField(rawTree, effectRoot, "Base position count", chunk.effectHeaderWords[2]);
                ZoneObjectTreeView::AddZoneTreeIntField(rawTree, effectRoot, "Secondary position count", chunk.effectHeaderWords[3]);
                ZoneObjectTreeView::AddZoneTreeHexField(rawTree, effectRoot, "Index block offset", chunk.effectHeaderWords[4]);
                ZoneObjectTreeView::AddZoneTreeIntField(rawTree, effectRoot, "Triangle count", chunk.effectHeaderWords[5]);
                ZoneObjectTreeView::AddZoneTreeHexField(rawTree, effectRoot, "Per-corner color offset", chunk.effectHeaderWords[6]);
                ZoneObjectTreeView::AddZoneTreeHexField(rawTree, effectRoot, "Per-corner UV offset", chunk.effectHeaderWords[7]);
            }
        }
        if (chunk.hasGeneratorMetadata)
        {
            const HTREEITEM generatorRoot = ZoneObjectTreeView::AddZoneTreeItem(rawTree, chunkItem, "Generator command stream 0x05");
            ZoneObjectTreeView::AddZoneTreeHex16Field(rawTree, generatorRoot, "Attach flags", chunk.generatorAttachFlags);
            ZoneObjectTreeView::AddZoneTreeIntField(rawTree, generatorRoot, "Emission variance", chunk.generatorEmissionVariance);
            ZoneObjectTreeView::AddZoneTreeIntField(rawTree, generatorRoot,
                                                    "Frames per emission (+1 at runtime)", chunk.generatorFramesPerEmission);
            ZoneObjectTreeView::AddZoneTreeIntField(rawTree, generatorRoot, "Particles per emission", chunk.generatorParticlesPerEmission);
            ZoneObjectTreeView::AddZoneTreeHexField(rawTree, generatorRoot, "Generator flags", chunk.generatorFlags);
            ZoneObjectTreeView::AddZoneTreeHexField(rawTree, generatorRoot, "More flags", chunk.generatorMoreFlags);
            if (chunk.generatorEnvironmentId[0])
                ZoneObjectTreeView::AddZoneTreeField(rawTree, generatorRoot, "Environment id", chunk.generatorEnvironmentId);
            if (chunk.generatorLinkedResource[0])
            {
                ZoneObjectTreeView::AddZoneTreeField(rawTree, generatorRoot, "Linked render resource", chunk.generatorLinkedResource);
                ZoneObjectTreeView::AddZoneTreeHexField(rawTree, generatorRoot, "Linked data type", chunk.generatorLinkedDataType);
            }
            for (int sectionIndex = 0; sectionIndex < 4; ++sectionIndex)
            {
                char label[64] = {};
                sprintf_s(label, "Section %d range", sectionIndex + 1);
                char rangeText[64] = {};
                const unsigned int sectionEnd = (sectionIndex + 1 < 4) ?
                    chunk.generatorSectionOffsets[sectionIndex + 1] : (unsigned int)chunk.size;
                sprintf_s(rangeText, "0x%X..0x%X", chunk.generatorSectionOffsets[sectionIndex], sectionEnd);
                ZoneObjectTreeView::AddZoneTreeField(rawTree, generatorRoot, label, rangeText);
            }
        }
        if (chunk.hasKeyframeMetadata)
        {
            const HTREEITEM keyframeRoot = ZoneObjectTreeView::AddZoneTreeItem(rawTree, chunkItem, "Keyframe pairs 0x19");
            ZoneObjectTreeView::AddZoneTreeIntField(rawTree, keyframeRoot, "Float-pair count", chunk.keyframePairCount);
        }
    }

    const HTREEITEM rawRoot = ZoneObjectTreeView::AddZoneTreeItem(rawTree, combinedRoot, "Unreferenced Geometry");
    sprintf_s(text, "Unreferenced MapGeo: %d", ListView_GetItemCount(context.unreferencedObjectList));
    ZoneObjectTreeView::AddZoneTreeItem(rawTree, rawRoot, text);
    const HTREEITEM unreferencedRoot = ZoneObjectTreeView::AddZoneTreeItem(
        rawTree, rawRoot, "MapGeo chunks 0x2E not referenced by placement records");

    for (int index = 0; index < Model_FF11_GetLastMapObjectCount(); ++index)
    {
        const ff11MapObjectDebug_t& object = gFF11LastMapObjects[index];
        const HTREEITEM parent = object.referencedByMap ? placementRoot : unreferencedRoot;
        if (object.referencedByMap)
            sprintf_s(text, "Record %03d - %s", object.mapRecordIndex, object.objectName);
        else
            sprintf_s(text, "MapGeo chunk %03d - %s", object.mapGeoIndex, object.objectName);
        const HWND tree = object.referencedByMap ? placedTree : rawTree;
        const HTREEITEM item = ZoneObjectTreeView::AddZoneTreeItem(
            tree, parent, text,
            ZoneObjectTreeView::MakeZoneTreeParam(ZoneObjectTreeView::kZoneTreeNode_MapObject, index));
        ZoneObjectTreeView::AddZoneTreePlaceholder(tree, item);
    }

    const HTREEITEM collisionRoot = ZoneObjectTreeView::AddZoneTreeItem(collisionTree, combinedRoot, "Collision Meshes");
    sprintf_s(text, "Collision grid entries: %d", Model_FF11_GetLastCollisionMeshCount());
    ZoneObjectTreeView::AddZoneTreeItem(collisionTree, collisionRoot, text);
    const HTREEITEM collisionGridRoot = ZoneObjectTreeView::AddZoneTreeItem(collisionTree, collisionRoot, "Collision grid");
    for (int index = 0; index < Model_FF11_GetLastCollisionMeshCount(); ++index)
    {
        const ff11CollisionMeshDebug_t* mesh = Model_FF11_GetLastCollisionMesh(index);
        if (!mesh)
            continue;

        const HTREEITEM item = ZoneObjectTreeView::AddZoneTreeItem(
            collisionTree, collisionGridRoot, mesh->displayName,
            ZoneObjectTreeView::MakeZoneTreeParam(ZoneObjectTreeView::kZoneTreeNode_CollisionMesh, index));
        ZoneObjectTreeView::AddZoneTreePlaceholder(collisionTree, item);
    }

    if (combinedRoot)
        TreeView_Expand(context.dataTree, combinedRoot, TVE_EXPAND);
    TreeView_Expand(placedTree, placedRoot, TVE_EXPAND);
    TreeView_Expand(placedTree, mapRoot, TVE_EXPAND);
    TreeView_Expand(placedTree, placementRoot, TVE_EXPAND);
    TreeView_Expand(rawTree, chunkRoot, TVE_EXPAND);
    TreeView_Expand(rawTree, rawRoot, TVE_EXPAND);
    TreeView_Expand(rawTree, unreferencedRoot, TVE_EXPAND);
    TreeView_Expand(collisionTree, collisionRoot, TVE_EXPAND);
    TreeView_Expand(collisionTree, collisionGridRoot, TVE_EXPAND);

    SendMessageA(context.dataTree, WM_SETREDRAW, TRUE, 0);
    SendMessageA(context.rawDataTree, WM_SETREDRAW, TRUE, 0);
    SendMessageA(context.collisionDataTree, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(context.dataTree, NULL, TRUE);
    InvalidateRect(context.rawDataTree, NULL, TRUE);
    InvalidateRect(context.collisionDataTree, NULL, TRUE);
}
}
