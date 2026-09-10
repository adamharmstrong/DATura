#include "stdafx.h"
#include "noesis_rapi.h"
#include "model_ff11.h"
#include "model_ff11_water.h"
#include "ffxi_file_io.h"
#include "model_renderer.h"
#include "zone_model_render_metadata.h"
#include "zone_water_data.h"
#include "d3d_math.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <map>
#include <limits>
#include <set>
#include <vector>

namespace
{
int failures = 0;
constexpr UINT imageSize = 512;
using Capture = std::vector<DWORD>;

void Check(const bool condition, const char* message)
{
    if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}

template<class T> class ComPtr
{
public:
    T* ptr = nullptr;
    ~ComPtr() { if (ptr) ptr->Release(); }
    ComPtr() = default;
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
    T* operator->() const { return ptr; }
    explicit operator bool() const { return ptr != nullptr; }
};

struct TestDevice
{
    HWND window = nullptr;
    ComPtr<IDirect3D9> d3d;
    ComPtr<IDirect3DDevice9> device;
    bool Create()
    {
        // WS_POPUP without WS_VISIBLE creates no visible or interactive window.
        window = CreateWindowExA(0, "STATIC", "DATura water regression tests", WS_POPUP,
            0, 0, imageSize, imageSize, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
        d3d.ptr = Direct3DCreate9(D3D_SDK_VERSION);
        D3DPRESENT_PARAMETERS pp = {};
        pp.Windowed = TRUE;
        pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        pp.hDeviceWindow = window;
        pp.BackBufferWidth = imageSize;
        pp.BackBufferHeight = imageSize;
        pp.BackBufferFormat = D3DFMT_X8R8G8B8;
        pp.EnableAutoDepthStencil = TRUE;
        pp.AutoDepthStencilFormat = D3DFMT_D24S8;
        pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        return window && d3d && SUCCEEDED(d3d->CreateDevice(D3DADAPTER_DEFAULT,
            D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING,
            &pp, &device.ptr));
    }
    ~TestDevice()
    {
        D3DModelRenderState::ReleaseFfxiPixelShaders();
        if (device.ptr) { device.ptr->Release(); device.ptr = nullptr; }
        if (window) DestroyWindow(window);
    }
};

struct LoadOptions
{
    ff11Opts_t value = {};
    ff11Opts_t* previous = gpFF11Opts;
    explicit LoadOptions(const bool water)
    {
        value.renderWater = water;
        // Water must work in normal zone loading without diagnostic effect meshes.
        value.renderEnvironment = true;
        value.renderEffectMeshes = false;
        gpFF11Opts = &value;
    }
    ~LoadOptions() { gpFF11Opts = previous; }
};

int PixelDifference(const DWORD a, const DWORD b)
{
    int result = 0;
    for (const int shift : {0, 8, 16})
        result += std::abs(int((a >> shift) & 255) - int((b >> shift) & 255));
    return result;
}

size_t ChangedPixels(const Capture& a, const Capture& b, const int tolerance = 12)
{
    if (a.size() != imageSize * imageSize || b.size() != a.size()) return 0;
    size_t count = 0;
    for (size_t i = 0; i < a.size(); ++i)
        if (PixelDifference(a[i], b[i]) > tolerance) ++count;
    return count;
}

Capture CaptureTarget(IDirect3DDevice9* device, const std::filesystem::path& output)
{
    ComPtr<IDirect3DSurface9> target, staging;
    D3DSURFACE_DESC desc = {};
    if (FAILED(device->GetRenderTarget(0, &target.ptr)) ||
        FAILED(target->GetDesc(&desc)) ||
        FAILED(device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format,
            D3DPOOL_SYSTEMMEM, &staging.ptr, nullptr)) ||
        FAILED(device->GetRenderTargetData(target.ptr, staging.ptr)))
    { Check(false, "copy offscreen D3D9 render target"); return {}; }
    Check(desc.Width == imageSize && desc.Height == imageSize &&
        (desc.Format == D3DFMT_X8R8G8B8 || desc.Format == D3DFMT_A8R8G8B8),
        "render target uses the expected dimensions and 32-bit format");
    D3DLOCKED_RECT pixels = {};
    if (FAILED(staging->LockRect(&pixels, nullptr, D3DLOCK_READONLY)))
    { Check(false, "lock render capture"); return {}; }
    Capture capture;
    capture.reserve(desc.Width * desc.Height);
    for (UINT row = 0; row < desc.Height; ++row)
    {
        const auto* colors = reinterpret_cast<const DWORD*>(
            static_cast<const char*>(pixels.pBits) + row * pixels.Pitch);
        capture.insert(capture.end(), colors, colors + desc.Width);
    }
    staging->UnlockRect();
    if (!output.empty())
    {
        BITMAPFILEHEADER header = {};
        BITMAPINFOHEADER info = {};
        info.biSize = sizeof(info);
        info.biWidth = desc.Width;
        info.biHeight = -static_cast<LONG>(desc.Height);
        info.biPlanes = 1;
        info.biBitCount = 32;
        header.bfType = 0x4d42;
        header.bfOffBits = sizeof(header) + sizeof(info);
        header.bfSize = header.bfOffBits + desc.Width * desc.Height * sizeof(DWORD);
        std::ofstream stream(output, std::ios::binary);
        stream.write(reinterpret_cast<const char*>(&header), sizeof(header));
        stream.write(reinterpret_cast<const char*>(&info), sizeof(info));
        stream.write(reinterpret_cast<const char*>(capture.data()), capture.size() * sizeof(DWORD));
        Check(stream.good(), "write water regression BMP");
    }
    return capture;
}

void CheckRestoredState(IDirect3DDevice9* device)
{
    struct ExpectedState { D3DRENDERSTATETYPE name; DWORD value; };
    for (const auto state : {ExpectedState{D3DRS_ALPHABLENDENABLE, FALSE},
        ExpectedState{D3DRS_ALPHATESTENABLE, FALSE}, ExpectedState{D3DRS_ZWRITEENABLE, TRUE},
        ExpectedState{D3DRS_ZFUNC, D3DCMP_LESS}, ExpectedState{D3DRS_DEPTHBIAS, 0},
        ExpectedState{D3DRS_SLOPESCALEDEPTHBIAS, 0}, ExpectedState{D3DRS_STENCILENABLE, FALSE}})
    {
        DWORD actual = ~state.value;
        Check(SUCCEEDED(device->GetRenderState(state.name, &actual)) && actual == state.value,
            "water pass restores baseline depth and blend states");
    }
    DWORD transform = D3DTTFF_COUNT2;
    device->GetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, &transform);
    Check(transform == D3DTTFF_DISABLE, "water UV transform cannot leak into following draws");
    ComPtr<IDirect3DVertexBuffer9> stream;
    ComPtr<IDirect3DIndexBuffer9> indices;
    ComPtr<IDirect3DBaseTexture9> texture;
    ComPtr<IDirect3DPixelShader9> shader;
    UINT offset = 0, stride = 0;
    device->GetStreamSource(0, &stream.ptr, &offset, &stride);
    device->GetIndices(&indices.ptr);
    device->GetTexture(0, &texture.ptr);
    device->GetPixelShader(&shader.ptr);
    Check(!stream && !indices && !texture && !shader, "water pass unbinds buffers, textures and shader");
}

