#include "stdafx.h"
#include "zone_render_frustum.h"

#include "d3d_math.h"

#include <cmath>

namespace ZoneRenderFrustum
{
void Build(Data &frustum, const D3DMATRIX &view, const D3DMATRIX &projection)
{
    const D3DMATRIX clip = D3DMath::Multiply(view, projection);
    const float rawPlanes[6][4] =
    {
        { clip._14 + clip._11, clip._24 + clip._21, clip._34 + clip._31, clip._44 + clip._41 }, // left
        { clip._14 - clip._11, clip._24 - clip._21, clip._34 - clip._31, clip._44 - clip._41 }, // right
        { clip._14 + clip._12, clip._24 + clip._22, clip._34 + clip._32, clip._44 + clip._42 }, // bottom
        { clip._14 - clip._12, clip._24 - clip._22, clip._34 - clip._32, clip._44 - clip._42 }, // top
        { clip._13,            clip._23,            clip._33,            clip._43 },            // near
        { clip._14 - clip._13, clip._24 - clip._23, clip._34 - clip._33, clip._44 - clip._43 }, // far
    };

    frustum.valid = true;
    for (int plane = 0; plane < 6; ++plane)
    {
        const float length = sqrtf(rawPlanes[plane][0] * rawPlanes[plane][0] +
                                   rawPlanes[plane][1] * rawPlanes[plane][1] +
                                   rawPlanes[plane][2] * rawPlanes[plane][2]);
        if (!(length > 0.000001f) || !std::isfinite(length))
        {
            frustum.valid = false;
            break;
        }
        for (int component = 0; component < 4; ++component)
            frustum.planes[plane][component] = rawPlanes[plane][component] / length;
    }
}

bool IntersectsBounds(const Data &frustum, const float boundsMin[3], const float boundsMax[3],
                      const bool hasBounds)
{
    if (!frustum.valid || !hasBounds)
        return true;

    for (int plane = 0; plane < 6; ++plane)
    {
        const float *p = frustum.planes[plane];
        const float x = p[0] >= 0.0f ? boundsMax[0] : boundsMin[0];
        const float y = p[1] >= 0.0f ? boundsMax[1] : boundsMin[1];
        const float z = p[2] >= 0.0f ? boundsMax[2] : boundsMin[2];
        if (p[0] * x + p[1] * y + p[2] * z + p[3] < 0.0f)
            return false;
    }
    return true;
}
}
