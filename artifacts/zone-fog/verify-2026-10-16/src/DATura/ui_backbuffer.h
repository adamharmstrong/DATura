#pragma once
#include <windows.h>
#include <d3d9.h>
#include "d3d9_graphics.h"
#include <memory>

namespace UiBackbuffer
{
inline bool active = false;
// Once compositing succeeds, native WM_PAINT must validate without drawing.
inline bool ConsumeNativePaint(HWND window, UINT message)
{
    if (!active || message != WM_PAINT) return false;
    PAINTSTRUCT paint{}; BeginPaint(window, &paint); EndPaint(window, &paint);
    return true;
}

inline RECT MapRect(const RECT& rect, int clientWidth, int clientHeight, int surfaceWidth, int surfaceHeight)
{
    if (clientWidth <= 0 || clientHeight <= 0) return {};
    return {MulDiv(rect.left, surfaceWidth, clientWidth), MulDiv(rect.top, surfaceHeight, clientHeight),
        MulDiv(rect.right, surfaceWidth, clientWidth), MulDiv(rect.bottom, surfaceHeight, clientHeight)};
}
// Child painters may change viewport origins or clipping. The final blit must
// always read the composed bitmap from its original (0,0) coordinate system.
template<class Painter>
bool PaintLocal(HDC dc, Painter paint)
{
    const int saved = SaveDC(dc);
    if (!saved) return false;
    paint(dc);
    RestoreDC(dc, saved);
    return true;
}
// Paint in child-window coordinates, then scale once to the actual presentation
// surface. This keeps nested GDI viewport changes independent of render resolution.
template<class Painter>
void Draw(IDirect3DDevice9* device, HWND window, Painter paint)
{
    if (!device || !window || !IsWindowVisible(window)) return;
    HWND owner = GetParent(window);
    RECT client{}, rect{};
    GetClientRect(owner, &client);
    GetWindowRect(window, &rect);
    MapWindowPoints(HWND_DESKTOP, owner, reinterpret_cast<POINT*>(&rect), 2);
    const int width = rect.right - rect.left, height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0 || client.right <= 0 || client.bottom <= 0) return;
    std::unique_ptr<RendererBackend::OverlayContext> overlay(D3D9Graphics::AcquireOverlay(device));
    if (overlay)
    {
        HDC target = overlay->DeviceContext();
        HDC memory = CreateCompatibleDC(target);
        HBITMAP bitmap = CreateCompatibleBitmap(target, width, height);
        if (memory && bitmap)
        {
            HGDIOBJ previous = SelectObject(memory, bitmap);
            const bool ready = PaintLocal(memory, paint);
            const RECT destination = MapRect(rect, client.right, client.bottom,
                static_cast<int>(overlay->Width()), static_cast<int>(overlay->Height()));
            const int saved = SaveDC(target);
            if (saved)
            {
                SetStretchBltMode(target, COLORONCOLOR);
                const BOOL painted = ready && StretchBlt(target, destination.left, destination.top,
                    destination.right - destination.left, destination.bottom - destination.top,
                    memory, 0, 0, width, height, SRCCOPY);
                if (painted) active = true;
                RestoreDC(target, saved);
            }
            SelectObject(memory, previous);
        }
        if (bitmap) DeleteObject(bitmap);
        if (memory) DeleteDC(memory);
    }
}
}
