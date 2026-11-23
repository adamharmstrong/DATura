#pragma once

#include <windows.h>

namespace Win32ComboBox
{
    void ComboAddString(HWND comboBox, const char* text);
    void ComboAddStringWithData(HWND comboBox, const char* text, int data);
    void ComboSetIndex(HWND comboBox, int index);
    void ComboSetIndexByData(HWND comboBox, int data);
    int ComboGetIndex(HWND comboBox);
    int ComboGetSelectedData(HWND comboBox);
}
