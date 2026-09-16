#pragma once

#include "application_settings.h"
#include "d3d9_device.h"

namespace RendererBackend
{
    using DisplayConfiguration = D3D9Device::DisplayConfiguration;
    using DefaultPoolCallback = D3D9Device::DefaultPoolCallback;
    using FrameStatus = D3D9Device::FrameStatus;

    enum class InitializeResult
    {
        Success,
        UnsupportedBackend,
        Direct3DUnavailable,
        NoSupportedDevice,
    };

    // Owns the selected graphics backend. The current renderer still borrows
    // the D3D9 device directly while backend-neutral draw interfaces are grown.
    class Runtime final
    {
    public:
        Runtime() = default;
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
        HRESULT Present();
        void Shutdown();

        int Backend() const { return backend_; }
        IDirect3DDevice9* D3D9Device() const { return d3d9_.Device(); }
        bool IsInitialized() const { return d3d9_.IsInitialized(); }
        bool IsDeviceLost() const { return d3d9_.IsDeviceLost(); }

    private:
        int backend_ = ApplicationSettings::RenderingBackendDirectX9;
        D3D9Device::Runtime d3d9_;
    };
}
