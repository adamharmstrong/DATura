#include "d3d9_graphics.h"
#include "stdafx.h"
#include "noesis_rapi.h"
#include "model_ff11.h"
#include "model_renderer.h"
#include "d3d_model_render_state.h"
#include "custom_texture_assets.h"
#include "d3d_math.h"
#include "d3d_model_buffers.h"
#include "zone_model_render_metadata.h"
#include "zone_environment_fog.h"
#include "zone_environment_state.h"
#include "zone_sky_dome.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <vector>

namespace
{

void ApplySharedOpaque(IDirect3DDevice9* device, RendererBackend::MaterialBindingCache& cache,
    const noesisMaterial_t* material, const noesisTex_t* texture, bool bump = false, float intensity = 1.0f)
{
    D3DModelRenderState::Materials implementation(device);
    RendererBackend::Materials& materials = implementation;
    materials.ApplyOpaque(cache, material, texture, bump, intensity);
}
void ApplySharedTransparent(IDirect3DDevice9* device, RendererBackend::MaterialBindingCache& cache,
    const noesisMaterial_t* material, const noesisTex_t* texture, bool bump = false, float intensity = 1.0f)
{
    D3DModelRenderState::Materials implementation(device);
    RendererBackend::Materials& materials = implementation;
    materials.ApplyTransparent(cache, material, texture, bump, intensity);
}

constexpr UINT size = 128;
constexpr DWORD background = 0xff14283c; // RGB 20,40,60: unequal channels reveal swizzles.
int failures = 0, assertions = 0;
using Capture = std::vector<DWORD>;
void Check(bool condition, const char* description)
{
    ++assertions;
    if (!condition) { ++failures; std::cerr << "FAIL: " << description << '\n'; }
}
template<class T> struct ComPtr
{
    T* ptr = nullptr;
    ~ComPtr() { if (ptr) ptr->Release(); }
    ComPtr() = default;
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
    T* operator->() const { return ptr; }
    explicit operator bool() const { return ptr != nullptr; }
};
struct Device
{
    HWND window = nullptr;
    ComPtr<IDirect3D9> d3d;
    ComPtr<IDirect3DDevice9> gpu;
    bool Create(bool hardware=false, D3DMULTISAMPLE_TYPE samples=D3DMULTISAMPLE_NONE, UINT resolution=size)
    {
        window = CreateWindowExA(0, "STATIC", "DATura material validation", WS_POPUP,
            0, 0, size, size, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
        d3d.ptr = Direct3DCreate9(D3D_SDK_VERSION);
        if (!window || !d3d) return false;
        D3DPRESENT_PARAMETERS pp = {};
        pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        pp.hDeviceWindow = window; pp.BackBufferWidth = pp.BackBufferHeight = resolution;
        pp.BackBufferFormat = D3DFMT_X8R8G8B8;
        pp.MultiSampleType=samples;
        pp.EnableAutoDepthStencil = TRUE; pp.AutoDepthStencilFormat = D3DFMT_D24S8;
        pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        return SUCCEEDED(d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
            hardware ? D3DCREATE_HARDWARE_VERTEXPROCESSING : D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &gpu.ptr));
    }
    ~Device()
    {
        D3DModelRenderState::ReleaseFfxiPixelShaders();
        if (gpu) { gpu->Release(); gpu.ptr = nullptr; }
        if (window) DestroyWindow(window);
    }
};
Capture Readback(IDirect3DDevice9* device, const std::filesystem::path& output)
{
    ComPtr<IDirect3DSurface9> target, staging, resolved;
    if (FAILED(device->GetRenderTarget(0, &target.ptr)))
    { Check(false,"get native D3D9 target"); return {}; }
    D3DSURFACE_DESC description={}; target->GetDesc(&description);
    if(description.MultiSampleType!=D3DMULTISAMPLE_NONE)
    {
        if(FAILED(device->CreateRenderTarget(size,size,description.Format,D3DMULTISAMPLE_NONE,
            0,FALSE,&resolved.ptr,nullptr)) ||
            FAILED(device->StretchRect(target.ptr,nullptr,resolved.ptr,nullptr,D3DTEXF_NONE)))
        { Check(false,"resolve multisampled D3D9 target"); return {}; }
    }
    if (
        FAILED(device->CreateOffscreenPlainSurface(description.Width, description.Height, D3DFMT_X8R8G8B8,
            D3DPOOL_SYSTEMMEM, &staging.ptr, nullptr)) ||
        FAILED(device->GetRenderTargetData(resolved ? resolved.ptr : target.ptr, staging.ptr)))
    { Check(false, "read back native D3D9 target"); return {}; }
    D3DLOCKED_RECT lock = {};
    if (FAILED(staging->LockRect(&lock, nullptr, D3DLOCK_READONLY)))
    { Check(false, "lock D3D9 target readback"); return {}; }
    Capture pixels(description.Width * description.Height);
    for (UINT row = 0; row < description.Height; ++row)
        memcpy(pixels.data() + row * description.Width, static_cast<char*>(lock.pBits) + row * lock.Pitch,
            description.Width * sizeof(DWORD));
    staging->UnlockRect();
    if (!output.empty())
    {
        BITMAPFILEHEADER file = {};
        BITMAPINFOHEADER info = {};
        info.biSize = sizeof(info); info.biWidth = description.Width; info.biHeight = -LONG(description.Height);
        info.biPlanes = 1; info.biBitCount = 32;
        file.bfType = 0x4d42; file.bfOffBits = sizeof(file) + sizeof(info);
        file.bfSize = file.bfOffBits + static_cast<DWORD>(pixels.size() * sizeof(DWORD));
        std::ofstream stream(output, std::ios::binary);
        stream.write(reinterpret_cast<const char*>(&file), sizeof(file));
        stream.write(reinterpret_cast<const char*>(&info), sizeof(info));
        stream.write(reinterpret_cast<const char*>(pixels.data()), pixels.size() * sizeof(DWORD));
        Check(stream.good(), "write material validation BMP");
    }
    return pixels;
}
// Independent, literal expected RGB values; allow two byte values for GPU quantization.
void Expect(const Capture& pixels, int x, int y, std::array<int, 3> rgb,
    const char* description, int tolerance = 2)
{
    if (pixels.size() != size * size) { Check(false, description); return; }
    // A 5x5 patch, away from triangle and texture edges, prevents one coincidental pixel passing.
    bool good = true;
    for (int dy = -2; dy <= 2; ++dy) for (int dx = -2; dx <= 2; ++dx)
    {
        DWORD value = pixels[(y + dy) * size + x + dx];
        for (int channel = 0; channel < 3; ++channel)
            good &= std::abs(int((value >> (16 - channel * 8)) & 255) - rgb[channel]) <= tolerance;
    }
    Check(good, description);
    DWORD sample = pixels[y * size + x];
    std::cout << (good ? "PASS: " : "FAIL: ") << description << " RGB=("
        << ((sample >> 16) & 255) << ',' << ((sample >> 8) & 255) << ',' << (sample & 255)
        << ") expected=(" << rgb[0] << ',' << rgb[1] << ',' << rgb[2] << ")\n";
}

noesisModel_t::Submesh Quad(const char* material, float left, float right,
    float z, DWORD color)
{
    noesisModel_t::Submesh mesh;
    mesh.objectName = material; mesh.materialName = material;
    // Coordinates are expressed in NDC then expanded by eye Z for a 90-degree perspective.
    for (const auto& point : {std::array<float, 4>{left, -.85f, 0, 1},
        std::array<float, 4>{right, -.85f, 1, 1},
        std::array<float, 4>{right, .85f, 1, 0},
        std::array<float, 4>{left, .85f, 0, 0}})
    {
        FFXIVertex vertex = {};
        vertex.pos[0] = point[0] * z; vertex.pos[1] = point[1] * z; vertex.pos[2] = z;
        vertex.nrm[2] = -1; vertex.diffuse = color;
        vertex.uv[0] = point[2]; vertex.uv[1] = point[3];
        mesh.cpuVerts.push_back(vertex);
    }
    mesh.cpuIndices = {0, 1, 2, 0, 2, 3}; mesh.vertCount = 4; mesh.triCount = 2;
    return mesh;
}
void ImmediateQuad(IDirect3DDevice9* device, float left, float right, float z, DWORD color)
{
    auto mesh = Quad("immediate", left, right, z, color);
    Check(SUCCEEDED(device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, 4, 2,
        mesh.cpuIndices.data(), D3DFMT_INDEX32, mesh.cpuVerts.data(), sizeof(FFXIVertex))),
        "draw material fixture");
}
void Prepare(IDirect3DDevice9* device)
{
    const auto identity = D3DMath::BuildIdentity();
    const auto projection = D3DMath::BuildPerspectiveFovLH(1.5707963268f, 1, .1f, 20);
    device->SetTransform(D3DTS_VIEW, &identity); device->SetTransform(D3DTS_PROJECTION, &projection);
    ModelRenderer::Context context; context.device = device;
    ModelRenderer::PrepareFixedFunctionPass(context, identity);
    device->SetRenderState(D3DRS_DITHERENABLE, FALSE);
    device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);
    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
}
Capture Frame(IDirect3DDevice9* device, const std::function<void()>& draw,
    const std::filesystem::path& output = {})
{
    Check(SUCCEEDED(device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
        background, 1, 0)), "clear material target");
    Prepare(device);
    if (FAILED(device->BeginScene())) { Check(false, "begin material scene"); return {}; }
    draw();
    Check(SUCCEEDED(device->EndScene()), "end material scene");
    return Readback(device, output);
}
struct Texture
{
    ComPtr<IDirect3DTexture9> gpu;
    noesisTex_t info;
    std::unique_ptr<D3D9Graphics::Texture> resource;
    bool Create(IDirect3DDevice9* device, DWORD color, int dxt3Nibble = -1)
    {
        const D3DFORMAT format = dxt3Nibble >= 0 ? D3DFMT_DXT3 : D3DFMT_A8R8G8B8;
        if (FAILED(device->CreateTexture(4, 4, 1, 0, format, D3DPOOL_MANAGED, &gpu.ptr, nullptr)))
        { Check(false, "create authored-alpha fixture texture"); return false; }
        D3DLOCKED_RECT lock = {};
        if (FAILED(gpu->LockRect(0, &lock, nullptr, 0)))
        { Check(false, "lock authored-alpha fixture texture"); return false; }
        if (dxt3Nibble >= 0)
        {
            // Real 16-byte BC2/DXT3 block: 16 equal 4-bit alphas, RGB565 red endpoint 0,
            // black endpoint 1, and 16 color selectors choosing the red endpoint.
            unsigned char block[16] = {};
            memset(block, dxt3Nibble | (dxt3Nibble << 4), 8);
            block[8] = 0x00; block[9] = 0xf8;
            memcpy(lock.pBits, block, sizeof(block));
        }
        else for (int y = 0; y < 4; ++y)
        {
            DWORD* row = reinterpret_cast<DWORD*>(static_cast<char*>(lock.pBits) + y * lock.Pitch);
            for (int x = 0; x < 4; ++x) row[x] = color;
        }
        gpu->UnlockRect(0); gpu->AddRef(); resource = std::make_unique<D3D9Graphics::Texture>(gpu.ptr); info.gpuTexture = resource.get();
        info.w = 4; info.h = 4;
        info.texType = dxt3Nibble >= 0 ? NOESISTEX_DXT3 : NOESISTEX_RGBA32;
        return true;
    }
    bool CreateRamp(IDirect3DDevice9* device)
    {
        if (FAILED(device->CreateTexture(4, 4, 1, 0, D3DFMT_A8R8G8B8,
            D3DPOOL_MANAGED, &gpu.ptr, nullptr)))
        { Check(false, "create bump-map ramp texture"); return false; }
        D3DLOCKED_RECT lock = {};
        if (FAILED(gpu->LockRect(0, &lock, nullptr, 0)))
        { Check(false, "lock bump-map ramp texture"); return false; }
        const BYTE levels[4] = { 64, 128, 192, 255 };
        for (int y = 0; y < 4; ++y)
        {
            DWORD* row = reinterpret_cast<DWORD*>(static_cast<char*>(lock.pBits) + y * lock.Pitch);
            for (int x = 0; x < 4; ++x)
                row[x] = D3DCOLOR_ARGB(255, levels[x], levels[x], levels[x]);
        }
        gpu->UnlockRect(0);
        gpu->AddRef(); resource = std::make_unique<D3D9Graphics::Texture>(gpu.ptr); info.gpuTexture = resource.get(); info.w = 4; info.h = 4; info.texType = NOESISTEX_RGBA32;
        return true;
    }
};
noesisMaterial_t Material(const char* name, bool opaque, float cutoff = 0)
{
    noesisMaterial_t material;
    material.name = const_cast<char*>(name); material.flags = NMATFLAG_TWOSIDED;
    material.noDefaultBlend = opaque; material.alphaTest = cutoff;
    return material;
}
void BlendState(IDirect3DDevice9* device)
{
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
}

