#pragma once

#include <windows.h>

namespace FFXISaveResultDialog
{
    // Shows a modal confirmation window. positioningOwner is used for text
    // measurement and centering when the dialog has no modal owner.
    void Show(HWND owner, HWND positioningOwner, const char* noesisPath, const char* datSetPath);
}