using Pass = std::pair<noesisModel_t*, ModelRenderer::GeometryPass>;
Capture DrawSequence(IDirect3DDevice9* device, const std::vector<Pass>& passes, ModelRenderer::Context context,
             const D3DMATRIX& view, const D3DMATRIX& projection,
             const std::filesystem::path& output = {})
{
    context.device = device;
    const auto identity = D3DMath::BuildIdentity();
    device->SetTransform(D3DTS_VIEW, &view);
    device->SetTransform(D3DTS_PROJECTION, &projection);
    Check(SUCCEEDED(device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
        0xff182028, 1.0f, 0)), "clear water test target");
    if (FAILED(device->BeginScene())) { Check(false, "begin water test scene"); return {}; }
    for (const auto& pass : passes)
    {
        context.geometryPass = pass.second;
        ModelRenderer::PrepareFixedFunctionPass(context, identity);
        ModelRenderer::DrawGeometry(context, pass.first, identity);
        CheckRestoredState(device);
    }
    Check(SUCCEEDED(device->EndScene()), "end water test scene");
    return CaptureTarget(device, output);
}

Capture Draw(IDirect3DDevice9* device, noesisModel_t* model, ModelRenderer::Context context,
             const D3DMATRIX& view, const D3DMATRIX& projection,
             const std::filesystem::path& output = {})
{
    return DrawSequence(device, {{model, context.geometryPass}}, context, view, projection, output);
}

noesisModel_t::Submesh Quad(const char* name, const char* material, const float left,
    const float right, const float bottom, const float top, const float z, const DWORD color)
{
    noesisModel_t::Submesh mesh;
    mesh.objectName = name;
    mesh.materialName = material;
    const float points[][4] = {{left, bottom, 0, 0}, {right, bottom, 1, 0},
        {right, top, 1, 1}, {left, top, 0, 1}};
    for (const auto& point : points)
    {
        FFXIVertex vertex = {};
        vertex.pos[0] = point[0]; vertex.pos[1] = point[1]; vertex.pos[2] = z;
        vertex.nrm[2] = -1;
        vertex.uv[0] = point[2]; vertex.uv[1] = point[3];
        vertex.diffuse = color;
        mesh.cpuVerts.push_back(vertex);
    }
    mesh.cpuIndices = {0, 1, 2, 0, 2, 3};
    mesh.vertCount = 4;
    mesh.triCount = 2;
    return mesh;
}

