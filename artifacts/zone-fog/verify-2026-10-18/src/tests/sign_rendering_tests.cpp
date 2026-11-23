#include "stdafx.h"
#include "noesis_rapi.h"
#include "model_ff11.h"
#include "ffxi_file_io.h"
#include <memory>
#include <iostream>
#include <fstream>

#include "model_renderer.h"
#include "zone_model_render_metadata.h"
#include "d3d_math.h"

int main(int argc, char** argv)
{
    if (argc < 2) { std::cerr << "Pass Bastok Markets ROM/1/35.DAT and optional sign resource name.\n"; return 2; }
    BYTE* raw = nullptr; DWORD size = 0;
    if (!FFXIFileIO::ReadWholeFile(argv[1], &raw, &size)) return 2;
    std::unique_ptr<BYTE[]> bytes(raw);
    HWND window = CreateWindowExA(0, "STATIC", "Sign tests", WS_POPUP, 0, 0, 512, 512, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    auto* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    IDirect3DDevice9* device = nullptr;
    D3DPRESENT_PARAMETERS pp = {}; pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = window; pp.EnableAutoDepthStencil = TRUE; pp.AutoDepthStencilFormat = D3DFMT_D24S8;
    if (d3d) d3d->CreateDevice(0, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device);
    if (!device) return 3;
    noeRAPI_t rapi(device);
    rapi.SetCurrentFilePath(argv[1]);
    ff11Opts_t options = {};
    gpFF11Opts = &options;
    int count = 0;
    auto* model = Model_FF11_LoadDAT(bytes.get(), size, count, &rapi);
    if (!model) return 1;
    model->UpdateSubmeshBounds();
    const std::string name = argc > 2 ? argv[2] : "zakka_bord_p";
    model->ReleaseD3DBuffers();
    std::erase_if(model->submeshes, [&](const auto& mesh) { return mesh.objectName.find(name) == std::string::npos; });
    model->BuildD3DBuffers(device);
    ZoneModelRenderMetadata::Prepare(model, device);
    if (model->submeshes.empty()) return 4;
    auto& mesh = model->submeshes.back();
    float x=mesh.boundsCenter[0], y=mesh.boundsCenter[1], z=mesh.boundsCenter[2];
    std::vector<DWORD> captures[3];
    const float nearPlanes[] = {0.01f, D3DMath::ZoneNearPlane, 1.0f};
    for (int version=0; version<3; ++version)
    {
        float a=3.14159265f;
        const auto view=D3DMath::BuildLookAtLH(x+3*cosf(a),y-0.2f,z+3*sinf(a),x,y,z);
        const auto proj=D3DMath::BuildPerspectiveFovLH(0.65f,1,nearPlanes[version],1000);
        const auto world=D3DMath::BuildIdentity();
        device->SetTransform(D3DTS_VIEW,&view); device->SetTransform(D3DTS_PROJECTION,&proj);
        device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff204060,1,0);
        device->BeginScene();
        ModelRenderer::Context context; context.device=device;
        context.cameraPosition[0]=x+3*cosf(a); context.cameraPosition[1]=y-0.2f; context.cameraPosition[2]=z+3*sinf(a);
        ModelRenderer::PrepareFixedFunctionPass(context,world);
        ModelRenderer::DrawGeometry(context,model,world);
        device->EndScene();
        IDirect3DSurface9 *target=nullptr,*surface=nullptr;
        device->GetRenderTarget(0,&target); D3DSURFACE_DESC desc={}; target->GetDesc(&desc);
        device->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&surface,nullptr);
        if (surface && SUCCEEDED(device->GetRenderTargetData(target,surface)))
        {
            D3DLOCKED_RECT pixels={};
            if (SUCCEEDED(surface->LockRect(&pixels,nullptr,D3DLOCK_READONLY)))
            {
                BITMAPFILEHEADER header={}; BITMAPINFOHEADER info={};
                info.biSize=sizeof(info); info.biWidth=desc.Width; info.biHeight=-(LONG)desc.Height;
                info.biPlanes=1; info.biBitCount=32;
                header.bfType=0x4d42; header.bfOffBits=sizeof(header)+sizeof(info);
                header.bfSize=header.bfOffBits+desc.Width*desc.Height*4;
                std::ofstream out("tests/bin/sign-rendering/"+name+std::to_string(version)+".bmp",std::ios::binary);
                out.write((char*)&header,sizeof(header)); out.write((char*)&info,sizeof(info));
                for (UINT row=0; row<desc.Height; ++row) out.write((char*)pixels.pBits+row*pixels.Pitch,desc.Width*4);
                for (UINT row=0; row<desc.Height; ++row) {
                    const auto* colors = reinterpret_cast<const DWORD*>(static_cast<const char*>(pixels.pBits)+row*pixels.Pitch);
                    captures[version].insert(captures[version].end(), colors, colors+desc.Width);
                }
                if (!out.good()) return 5;
                surface->UnlockRect();
            }
        }
        if(surface)surface->Release(); if(target)target->Release();
    }
    if (captures[0].size()!=512*512 || captures[1].size()!=captures[0].size() || captures[2].size()!=captures[0].size()) return 6;
    size_t errors[2]={};
    for (int version=0;version<2;++version)
        for (size_t pixel=0;pixel<captures[2].size();++pixel) {
            int difference=0;
            for (int shift : {0,8,16}) difference += abs(int((captures[version][pixel]>>shift)&255)-int((captures[2][pixel]>>shift)&255));
            if (difference>24) ++errors[version];
        }
    std::cout << "Pixels differing from high-precision reference: old=" << errors[0] << " corrected=" << errors[1] << '\n';
    gpFF11Opts = nullptr;
    // The captured retail sign must expose the original regression, and the
    // production near plane must remove its large gray patches.
    return errors[0] > 100 && errors[1] < errors[0]/4 ? 0 : 7;
}

