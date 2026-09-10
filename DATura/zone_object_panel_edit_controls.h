#pragma once

#include <windows.h>

namespace ZoneObjectPanelEditControls
{
void SetUnreferencedEditorVisible(const HWND* labels, const HWND* edits, int fieldCount,
                                  HWND applyButton, bool visible);
void SetControlsEnabled(const HWND* controls, int controlCount, bool enabled);
}
