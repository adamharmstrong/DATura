#include "stdafx.h"
#include "d3d9_device.h"

#include <algorithm>
#include <cstdio>
#include <memory>

namespace
{
struct ScreenVertex
{
    float x, y, z, rhw, u, v;
};

bool CompileVertexShader(IDirect3DDevice9* device, const char* source,
                         RendererBackend::Shader** outShader)
{
    if (!source || !outShader) return false;
    *outShader = D3D9Graphics::Context(device).CreateShader({
        RendererBackend::ShaderStage::Vertex, source, strlen(source), "DATuraAA", "main", "vs_2_0"});
    return *outShader != nullptr;
}

bool CompilePixelShader(IDirect3DDevice9* device, const char* source,
                        RendererBackend::Shader** outShader)
{
    if (!source || !outShader) return false;
    *outShader = D3D9Graphics::Context(device).CreateShader({
        RendererBackend::ShaderStage::Pixel, source, strlen(source), "DATuraAA", "main", "ps_2_0"});
    return *outShader != nullptr;
}

HRESULT DrawFullscreen(IDirect3DDevice9* device, RendererBackend::Shader* vertexShader,
                       RendererBackend::VertexLayout* declaration, RendererBackend::Shader* shader,
                       RendererBackend::Texture* texture0, RendererBackend::Texture* texture1,
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
    const RendererBackend::Viewport viewport =
    {
        0, 0, static_cast<DWORD>(width), static_cast<DWORD>(height), 0.0f, 1.0f
    };
    D3D9Graphics::Context(device).SetViewport(viewport);
    D3D9Graphics::Context(device).BindShader(RendererBackend::ShaderStage::Vertex, vertexShader);
    D3D9Graphics::Context(device).BindVertexLayout(declaration);
    D3D9Graphics::Context(device).BindShader(RendererBackend::ShaderStage::Pixel, shader);
    D3D9Graphics::Context(device).SetShaderConstants(RendererBackend::ShaderStage::Pixel, 0, pixelSize, 1);
    D3D9Graphics::Context(device).BindTexture(0, texture0);
    D3D9Graphics::Context(device).BindTexture(1, texture1);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::coordinateIndex, uint32_t(0));
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::transform, RendererBackend::TextureTransform::Disabled);
    D3D9Graphics::Context(device).SetTextureStageField(1, &RendererBackend::TextureStageState::coordinateIndex, uint32_t(1));
    D3D9Graphics::Context(device).SetTextureStageField(1, &RendererBackend::TextureStageState::transform, RendererBackend::TextureTransform::Disabled);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthTest, false);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthWrite, false);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::blend, false);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::alphaTest, false);
    D3D9Graphics::Context(device).SetStencilField(&RendererBackend::StencilState::enabled, false);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::cull, RendererBackend::Cull::None);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::scissor, false);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::clipPlaneMask, uint32_t(0));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::colorWriteMask, uint32_t(1u | 2u |
        4u | 8u));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::srgbWrite, false);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::separateAlphaBlend, false);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::multisample, false);
    D3D9Graphics::Context(device).SetSamplerField(0, &RendererBackend::SamplerState::minFilter, RendererBackend::TextureFilter::Linear);
    D3D9Graphics::Context(device).SetSamplerField(0, &RendererBackend::SamplerState::magFilter, RendererBackend::TextureFilter::Linear);
    D3D9Graphics::Context(device).SetSamplerField(0, &RendererBackend::SamplerState::addressU, RendererBackend::TextureAddress::Clamp);
    D3D9Graphics::Context(device).SetSamplerField(0, &RendererBackend::SamplerState::addressV, RendererBackend::TextureAddress::Clamp);
    D3D9Graphics::Context(device).SetSamplerField(0, &RendererBackend::SamplerState::mipFilter, RendererBackend::TextureFilter::None);
    D3D9Graphics::Context(device).SetSamplerField(0, &RendererBackend::SamplerState::srgb, false);
    D3D9Graphics::Context(device).SetSamplerField(1, &RendererBackend::SamplerState::minFilter, RendererBackend::TextureFilter::Linear);
    D3D9Graphics::Context(device).SetSamplerField(1, &RendererBackend::SamplerState::magFilter, RendererBackend::TextureFilter::Linear);
    D3D9Graphics::Context(device).SetSamplerField(1, &RendererBackend::SamplerState::addressU, RendererBackend::TextureAddress::Clamp);
    D3D9Graphics::Context(device).SetSamplerField(1, &RendererBackend::SamplerState::addressV, RendererBackend::TextureAddress::Clamp);
    D3D9Graphics::Context(device).SetSamplerField(1, &RendererBackend::SamplerState::mipFilter, RendererBackend::TextureFilter::None);
    D3D9Graphics::Context(device).SetSamplerField(1, &RendererBackend::SamplerState::srgb, false);
    const HRESULT result = D3D9Graphics::Context(device).DrawImmediate(
        RendererBackend::Primitive::TriangleStrip, 2, vertices, sizeof(ScreenVertex)) ? D3D_OK : E_FAIL;
    D3D9Graphics::Context(device).BindTexture(0, nullptr);
    D3D9Graphics::Context(device).BindTexture(1, nullptr);
    D3D9Graphics::Context(device).BindShader(RendererBackend::ShaderStage::Pixel, nullptr);
    D3D9Graphics::Context(device).BindShader(RendererBackend::ShaderStage::Vertex, nullptr);
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
            graphics_.SetDevice(device_);
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
    D3D9Graphics::Context(device_).SetPipelineField(&RendererBackend::PipelineState::multisample, ((multisampleColor_ ? TRUE : FALSE) != FALSE));
    D3D9Graphics::Context(device_).SetPipelineField(&RendererBackend::PipelineState::multisampleMask, uint32_t(0xffffffffu));
    if (multisampleColor_ && multisampleDepth_)
    {
        // The swap-chain's automatic depth surface is non-multisampled.
        // Detach it before selecting the MSAA color target; otherwise D3D9
        // can retain an incompatible color/depth pair. Screen-space title
        // rendering may still work in that state, but every depth-tested zone
        // draw is rejected.
        const HRESULT detachResult = graphics_.BindRenderTarget(nullptr,true) ? D3D_OK : E_FAIL;
        const HRESULT colorResult = SUCCEEDED(detachResult) ?
            (graphics_.BindRenderTarget(multisampleColor_) ? D3D_OK : E_FAIL) : detachResult;
        const HRESULT depthResult = SUCCEEDED(colorResult) ?
            (graphics_.BindRenderTarget(multisampleDepth_,true) ? D3D_OK : E_FAIL) : colorResult;
        if (FAILED(depthResult))
        {
            return FrameStatus::ResetFailed;
        }
        frameResolved_ = false;
    }
    else if (sceneTexture_)
    {
        if (!graphics_.BindRenderTarget(sceneTexture_))
            return FrameStatus::ResetFailed;
        frameResolved_ = false;
    }
    return FrameStatus::Ready;
}

