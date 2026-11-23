#include "stdafx.h"
#include "d3d9_graphics.h"
#include "d3d_bump_mapping.h"

namespace D3DBumpMapping
{
namespace
{
RendererBackend::Shader* g_texturePixelShader = nullptr;
bool g_texturePixelShaderTried = false;

bool EnsureTexturePixelShader(IDirect3DDevice9* device)
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
        "float4 BumpParams : register(c2);\n"
        "float4 main(float4 diffuse : COLOR0, float2 uv : TEXCOORD0) : COLOR0\n"
        "{\n"
        "    float4 texel = lerp(tex2D(BaseTexture, uv), float4(1,1,1,1), AlphaParams.w);\n"
        "    float height = dot(texel.rgb, float3(0.299, 0.587, 0.114));\n"
        "    float shiftedHeight = dot(tex2D(BaseTexture, uv + BumpParams.xy).rgb, float3(0.299, 0.587, 0.114));\n"
        "    float bumpScale = 1.0 + (shiftedHeight - height) * BumpParams.z;\n"
        "    float textureAlpha = (AlphaParams.y > 0.5) ? saturate(texel.a * 1.875) : texel.a;\n"
        "    float vertexAlpha = saturate(diffuse.a * 2.0);\n"
        "    float alpha = ((AlphaParams.x > 0.5) ? textureAlpha * vertexAlpha : 1.0) * AlphaParams.z;\n"
        "    return float4(saturate(2.0 * texel.rgb * diffuse.rgb * ColorScale.rgb * bumpScale), saturate(alpha));\n"
        "}\n";

    D3D9Graphics::Context graphics(device);
    g_texturePixelShader = graphics.CreateShader({ RendererBackend::ShaderStage::Pixel,
        kShaderSource, sizeof(kShaderSource) - 1, "DATuraFfxiTexture", "main", "ps_2_0" });
    if (!g_texturePixelShader)
        return false;
    return true;
}
}

bool ApplyTextureShader(IDirect3DDevice9* device, const bool useAuthoredAlpha,
                        const bool expandDxt3Alpha, const float opacityScale,
                        const float* colorScale, const bool hasTexture,
                        const bool enableBumpMapping, const float bumpIntensity,
                        const float baseTextureWidth, const float baseTextureHeight)
{
    if (!EnsureTexturePixelShader(device))
    {
        if (device)
            D3D9Graphics::Context(device).BindShader(RendererBackend::ShaderStage::Pixel, nullptr);
        return false;
    }

    const float alphaParams[4] =
    {
        useAuthoredAlpha ? 1.0f : 0.0f,
        expandDxt3Alpha ? 1.0f : 0.0f,
        opacityScale,
        hasTexture ? 0.0f : 1.0f
    };
    const float defaultColorScale[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float authoredColorScale[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    if (colorScale)
    {
        authoredColorScale[0] = colorScale[0];
        authoredColorScale[1] = colorScale[1];
        authoredColorScale[2] = colorScale[2];
    }
    const float bumpParams[4] =
    {
        enableBumpMapping && baseTextureWidth > 0.0f ? 1.0f / baseTextureWidth : 0.0f,
        enableBumpMapping && baseTextureHeight > 0.0f ? -1.0f / baseTextureHeight : 0.0f,
        enableBumpMapping ? bumpIntensity * 2.0f : 0.0f,
        0.0f
    };

    D3D9Graphics::Context graphics(device);
    graphics.BindShader(RendererBackend::ShaderStage::Pixel, g_texturePixelShader);
    graphics.SetShaderConstants(RendererBackend::ShaderStage::Pixel, 0, alphaParams, 1);
    graphics.SetShaderConstants(RendererBackend::ShaderStage::Pixel, 1,
        colorScale ? authoredColorScale : defaultColorScale, 1);
    graphics.SetShaderConstants(RendererBackend::ShaderStage::Pixel, 2, bumpParams, 1);
    return true;
}

void Release()
{
    if (g_texturePixelShader)
    {
        delete g_texturePixelShader;
        g_texturePixelShader = nullptr;
    }
    g_texturePixelShaderTried = false;
}
}
