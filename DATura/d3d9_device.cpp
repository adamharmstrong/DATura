#include "stdafx.h"
#include "d3d9_device.h"

#include <algorithm>
#include <cstdio>

namespace
{
struct ScreenVertex
{
    float x, y, z, rhw, u, v;
};

bool CompileVertexShader(IDirect3DDevice9* device, const char* source,
                         IDirect3DVertexShader9** outShader)
{
    ID3DBlob* code = nullptr;
    ID3DBlob* errors = nullptr;
    const HRESULT compiled = D3DCompile(source, strlen(source), "DATuraAA", nullptr,
        nullptr, "main", "vs_2_0", 0, 0, &code, &errors);
    if (errors) errors->Release();
    if (FAILED(compiled) || !code)
        return false;
    const HRESULT created = device->CreateVertexShader(
        static_cast<const DWORD*>(code->GetBufferPointer()), outShader);
    code->Release();
    return SUCCEEDED(created);
}

bool CompilePixelShader(IDirect3DDevice9* device, const char* source,
                        IDirect3DPixelShader9** outShader)
{
    if (!device || !source || !outShader)
        return false;
    ID3DBlob* code = nullptr;
    ID3DBlob* errors = nullptr;
    const HRESULT compileResult = D3DCompile(source, strlen(source), "DATuraAA", nullptr,
        nullptr, "main", "ps_2_0", 0, 0, &code, &errors);
    if (FAILED(compileResult))
    {
        if (errors) OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
        if (errors) errors->Release();
        return false;
    }
    const HRESULT createResult = device->CreatePixelShader(
        static_cast<const DWORD*>(code->GetBufferPointer()), outShader);
    code->Release();
    if (errors) errors->Release();
    return SUCCEEDED(createResult);
}

HRESULT DrawFullscreen(IDirect3DDevice9* device, IDirect3DVertexShader9* vertexShader,
                       IDirect3DVertexDeclaration9* declaration, IDirect3DPixelShader9* shader,
                       IDirect3DTexture9* texture0, IDirect3DTexture9* texture1,
                       const int width, const int height)
{
    const ScreenVertex vertices[] =
    {
        {-1.0f,  1.0f, 0.0f, 1.0f, 0.0f, 0.0f},
        { 1.0f,  1.0f, 0.0f, 1.0f, 1.0f, 0.0f},
        {-1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f},
        { 1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f}
    };
    const float pixelSize[4] = {1.0f / width, 1.0f / height, (float)width, (float)height};
    const D3DVIEWPORT9 viewport =
    {
        0, 0, static_cast<DWORD>(width), static_cast<DWORD>(height), 0.0f, 1.0f
    };
    device->SetViewport(&viewport);
    device->SetVertexShader(vertexShader);
    device->SetVertexDeclaration(declaration);
    device->SetPixelShader(shader);
    device->SetPixelShaderConstantF(0, pixelSize, 1);
    device->SetTexture(0, texture0);
    device->SetTexture(1, texture1);
    device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
    device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    device->SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1);
    device->SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    device->SetRenderState(D3DRS_ZENABLE, FALSE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
    device->SetRenderState(D3DRS_COLORWRITEENABLE,
        D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
        D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
    device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, FALSE);
    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);
    device->SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    device->SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    device->SetSamplerState(1, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    device->SetSamplerState(1, D3DSAMP_SRGBTEXTURE, FALSE);
    const HRESULT result = device->DrawPrimitiveUP(
        D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(ScreenVertex));
    device->SetTexture(0, nullptr);
    device->SetTexture(1, nullptr);
    device->SetPixelShader(nullptr);
    device->SetVertexShader(nullptr);
    return result;
}

void BuildPresentParameters(const int width, const int height, const bool windowed,
                            D3DPRESENT_PARAMETERS& outParameters)
{
    ZeroMemory(&outParameters, sizeof(outParameters));
    outParameters.Windowed = windowed ? TRUE : FALSE;
    outParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    outParameters.BackBufferFormat = windowed ? D3DFMT_UNKNOWN : D3DFMT_X8R8G8B8;
    outParameters.BackBufferWidth = width > 0 ? width : 1280;
    outParameters.BackBufferHeight = height > 0 ? height : 720;
    outParameters.EnableAutoDepthStencil = TRUE;
    outParameters.AutoDepthStencilFormat = D3DFMT_D24S8;
    outParameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    // Title/nation text is rendered with GDI directly onto the completed D3D
    // frame so textures and labels are presented atomically. GetDC on a swap-
    // chain back buffer requires this flag.
    outParameters.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
}

void ApplyWindowMode(const HWND window, const bool borderless, const bool fullscreen,
                     const int windowedClientWidth, const int windowedClientHeight)
{
    if (!window)
        return;

    DWORD style = WS_OVERLAPPEDWINDOW;
    constexpr DWORD extendedStyle = 0;
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    int width = windowedClientWidth;
    int height = windowedClientHeight;

    RECT workArea = {};
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &workArea, 0);
    RECT monitorArea = workArea;
    const HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (monitor && GetMonitorInfoA(monitor, &monitorInfo))
        monitorArea = monitorInfo.rcMonitor;

