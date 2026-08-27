#include "stdafx.h"
#include "d3d9_device.h"

#include <cstdio>

namespace
{
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
    BuildPresentParameters(display.width, display.height, !display.fullscreen, parameters_);

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
    return ResetWithBackBufferSize(display.width, display.height);
}

bool Runtime::Resize(const int clientWidth, const int clientHeight)
{
    if (!device_)
        return false;
    if (applyingDisplayConfiguration_)
        return true;

    const bool fixedBackBuffer = display_.borderless || display_.fullscreen;
    return ResetWithBackBufferSize(
        fixedBackBuffer ? display_.width : clientWidth,
        fixedBackBuffer ? display_.height : clientHeight);
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
    return FrameStatus::Ready;
}

HRESULT Runtime::Present()
{
    if (!device_)
        return D3DERR_INVALIDCALL;

    const HRESULT result = device_->Present(nullptr, nullptr, nullptr, nullptr);
    if (result == D3DERR_DEVICELOST)
        deviceLost_ = true;
    return result;
}

void Runtime::Shutdown()
{
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

    ReleaseDefaultPoolResources();
    const HRESULT result = device_->Reset(&parameters_);
    if (FAILED(result))
        return false;

    RecreateDefaultPoolResources();
    deviceLost_ = false;
    return true;
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
