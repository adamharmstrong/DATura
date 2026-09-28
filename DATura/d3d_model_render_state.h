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
    bool bumpMappingEnabled = false;
    float bumpMappingIntensity = 1.0f;
    bool valid = false;

    void Invalidate();
};

void ApplyFixedFunctionModelState(IDirect3DDevice9 *device, bool enableMipMapping);
void CalculateDirectionalLight(int minuteOfDay, bool automatic, float azimuthDegrees,
                               float elevationDegrees, float outDirection[3]);
void ApplyDynamicLighting(IDirect3DDevice9 *device, int quality, int minuteOfDay,
                          bool indoor, bool automaticDirection, float azimuthDegrees,
                          float elevationDegrees, bool useAuthoredColor,
                          D3DCOLOR authoredMainColor, D3DCOLOR authoredSecondaryColor,
                          D3DCOLOR authoredAmbientColor, float authoredLightPower,
                          bool useAuthoredDirection, const float authoredDirection[3]);
void SetTextureStageForOptionalTexture(IDirect3DDevice9 *device, IDirect3DTexture9 *texture,
                                       D3DTEXTUREOP alphaOp);
void SetTextureScroll(IDirect3DDevice9 *device, bool enabled, float speedU = 0.0f,
                      float speedV = 0.0f);
DWORD FloatBits(float value);

bool SetFfxiTexturePixelShader(IDirect3DDevice9 *device, bool useAuthoredAlpha,
                               bool expandDxt3Alpha, float opacityScale = 1.0f,
                               const float *colorScale = nullptr, bool hasTexture = true,
                               bool enableBumpMapping = false,
                               float bumpMappingIntensity = 1.0f,
                               float baseTextureWidth = 0.0f, float baseTextureHeight = 0.0f);
bool SetFfxiUiPixelShader(IDirect3DDevice9 *device, bool expandDxt3Alpha,
                          float alphaScale = 1.0f, float maxOpacity = 1.0f);
void ApplyOpaqueMaterial(IDirect3DDevice9 *device, MaterialBindingCache& cache,
                         const noesisMaterial_t *material, const noesisTex_t *texture,
                         bool enableBumpMapping = false, float bumpMappingIntensity = 1.0f);
void ApplyTransparentMaterial(IDirect3DDevice9 *device, MaterialBindingCache& cache,
                              const noesisMaterial_t *material, const noesisTex_t *texture,
                              bool enableBumpMapping = false, float bumpMappingIntensity = 1.0f);
void ReleaseFfxiPixelShaders();
}