HRESULT Runtime::ResolveFrame()
{
    if (!device_) return D3DERR_INVALIDCALL;
    if (frameResolved_ || (!multisampleColor_ && !sceneTexture_)) return D3D_OK;
    std::unique_ptr<RendererBackend::RenderTarget> backBuffer(graphics_.CaptureBackBuffer());
    if (!backBuffer) return E_FAIL;
    std::unique_ptr<RendererBackend::RenderTarget> previousDepth;
    if (!multisampleColor_) previousDepth.reset(graphics_.CaptureRenderTarget(true));
    graphics_.BindRenderTarget(nullptr,true);
    HRESULT result=D3D_OK;
    bool bypassPostProcess=false;
    if (multisampleColor_)
    {
        // Preserve the driver-compatible MSAA -> swap chain -> scene texture path.
        result=graphics_.CopyRenderTarget(multisampleColor_,backBuffer.get()) ? D3D_OK : E_FAIL;
        if (SUCCEEDED(result) && sceneTexture_)
            bypassPostProcess=!graphics_.CopyRenderTarget(backBuffer.get(),sceneTexture_);
    }
    if (SUCCEEDED(result) && sceneTexture_ && !bypassPostProcess)
    {
        result=ApplyPostProcess(backBuffer.get());
        if (SUCCEEDED(result))
        {
            RendererBackend::TargetReadMapping locked;
            if (backBuffer->MapRead(locked))
            {
                const auto desc=backBuffer->Description();
                const uint32_t first=*static_cast<const uint32_t*>(locked.pixels);
                bool uniform=true;
                for(uint32_t y=0;uniform && y<desc.height;y+=32)
                {
                    const auto* row=reinterpret_cast<const uint32_t*>(
                        static_cast<const unsigned char*>(locked.pixels)+y*locked.rowPitch);
                    for(uint32_t x=0;x<desc.width;x+=32)
                        if(row[x]!=first) { uniform=false; break; }
                }
                backBuffer->UnmapRead();
                if(uniform)
                    result=graphics_.CopyRenderTarget(sceneTexture_,backBuffer.get()) ? D3D_OK : E_FAIL;
            }
        }
        // Keep the unprocessed scene available when a postprocessing pass fails.
        if(FAILED(result))
        {
            graphics_.BindShader(RendererBackend::ShaderStage::Pixel,nullptr);
            graphics_.BindTexture(0,nullptr); graphics_.BindTexture(1,nullptr);
            graphics_.BindRenderTarget(backBuffer.get());
            result=graphics_.CopyRenderTarget(sceneTexture_,backBuffer.get()) ? D3D_OK : E_FAIL;
        }
    }
    else if(SUCCEEDED(result))
        result=graphics_.BindRenderTarget(backBuffer.get()) ? D3D_OK : E_FAIL;
    if(previousDepth)
    {
        const bool restored=graphics_.BindRenderTarget(previousDepth.get(),true);
        if(SUCCEEDED(result) && !restored) result=E_FAIL;
    }
    if(SUCCEEDED(result)) frameResolved_=true;
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
    graphics_.SetDevice(nullptr);
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
    if(!device_ || display_.antiAliasingSamples<2) return true;
    std::unique_ptr<RendererBackend::RenderTarget> backBuffer(graphics_.CaptureBackBuffer());
    if(!backBuffer) return false;
    const auto desc=backBuffer->Description();
    for(int samples=display_.antiAliasingSamples;samples>=2;samples/=2)
    {
        multisampleColor_=graphics_.CreateRenderTarget(
            {desc.width,desc.height,desc.format,uint32_t(samples)});
        if(multisampleColor_)
            multisampleDepth_=graphics_.CreateRenderTarget(
                {desc.width,desc.height,RendererBackend::TargetFormat::Depth24Stencil8,uint32_t(samples)});
        if(multisampleColor_ && multisampleDepth_) return true;
        ReleaseMultisampleTargets();
    }
    return false;
}

