#pragma once

#include <d3d9.h>

namespace ZoneSkyDome
{
struct Parameters
{
    bool environmentValid = false;
    bool indoor = false;
    float radius = 0.0f;
    int spokeCount = 0;
    int ringCount = 0;
    const float *ringElevations = nullptr;
    const DWORD *ringColors = nullptr;
};

void Draw(IDirect3DDevice9 *device, const Parameters &parameters,
          float cameraX, float cameraY, float cameraZ, float farPlane);
}
