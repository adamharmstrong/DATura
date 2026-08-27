#include "stdafx.h"
#include "d3d_model_render_state.h"

#include "d3d_math.h"
#include "noesis_rapi.h"

#include <cmath>
#include <cstring>

namespace D3DModelRenderState
{
namespace
{
IDirect3DPixelShader9 *g_texturePixelShader = nullptr;
bool g_texturePixelShaderTried = false;
IDirect3DPixelShader9 *g_uiPixelShader = nullptr;
bool g_uiPixelShaderTried = false;

bool EnsureFfxiTexturePixelShader(IDirect3DDevice9 *device)
{
    if (g_texturePixelShader)
        return true;
    if (g_texturePixelShaderTried || !device)
        return false;

    g_texturePixelShaderTried = true;
    static const char kShaderSource[] =
        "sampler2D BaseTexture : register(s0);\n"
        "float4 AlphaParams : register(c0);\n"
        "float4 ColorScale : register(c1);\n"
        "float4 main(float4 diffuse : COLOR0, float2 uv : TEXCOORD0) : COLOR0\n"
        "{\n"
        "    float4 texel = tex2D(BaseTexture, uv);\n"
        "    float textureAlpha = (AlphaParams.y > 0.5) ? saturate(texel.a * 1.875) : texel.a;\n"
        "    float vertexAlpha = saturate(diffuse.a * 2.0);\n"
        "    float alpha = ((AlphaParams.x > 0.5) ? textureAlpha * vertexAlpha : 1.0) * AlphaParams.z;\n"
        "    return float4(saturate(2.0 * texel.rgb * diffuse.rgb * ColorScale.rgb), saturate(alpha));\n"
        "}\n";

    ID3DBlob *byteCode = nullptr;
    ID3DBlob *errors = nullptr;
    const HRESULT compileHr = D3DCompile(kShaderSource, sizeof(kShaderSource) - 1,
        "DATuraFfxiTexture", nullptr, nullptr, "main", "ps_2_0",
        D3DCOMPILE_ENABLE_STRICTNESS, 0, &byteCode, &errors);
    if (FAILED(compileHr))
    {
        if (errors)
            OutputDebugStringA(static_cast<const char *>(errors->GetBufferPointer()));
        if (errors)
            errors->Release();
        return false;
    }

    const HRESULT shaderHr = device->CreatePixelShader(
        static_cast<const DWORD *>(byteCode->GetBufferPointer()), &g_texturePixelShader);
    byteCode->Release();
    if (errors)
        errors->Release();
    if (FAILED(shaderHr))
    {
        g_texturePixelShader = nullptr;
        OutputDebugStringA("WARNING: Unable to create FFXI DXT texture pixel shader.\n");
        return false;
    }
    return true;
}

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
        D3DCOMPILE_ENABLE_STRICTNESS, 0, &byteCode, &errors);
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
}

void ApplyDynamicLighting(IDirect3DDevice9 *device, const int quality, const int minuteOfDay,
                          const bool indoor)
{
    if (!device)
        return;

    if (quality <= 0)
    {
        device->SetRenderState(D3DRS_LIGHTING, FALSE);
        device->LightEnable(0, FALSE);
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
    sun.Diffuse = { 1.0f, 0.88f + 0.12f * daylight, 0.72f + 0.28f * daylight, 1.0f };
    sun.Direction = { -0.45f * cosf(dayAngle), -0.35f - 0.65f * fmaxf(altitude, 0.15f),
                      0.45f * sinf(dayAngle) };
    sun.Attenuation0 = 1.0f;
    device->SetLight(0, &sun);
    device->LightEnable(0, TRUE);
    device->SetRenderState(D3DRS_LIGHTING, TRUE);
    device->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_COLORVALUE(
        ambient * 0.82f, ambient * 0.88f, ambient, 1.0f));
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
                               const float *colorScale)
{
    if (!EnsureFfxiTexturePixelShader(device))
    {
        if (device)
            device->SetPixelShader(nullptr);
        return false;
    }

    const float alphaParams[4] =
    {
        useAuthoredAlpha ? 1.0f : 0.0f,
        expandDxt3Alpha ? 1.0f : 0.0f,
        opacityScale,
        0.0f
    };
    const float defaultColorScale[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float authoredColorScale[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    if (colorScale)
    {
        authoredColorScale[0] = colorScale[0];
        authoredColorScale[1] = colorScale[1];
        authoredColorScale[2] = colorScale[2];
    }
    device->SetPixelShader(g_texturePixelShader);
    device->SetPixelShaderConstantF(0, alphaParams, 1);
    device->SetPixelShaderConstantF(1,
        colorScale ? authoredColorScale : defaultColorScale, 1);
    return true;
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
                         const noesisMaterial_t *material, const noesisTex_t *texture)
{
    if (!device || (cache.valid && material == cache.material && texture == cache.texture))
        return;

    cache.valid = true;
    cache.material = material;
    cache.texture = texture;
    const bool twoSided = !material || (material->flags & NMATFLAG_TWOSIDED) != 0;
    const float alphaRef = material ? material->alphaTest : 0.0f;
    IDirect3DTexture9 *d3dTexture = texture ? texture->pD3DTex : nullptr;
    const bool expandDxt3Alpha = texture && texture->texType == NOESISTEX_DXT3;
    device->SetRenderState(D3DRS_CULLMODE, twoSided ? D3DCULL_NONE : D3DCULL_CW);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, alphaRef > 0.0f ? TRUE : FALSE);
    if (alphaRef > 0.0f)
    {
        device->SetRenderState(D3DRS_ALPHAREF, (DWORD)(alphaRef * 255.0f));
        device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
    }
    const bool shaderActive = d3dTexture && SetFfxiTexturePixelShader(
        device, alphaRef > 0.0f, expandDxt3Alpha);
    SetTextureStageForOptionalTexture(device, d3dTexture,
        shaderActive ? D3DTOP_SELECTARG1 :
        (alphaRef > 0.0f ? D3DTOP_MODULATE4X : D3DTOP_MODULATE2X));
}

void ApplyTransparentMaterial(IDirect3DDevice9 *device, MaterialBindingCache& cache,
                              const noesisMaterial_t *material, const noesisTex_t *texture)
{
    if (!device || (cache.valid && material == cache.material && texture == cache.texture))
        return;

    cache.valid = true;
    cache.material = material;
    cache.texture = texture;
    const bool twoSided = !material || (material->flags & NMATFLAG_TWOSIDED) != 0;
    IDirect3DTexture9 *d3dTexture = texture ? texture->pD3DTex : nullptr;
    const bool expandDxt3Alpha = texture && texture->texType == NOESISTEX_DXT3;
    device->SetRenderState(D3DRS_CULLMODE, twoSided ? D3DCULL_NONE : D3DCULL_CW);
    const bool shaderActive = d3dTexture && SetFfxiTexturePixelShader(
        device, true, expandDxt3Alpha);
    SetTextureStageForOptionalTexture(device, d3dTexture,
        shaderActive ? D3DTOP_SELECTARG1 : D3DTOP_MODULATE4X);
}

void ReleaseFfxiPixelShaders()
{
    if (g_texturePixelShader)
    {
        g_texturePixelShader->Release();
        g_texturePixelShader = nullptr;
    }
    g_texturePixelShaderTried = false;
    if (g_uiPixelShader)
    {
        g_uiPixelShader->Release();
        g_uiPixelShader = nullptr;
    }
    g_uiPixelShaderTried = false;
}
}
