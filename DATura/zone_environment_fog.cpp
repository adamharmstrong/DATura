#include "stdafx.h"
#include "zone_environment_fog.h"

#include "d3d_model_render_state.h"

namespace ZoneEnvironmentFog
{
void Apply(IDirect3DDevice9 *device, const ZoneEnvironmentState::Data &environment)
{
    if (!device)
        return;

    const bool enabled = environment.valid && environment.fogFar > 0.0f &&
                         environment.fogFar > environment.fogNear;
    device->SetRenderState(D3DRS_FOGENABLE, enabled ? TRUE : FALSE);
    if (!enabled)
        return;
    device->SetRenderState(D3DRS_FOGCOLOR, environment.fogColor);
    device->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_NONE);
    device->SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR);
    device->SetRenderState(D3DRS_FOGSTART, D3DModelRenderState::FloatBits(environment.fogNear));
    device->SetRenderState(D3DRS_FOGEND, D3DModelRenderState::FloatBits(environment.fogFar));
}
}
