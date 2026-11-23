#include "stdafx.h"
#include "zone_object_picker.h"

#include "d3d_math.h"
#include "noesis_rapi.h"
#include "zone_object_transform.h"

#include <cfloat>

namespace ZoneObjectPicker
{
namespace
{
void TransformPoint(const float* point, const D3DMATRIX& matrix, float* result)
{
    for (int axis = 0; axis < 3; ++axis)
    {
        result[axis] = point[0] * matrix.m[0][axis] +
            point[1] * matrix.m[1][axis] +
            point[2] * matrix.m[2][axis] + matrix.m[3][axis];
    }
}

bool RayIntersectsBounds(const float* origin, const float* direction,
                         const float* minimum, const float* maximum,
                         const float distanceLimit)
{
    float nearDistance = 0.0f;
    float farDistance = distanceLimit;
    for (int axis = 0; axis < 3; ++axis)
    {
        if (std::fabs(direction[axis]) < 1.0e-8f)
        {
            if (origin[axis] < minimum[axis] || origin[axis] > maximum[axis])
                return false;
            continue;
        }

        float nearAxis = (minimum[axis] - origin[axis]) / direction[axis];
        float farAxis = (maximum[axis] - origin[axis]) / direction[axis];
        if (nearAxis > farAxis)
            std::swap(nearAxis, farAxis);
        nearDistance = (std::max)(nearDistance, nearAxis);
        farDistance = (std::min)(farDistance, farAxis);
        if (nearDistance > farDistance)
            return false;
    }
    return true;
}

bool RayIntersectsTriangle(const float* origin, const float* direction,
                           const float points[3][3], float& distance)
{
    float edge1[3], edge2[3], crossDirection[3], fromPoint[3], crossPoint[3];
    for (int axis = 0; axis < 3; ++axis)
    {
        edge1[axis] = points[1][axis] - points[0][axis];
        edge2[axis] = points[2][axis] - points[0][axis];
        fromPoint[axis] = origin[axis] - points[0][axis];
    }
    const auto cross = [](const float* left, const float* right, float* result)
    {
        result[0] = left[1] * right[2] - left[2] * right[1];
        result[1] = left[2] * right[0] - left[0] * right[2];
        result[2] = left[0] * right[1] - left[1] * right[0];
    };
    const auto dot = [](const float* left, const float* right)
    {
        return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
    };

    cross(direction, edge2, crossDirection);
    const float determinant = dot(edge1, crossDirection);
    if (std::fabs(determinant) < 1.0e-7f)
        return false;
    const float u = dot(fromPoint, crossDirection) / determinant;
    if (u < 0.0f || u > 1.0f)
        return false;
    cross(fromPoint, edge1, crossPoint);
    const float v = dot(direction, crossPoint) / determinant;
    if (v < 0.0f || u + v > 1.0f)
        return false;
    distance = dot(edge2, crossPoint) / determinant;
    return distance > 0.0f;
}

D3DMATRIX ObjectWorldMatrix(
    const std::string& objectName,
    const std::map<std::string, ZoneObjectTransform::DebugTransform>* overrides)
{
    if (overrides)
    {
        const auto found = overrides->find(objectName);
        if (found != overrides->end())
            return ZoneObjectTransform::BuildWorldMatrix(found->second);
    }
    return D3DMath::BuildIdentity();
}
}

Hit Pick(const noesisModel_t* model,
         const ZoneObjectVisibility::RenderContext& visibility,
         const D3DMATRIX& view, const D3DMATRIX& projection,
         const int width, const int height, const float x, const float y)
{
    Hit hit;
    if (!model || width <= 0 || height <= 0 ||
        x < 0.0f || y < 0.0f || x >= width || y >= height)
    {
        return hit;
    }

    const float camera[3] =
    {
        -(view._41 * view._11 + view._42 * view._12 + view._43 * view._13),
        -(view._41 * view._21 + view._42 * view._22 + view._43 * view._23),
        -(view._41 * view._31 + view._42 * view._32 + view._43 * view._33)
    };
    const float viewX = (2.0f * x / width - 1.0f) / projection._11;
    const float viewY = (1.0f - 2.0f * y / height) / projection._22;
    const float direction[3] =
    {
        viewX * view._11 + viewY * view._12 + view._13,
        viewX * view._21 + viewY * view._22 + view._23,
        viewX * view._31 + viewY * view._32 + view._33
    };

    float nearest = FLT_MAX;
    for (const noesisModel_t::Submesh& submesh : model->submeshes)
    {
        if (submesh.objectName.empty() ||
            !ZoneObjectVisibility::PassesRenderVisibility(model, submesh, true, visibility))
        {
            continue;
        }

        const D3DMATRIX world = ObjectWorldMatrix(submesh.objectName, visibility.overrides);
        if (submesh.hasBounds)
        {
            float minimum[3] = { FLT_MAX, FLT_MAX, FLT_MAX };
            float maximum[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
            for (int corner = 0; corner < 8; ++corner)
            {
                float local[3], transformed[3];
                for (int axis = 0; axis < 3; ++axis)
                {
                    local[axis] = (corner & (1 << axis))
                        ? submesh.boundsMax[axis] : submesh.boundsMin[axis];
                }
                TransformPoint(local, world, transformed);
                for (int axis = 0; axis < 3; ++axis)
                {
                    minimum[axis] = (std::min)(minimum[axis], transformed[axis]);
                    maximum[axis] = (std::max)(maximum[axis], transformed[axis]);
                }
            }
            if (!RayIntersectsBounds(camera, direction, minimum, maximum, nearest))
                continue;
        }

        for (size_t index = 0; index + 2 < submesh.cpuIndices.size(); index += 3)
        {
            float triangle[3][3];
            bool validTriangle = true;
            for (int vertex = 0; vertex < 3; ++vertex)
            {
                const DWORD vertexIndex = submesh.cpuIndices[index + vertex];
                if (vertexIndex >= submesh.cpuVerts.size())
                {
                    validTriangle = false;
                    break;
                }
                TransformPoint(submesh.cpuVerts[vertexIndex].pos, world, triangle[vertex]);
            }
            if (!validTriangle)
                continue;

            float distance = 0.0f;
            if (!RayIntersectsTriangle(camera, direction, triangle, distance) ||
                distance >= nearest)
            {
                continue;
            }
            const float depth = projection._33 + projection._43 / distance;
            if (depth < 0.0f || depth > 1.0f)
                continue;
            nearest = distance;
            hit.objectName = submesh.objectName;
            hit.depth = depth;
        }
    }
    return hit;
}
}