void Runtime::ReleaseMultisampleTargets()
{
    if (multisampleDepth_)
    {
        delete multisampleDepth_;
        multisampleDepth_ = nullptr;
    }
    if (multisampleColor_)
    {
        delete multisampleColor_;
        multisampleColor_ = nullptr;
    }
    frameResolved_ = true;
}

bool Runtime::CreatePostProcessResources()
{
    ReleasePostProcessResources();
    if (!device_ || display_.postProcessAntiAliasingMode == 0)
        return true;

    std::unique_ptr<RendererBackend::RenderTarget> backBuffer(graphics_.CaptureBackBuffer());
    if (!backBuffer) return false;
    const auto desc=backBuffer->Description();
    if (!(sceneTexture_ = graphics_.CreateRenderTarget(
            {desc.width, desc.height, RendererBackend::TargetFormat::Bgra8, 0, 0, true})))
        return false;

    static const char vertexSource[] =
        "struct O{float4 p:POSITION;float2 uv:TEXCOORD0;};"
        "O main(float4 p:POSITION,float2 uv:TEXCOORD0){O o;o.p=p;o.uv=uv;return o;}";
    const RendererBackend::VertexElement declaration[] =
    {
        {0, RendererBackend::VertexFormat::Float4, RendererBackend::VertexSemantic::Position},
        {16, RendererBackend::VertexFormat::Float2, RendererBackend::VertexSemantic::TexCoord}
    };
    if (!CompileVertexShader(device_, vertexSource, &fullscreenVertexShader_) ||
        !(fullscreenVertexDeclaration_ = graphics_.CreateVertexLayout(declaration, 2)))
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

    if (!(edgeTexture_ = graphics_.CreateRenderTarget(
            {desc.width, desc.height, RendererBackend::TargetFormat::Bgra8, 0, 0, true})) ||
        !(blendTexture_ = graphics_.CreateRenderTarget(
            {desc.width, desc.height, RendererBackend::TargetFormat::Bgra8, 0, 0, true})) ||
        !CompilePixelShader(device_, edgeSource, &smaaEdgeShader_) ||
        !CompilePixelShader(device_, blendSource, &smaaBlendShader_) ||
        !CompilePixelShader(device_, neighborhoodSource, &smaaNeighborhoodShader_))
    {
        ReleasePostProcessResources();
        return false;
    }
    return true;
}

