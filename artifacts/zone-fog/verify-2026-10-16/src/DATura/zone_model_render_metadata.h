#pragma once

#include "noesis_rapi.h"

namespace ZoneModelRenderMetadata
{
bool IsAnimatedWaterSurface(const noesisModel_t::Submesh &submesh);
void Prepare(noesisModel_t *model, IDirect3DDevice9 *device);
}
