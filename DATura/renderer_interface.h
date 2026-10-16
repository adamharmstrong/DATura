#pragma once

#include <windows.h>
#include "renderer_resources.h"

struct IDirect3DDevice9;

namespace RendererBackend
{
enum class InitializeResult { Success, UnsupportedBackend, Direct3DUnavailable, NoSupportedDevice };
enum class FrameStatus { Ready, Uninitialized, DeviceLost, ResetFailed };

struct DisplayConfiguration
{
    bool borderless = false;
    bool fullscreen = false;
    int width = 1280;
    int height = 720;
    int renderWidth = 1280;
    int renderHeight = 720;
    int antiAliasingSamples = 0;
    int postProcessAntiAliasingMode = 0;
};

using DefaultPoolCallback = void (*)();

// Windows UI bridge. Destroy the lease before rendering, presenting, or resetting.
// The backend owns the presentation surface and releases its DC with the lease.
class OverlayContext
{
public:
    virtual ~OverlayContext() = default;
    virtual HDC DeviceContext() const = 0;
    virtual unsigned Width() const = 0;
    virtual unsigned Height() const = 0;
};

// Device lifetime, shared graphics services, and presentation.
class Interface
{
public:
    virtual ~Interface() = default;
    virtual void SetDefaultPoolCallbacks(DefaultPoolCallback releaseResources,
                                         DefaultPoolCallback recreateResources) = 0;
    virtual InitializeResult Initialize(HWND window, const DisplayConfiguration& display) = 0;
    virtual bool ApplyDisplayConfiguration(const DisplayConfiguration& display) = 0;
    virtual bool Resize(int clientWidth, int clientHeight) = 0;
    virtual FrameStatus PrepareFrame() = 0;
    virtual HRESULT ResolveFrame() = 0;
    virtual HRESULT Present() = 0;
    virtual OverlayContext* AcquireOverlay() = 0;
    virtual void Shutdown() = 0;
    virtual bool IsInitialized() const = 0;
    virtual bool IsDeviceLost() const = 0;
    virtual Graphics& GetGraphics() = 0;

    // Temporary typed bridge for unported rendering code. Borrowed, never released
    // by the caller; valid only while the backend is initialized.
    virtual IDirect3DDevice9* LegacyD3D9Device() const { return nullptr; }
};
}
