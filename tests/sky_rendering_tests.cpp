#include "stdafx.h"
#include "noesis_rapi.h"
#include "model_ff11.h"
#include "model_ff11_water.h"
#include "ffxi_file_io.h"
#include "model_renderer.h"
#include "zone_model_render_metadata.h"
#include "zone_environment_state.h"
#include "zone_environment_animation.h"
#include "zone_environment_render_state.h"
#include "zone_sky_dome.h"
#include "d3d_math.h"
#include "d3d_model_render_state.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
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
        window = CreateWindowExA(0, "STATIC", "DATura sky regression tests", WS_POPUP,
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
        Check(stream.good(), "write sky regression BMP");
    }
    return capture;
}


Capture DrawSky(IDirect3DDevice9* device, noesisModel_t* model, int minute,
                bool missingCurves, const std::filesystem::path& output, const char *weatherPath = "f_ko/weat/fine")
{
    auto generators = gFF11LastGeneratorRecords;
    auto curves = gFF11LastKeyframeRecords;
    for (auto& g : generators) g.hasRotationVelocity = false;
    for (auto& curve : curves)
    {
        const float value = ZoneEnvironmentAnimation::EvaluateKeyframe(&curve, minute / 1440.0f, 1.0f);
        for (int i = 0; i < curve.pairCount; ++i) curve.values[i] = value;
    }
    if (missingCurves) curves.clear();
    ZoneEnvironmentState::Data env;
    ZoneEnvironmentState::Cache cache;
    int weather = 0;
    ZoneEnvironmentState::Update(env, cache, weather, gFF11LastEnvironmentRecords, minute);
    for (size_t i = 0; i < cache.weatherGroups.size(); ++i)
        if (!strcmp(cache.weatherGroups[i]->directoryPath, weatherPath)) weather = static_cast<int>(i);
    ZoneEnvironmentState::Update(env, cache, weather, gFF11LastEnvironmentRecords, minute);
    const auto view = D3DMath::BuildLookAtLH(-307.369f, -20.025f, 233.480f, -284.089f, -41.906f, 328.520f);
    const auto projection = D3DMath::BuildPerspectiveFovLH(3.14159265f / 4, 1, 0.1f, 3000);
    device->SetTransform(D3DTS_VIEW, &view);
    device->SetTransform(D3DTS_PROJECTION, &projection);
    device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, env.clearColor, 1, 0);
    device->BeginScene();
    ZoneSkyDome::Parameters sky;
    sky.environmentValid = env.valid;
    sky.radius = env.skyRadius;
    sky.spokeCount = env.sphereSpokeCount;
    sky.ringCount = env.ringCount;
    sky.ringElevations = env.ringElevations;
    sky.ringColors = env.ringColors;
    ZoneSkyDome::Draw(device, sky, -307.369f, -20.025f, 233.480f, 3000);
    ZoneEnvironmentRenderState::DrawCameraShells(device, model, true, env.weatherPath,
        generators, curves, -307.369f, -20.025f, 233.480f);
    device->EndScene();
    return CaptureTarget(device, output);
}
}
int main(int argc, char** argv)
{
    if (argc != 2) { std::cerr << "Usage: SkyRenderingTests FFXI-root\n"; return 2; }
    TestDevice device;
    if (!device.Create()) return 2;
    const auto file = std::filesystem::path(argv[1]) / "ROM/0/90.DAT";
    BYTE* raw = nullptr; DWORD size = 0;
    if (!FFXIFileIO::ReadWholeFile(file.string().c_str(), &raw, &size)) return 2;
    std::unique_ptr<BYTE[]> bytes(raw);
    noeRAPI_t rapi(device.device.ptr);
    rapi.SetCurrentFilePath(file.string().c_str());
    LoadOptions options(false);
    int count = 0;
    auto* model = Model_FF11_LoadDAT(raw, static_cast<int>(size), count, &rapi);
    if (!model) return 2;
    ZoneModelRenderMetadata::Prepare(model, device.device.ptr);
    Check(D3DModelRenderState::SetFfxiTexturePixelShader(device.device.ptr, true, false), "FFXI texture shader available");
    int clouds = 0;
    for (const auto& g : gFF11LastGeneratorRecords)
    {
        if (strcmp(g.directoryPath, "f_ko/weat/fine") ||
            (strcmp(g.name, "cld1") && strcmp(g.name, "cld2"))) continue;
        ++clouds;
        Check(!strcmp(g.redKeyframe, "kcr1") && !strcmp(g.greenKeyframe, "kcg1") &&
            !strcmp(g.blueKeyframe, "kcb1"), "fine cloud generator resolves RGB curve names after flags word");
        const auto* red = ZoneEnvironmentAnimation::FindKeyframe(g, g.redKeyframe, gFF11LastKeyframeRecords);
        Check(red != nullptr, "fine cloud color curve exists");
        Check(std::abs(ZoneEnvironmentAnimation::EvaluateKeyframe(red, 0, 1) - 0.04f) < 0.001f,
            "night cloud red uses authored 0.04 instead of full-bright fallback");
    }
    Check(clouds == 2, "both fine cloud layers are tested");
    const std::filesystem::path output = "tests/bin/sky-rendering/captures";
    std::filesystem::create_directories(output);
    for (int minute : {0, 720})
    {
        const auto prefix = output / std::to_string(minute);
        const auto fixed = DrawSky(device.device.ptr, model, minute, false, prefix.string() + "-fixed.bmp");
        const auto broken = DrawSky(device.device.ptr, model, minute, true, prefix.string() + "-missing-curves.bmp");
        Check(!fixed.empty() && ChangedPixels(fixed, broken) > 10000, "authored curves affect actual cloud pixels");
        const auto whites = [](const Capture& pixels) {
            return std::count_if(pixels.begin(), pixels.end(), [](DWORD c) {
                return (c & 255) > 245 && ((c >> 8) & 255) > 245 && ((c >> 16) & 255) > 245;
            });
        };
        std::cout << minute << " white pixels: " << whites(broken) << " -> " << whites(fixed) << '\n';
        Check(whites(fixed) < whites(broken), "cloud tint removes blown-out white band");
    }

    std::set<std::string> patterns;
    for (const auto& env : gFF11LastEnvironmentRecords) patterns.insert(env.directoryPath);
    for (const auto& pattern : patterns)
    {
        if (pattern.find("/weat/") == std::string::npos) continue;
        for (int minute : {0,720})
        {
            auto capture = DrawSky(device.device.ptr, model, minute, false,
                output / (std::filesystem::path(pattern).filename().string()+"-"+std::to_string(minute)+".bmp"),pattern.c_str());
            Check(capture.size()==imageSize*imageSize,"every Konschtat weather pattern renders at day and night");
        }
    }
    std::cout << "Weather sky patterns exercised: " << patterns.size() << '\n';
    std::cout << "Sky rendering failures: " << failures << '\n';
    return failures ? 1 : 0;
}
