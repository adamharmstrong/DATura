#pragma once

#include "application_settings.h"
#include "renderer_interface.h"
#include <memory>

namespace RendererBackend
{
    // Owns the selected graphics backend and delegates presentation/lifecycle.
    class Runtime final
    {
    public:
        Runtime();
        ~Runtime();

        Runtime(const Runtime&) = delete;
        Runtime& operator=(const Runtime&) = delete;

        void SetDefaultPoolCallbacks(DefaultPoolCallback releaseResources,
                                     DefaultPoolCallback recreateResources);
        InitializeResult Initialize(HWND window, int backend,
                                    const DisplayConfiguration& display);
        bool ApplyDisplayConfiguration(const DisplayConfiguration& display);
        bool Resize(int clientWidth, int clientHeight);
        FrameStatus PrepareFrame();
        HRESULT ResolveFrame();
        HRESULT Present();
        OverlayContext* AcquireOverlay() { return implementation_->AcquireOverlay(); }
        void Shutdown();

        int Backend() const { return backend_; }
        Graphics& GetGraphics() { return implementation_->GetGraphics(); }
        IDirect3DDevice9* D3D9Device() const { return implementation_->LegacyD3D9Device(); }
        bool IsInitialized() const { return implementation_->IsInitialized(); }
        bool IsDeviceLost() const { return implementation_->IsDeviceLost(); }

    private:
        int backend_ = ApplicationSettings::RenderingBackendDirectX9;
        std::unique_ptr<Interface> implementation_;
    };
}
