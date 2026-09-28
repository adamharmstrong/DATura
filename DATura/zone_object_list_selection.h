#pragma once

#include <windows.h>
#include <vector>

namespace ZoneObjectListSelection
{
HWND GetListForControlId(UINT controlId, UINT unreferencedListId, UINT collisionListId,
                         HWND placedList, HWND unreferencedList, HWND collisionList);
HWND GetActiveMapObjectList(HWND placedList, HWND unreferencedList);
int GetMapObjectIndex(HWND list, int row);
int GetSelectedMapObjectIndex(HWND placedList, HWND unreferencedList, int treeSelectedMapObjectIndex);
std::vector<int> GetSelectedMapObjectIndices(HWND placedList, HWND unreferencedList);
void ClearOtherMapObjectListSelection(HWND selectedList, HWND placedList, HWND unreferencedList);
bool SelectMapObjectIndex(HWND placedList, HWND unreferencedList,
                          int mapObjectIndex, bool additive = false);
void SetCheckStateForMapObjectIndex(HWND placedList, HWND unreferencedList,
                                    int mapObjectIndex, BOOL checked);
}
