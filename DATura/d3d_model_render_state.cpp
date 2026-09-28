#include "stdafx.h"
#include "d3d_model_render_state.h"

#include "d3d_bump_mapping.h"
#include "custom_texture_assets.h"
#include "d3d_math.h"
#include "noesis_rapi.h"

#include <cmath>
#include <cstring>

namespace D3DModelRenderState
{
namespace
{
IDirect3DPixelShader9 *g_uiPixelShader = nullptr;
bool g_uiPixelShaderTried = false;

bool EnsureFfxiUiPixelShader(IDirect3DDevice9 *device)
{
    if (g_uiPixelShader)
        return true;
    if (g_uiPixelShaderTried || !device)
        return false;

    g_uiPixelShaderTried = true;
    static const char kShaderSource[] =
        "sampler2D BaseTexture : register(s0);\n"
        "float4 AlphaParams : register(c0);\n"
        "float4 main(float4 diffuse : COLOR0, float2 uv : TEXCOORD0) : COLOR0\n"
        "{\n"
        "    float4 texel = tex2D(BaseTexture, uv);\n"
        "    texel.a = min(saturate(texel.a * AlphaParams.x), AlphaParams.y);\n"
        "    return texel * diffuse;\n"
        "}\n";

    ID3DBlob *byteCode = nullptr;
    ID3DBlob *errors = nullptr;
    const HRESULT compileHr = D3DCompile(kShaderSource, sizeof(kShaderSource) - 1,
        "DATuraFfxiUi", nullptr, nullptr, "main", "ps_2_0",
        0, 0, &byteCode, &errors);
    if (FAILED(compileHr))
    {
        if (errors)
            OutputDebugStringA(static_cast<const char *>(errors->GetBufferPointer()));
        if (errors)
            errors->Release();
        return false;
    }

    const HRESULT shaderHr = device->CreatePixelShader(
        static_cast<const DWORD *>(byteCode->GetBufferPointer()), &g_uiPixelShader);
    byteCode->Release();
    if (errors)
        errors->Release();
    if (FAILED(shaderHr))
    {
        g_uiPixelShader = nullptr;
        OutputDebugStringA("WARNING: Unable to create FFXI UI pixel shader.\n");
        return false;
    }
    return true;
}
}

void MaterialBindingCache::Invalidate()
{
    material = nullptr;
    texture = nullptr;
    bumpMappingEnabled = false;
    bumpMappingIntensity = 1.0f;
    valid = false;
}

void ApplyFixedFunctionModelState(IDirect3DDevice9 *device, const bool enableMipMapping)
{
    if (!device)
        return;

    device->SetVertexShader(nullptr);
    device->SetPixelShader(nullptr);
    device->SetTexture(1, nullptr);

    // Keep the D3D9 renderer in the same fixed-function lane as FFXI's D3D8 client.
    device->SetRenderState(D3DRS_COLORVERTEX, TRUE);
    device->SetRenderState(D3DRS_NORMALIZENORMALS, TRUE);
    device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
    device->SetRenderState(D3DRS_DITHERENABLE, TRUE);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_RANGEFOGENABLE, FALSE);
    device->SetRenderState(D3DRS_CLIPPING, TRUE);
    device->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
    device->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_COLOR1);
    device->SetRenderState(D3DRS_SPECULARMATERIALSOURCE, D3DMCS_MATERIAL);
    device->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);

    device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
    device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    device->SetTextureStageState(0, D3DTSS_RESULTARG, D3DTA_CURRENT);
    device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    device->SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1);
    device->SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);

    device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_MIPFILTER,
                            enableMipMapping ? D3DTEXF_LINEAR : D3DTEXF_NONE);
    device->SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    device->SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
    device->SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(1, D3DSAMP_MIPFILTER,
                            enableMipMapping ? D3DTEXF_LINEAR : D3DTEXF_NONE);
}