    if (borderless || fullscreen)
    {
        style = WS_POPUP;
        const RECT& target = fullscreen ? monitorArea : workArea;
        x = target.left;
        y = target.top;
        width = target.right - target.left;
        height = target.bottom - target.top;
    }
    else
    {
        RECT windowRect = { 0, 0, windowedClientWidth, windowedClientHeight };
        AdjustWindowRect(&windowRect, style, TRUE);
        width = windowRect.right - windowRect.left;
        height = windowRect.bottom - windowRect.top;
        x = workArea.left + ((workArea.right - workArea.left) - width) / 2;
        y = workArea.top + ((workArea.bottom - workArea.top) - height) / 2;
    }

    SetWindowLongPtrA(window, GWL_STYLE, static_cast<LONG_PTR>(style));
    SetWindowLongPtrA(window, GWL_EXSTYLE, static_cast<LONG_PTR>(extendedStyle));
    SetWindowPos(window, nullptr, x, y, width, height,
                 SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
}
}

namespace D3D9Device
{
Runtime::~Runtime()
{
    Shutdown();
}

void Runtime::SetDefaultPoolCallbacks(const DefaultPoolCallback releaseResources,
                                      const DefaultPoolCallback recreateResources)
{
    releaseResources_ = releaseResources;
    recreateResources_ = recreateResources;
}

InitializeResult Runtime::Initialize(const HWND window, const DisplayConfiguration& display)
{
    Shutdown();
    window_ = window;
    display_ = display;
    BuildPresentParameters(display.renderWidth, display.renderHeight,
                           !display.fullscreen, parameters_);

    d3d_ = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d_)
    {
        window_ = nullptr;
        return InitializeResult::Direct3DUnavailable;
    }

    struct Attempt
    {
        D3DDEVTYPE deviceType;
        DWORD vertexProcessingFlags;
        const char* description;
    };
    const Attempt attempts[] =
    {
        { D3DDEVTYPE_HAL, D3DCREATE_HARDWARE_VERTEXPROCESSING, "HAL + HW VP" },
        { D3DDEVTYPE_HAL, D3DCREATE_SOFTWARE_VERTEXPROCESSING, "HAL + SW VP" },
        { D3DDEVTYPE_REF, D3DCREATE_SOFTWARE_VERTEXPROCESSING, "REF + SW VP" },
    };

    for (const Attempt& attempt : attempts)
    {
        const HRESULT result = d3d_->CreateDevice(
            D3DADAPTER_DEFAULT,
            attempt.deviceType,
            window_,
            attempt.vertexProcessingFlags,
            &parameters_,
            &device_);
        if (SUCCEEDED(result))
        {
            char message[128] = {};
            sprintf_s(message, "D3D9 device created (%s)\n", attempt.description);
            OutputDebugStringA(message);
            deviceLost_ = false;
            CreateMultisampleTargets();
            CreatePostProcessResources();
            return InitializeResult::Success;
        }
    }