void TestSyntheticRendering(IDirect3DDevice9* device, const std::filesystem::path& output)
{
    ComPtr<IDirect3DTexture9> gpuTexture;
    Check(SUCCEEDED(device->CreateTexture(16, 16, 1, 0, D3DFMT_A8R8G8B8,
        D3DPOOL_MANAGED, &gpuTexture.ptr, nullptr)), "create patterned water fixture texture");
    if (!gpuTexture) return;
    D3DLOCKED_RECT lock = {};
    if (FAILED(gpuTexture->LockRect(0, &lock, nullptr, 0)))
    { Check(false, "fill water fixture texture"); return; }
    for (int y = 0; y < 16; ++y)
    {
        auto* row = reinterpret_cast<DWORD*>(static_cast<char*>(lock.pBits) + y * lock.Pitch);
        for (int x = 0; x < 16; ++x)
            row[x] = (x < 4 || y < 5) ? 0xff2040a0 : 0xff60b040;
    }
    gpuTexture->UnlockRect(0);
    noesisTex_t texture;
    texture.pD3DTex = gpuTexture.ptr;
    texture.texType = NOESISTEX_RGBA32;
    noesisMaterial_t opaque, liquid;
    char opaqueName[] = "opaque", liquidName[] = "water";
    opaque.name = opaqueName; opaque.noDefaultBlend = true; opaque.flags = NMATFLAG_TWOSIDED;
    liquid.name = liquidName; liquid.texIdx = 0; liquid.flags = NMATFLAG_TWOSIDED;
    noesisMaterial_t* materials[] = {&opaque, &liquid};
    noesisTex_t* textures[] = {&texture};
    noesisMatData_t data;
    data.mats = materials; data.matCount = 2; data.textures = textures; data.texCount = 1;
    noesisModel_t model;
    // Declared after model so resources always release before the stack model dies.
    struct ReleaseModel { noesisModel_t& model; ~ReleaseModel() { model.ReleaseD3DBuffers(); } } release{model};
    model.pMatData = &data;
    model.submeshes.push_back(Quad("backdrop", "opaque", -4, 4, -4, 4, 3, 0xff202020));
    model.submeshes.push_back(Quad("foreground blocker", "opaque", -0.8f, -0.2f, -0.4f, 0.4f, 1.5f, 0xffc02020));
    model.submeshes.push_back(Quad("water fixture", "water", -1.4f, 1.4f, -1.4f, 1.4f, 2, 0x80808080));
    auto surface = std::make_shared<ZoneWater::Surface>();
    surface->generator = "test"; surface->resource = "water";
    surface->uvVelocity[0] = 0.125f; surface->uvVelocity[1] = -0.0625f;
    surface->colorScale[3] = 0.5f;
    model.submeshes.back().water = surface;
    const auto immutable = model.submeshes.back().cpuVerts;
    model.BuildD3DBuffers(device);
    ZoneModelRenderMetadata::Prepare(&model, device);
    Check(model.submeshes.back().softBlend && model.submeshes.back().water == surface,
        "authored water enters the transparent draw set with its animation metadata");
    for (const auto index : model.opaqueSubmeshOrder)
        Check(!model.submeshes[index].water, "water is excluded from opaque batching");
    ModelRenderer::Context context;
    context.rendersZoneObjects = true;
    context.animationSeconds = 0;
    const auto view = D3DMath::BuildLookAtLH(0, 0, 0, 0, 0, 3);
    const auto projection = D3DMath::BuildPerspectiveFovLH(1.5707963f, 1, 0.1f, 10);
    context.waterRenderingEnabled = false;
    const auto disabled = Draw(device, &model, context, view, projection, output / "synthetic-disabled.bmp");
    context.waterRenderingEnabled = true;
    const auto enabled = Draw(device, &model, context, view, projection, output / "synthetic-t0.bmp");
    Check(ChangedPixels(disabled, enabled) > 20000, "enabling water produces visible pixels");
    size_t blockerPixels = 0, overwritten = 0;
    for (size_t i = 0; i < disabled.size() && i < enabled.size(); ++i)
        if (((disabled[i] >> 16) & 255) > 160 && ((disabled[i] >> 8) & 255) < 60)
        { ++blockerPixels; overwritten += PixelDifference(disabled[i], enabled[i]) > 3; }
    Check(blockerPixels > 1000 && overwritten == 0, "opaque foreground occludes water through the depth buffer");
    surface->colorScale[3] = 0;
    const auto invisible = Draw(device, &model, context, view, projection);
    Check(ChangedPixels(disabled, invisible, 0) == 0, "authored zero opacity produces no water contribution");
    surface->colorScale[3] = 1;
    const auto opaqueWater = Draw(device, &model, context, view, projection);
    Check(ChangedPixels(enabled, opaqueWater) > 20000, "authored opacity controls actual blended pixels");
    surface->colorScale[3] = 0.5f;
    context.animationSeconds = 2;
    const auto moved = Draw(device, &model, context, view, projection, output / "synthetic-t2.bmp");
    Check(ChangedPixels(enabled, moved) > 10000, "authored U and V velocities move the sampled texture");
    context.animationSeconds = 0;
    const auto repeated = Draw(device, &model, context, view, projection);
    Check(ChangedPixels(enabled, repeated, 0) == 0, "explicit animation time is deterministic and reversible");
    model.ReleaseD3DBuffers();
    model.BuildD3DBuffers(device);
    ZoneModelRenderMetadata::Prepare(&model, device);
    const auto rebuilt = Draw(device, &model, context, view, projection);
    Check(ChangedPixels(enabled, rebuilt, 0) == 0, "buffer recreation preserves water metadata and frame output");
    Check(immutable.size() == model.submeshes.back().cpuVerts.size() &&
        !memcmp(immutable.data(), model.submeshes.back().cpuVerts.data(), immutable.size() * sizeof(FFXIVertex)),
        "UV animation leaves source geometry and material attributes immutable");

    // Actors use separate model containers in the application. A combined model
    // provides an independent depth/blend reference for the scene pass ordering.
    noesisModel_t zone, actors, reference;
    ReleaseModel releaseZone{zone}, releaseActors{actors}, releaseReference{reference};
    zone.pMatData = actors.pMatData = reference.pMatData = &data;
    zone.submeshes.push_back(Quad("scene background", "opaque", -4, 4, -4, 4, 3, 0xff202020));
    zone.submeshes.push_back(Quad("scene water", "water", -1.4f, 1.4f, -1.4f, 1.4f, 2, 0x80808080));
    zone.submeshes.back().water = surface;
    actors.submeshes.push_back(Quad("actor above water", "opaque", -0.8f, -0.2f, -0.4f, 0.4f, 1.5f, 0xffc02020));
    actors.submeshes.push_back(Quad("actor below water", "opaque", 0.2f, 1.2f, -0.7f, 0.7f, 2.5f, 0xff2040c0));
    reference.submeshes = zone.submeshes;
    reference.submeshes.insert(reference.submeshes.end(), actors.submeshes.begin(), actors.submeshes.end());
    for (auto* scene : {&zone, &actors, &reference})
    { scene->BuildD3DBuffers(device); ZoneModelRenderMetadata::Prepare(scene, device); }
    const auto combined = Draw(device, &reference, context, view, projection, output / "synthetic-actor-reference.bmp");
    const auto split = DrawSequence(device, {{&zone, ModelRenderer::GeometryPass::Opaque},
        {&actors, ModelRenderer::GeometryPass::All}, {&zone, ModelRenderer::GeometryPass::Transparent}},
        context, view, projection, output / "synthetic-actor-composition.bmp");
    Check(ChangedPixels(combined, split, 0) == 0,
        "zone opaque, actor, then zone transparent matches the combined reference above and below water");
    std::cout << "Actor composition differs from reference: exact=" << ChangedPixels(combined, split, 0)
        << " tolerance12=" << ChangedPixels(combined, split) << " pixels\n";
    const auto wrongOrder = DrawSequence(device, {{&zone, ModelRenderer::GeometryPass::All},
        {&actors, ModelRenderer::GeometryPass::All}}, context, view, projection, output / "synthetic-actor-wrong-order.bmp");
    Check(ChangedPixels(combined, wrongOrder) > 1000,
        "fixture exposes actors incorrectly overwriting water when all zone passes run first");

    // A broad water mesh's distant centroid cannot put a nearby translucent
    // seabed decal on top of the water: its fragments are physically underwater.
    noesisMaterial_t decal;
    char decalName[] = "underwater terrain overlay";
    decal.name = decalName; decal.flags = NMATFLAG_TWOSIDED;
    noesisMaterial_t* wideMaterials[] = {&opaque, &liquid, &decal};
    noesisMatData_t wideData = data;
    wideData.mats = wideMaterials; wideData.matCount = 3;
    noesisModel_t wideScene, wideBackdrop, wideOverlay, wideWater;
    ReleaseModel releaseWide{wideScene}, releaseWideBackdrop{wideBackdrop},
        releaseWideOverlay{wideOverlay}, releaseWideWater{wideWater};
    wideBackdrop.submeshes.push_back(Quad("wide-scene seabed", "opaque", -4, 4, -4, 4, 3, 0xff202020));
    wideOverlay.submeshes.push_back(Quad("near-centroid seabed overlay", decalName,
        -1.1f, 1.1f, -0.8f, 0.8f, 2.5f, 0x80909090));
    wideWater.submeshes.push_back(Quad("far-centroid broad water", "water",
        -2, 40, -1.4f, 1.4f, 2, 0x80808080));
    wideWater.submeshes.back().water = surface;
    wideScene.submeshes = wideBackdrop.submeshes;
    wideScene.submeshes.push_back(wideOverlay.submeshes.front());
    wideScene.submeshes.push_back(wideWater.submeshes.front());
    for (auto* scene : {&wideScene, &wideBackdrop, &wideOverlay, &wideWater})
    { scene->pMatData = &wideData; scene->BuildD3DBuffers(device); ZoneModelRenderMetadata::Prepare(scene, device); }
    const auto wideReference = DrawSequence(device, {{&wideBackdrop, ModelRenderer::GeometryPass::All},
        {&wideOverlay, ModelRenderer::GeometryPass::All}, {&wideWater, ModelRenderer::GeometryPass::All}},
        context, view, projection);
    const auto wideActual = Draw(device, &wideScene, context, view, projection,
        output / "synthetic-underwater-transparent-terrain.bmp");
    Check(ChangedPixels(wideReference, wideActual, 0) == 0,
        "underwater translucent terrain composites before a broad water surface despite misleading centroid distance");
    const auto wideWrongOrder = DrawSequence(device, {{&wideBackdrop, ModelRenderer::GeometryPass::All},
        {&wideWater, ModelRenderer::GeometryPass::All}, {&wideOverlay, ModelRenderer::GeometryPass::All}},
        context, view, projection);
    Check(ChangedPixels(wideReference, wideWrongOrder) > 1000,
        "wide-surface fixture exposes the original underwater transparent-overlay ordering defect");

    noesisModel_t overlappingSea;
    ReleaseModel releaseSea{overlappingSea};
    overlappingSea.pMatData = &data;
    overlappingSea.submeshes.push_back(Quad("sea backdrop", "opaque", -4, 4, -4, 4, 3, 0xff202020));
    for (const auto* name : {"sea tile A", "sea tile B"})
    {
        overlappingSea.submeshes.push_back(Quad(name, "water", -1.4f, 1.4f, -1.4f, 1.4f, 2, 0x80808080));
        overlappingSea.submeshes.back().water = surface;
    }
    overlappingSea.BuildD3DBuffers(device);
    ZoneModelRenderMetadata::Prepare(&overlappingSea, device);
    surface->singleCoverage = true;
    const auto singleSheet = Draw(device, &zone, context, view, projection);
    const auto overlapping = Draw(device, &overlappingSea, context, view, projection,
        output / "synthetic-sea-overlap.bmp");
    Check(ChangedPixels(singleSheet, overlapping, 0) == 0,
        "overlapping sea placements shade their shared screen footprint exactly once");
    auto invisibleSea = std::make_shared<ZoneWater::Surface>(*surface);
    invisibleSea->colorScale[3] = 0;
    overlappingSea.submeshes[1].water = invisibleSea;
    const auto transparentFirst = Draw(device, &overlappingSea, context, view, projection);
    Check(ChangedPixels(singleSheet, transparentFirst, 0) == 0,
        "zero-alpha sea does not reserve coverage and hide the visible layer behind it");
    overlappingSea.submeshes[1].water = surface;
    surface->singleCoverage = false;
    const auto ordinaryOverlap = Draw(device, &overlappingSea, context, view, projection);
    Check(ChangedPixels(singleSheet, ordinaryOverlap) > 20000,
        "ordinary layered water retains multiple contributions rather than using sea coverage rules");
    liquid.texIdx = -1;
    model.renderMetadataPrepared = false;
    ZoneModelRenderMetadata::Prepare(&model, device);
    const auto untextured = Draw(device, &model, context, view, projection, output / "synthetic-untextured.bmp");
    Check(ChangedPixels(disabled, untextured) > 20000,
        "untextured authored water uses vertex color and opacity instead of sampling an unbound texture");
    surface->colorScale[0] = 0.1f; surface->colorScale[1] = 0.3f; surface->colorScale[2] = 0.6f;
    const auto tinted = Draw(device, &model, context, view, projection);
    Check(ChangedPixels(untextured, tinted) > 20000, "authored RGB channels tint untextured water pixels");
    surface->colorScale[3] = 0;
    Check(ChangedPixels(disabled, Draw(device, &model, context, view, projection), 0) == 0,
        "zero opacity also hides untextured water");
    std::cout << "Synthetic water: enabled=" << ChangedPixels(disabled, enabled)
        << " moving=" << ChangedPixels(enabled, moved) << " occluded=" << blockerPixels << " pixels\n";
}

