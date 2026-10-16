#include "stdafx.h"
#include "d3d9_graphics.h"
#include "zone_object_highlight_renderer.h"

#include "d3d_model_buffers.h"
#include "noesis_rapi.h"

namespace ZoneObjectHighlightRenderer
{
void Draw(IDirect3DDevice9 *device, noesisModel_t *model,
          const std::set<std::string>& highlightedObjects,
          const std::map<std::string, ZoneObjectTransform::DebugTransform>& overrides)
{
    if (!device || !model || highlightedObjects.empty())
        return;

    D3DMATRIX baseWorld = {};
    D3D9Graphics::Context(device).QueryTransform(RendererBackend::Transform::World, &baseWorld._11);
    D3D9Graphics::Context(device).BindBuiltinVertexLayout(RendererBackend::BuiltinVertexLayout::Model);
    D3D9Graphics::Context(device).BindTexture(0, nullptr);
    D3D9Graphics::Context(device).SetLightingField(&RendererBackend::LightingState::enabled, false);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthTest, ((TRUE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthWrite, ((FALSE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::blend, ((FALSE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::alphaTest, ((FALSE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::cull, RendererBackend::Cull::None);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::fill, RendererBackend::Fill::Wireframe);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::colorOperation, RendererBackend::TextureOperation::SelectArg1);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::colorArg1, RendererBackend::TextureArgument::Factor);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::alphaOperation, RendererBackend::TextureOperation::SelectArg1);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::alphaArg1, RendererBackend::TextureArgument::Factor);
    D3D9Graphics::Context(device).SetTextureFactor(D3DCOLOR_XRGB(255, 128, 0));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthBias, RendererBackend::FloatFromBits(0xBA83126F));

    for (const noesisModel_t::Submesh& submesh : model->submeshes)
    {
        if (highlightedObjects.find(submesh.objectName) == highlightedObjects.end() ||
            !D3DModelBuffers::HasDrawBuffers(model, submesh))
        {
            continue;
        }

        ZoneObjectTransform::ApplyWorldTransform(
            device, submesh.objectName, true, overrides, baseWorld);
        D3DModelBuffers::DrawSubmesh(device, model, submesh);
    }

    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::fill, RendererBackend::Fill::Solid);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthWrite, ((TRUE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthBias, RendererBackend::FloatFromBits(0));
    D3D9Graphics::Context(device).SetTransform(RendererBackend::Transform::World, &baseWorld._11);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::colorOperation, RendererBackend::TextureOperation::Modulate);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::alphaOperation, RendererBackend::TextureOperation::Modulate);
    device->SetStreamSource(0, nullptr, 0, 0);
    device->SetIndices(nullptr);
}
}