    d3d_->Release();
    d3d_ = nullptr;
    window_ = nullptr;
    ZeroMemory(&parameters_, sizeof(parameters_));
    return InitializeResult::NoSupportedDevice;
}

bool Runtime::ApplyDisplayConfiguration(const DisplayConfiguration& display)
{
    if (!window_)
        return false;

    display_ = display;
    applyingDisplayConfiguration_ = true;
    ApplyWindowMode(window_, display.borderless, display.fullscreen,
                    display.width, display.height);
    applyingDisplayConfiguration_ = false;

    if (!device_)
        return true;
    return ResetWithBackBufferSize(display.renderWidth, display.renderHeight);
}

bool Runtime::Resize(const int clientWidth, const int clientHeight)
{
    if (!device_)
        return false;
    if (applyingDisplayConfiguration_)
        return true;

    const bool fixedWindow = display_.borderless || display_.fullscreen;
    const int renderWidth = fixedWindow ? display_.renderWidth :
        (display_.width > 0 ? MulDiv(clientWidth, display_.renderWidth, display_.width) : clientWidth);
    const int renderHeight = fixedWindow ? display_.renderHeight :
        (display_.height > 0 ? MulDiv(clientHeight, display_.renderHeight, display_.height) : clientHeight);
    return ResetWithBackBufferSize((std::max)(1, renderWidth), (std::max)(1, renderHeight));
}

FrameStatus Runtime::PrepareFrame()
{
    if (!device_)
        return FrameStatus::Uninitialized;

    const HRESULT result = device_->TestCooperativeLevel();
    if (result == D3DERR_DEVICELOST)
    {
        deviceLost_ = true;
        return FrameStatus::DeviceLost;
    }
    if (result == D3DERR_DEVICENOTRESET)
    {
        if (!ResetCurrentParameters())
        {
            deviceLost_ = true;
            return FrameStatus::ResetFailed;
        }
    }
    else if (FAILED(result))
    {
        deviceLost_ = true;
        return FrameStatus::ResetFailed;
    }

    deviceLost_ = false;
    frameResolved_ = true;
    device_->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS,
        multisampleColor_ ? TRUE : FALSE);
    device_->SetRenderState(D3DRS_MULTISAMPLEMASK, 0xffffffffu);
    if (multisampleColor_ && multisampleDepth_)
    {
        // The swap-chain's automatic depth surface is non-multisampled.
        // Detach it before selecting the MSAA color target; otherwise D3D9
        // can retain an incompatible color/depth pair. Screen-space title
        // rendering may still work in that state, but every depth-tested zone
        // draw is rejected.
        const HRESULT detachResult = device_->SetDepthStencilSurface(nullptr);
        const HRESULT colorResult = SUCCEEDED(detachResult) ?
            device_->SetRenderTarget(0, multisampleColor_) : detachResult;
        const HRESULT depthResult = SUCCEEDED(colorResult) ?
            device_->SetDepthStencilSurface(multisampleDepth_) : colorResult;
        if (FAILED(depthResult))
        {
            return FrameStatus::ResetFailed;
        }
        frameResolved_ = false;
    }
    else if (sceneTexture_)
    {
        IDirect3DSurface9* sceneSurface = nullptr;
        if (FAILED(sceneTexture_->GetSurfaceLevel(0, &sceneSurface)) || !sceneSurface)
            return FrameStatus::ResetFailed;
        const HRESULT targetResult = device_->SetRenderTarget(0, sceneSurface);
        sceneSurface->Release();
        if (FAILED(targetResult))
            return FrameStatus::ResetFailed;
        frameResolved_ = false;
    }
    return FrameStatus::Ready;
}