void TestAlpha(IDirect3DDevice9* device, const std::filesystem::path& output)
{
    Texture transparent, quarter, below, above, dxtHalf, dxtFull;
    if (!transparent.Create(device, 0x004080c0) || !quarter.Create(device, 0x404080c0) ||
        !below.Create(device, 0x7f4080c0) || !above.Create(device, 0x804080c0) ||
        !dxtHalf.Create(device, 0, 4) || !dxtFull.Create(device, 0, 8)) return;
    auto opaque = Material("opaque", true), cutout = Material("cutout", true, .5f);
    auto soft = Material("soft", false);
    auto draw = [&](noesisMaterial_t& material, Texture& texture, DWORD diffuse, bool blended)
    {
        D3DModelRenderState::MaterialBindingCache cache;
        if (blended) { BlendState(device); ApplySharedTransparent(device, cache, &material, &texture.info); }
        else ApplySharedOpaque(device, cache, &material, &texture.info);
        ImmediateQuad(device, -.85f, .85f, 2, diffuse);
    };
    auto pixels = Frame(device, [&] { draw(opaque, transparent, 0x00808080, false); }, output / "opaque-zero-alpha.bmp");
    Expect(pixels, 64, 64, {64,129,193}, "opaque ignores both stored zero alpha channels");
    pixels = Frame(device, [&] { draw(cutout, below, 0x80808080, false); });
    Expect(pixels, 64, 64, {20,40,60}, "hard cutout rejects alpha equal to integer reference 127");
    pixels = Frame(device, [&] { draw(cutout, above, 0x80808080, false); }, output / "cutout-above-threshold.bmp");
    Expect(pixels, 64, 64, {64,129,193}, "hard cutout accepts alpha 128 and writes full color");
    pixels = Frame(device, [&] { draw(cutout, above, 0x40808080, false); });
    Expect(pixels, 64, 64, {20,40,60}, "hard cutout includes expanded authored vertex alpha");
    pixels = Frame(device, [&] { draw(soft, quarter, 0x40808080, true); }, output / "softblend-quarter-texture-half-vertex.bmp");
    Expect(pixels, 64, 64, {26,51,77}, "soft blend multiplies texture alpha by expanded vertex alpha");
    pixels = Frame(device, [&] { draw(soft, dxtHalf, 0x80808080, true); }, output / "dxt3-half-alpha.bmp");
    Expect(pixels, 64, 64, {138,20,30}, "real DXT3 nibble four expands to one-half opacity");
    pixels = Frame(device, [&] { draw(soft, dxtHalf, 0x40808080, true); });
    Expect(pixels, 64, 64, {79,30,45}, "DXT3 expansion preserves separate vertex alpha multiplication");
    pixels = Frame(device, [&] { draw(soft, dxtFull, 0x80808080, true); });
    Expect(pixels, 64, 64, {255,0,0}, "DXT3 nibble eight reaches opaque after expansion");
    pixels = Frame(device, [&] { draw(opaque, quarter, 0x80ffffff, false); });
    Expect(pixels, 64, 64, {128,255,255}, "MODULATE2X RGB clamps saturated channels independently");
}

