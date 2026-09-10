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
#include "zone_weather_particles.h"
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



Capture DrawWeather(IDirect3DDevice9 *device, noesisModel_t *model, const char *path,
    const std::vector<ff11GeneratorRecord_t> &generators,
    const std::vector<ff11KeyframeRecord_t> &curves, const float eye[3], double seconds,
    bool occluder, const std::filesystem::path &output = {})
{
    const auto view = D3DMath::BuildLookAtLH(eye[0],eye[1],eye[2],eye[0],eye[1],eye[2]+10);
    const auto projection = D3DMath::BuildPerspectiveFovLH(3.14159265f/3,1,0.1f,200);
    device->SetTransform(D3DTS_VIEW,&view); device->SetTransform(D3DTS_PROJECTION,&projection);
    device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,D3DCOLOR_XRGB(30,45,60),occluder?0.0f:1.0f,0);
    device->BeginScene();
    ZoneWeatherParticles::DrawAtTime(device,model,true,true,path,generators,curves,eye[0],eye[1],eye[2],seconds,720);
    device->EndScene();
    return CaptureTarget(device,output);
}
void Fixture(IDirect3DDevice9 *device,const std::filesystem::path &root,const char *relative,const char *weather,const float eye[3])
{
    BYTE *raw=nullptr;DWORD size=0;
    const auto file=root/relative;
    if (!FFXIFileIO::ReadWholeFile(file.string().c_str(),&raw,&size)) {Check(false,"read weather DAT");return;}
    std::unique_ptr<BYTE[]> bytes(raw);
    noeRAPI_t rapi(device);rapi.SetCurrentFilePath(file.string().c_str());LoadOptions opts(false);int count=0;
    auto *model=Model_FF11_LoadDAT(raw,(int)size,count,&rapi);
    Check(model!=nullptr,"load weather DAT");if(!model)return;
    auto generators=gFF11LastGeneratorRecords;auto curves=gFF11LastKeyframeRecords;
    const bool rain = std::string(weather).find("rain") != std::string::npos;
    int spriteGenerators=0;
    for(const auto &g:generators) if(!strcmp(g.directoryPath,weather)&&(g.moreFlags&32)&&g.linkedDataType==14) {
        ++spriteGenerators;
        Check(rain || (g.updateLifetimeAlpha && g.lifetimeAlphaKeyframe[0]),"sprite lifetime alpha updater decoded");
        Check(rain || (g.hasPositionVariance && g.spawnRadius>0),"authored emission spread decoded");
        Check(rain || g.hasLinearVelocity,"authored velocity decoded");
    }
    Check(spriteGenerators>0,"fixture has authored sprite emitters");
    Check(!model->weatherSprites.empty(),"shared sprite resource loaded without overwriting zone metadata");
    const auto out=std::filesystem::path("tests/bin/weather-rendering/captures")/std::filesystem::path(weather).filename();
    std::filesystem::create_directories(out);
    const auto blank=DrawWeather(device,model,"none",generators,curves,eye,2,false);
    const auto enabled=DrawWeather(device,model,weather,generators,curves,eye,2,false,out/"enabled.bmp");
    const auto moving=DrawWeather(device,model,weather,generators,curves,eye,2.15,false,out/"moving.bmp");
    const auto repeated=DrawWeather(device,model,weather,generators,curves,eye,2,false);
    const auto blocked=DrawWeather(device,model,weather,generators,curves,eye,2,true);
    std::cout<<weather<<" emitters="<<spriteGenerators<<" sprites="<<model->weatherSprites.size()<<" visible="<<ChangedPixels(blank,enabled)<<" moving="<<ChangedPixels(enabled,moving)<<'\n';
    Check(ChangedPixels(blank,enabled)>20,"authored particles render actual pixels");
    Check(ChangedPixels(enabled,moving)>20,"authored particles animate");
    Check(ChangedPixels(enabled,repeated,0)==0,"fixed clock produces identical frames");
    Check(ChangedPixels(blank,blocked,0)==0,"opaque depth occludes all weather particles");

    if (!rain) {
    auto noAnimation = generators;
    for(auto &g : noAnimation) g.animateSprite = false;
    Check(ChangedPixels(enabled,DrawWeather(device,model,weather,noAnimation,curves,eye,2,false))>20,
        "authored sprite atlas progresses during particle lifetime");
    auto hiddenCurves = curves;
    for(auto &c : hiddenCurves)
        for(const auto &g : generators)
            if(!strcmp(g.directoryPath,weather) && !strcmp(g.lifetimeAlphaKeyframe,c.name))
                for(int k=0;k<c.pairCount;++k) c.values[k]=0;
    Check(ChangedPixels(blank,DrawWeather(device,model,weather,generators,hiddenCurves,eye,2,false),0)==0,
        "zero authored lifetime opacity hides particles");
    const float movedEye[3] = {eye[0]+1000,eye[1],eye[2]};
    const auto movedCamera = DrawWeather(device,model,weather,generators,curves,movedEye,2,false);
    if(std::string(weather).find("dust")!=std::string::npos)
        Check(ChangedPixels(enabled,movedCamera)<100,"camera-relative dust follows the camera");
    else
        Check(ChangedPixels(blank,movedCamera,0)==0,"world-space snow emitters stay at authored positions");
    }
    for(auto &g:generators) if(!strcmp(g.directoryPath,weather)) g.generatorFlags &= ~0x10;
    Check(ChangedPixels(blank,DrawWeather(device,model,weather,generators,curves,eye,2,false),0)==0,"inactive emitters produce no particles");
}
}
int main(int argc,char **argv)
{
    if(argc!=2)return 2;TestDevice device;if(!device.Create())return 2;
    const float dustEye[3]={0,0,0},snowEye[3]={23,-3,257},blizEye[3]={0,-60,-4};
    Fixture(device.device.ptr,argv[1],"ROM/0/90.DAT","f_ko/weat/dust",dustEye);
    Fixture(device.device.ptr,argv[1],"ROM/0/122.DAT","f_no/weat/snow",snowEye);
    Fixture(device.device.ptr,argv[1],"ROM/1/19.DAT","h_ga/weat/bliz",blizEye);
    Fixture(device.device.ptr,argv[1],"ROM/0/115.DAT","f_la/weat/rain",dustEye);
    std::cout<<"Weather rendering failures: "<<failures<<'\n';return failures?1:0;
}
