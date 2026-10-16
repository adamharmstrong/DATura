#include "stdafx.h"
#include "d3d9_graphics.h"
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
RendererBackend::Shader* g_uiPixelShader = nullptr;
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

    D3D9Graphics::Context graphics(device);
    g_uiPixelShader = graphics.CreateShader({ RendererBackend::ShaderStage::Pixel,
        kShaderSource, sizeof(kShaderSource) - 1, "DATuraFfxiUi", "main", "ps_2_0" });
    if (!g_uiPixelShader)
        return false;
    return true;
}
}

void ApplyFixedFunctionModelState(IDirect3DDevice9 *device, const bool enableMipMapping)
{
    if (!device)
        return;

    D3D9Graphics::Context(device).BindShader(RendererBackend::ShaderStage::Vertex, nullptr);
    D3D9Graphics::Context(device).BindShader(RendererBackend::ShaderStage::Pixel, nullptr);
    D3D9Graphics::Context(device).BindTexture(1, nullptr);

    // Keep the D3D9 renderer in the same fixed-function lane as FFXI's D3D8 client.
    D3D9Graphics::Context(device).SetLightingField(&RendererBackend::LightingState::vertexColor, true);
    D3D9Graphics::Context(device).SetLightingField(&RendererBackend::LightingState::normalizeNormals, true);
    D3D9Graphics::Context(device).SetLightingField(&RendererBackend::LightingState::specular, false);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::dither, ((TRUE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::fog, ((FALSE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::rangeFog, ((FALSE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::clipping, ((TRUE) != FALSE));
    D3D9Graphics::Context(device).SetLightingField(&RendererBackend::LightingState::diffuseSource, RendererBackend::MaterialSource::DiffuseVertex);
    D3D9Graphics::Context(device).SetLightingField(&RendererBackend::LightingState::ambientSource, RendererBackend::MaterialSource::DiffuseVertex);
    D3D9Graphics::Context(device).SetLightingField(&RendererBackend::LightingState::specularSource, RendererBackend::MaterialSource::Material);
    D3D9Graphics::Context(device).SetLightingField(&RendererBackend::LightingState::emissiveSource, RendererBackend::MaterialSource::Material);

    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::coordinateIndex, uint32_t(0));
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::transform, RendererBackend::TextureTransform::Disabled);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::result, RendererBackend::TextureArgument::Current);
    D3D9Graphics::Context(device).SetTextureStageField(1, &RendererBackend::TextureStageState::colorOperation, RendererBackend::TextureOperation::Disable);
    D3D9Graphics::Context(device).SetTextureStageField(1, &RendererBackend::TextureStageState::alphaOperation, RendererBackend::TextureOperation::Disable);
    D3D9Graphics::Context(device).SetTextureStageField(1, &RendererBackend::TextureStageState::coordinateIndex, uint32_t(1));
    D3D9Graphics::Context(device).SetTextureStageField(1, &RendererBackend::TextureStageState::transform, RendererBackend::TextureTransform::Disabled);

    D3D9Graphics::Context(device).SetSamplerField(0, &RendererBackend::SamplerState::addressU, RendererBackend::TextureAddress::Wrap);
    D3D9Graphics::Context(device).SetSamplerField(0, &RendererBackend::SamplerState::addressV, RendererBackend::TextureAddress::Wrap);
    D3D9Graphics::Context(device).SetSamplerField(0, &RendererBackend::SamplerState::minFilter, RendererBackend::TextureFilter::Linear);
    D3D9Graphics::Context(device).SetSamplerField(0, &RendererBackend::SamplerState::magFilter, RendererBackend::TextureFilter::Linear);
    D3D9Graphics::Context(device).SetSamplerField(0, &RendererBackend::SamplerState::mipFilter, enableMipMapping ? RendererBackend::TextureFilter::Linear : RendererBackend::TextureFilter::None);
    D3D9Graphics::Context(device).SetSamplerField(1, &RendererBackend::SamplerState::addressU, RendererBackend::TextureAddress::Wrap);
    D3D9Graphics::Context(device).SetSamplerField(1, &RendererBackend::SamplerState::addressV, RendererBackend::TextureAddress::Wrap);
    D3D9Graphics::Context(device).SetSamplerField(1, &RendererBackend::SamplerState::minFilter, RendererBackend::TextureFilter::Linear);
    D3D9Graphics::Context(device).SetSamplerField(1, &RendererBackend::SamplerState::magFilter, RendererBackend::TextureFilter::Linear);
    D3D9Graphics::Context(device).SetSamplerField(1, &RendererBackend::SamplerState::mipFilter, enableMipMapping ? RendererBackend::TextureFilter::Linear : RendererBackend::TextureFilter::None);
}

void CalculateDirectionalLight(const float minuteOfDay, const bool automatic,
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
        float minute = std::isfinite(minuteOfDay) ? std::fmod(minuteOfDay, 1440.0f) : 0.0f;
        if (minute < 0.0f)
            minute += 1440.0f;
        const float orbit = (minute / 1440.0f) * 6.2831853f +
            elevationDegrees * kRadiansPerDegree;
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

void ApplyDynamicLighting(IDirect3DDevice9 *device, const int quality, const float minuteOfDay,
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
        D3D9Graphics::Context(device).SetLightingField(&RendererBackend::LightingState::enabled, false);
        D3D9Graphics::Context(device).EnableLight(0, false);
        D3D9Graphics::Context(device).EnableLight(1, false);
        return;
    }

    // A Vana'diel day drives a low, warm sun at dawn/dusk and a cooler overhead sun at noon.
    float minute = std::isfinite(minuteOfDay) ? std::fmod(minuteOfDay, 1440.0f) : 0.0f;
    if (minute < 0.0f)
        minute += 1440.0f;
    const float dayAngle = (minute / 1440.0f) * 6.2831853f;
    const float altitude = sinf(dayAngle - 1.5707963f);
    const float daylight = indoor ? 0.0f : fmaxf(0.0f, altitude);
    const float sunIntensity = indoor ? 0.28f : 0.22f + 0.78f * daylight;
    const float ambient = indoor ? 0.42f : 0.22f + 0.30f * daylight;

    RendererBackend::Light sun = {};
    sun.type = RendererBackend::LightType::Directional;
    if (useAuthoredColor)
    {
        const float power = std::isfinite(authoredLightPower)
            ? fmaxf(authoredLightPower, 0.0f) : 1.0f;
        sun.diffuse = {
            (static_cast<float>((authoredMainColor >> 16) & 0xff) / 255.0f) * power,
            (static_cast<float>((authoredMainColor >> 8) & 0xff) / 255.0f) * power,
            (static_cast<float>(authoredMainColor & 0xff) / 255.0f) * power,
            1.0f
        };
    }
    else
    {
        sun.diffuse = { 1.0f, 0.88f + 0.12f * daylight,
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
    sun.direction = { direction[0], direction[1], direction[2] };
    sun.attenuation0 = 1.0f;
    D3D9Graphics::Context(device).SetLight(0, sun);
    D3D9Graphics::Context(device).EnableLight(0, true);
    if (useAuthoredColor && !useAuthoredDirection)
    {
        const float power = std::isfinite(authoredLightPower)
            ? fmaxf(authoredLightPower, 0.0f) : 1.0f;
        RendererBackend::Light fill = {};
        fill.type = RendererBackend::LightType::Directional;
        fill.diffuse = {
            (static_cast<float>((authoredSecondaryColor >> 16) & 0xff) / 255.0f) * power,
            (static_cast<float>((authoredSecondaryColor >> 8) & 0xff) / 255.0f) * power,
            (static_cast<float>(authoredSecondaryColor & 0xff) / 255.0f) * power,
            1.0f
        };
        fill.direction = { -direction[0], -direction[1], -direction[2] };
        fill.attenuation0 = 1.0f;
        D3D9Graphics::Context(device).SetLight(1, fill);
        D3D9Graphics::Context(device).EnableLight(1, true);
    }
    else
    {
        D3D9Graphics::Context(device).EnableLight(1, false);
    }
    D3D9Graphics::Context(device).SetLightingField(&RendererBackend::LightingState::enabled, true);
    D3D9Graphics::Context(device).SetLightingField(&RendererBackend::LightingState::ambient, uint32_t(useAuthoredColor
        ? authoredAmbientColor
        : D3DCOLOR_COLORVALUE(ambient * 0.82f, ambient * 0.88f, ambient, 1.0f)));
    D3D9Graphics::Context(device).SetLightingField(&RendererBackend::LightingState::specular, false);
}

void SetTextureStageForOptionalTexture(IDirect3DDevice9 *device, IDirect3DTexture9 *texture,
                                       const D3DTEXTUREOP alphaOp)
{
    if (!device)
        return;

    device->SetTexture(0, texture);
    if (texture)
    {
        D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::colorOperation, RendererBackend::TextureOperation::Modulate2X);
        D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::colorArg1, RendererBackend::TextureArgument::Texture);
        D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::colorArg2, RendererBackend::TextureArgument::Diffuse);
        D3D9Graphics::Context(device).SetLegacyTextureAlphaOperation(0, alphaOp);
        D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::alphaArg1, RendererBackend::TextureArgument::Texture);
        D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::alphaArg2, RendererBackend::TextureArgument::Diffuse);
    }
    else
    {
        D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::colorOperation, RendererBackend::TextureOperation::SelectArg1);
        D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::colorArg1, RendererBackend::TextureArgument::Diffuse);
        D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::alphaOperation, RendererBackend::TextureOperation::SelectArg1);
        D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::alphaArg1, RendererBackend::TextureArgument::Diffuse);
    }
}

