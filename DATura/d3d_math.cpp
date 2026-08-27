#include "stdafx.h"
#include "d3d_math.h"

#include <cmath>

namespace D3DMath
{
D3DMATRIX BuildIdentity()
{
    D3DMATRIX matrix = {};
    matrix._11 = matrix._22 = matrix._33 = matrix._44 = 1.0f;
    return matrix;
}

D3DMATRIX BuildLookAtLH(const float eyeX, const float eyeY, const float eyeZ,
                         const float targetX, const float targetY, const float targetZ)
{
    float zX = targetX - eyeX;
    float zY = targetY - eyeY;
    float zZ = targetZ - eyeZ;
    const float zLength = std::sqrt(zX * zX + zY * zY + zZ * zZ);
    if (zLength > 1e-8f)
    {
        zX /= zLength;
        zY /= zLength;
        zZ /= zLength;
    }

    float xX = -zZ;
    float xY = 0.0f;
    float xZ = zX;
    const float xLength = std::sqrt(xX * xX + xY * xY + xZ * xZ);
    if (xLength > 1e-8f)
    {
        xX /= xLength;
        xY /= xLength;
        xZ /= xLength;
    }
    else
    {
        xX = 1.0f;
        xY = 0.0f;
        xZ = 0.0f;
    }

    const float yX = zY * xZ - zZ * xY;
    const float yY = zZ * xX - zX * xZ;
    const float yZ = zX * xY - zY * xX;

    D3DMATRIX matrix = {};
    matrix._11 = xX; matrix._12 = yX; matrix._13 = zX; matrix._14 = 0.0f;
    matrix._21 = xY; matrix._22 = yY; matrix._23 = zY; matrix._24 = 0.0f;
    matrix._31 = xZ; matrix._32 = yZ; matrix._33 = zZ; matrix._34 = 0.0f;
    matrix._41 = -(xX * eyeX + xY * eyeY + xZ * eyeZ);
    matrix._42 = -(yX * eyeX + yY * eyeY + yZ * eyeZ);
    matrix._43 = -(zX * eyeX + zY * eyeY + zZ * eyeZ);
    matrix._44 = 1.0f;
    return matrix;
}

D3DMATRIX BuildPerspectiveFovLH(const float fieldOfViewY, const float aspect,
                                 const float nearPlane, const float farPlane)
{
    const float height = 1.0f / std::tan(fieldOfViewY * 0.5f);
    const float width = height / aspect;
    D3DMATRIX matrix = {};
    matrix._11 = width;
    matrix._22 = height;
    matrix._33 = farPlane / (farPlane - nearPlane);
    matrix._34 = 1.0f;
    matrix._43 = -nearPlane * farPlane / (farPlane - nearPlane);
    return matrix;
}

D3DMATRIX Multiply(const D3DMATRIX& a, const D3DMATRIX& b)
{
    D3DMATRIX out = {};
    const float* aValues = &a._11;
    const float* bValues = &b._11;
    float* outValues = &out._11;
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            outValues[row * 4 + column] =
                aValues[row * 4 + 0] * bValues[0 * 4 + column] +
                aValues[row * 4 + 1] * bValues[1 * 4 + column] +
                aValues[row * 4 + 2] * bValues[2 * 4 + column] +
                aValues[row * 4 + 3] * bValues[3 * 4 + column];
        }
    }
    return out;
}

D3DMATRIX BuildRotationX(const float radians)
{
    D3DMATRIX matrix = BuildIdentity();
    const float sine = std::sin(radians);
    const float cosine = std::cos(radians);
    matrix._22 = cosine;
    matrix._23 = sine;
    matrix._32 = -sine;
    matrix._33 = cosine;
    return matrix;
}

D3DMATRIX BuildRotationY(const float radians)
{
    D3DMATRIX matrix = BuildIdentity();
    const float sine = std::sin(radians);
    const float cosine = std::cos(radians);
    matrix._11 = cosine;
    matrix._13 = -sine;
    matrix._31 = sine;
    matrix._33 = cosine;
    return matrix;
}

D3DMATRIX BuildRotationZ(const float radians)
{
    D3DMATRIX matrix = BuildIdentity();
    const float sine = std::sin(radians);
    const float cosine = std::cos(radians);
    matrix._11 = cosine;
    matrix._12 = sine;
    matrix._21 = -sine;
    matrix._22 = cosine;
    return matrix;
}

D3DMATRIX BuildYawTranslation(const float yawRadians, const float position[3])
{
    D3DMATRIX matrix = BuildRotationY(yawRadians);
    matrix._41 = position ? position[0] : 0.0f;
    matrix._42 = position ? position[1] : 0.0f;
    matrix._43 = position ? position[2] : 0.0f;
    return matrix;
}

D3DMATRIX BuildScaleRotateTranslate(const float scale[3], const float rotation[3],
                                     const float translation[3])
{
    D3DMATRIX scaleMatrix = BuildIdentity();
    scaleMatrix._11 = scale ? scale[0] : 1.0f;
    scaleMatrix._22 = scale ? scale[1] : 1.0f;
    scaleMatrix._33 = scale ? scale[2] : 1.0f;

    D3DMATRIX world = Multiply(scaleMatrix, BuildRotationX(rotation ? rotation[0] : 0.0f));
    world = Multiply(world, BuildRotationY(rotation ? rotation[1] : 0.0f));
    world = Multiply(world, BuildRotationZ(rotation ? rotation[2] : 0.0f));
    world._41 = translation ? translation[0] : 0.0f;
    world._42 = translation ? translation[1] : 0.0f;
    world._43 = translation ? translation[2] : 0.0f;
    return world;
}
}
