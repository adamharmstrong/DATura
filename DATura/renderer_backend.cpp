#include "stdafx.h"
#include "renderer_backend.h"
#include "d3d9_device.h"

namespace RendererBackend
{
Runtime::Runtime() : implementation_(std::make_unique<D3D9Device::Runtime>())
{
}

Runtime::~Runtime()
{
    Shutdown();
}

void Runtime::SetDefaultPoolCallbacks(const DefaultPoolCallback releaseResources,
                                      const DefaultPoolCallback recreateResources)
{
    implementation_->SetDefaultPoolCallbacks(releaseResources, recreateResources);
}

InitializeResult Runtime::Initialize(const HWND window, const int backend,
                                     const DisplayConfiguration& display)
{
    Shutdown();
    backend_ = ApplicationSettings::ClampRenderingBackend(backend);
    if (!ApplicationSettings::RenderingBackendIsAvailable(backend_))
        return InitializeResult::UnsupportedBackend;

    return implementation_->Initialize(window, display);
}

bool Runtime::ApplyDisplayConfiguration(const DisplayConfiguration& display)
{
    return implementation_->ApplyDisplayConfiguration(display);
}

bool Runtime::Resize(const int clientWidth, const int clientHeight)
{
    return implementation_->Resize(clientWidth, clientHeight);
}

FrameStatus Runtime::PrepareFrame()
{
    return implementation_->PrepareFrame();
}

HRESULT Runtime::ResolveFrame()
{
    return implementation_->ResolveFrame();
}

HRESULT Runtime::Present()
{
    return implementation_->Present();
}

void Runtime::Shutdown()
{
    implementation_->Shutdown();
}
}