bool Near(const float a, const float b, const float tolerance = 0.00001f)
{
    return std::abs(a - b) <= tolerance;
}

void TestWaterPolicy()
{
    ZoneWater::Curve curve;
    Check(curve.Evaluate(0.5f, 0.7f) == 0.7f, "missing curves preserve authored base color");
    curve.keys = {{0.0f, 0.2f}, {0.5f, 0.8f}, {1.0f, 0.2f}};
    Check(Near(curve.Evaluate(0.25f), 0.5f) && Near(curve.Evaluate(0.75f), 0.5f),
        "time-of-day curves interpolate on both sides of noon");
    Check(curve.Evaluate(std::numeric_limits<float>::quiet_NaN(), 0.4f) == 0.4f,
        "invalid clock returns finite curve fallback");
    ZoneWater::Surface surface;
    surface.colorCurves[3] = curve;
    surface.uvVelocity[0] = 0.125f;
    surface.uvVelocity[1] = -0.12f;
    const auto state = ZoneWater::Evaluate(surface, 720, 2);
    Check(Near(state.opacity, 0.8f) && Near(state.uvOffset[0], 0.25f) && Near(state.uvOffset[1], -0.24f),
        "water evaluation preserves authored UV direction and time-of-day opacity");
    Check(Near(ZoneWater::Evaluate(surface, 2160, 2).opacity, state.opacity) &&
        Near(ZoneWater::Evaluate(surface, -720, 2).opacity, state.opacity),
        "water day wrapping is stable for positive and negative clock offsets");
    Check(ZoneWater::Evaluate(surface, 720, std::numeric_limits<double>::infinity()).uvOffset[0] == 0,
        "invalid animation time cannot send nonfinite UVs to D3D");
    curve.keys = {{0.5f, 1.0f}, {0.2f, 0.0f}};
    Check(curve.Evaluate(0.25f, 0.6f) == 0.6f, "unordered DAT curves use safe fallback");

    Check(FF11Water::ResourceScopeRank("f_ro/effe/kawa", "f_ro/effe/kawa") >
        FF11Water::ResourceScopeRank("f_ro/effe/kawa", "f_ro/effe"),
        "effect resolution prefers the generator directory over its parent");
    Check(FF11Water::ResourceScopeRank("f_ro/effe/kawa", "f_ro/effe/kawa2") == 0 &&
        FF11Water::ResourceScopeRank("f_ro/effe/kawa", "f_ro/effe/umi1") == 0 &&
        FF11Water::ResourceScopeRank("f_ro/effe/kawa", "f_ro") == 0,
        "same resource names cannot resolve across sibling effects or outside the effect root");
    ff11GeneratorRecord_t generator = {};
    strcpy_s(generator.name, "ka01"); strcpy_s(generator.directoryPath, "f_ro/effe/kawa");
    strcpy_s(generator.linkedResource, "ka1");
    generator.hasStandardParticleSetup = true;
    generator.linkedDataType = 0x0b;
    generator.generatorFlags = 0x10;
    generator.hasSpawnPosition = true;
    Check(FF11Water::IsSupportedGenerator(generator), "persistent world MMB generator is supported");
    generator.particleLifetimeFrames = 60;
    Check(!FF11Water::IsSupportedGenerator(generator), "finite-lifetime particles are not frozen as water");
    generator.particleLifetimeFrames = 0;
    generator.hasLinearVelocity = true;
    Check(!FF11Water::IsSupportedGenerator(generator), "moving emitters are not flattened into static water");
    generator.hasLinearVelocity = false;
    generator.attachFlags = 1;
    Check(!FF11Water::IsSupportedGenerator(generator), "bone-attached effects are not drawn as world water");
    generator.attachFlags = 0;
    strcpy_s(generator.directoryPath, "f_ro/weat/suny/effe");
    Check(!FF11Water::IsSupportedGenerator(generator), "weather geometry stays owned by the weather pipeline");
    strcpy_s(generator.directoryPath, "f_ro/effe/kawa");
    generator.hasRotation = true;
    generator.rotation[1] = 1.5707963268f;
    generator.hasScale = true;
    generator.scale[0] = 2; generator.scale[1] = 3; generator.scale[2] = 4;
    generator.spawnPosition[0] = 10; generator.spawnPosition[1] = 20; generator.spawnPosition[2] = 30;
    const auto transform = FF11Water::PlacementTransform(generator);
    Check(Near(transform[0][0], 0) && Near(transform[0][2], -2) && Near(transform[1][1], 3) &&
        Near(transform[2][0], 4) && Near(transform[3][0], 10),
        "water SRT preserves raw FFXI XYZ radians, per-axis scale, and world translation");
    generator.hasRotation = false;
    generator.scale[0] = 6; generator.scale[1] = 0; generator.scale[2] = 6;
    const auto flattened = FF11Water::PlacementTransform(generator);
    Check(Near(flattened[0][0], 6) && Near(flattened[1][1], 0) && Near(flattened[2][2], 6),
        "authored zero Y scale remains a flat sea plane");
    generator.scale[1] = 1; generator.scale[2] = -6;
    Check(FF11Water::BackwardWinding(generator), "reflected water placement corrects winding");
    Check(!FF11Water::IsSurfaceBatch("waterfall", "fire") &&
        FF11Water::IsSurfaceBatch("ka1", "effect  kaw1    "),
        "water classification uses verified material identity rather than name substrings");

    const auto savedCurves = std::move(gFF11LastKeyframeRecords);
    struct RestoreCurves
    {
        const std::vector<ff11KeyframeRecord_t>& records;
        ~RestoreCurves() { gFF11LastKeyframeRecords = records; }
    } restore{savedCurves};
    ff11KeyframeRecord_t parentCurve = {}, localCurve = {};
    strcpy_s(parentCurve.name, "colr"); strcpy_s(parentCurve.directoryPath, "f_ro/effe");
    parentCurve.pairCount = 2; parentCurve.times[1] = 1;
    parentCurve.values[0] = parentCurve.values[1] = 0.25f;
    localCurve = parentCurve;
    strcpy_s(localCurve.directoryPath, "f_ro/effe/kawa");
    localCurve.values[0] = localCurve.values[1] = 0.75f;
    gFF11LastKeyframeRecords = {parentCurve, localCurve};
    strcpy_s(generator.redKeyframe, "colr");
    auto copied = FF11Water::BuildSurface(generator);
    gFF11LastKeyframeRecords.clear();
    Check(Near(copied->colorCurves[0].Evaluate(0.5f), 0.75f),
        "nearest scoped curves remain model-owned after loading another DAT");
}

