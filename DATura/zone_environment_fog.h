#pragma once

#include <d3d9.h>

#include "zone_environment_state.h"

namespace ZoneEnvironmentFog
{
void Apply(IDirect3DDevice9 *device, const ZoneEnvironmentState::Data &environment);
}
