#include "stdafx.h"
#include "win32_combo_box.h"

namespace Win32ComboBox
{
void ComboAddString(HWND comboBox, const char* text)
{
    SendMessageA(comboBox, CB_ADDSTRING, 0, (LPARAM)text);
}

void ComboAddStringWithData(HWND comboBox, const char* text, int data)
{
    const LRESULT index = SendMessageA(comboBox, CB_ADDSTRING, 0, (LPARAM)text);
    if (index != CB_ERR)
        SendMessageA(comboBox, CB_SETITEMDATA, (WPARAM)index, (LPARAM)data);
}

void ComboSetIndex(HWND comboBox, int index)
{
    SendMessageA(comboBox, CB_SETCURSEL, (WPARAM)index, 0);
}

void ComboSetIndexByData(HWND comboBox, int data)
{
    const int count = (int)SendMessageA(comboBox, CB_GETCOUNT, 0, 0);
    for (int index = 0; index < count; ++index)
    {
        if ((int)SendMessageA(comboBox, CB_GETITEMDATA, (WPARAM)index, 0) == data)
        {
            ComboSetIndex(comboBox, index);
            return;
        }
    }
    ComboSetIndex(comboBox, 0);
}

int ComboGetIndex(HWND comboBox)
{
    const LRESULT index = SendMessageA(comboBox, CB_GETCURSEL, 0, 0);
    return index == CB_ERR ? 0 : (int)index;
}

int ComboGetSelectedData(HWND comboBox)
{
    const int index = ComboGetIndex(comboBox);
    const LRESULT data = SendMessageA(comboBox, CB_GETITEMDATA, (WPARAM)index, 0);
    return data == CB_ERR ? index : (int)data;
}
}
