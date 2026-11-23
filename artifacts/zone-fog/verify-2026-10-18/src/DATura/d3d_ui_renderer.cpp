#include "stdafx.h"
#include "d3d9_graphics.h"
#include "d3d_ui_renderer.h"

#include "d3d9_device.h"
#include "d3d_model_render_state.h"
#include "noesis_rapi.h"

namespace
{
struct Vertex
{
    float x, y, z, rhw;
    DWORD color;
    float u, v;
};

constexpr auto kVertexFvf = RendererBackend::BuiltinVertexLayout::ScreenColorTexture;
}

namespace D3DUiRenderer
{
bool UsesFfxiDxt3Alpha(const noesisTex_t* texture)
{
    return texture && texture->texType == NOESISTEX_DXT3;
}

void DrawTexturedQuadUV(IDirect3DDevice9* device, const bool enableMipMapping,
                        RendererBackend::Texture* texture,
                        const float x, const float y, const float width, const float height,
                        const float u0, const float v0, const float u1, const float v1,
                        const DWORD color, const bool expandDxt3Alpha,
                        const float alphaScale, const float maxOpacity)
{
    if (!device || !texture || width <= 0.0f || height <= 0.0f)
        return;

    const Vertex vertices[4] =
    {
        { x - 0.5f,         y - 0.5f,          0.0f, 1.0f, color, u0, v0 },
        { x + width - 0.5f, y - 0.5f,          0.0f, 1.0f, color, u1, v0 },
        { x - 0.5f,         y + height - 0.5f, 0.0f, 1.0f, color, u0, v1 },
        { x + width - 0.5f, y + height - 0.5f, 0.0f, 1.0f, color, u1, v1 },
    };

    D3D9Device::PrepareScreenSpaceUiRenderState(device, kVertexFvf);
    D3D9Graphics::Context(device).BindTexture(0, texture);
    D3DModelRenderState::SetFfxiUiPixelShader(
        device, expandDxt3Alpha, alphaScale, maxOpacity);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::blend, ((TRUE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::sourceBlend, RendererBackend::BlendFactor::SourceAlpha);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::destinationBlend, RendererBackend::BlendFactor::InverseSourceAlpha);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::alphaTest, ((FALSE) != FALSE));
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::colorOperation, RendererBackend::TextureOperation::Modulate);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::colorArg1, RendererBackend::TextureArgument::Texture);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::colorArg2, RendererBackend::TextureArgument::Diffuse);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::alphaOperation, RendererBackend::TextureOperation::Modulate);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::alphaArg1, RendererBackend::TextureArgument::Texture);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::alphaArg2, RendererBackend::TextureArgument::Diffuse);
    D3D9Graphics::Context(device).SetSamplerField(0, &RendererBackend::SamplerState::minFilter, RendererBackend::TextureFilter::Linear);
    D3D9Graphics::Context(device).SetSamplerField(0, &RendererBackend::SamplerState::magFilter, RendererBackend::TextureFilter::Linear);
    D3D9Graphics::Context(device).SetSamplerField(0, &RendererBackend::SamplerState::mipFilter, enableMipMapping ? RendererBackend::TextureFilter::Linear : RendererBackend::TextureFilter::None);
    D3D9Graphics::Context(device).DrawImmediate(RendererBackend::Primitive::TriangleStrip, 2, vertices, sizeof(Vertex));

    D3D9Graphics::Context(device).BindTexture(0, nullptr);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::blend, ((FALSE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::alphaTest, ((FALSE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthTest, ((TRUE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthWrite, ((TRUE) != FALSE));
}

void DrawTexturedQuad(IDirect3DDevice9* device, const bool enableMipMapping,
                      noesisTex_t* texture,
                      const float x, const float y, const float width, const float height,
                      const DWORD color)
{
    if (!texture || !texture->gpuTexture)
        return;
    DrawTexturedQuadUV(device, enableMipMapping, texture->gpuTexture,
                       x, y, width, height, 0.0f, 0.0f, 1.0f, 1.0f, color,
                       UsesFfxiDxt3Alpha(texture));
}

void DrawSolidQuad(IDirect3DDevice9* device,
                   const float x, const float y, const float width, const float height,
                   const DWORD color)
{
    if (!device || width <= 0.0f || height <= 0.0f)
        return;

    const Vertex vertices[4] =
    {
        { x,         y,          0.0f, 1.0f, color, 0.0f, 0.0f },
        { x + width, y,          0.0f, 1.0f, color, 0.0f, 0.0f },
        { x,         y + height, 0.0f, 1.0f, color, 0.0f, 0.0f },
        { x + width, y + height, 0.0f, 1.0f, color, 0.0f, 0.0f },
    };

    D3D9Device::PrepareScreenSpaceUiRenderState(device, kVertexFvf);
    D3D9Graphics::Context(device).BindTexture(0, nullptr);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::blend, ((TRUE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::sourceBlend, RendererBackend::BlendFactor::SourceAlpha);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::destinationBlend, RendererBackend::BlendFactor::InverseSourceAlpha);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::colorOperation, RendererBackend::TextureOperation::SelectArg1);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::colorArg1, RendererBackend::TextureArgument::Diffuse);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::alphaOperation, RendererBackend::TextureOperation::SelectArg1);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::alphaArg1, RendererBackend::TextureArgument::Diffuse);
    D3D9Graphics::Context(device).DrawImmediate(RendererBackend::Primitive::TriangleStrip, 2, vertices, sizeof(Vertex));
}

void DrawTextureRegion(IDirect3DDevice9* device, const bool enableMipMapping,
                       noesisTex_t* texture,
                       const float x, const float y, const float width, const float height,
                       const float sourceX, const float sourceY,
                       const float sourceWidth, const float sourceHeight, const DWORD color)
{
    if (!texture || !texture->gpuTexture || texture->w <= 0 || texture->h <= 0)
        return;

    DrawTexturedQuadUV(device, enableMipMapping, texture->gpuTexture,
                       x, y, width, height,
                       sourceX / static_cast<float>(texture->w),
                       sourceY / static_cast<float>(texture->h),
                       (sourceX + sourceWidth) / static_cast<float>(texture->w),
                       (sourceY + sourceHeight) / static_cast<float>(texture->h),
                       color, UsesFfxiDxt3Alpha(texture));
}
}