HRESULT Runtime::ApplyPostProcess(RendererBackend::RenderTarget* backBuffer)
{
    if (!device_ || !sceneTexture_ || !backBuffer)
        return D3DERR_INVALIDCALL;

    // The zone renderer is predominantly fixed-function D3D9, while this
    // pass uses programmable shaders and screen-space state. Preserve the
    // complete device state so the AA pass cannot poison the following
    // frame. The title screen happens to rebuild nearly all of its state;
    // normal zone rendering intentionally relies on more persistent state.
    auto* savedState = graphics_.CapturePipelineState();
    if (!savedState) return E_FAIL;
    HRESULT result = D3D_OK;

    const auto desc=backBuffer->Description();
    // The scene texture was the previous render target. Detach it before
    // BeginScene so D3D9 can legally bind that same resource as sampler 0.
    // Switching away only after BeginScene leaves some drivers treating the
    // texture as render-target-bound and every lookup returns one texel.
    result = (graphics_.BindRenderTarget(backBuffer) ? D3D_OK : E_FAIL);
    if (FAILED(result))
    {
        (savedState->Restore() ? D3D_OK : E_FAIL);
        delete savedState;
        return result;
    }
    result = (graphics_.BeginScene() ? D3D_OK : E_FAIL);
    if (FAILED(result))
    {
        (savedState->Restore() ? D3D_OK : E_FAIL);
        delete savedState;
        return result;
    }

    if (display_.postProcessAntiAliasingMode == 1 && fxaaShader_)
    {
        (graphics_.BindRenderTarget(backBuffer) ? D3D_OK : E_FAIL);
        result = DrawFullscreen(device_, fullscreenVertexShader_, fullscreenVertexDeclaration_,
                                fxaaShader_, sceneTexture_, nullptr,
                                (int)desc.width, (int)desc.height);
    }
    else if (smaaEdgeShader_ && smaaBlendShader_ && smaaNeighborhoodShader_)
    {
        graphics_.BindRenderTarget(edgeTexture_);
        result = DrawFullscreen(device_, fullscreenVertexShader_, fullscreenVertexDeclaration_,
                                smaaEdgeShader_, sceneTexture_, nullptr,
                                (int)desc.width, (int)desc.height);
        if (SUCCEEDED(result))
        {
            graphics_.BindRenderTarget(blendTexture_);
            result = DrawFullscreen(device_, fullscreenVertexShader_, fullscreenVertexDeclaration_,
                                    smaaBlendShader_, edgeTexture_, nullptr,
                                    (int)desc.width, (int)desc.height);
        }
        if (SUCCEEDED(result))
        {
            (graphics_.BindRenderTarget(backBuffer) ? D3D_OK : E_FAIL);
            result = DrawFullscreen(device_, fullscreenVertexShader_, fullscreenVertexDeclaration_,
                                    smaaNeighborhoodShader_, sceneTexture_, blendTexture_,
                                    (int)desc.width, (int)desc.height);
        }
    }
    const HRESULT endResult = (graphics_.EndScene() ? D3D_OK : E_FAIL);
    const HRESULT restoreResult = (savedState->Restore() ? D3D_OK : E_FAIL);
    delete savedState;
    // Render targets are not guaranteed to be part of a D3D9 state block.
    // ResolveFrame promises that its caller receives the swap-chain target.
    const HRESULT targetResult = (graphics_.BindRenderTarget(backBuffer) ? D3D_OK : E_FAIL);
    if (SUCCEEDED(result) && FAILED(endResult)) result = endResult;
    if (SUCCEEDED(result) && FAILED(restoreResult)) result = restoreResult;
    if (SUCCEEDED(result) && FAILED(targetResult)) result = targetResult;
    return result;
}