HRESULT Runtime::ResolveFrame()
{
    if (!device_)
        return D3DERR_INVALIDCALL;
    if (frameResolved_ || (!multisampleColor_ && !sceneTexture_))
        return D3D_OK;

    IDirect3DSurface9* backBuffer = nullptr;
    const HRESULT backBufferResult = device_->GetBackBuffer(
        0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
    if (FAILED(backBufferResult) || !backBuffer)
        return FAILED(backBufferResult) ? backBufferResult : D3DERR_INVALIDCALL;

    HRESULT result = D3D_OK;
    IDirect3DSurface9* previousDepth = nullptr;
    if (!multisampleColor_)
        device_->GetDepthStencilSurface(&previousDepth);
    device_->SetDepthStencilSurface(nullptr);
    IDirect3DSurface9* sceneSurface = nullptr;
    if (sceneTexture_)
        sceneTexture_->GetSurfaceLevel(0, &sceneSurface);
    bool bypassPostProcess = false;
    if (multisampleColor_)
    {
        // Resolve to the swap-chain surface first. Some D3D9 drivers return
        // success for an MSAA-to-texture resolve but transfer only the clear
        // color. The backbuffer resolve is the interoperable path; the
        // resolved image can then be copied into the post-process texture.
        result = device_->StretchRect(
            multisampleColor_, nullptr, backBuffer, nullptr, D3DTEXF_NONE);
        if (SUCCEEDED(result) && sceneSurface)
        {
            const HRESULT copyResult = device_->StretchRect(
                backBuffer, nullptr, sceneSurface, nullptr, D3DTEXF_NONE);
            if (FAILED(copyResult))
                bypassPostProcess = true; // The resolved backbuffer remains valid.
        }
    }
    if (SUCCEEDED(result) && sceneTexture_ && !bypassPostProcess)
    {
        result = ApplyPostProcess(backBuffer);
        if (SUCCEEDED(result) && sceneSurface)
        {
            D3DLOCKED_RECT locked = {};
            if (SUCCEEDED(backBuffer->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
            {
                D3DSURFACE_DESC outputDesc = {};
                backBuffer->GetDesc(&outputDesc);
                const DWORD first = *static_cast<const DWORD*>(locked.pBits);
                bool uniform = true;
                for (UINT y = 0; uniform && y < outputDesc.Height; y += 32)
                {
                    const DWORD* row = reinterpret_cast<const DWORD*>(
                        static_cast<const BYTE*>(locked.pBits) + y * locked.Pitch);
                    for (UINT x = 0; x < outputDesc.Width; x += 32)
                        if (row[x] != first) { uniform = false; break; }
                }
                backBuffer->UnlockRect();
                if (uniform)
                    result = device_->StretchRect(
                        sceneSurface, nullptr, backBuffer, nullptr, D3DTEXF_NONE);
            }
        }
        // A shader/resource failure must not turn the entire frame black.
        // The unprocessed scene is still a valid image, so copy it directly
        // to the swap-chain back buffer as a safe fallback.
        if (FAILED(result) && sceneSurface)
        {
            device_->SetPixelShader(nullptr);
            device_->SetTexture(0, nullptr);
            device_->SetTexture(1, nullptr);
            device_->SetRenderTarget(0, backBuffer);
            result = device_->StretchRect(
                sceneSurface, nullptr, backBuffer, nullptr, D3DTEXF_NONE);
        }
    }
    else if (SUCCEEDED(result))
        result = device_->SetRenderTarget(0, backBuffer);
    // With post-process AA and no MSAA, this is the automatic depth surface
    // used by the next scene. Leaving it detached makes later 3D draws fail.
    if (previousDepth)
    {
        const HRESULT depthResult = device_->SetDepthStencilSurface(previousDepth);
        previousDepth->Release();
        if (SUCCEEDED(result) && FAILED(depthResult))
            result = depthResult;
    }
    if (sceneSurface) sceneSurface->Release();
    backBuffer->Release();
    if (SUCCEEDED(result))
        frameResolved_ = true;
    return result;
}

HRESULT Runtime::Present()
{
    if (!device_)
        return D3DERR_INVALIDCALL;

    const HRESULT resolveResult = ResolveFrame();
    if (FAILED(resolveResult))
        return resolveResult;
    const HRESULT result = device_->Present(nullptr, nullptr, nullptr, nullptr);
    if (result == D3DERR_DEVICELOST)
        deviceLost_ = true;
    return result;
}

void Runtime::Shutdown()
{
    ReleaseMultisampleTargets();
    ReleasePostProcessResources();
    if (device_)
        ReleaseDefaultPoolResources();
    if (device_)
    {
        device_->Release();
        device_ = nullptr;
    }
    if (d3d_)
    {
        d3d_->Release();
        d3d_ = nullptr;
    }

    window_ = nullptr;
    display_ = {};
    deviceLost_ = false;
    applyingDisplayConfiguration_ = false;
    frameResolved_ = true;
    ZeroMemory(&parameters_, sizeof(parameters_));
}

bool Runtime::ResetWithBackBufferSize(const int width, const int height)
{
    BuildPresentParameters(width, height, !display_.fullscreen, parameters_);
    return ResetCurrentParameters();
}

bool Runtime::ResetCurrentParameters()
{
    if (!device_)
        return false;

    ReleaseMultisampleTargets();
    ReleasePostProcessResources();
    ReleaseDefaultPoolResources();
    const HRESULT result = device_->Reset(&parameters_);
    if (FAILED(result))
        return false;

    RecreateDefaultPoolResources();
    CreateMultisampleTargets();
    CreatePostProcessResources();
    deviceLost_ = false;
    return true;
}

bool Runtime::CreateMultisampleTargets()
{
    ReleaseMultisampleTargets();
    if (!device_ || display_.antiAliasingSamples < 2)
        return true;

    IDirect3DSurface9* backBuffer = nullptr;
    if (FAILED(device_->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)) || !backBuffer)
        return false;
    D3DSURFACE_DESC desc = {};
    const HRESULT descResult = backBuffer->GetDesc(&desc);
    backBuffer->Release();
    if (FAILED(descResult))
        return false;

    for (int samples = display_.antiAliasingSamples; samples >= 2; samples /= 2)
    {
        const D3DMULTISAMPLE_TYPE type = static_cast<D3DMULTISAMPLE_TYPE>(samples);
        if (SUCCEEDED(device_->CreateRenderTarget(desc.Width, desc.Height, desc.Format,
                type, 0, FALSE, &multisampleColor_, nullptr)) &&
            SUCCEEDED(device_->CreateDepthStencilSurface(desc.Width, desc.Height,
                parameters_.AutoDepthStencilFormat, type, 0, TRUE,
                &multisampleDepth_, nullptr)))
        {
            return true;
        }
        ReleaseMultisampleTargets();
    }
    return false;
}

void Runtime::ReleaseMultisampleTargets()
{
    if (multisampleDepth_)
    {
        multisampleDepth_->Release();
        multisampleDepth_ = nullptr;
    }
    if (multisampleColor_)
    {
        multisampleColor_->Release();
        multisampleColor_ = nullptr;
    }
    frameResolved_ = true;
}

bool Runtime::CreatePostProcessResources()
{
    ReleasePostProcessResources();
    if (!device_ || display_.postProcessAntiAliasingMode == 0)
        return true;

    IDirect3DSurface9* backBuffer = nullptr;
    if (FAILED(device_->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)) || !backBuffer)
        return false;
    D3DSURFACE_DESC desc = {};
    const HRESULT descResult = backBuffer->GetDesc(&desc);
    backBuffer->Release();
    if (FAILED(descResult) || FAILED(device_->CreateTexture(desc.Width, desc.Height, 1,
            D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
            &sceneTexture_, nullptr)))
        return false;

    static const char vertexSource[] =
        "struct O{float4 p:POSITION;float2 uv:TEXCOORD0;};"
        "O main(float4 p:POSITION,float2 uv:TEXCOORD0){O o;o.p=p;o.uv=uv;return o;}";
    const D3DVERTEXELEMENT9 declaration[] =
    {
        {0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
        D3DDECL_END()
    };
    if (!CompileVertexShader(device_, vertexSource, &fullscreenVertexShader_) ||
        FAILED(device_->CreateVertexDeclaration(declaration, &fullscreenVertexDeclaration_)))
    {
        ReleasePostProcessResources();
        return false;
    }

    static const char fxaaSource[] =
        "sampler2D image:register(s0); float4 px:register(c0);"
        "float l(float3 c){return dot(c,float3(.299,.587,.114));}"
        "float4 main(float2 uv:TEXCOORD0):COLOR0{"
        "float3 c=tex2D(image,uv).rgb,n=tex2D(image,uv+float2(0,-px.y)).rgb;"
        "float3 s=tex2D(image,uv+float2(0,px.y)).rgb,e=tex2D(image,uv+float2(px.x,0)).rgb,w=tex2D(image,uv-float2(px.x,0)).rgb;"
        "float lc=l(c),lo=min(lc,min(min(l(n),l(s)),min(l(e),l(w)))),hi=max(lc,max(max(l(n),l(s)),max(l(e),l(w))));"
        "float span=hi-lo;if(span<max(.0312,hi*.125))return float4(c,1);"
        "float2 dir=float2(-(l(n)-l(s)),l(e)-l(w));dir=normalize(dir+1e-5)*px.xy*.75;"
        "return float4((tex2D(image,uv-dir).rgb+tex2D(image,uv+dir).rgb)*.5,1);}";

    if (display_.postProcessAntiAliasingMode == 1)
        return CompilePixelShader(device_, fxaaSource, &fxaaShader_);

    static const char edgeSource[] =
        "sampler2D image:register(s0);float4 px:register(c0);"
        "float l(float3 c){return dot(c,float3(.299,.587,.114));}"
        "float4 main(float2 uv:TEXCOORD0):COLOR0{float c=l(tex2D(image,uv).rgb);"
        "float2 d=abs(c-float2(l(tex2D(image,uv+float2(px.x,0)).rgb),l(tex2D(image,uv+float2(0,px.y)).rgb)));"
        "d=step(.05,d);return float4(d,0,1);}";
    static const char blendSource[] =
        "sampler2D edges:register(s0);float4 px:register(c0);"
        "float4 main(float2 uv:TEXCOORD0):COLOR0{float2 e=tex2D(edges,uv).rg;"
        "float h=e.x*(tex2D(edges,uv-float2(px.x,0)).x+tex2D(edges,uv+float2(px.x,0)).x);"
        "float v=e.y*(tex2D(edges,uv-float2(0,px.y)).y+tex2D(edges,uv+float2(0,px.y)).y);"
        "return float4(saturate(h*.25),saturate(v*.25),0,1);}";
    static const char neighborhoodSource[] =
        "sampler2D image:register(s0);sampler2D weights:register(s1);float4 px:register(c0);"
        "float4 main(float2 uv:TEXCOORD0):COLOR0{float2 w=tex2D(weights,uv).rg;float3 c=tex2D(image,uv).rgb;"
        "float3 h=(tex2D(image,uv-float2(px.x,0)).rgb+tex2D(image,uv+float2(px.x,0)).rgb)*.5;"
        "float3 v=(tex2D(image,uv-float2(0,px.y)).rgb+tex2D(image,uv+float2(0,px.y)).rgb)*.5;"
        "c=lerp(c,h,w.x);c=lerp(c,v,w.y);return float4(c,1);}";

    if (FAILED(device_->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_RENDERTARGET,
            D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &edgeTexture_, nullptr)) ||
        FAILED(device_->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_RENDERTARGET,
            D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &blendTexture_, nullptr)) ||
        !CompilePixelShader(device_, edgeSource, &smaaEdgeShader_) ||
        !CompilePixelShader(device_, blendSource, &smaaBlendShader_) ||
        !CompilePixelShader(device_, neighborhoodSource, &smaaNeighborhoodShader_))
    {
        ReleasePostProcessResources();
        return false;
    }
    return true;
}

