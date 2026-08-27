#pragma once

#include <d3d9.h>
#include <vector>

struct ff11GeneratorRecord_t;
struct ff11KeyframeRecord_t;
struct noesisModel_t;

namespace ZoneWeatherParticles
{
void Draw(IDirect3DDevice9 *device, noesisModel_t *model, bool environmentValid,
          bool enableMipMapping, const char *weatherPath,
          const std::vector<ff11GeneratorRecord_t> &generators,
          const std::vector<ff11KeyframeRecord_t> &keyframes,
          float cameraX, float cameraY, float cameraZ);
}
