#include "stdafx.h"
#include "zone_object_panel_edit_controls.h"

namespace ZoneObjectPanelEditControls
{
void SetUnreferencedEditorVisible(const HWND* labels, const HWND* edits, const int fieldCount,
                                  const HWND applyButton, const bool visible)
{
    const int command = visible ? SW_SHOW : SW_HIDE;
    for (int index = 0; index < fieldCount; ++index)
    {
        if (labels && labels[index])
            ShowWindow(labels[index], command);
        if (edits && edits[index])
            ShowWindow(edits[index], command);
    }
    if (applyButton)
        ShowWindow(applyButton, command);
}

void SetControlsEnabled(const HWND* controls, const int controlCount, const bool enabled)
{
    if (!controls)
        return;
    for (int index = 0; index < controlCount; ++index)
    {
        if (controls[index])
            EnableWindow(controls[index], enabled ? TRUE : FALSE);
    }
}
}