void TestUiShader(IDirect3DDevice9* device)
{
    Texture texture;
    if (!texture.Create(device, 0, 4)) return;
    const auto draw = [&](float maximumOpacity)
    {
        BlendState(device);
        device->SetTexture(0, texture.gpu.ptr);
        Check(D3DModelRenderState::SetFfxiUiPixelShader(device, true, 1.0f, maximumOpacity),
            "shared UI shader compiles and binds");
        ImmediateQuad(device, -.85f, .85f, 2, 0xffffffff);
    };
    auto pixels = Frame(device, [&] { draw(1.0f); });
    Expect(pixels, 64, 64, {138,20,30}, "shared UI shader preserves DXT3 half opacity");
    pixels = Frame(device, [&] { draw(.25f); });
    Expect(pixels, 64, 64, {79,30,45}, "shared UI shader preserves maximum opacity clamp");
}

void TestTextureTransitions(IDirect3DDevice9* device, const std::filesystem::path& output)
{
    Texture texture;
    if (!texture.Create(device, 0xff4080c0)) return;
    for (bool transparent : {false, true})
    {
        auto textured = Material("textured", !transparent), plain = Material("plain", !transparent);
        auto pixels = Frame(device, [&]
        {
            D3DModelRenderState::MaterialBindingCache cache;
            if (transparent) BlendState(device);
            auto bind = [&](noesisMaterial_t* material, noesisTex_t* tex)
            {
                if (transparent) ApplySharedTransparent(device, cache, material, tex);
                else ApplySharedOpaque(device, cache, material, tex);
            };
            bind(&textured, &texture.info);
            ImmediateQuad(device, -.9f, -.1f, 2, 0x80808080);
            bind(&plain, nullptr);
            ComPtr<IDirect3DPixelShader9> shader;
            device->GetPixelShader(&shader.ptr);
            Check(!shader, "untextured material clears the prior sampling shader");
            ImmediateQuad(device, .1f, .9f, 2, 0x8090c040);
        }, output / (transparent ? "transparent-texture-transition.bmp" : "opaque-texture-transition.bmp"));
        Expect(pixels, 32, 64, {64,129,193}, "textured side of material transition retains its own color");
        Expect(pixels, 96, 64, transparent ? std::array<int,3>{82,116,62} : std::array<int,3>{144,192,64},
            transparent ? "untextured transparency survives a textured predecessor" : "untextured color survives a textured predecessor");
    }
}

void TestClassicBumpMapping(IDirect3DDevice9* device, const std::filesystem::path& output)
{
    Texture ramp;
    if (!ramp.CreateRamp(device)) return;
    auto opaque = Material("bump-ramp", true);
    const auto draw = [&](const bool enabled)
    {
        D3DModelRenderState::MaterialBindingCache cache;
        ApplySharedOpaque(
            device, cache, &opaque, &ramp.info, enabled, 1.0f);
        ImmediateQuad(device, -.85f, .85f, 2, 0x80808080);
    };
    const Capture flat = Frame(device, [&] { draw(false); });
    const Capture bumped = Frame(device, [&] { draw(true); }, output / "classic-bump-ramp.bmp");
    const int flatRed = static_cast<int>((flat[64 * size + 64] >> 16) & 255);
    const int bumpedRed = static_cast<int>((bumped[64 * size + 64] >> 16) & 255);
    Check(bumpedRed - flatRed >= 40,
        "classic bump mapping embosses diffuse height differences without a normal map");
}

struct Model
{
    noesisModel_t value;
    ~Model() { value.ReleaseD3DBuffers(); }
    void Prepare(IDirect3DDevice9* device, noesisMatData_t* materials)
    {
        value.pMatData = materials; value.BuildD3DBuffers(device);
        ZoneModelRenderMetadata::Prepare(&value, device);
    }
};
void CheckBaseline(IDirect3DDevice9* device)
{
    for (const auto& expected : {std::pair<D3DRENDERSTATETYPE,DWORD>{D3DRS_ALPHABLENDENABLE,FALSE},
        {D3DRS_ALPHATESTENABLE,FALSE}, {D3DRS_ZWRITEENABLE,TRUE}, {D3DRS_ZFUNC,D3DCMP_LESS},
        {D3DRS_DEPTHBIAS,0}, {D3DRS_SLOPESCALEDEPTHBIAS,0}, {D3DRS_BLENDOP,D3DBLENDOP_ADD}})
    {
        DWORD actual = ~expected.second;
        device->GetRenderState(expected.first, &actual);
        Check(actual == expected.second, "geometry pass restores material/depth baseline");
    }
    ComPtr<IDirect3DPixelShader9> shader;
    ComPtr<IDirect3DBaseTexture9> texture;
    device->GetPixelShader(&shader.ptr); device->GetTexture(0, &texture.ptr);
    Check(!shader && !texture, "geometry pass releases material shader and texture binding");
}
void RenderModel(IDirect3DDevice9* device, Model& model, ModelRenderer::GeometryPass pass)
{
    ModelRenderer::Context context; context.device = device; context.geometryPass = pass;
    ModelRenderer::DrawGeometry(context, &model.value, D3DMath::BuildIdentity());
    CheckBaseline(device);
}
void TestDepthOrdering(IDirect3DDevice9* device, const std::filesystem::path& output)
{
    auto opaque = Material("opaque", true), soft = Material("soft", false);
    noesisMaterial_t* mats[] = {&opaque, &soft};
    noesisMatData_t data; data.mats = mats; data.matCount = 2;
    Model model;
    model.value.submeshes.push_back(Quad("soft", -.9f,.9f,2,0x80ff0000)); // intentionally near first
    model.value.submeshes.push_back(Quad("soft", -.9f,.9f,4,0x800000ff));
    model.value.submeshes.push_back(Quad("opaque", -.9f,.9f,6,0xff00ff00));
    model.value.submeshes.push_back(Quad("opaque", -.9f,-.1f,1,0xffd0a020));
    model.Prepare(device, &data);
    auto pixels = Frame(device, [&] { RenderModel(device, model, ModelRenderer::GeometryPass::All); }, output / "depth-transparent-order.bmp");
    Expect(pixels, 32, 64, {208,160,32}, "opaque foreground blocks every transparent layer");
    Expect(pixels, 96, 64, {128,63,64}, "transparent geometry sorts far to near over opaque background");
    Model behind;
    behind.value.submeshes.push_back(Quad("opaque", .1f,.9f,5,0xffffff00)); behind.Prepare(device, &data);
    pixels = Frame(device, [&]
    {
        RenderModel(device, model, ModelRenderer::GeometryPass::All);
        RenderModel(device, behind, ModelRenderer::GeometryPass::Opaque);
    });
    Expect(pixels, 96, 64, {255,255,0}, "transparent pass leaves depth writable for a later opaque surface behind it");
    Expect(pixels, 32, 64, {208,160,32}, "opaque depth remains intact across draw calls");
    Model hidden;
    hidden.value.submeshes.push_back(Quad("opaque", -.9f,.9f,8,0xffff00ff)); hidden.Prepare(device, &data);
    pixels = Frame(device, [&]
    {
        RenderModel(device, model, ModelRenderer::GeometryPass::Opaque);
        RenderModel(device, hidden, ModelRenderer::GeometryPass::Opaque);
    });
    Expect(pixels, 96, 64, {0,255,0}, "opaque background writes depth and rejects later farther geometry");
}

