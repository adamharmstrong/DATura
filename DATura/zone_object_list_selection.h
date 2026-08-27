#pragma once

#include <windows.h>

namespace ZoneObjectListSelection
{
HWND GetListForControlId(UINT controlId, UINT unreferencedListId, UINT collisionListId,
                         HWND placedList, HWND unreferencedList, HWND collisionList);
HWND GetActiveMapObjectList(HWND placedList, HWND unreferencedList);
int GetMapObjectIndex(HWND list, int row);
int GetSelectedMapObjectIndex(HWND placedList, HWND unreferencedList, int treeSelectedMapObjectIndex);
void ClearOtherMapObjectListSelection(HWND selectedList, HWND placedList, HWND unreferencedList);
void SetCheckStateForMapObjectIndex(HWND placedList, HWND unreferencedList,
                                    int mapObjectIndex, BOOL checked);
}
