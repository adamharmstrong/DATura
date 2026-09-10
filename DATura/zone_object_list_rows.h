#pragma once

#include <windows.h>

#include <map>
#include <string>
#include <vector>

#include "zone_object_transform.h"

namespace ZoneObjectListRows
{
void InsertMapObjectRow(HWND list, int mapObjectIndex,
                        const std::map<std::string, ZoneObjectTransform::DebugTransform>& overrides,
                        const std::vector<std::string>& hiddenObjectNames);
void InsertCollisionRow(HWND list, int collisionMeshIndex);
void InsertDrawBatchRow(HWND list, int batchIndex);
}