void TestFog(IDirect3DDevice9* device, const std::filesystem::path& output)
{
    ZoneEnvironmentState::Data environment;
    environment.valid = true; environment.fogNear = 2; environment.fogFar = 6;
    environment.fogColor = 0xff2040a0;
    Texture texture;
    if (!texture.Create(device, 0xff804020)) return;
    auto opaque = Material("opaque", true);
    for (bool textured : {false, true})
    {
        auto pixels = Frame(device, [&]
        {
            ZoneEnvironmentFog::Apply(device, environment);
            D3DModelRenderState::MaterialBindingCache cache;
            ApplySharedOpaque(device, cache, &opaque, textured ? &texture.info : nullptr);
            for (int step = 0; step < 3; ++step)
                ImmediateQuad(device, -.95f + step * .65f, -.35f + step * .65f,
                    2.f + step * 2.f, textured ? 0x80808080 : 0xff804020);
        }, output / (textured ? "fog-textured-endpoints.bmp" : "fog-untextured-endpoints.bmp"));
        Expect(pixels, 20,64,{128,64,32}, textured ? "textured surface at fog start retains its color" : "untextured surface at fog start retains its color");
        Expect(pixels, 62,64,{80,64,96}, textured ? "textured surface halfway through fog interpolates RGB" : "untextured surface halfway through fog interpolates RGB");
        Expect(pixels, 103,64,{32,64,160}, textured ? "textured surface at fog end becomes fog color" : "untextured surface at fog end becomes fog color");
    }
    for (int invalid = 0; invalid < 7; ++invalid)
    {
        auto value = environment;
        if (invalid == 0) value.valid = false;
        if (invalid == 1) value.fogFar = 0;
        if (invalid == 2) value.fogFar = value.fogNear;
        if (invalid == 3) value.fogFar = std::numeric_limits<float>::quiet_NaN();
        if (invalid == 4) value.fogFar = std::numeric_limits<float>::infinity();
        if (invalid == 5) value.fogNear = -std::numeric_limits<float>::infinity();
        if (invalid == 6) { value.fogNear = -2; value.fogFar = 0; }
        auto pixels = Frame(device, [&]
        {
            ZoneEnvironmentFog::Apply(device, environment);
            ZoneEnvironmentFog::Apply(device, value);
            D3DModelRenderState::MaterialBindingCache cache;
            ApplySharedOpaque(device, cache, &opaque, nullptr);
            ImmediateQuad(device, -.85f,.85f,8,0xff804020);
        });
        Expect(pixels,64,64,{128,64,32}, "disabled/invalid fog cannot retain previous weather fog");
        pixels = Frame(device, [&]
        {
            ModelRenderer::Context actor;
            actor.device = device;
            actor.useAuthoredFog = value.valid;
            actor.authoredFogColor = value.fogColor;
            actor.authoredFogNear = value.fogNear;
            actor.authoredFogFar = value.fogFar;
            ModelRenderer::PrepareFixedFunctionPass(actor, D3DMath::BuildIdentity());
            D3DModelRenderState::MaterialBindingCache cache;
            ApplySharedOpaque(device, cache, &opaque, nullptr);
            ImmediateQuad(device, -.85f,.85f,8,0xff804020);
        });
        Expect(pixels,64,64,{128,64,32}, "actor pass rejects disabled and nonfinite zone fog");
    }
    for (bool inWorld : {true, false})
    {
        auto actorPixels = Frame(device, [&]
        {
            ModelRenderer::Context actor;
            actor.device = device;
            actor.useAuthoredFog = inWorld;
            actor.authoredFogColor = environment.fogColor;
            actor.authoredFogNear = environment.fogNear;
            actor.authoredFogFar = environment.fogFar;
            ModelRenderer::PrepareFixedFunctionPass(actor, D3DMath::BuildIdentity());
            D3DModelRenderState::MaterialBindingCache cache;
            ApplySharedOpaque(device, cache, &opaque, nullptr);
            ImmediateQuad(device, -.85f,.85f,8,0xff804020);
        }, output / (inWorld ? "fog-world-actor.bmp" : "fog-preview-actor.bmp"));
        Expect(actorPixels,64,64,inWorld ? std::array<int,3>{32,64,160} : std::array<int,3>{128,64,32},
            inWorld ? "world actor beyond fog end shares zone atmosphere" : "preview actor clears zone atmosphere");
    }
    auto pixels = Frame(device, [&]
    {
        ZoneEnvironmentFog::Apply(device, environment);
        Prepare(device); // Exactly the standalone-model pass boundary used by main.cpp.
        D3DModelRenderState::MaterialBindingCache cache;
        ApplySharedOpaque(device, cache, &opaque, nullptr);
        ImmediateQuad(device,-.85f,.85f,8,0xff804020);
    });
    Expect(pixels,64,64,{128,64,32}, "standalone-model preparation clears prior zone fog");
    const float elevations[] = {-.1f, .4f, 1};
    const DWORD colors[] = {0xff804020,0xff804020,0xff804020};
    pixels = Frame(device, [&]
    {
        ZoneEnvironmentFog::Apply(device, environment);
        ZoneSkyDome::Parameters sky;
        sky.environmentValid = true; sky.radius = 15; sky.spokeCount = 32; sky.ringCount = 3;
        sky.ringElevations = elevations; sky.ringColors = colors;
        ZoneSkyDome::Draw(device, sky,0,0,0,20);
        DWORD fog = TRUE; device->GetRenderState(D3DRS_FOGENABLE,&fog);
        Check(fog == FALSE, "sky dome explicitly excludes landscape fog");
    }, output / "sky-fog-exclusion.bmp");
    Expect(pixels,64,88,{128,64,32}, "sky dome beyond fog end retains authored color");
}

