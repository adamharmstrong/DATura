#pragma once

#include <d3d9.h>
#include "renderer_materials.h"

struct noesisMaterial_t;
struct noesisTex_t;

namespace D3DModelRenderState
{
using MaterialBindingCache = RendererBackend::MaterialBindingCache;

void ApplyFixedFunctionModelState(IDirect3DDevice9 *device, bool enableMipMapping);
void CalculateDirectionalLight(float minuteOfDay, bool automatic, float azimuthDegrees,
                               float elevationDegrees, float outDirection[3]);
void ApplyDynamicLighting(IDirect3DDevice9 *device, int quality, float minuteOfDay,
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

// Borrows the device. The existing state functions remain the D3D9 implementation
// and compatibility entry points for callers still using the legacy API.
class Materials final : public RendererBackend::Materials
{
public:
    explicit Materials(IDirect3DDevice9* device) : device_(device) {}
    void ApplyOpaque(MaterialBindingCache& cache, const noesisMaterial_t* material,
        const noesisTex_t* texture, bool bump = false, float intensity = 1.0f) override
    { ApplyOpaqueMaterial(device_, cache, material, texture, bump, intensity); }
    void ApplyTransparent(MaterialBindingCache& cache, const noesisMaterial_t* material,
        const noesisTex_t* texture, bool bump = false, float intensity = 1.0f) override
    { ApplyTransparentMaterial(device_, cache, material, texture, bump, intensity); }
private:
    IDirect3DDevice9* device_;
};
}
