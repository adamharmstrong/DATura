#pragma once

#include <d3d9.h>

namespace D3DMath
{
    // Zone signs contain closely spaced front/back panels. A 0.01 near
    // plane loses too much depth precision at ordinary viewing distances.
    inline constexpr float ZoneNearPlane = 0.1f;
    D3DMATRIX BuildIdentity();
    D3DMATRIX BuildLookAtLH(float eyeX, float eyeY, float eyeZ,
                            float targetX, float targetY, float targetZ);
    D3DMATRIX BuildPerspectiveFovLH(float fieldOfViewY, float aspect,
                                    float nearPlane, float farPlane);
    D3DMATRIX Multiply(const D3DMATRIX& a, const D3DMATRIX& b);
    D3DMATRIX BuildRotationX(float radians);
    D3DMATRIX BuildRotationY(float radians);
    D3DMATRIX BuildRotationZ(float radians);
    D3DMATRIX BuildYawTranslation(float yawRadians, const float position[3]);
    D3DMATRIX BuildScaleRotateTranslate(const float scale[3], const float rotation[3],
                                        const float translation[3]);
}
