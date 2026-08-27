#pragma once

#include <windows.h>
#include <commctrl.h>

namespace ZoneObjectTreeView
{
enum ZoneTreeNodeType
{
    kZoneTreeNode_None = 0,
    kZoneTreeNode_MapObject = 1,
    kZoneTreeNode_DrawBatch = 2,
    kZoneTreeNode_CollisionMesh = 3,
};

LPARAM MakeZoneTreeParam(int type, int index);
int GetZoneTreeParamType(LPARAM param);
int GetZoneTreeParamIndex(LPARAM param);

HTREEITEM AddZoneTreeItem(HWND tree, HTREEITEM parent, const char* text, LPARAM param = 0);
void AddZoneTreePlaceholder(HWND tree, HTREEITEM parent);
bool ZoneTreeNodeHasPlaceholder(HWND tree, HTREEITEM item);
void ZoneTreeDeleteChildren(HWND tree, HTREEITEM item);

void AddZoneTreeField(HWND tree, HTREEITEM parent, const char* name, const char* value);
void AddZoneTreeIntField(HWND tree, HTREEITEM parent, const char* name, int value);
void AddZoneTreeHexField(HWND tree, HTREEITEM parent, const char* name, unsigned int value);
void AddZoneTreeHex16Field(HWND tree, HTREEITEM parent, const char* name, unsigned int value);
void AddZoneTreeBytesField(HWND tree, HTREEITEM parent, const char* name,
                           const unsigned char* bytes, int count);
void AddZoneTreeVec3Field(HWND tree, HTREEITEM parent, const char* name, const float value[3]);
void AddZoneTreeVec4Field(HWND tree, HTREEITEM parent, const char* name, const float value[4]);
void AddZoneTreeBoundsField(HWND tree, HTREEITEM parent, const char* name, const float bounds[6]);
}
