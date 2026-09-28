#include "stdafx.h"
#include "custom_texture_assets.h"
#include "noesis_rapi.h"

#include <wincodec.h>
#include <algorithm>
#include <filesystem>
#include <map>
#include <vector>

#pragma comment(lib, "windowscodecs.lib")

namespace CustomTextureAssets
{
namespace
{
struct CachedTexture
{
    IDirect3DTexture9* texture = nullptr;
    int width = 0;
    int height = 0;
};

IDirect3DDevice9* g_device = nullptr;
bool g_hdEnabled = false;
bool g_pbrEnabled = false;
std::string g_hdFolder;
std::string g_pbrFolder;
std::map<std::string, CachedTexture> g_cache;
IDirect3DPixelShader9* g_pbrShader = nullptr;
bool g_pbrShaderTried = false;

void ClearCache()
{
    for (auto& item : g_cache)
        if (item.second.texture) item.second.texture->Release();
    g_cache.clear();
}

std::string TextureStem(const noesisTex_t* texture)
{
    if (!texture || !texture->name || !*texture->name) return {};
    std::string stem = std::filesystem::path(texture->name).stem().string();
    std::transform(stem.begin(), stem.end(), stem.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return stem;
}

std::filesystem::path FindFile(const std::string& folder, const std::string& stem,
                               const char* suffix)
{
    if (folder.empty() || stem.empty()) return {};
    for (const char* extension : {".png", ".jpg", ".jpeg", ".bmp", ".tif", ".tiff"})
    {
        const std::filesystem::path candidate =
            std::filesystem::path(folder) / (stem + suffix + extension);
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate, error)) return candidate;
    }
    return {};
}

CachedTexture Load(const std::filesystem::path& path)
{
    if (path.empty() || !g_device) return {};
    const std::string key = path.string();
    if (const auto found = g_cache.find(key); found != g_cache.end()) return found->second;
    CachedTexture result;
    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&factory))) &&
        SUCCEEDED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
            WICDecodeMetadataCacheOnLoad, &decoder)) &&
        SUCCEEDED(decoder->GetFrame(0, &frame)) &&
        SUCCEEDED(factory->CreateFormatConverter(&converter)) &&
        SUCCEEDED(converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA,
            WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
    {
        UINT width = 0, height = 0;
        converter->GetSize(&width, &height);
        if (width && height && width <= 16384 && height <= 16384)
        {
            std::vector<BYTE> pixels(static_cast<size_t>(width) * height * 4);
            if (SUCCEEDED(converter->CopyPixels(nullptr, width * 4,
                    static_cast<UINT>(pixels.size()), pixels.data())))
            {
                IDirect3DTexture9* texture = nullptr;
                if (SUCCEEDED(g_device->CreateTexture(width, height, 0, D3DUSAGE_AUTOGENMIPMAP,
                        D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, nullptr)))
                {
                    D3DLOCKED_RECT lock = {};
                    if (SUCCEEDED(texture->LockRect(0, &lock, nullptr, 0)))
                    {
                        for (UINT y = 0; y < height; ++y)
                            memcpy(static_cast<BYTE*>(lock.pBits) + y * lock.Pitch,
                                pixels.data() + static_cast<size_t>(y) * width * 4, width * 4);
                        texture->UnlockRect(0);
                        texture->GenerateMipSubLevels();
                        result = {texture, static_cast<int>(width), static_cast<int>(height)};
                    }
                    else texture->Release();
                }
            }
        }
    }
    if (converter) converter->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (factory) factory->Release();
    g_cache.emplace(key, result);
    return result;
}

