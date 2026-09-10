#pragma once

#include <d3d9.h>

struct noesisMaterial_t;
struct noesisTex_t;

namespace D3DModelRenderState
{
struct MaterialBindingCache
{
    const noesisMaterial_t *material = nullptr;
    const noesisTex_t *texture = nullptr;
    bool valid = false;

    void Invalidate();
};

void ApplyFixedFunctionModelState(IDirect3DDevice9 *device, bool enableMipMapping);
void ApplyDynamicLighting(IDirect3DDevice9 *device, int quality, int minuteOfDay,
                          bool indoor);
void SetTextureStageForOptionalTexture(IDirect3DDevice9 *device, IDirect3DTexture9 *texture,
                                       D3DTEXTUREOP alphaOp);
void SetTextureScroll(IDirect3DDevice9 *device, bool enabled, float speedU = 0.0f,
                      float speedV = 0.0f);
DWORD FloatBits(float value);

bool SetFfxiTexturePixelShader(IDirect3DDevice9 *device, bool useAuthoredAlpha,
                               bool expandDxt3Alpha, float opacityScale = 1.0f,
                               const float *colorScale = nullptr, bool hasTexture = true);
bool SetFfxiUiPixelShader(IDirect3DDevice9 *device, bool expandDxt3Alpha,
                          float alphaScale = 1.0f, float maxOpacity = 1.0f);
void ApplyOpaqueMaterial(IDirect3DDevice9 *device, MaterialBindingCache& cache,
                         const noesisMaterial_t *material, const noesisTex_t *texture);
void ApplyTransparentMaterial(IDirect3DDevice9 *device, MaterialBindingCache& cache,
                              const noesisMaterial_t *material, const noesisTex_t *texture);
void ReleaseFfxiPixelShaders();
}