HRESULT Runtime::ApplyPostProcess(IDirect3DSurface9* backBuffer)
{
    if (!device_ || !sceneTexture_ || !backBuffer)
        return D3DERR_INVALIDCALL;

    // The zone renderer is predominantly fixed-function D3D9, while this
    // pass uses programmable shaders and screen-space state. Preserve the
    // complete device state so the AA pass cannot poison the following
    // frame. The title screen happens to rebuild nearly all of its state;
    // normal zone rendering intentionally relies on more persistent state.
    IDirect3DStateBlock9* savedState = nullptr;
    HRESULT result = device_->CreateStateBlock(D3DSBT_ALL, &savedState);
    if (FAILED(result) || !savedState)
        return FAILED(result) ? result : D3DERR_INVALIDCALL;
    result = savedState->Capture();
    if (FAILED(result))
    {
        savedState->Release();
        return result;
    }

    D3DSURFACE_DESC desc = {};
    backBuffer->GetDesc(&desc);
    // The scene texture was the previous render target. Detach it before
    // BeginScene so D3D9 can legally bind that same resource as sampler 0.
    // Switching away only after BeginScene leaves some drivers treating the
    // texture as render-target-bound and every lookup returns one texel.
    result = device_->SetRenderTarget(0, backBuffer);
    if (FAILED(result))
    {
        savedState->Apply();
        savedState->Release();
        return result;
    }
    result = device_->BeginScene();
    if (FAILED(result))
    {
        savedState->Apply();
        savedState->Release();
        return result;
    }

    if (display_.postProcessAntiAliasingMode == 1 && fxaaShader_)
    {
        device_->SetRenderTarget(0, backBuffer);
        result = DrawFullscreen(device_, fullscreenVertexShader_, fullscreenVertexDeclaration_,
                                fxaaShader_, sceneTexture_, nullptr,
                                (int)desc.Width, (int)desc.Height);
    }
    else if (smaaEdgeShader_ && smaaBlendShader_ && smaaNeighborhoodShader_)
    {
        IDirect3DSurface9* edgeSurface = nullptr;
        IDirect3DSurface9* blendSurface = nullptr;
        edgeTexture_->GetSurfaceLevel(0, &edgeSurface);
        blendTexture_->GetSurfaceLevel(0, &blendSurface);
        device_->SetRenderTarget(0, edgeSurface);
        result = DrawFullscreen(device_, fullscreenVertexShader_, fullscreenVertexDeclaration_,
                                smaaEdgeShader_, sceneTexture_, nullptr,
                                (int)desc.Width, (int)desc.Height);
        if (SUCCEEDED(result))
        {
            device_->SetRenderTarget(0, blendSurface);
            result = DrawFullscreen(device_, fullscreenVertexShader_, fullscreenVertexDeclaration_,
                                    smaaBlendShader_, edgeTexture_, nullptr,
                                    (int)desc.Width, (int)desc.Height);
        }
        if (SUCCEEDED(result))
        {
            device_->SetRenderTarget(0, backBuffer);
            result = DrawFullscreen(device_, fullscreenVertexShader_, fullscreenVertexDeclaration_,
                                    smaaNeighborhoodShader_, sceneTexture_, blendTexture_,
                                    (int)desc.Width, (int)desc.Height);
        }
        if (edgeSurface) edgeSurface->Release();
        if (blendSurface) blendSurface->Release();
    }
    const HRESULT endResult = device_->EndScene();
    const HRESULT restoreResult = savedState->Apply();
    savedState->Release();
    // Render targets are not guaranteed to be part of a D3D9 state block.
    // ResolveFrame promises that its caller receives the swap-chain target.
    const HRESULT targetResult = device_->SetRenderTarget(0, backBuffer);
    if (SUCCEEDED(result) && FAILED(endResult)) result = endResult;
    if (SUCCEEDED(result) && FAILED(restoreResult)) result = restoreResult;
    if (SUCCEEDED(result) && FAILED(targetResult)) result = targetResult;
    return result;
}

