#pragma once

#include <d3d9.h>
#include "renderer_interface.h"
#include "d3d9_graphics.h"

namespace D3D9Device
{
    using InitializeResult = RendererBackend::InitializeResult;
    using FrameStatus = RendererBackend::FrameStatus;
    using DisplayConfiguration = RendererBackend::DisplayConfiguration;
    using DefaultPoolCallback = RendererBackend::DefaultPoolCallback;

    // Owns the D3D9 interfaces and all presentation/reset state. Rendering
    // code may borrow Device(), but it must not retain or release that pointer.
    class Runtime final : public RendererBackend::Interface
    {
    public:
        Runtime() = default;
        ~Runtime() override;

        Runtime(const Runtime&) = delete;
        Runtime& operator=(const Runtime&) = delete;

        void SetDefaultPoolCallbacks(DefaultPoolCallback releaseResources,
                                     DefaultPoolCallback recreateResources) override;
        InitializeResult Initialize(HWND window, const DisplayConfiguration& display) override;
        bool ApplyDisplayConfiguration(const DisplayConfiguration& display) override;
        bool Resize(int clientWidth, int clientHeight) override;
        FrameStatus PrepareFrame() override;
        HRESULT ResolveFrame() override;
        HRESULT Present() override;
        RendererBackend::OverlayContext* AcquireOverlay() override
        { return D3D9Graphics::AcquireOverlay(device_); }
        void Shutdown() override;

        IDirect3DDevice9* Device() const { return device_; }
        IDirect3DDevice9* LegacyD3D9Device() const override { return device_; }
        bool IsInitialized() const override { return device_ != nullptr; }
        bool IsDeviceLost() const override { return deviceLost_; }
        RendererBackend::Graphics& GetGraphics() override { return graphics_; }

    private:
        bool ResetWithBackBufferSize(int width, int height);
        bool ResetCurrentParameters();
        void ReleaseDefaultPoolResources();
        void RecreateDefaultPoolResources();
        bool CreateMultisampleTargets();
        void ReleaseMultisampleTargets();
        bool CreatePostProcessResources();
        void ReleasePostProcessResources();
        HRESULT ApplyPostProcess(RendererBackend::RenderTarget* backBuffer);

        HWND window_ = nullptr;
        D3D9Graphics::Context graphics_;
        IDirect3D9* d3d_ = nullptr;
        IDirect3DDevice9* device_ = nullptr;
        D3DPRESENT_PARAMETERS parameters_ = {};
        DisplayConfiguration display_ = {};
        DefaultPoolCallback releaseResources_ = nullptr;
        DefaultPoolCallback recreateResources_ = nullptr;
        bool deviceLost_ = false;
        bool applyingDisplayConfiguration_ = false;
        RendererBackend::RenderTarget* multisampleColor_ = nullptr;
        RendererBackend::RenderTarget* multisampleDepth_ = nullptr;
        RendererBackend::RenderTarget* sceneTexture_ = nullptr;
        RendererBackend::RenderTarget* edgeTexture_ = nullptr;
        RendererBackend::RenderTarget* blendTexture_ = nullptr;
        RendererBackend::Shader* fullscreenVertexShader_ = nullptr;
        RendererBackend::VertexLayout* fullscreenVertexDeclaration_ = nullptr;
        RendererBackend::Shader* fxaaShader_ = nullptr;
        RendererBackend::Shader* smaaEdgeShader_ = nullptr;
        RendererBackend::Shader* smaaBlendShader_ = nullptr;
        RendererBackend::Shader* smaaNeighborhoodShader_ = nullptr;
        bool frameResolved_ = true;
    };

    // Returns the active D3D viewport size, falling back to the window client
    // area while the device is unavailable or has no usable viewport.
    void GetViewportOrClientSize(IDirect3DDevice9* device, HWND window,
                                 int* outWidth, int* outHeight);
    POINT MapClientPointToViewport(HWND window, int viewportWidth,
                                   int viewportHeight, POINT point);
    int BeginGdiViewportMapping(HDC hdc, HWND window, int viewportWidth,
                                int viewportHeight);
    void PrepareScreenSpaceUiRenderState(IDirect3DDevice9* device, RendererBackend::BuiltinVertexLayout layout);

}