struct InstalledFixture
{
    const char* name;
    const char* relative;
    const char* generator;
    const char* resource;
    const char* directory;
    size_t expectedPlacements;
    std::array<float, 3> eye;
    std::array<float, 3> target;
    float velocityV;
    bool hasColorCurve;
    bool hasAlphaCurve;
    bool flatSurface;
    bool textured;
    bool uvMovesPixels;
    bool collapsedScale;
};

void ReportRiverHeights(const noesisModel_t& model)
{
    // Geometry diagnostic at an independently decoded ka01 river coordinate.
    for (const auto& mesh : model.submeshes)
    {
        if ((!mesh.water && mesh.softBlend) || mesh.environmentObject || !mesh.hasBounds ||
            420 < mesh.boundsMin[0] || 420 > mesh.boundsMax[0] ||
            172 < mesh.boundsMin[2] || 172 > mesh.boundsMax[2]) continue;
        for (size_t i = 0; i + 2 < mesh.cpuIndices.size(); i += 3)
        {
            const auto& a = mesh.cpuVerts[mesh.cpuIndices[i]];
            const auto& b = mesh.cpuVerts[mesh.cpuIndices[i + 1]];
            const auto& c = mesh.cpuVerts[mesh.cpuIndices[i + 2]];
            const float bx = b.pos[0] - a.pos[0], bz = b.pos[2] - a.pos[2];
            const float cx = c.pos[0] - a.pos[0], cz = c.pos[2] - a.pos[2];
            const float determinant = bx * cz - cx * bz;
            if (std::abs(determinant) < 0.00001f) continue;
            const float x = 420 - a.pos[0], z = 172 - a.pos[2];
            const float u = (x * cz - cx * z) / determinant;
            const float v = (bx * z - x * bz) / determinant;
            if (u < 0 || v < 0 || u + v > 1) continue;
            const float y = a.pos[1] + u * (b.pos[1] - a.pos[1]) + v * (c.pos[1] - a.pos[1]);
            if (y > -65 && y < -30)
                std::cout << "River XZ(420,172): " << (mesh.water ? "water" : "opaque")
                    << " Y=" << y << " object=" << mesh.objectName << '\n';
            break;
        }
    }
}

