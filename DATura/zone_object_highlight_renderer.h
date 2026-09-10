#pragma once

#include <map>
#include <string>

#include "zone_object_transform.h"

struct IDirect3DDevice9;
struct noesisModel_t;

namespace ZoneObjectHighlightRenderer
{
void Draw(IDirect3DDevice9 *device, noesisModel_t *model, const std::string& highlightedObject,
          const std::map<std::string, ZoneObjectTransform::DebugTransform>& overrides);
}