void Runtime::ReleasePostProcessResources()
{
    if (smaaNeighborhoodShader_) { delete smaaNeighborhoodShader_; smaaNeighborhoodShader_ = nullptr; }
    if (smaaBlendShader_) { delete smaaBlendShader_; smaaBlendShader_ = nullptr; }
    if (smaaEdgeShader_) { delete smaaEdgeShader_; smaaEdgeShader_ = nullptr; }
    if (fxaaShader_) { delete fxaaShader_; fxaaShader_ = nullptr; }
    if (fullscreenVertexDeclaration_) { delete fullscreenVertexDeclaration_; fullscreenVertexDeclaration_ = nullptr; }
    if (fullscreenVertexShader_) { delete fullscreenVertexShader_; fullscreenVertexShader_ = nullptr; }
    if (blendTexture_) { delete blendTexture_; blendTexture_ = nullptr; }
    if (edgeTexture_) { delete edgeTexture_; edgeTexture_ = nullptr; }
    if (sceneTexture_) { delete sceneTexture_; sceneTexture_ = nullptr; }
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
        RendererBackend::Viewport viewport = {};
        if (D3D9Graphics::Context(device).QueryViewport(viewport))
        {
            width = (int)viewport.width;
            height = (int)viewport.height;
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

void PrepareScreenSpaceUiRenderState(IDirect3DDevice9* device, const RendererBackend::BuiltinVertexLayout layout)
{
    // Screen-space overlays must not inherit world shader, depth/cull, or
    // texture-stage state from the preceding model render pass.
    D3D9Graphics::Context(device).BindShader(RendererBackend::ShaderStage::Vertex, nullptr);
    D3D9Graphics::Context(device).BindShader(RendererBackend::ShaderStage::Pixel, nullptr);
    D3D9Graphics::Context(device).BindBuiltinVertexLayout(layout);
    D3D9Graphics::Context(device).BindTexture(1, nullptr);
    D3D9Graphics::Context(device).SetLightingField(&RendererBackend::LightingState::enabled, false);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::fog, false);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthTest, false);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthWrite, false);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::cull, RendererBackend::Cull::None);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::alphaTest, false);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::coordinateIndex, uint32_t(0));
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::transform, RendererBackend::TextureTransform::Disabled);
    D3D9Graphics::Context(device).SetTextureStageField(1, &RendererBackend::TextureStageState::colorOperation, RendererBackend::TextureOperation::Disable);
    D3D9Graphics::Context(device).SetTextureStageField(1, &RendererBackend::TextureStageState::alphaOperation, RendererBackend::TextureOperation::Disable);
    D3D9Graphics::Context(device).SetSamplerField(0, &RendererBackend::SamplerState::addressU, RendererBackend::TextureAddress::Clamp);
    D3D9Graphics::Context(device).SetSamplerField(0, &RendererBackend::SamplerState::addressV, RendererBackend::TextureAddress::Clamp);
}

}
