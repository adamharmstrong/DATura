#pragma once

#include <d3d9.h>

namespace D3DBumpMapping
{
bool ApplyTextureShader(IDirect3DDevice9* device, bool useAuthoredAlpha,
                        bool expandDxt3Alpha, float opacityScale,
                        const float* colorScale, bool hasTexture,
                        bool enableBumpMapping, float bumpIntensity,
                        float baseTextureWidth, float baseTextureHeight);
void Release();
}
