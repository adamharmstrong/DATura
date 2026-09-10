#pragma once

#include <d3d9.h>

namespace ZoneRenderFrustum
{
struct Data
{
    float planes[6][4] = {};
    bool valid = false;
};

void Build(Data &frustum, const D3DMATRIX &view, const D3DMATRIX &projection);
bool IntersectsBounds(const Data &frustum, const float boundsMin[3], const float boundsMax[3],
                      bool hasBounds);
}