void CalculateDirectionalLight(const int minuteOfDay, const bool automatic,
                               const float azimuthDegrees, const float elevationDegrees,
                               float outDirection[3])
{
    if (!outDirection)
        return;

    constexpr float kRadiansPerDegree = 0.01745329252f;
    if (automatic)
    {
        // XiArea::SetSunMoonTime rotates the retail light through one complete
        // vertical orbit per Vana'diel day. At night the main light passes
        // below the horizon and the inverse secondary light illuminates the scene.
        const float orbit =
            (static_cast<float>((minuteOfDay % 1440 + 1440) % 1440) / 1440.0f) *
            6.2831853f + elevationDegrees * kRadiansPerDegree;
        const float horizontal = sinf(orbit);
        const float compass = azimuthDegrees * kRadiansPerDegree;
        outDirection[0] = horizontal * cosf(compass);
        outDirection[1] = cosf(orbit);
        outDirection[2] = horizontal * sinf(compass);
        return;
    }

    const float azimuth = azimuthDegrees * kRadiansPerDegree;
    float elevation = elevationDegrees * kRadiansPerDegree;
    elevation = fmaxf(-1.3962634f, fminf(1.5533430f, elevation));

    const float horizontal = cosf(elevation);
    outDirection[0] = -horizontal * cosf(azimuth);
    outDirection[1] = -sinf(elevation);
    outDirection[2] = horizontal * sinf(azimuth);
}

void ApplyDynamicLighting(IDirect3DDevice9 *device, const int quality, const int minuteOfDay,
                          const bool indoor, const bool automaticDirection,
                          const float azimuthDegrees, const float elevationDegrees,
                          const bool useAuthoredColor, const D3DCOLOR authoredMainColor,
                          const D3DCOLOR authoredSecondaryColor,
                          const D3DCOLOR authoredAmbientColor, const float authoredLightPower,
                          const bool useAuthoredDirection, const float authoredDirection[3])
{
    if (!device)
        return;

    if (quality <= 0)
    {
        device->SetRenderState(D3DRS_LIGHTING, FALSE);
        device->LightEnable(0, FALSE);
        device->LightEnable(1, FALSE);
        return;
    }

    // A Vana'diel day drives a low, warm sun at dawn/dusk and a cooler overhead sun at noon.
    const float dayAngle = (static_cast<float>(minuteOfDay % 1440) / 1440.0f) * 6.2831853f;
    const float altitude = sinf(dayAngle - 1.5707963f);
    const float daylight = indoor ? 0.0f : fmaxf(0.0f, altitude);
    const float sunIntensity = indoor ? 0.28f : 0.22f + 0.78f * daylight;
    const float ambient = indoor ? 0.42f : 0.22f + 0.30f * daylight;

    D3DLIGHT9 sun = {};
    sun.Type = D3DLIGHT_DIRECTIONAL;
    if (useAuthoredColor)
    {
        const float power = std::isfinite(authoredLightPower)
            ? fmaxf(authoredLightPower, 0.0f) : 1.0f;
        sun.Diffuse = {
            (static_cast<float>((authoredMainColor >> 16) & 0xff) / 255.0f) * power,
            (static_cast<float>((authoredMainColor >> 8) & 0xff) / 255.0f) * power,
            (static_cast<float>(authoredMainColor & 0xff) / 255.0f) * power,
            1.0f
        };
    }
    else
    {
        sun.Diffuse = { 1.0f, 0.88f + 0.12f * daylight,
                        0.72f + 0.28f * daylight, 1.0f };
    }
    float direction[3] = {};
    if (automaticDirection && useAuthoredDirection && authoredDirection)
    {
        direction[0] = authoredDirection[0];
        direction[1] = authoredDirection[1];
        direction[2] = authoredDirection[2];
    }
    else
    {
        CalculateDirectionalLight(minuteOfDay, automaticDirection, azimuthDegrees,
                                  elevationDegrees, direction);
    }
    sun.Direction = { direction[0], direction[1], direction[2] };
    sun.Attenuation0 = 1.0f;
    device->SetLight(0, &sun);
    device->LightEnable(0, TRUE);
    if (useAuthoredColor && !useAuthoredDirection)
    {
        const float power = std::isfinite(authoredLightPower)
            ? fmaxf(authoredLightPower, 0.0f) : 1.0f;
        D3DLIGHT9 fill = {};
        fill.Type = D3DLIGHT_DIRECTIONAL;
        fill.Diffuse = {
            (static_cast<float>((authoredSecondaryColor >> 16) & 0xff) / 255.0f) * power,
            (static_cast<float>((authoredSecondaryColor >> 8) & 0xff) / 255.0f) * power,
            (static_cast<float>(authoredSecondaryColor & 0xff) / 255.0f) * power,
            1.0f
        };
        fill.Direction = { -direction[0], -direction[1], -direction[2] };
        fill.Attenuation0 = 1.0f;
        device->SetLight(1, &fill);
        device->LightEnable(1, TRUE);
    }
    else
    {
        device->LightEnable(1, FALSE);
    }
    device->SetRenderState(D3DRS_LIGHTING, TRUE);
    device->SetRenderState(D3DRS_AMBIENT, useAuthoredColor
        ? authoredAmbientColor
        : D3DCOLOR_COLORVALUE(ambient * 0.82f, ambient * 0.88f, ambient, 1.0f));
    device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
}

