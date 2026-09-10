#pragma once

#include <d3d9.h>
#include <string>
#include <vector>

struct ff11GeneratorRecord_t;
struct ff11KeyframeRecord_t;
struct noesisModel_t;

namespace ZoneEnvironmentRenderState
{
struct Data
{
    const ff11GeneratorRecord_t *generator = nullptr;
    float opacity = 1.0f;
    float colorScale[3] = { 1.0f, 1.0f, 1.0f };
    float uvScrollU = 0.0f;
    float uvScrollV = 0.0f;
    float rotation[3] = {};
    float rotationPivot[3] = {};
    unsigned short blendMode = 0x48;
};

Data Build(const std::string &objectName, int vanadielMinute,
           const std::vector<ff11GeneratorRecord_t> &generators,
           const std::vector<ff11KeyframeRecord_t> &keyframes);
bool IsCameraShell(const Data &state);
D3DMATRIX BuildCameraShellWorld(const Data &state, float cameraX, float cameraY, float cameraZ);
void ApplyBlendMode(IDirect3DDevice9 *device, unsigned short blendMode);
void DrawCameraShells(IDirect3DDevice9 *device, noesisModel_t *model, bool enableMipMapping,
                      const char *weatherPath,
                      const std::vector<ff11GeneratorRecord_t> &generators,
                      const std::vector<ff11KeyframeRecord_t> &keyframes,
                      float cameraX, float cameraY, float cameraZ);
}
