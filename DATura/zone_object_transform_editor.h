#pragma once

#include <windows.h>

#include <map>
#include <string>

#include "zone_object_transform.h"

namespace ZoneObjectTransformEditor
{
void PopulateMapObjectFields(HWND* edits, int editCount, int mapObjectIndex,
                             const std::map<std::string, ZoneObjectTransform::DebugTransform>& overrides);
ZoneObjectTransform::DebugTransform ReadFields(const HWND* edits, int editCount);
}