void SetTextureStageForOptionalTexture(IDirect3DDevice9 *device, IDirect3DTexture9 *texture,
                                       const D3DTEXTUREOP alphaOp)
{
    if (!device)
        return;

    device->SetTexture(0, texture);
    if (texture)
    {
        device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE2X);
        device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, alphaOp);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    }
    else
    {
        device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    }
}

void SetTextureScroll(IDirect3DDevice9 *device, const bool enabled, const float speedU,
                      const float speedV)
{
    if (!device)
        return;
    if (!enabled)
    {
        device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
        return;
    }
    const float seconds = (float)(GetTickCount64() % 1000000ULL) * 0.001f;
    D3DMATRIX texture = D3DMath::BuildIdentity();
    texture._31 = fmodf(seconds * speedU, 1.0f);
    texture._32 = fmodf(seconds * speedV, 1.0f);
    device->SetTransform(D3DTS_TEXTURE0, &texture);
    device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
}

DWORD FloatBits(const float value)
{
    DWORD bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

bool SetFfxiTexturePixelShader(IDirect3DDevice9 *device, const bool useAuthoredAlpha,
                               const bool expandDxt3Alpha, const float opacityScale,
                               const float *colorScale, const bool hasTexture,
                               const bool enableBumpMapping,
                               const float bumpMappingIntensity,
                               const float baseTextureWidth, const float baseTextureHeight)
{
    return D3DBumpMapping::ApplyTextureShader(
        device, useAuthoredAlpha, expandDxt3Alpha, opacityScale, colorScale,
        hasTexture, enableBumpMapping, bumpMappingIntensity,
        baseTextureWidth, baseTextureHeight);
}

bool SetFfxiUiPixelShader(IDirect3DDevice9 *device, const bool expandDxt3Alpha,
                          const float alphaScale, const float maxOpacity)
{
    if (!EnsureFfxiUiPixelShader(device))
    {
        if (device)
            device->SetPixelShader(nullptr);
        return false;
    }

    const float alphaParams[4] =
    {
        alphaScale * (expandDxt3Alpha ? 1.875f : 1.0f),
        maxOpacity,
        0.0f,
        0.0f
    };
    device->SetPixelShader(g_uiPixelShader);
    device->SetPixelShaderConstantF(0, alphaParams, 1);
    return true;
}

void ApplyOpaqueMaterial(IDirect3DDevice9 *device, MaterialBindingCache& cache,
                         const noesisMaterial_t *material, const noesisTex_t *texture,
                         const bool enableBumpMapping, const float bumpMappingIntensity)
{
    if (!device || (cache.valid && material == cache.material && texture == cache.texture &&
        enableBumpMapping == cache.bumpMappingEnabled &&
        bumpMappingIntensity == cache.bumpMappingIntensity))
        return;

    cache.valid = true;
    cache.material = material;
    cache.texture = texture;
    cache.bumpMappingEnabled = enableBumpMapping;
    cache.bumpMappingIntensity = bumpMappingIntensity;
    const bool twoSided = !material || (material->flags & NMATFLAG_TWOSIDED) != 0;
    const float alphaRef = material ? material->alphaTest : 0.0f;
    const CustomTextureAssets::Texture customTexture = CustomTextureAssets::ResolveBase(texture);
    IDirect3DTexture9 *d3dTexture = customTexture.texture ? customTexture.texture :
        (texture ? texture->pD3DTex : nullptr);
    const bool expandDxt3Alpha = texture && texture->texType == NOESISTEX_DXT3;
    device->SetRenderState(D3DRS_CULLMODE, twoSided ? D3DCULL_NONE : D3DCULL_CW);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, alphaRef > 0.0f ? TRUE : FALSE);
    if (alphaRef > 0.0f)
    {
        device->SetRenderState(D3DRS_ALPHAREF, (DWORD)(alphaRef * 255.0f));
        device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
    }
    const bool pbrActive = d3dTexture && CustomTextureAssets::ApplyPbrShader(
        device, texture, d3dTexture, alphaRef > 0.0f, expandDxt3Alpha, 1.0f);
    const bool shaderActive = pbrActive || (d3dTexture && SetFfxiTexturePixelShader(
        device, alphaRef > 0.0f, expandDxt3Alpha, 1.0f, nullptr, true,
        enableBumpMapping, bumpMappingIntensity,
        customTexture.texture ? static_cast<float>(customTexture.width) :
            (texture ? static_cast<float>(texture->w) : 0.0f),
        customTexture.texture ? static_cast<float>(customTexture.height) :
            (texture ? static_cast<float>(texture->h) : 0.0f)));
    if (!d3dTexture)
        device->SetPixelShader(nullptr); // Untextured materials use fixed-function vertex color.
    if (!pbrActive) { device->SetTexture(1, nullptr); device->SetTexture(2, nullptr); device->SetTexture(3, nullptr); }
    SetTextureStageForOptionalTexture(device, d3dTexture,
        shaderActive ? D3DTOP_SELECTARG1 :
        (alphaRef > 0.0f ? D3DTOP_MODULATE4X : D3DTOP_MODULATE2X));
}