void Runtime::ReleasePostProcessResources()
{
    if (smaaNeighborhoodShader_) { smaaNeighborhoodShader_->Release(); smaaNeighborhoodShader_ = nullptr; }
    if (smaaBlendShader_) { smaaBlendShader_->Release(); smaaBlendShader_ = nullptr; }
    if (smaaEdgeShader_) { smaaEdgeShader_->Release(); smaaEdgeShader_ = nullptr; }
    if (fxaaShader_) { fxaaShader_->Release(); fxaaShader_ = nullptr; }
    if (fullscreenVertexDeclaration_) { fullscreenVertexDeclaration_->Release(); fullscreenVertexDeclaration_ = nullptr; }
    if (fullscreenVertexShader_) { fullscreenVertexShader_->Release(); fullscreenVertexShader_ = nullptr; }
    if (blendTexture_) { blendTexture_->Release(); blendTexture_ = nullptr; }
    if (edgeTexture_) { edgeTexture_->Release(); edgeTexture_ = nullptr; }
    if (sceneTexture_) { sceneTexture_->Release(); sceneTexture_ = nullptr; }
}

void Runtime::ReleaseDefaultPoolResources()
{
    if (releaseResources_)
        releaseResources_();
}

void Runtime::RecreateDefaultPoolResources()
{
    if (recreateResources_)
        recreateResources_();
}

