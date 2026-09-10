#pragma once

#include <d3d9.h>

struct noesisTex_t;

namespace D3DUiRenderer
{
    bool UsesFfxiDxt3Alpha(const noesisTex_t* texture);

    void DrawTexturedQuadUV(IDirect3DDevice9* device, bool enableMipMapping,
                            IDirect3DTexture9* texture,
                            float x, float y, float width, float height,
                            float u0, float v0, float u1, float v1, DWORD color,
                            bool expandDxt3Alpha, float alphaScale = 1.0f,
                            float maxOpacity = 1.0f);
    void DrawTexturedQuad(IDirect3DDevice9* device, bool enableMipMapping,
                          noesisTex_t* texture,
                          float x, float y, float width, float height, DWORD color);
    void DrawSolidQuad(IDirect3DDevice9* device,
                       float x, float y, float width, float height, DWORD color);
    void DrawTextureRegion(IDirect3DDevice9* device, bool enableMipMapping,
                           noesisTex_t* texture,
                           float x, float y, float width, float height,
                           float sourceX, float sourceY, float sourceWidth, float sourceHeight,
                           DWORD color);
}
