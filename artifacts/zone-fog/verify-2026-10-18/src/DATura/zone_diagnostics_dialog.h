#pragma once
#include <windows.h>
#include "zone_scene_diagnostics.h"

namespace ZoneDiagnosticsDialog
{
// Takes ownership of a static scene snapshot. Reopen from the menu to recapture.
void Show(HWND owner, ZoneSceneDiagnostics::Snapshot snapshot);
}
