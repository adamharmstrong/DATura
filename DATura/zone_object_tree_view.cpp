#include "stdafx.h"
#include "zone_object_tree_view.h"

#include <commctrl.h>
#include <cstring>

namespace ZoneObjectTreeView
{
LPARAM MakeZoneTreeParam(const int type, const int index)
{
    return ((LPARAM)type << 24) | (LPARAM)(index + 1);
}

int GetZoneTreeParamType(const LPARAM param)
{
    return (int)((param >> 24) & 0xFF);
}

int GetZoneTreeParamIndex(const LPARAM param)
{
    return (int)(param & 0x00FFFFFF) - 1;
}

HTREEITEM AddZoneTreeItem(const HWND tree, const HTREEITEM parent, const char* text,
                          const LPARAM param)
{
    if (!tree || !text)
        return NULL;

    TVINSERTSTRUCTA insert = {};
    insert.hParent = parent ? parent : TVI_ROOT;
    insert.hInsertAfter = TVI_LAST;
    insert.item.mask = TVIF_TEXT | TVIF_PARAM;
    insert.item.pszText = const_cast<char*>(text);
    insert.item.lParam = param;
    return (HTREEITEM)SendMessageA(tree, TVM_INSERTITEMA, 0, (LPARAM)&insert);
}

void AddZoneTreePlaceholder(const HWND tree, const HTREEITEM parent)
{
    AddZoneTreeItem(tree, parent, "...");
}

bool ZoneTreeNodeHasPlaceholder(const HWND tree, const HTREEITEM item)
{
    const HTREEITEM child = TreeView_GetChild(tree, item);
    if (!child)
        return false;

    char text[16] = {};
    TVITEMA treeItem = {};
    treeItem.mask = TVIF_TEXT;
    treeItem.hItem = child;
    treeItem.pszText = text;
    treeItem.cchTextMax = sizeof(text);
    if (!SendMessageA(tree, TVM_GETITEMA, 0, (LPARAM)&treeItem))
        return false;
    return std::strcmp(text, "...") == 0;
}

void ZoneTreeDeleteChildren(const HWND tree, const HTREEITEM item)
{
    HTREEITEM child = TreeView_GetChild(tree, item);
    while (child)
    {
        TreeView_DeleteItem(tree, child);
        child = TreeView_GetChild(tree, item);
    }
}

void AddZoneTreeField(const HWND tree, const HTREEITEM parent, const char* name, const char* value)
{
    char text[512] = {};
    sprintf_s(text, "%s: %s", name ? name : "", value ? value : "");
    AddZoneTreeItem(tree, parent, text);
}

void AddZoneTreeIntField(const HWND tree, const HTREEITEM parent, const char* name, const int value)
{
    char valueText[64] = {};
    sprintf_s(valueText, "%d", value);
    AddZoneTreeField(tree, parent, name, valueText);
}

void AddZoneTreeHexField(const HWND tree, const HTREEITEM parent, const char* name,
                         const unsigned int value)
{
    char valueText[64] = {};
    sprintf_s(valueText, "0x%08X", value);
    AddZoneTreeField(tree, parent, name, valueText);
}

void AddZoneTreeHex16Field(const HWND tree, const HTREEITEM parent, const char* name,
                           const unsigned int value)
{
    char valueText[64] = {};
    sprintf_s(valueText, "0x%04X", value & 0xFFFF);
    AddZoneTreeField(tree, parent, name, valueText);
}

void AddZoneTreeBytesField(const HWND tree, const HTREEITEM parent, const char* name,
                           const unsigned char* bytes, const int count)
{
    char valueText[128] = {};
    char* destination = valueText;
    size_t remaining = sizeof(valueText);
    for (int index = 0; index < count && remaining > 1; ++index)
    {
        const int written = sprintf_s(destination, remaining, "%s%02X",
                                      index ? " " : "", bytes[index]);
        if (written <= 0)
            break;
        destination += written;
        remaining -= written;
    }
    AddZoneTreeField(tree, parent, name, valueText);
}

void AddZoneTreeVec3Field(const HWND tree, const HTREEITEM parent, const char* name,
                          const float value[3])
{
    char valueText[128] = {};
    sprintf_s(valueText, "%.6f, %.6f, %.6f", value[0], value[1], value[2]);
    AddZoneTreeField(tree, parent, name, valueText);
}

void AddZoneTreeVec4Field(const HWND tree, const HTREEITEM parent, const char* name,
                          const float value[4])
{
    char valueText[160] = {};
    sprintf_s(valueText, "%.6f, %.6f, %.6f, %.6f", value[0], value[1], value[2], value[3]);
    AddZoneTreeField(tree, parent, name, valueText);
}

void AddZoneTreeBoundsField(const HWND tree, const HTREEITEM parent, const char* name,
                            const float bounds[6])
{
    char valueText[256] = {};
    sprintf_s(valueText, "min %.6f, %.6f, %.6f / max %.6f, %.6f, %.6f",
              bounds[0], bounds[1], bounds[2], bounds[3], bounds[4], bounds[5]);
    AddZoneTreeField(tree, parent, name, valueText);
}
}
