#include "stdafx.h"
#include "d3d9_graphics.h"
#include "zone_environment_fog.h"

#include "d3d_model_render_state.h"
#include <cmath>

namespace ZoneEnvironmentFog
{
void Apply(IDirect3DDevice9 *device, const ZoneEnvironmentState::Data &environment)
{
    if (!device)
        return;

    const bool enabled = environment.valid &&
                         std::isfinite(environment.fogNear) && std::isfinite(environment.fogFar) &&
                         environment.fogFar > 0.0f &&
                         environment.fogFar > environment.fogNear;
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::fog, ((enabled ? TRUE : FALSE) != FALSE));
    if (!enabled)
        return;
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::fogColor, uint32_t(environment.fogColor));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::fogTableMode, RendererBackend::FogMode::None);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::fogVertexMode, RendererBackend::FogMode::Linear);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::fogStart, RendererBackend::FloatFromBits(D3DModelRenderState::FloatBits(environment.fogNear)));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::fogEnd, RendererBackend::FloatFromBits(D3DModelRenderState::FloatBits(environment.fogFar)));
}
}