void TestDirectionalShadows(IDirect3DDevice9* device, const std::filesystem::path& output)
{
    std::filesystem::create_directories(output);
    auto opaque = Material("shadow-fixture",true);
    noesisMaterial_t* materials[] = {&opaque};
    noesisMatData_t data = {}; data.mats = materials; data.matCount = 1;
    Model terrain, actor;
    terrain.value.submeshes.push_back(Quad("shadow-fixture",-.85f,.85f,8,0xffffffff));
    actor.value.submeshes.push_back(Quad("shadow-fixture",-.2f,.2f,4,0xffffffff));
    terrain.Prepare(device,&data); actor.Prepare(device,&data);
    const auto identity = D3DMath::BuildIdentity();
    ModelRenderer::Context context; context.device=device;
    context.lightingQuality=2; context.dynamicObjectShadows=true; context.dynamicActorShadows=true;
    context.rendersZoneObjects=true; context.geometryPass=ModelRenderer::GeometryPass::Opaque;
    context.useAuthoredLightDirection=true;
    context.authoredMainLightDirection[0]=.6f; context.authoredMainLightDirection[2]=.8f;
    context.shadowMaxDistance=16; context.shadowOpacity=.5f;
    const auto draw = [&](bool shadow, bool castActor, bool changeSharedPose=false)
    {
        ModelRenderer::BeginDirectionalShadowFrame();
        ModelRenderer::DrawGeometry(context,&terrain.value,identity);
        auto actorContext=context; actorContext.rendersZoneObjects=false;
        actorContext.dynamicActorShadows=castActor;
        ModelRenderer::DrawGeometry(actorContext,&actor.value,identity);
        const auto pose=actor.value.submeshes[0].cpuVerts;
        if(changeSharedPose)
        {
            for(auto& vertex:actor.value.submeshes[0].cpuVerts) vertex.pos[0]+=100;
            D3DModelBuffers::UploadVertices(actor.value.submeshes[0]);
        }
        if(shadow) Check(ModelRenderer::DrawDirectionalShadowFrame(),"render sunlight depth and receiver passes");
        else ModelRenderer::BeginDirectionalShadowFrame();
        if(changeSharedPose)
        {
            actor.value.submeshes[0].cpuVerts=pose;
            D3DModelBuffers::UploadVertices(actor.value.submeshes[0]);
        }
        CheckBaseline(device);
    };
    const auto baseline=Frame(device,[&]{draw(false,true);},output/"sun-shadow-disabled.bmp");
    const auto shadow=Frame(device,[&]{draw(true,true);},output/"sun-shadow-actor.bmp");
    Expect(baseline,88,64,{255,255,255},"unshadowed receiver fixture");
    Expect(shadow,88,64,{128,128,128},"actor blocks oblique sunlight on actual receiver",3);
    Expect(shadow,25,64,{255,255,255},"unoccluded receiver stays bright");
    for(auto& vertex:actor.value.submeshes[0].cpuVerts) vertex.diffuse=0xffff0000;
    D3DModelBuffers::UploadVertices(actor.value.submeshes[0]);
    const auto casterOnly=Frame(device,[&]
    {
        ModelRenderer::BeginDirectionalShadowFrame();
        ModelRenderer::DrawGeometry(context,&terrain.value,identity);
        auto actorContext=context; actorContext.rendersZoneObjects=false;
        ModelRenderer::SubmitDirectionalShadowCaster(actorContext,&actor.value,identity);
        Check(ModelRenderer::DrawDirectionalShadowFrame(),"render camera-hidden actor as a shadow-only caster");
        CheckBaseline(device);
    },output/"sun-shadow-camera-hidden-actor.bmp");
    Expect(casterOnly,88,64,{128,128,128},"camera-hidden actor still casts onto visible ground",3);
    Expect(casterOnly,64,64,{255,255,255},"shadow-only actor does not enter the camera color pass");
    for(auto& vertex:actor.value.submeshes[0].cpuVerts) vertex.diffuse=0xffffffff;
    D3DModelBuffers::UploadVertices(actor.value.submeshes[0]);
    // Real zone ground contains overlapping/coplanar pieces. Shadow composition
    // must not depend on how many of those pieces pass the receiver depth test.
    terrain.value.ReleaseD3DBuffers();
    terrain.value.submeshes.push_back(terrain.value.submeshes[0]);
    terrain.Prepare(device,&data);
    const auto overlapping=Frame(device,[&]{draw(true,true);},output/"sun-shadow-overlap.bmp");
    Expect(overlapping,88,64,{128,128,128},"coplanar receivers apply shadow opacity exactly once",3);
    for(int frame=0;frame<8;++frame)
    {
        context.cameraPosition[0]=frame*.004f;
        auto movingView=D3DMath::BuildRotationY(frame*.0002f);
        movingView._41=-context.cameraPosition[0];
        const auto moving=Frame(device,[&]
        {
            device->SetTransform(D3DTS_VIEW,&movingView);
            draw(true,true);
        });
        Expect(moving,88,64,{128,128,128},"overlapping receiver stays stable during small camera movements",3);
        Expect(moving,25,64,{255,255,255},"camera motion does not shadow unoccluded ground");
    }
    context.cameraPosition[0]=0;
    const auto preservedStencil=Frame(device,[&]
    {
        device->Clear(0,nullptr,D3DCLEAR_STENCIL,0,1,3);
        draw(true,true);
        device->SetRenderState(D3DRS_STENCILENABLE,TRUE);
        device->SetRenderState(D3DRS_STENCILMASK,0xff);
        device->SetRenderState(D3DRS_STENCILWRITEMASK,0);
        device->SetRenderState(D3DRS_STENCILREF,3);
        device->SetRenderState(D3DRS_STENCILFUNC,D3DCMP_EQUAL);
        device->SetRenderState(D3DRS_STENCILPASS,D3DSTENCILOP_KEEP);
        device->SetRenderState(D3DRS_STENCILFAIL,D3DSTENCILOP_KEEP);
        device->SetRenderState(D3DRS_STENCILZFAIL,D3DSTENCILOP_KEEP);
        ImmediateQuad(device,-.85f,.85f,1,0xffffff00);
        device->SetRenderState(D3DRS_STENCILENABLE,FALSE);
    });
    Expect(preservedStencil,88,64,{255,255,0},"receiver coverage preserves other stencil bits and clears its reserved bit");
    terrain.value.ReleaseD3DBuffers(); terrain.value.submeshes.pop_back(); terrain.Prepare(device,&data);
    const auto noActor=Frame(device,[&]{draw(true,false);});
    Expect(noActor,88,64,{255,255,255},"disabled actor casting removes its shadow");
    context.authoredMainLightDirection[0]=.9995f;
    context.authoredMainLightDirection[2]=.03f;
    for(int frame=0;frame<6;++frame)
    {
        auto rotatingView=D3DMath::BuildRotationY((frame-3)*.015f);
        const auto grazing=Frame(device,[&]
        {
            device->SetTransform(D3DTS_VIEW,&rotatingView);
            draw(true,false);
        });
        Expect(grazing,25,64,{255,255,255},"grazing sunlight does not create rotating self-shadow stripes",3);
    }
    context.authoredMainLightDirection[0]=.6f;
    context.authoredMainLightDirection[2]=.8f;
    context.shadowCenterValid=true; context.shadowCenter[2]=8;
    for(int frame=0;frame<9;++frame)
    {
        // Orbit from the side where the caster does not hide the sampled ground.
        const float yaw=frame*.075f;
        context.cameraPosition[0]=8*sinf(yaw);
        context.cameraPosition[2]=8-8*cosf(yaw);
        const auto orbitView=D3DMath::BuildLookAtLH(context.cameraPosition[0],0,
            context.cameraPosition[2],0,0,8);
        const auto projection=D3DMath::BuildPerspectiveFovLH(1.5707963268f,1,.1f,20);
        const auto vp=D3DMath::Multiply(orbitView,projection);
        const float clipX=3*vp._11+8*vp._31+vp._41;
        const float clipW=3*vp._14+8*vp._34+vp._44;
        const int sampleX=static_cast<int>((clipX/clipW+1)*size*.5f);
        const auto orbit=Frame(device,[&]
        {
            device->SetTransform(D3DTS_VIEW,&orbitView);
            draw(true,true);
        });
        Expect(orbit,sampleX,64,{128,128,128},"world-space shadow remains present across a wide camera orbit",3);
    }
    context.shadowCenterValid=false;
    context.cameraPosition[0]=context.cameraPosition[2]=0;
    const auto sharedPose=Frame(device,[&]{draw(true,true,true);});
    Expect(sharedPose,88,64,{128,128,128},"shared actor buffer updates preserve the submitted shadow pose",3);
    context.useAuthoredFog=true; context.authoredFogNear=0; context.authoredFogFar=1;
    const auto fogged=Frame(device,[&]{draw(true,true);});
    Expect(fogged,88,64,{255,255,255},"fully fogged receivers are not darkened by the shadow composite");
    context.useAuthoredFog=false;
    // A sloping receiver cannot be represented by the old single ground-Y plane.
    for(auto& vertex:terrain.value.submeshes[0].cpuVerts) vertex.pos[2] += vertex.pos[0]*.15f;
    terrain.value.ReleaseD3DBuffers(); terrain.Prepare(device,&data);
    const auto slope=Frame(device,[&]{draw(true,true);},output/"sun-shadow-slope.bmp");
    Expect(slope,88,64,{128,128,128},"sun shadow follows a sloping receiver",3);
    Texture transparent; Check(transparent.Create(device,0x00ffffff),"create cutout shadow fixture");
    auto cutout=Material("shadow-fixture",true,.5f); cutout.texIdx=0;
    noesisMaterial_t* actorMaterials[]={&cutout}; noesisTex_t* textures[]={&transparent.info};
    noesisMatData_t actorData={}; actorData.mats=actorMaterials; actorData.matCount=1;
    actorData.textures=textures; actorData.texCount=1;
    actor.value.ReleaseD3DBuffers(); actor.Prepare(device,&actorData);
    // Metadata is cached by production models, so update the fixture binding explicitly.
    actor.value.submeshes[0].pResolvedMaterial=&cutout;
    actor.value.submeshes[0].pResolvedTexture=&transparent.info;
    const auto holes=Frame(device,[&]{draw(true,true);},output/"sun-shadow-cutout.bmp");
    Expect(holes,88,64,{255,255,255},"transparent cutout texels do not cast solid shadows");
    actor.value.submeshes[0].pResolvedMaterial=&opaque;
    actor.value.submeshes[0].pResolvedTexture=nullptr;
    ModelRenderer::ReleaseDirectionalShadowResources();
    const auto recreated=Frame(device,[&]{draw(true,true);});
    Expect(recreated,88,64,{128,128,128},"shadow resources recreate after release",3);
    ModelRenderer::ReleaseDirectionalShadowResources();
}

