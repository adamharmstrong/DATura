#pragma once

#include <d3d9.h>

namespace D3D9Device
{
    enum class InitializeResult
    {
        Success,
        Direct3DUnavailable,
        NoSupportedDevice,
    };

    enum class FrameStatus
    {
        Ready,
        Uninitialized,
        DeviceLost,
        ResetFailed,
    };

    struct DisplayConfiguration
    {
        bool borderless = false;
        bool fullscreen = false;
        int width = 1280;
        int height = 720;
    };

    using DefaultPoolCallback = void (*)();

    // Owns the D3D9 interfaces and all presentation/reset state. Rendering
    // code may borrow Device(), but it must not retain or release that pointer.
    class Runtime final
    {
    public:
        Runtime() = default;
        ~Runtime();

        Runtime(const Runtime&) = delete;
        Runtime& operator=(const Runtime&) = delete;

        void SetDefaultPoolCallbacks(DefaultPoolCallback releaseResources,
                                     DefaultPoolCallback recreateResources);
        InitializeResult Initialize(HWND window, const DisplayConfiguration& display);
        bool ApplyDisplayConfiguration(const DisplayConfiguration& display);
        bool Resize(int clientWidth, int clientHeight);
        FrameStatus PrepareFrame();
        HRESULT Present();
        void Shutdown();

        IDirect3DDevice9* Device() const { return device_; }
        bool IsInitialized() const { return device_ != nullptr; }
        bool IsDeviceLost() const { return deviceLost_; }

    private:
        bool ResetWithBackBufferSize(int width, int height);
        bool ResetCurrentParameters();
        void ReleaseDefaultPoolResources();
        void RecreateDefaultPoolResources();

        HWND window_ = nullptr;
        IDirect3D9* d3d_ = nullptr;
        IDirect3DDevice9* device_ = nullptr;
        D3DPRESENT_PARAMETERS parameters_ = {};
        DisplayConfiguration display_ = {};
        DefaultPoolCallback releaseResources_ = nullptr;
        DefaultPoolCallback recreateResources_ = nullptr;
        bool deviceLost_ = false;
        bool applyingDisplayConfiguration_ = false;
    };

    // Returns the active D3D viewport size, falling back to the window client
    // area while the device is unavailable or has no usable viewport.
    void GetViewportOrClientSize(IDirect3DDevice9* device, HWND window,
                                 int* outWidth, int* outHeight);
    POINT MapClientPointToViewport(HWND window, int viewportWidth,
                                   int viewportHeight, POINT point);
    int BeginGdiViewportMapping(HDC hdc, HWND window, int viewportWidth,
                                int viewportHeight);
    void PrepareScreenSpaceUiRenderState(IDirect3DDevice9* device, DWORD vertexFvf);

}
