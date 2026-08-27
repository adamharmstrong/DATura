#include "stdafx.h"
#include "zone_object_list_selection.h"

#include <commctrl.h>

namespace ZoneObjectListSelection
{
HWND GetListForControlId(const UINT controlId, const UINT unreferencedListId,
                         const UINT collisionListId, const HWND placedList,
                         const HWND unreferencedList, const HWND collisionList)
{
    if (controlId == unreferencedListId)
        return unreferencedList;
    if (controlId == collisionListId)
        return collisionList;
    return placedList;
}

HWND GetActiveMapObjectList(const HWND placedList, const HWND unreferencedList)
{
    if (placedList && ListView_GetNextItem(placedList, -1, LVNI_SELECTED) >= 0)
        return placedList;
    if (unreferencedList && ListView_GetNextItem(unreferencedList, -1, LVNI_SELECTED) >= 0)
        return unreferencedList;
    return placedList ? placedList : unreferencedList;
}

int GetMapObjectIndex(const HWND list, const int row)
{
    if (!list)
        return -1;
    LVITEMA item = {};
    item.mask = LVIF_PARAM;
    item.iItem = row;
    if (!SendMessageA(list, LVM_GETITEMA, 0, (LPARAM)&item))
        return -1;
    return (int)item.lParam;
}

int GetSelectedMapObjectIndex(const HWND placedList, const HWND unreferencedList,
                              const int treeSelectedMapObjectIndex)
{
    if (treeSelectedMapObjectIndex >= 0)
        return treeSelectedMapObjectIndex;

    const HWND list = GetActiveMapObjectList(placedList, unreferencedList);
    if (!list)
        return -1;
    const int row = ListView_GetNextItem(list, -1, LVNI_SELECTED);
    return (row >= 0) ? GetMapObjectIndex(list, row) : -1;
}

void ClearOtherMapObjectListSelection(const HWND selectedList, const HWND placedList,
                                      const HWND unreferencedList)
{
    const HWND otherList = (selectedList == placedList) ? unreferencedList : placedList;
    if (!otherList)
        return;

    const int selectedRow = ListView_GetNextItem(otherList, -1, LVNI_SELECTED);
    if (selectedRow >= 0)
        ListView_SetItemState(otherList, selectedRow, 0, LVIS_SELECTED | LVIS_FOCUSED);
}

void SetCheckStateForMapObjectIndex(const HWND placedList, const HWND unreferencedList,
                                    const int mapObjectIndex, const BOOL checked)
{
    const HWND lists[2] = { placedList, unreferencedList };
    for (int listIndex = 0; listIndex < 2; ++listIndex)
    {
        const HWND list = lists[listIndex];
        if (!list)
            continue;
        const int rowCount = ListView_GetItemCount(list);
        for (int row = 0; row < rowCount; ++row)
        {
            if (GetMapObjectIndex(list, row) == mapObjectIndex)
            {
                ListView_SetCheckState(list, row, checked);
                return;
            }
        }
    }
}
}