void TestLayeredDirectionalShadows(IDirect3DDevice9* device, const std::filesystem::path& output)
{
    std::filesystem::create_directories(output);
    auto opaque=Material("layered-ground",true);
    noesisMaterial_t* materials[]={&opaque};
    noesisMatData_t data={}; data.mats=materials; data.matCount=1;
    const auto identity=D3DMath::BuildIdentity();
    const auto projection=D3DMath::BuildPerspectiveFovLH(1.5707963268f,1,.1f,1000);
    ModelRenderer::Context context; context.device=device;
    context.lightingQuality=2; context.dynamicObjectShadows=true; context.dynamicActorShadows=true;
    context.rendersZoneObjects=true; context.geometryPass=ModelRenderer::GeometryPass::Opaque;
    context.useAuthoredLightDirection=true;
    context.authoredMainLightDirection[0]=.6f; context.authoredMainLightDirection[2]=.8f;
    context.shadowMaxDistance=160; context.shadowOpacity=.5f;
    context.shadowCenterValid=true; context.shadowCenter[2]=400;
    // The lower surface is invisible to the camera but farther from the light.
    // At this distance its camera-depth gap is smaller than the old receiver
    // offset, so it used to claim coverage and shade the visible surface above.
    for(int order=0;order<2;++order)
    {
        Model ground, actor;
        auto visible=Quad("layered-ground",-.85f,.85f,400,0xffffffff);
        auto hidden=Quad("layered-ground",-.85f,.85f,400.5f,0xffffffff);
        ground.value.submeshes.push_back(order==0 ? hidden : visible);
        ground.value.submeshes.push_back(order==0 ? visible : hidden);
        // A real caster remains in the map: its shadow hits x=125 on z=400,
        // while the caster itself sits away from the sampled visible ground.
        actor.value.submeshes.push_back(Quad("layered-ground",15.0f/280,55.0f/280,280,0xffffffff));
        ground.Prepare(device,&data); actor.Prepare(device,&data);
        for(int frame=0;frame<5;++frame)
        {
            const float yaw=(frame-2)*.08f;
            context.cameraPosition[0]=400*sinf(yaw);
            context.cameraPosition[2]=400-400*cosf(yaw);
            const auto view=D3DMath::BuildLookAtLH(context.cameraPosition[0],0,
                context.cameraPosition[2],0,0,400);
            const auto vp=D3DMath::Multiply(view,projection);
            const auto screenX=[&](float worldX)
            {
                const float clipX=worldX*vp._11+400*vp._31+vp._41;
                const float clipW=worldX*vp._14+400*vp._34+vp._44;
                return static_cast<int>((clipX/clipW+1)*size*.5f);
            };
            const auto render=[&](bool shadows)
            {
                device->SetTransform(D3DTS_VIEW,&view);
                device->SetTransform(D3DTS_PROJECTION,&projection);
                ModelRenderer::BeginDirectionalShadowFrame();
                ModelRenderer::DrawGeometry(context,&ground.value,identity);
                auto actorContext=context; actorContext.rendersZoneObjects=false;
                ModelRenderer::DrawGeometry(actorContext,&actor.value,identity);
                if(shadows) Check(ModelRenderer::DrawDirectionalShadowFrame(),
                    "render separated ground layers with sunlight shadows");
                else ModelRenderer::BeginDirectionalShadowFrame();
                CheckBaseline(device);
            };
            const auto baseline=Frame(device,[&]{render(false);});
            const auto layered=Frame(device,[&]{render(true);},frame==2 ?
                output/(order==0 ? "sun-shadow-layer-hidden-first.bmp" :
                    "sun-shadow-layer-visible-first.bmp") : std::filesystem::path{});
            Expect(baseline,screenX(0),64,{255,255,255},"visible stacked ground is bright before shadowing");
            Expect(layered,screenX(0),64,{255,255,255},
                "hidden ground layers cannot shadow the visible surface during camera rotation",3);
            Expect(layered,screenX(125),64,{128,128,128},
                "genuine caster shadows remain on stacked ground in either layer order",3);
        }
    }
    ModelRenderer::ReleaseDirectionalShadowResources();
}

void TestTemporalShadowEdges(IDirect3DDevice9* device, const std::filesystem::path& output)
{
    std::filesystem::create_directories(output);
    auto opaque=Material("temporal-shadow",true);
    noesisMaterial_t* materials[]={&opaque};
    noesisMatData_t data={}; data.mats=materials; data.matCount=1;
    Model ground, actor;
    ground.value.submeshes.push_back(Quad("temporal-shadow",-.85f,.85f,8,0xffffffff));
    actor.value.submeshes.push_back(Quad("temporal-shadow",-.2f,.2f,4,0xffffffff));
    ground.Prepare(device,&data); actor.Prepare(device,&data);
    const auto identity=D3DMath::BuildIdentity();
    ModelRenderer::Context context; context.device=device;
    context.lightingQuality=2; context.dynamicObjectShadows=true; context.dynamicActorShadows=true;
    context.rendersZoneObjects=true; context.geometryPass=ModelRenderer::GeometryPass::Opaque;
    context.useAuthoredLightDirection=true;
    context.authoredMainLightDirection[0]=.6f; context.authoredMainLightDirection[2]=.8f;
    context.shadowMaxDistance=160; context.shadowOpacity=.5f;
    context.shadowCenterValid=true; context.shadowCenter[2]=8;
    const auto render=[&]
    {
        ModelRenderer::BeginDirectionalShadowFrame();
        ModelRenderer::DrawGeometry(context,&ground.value,identity);
        auto actorContext=context; actorContext.rendersZoneObjects=false;
        ModelRenderer::DrawGeometry(actorContext,&actor.value,identity);
        Check(ModelRenderer::DrawDirectionalShadowFrame(),"render temporal shadow edge fixture");
        CheckBaseline(device);
    };
    for(int movement=0;movement<2;++movement)
    {
        Capture previous;
        int largestChange=0, changedPixels=0;
        for(int frame=0;frame<33;++frame)
        {
            context.authoredMainLightDirection[0]=.6f+(movement==0 ? (frame-16)*.00002f : 0);
            context.shadowCenter[0]=movement==1 ? (frame-16)*.006f : 0;
            const auto capture=Frame(device,render,frame==16 ? output/
                (movement==0 ? "sun-shadow-tiny-light-motion.bmp" : "sun-shadow-subtexel-focus.bmp") :
                std::filesystem::path{});
            if(!previous.empty() && capture.size()==previous.size())
                // This region contains both filtered shadow edges, away from the
                // visible actor and terrain borders. It deliberately includes
                // partial coverage: interior-only samples miss temporal aliasing.
                for(int y=36;y<92;++y) for(int x=78;x<102;++x)
                {
                    const int index=y*size+x;
                    const int change=std::abs(int(capture[index]&255)-int(previous[index]&255));
                    largestChange=(std::max)(largestChange,change);
                    if(change>8) ++changedPixels;
                }
            previous=capture;
        }
        std::cout<<"Temporal shadow "<<(movement==0 ? "light" : "focus")
            <<" edge maximum frame change: "<<largestChange
            <<", changes above 8: "<<changedPixels<<'\n';
        Check(largestChange<=3,movement==0 ?
            "subpixel sun motion does not abruptly change filtered shadow edges" :
            "subtexel focus motion leaves world-space shadow edges stable");
    }
    ModelRenderer::ReleaseDirectionalShadowResources();
}