void GetViewportOrClientSize(IDirect3DDevice9* device, const HWND window,
                             int* outWidth, int* outHeight)
{
    int width = 0;
    int height = 0;
    if (device)
    {
        D3DVIEWPORT9 viewport = {};
        if (SUCCEEDED(device->GetViewport(&viewport)))
        {
            width = (int)viewport.Width;
            height = (int)viewport.Height;
        }
    }
    if ((width <= 0 || height <= 0) && window)
    {
        RECT client = {};
        GetClientRect(window, &client);
        width = client.right - client.left;
        height = client.bottom - client.top;
    }
    if (outWidth)
        *outWidth = width;
    if (outHeight)
        *outHeight = height;
}

POINT MapClientPointToViewport(const HWND window, const int viewportWidth,
                               const int viewportHeight, POINT point)
{
    if (!window)
        return point;
    RECT client = {};
    GetClientRect(window, &client);
    const int clientWidth = client.right - client.left;
    const int clientHeight = client.bottom - client.top;
    if (clientWidth > 0 && clientHeight > 0 && viewportWidth > 0 && viewportHeight > 0)
    {
        point.x = MulDiv(point.x, viewportWidth, clientWidth);
        point.y = MulDiv(point.y, viewportHeight, clientHeight);
    }
    return point;
}