bool EnsurePbrShader(IDirect3DDevice9* device)
{
    if (g_pbrShader) return true;
    if (g_pbrShaderTried || !device) return false;
    g_pbrShaderTried = true;
    static const char source[] =
        "sampler2D Albedo:register(s0); sampler2D NormalMap:register(s1);\n"
        "sampler2D RoughnessMap:register(s2); sampler2D MetallicMap:register(s3);\n"
        "float4 AlphaParams:register(c0);\n"
        "float4 main(float4 diffuse:COLOR0,float2 uv:TEXCOORD0):COLOR0 {\n"
        " float4 a=tex2D(Albedo,uv); float3 n=normalize(tex2D(NormalMap,uv).xyz*2-1);\n"
        " float rough=saturate(tex2D(RoughnessMap,uv).r); float metal=saturate(tex2D(MetallicMap,uv).r);\n"
        " float3 l=normalize(float3(-.45,.6,.66)); float3 v=float3(0,0,1); float3 h=normalize(l+v);\n"
        " float nl=max(dot(n,l),.001); float nv=max(dot(n,v),.001); float nh=max(dot(n,h),.001); float vh=max(dot(v,h),.001);\n"
        " float r=max(rough,.045); float r2=r*r; float r4=r2*r2; float d0=nh*nh*(r4-1)+1;\n"
        " float D=r4/(3.14159265*d0*d0); float k=(r+1)*(r+1)/8;\n"
        " float G=(nv/(nv*(1-k)+k))*(nl/(nl*(1-k)+k));\n"
        " float3 f0=lerp(float3(.04,.04,.04),a.rgb,metal); float3 F=f0+(1-f0)*pow(1-vh,5);\n"
        " float3 spec=D*G*F/max(4*nv*nl,.001); float3 kd=(1-F)*(1-metal);\n"
        " float3 rgb=(a.rgb*.18+((kd*a.rgb/3.14159265)+spec)*nl)*diffuse.rgb*2;\n"
        " float ta=(AlphaParams.y>.5)?saturate(a.a*1.875):a.a;\n"
        " float alpha=((AlphaParams.x>.5)?ta*saturate(diffuse.a*2):1)*AlphaParams.z;\n"
        " return float4(saturate(rgb),saturate(alpha)); }\n";
    ID3DBlob* code = nullptr; ID3DBlob* errors = nullptr;
    const HRESULT compiled = D3DCompile(source, sizeof(source) - 1, "DATuraPBR", nullptr,
        nullptr, "main", "ps_3_0", 0, 0, &code, &errors);
    if (FAILED(compiled))
    {
        if (errors) { OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer())); errors->Release(); }
        return false;
    }
    const HRESULT created = device->CreatePixelShader(
        static_cast<const DWORD*>(code->GetBufferPointer()), &g_pbrShader);
    code->Release(); if (errors) errors->Release();
    return SUCCEEDED(created);
}
}

void Configure(IDirect3DDevice9* device, const bool enableHdTextures,
               const std::string& hdFolder, const bool enablePbr,
               const std::string& pbrFolder)
{
    if (g_device != device || g_hdEnabled != enableHdTextures || g_pbrEnabled != enablePbr ||
        g_hdFolder != hdFolder || g_pbrFolder != pbrFolder)
    {
        ClearCache();
        g_device = device; g_hdEnabled = enableHdTextures; g_hdFolder = hdFolder;
        g_pbrEnabled = enablePbr; g_pbrFolder = pbrFolder;
    }
}

Texture ResolveBase(const noesisTex_t* original)
{
    const std::string stem = TextureStem(original);
    CachedTexture loaded;
    if (g_pbrEnabled) loaded = Load(FindFile(g_pbrFolder, stem, "_albedo"));
    if (!loaded.texture && g_hdEnabled) loaded = Load(FindFile(g_hdFolder, stem, ""));
    return {loaded.texture, loaded.width, loaded.height};
}

bool SupportsPbr(IDirect3DDevice9* device)
{
    return EnsurePbrShader(device);
}

bool ApplyPbrShader(IDirect3DDevice9* device, const noesisTex_t* original,
                    IDirect3DTexture9* baseTexture, const bool useAuthoredAlpha,
                    const bool expandDxt3Alpha, const float opacityScale)
{
    if (!g_pbrEnabled || !baseTexture || !EnsurePbrShader(device)) return false;
    const std::string stem = TextureStem(original);
    const CachedTexture normal = Load(FindFile(g_pbrFolder, stem, "_normal"));
    const CachedTexture roughness = Load(FindFile(g_pbrFolder, stem, "_roughness"));
    const CachedTexture metallic = Load(FindFile(g_pbrFolder, stem, "_metallic"));
    if (!normal.texture || !roughness.texture || !metallic.texture) return false;
    const float alpha[4] = {useAuthoredAlpha ? 1.0f : 0.0f,
        expandDxt3Alpha ? 1.0f : 0.0f, opacityScale, 0.0f};
    device->SetTexture(0, baseTexture); device->SetTexture(1, normal.texture);
    device->SetTexture(2, roughness.texture); device->SetTexture(3, metallic.texture);
    device->SetPixelShader(g_pbrShader); device->SetPixelShaderConstantF(0, alpha, 1);
    return true;
}

void Release()
{
    ClearCache();
    if (g_pbrShader) { g_pbrShader->Release(); g_pbrShader = nullptr; }
    g_pbrShaderTried = false; g_device = nullptr;
}
}