void TestHighResolutionShadowSlope(IDirect3DDevice9* device, UINT resolution,
    const std::filesystem::path& output)
{
    std::filesystem::create_directories(output);
    auto opaque=Material("unoccluded-close-ground",true);
    noesisMaterial_t* materials[]={&opaque};
    noesisMatData_t data={}; data.mats=materials; data.matCount=1;
    Model ground;
    ground.value.submeshes.push_back(Quad("unoccluded-close-ground",-.85f,.85f,2,0xffffffff));
    ground.Prepare(device,&data);
    const auto identity=D3DMath::BuildIdentity();
    ModelRenderer::Context context; context.device=device;
    context.lightingQuality=2; context.dynamicObjectShadows=true;
    context.rendersZoneObjects=true; context.geometryPass=ModelRenderer::GeometryPass::Opaque;
    context.useAuthoredLightDirection=true;
    context.authoredMainLightDirection[0]=1; context.authoredMainLightDirection[2]=.02f;
    context.shadowMaxDistance=160; context.shadowOpacity=.5f;
    context.shadowCenterValid=true; context.shadowCenter[2]=2;
    for(int frame=0;frame<5;++frame)
    {
        const float yaw=(frame-2)*.035f;
        context.cameraPosition[0]=2*sinf(yaw);
        context.cameraPosition[2]=2-2*cosf(yaw);
        const auto view=D3DMath::BuildLookAtLH(context.cameraPosition[0],0,
            context.cameraPosition[2],0,0,2);
        const auto capture=Frame(device,[&]
        {
            device->SetTransform(D3DTS_VIEW,&view);
            ModelRenderer::BeginDirectionalShadowFrame();
            ModelRenderer::DrawGeometry(context,&ground.value,identity);
            Check(ModelRenderer::DrawDirectionalShadowFrame(),"render full-resolution grazing-light fixture");
            CheckBaseline(device);
        },frame==2 ? output/"sun-shadow-grazing-close.bmp" : std::filesystem::path{});
        int darkest=255, unexpected=0;
        if(capture.size()==resolution*resolution)
            // With one unobstructed planar receiver, no ground pixel can be
            // legitimately occluded. A wide interior region exposes slope
            // correction that changes with the screen-space pixel footprint.
            for(UINT y=resolution/3;y<resolution*2/3;++y)
                for(UINT x=resolution/3;x<resolution*2/3;++x)
                {
                    const int value=capture[y*resolution+x]&255;
                    darkest=(std::min)(darkest,value);
                    if(value<252) ++unexpected;
                }
        else unexpected=1;
        std::cout<<"Grazing slope "<<resolution<<" frame "<<frame
            <<" darkest="<<darkest<<", false shadow pixels="<<unexpected<<'\n';
        Check(unexpected==0,"unoccluded grazing ground remains bright at full resolution during camera orbit");
    }
    ModelRenderer::ReleaseDirectionalShadowResources();
}

void TestEnvironmentInterpolation()
{
    ZoneEnvironmentState::SetTimeOverride(721);
    Check(ZoneEnvironmentState::CurrentMinuteOfDayPrecise()==721.0,
        "fractional sun clock preserves exact manual time overrides");
    ZoneEnvironmentState::SetTimeOverride(-1);
    Check(ZoneEnvironmentState::CurrentMinuteOfDayPrecise()==1439.0,
        "fractional sun clock wraps negative manual time overrides");
    ZoneEnvironmentState::ClearTimeOverride();
    float first[3]={}, quarter[3]={}, half[3]={};
    D3DModelRenderState::CalculateDirectionalLight(720.0f,true,0,0,first);
    D3DModelRenderState::CalculateDirectionalLight(720.25f,true,0,0,quarter);
    D3DModelRenderState::CalculateDirectionalLight(720.5f,true,0,0,half);
    Check(quarter[0]<first[0] && quarter[0]>half[0],
        "sun direction advances between whole game minutes");
    Check(std::abs(quarter[0]*quarter[0]+quarter[1]*quarter[1]+quarter[2]*quarter[2]-1)<.00001f,
        "fractional sun direction stays normalized");
    std::vector<ff11EnvironmentRecord_t> records(2);
    for (auto& record : records) strcpy_s(record.directoryPath,"test/weat/fine");
    records[0].minuteOfDay = 0; records[1].minuteOfDay = 720;
    records[0].terrainLight.fogNear = 2; records[1].terrainLight.fogNear = 6;
    records[0].terrainLight.fogFar = 10; records[1].terrainLight.fogFar = 18;
    records[0].terrainLight.fogColor = 0x00302010; records[1].terrainLight.fogColor = 0x00706050;
    ZoneEnvironmentState::Data data; ZoneEnvironmentState::Cache cache; int weather = 0;
    ZoneEnvironmentState::Update(data,cache,weather,records,360);
    Check(data.fogNear == 4 && data.fogFar == 14 && data.fogColor == 0xff304050,
        "authored daytime fog interpolates distances and converts packed RGB once");
    ZoneEnvironmentState::Update(data,cache,weather,records,1080);
    Check(data.fogNear == 4 && data.fogFar == 14 && data.fogColor == 0xff304050,
        "authored fog interpolation wraps across midnight");
    records[1].terrainLight.fogFar = 0;
    ZoneEnvironmentState::Invalidate(cache,true);
    ZoneEnvironmentState::Update(data,cache,weather,records,359);
    Check(data.fogNear == 2 && data.fogFar == 10,"fog-off sentinel keeps the nearer enabled record discrete");
    ZoneEnvironmentState::Update(data,cache,weather,records,360);
    Check(data.fogFar == 0,"fog-off sentinel switches off at the record midpoint without inventing short-range fog");
}

#include "installed_zone_fog_cases.h"