void TestInstalledDat(IDirect3DDevice9* device, const std::filesystem::path& root,
                      const std::filesystem::path& output, const InstalledFixture& fixture)
{
    const auto file = root / fixture.relative;
    BYTE* raw = nullptr;
    DWORD size = 0;
    Check(FFXIFileIO::ReadWholeFile(file.string().c_str(), &raw, &size), "read installed water fixture DAT");
    if (!raw) return;
    std::unique_ptr<BYTE[]> bytes(raw);
    noeRAPI_t rapi(device);
    rapi.SetCurrentFilePath(file.string().c_str());
    LoadOptions options(true);
    int count = 0;
    auto* model = Model_FF11_LoadDAT(bytes.get(), static_cast<int>(size), count, &rapi);
    Check(model != nullptr, "load installed water fixture with normal effect-mesh settings");
    if (!model) return;
    ZoneModelRenderMetadata::Prepare(model, device);
    std::set<unsigned int> placements;
    const noesisModel_t::Submesh* selected = nullptr;
    size_t waterMeshes = 0;
    for (const auto& mesh : model->submeshes)
    {
        if (!mesh.water) continue;
        ++waterMeshes;
        placements.insert(mesh.water->sourceOffset);
        Check(mesh.softBlend && !mesh.environmentObject && mesh.hasBounds,
            "installed water is visible world geometry in the transparent draw set");
        if (mesh.pResolvedMaterial && mesh.pResolvedMaterial->texIdx >= 0)
            Check(mesh.pResolvedTexture && mesh.pResolvedTexture->pD3DTex,
                "installed water resolves its authored texture to a live GPU resource");
        Check(std::any_of(gFF11LastMapObjects.begin(), gFF11LastMapObjects.end(),
            [&](const auto& object) { return mesh.objectName == object.displayName; }),
            "water draw identity exactly matches its inspector and hide/show record");
        if (mesh.water->generator == fixture.generator && mesh.water->resource == fixture.resource &&
            mesh.water->directory == fixture.directory)
            selected = &mesh;
    }
    std::cout << fixture.name << ": water meshes=" << waterMeshes << " placements=" << placements.size() << '\n';
    Check(placements.size() == fixture.expectedPlacements, "installed zone has the audited number of permanent water placements");
    Check(selected != nullptr, "installed fixture includes its named generator/resource pair");
    if (!selected) return;
    Check(Near(selected->water->uvVelocity[1], fixture.velocityV),
        "installed UV velocity converts authored 60 Hz frames to seconds");
    Check((selected->pResolvedTexture != nullptr) == fixture.textured,
        "textured and untextured water preserve their authored material source");
    Check(selected->water->twoSided == fixture.collapsedScale,
        "collapsed placement scales disable ambiguous culling after coordinate reflection");
    Check(selected->water->singleCoverage == fixture.collapsedScale,
        "only audited base sea placements use one-footprint blending");
    Check(!selected->water->colorCurves[0].keys.empty() == fixture.hasColorCurve,
        "generator color curves are preserved only when authored");
    Check(!selected->water->colorCurves[3].keys.empty() == fixture.hasAlphaCurve,
        "generator alpha curve is preserved only when authored");
    if (fixture.flatSurface)
        Check(Near(selected->boundsMin[1], selected->boundsMax[1]),
            "installed sea generator preserves its authored zero-height scale");
    if (std::string(fixture.name) == "east-ronfaure") ReportRiverHeights(*model);
    ModelRenderer::Context context;
    context.rendersZoneObjects = true;
    context.minuteOfDay = 720;
    context.animationSeconds = 0;
    std::copy(fixture.eye.begin(), fixture.eye.end(), context.cameraPosition);
    const auto view = D3DMath::BuildLookAtLH(fixture.eye[0], fixture.eye[1], fixture.eye[2],
        fixture.target[0], fixture.target[1], fixture.target[2]);
    const auto projection = D3DMath::BuildPerspectiveFovLH(0.9f, 1, D3DMath::ZoneNearPlane, 1000);
    context.waterRenderingEnabled = false;
    const auto disabled = Draw(device, model, context, view, projection,
        output / (std::string(fixture.name) + "-disabled.bmp"));
    context.waterRenderingEnabled = true;
    const auto enabled = Draw(device, model, context, view, projection,
        output / (std::string(fixture.name) + "-t0.bmp"));
    context.animationSeconds = 2;
    const auto moved = Draw(device, model, context, view, projection,
        output / (std::string(fixture.name) + "-t2.bmp"));
    Check(ChangedPixels(disabled, enabled) > 100, "water is visibly drawn in the complete installed zone");
    if (fixture.uvMovesPixels)
        Check(ChangedPixels(enabled, moved, 3) > 100, "installed authored UV animation changes visible water pixels");
    std::cout << fixture.name << ": visible=" << ChangedPixels(disabled, enabled)
        << " moving=" << ChangedPixels(enabled, moved, 3) << " pixels\n";
    if (fixture.collapsedScale)
    {
        const auto fullOrder = model->softBlendSubmeshOrder;
        context.animationSeconds = 0;
        std::vector<std::shared_ptr<ZoneWater::Surface>> coverageSurfaces;
        for (auto& mesh : model->submeshes)
            if (mesh.water && mesh.water->singleCoverage)
            {
                coverageSurfaces.push_back(mesh.water);
                mesh.water->singleCoverage = false;
            }
        Draw(device, model, context, view, projection, output / "valkurm-water-last-no-stencil.bmp");
        for (const auto& water : coverageSurfaces) water->singleCoverage = true;
        std::erase_if(model->softBlendSubmeshOrder,
            [&](const size_t index) { return !model->submeshes[index].water; });
        Draw(device, model, context, view, projection, output / "valkurm-all-water-only.bmp");
        model->softBlendSubmeshOrder = fullOrder;
        std::erase_if(model->softBlendSubmeshOrder,
            [&](const size_t index) { return model->submeshes[index].water != nullptr; });
        Draw(device, model, context, view, projection, output / "valkurm-nonwater-over-opaque.bmp");
        auto transparentContext = context;
        transparentContext.geometryPass = ModelRenderer::GeometryPass::Transparent;
        Draw(device, model, transparentContext, view, projection, output / "valkurm-nonwater-transparent-only.bmp");
        ZoneRenderFrustum::Data diagnosticFrustum;
        ZoneRenderFrustum::Build(diagnosticFrustum, view, projection);
        for (const auto index : model->softBlendSubmeshOrder)
        {
            const auto& mesh = model->submeshes[index];
            if (mesh.environmentObject || !mesh.hasBounds ||
                !ZoneRenderFrustum::IntersectsBounds(diagnosticFrustum, mesh.boundsMin, mesh.boundsMax, true))
                continue;
            float distanceSquared = 0;
            for (int axis = 0; axis < 3; ++axis)
            {
                const float delta = mesh.boundsCenter[axis] - context.cameraPosition[axis];
                distanceSquared += delta * delta;
            }
            if (distanceSquared > 600 * 600) continue;
            std::cout << "Valkurm nonwater transparent: object=" << mesh.objectName << " material=" << mesh.materialName
                << " bounds=(" << mesh.boundsMin[0] << ',' << mesh.boundsMin[1] << ',' << mesh.boundsMin[2]
                << ")..(" << mesh.boundsMax[0] << ',' << mesh.boundsMax[1] << ',' << mesh.boundsMax[2] << ")\n";
        }
        model->softBlendSubmeshOrder = fullOrder;
        std::vector<size_t> selectedBatches;
        for (const auto index : fullOrder)
            if (model->submeshes[index].water &&
                model->submeshes[index].water->sourceOffset == selected->water->sourceOffset)
                selectedBatches.push_back(index);
        context.animationSeconds = 0;
        model->softBlendSubmeshOrder = selectedBatches;
        Draw(device, model, context, view, projection, output / "valkurm-selected-generator.bmp");
        for (size_t batch = 0; batch < selectedBatches.size(); ++batch)
        {
            model->softBlendSubmeshOrder = {selectedBatches[batch]};
            Draw(device, model, context, view, projection,
                output / ("valkurm-selected-batch" + std::to_string(batch) + ".bmp"));
        }
        model->softBlendSubmeshOrder = fullOrder;
        std::erase_if(model->softBlendSubmeshOrder, [&](const size_t index)
        {
            const auto& water = model->submeshes[index].water;
            return !water || water->resource != "umif";
        });
        Draw(device, model, context, view, projection, output / "valkurm-only-umif.bmp");
        model->softBlendSubmeshOrder = fullOrder;

        // Bounded visual diagnostic: retain the existing sea sort slots but put
        // high-alpha cores before feathered aprons. Only temporary sort centers
        // change; source vertices, GPU data, and all production ordering stay intact.
        struct SeaOrder
        {
            size_t index;
            unsigned int minAlpha;
            double meanAlpha;
            float distanceSquared;
            std::array<float, 3> originalCenter;
        };
        std::vector<SeaOrder> seaOrder;
        std::vector<float> seaSlots;
        for (const auto index : fullOrder)
        {
            const auto& mesh = model->submeshes[index];
            if (!mesh.water || !mesh.water->singleCoverage || mesh.cpuVerts.empty()) continue;
            unsigned int minAlpha = 255;
            double sum = 0;
            for (const auto& vertex : mesh.cpuVerts)
            {
                const unsigned int alpha = vertex.diffuse >> 24;
                minAlpha = std::min(minAlpha, alpha);
                sum += alpha;
            }
            float distanceSquared = 0;
            for (int axis = 0; axis < 3; ++axis)
            {
                const float delta = mesh.boundsCenter[axis] - context.cameraPosition[axis];
                distanceSquared += delta * delta;
            }
            seaOrder.push_back({index, minAlpha, sum / mesh.cpuVerts.size(), distanceSquared,
                {mesh.boundsCenter[0], mesh.boundsCenter[1], mesh.boundsCenter[2]}});
            seaSlots.push_back(distanceSquared);
        }
        std::sort(seaSlots.begin(), seaSlots.end(), std::greater<float>());
        std::stable_sort(seaOrder.begin(), seaOrder.end(), [](const auto& a, const auto& b)
        {
            if (a.minAlpha != b.minAlpha) return a.minAlpha > b.minAlpha;
            if (a.meanAlpha != b.meanAlpha) return a.meanAlpha > b.meanAlpha;
            return a.distanceSquared > b.distanceSquared;
        });
        for (size_t rank = 0; rank < seaOrder.size(); ++rank)
        {
            auto& mesh = model->submeshes[seaOrder[rank].index];
            mesh.boundsCenter[0] = context.cameraPosition[0] + std::sqrt(seaSlots[rank]);
            mesh.boundsCenter[1] = context.cameraPosition[1];
            mesh.boundsCenter[2] = context.cameraPosition[2];
        }
        const auto coreFirst = Draw(device, model, context, view, projection, output / "valkurm-core-first.bmp");
        std::cout << "Valkurm core-first diagnostic: " << ChangedPixels(enabled, coreFirst) << " changed pixels\n";
        std::map<std::string, ZoneWater::Surface> primaryAppearance;
        std::vector<std::pair<std::shared_ptr<ZoneWater::Surface>, ZoneWater::Surface>> originalSurfaces;
        std::set<ZoneWater::Surface*> savedSurfaces;
        for (const auto& mesh : model->submeshes)
        {
            if (!mesh.water || !mesh.water->singleCoverage || !savedSurfaces.insert(mesh.water.get()).second)
                continue;
            originalSurfaces.emplace_back(mesh.water, *mesh.water);
            if (mesh.water->resource == "umi0") primaryAppearance.emplace(mesh.water->directory, *mesh.water);
        }
        for (const auto& saved : originalSurfaces)
        {
            const auto primary = primaryAppearance.find(saved.first->directory);
            if (primary == primaryAppearance.end()) continue;
            for (int channel = 0; channel < 4; ++channel)
            {
                saved.first->colorScale[channel] = primary->second.colorScale[channel];
                saved.first->colorCurves[channel] = primary->second.colorCurves[channel];
            }
        }
        const auto sharedBody = Draw(device, model, context, view, projection,
            output / "valkurm-shared-body-appearance.bmp");
        std::cout << "Valkurm shared-body-appearance diagnostic: " << ChangedPixels(enabled, sharedBody)
            << " changed pixels\n";
        for (auto& saved : originalSurfaces) *saved.first = std::move(saved.second);
        for (const auto& entry : seaOrder)
            std::copy(entry.originalCenter.begin(), entry.originalCenter.end(),
                model->submeshes[entry.index].boundsCenter);

        // Reference-style material diagnostic: one global sea appearance with
        // neutral vertex diffuse, retaining authored placement, UVs and flow.
        const ZoneWater::Surface neutralAppearance = *selected->water;
        originalSurfaces.clear();
        savedSurfaces.clear();
        std::vector<std::pair<size_t, std::vector<FFXIVertex>>> originalSeaVertices;
        for (size_t index = 0; index < model->submeshes.size(); ++index)
        {
            auto& mesh = model->submeshes[index];
            if (!mesh.water || !mesh.water->singleCoverage) continue;
            originalSeaVertices.emplace_back(index, mesh.cpuVerts);
            for (auto& vertex : mesh.cpuVerts) vertex.diffuse = 0x80808080;
            if (savedSurfaces.insert(mesh.water.get()).second)
            {
                originalSurfaces.emplace_back(mesh.water, *mesh.water);
                for (int channel = 0; channel < 4; ++channel)
                {
                    mesh.water->colorScale[channel] = neutralAppearance.colorScale[channel];
                    mesh.water->colorCurves[channel] = neutralAppearance.colorCurves[channel];
                }
            }
        }
        model->ReleaseD3DBuffers();
        model->BuildD3DBuffers(device);
        ZoneModelRenderMetadata::Prepare(model, device);
        const auto neutralSea = Draw(device, model, context, view, projection,
            output / "valkurm-neutral-sea-material.bmp");
        std::cout << "Valkurm neutral-sea-material diagnostic: " << ChangedPixels(enabled, neutralSea)
            << " changed pixels\n";
        for (auto& saved : originalSeaVertices)
            model->submeshes[saved.first].cpuVerts = std::move(saved.second);
        for (auto& saved : originalSurfaces) *saved.first = std::move(saved.second);
        model->ReleaseD3DBuffers();
        model->BuildD3DBuffers(device);
        ZoneModelRenderMetadata::Prepare(model, device);
    }
    if (fixture.hasColorCurve)
    {
        context.minuteOfDay = 0;
        context.animationSeconds = 0;
        const auto night = Draw(device, model, context, view, projection,
            output / (std::string(fixture.name) + "-night.bmp"));
        Check(ChangedPixels(enabled, night) > 100, "authored day and night curves change visible water color");
        context.minuteOfDay = 720;
    }

    // Restore the original clock after loading an unrelated model: metadata must
    // live with its scene rather than refer to mutable last-DAT parser globals.
    context.animationSeconds = 0;
    const auto beforeReload = ZoneWater::Evaluate(*selected->water, 720, 0);
    std::vector<BYTE> disabledBytes(bytes.get(), bytes.get() + size);
    noeRAPI_t disabledRapi(nullptr);
    disabledRapi.SetCurrentFilePath(file.string().c_str());
    options.value.renderWater = false;
    auto* withoutWater = Model_FF11_LoadDAT(disabledBytes.data(), static_cast<int>(disabledBytes.size()), count, &disabledRapi);
    Check(withoutWater != nullptr, "base geometry still loads with water disabled");
    if (withoutWater)
        Check(std::none_of(withoutWater->submeshes.begin(), withoutWater->submeshes.end(),
            [](const auto& mesh) { return mesh.water != nullptr; }), "load option disables water without enabling diagnostic effects");
    Check(Near(beforeReload.opacity, ZoneWater::Evaluate(*selected->water, 720, 0).opacity),
        "water metadata survives a subsequent DAT parse");
    const auto repeated = Draw(device, model, context, view, projection);
    Check(ChangedPixels(enabled, repeated, 0) == 0, "installed water frame stays deterministic after another DAT parse");
}
}