void ApplyTransparentMaterial(IDirect3DDevice9 *device, MaterialBindingCache& cache,
                              const noesisMaterial_t *material, const noesisTex_t *texture,
                              const bool enableBumpMapping, const float bumpMappingIntensity)
{
    if (!device || (cache.valid && material == cache.material && texture == cache.texture &&
        enableBumpMapping == cache.bumpMappingEnabled &&
        bumpMappingIntensity == cache.bumpMappingIntensity))
        return;

    cache.valid = true;
    cache.material = material;
    cache.texture = texture;
    cache.bumpMappingEnabled = enableBumpMapping;
    cache.bumpMappingIntensity = bumpMappingIntensity;
    const bool twoSided = !material || (material->flags & NMATFLAG_TWOSIDED) != 0;
    const CustomTextureAssets::Texture customTexture = CustomTextureAssets::ResolveBase(texture);
    IDirect3DTexture9 *d3dTexture = customTexture.texture ? customTexture.texture :
        (texture ? texture->pD3DTex : nullptr);
    const bool expandDxt3Alpha = texture && texture->texType == NOESISTEX_DXT3;
    device->SetRenderState(D3DRS_CULLMODE, twoSided ? D3DCULL_NONE : D3DCULL_CW);
    const bool pbrActive = d3dTexture && CustomTextureAssets::ApplyPbrShader(
        device, texture, d3dTexture, true, expandDxt3Alpha, 1.0f);
    const bool shaderActive = pbrActive || (d3dTexture && SetFfxiTexturePixelShader(
        device, true, expandDxt3Alpha, 1.0f, nullptr, true,
        enableBumpMapping, bumpMappingIntensity,
        customTexture.texture ? static_cast<float>(customTexture.width) :
            (texture ? static_cast<float>(texture->w) : 0.0f),
        customTexture.texture ? static_cast<float>(customTexture.height) :
            (texture ? static_cast<float>(texture->h) : 0.0f)));
    if (!d3dTexture)
        device->SetPixelShader(nullptr);
    if (!pbrActive) { device->SetTexture(1, nullptr); device->SetTexture(2, nullptr); device->SetTexture(3, nullptr); }
    SetTextureStageForOptionalTexture(device, d3dTexture,
        shaderActive ? D3DTOP_SELECTARG1 : D3DTOP_MODULATE4X);
}

void ReleaseFfxiPixelShaders()
{
    D3DBumpMapping::Release();
    CustomTextureAssets::Release();
    if (g_uiPixelShader)
    {
        g_uiPixelShader->Release();
        g_uiPixelShader = nullptr;
    }
    g_uiPixelShaderTried = false;
}
}
