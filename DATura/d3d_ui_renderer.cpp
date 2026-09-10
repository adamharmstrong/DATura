#include "stdafx.h"
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

constexpr DWORD kVertexFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;
}

namespace D3DUiRenderer
{
bool UsesFfxiDxt3Alpha(const noesisTex_t* texture)
{
    return texture && texture->texType == NOESISTEX_DXT3;
}

void DrawTexturedQuadUV(IDirect3DDevice9* device, const bool enableMipMapping,
                        IDirect3DTexture9* texture,
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
    device->SetTexture(0, texture);
    D3DModelRenderState::SetFfxiUiPixelShader(
        device, expandDxt3Alpha, alphaScale, maxOpacity);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_MIPFILTER,
        enableMipMapping ? D3DTEXF_LINEAR : D3DTEXF_NONE);
    device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(Vertex));

    device->SetTexture(0, nullptr);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_ZENABLE, TRUE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
}

void DrawTexturedQuad(IDirect3DDevice9* device, const bool enableMipMapping,
                      noesisTex_t* texture,
                      const float x, const float y, const float width, const float height,
                      const DWORD color)
{
    if (!texture || !texture->pD3DTex)
        return;
    DrawTexturedQuadUV(device, enableMipMapping, texture->pD3DTex,
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
    device->SetTexture(0, nullptr);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(Vertex));
}

void DrawTextureRegion(IDirect3DDevice9* device, const bool enableMipMapping,
                       noesisTex_t* texture,
                       const float x, const float y, const float width, const float height,
                       const float sourceX, const float sourceY,
                       const float sourceWidth, const float sourceHeight, const DWORD color)
{
    if (!texture || !texture->pD3DTex || texture->w <= 0 || texture->h <= 0)
        return;

    DrawTexturedQuadUV(device, enableMipMapping, texture->pD3DTex,
                       x, y, width, height,
                       sourceX / static_cast<float>(texture->w),
                       sourceY / static_cast<float>(texture->h),
                       (sourceX + sourceWidth) / static_cast<float>(texture->w),
                       (sourceY + sourceHeight) / static_cast<float>(texture->h),
                       color, UsesFfxiDxt3Alpha(texture));
}
}
