#include "stdafx.h"
#include "bgw_player.h"
#include "d3d_math.h"
#include "effect_model_layout.h"
#include "ffxi_file_io.h"
#include "home_point_data.h"
#include "home_point_effect.h"
#include "noesis_rapi.h"
#include "model_ff11.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

int failures = 0;
void Check(bool ok, const char *what)
{
    if (!ok)
    {
        ++failures;
        std::cerr << "FAIL " << what << '\n';
    }
}
std::vector<DWORD> Capture(IDirect3DDevice9 *device, const char *name)
{
    IDirect3DSurface9 *target = nullptr, *copy = nullptr;
    if (FAILED(device->GetRenderTarget(0, &target)))
        return {};
    D3DSURFACE_DESC desc;
    target->GetDesc(&desc);
    if (FAILED(device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM,
                                                   &copy, nullptr)))
    {
        target->Release();
        return {};
    }
    if (FAILED(device->GetRenderTargetData(target, copy)))
    {
        target->Release();
        copy->Release();
        return {};
    }
    target->Release();
    D3DLOCKED_RECT lock;
    copy->LockRect(&lock, nullptr, D3DLOCK_READONLY);
    std::vector<DWORD> pixels(desc.Width * desc.Height);
    for (UINT y = 0; y < desc.Height; ++y)
        memcpy(pixels.data() + y * desc.Width, (char *)lock.pBits + y * lock.Pitch, desc.Width * 4);
    copy->UnlockRect();
    copy->Release();
    std::filesystem::create_directories("tests/bin/home-point/captures");
    std::ofstream out(std::string("tests/bin/home-point/captures/") + name + ".bmp", std::ios::binary);
    BITMAPFILEHEADER h = {};
    BITMAPINFOHEADER info = {};
    h.bfType = 0x4d42;
    h.bfOffBits = sizeof(h) + sizeof(info);
    h.bfSize = h.bfOffBits + (DWORD)pixels.size() * 4;
    info.biSize = sizeof(info);
    info.biWidth = desc.Width;
    info.biHeight = -(int)desc.Height;
    info.biPlanes = 1;
    info.biBitCount = 32;
    out.write((char *)&h, sizeof(h));
    out.write((char *)&info, sizeof(info));
    out.write((char *)pixels.data(), pixels.size() * 4);
    return pixels;
}
int main(int argc, char **argv)
{
    if (argc < 2)
        return 2;
    const std::string root = argv[1];
    BYTE *raw = nullptr;
    DWORD size = 0;
    for (int id : {9013, 16023})
    {
        char relative[100];
        sprintf_s(relative, "/sound/win/se/se%03d/se%06d.spw", id / 1000, id);
        FFXIAudioInfo info;
        char wav[MAX_PATH] = {};
        bool read = FFXIAudio_ReadInfo((root + relative).c_str(), &info);
        bool prepared = FFXIAudio_PrepareFile((root + relative).c_str(), wav, sizeof(wav));
        std::cout << "sound=" << id << " info=" << read << " decoded=" << prepared
                  << " rate=" << info.sampleRate << " wav=" << wav << '\n';
        Check(read && prepared, "decode installed crystal sound");
    }
    if (!FFXIFileIO::ReadWholeFile((root + "/ROM/3/25.DAT").c_str(), &raw, &size))
        return 2;
    std::vector<uint8_t> bytes(raw, raw + size);
    delete[] raw;
    HomePoint::Data data;
    Check(HomePoint::Parse(bytes, data), "retail Home Point DAT parses");
    Check(data.generators.size() == 13, "all authored generators retained");
    Check(data.schedules["aper"].size() == 9, "idle schedule has all nine resources");
    Check(data.schedules["bind"].size() == 5, "activation schedule has sound and four emitters");
    Check(data.sounds["9013"] == 9013 && data.sounds["6023"] == 16023, "sound IDs come from DAT references");
    Check(data.generators["bnd0"].specularTexture == "nami", "authored specular texture selected");
    Check(data.generators["bnd0"].rotationSpeed[1] < -.01f, "crystal rotation decoded");
    Check(EffectModelLayout::VertexOffset6(1, 0) == 30 && EffectModelLayout::MaterialOffset6(4, 0) == 16,
          "retail group offsets");
    const auto idle = HomePoint::Evaluate(data, 5, -1000);
    const auto active = HomePoint::Evaluate(data, 5.5, 5);
    Check(active.size() > idle.size(), "activation adds scheduled particles");
    Check(HomePoint::Evaluate(data, 10, 5).size() == HomePoint::Evaluate(data, 10, -1000).size(),
          "activation expires");
    Check(HomePoint::Evaluate(data, 100000, -1000).size() >= idle.size(),
          "idle emitters survive long sessions");
    Check(HomePoint::Evaluate(data, std::numeric_limits<double>::quiet_NaN(), 0).empty(),
          "invalid time is rejected");
    auto bad = bytes;
    bad.resize(bad.size() - 1);
    HomePoint::Data rejected;
    Check(!HomePoint::Parse(bad, rejected), "truncated chunk rejected");
    bad = bytes;
    bad[0x20 + 16 + 0x70] = 0;
    bad[0x20 + 16 + 0x71] = 0;
    Check(!HomePoint::Parse(bad, rejected), "invalid generator offset rejected");
    HWND window = CreateWindowExA(0, "STATIC", "Home Point tests", WS_POPUP, 0, 0, 640, 640, nullptr, nullptr,
                                  GetModuleHandle(nullptr), nullptr);
    auto *d3d = Direct3DCreate9(D3D_SDK_VERSION);
    IDirect3DDevice9 *device = nullptr;
    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = window;
    pp.BackBufferWidth = 640;
    pp.BackBufferHeight = 640;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D24S8;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    if (!d3d || FAILED(d3d->CreateDevice(0, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp,
                                         &device)))
        return 3;
    {
        HomePoint::Effect effect;
        Check(effect.Load(device, root.c_str(), false), "load geometry, textures and shaders");
        std::cout << "layers=" << effect.LayerCount() << " triangles=" << effect.TriangleCount()
                  << " audio=" << effect.HasSounds() << '\n';
        Check(effect.LayerCount() == 7, "five mesh resources and two sprite resources");
        Check(effect.TriangleCount() == 302, "all 296 mesh triangles and six sprite triangles");
        float eye[3] = {3, -2.3f, 4};
        auto view = D3DMath::BuildLookAtLH(eye[0], eye[1], eye[2], 0, -1, 0);
        auto proj = D3DMath::BuildPerspectiveFovLH(.7f, 1, .1f, 100);
        auto world = D3DMath::BuildIdentity();
        device->SetTransform(D3DTS_VIEW, &view);
        device->SetTransform(D3DTS_PROJECTION, &proj);
        device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        device->SetRenderState(D3DRS_FOGENABLE, FALSE);
        std::vector<DWORD> captures[3];
        for (int i = 0; i < 3; ++i)
        {
            device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff18202c, 1, 0);
            device->BeginScene();
            effect.Draw(device, world, eye, i == 0 ? 3 : 3.7, i == 2 ? 3.2 : -1000, true);
            device->EndScene();
            captures[i] = Capture(device, i == 0 ? "idle-a" : i == 1 ? "idle-b" : "activation");
        }
        for (int k = 0; k < 3; ++k)
            Check(captures[k].size() == 640 * 640, "render capture succeeded");
        if (captures[0].size() == 640 * 640)
        {
            size_t drawn = 0, moving = 0, activation = 0;
            for (size_t i = 0; i < captures[0].size(); ++i)
            {
                if ((captures[0][i] & 0xffffff) != 0x18202c)
                    ++drawn;
                if (captures[0][i] != captures[1][i])
                    ++moving;
                if (captures[1][i] != captures[2][i])
                    ++activation;
            }
            std::cout << "drawn=" << drawn << " moving=" << moving << " activation=" << activation << '\n';
            Check(drawn > 1000, "crystal visible");
            Check(moving > 100, "authored idle animation changes pixels");
            Check(activation > 50, "activation changes pixels");
        }
        DWORD state = 0;
        device->GetRenderState(D3DRS_ZWRITEENABLE, &state);
        Check(state == TRUE, "depth writes restored");
        device->GetRenderState(D3DRS_ALPHABLENDENABLE, &state);
        Check(state == FALSE, "blending restored");
        IDirect3DPixelShader9 *shader = nullptr;
        device->GetPixelShader(&shader);
        Check(shader == nullptr, "shader restored");
        if (shader)
            shader->Release();
        float right[3] = {1, 0, 0}, listener[3] = {24.99f, 0, 0};
        std::vector<HomePoint::Instance> instances = {{1, {0, 0, 0}}};
        effect.UpdateSound(instances, listener, right, true, 1);
        if (effect.HasSounds())
            Check(effect.SoundVoiceCount() == 1, "nearby idle sound uses an independent voice");
        effect.ActivateSound(instances[0], listener, right);
        Check(effect.SoundVoiceCount() <= 1, "activation honors maximum sounds");
        effect.UpdateSound(instances, listener, right, false, 1);
        Check(effect.SoundVoiceCount() == 0, "mute stops all crystal voices");
        listener[0] = 30;
        effect.UpdateSound(instances, listener, right, true, 1);
        Check(effect.SoundVoiceCount() == 0, "distant crystal is silent");
        effect.StopSound();
        Check(SUCCEEDED(device->Reset(&pp)), "device reset with loaded effect succeeds");
        device->SetTransform(D3DTS_VIEW, &view);
        device->SetTransform(D3DTS_PROJECTION, &proj);
        device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff18202c, 1, 0);
        device->BeginScene();
        effect.Draw(device, world, eye, 4, -1000, true);
        device->EndScene();
        auto afterReset = Capture(device, "after-reset");
        size_t resetPixels = 0;
        for (DWORD c : afterReset)
            if ((c & 0xffffff) != 0x18202c)
                ++resetPixels;
        Check(resetPixels > 1000, "effect survives device reset");
        Check(!effect.Load(device, "Z:/missing-homepoint-install", false),
              "missing installation fails safely");
        Check(effect.LayerCount() == 7, "failed reload keeps previous asset");
    }
    device->Release();
    d3d->Release();
    DestroyWindow(window);
    std::cout << "failures=" << failures << '\n';
    return failures ? 1 : 0;
}
