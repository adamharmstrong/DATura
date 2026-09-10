#include "stdafx.h"
#include "noesis_rapi.h"
#include "model_ff11.h"
#include "model_renderer.h"
#include "d3d_model_render_state.h"
#include "d3d_math.h"
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
    bool Create()
    {
        window = CreateWindowExA(0, "STATIC", "DATura material validation", WS_POPUP,
            0, 0, size, size, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
        d3d.ptr = Direct3DCreate9(D3D_SDK_VERSION);
        if (!window || !d3d) return false;
        D3DPRESENT_PARAMETERS pp = {};
        pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        pp.hDeviceWindow = window; pp.BackBufferWidth = pp.BackBufferHeight = size;
        pp.BackBufferFormat = D3DFMT_X8R8G8B8;
        pp.EnableAutoDepthStencil = TRUE; pp.AutoDepthStencilFormat = D3DFMT_D24S8;
        pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        return SUCCEEDED(d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &gpu.ptr));
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
    ComPtr<IDirect3DSurface9> target, staging;
    if (FAILED(device->GetRenderTarget(0, &target.ptr)) ||
        FAILED(device->CreateOffscreenPlainSurface(size, size, D3DFMT_X8R8G8B8,
            D3DPOOL_SYSTEMMEM, &staging.ptr, nullptr)) ||
        FAILED(device->GetRenderTargetData(target.ptr, staging.ptr)))
    { Check(false, "read back native D3D9 target"); return {}; }
    D3DLOCKED_RECT lock = {};
    if (FAILED(staging->LockRect(&lock, nullptr, D3DLOCK_READONLY)))
    { Check(false, "lock D3D9 target readback"); return {}; }
    Capture pixels(size * size);
    for (UINT row = 0; row < size; ++row)
        memcpy(pixels.data() + row * size, static_cast<char*>(lock.pBits) + row * lock.Pitch,
            size * sizeof(DWORD));
    staging->UnlockRect();
    if (!output.empty())
    {
        BITMAPFILEHEADER file = {};
        BITMAPINFOHEADER info = {};
        info.biSize = sizeof(info); info.biWidth = size; info.biHeight = -LONG(size);
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
        gpu->UnlockRect(0); info.pD3DTex = gpu.ptr;
        info.texType = dxt3Nibble >= 0 ? NOESISTEX_DXT3 : NOESISTEX_RGBA32;
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
        if (blended) { BlendState(device); D3DModelRenderState::ApplyTransparentMaterial(device, cache, &material, &texture.info); }
        else D3DModelRenderState::ApplyOpaqueMaterial(device, cache, &material, &texture.info);
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
                if (transparent) D3DModelRenderState::ApplyTransparentMaterial(device, cache, material, tex);
                else D3DModelRenderState::ApplyOpaqueMaterial(device, cache, material, tex);
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
            D3DModelRenderState::ApplyOpaqueMaterial(device, cache, &opaque, textured ? &texture.info : nullptr);
            for (int step = 0; step < 3; ++step)
                ImmediateQuad(device, -.95f + step * .65f, -.35f + step * .65f,
                    2.f + step * 2.f, textured ? 0x80808080 : 0xff804020);
        }, output / (textured ? "fog-textured-endpoints.bmp" : "fog-untextured-endpoints.bmp"));
        Expect(pixels, 20,64,{128,64,32}, textured ? "textured surface at fog start retains its color" : "untextured surface at fog start retains its color");
        Expect(pixels, 62,64,{80,64,96}, textured ? "textured surface halfway through fog interpolates RGB" : "untextured surface halfway through fog interpolates RGB");
        Expect(pixels, 103,64,{32,64,160}, textured ? "textured surface at fog end becomes fog color" : "untextured surface at fog end becomes fog color");
    }
    for (int invalid = 0; invalid < 4; ++invalid)
    {
        auto value = environment;
        if (invalid == 0) value.valid = false;
        if (invalid == 1) value.fogFar = 0;
        if (invalid == 2) value.fogFar = value.fogNear;
        if (invalid == 3) value.fogFar = std::numeric_limits<float>::quiet_NaN();
        auto pixels = Frame(device, [&]
        {
            ZoneEnvironmentFog::Apply(device, environment);
            ZoneEnvironmentFog::Apply(device, value);
            D3DModelRenderState::MaterialBindingCache cache;
            D3DModelRenderState::ApplyOpaqueMaterial(device, cache, &opaque, nullptr);
            ImmediateQuad(device, -.85f,.85f,8,0xff804020);
        });
        Expect(pixels,64,64,{128,64,32}, "disabled/invalid fog cannot retain previous weather fog");
    }
    auto pixels = Frame(device, [&]
    {
        ZoneEnvironmentFog::Apply(device, environment);
        Prepare(device); // Exactly the standalone-model pass boundary used by main.cpp.
        D3DModelRenderState::MaterialBindingCache cache;
        D3DModelRenderState::ApplyOpaqueMaterial(device, cache, &opaque, nullptr);
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

void TestEnvironmentInterpolation()
{
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
}
int main(int argc, char** argv)
{
    const std::filesystem::path output = argc > 1 ? argv[1] : "tests/bin/material-rendering/captures";
    std::filesystem::create_directories(output);
    TestEnvironmentInterpolation();
    Device device;
    Check(device.Create(),"create hidden native D3D9 HAL device");
    if (!device.gpu) return 2;
    D3DCAPS9 caps = {}; device.gpu->GetDeviceCaps(&caps);
    Check(caps.PixelShaderVersion >= D3DPS_VERSION(2,0),"required production ps_2_0 shader support");
    TestAlpha(device.gpu.ptr,output);
    TestTextureTransitions(device.gpu.ptr,output);
    TestDepthOrdering(device.gpu.ptr,output);
    TestFog(device.gpu.ptr,output);
    std::cout << "Material/depth/fog: " << assertions << " assertions, " << failures << " failures\n";
    return failures ? 1 : 0;
}