int main(int argc, char** argv)
{
    if (argc > 3)
    { std::cerr << "Usage: WaterRenderingTests [FFXI-root [capture-directory]]\n"; return 2; }
    const std::filesystem::path output = argc > 2 ? argv[2] : "tests/bin/water-rendering/captures";
    std::error_code error;
    std::filesystem::create_directories(output, error);
    Check(!error, "create capture directory");
    TestDevice device;
    Check(device.Create(), "create hidden D3D9 test device");
    if (!device.device) return 2;
    TestWaterPolicy();
    TestSyntheticRendering(device.device.ptr, output);
    if (argc > 1)
    {
        const InstalledFixture river = {"east-ronfaure", "ROM/0/121.DAT", "ka01", "ka1", "f_ro/effe/kawa", 44,
            {420, -58, 194}, {420, -48, 172}, -0.12f, false, true, true, true, true, false};
        TestInstalledDat(device.device.ptr, argv[1], output, river);
        const InstalledFixture sea = {"valkurm", "ROM/0/102.DAT", "uma1", "umi0", "f_ki/effe/umi1", 11,
            {295, -6, -155}, {295, 3.956f, -205}, 0.12f, true, false, true, true, true, true};
        TestInstalledDat(device.device.ptr, argv[1], output, sea);
        const InstalledFixture fountain = {"bastok-markets", "ROM/1/35.DAT", "fnmz", "funm", "t_ba/effe/funs", 4,
            {-276, -24, -42}, {-276, -12.93f, -68}, 0.48f, true, true, false, false, false, false};
        TestInstalledDat(device.device.ptr, argv[1], output, fountain);
    }
    else std::cout << "Installed-DAT checks skipped: pass the FFXI installation root to enable them.\n";
    std::cout << (failures ? "Water rendering tests failed: " : "Water rendering tests passed: ") << failures << '\n';
    return failures ? 1 : 0;
}
