#include "stdafx.h"
#include "renderer_backend.h"

namespace RendererBackend
{
Runtime::~Runtime()
{
    Shutdown();
}

void Runtime::SetDefaultPoolCallbacks(const DefaultPoolCallback releaseResources,
                                      const DefaultPoolCallback recreateResources)
{
    d3d9_.SetDefaultPoolCallbacks(releaseResources, recreateResources);
}

InitializeResult Runtime::Initialize(const HWND window, const int backend,
                                     const DisplayConfiguration& display)
{
    Shutdown();
    backend_ = ApplicationSettings::ClampRenderingBackend(backend);
    if (!ApplicationSettings::RenderingBackendIsAvailable(backend_))
        return InitializeResult::UnsupportedBackend;

    const D3D9Device::InitializeResult result = d3d9_.Initialize(window, display);
    switch (result)
    {
    case D3D9Device::InitializeResult::Success:
        return InitializeResult::Success;
    case D3D9Device::InitializeResult::Direct3DUnavailable:
        return InitializeResult::Direct3DUnavailable;
    case D3D9Device::InitializeResult::NoSupportedDevice:
    default:
        return InitializeResult::NoSupportedDevice;
    }
}

bool Runtime::ApplyDisplayConfiguration(const DisplayConfiguration& display)
{
    return d3d9_.ApplyDisplayConfiguration(display);
}

bool Runtime::Resize(const int clientWidth, const int clientHeight)
{
    return d3d9_.Resize(clientWidth, clientHeight);
}

FrameStatus Runtime::PrepareFrame()
{
    return d3d9_.PrepareFrame();
}

HRESULT Runtime::Present()
{
    return d3d9_.Present();
}

void Runtime::Shutdown()
{
    d3d9_.Shutdown();
}
}