void SetTextureScroll(IDirect3DDevice9 *device, const bool enabled, const float speedU,
                      const float speedV)
{
    if (!device)
        return;
    if (!enabled)
    {
        D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::transform, RendererBackend::TextureTransform::Disabled);
        return;
    }
    const float seconds = (float)(GetTickCount64() % 1000000ULL) * 0.001f;
    D3DMATRIX texture = D3DMath::BuildIdentity();
    texture._31 = fmodf(seconds * speedU, 1.0f);
    texture._32 = fmodf(seconds * speedV, 1.0f);
    D3D9Graphics::Context(device).SetTextureTransform(0, &texture._11);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::transform, RendererBackend::TextureTransform::Count2);
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
            D3D9Graphics::Context(device).BindShader(RendererBackend::ShaderStage::Pixel, nullptr);
        return false;
    }

    const float alphaParams[4] =
    {
        alphaScale * (expandDxt3Alpha ? 1.875f : 1.0f),
        maxOpacity,
        0.0f,
        0.0f
    };
    D3D9Graphics::Context graphics(device);
    graphics.BindShader(RendererBackend::ShaderStage::Pixel, g_uiPixelShader);
    graphics.SetShaderConstants(RendererBackend::ShaderStage::Pixel, 0, alphaParams, 1);
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
        (texture ? D3D9Graphics::NativeTexture(texture->gpuTexture) : nullptr);
    const bool expandDxt3Alpha = texture && texture->texType == NOESISTEX_DXT3;
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::cull, twoSided ? RendererBackend::Cull::None : RendererBackend::Cull::Clockwise);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::alphaTest, ((alphaRef > 0.0f ? TRUE : FALSE) != FALSE));
    if (alphaRef > 0.0f)
    {
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::alphaReference, uint32_t((DWORD)(alphaRef * 255.0f)));
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::alphaCompare, RendererBackend::Compare::Greater);
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
        D3D9Graphics::Context(device).BindShader(RendererBackend::ShaderStage::Pixel, nullptr); // Untextured materials use fixed-function vertex color.
    if (!pbrActive) { D3D9Graphics::Context(device).BindTexture(1, nullptr); D3D9Graphics::Context(device).BindTexture(2, nullptr); D3D9Graphics::Context(device).BindTexture(3, nullptr); }
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
        (texture ? D3D9Graphics::NativeTexture(texture->gpuTexture) : nullptr);
    const bool expandDxt3Alpha = texture && texture->texType == NOESISTEX_DXT3;
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::cull, twoSided ? RendererBackend::Cull::None : RendererBackend::Cull::Clockwise);
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
        D3D9Graphics::Context(device).BindShader(RendererBackend::ShaderStage::Pixel, nullptr);
    if (!pbrActive) { D3D9Graphics::Context(device).BindTexture(1, nullptr); D3D9Graphics::Context(device).BindTexture(2, nullptr); D3D9Graphics::Context(device).BindTexture(3, nullptr); }
    SetTextureStageForOptionalTexture(device, d3dTexture,
        shaderActive ? D3DTOP_SELECTARG1 : D3DTOP_MODULATE4X);
}

void ReleaseFfxiPixelShaders()
{
    D3DBumpMapping::Release();
    CustomTextureAssets::Release();
    if (g_uiPixelShader)
    {
        delete g_uiPixelShader;
        g_uiPixelShader = nullptr;
    }
    g_uiPixelShaderTried = false;
}
}