int BeginGdiViewportMapping(const HDC hdc, const HWND window,
                            const int viewportWidth, const int viewportHeight)
{
    if (!hdc || !window || viewportWidth <= 0 || viewportHeight <= 0)
        return 0;
    RECT client = {};
    GetClientRect(window, &client);
    const int clientWidth = client.right - client.left;
    const int clientHeight = client.bottom - client.top;
    if (clientWidth <= 0 || clientHeight <= 0)
        return 0;

    const int savedDc = SaveDC(hdc);
    SetMapMode(hdc, MM_ANISOTROPIC);
    SetWindowOrgEx(hdc, 0, 0, nullptr);
    SetViewportOrgEx(hdc, 0, 0, nullptr);
    SetWindowExtEx(hdc, viewportWidth, viewportHeight, nullptr);
    SetViewportExtEx(hdc, clientWidth, clientHeight, nullptr);
    return savedDc;
}

void PrepareScreenSpaceUiRenderState(IDirect3DDevice9* device, const DWORD vertexFvf)
{
    // Screen-space overlays must not inherit world shader, depth/cull, or
    // texture-stage state from the preceding model render pass.
    device->SetVertexShader(nullptr);
    device->SetPixelShader(nullptr);
    device->SetFVF(vertexFvf);
    device->SetTexture(1, nullptr);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_ZENABLE, FALSE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
    device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
}

}
