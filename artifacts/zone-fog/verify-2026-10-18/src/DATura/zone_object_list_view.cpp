#include "stdafx.h"
#include "zone_object_list_view.h"

#include <commctrl.h>

namespace ZoneObjectListView
{
void ClearColumns(const HWND list)
{
    if (!list)
        return;
    while (Header_GetItemCount(ListView_GetHeader(list)) > 0)
        ListView_DeleteColumn(list, 0);
}

void AddColumn(const HWND list, const int index, const char* text, const int width)
{
    if (!list)
        return;
    LVCOLUMNA column = {};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    column.pszText = const_cast<char*>(text);
    column.cx = width;
    column.iSubItem = index;
    SendMessageA(list, LVM_INSERTCOLUMNA, (WPARAM)index, (LPARAM)&column);
}

void SetSubItem(const HWND list, const int row, const int column, const char* text)
{
    if (!list)
        return;
    LVITEMA item = {};
    item.mask = LVIF_TEXT;
    item.iItem = row;
    item.iSubItem = column;
    item.pszText = const_cast<char*>(text);
    SendMessageA(list, LVM_SETITEMA, 0, (LPARAM)&item);
}
}
