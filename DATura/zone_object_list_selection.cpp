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

std::vector<int> GetSelectedMapObjectIndices(
    const HWND placedList, const HWND unreferencedList)
{
    std::vector<int> indices;
    const HWND lists[] = { placedList, unreferencedList };
    for (const HWND list : lists)
    {
        if (!list)
            continue;
        for (int row = ListView_GetNextItem(list, -1, LVNI_SELECTED);
             row >= 0;
             row = ListView_GetNextItem(list, row, LVNI_SELECTED))
        {
            const int index = GetMapObjectIndex(list, row);
            if (index >= 0)
                indices.push_back(index);
        }
    }
    return indices;
}

void ClearOtherMapObjectListSelection(const HWND selectedList, const HWND placedList,
                                      const HWND unreferencedList)
{
    const HWND otherList = (selectedList == placedList) ? unreferencedList : placedList;
    if (!otherList)
        return;

    ListView_SetItemState(otherList, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
}

bool SelectMapObjectIndex(const HWND placedList, const HWND unreferencedList,
                          const int mapObjectIndex, const bool additive)
{
    const HWND lists[2] = { placedList, unreferencedList };
    for (const HWND list : lists)
    {
        if (!list)
            continue;
        const int rowCount = ListView_GetItemCount(list);
        for (int row = 0; row < rowCount; ++row)
        {
            if (GetMapObjectIndex(list, row) != mapObjectIndex)
                continue;
            if (!additive)
            {
                ClearOtherMapObjectListSelection(list, placedList, unreferencedList);
                ListView_SetItemState(list, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
            }
            ListView_SetItemState(list, row, LVIS_SELECTED | LVIS_FOCUSED,
                                  LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(list, row, FALSE);
            return true;
        }
    }
    return false;
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