void TestInstalledSunShadows(IDirect3DDevice9* device, const std::filesystem::path& root,
                             const std::filesystem::path& output)
{
    const auto path=root/"ROM/1/35.DAT";
    std::ifstream stream(path,std::ios::binary|std::ios::ate);
    Check(stream.good(),"open installed Bastok Markets DAT for shadow validation");
    if(!stream.good()) return;
    std::vector<BYTE> bytes(static_cast<size_t>(stream.tellg()));
    stream.seekg(0); stream.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
    noeRAPI_t rapi(device); rapi.SetCurrentFilePath(path.string().c_str());
    ff11Opts_t options={}; options.renderWater=true; options.renderEnvironment=true;
    const auto previous=gpFF11Opts; gpFF11Opts=&options;
    int count=0; auto* model=Model_FF11_LoadDAT(bytes.data(),static_cast<int>(bytes.size()),count,&rapi);
    gpFF11Opts=previous;
    Check(model!=nullptr,"load installed Bastok Markets geometry");
    if(!model) return;
    ZoneModelRenderMetadata::Prepare(model,device);
    ModelRenderer::Context context; context.device=device; context.rendersZoneObjects=true;
    context.geometryPass=ModelRenderer::GeometryPass::Opaque;
    context.lightingQuality=2; context.dynamicObjectShadows=true; context.dynamicActorShadows=true;
    context.useAuthoredLightDirection=true; context.authoredMainLightDirection[0]=.6f;
    context.authoredMainLightDirection[1]=-.7f; context.authoredMainLightDirection[2]=.4f;
    context.cameraPosition[0]=-276; context.cameraPosition[1]=-24; context.cameraPosition[2]=-42;
    context.minuteOfDay=720;
    context.shadowOpacity=.5f;
    const auto view=D3DMath::BuildLookAtLH(-276,-24,-42,-276,-12.93f,-68);
    const auto projection=D3DMath::BuildPerspectiveFovLH(.9f,1,.1f,1000);
    const auto identity=D3DMath::BuildIdentity();
    const auto draw=[&](bool shadows)
    {
        device->SetTransform(D3DTS_VIEW,&view); device->SetTransform(D3DTS_PROJECTION,&projection);
        ModelRenderer::BeginDirectionalShadowFrame();
        ModelRenderer::PrepareFixedFunctionPass(context,identity);
        ModelRenderer::DrawGeometry(context,model,identity);
        if(shadows) Check(ModelRenderer::DrawDirectionalShadowFrame(),"render installed zone sunlight shadows");
        else ModelRenderer::BeginDirectionalShadowFrame();
        auto transparent=context; transparent.geometryPass=ModelRenderer::GeometryPass::Transparent;
        ModelRenderer::PrepareFixedFunctionPass(transparent,identity);
        ModelRenderer::DrawGeometry(transparent,model,identity);
        CheckBaseline(device);
    };
    const auto baseline=Frame(device,[&]{draw(false);},output/"bastok-sun-disabled.bmp");
    const auto shadow=Frame(device,[&]{draw(true);},output/"bastok-sun-shadow.bmp");
    int changed=0;
    for(size_t i=0;i<baseline.size();++i) if(baseline[i]!=shadow[i]) ++changed;
    Check(changed>20,"sunlight occlusion visibly changes installed Bastok terrain");
    std::cout<<"Bastok sunlight shadow pixels: "<<changed<<'\n';
    ModelRenderer::ReleaseDirectionalShadowResources();
}
}
int main(int argc, char** argv)
{
    if (argc == 3 && std::string(argv[1]) == "--shadow-temporal")
    {
        Device device;
        Check(device.Create(true),"create temporal shadow validation device");
        if(!device.gpu) return 2;
        TestTemporalShadowEdges(device.gpu.ptr,argv[2]);
        TestHighResolutionShadowSlope(device.gpu.ptr,size,std::filesystem::path(argv[2])/"128");
        Device fullResolution;
        Check(fullResolution.Create(true,D3DMULTISAMPLE_NONE,1920),"create full-resolution shadow validation device");
        if(fullResolution.gpu) TestHighResolutionShadowSlope(fullResolution.gpu.ptr,1920,
            std::filesystem::path(argv[2])/"1920");
        std::cout<<"Temporal shadows: "<<assertions<<" assertions, "<<failures<<" failures\n";
        return failures ? 1 : 0;
    }
    if (argc == 4 && std::string(argv[1]) == "--zone-fog")
    {
        std::filesystem::create_directories(argv[3]);
        Device device;
        Check(device.Create(true, D3DMULTISAMPLE_NONE, 640), "create hardware zone fog capture device");
        if (!device.gpu) return 2;
        TestInstalledZoneFog(device.gpu.ptr, argv[2], argv[3]);
        std::cout << "Installed zone fog: " << assertions << " assertions, " << failures << " failures\n";
        return failures ? 1 : 0;
    }
    const bool materialsOnly = argc == 3 && std::string(argv[1]) == "--materials-only";
    const std::filesystem::path output = materialsOnly ? argv[2] :
        (argc > 1 ? argv[1] : "tests/bin/material-rendering/captures");
    std::filesystem::create_directories(output);
    TestEnvironmentInterpolation();
    Device device;
    Check(device.Create(),"create hidden native D3D9 HAL device");
    if (!device.gpu) return 2;
    D3DCAPS9 caps = {}; device.gpu->GetDeviceCaps(&caps);
    Check(caps.PixelShaderVersion >= D3DPS_VERSION(2,0),"required production ps_2_0 shader support");
    Check(CustomTextureAssets::SupportsPbr(device.gpu.ptr),
        "optional metallic/roughness PBR shader compiles on the D3D9 device");
    TestAlpha(device.gpu.ptr,output);
    TestUiShader(device.gpu.ptr);
    TestTextureTransitions(device.gpu.ptr,output);
    TestClassicBumpMapping(device.gpu.ptr,output);
    TestDepthOrdering(device.gpu.ptr,output);
    TestFog(device.gpu.ptr,output);
    if (materialsOnly)
    {
        std::cout << "Material/depth/fog (without shadows): " << assertions << " assertions, " << failures << " failures\n";
        return failures ? 1 : 0;
    }
    TestDirectionalShadows(device.gpu.ptr,output);
    TestLayeredDirectionalShadows(device.gpu.ptr,output);
    TestTemporalShadowEdges(device.gpu.ptr,output);
    TestHighResolutionShadowSlope(device.gpu.ptr,size,output/"slope-128");
    if(argc>2) TestInstalledSunShadows(device.gpu.ptr,argv[2],output);
    Device hardware;
    Check(hardware.Create(true),"create hardware vertex-processing shadow validation device");
    if(hardware.gpu)
    {
        TestDirectionalShadows(hardware.gpu.ptr,output/"hardware");
        TestLayeredDirectionalShadows(hardware.gpu.ptr,output/"hardware");
    }
    if(hardware.d3d && SUCCEEDED(hardware.d3d->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,D3DFMT_X8R8G8B8,TRUE,D3DMULTISAMPLE_4_SAMPLES,nullptr)) &&
        SUCCEEDED(hardware.d3d->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT,
            D3DDEVTYPE_HAL,D3DFMT_D24S8,TRUE,D3DMULTISAMPLE_4_SAMPLES,nullptr)))
    {
        Device multisampled;
        Check(multisampled.Create(true,D3DMULTISAMPLE_4_SAMPLES),"create 4x MSAA layered-shadow validation device");
        if(multisampled.gpu) TestLayeredDirectionalShadows(multisampled.gpu.ptr,output/"msaa-4x");
    }
    {
        Device fullResolution;
        Check(fullResolution.Create(true,D3DMULTISAMPLE_NONE,1920),"create full-resolution grazing-shadow validation device");
        if(fullResolution.gpu) TestHighResolutionShadowSlope(fullResolution.gpu.ptr,1920,output/"slope-1920");
    }
    std::cout << "Material/depth/fog: " << assertions << " assertions, " << failures << " failures\n";
    return failures ? 1 : 0;
}
