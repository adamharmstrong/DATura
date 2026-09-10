#include "stdafx.h"
#include "zone_collision_geometry.h"
#include "ffxi_coordinate_frame.h"

#include <cmath>
#include <algorithm>
#include <cfloat>

namespace
{
float Normalize(float value[3])
{
    const float length = std::sqrt(value[0] * value[0] + value[1] * value[1] + value[2] * value[2]);
    if (length > 0.00001f)
    {
        value[0] /= length;
        value[1] /= length;
        value[2] /= length;
    }
    return length;
}

float PointSegmentDistanceSquaredXZ(const float x, const float z, const float a[3], const float b[3])
{
    const float abX = b[0] - a[0];
    const float abZ = b[2] - a[2];
    const float lengthSquared = abX * abX + abZ * abZ;
    if (lengthSquared <= 0.00001f)
    {
        const float dx = x - a[0];
        const float dz = z - a[2];
        return dx * dx + dz * dz;
    }

    float t = ((x - a[0]) * abX + (z - a[2]) * abZ) / lengthSquared;
    t = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);
    const float pointX = a[0] + abX * t;
    const float pointZ = a[2] + abZ * t;
    const float dx = x - pointX;
    const float dz = z - pointZ;
    return dx * dx + dz * dz;
}
}

namespace ZoneCollision
{
SpatialIndex::SpatialIndex(const float cellSize)
    : cellSize_(cellSize > 0.0f ? cellSize : 8.0f)
{
}

void SpatialIndex::Clear()
{
    cells_.clear();
    queryMarks_.clear();
    queryMark_ = 0;
}

int SpatialIndex::GridCoordinate(const float value) const
{
    return static_cast<int>(std::floor(value / cellSize_));
}

long long SpatialIndex::GridKey(const int x, const int z)
{
    return (static_cast<long long>(x) << 32) ^ static_cast<unsigned int>(z);
}

void SpatialIndex::AddTriangle(const int triangleIndex, const Triangle& triangle, const float expansionRadius)
{
    if (triangleIndex >= 0 && static_cast<size_t>(triangleIndex) >= queryMarks_.size())
        queryMarks_.resize(static_cast<size_t>(triangleIndex) + 1, 0);
    const int minX = GridCoordinate(triangle.minX - expansionRadius);
    const int maxX = GridCoordinate(triangle.maxX + expansionRadius);
    const int minZ = GridCoordinate(triangle.minZ - expansionRadius);
    const int maxZ = GridCoordinate(triangle.maxZ + expansionRadius);
    for (int z = minZ; z <= maxZ; ++z)
    {
        for (int x = minX; x <= maxX; ++x)
            cells_[GridKey(x, z)].push_back(triangleIndex);
    }
}

void SpatialIndex::Query(const float x, const float z, const float radius,
                         std::vector<int>& outIndices) const
{
    outIndices.clear();
    ++queryMark_;
    if (queryMark_ == 0)
    {
        std::fill(queryMarks_.begin(), queryMarks_.end(), 0);
        ++queryMark_;
    }
    const int minX = GridCoordinate(x - radius);
    const int maxX = GridCoordinate(x + radius);
    const int minZ = GridCoordinate(z - radius);
    const int maxZ = GridCoordinate(z + radius);
    for (int gridZ = minZ; gridZ <= maxZ; ++gridZ)
    {
        for (int gridX = minX; gridX <= maxX; ++gridX)
        {
            const auto cell = cells_.find(GridKey(gridX, gridZ));
            if (cell == cells_.end())
                continue;
            for (const int triangleIndex : cell->second)
            {
                if (triangleIndex < 0 || static_cast<size_t>(triangleIndex) >= queryMarks_.size() ||
                    queryMarks_[static_cast<size_t>(triangleIndex)] == queryMark_)
                {
                    continue;
                }
                queryMarks_[static_cast<size_t>(triangleIndex)] = queryMark_;
                outIndices.push_back(triangleIndex);
            }
        }
    }
}

void Mesh::Clear()
{
    triangles_.clear();
    index_.Clear();
    hasBounds_ = false;
}

void Mesh::Reserve(const int triangleCount)
{
    if (triangleCount > 0)
        triangles_.reserve(static_cast<size_t>(triangleCount));
}

void Mesh::AddTriangle(const Triangle& triangle, const float indexExpansionRadius)
{
    const int triangleIndex = static_cast<int>(triangles_.size());
    triangles_.push_back(triangle);
    if (!hasBounds_)
    {
        minBounds_[0] = triangle.minX; maxBounds_[0] = triangle.maxX;
        minBounds_[1] = triangle.minY; maxBounds_[1] = triangle.maxY;
        minBounds_[2] = triangle.minZ; maxBounds_[2] = triangle.maxZ;
        hasBounds_ = true;
    }
    else
    {
        minBounds_[0] = std::min(minBounds_[0], triangle.minX);
        maxBounds_[0] = std::max(maxBounds_[0], triangle.maxX);
        minBounds_[1] = std::min(minBounds_[1], triangle.minY);
        maxBounds_[1] = std::max(maxBounds_[1], triangle.maxY);
        minBounds_[2] = std::min(minBounds_[2], triangle.minZ);
        maxBounds_[2] = std::max(maxBounds_[2], triangle.maxZ);
    }
    index_.AddTriangle(triangleIndex, triangle, indexExpansionRadius);
}

bool Mesh::Empty() const
{
    return triangles_.empty();
}

const std::vector<Triangle>& Mesh::Triangles() const
{
    return triangles_;
}

std::vector<Triangle>& Mesh::Triangles()
{
    return triangles_;
}

const SpatialIndex& Mesh::Index() const
{
    return index_;
}

SpatialIndex& Mesh::Index()
{
    return index_;
}

bool& Mesh::HasBounds()
{
    return hasBounds_;
}

const bool& Mesh::HasBounds() const
{
    return hasBounds_;
}

float (&Mesh::MinBounds())[3]
{
    return minBounds_;
}

float (&Mesh::MaxBounds())[3]
{
    return maxBounds_;
}

const float (&Mesh::MinBounds() const)[3]
{
    return minBounds_;
}

const float (&Mesh::MaxBounds() const)[3]
{
    return maxBounds_;
}

bool BuildTriangle(const float* sourcePoints, const bool mirrorX, Triangle& outTriangle)
{
    if (!sourcePoints)
        return false;

    outTriangle = {};
    for (int vertex = 0; vertex < 3; ++vertex)
        FFXICoordinateFrame::NativeDatToScene(sourcePoints + vertex * 3, mirrorX,
                                            outTriangle.p[vertex]);
    FFXICoordinateFrame::ReverseTriangleWindingIfReflected(
        outTriangle.p[1], outTriangle.p[2], mirrorX);

    const float edge0[3] =
    {
        outTriangle.p[1][0] - outTriangle.p[0][0],
        outTriangle.p[1][1] - outTriangle.p[0][1],
        outTriangle.p[1][2] - outTriangle.p[0][2],
    };
    const float edge1[3] =
    {
        outTriangle.p[2][0] - outTriangle.p[0][0],
        outTriangle.p[2][1] - outTriangle.p[0][1],
        outTriangle.p[2][2] - outTriangle.p[0][2],
    };
    outTriangle.normal[0] = edge0[1] * edge1[2] - edge0[2] * edge1[1];
    outTriangle.normal[1] = edge0[2] * edge1[0] - edge0[0] * edge1[2];
    outTriangle.normal[2] = edge0[0] * edge1[1] - edge0[1] * edge1[0];
    if (Normalize(outTriangle.normal) <= 0.0001f)
        return false;

    outTriangle.minX = outTriangle.maxX = outTriangle.p[0][0];
    outTriangle.minY = outTriangle.maxY = outTriangle.p[0][1];
    outTriangle.minZ = outTriangle.maxZ = outTriangle.p[0][2];
    for (int vertex = 1; vertex < 3; ++vertex)
    {
        outTriangle.minX = (outTriangle.p[vertex][0] < outTriangle.minX) ? outTriangle.p[vertex][0] : outTriangle.minX;
        outTriangle.maxX = (outTriangle.p[vertex][0] > outTriangle.maxX) ? outTriangle.p[vertex][0] : outTriangle.maxX;
        outTriangle.minY = (outTriangle.p[vertex][1] < outTriangle.minY) ? outTriangle.p[vertex][1] : outTriangle.minY;
        outTriangle.maxY = (outTriangle.p[vertex][1] > outTriangle.maxY) ? outTriangle.p[vertex][1] : outTriangle.maxY;
        outTriangle.minZ = (outTriangle.p[vertex][2] < outTriangle.minZ) ? outTriangle.p[vertex][2] : outTriangle.minZ;
        outTriangle.maxZ = (outTriangle.p[vertex][2] > outTriangle.maxZ) ? outTriangle.p[vertex][2] : outTriangle.maxZ;
    }
    return true;
}

bool PointInTriangleXZ(const float x, const float z, const Triangle& triangle)
{
    const float x0 = triangle.p[0][0], z0 = triangle.p[0][2];
    const float x1 = triangle.p[1][0], z1 = triangle.p[1][2];
    const float x2 = triangle.p[2][0], z2 = triangle.p[2][2];
    const float denominator = (z1 - z2) * (x0 - x2) + (x2 - x1) * (z0 - z2);
    if (std::fabs(denominator) < 0.00001f)
        return false;
    const float a = ((z1 - z2) * (x - x2) + (x2 - x1) * (z - z2)) / denominator;
    const float b = ((z2 - z0) * (x - x2) + (x0 - x2) * (z - z2)) / denominator;
    const float c = 1.0f - a - b;
    constexpr float epsilon = -0.001f;
    return a >= epsilon && b >= epsilon && c >= epsilon;
}

bool PointNearTriangleXZ(const float x, const float z, const Triangle& triangle, const float radius)
{
    if (x < triangle.minX - radius || x > triangle.maxX + radius ||
        z < triangle.minZ - radius || z > triangle.maxZ + radius)
    {
        return false;
    }
    if (PointInTriangleXZ(x, z, triangle))
        return true;
    const float radiusSquared = radius * radius;
    return PointSegmentDistanceSquaredXZ(x, z, triangle.p[0], triangle.p[1]) <= radiusSquared ||
           PointSegmentDistanceSquaredXZ(x, z, triangle.p[1], triangle.p[2]) <= radiusSquared ||
           PointSegmentDistanceSquaredXZ(x, z, triangle.p[2], triangle.p[0]) <= radiusSquared;
}

bool FindFloorAt(const std::vector<Triangle>& triangles, const std::vector<int>& candidates,
                 const float x, const float z, float minY, float maxY,
                 float* outY, float outNormal[3])
{
    if (minY > maxY)
    {
        const float temp = minY;
        minY = maxY;
        maxY = temp;
    }

    bool found = false;
    float bestY = maxY;
    for (const int index : candidates)
    {
        if (index < 0 || index >= static_cast<int>(triangles.size()))
            continue;
        const Triangle& triangle = triangles[static_cast<size_t>(index)];
        if (triangle.normal[1] < 0.35f && triangle.normal[1] > -0.35f)
            continue;
        if (maxY < triangle.minY - 0.1f || minY > triangle.maxY + 0.1f ||
            !PointInTriangleXZ(x, z, triangle))
        {
            continue;
        }

        const float denominator = triangle.normal[1];
        if (std::fabs(denominator) < 0.0001f)
            continue;
        const float y = triangle.p[0][1] -
            (triangle.normal[0] * (x - triangle.p[0][0]) +
             triangle.normal[2] * (z - triangle.p[0][2])) / denominator;
        if (y >= minY && y <= maxY && (!found || y < bestY))
        {
            found = true;
            bestY = y;
            if (outNormal)
            {
                outNormal[0] = triangle.normal[0];
                outNormal[1] = triangle.normal[1];
                outNormal[2] = triangle.normal[2];
                if (outNormal[1] < 0.0f)
                {
                    outNormal[0] = -outNormal[0];
                    outNormal[1] = -outNormal[1];
                    outNormal[2] = -outNormal[2];
                }
            }
        }
    }

    if (found && outY)
        *outY = bestY;
    return found;
}

bool FindFloorAt(const std::vector<Triangle>& triangles, const SpatialIndex& index,
                 const float queryRadius, const float x, const float z, const float minY, const float maxY,
                 float* outY, float outNormal[3])
{
    std::vector<int> candidates;
    index.Query(x, z, queryRadius, candidates);
    return FindFloorAt(triangles, candidates, x, z, minY, maxY, outY, outNormal);
}

bool OverlapsWallAt(const std::vector<Triangle>& triangles, const SpatialIndex& index,
                    const float x, const float y, const float z, const float playerRadius,
                    const float playerHeight, const float stepHeight)
{
    std::vector<int> candidates;
    index.Query(x, z, playerRadius + 0.25f, candidates);
    const float playerTopY = y - playerHeight;
    const float playerBottomY = y - 0.001f;
    for (const int triangleIndex : candidates)
    {
        if (triangleIndex < 0 || triangleIndex >= static_cast<int>(triangles.size()))
            continue;
        const Triangle& triangle = triangles[static_cast<size_t>(triangleIndex)];
        if (triangle.normal[1] > 0.45f || triangle.normal[1] < -0.45f ||
            playerBottomY < triangle.minY || playerTopY > triangle.maxY ||
            !PointNearTriangleXZ(x, z, triangle, playerRadius))
        {
            continue;
        }

        // Only the height above the feet matters, not how far the face
        // extends below the ground. World Y increases downward.
        const bool shortStepFace = triangle.minY >= y - stepHeight;
        if (!shortStepFace)
            return true;
    }
    return false;
}

void ResolveHorizontalCollision(const std::vector<Triangle>& triangles, const SpatialIndex& index,
                                const float playerRadius, const float playerHeight, const float stepHeight,
                                const bool playerOnGround, const float oldX, const float oldZ,
                                float& newX, const float playerY, float& newZ)
{
    std::vector<int> candidates;
    index.Query(newX, newZ, playerRadius + 0.5f, candidates);
    for (int pass = 0; pass < 3; ++pass)
    {
        bool adjusted = false;
        for (const int triangleIndex : candidates)
        {
            if (triangleIndex < 0 || triangleIndex >= static_cast<int>(triangles.size()))
                continue;
            const Triangle& triangle = triangles[static_cast<size_t>(triangleIndex)];
            if (triangle.normal[1] > 0.45f || triangle.normal[1] < -0.45f)
                continue;

            const float playerTopY = playerY - playerHeight;
            const float playerBottomY = playerY - 0.001f;
            if (playerBottomY < triangle.minY || playerTopY > triangle.maxY ||
                !PointNearTriangleXZ(newX, newZ, triangle, playerRadius))
            {
                continue;
            }

            const bool shortStepFace =
                playerOnGround && triangle.minY >= playerY - stepHeight;
            if (shortStepFace)
                continue;

            float normalX = triangle.normal[0];
            float normalZ = triangle.normal[2];
            const float normalLength = std::sqrt(normalX * normalX + normalZ * normalZ);
            if (normalLength < 0.0001f)
                continue;
            normalX /= normalLength;
            normalZ /= normalLength;

            float oldDistance = (oldX - triangle.p[0][0]) * normalX + (oldZ - triangle.p[0][2]) * normalZ;
            float newDistance = (newX - triangle.p[0][0]) * normalX + (newZ - triangle.p[0][2]) * normalZ;
            // Keep the player on the side they started on, independently of
            // triangle winding or whether this move crosses the plane.
            if (oldDistance < 0.0f || (oldDistance == 0.0f && newDistance > 0.0f))
            {
                normalX = -normalX;
                normalZ = -normalZ;
                oldDistance = -oldDistance;
                newDistance = -newDistance;
            }

            if (newDistance >= playerRadius)
                continue;
            const float push = playerRadius - newDistance;
            if (push > 0.0f && push < playerRadius * 2.5f)
            {
                newX += normalX * push;
                newZ += normalZ * push;
                adjusted = true;
            }
        }
        if (!adjusted)
            break;
    }
}

bool FindNearestSafeFloor(const std::vector<Triangle>& triangles, const SpatialIndex& index,
                          const float playerRadius, const float playerHeight, const float stepHeight,
                          const float originX, const float originY, const float originZ,
                          float* outX, float* outY, float* outZ)
{
    if (triangles.empty())
        return false;

    constexpr float searchUp = 1.4f;
    constexpr float searchDown = 12.0f;
    constexpr float maxSnapUp = 1.25f;
    constexpr float radii[] = { 0.0f, 0.35f, 0.7f, 1.1f, 1.6f, 2.3f, 3.2f, 4.5f, 6.0f };

    bool found = false;
    float bestX = originX;
    float bestY = originY;
    float bestZ = originZ;
    float bestDistanceSquared = FLT_MAX;
    for (const float radius : radii)
    {
        const int samples = radius <= 0.0f ? 1 : (radius < 2.0f ? 12 : 24);
        for (int sample = 0; sample < samples; ++sample)
        {
            float sampleX = originX;
            float sampleZ = originZ;
            if (radius > 0.0f)
            {
                const float angle = (6.28318530718f * static_cast<float>(sample)) / static_cast<float>(samples);
                sampleX += std::cos(angle) * radius;
                sampleZ += std::sin(angle) * radius;
            }

            float floorY = 0.0f;
            float floorNormal[3] = {};
            if (!FindFloorAt(triangles, index, playerRadius, sampleX, sampleZ,
                             originY - searchUp, originY + searchDown, &floorY, floorNormal) ||
                floorY < originY - maxSnapUp ||
                OverlapsWallAt(triangles, index, sampleX, floorY, sampleZ,
                               playerRadius, playerHeight, stepHeight))
            {
                continue;
            }

            const float dx = sampleX - originX;
            const float dy = (floorY - originY) * 0.25f;
            const float dz = sampleZ - originZ;
            const float distanceSquared = dx * dx + dy * dy + dz * dz;
            if (!found || distanceSquared < bestDistanceSquared)
            {
                found = true;
                bestDistanceSquared = distanceSquared;
                bestX = sampleX;
                bestY = floorY;
                bestZ = sampleZ;
            }
        }
        if (found)
            break;
    }

    if (found)
    {
        if (outX) *outX = bestX;
        if (outY) *outY = bestY;
        if (outZ) *outZ = bestZ;
    }
    return found;
}

bool TryMoveHorizontal(const Mesh& mesh, const float playerRadius, const float playerHeight,
                       const float stepHeight, const float maxStepDown, float position[3],
                       float& verticalVelocity, bool& onGround,
                       const float targetX, const float targetZ)
{
    const float oldX = position[0];
    const float oldZ = position[2];
    float newX = targetX;
    float newZ = targetZ;
    ResolveHorizontalCollision(mesh.Triangles(), mesh.Index(), playerRadius, playerHeight,
                               stepHeight, onGround, oldX, oldZ, newX, position[1], newZ);

    if (onGround)
    {
        float floorY = 0.0f;
        float floorNormal[3] = {};
        const bool hasFloor = FindFloorAt(mesh.Triangles(), mesh.Index(), playerRadius,
                                          newX, newZ, position[1] - stepHeight,
                                          position[1] + maxStepDown, &floorY, floorNormal);
        if (hasFloor)
        {
            position[1] = floorY;
            verticalVelocity = 0.0f;
        }
        // A missing center sample is not a wall. Cross seams and let vertical
        // motion handle unsupported ground and drops.
        onGround = hasFloor;
    }

    position[0] = newX;
    position[2] = newZ;
    return true;
}

void MoveHorizontal(const Mesh& mesh, const float playerRadius, const float playerHeight,
                    const float stepHeight, const float maxStepDown, float position[3],
                    float& verticalVelocity, bool& onGround,
                    const float deltaX, const float deltaZ)
{
    const float distance = std::sqrt(deltaX * deltaX + deltaZ * deltaZ);
    if (distance <= 0.0001f)
        return;

    // Keep each move smaller than the radius so thin walls cannot be skipped.
    const float maxStep = std::max(0.001f, std::min(0.35f, playerRadius * 0.5f));
    const int clampedSteps = std::max(1, static_cast<int>(std::ceil(distance / maxStep)));
    const float stepX = deltaX / static_cast<float>(clampedSteps);
    const float stepZ = deltaZ / static_cast<float>(clampedSteps);
    for (int step = 0; step < clampedSteps; ++step)
    {
        if (!TryMoveHorizontal(mesh, playerRadius, playerHeight, stepHeight, maxStepDown,
                               position, verticalVelocity, onGround,
                               position[0] + stepX, position[2] + stepZ))
        {
            break;
        }
    }
}

bool UpdateVerticalMotion(const Mesh& mesh, const float playerRadius, const float stepHeight,
                          const float initialGroundStep, const float floorSearchDistance,
                          const float gravity, const float maximumFallSpeed, const float dt,
                          float position[3], float& verticalVelocity, bool& onGround,
                          const float playerHeight)
{
    float floorY = 0.0f;
    float floorNormal[3] = {};
    const float searchMinY = position[1] - (onGround ? stepHeight : initialGroundStep);
    const float searchMaxY = position[1] + floorSearchDistance;
    const bool hasFloor = FindFloorAt(mesh.Triangles(), mesh.Index(), playerRadius,
                                      position[0], position[2], searchMinY, searchMaxY,
                                      &floorY, floorNormal);

    verticalVelocity += gravity * dt;
    if (verticalVelocity > maximumFallSpeed)
        verticalVelocity = maximumFallSpeed;
    const float previousY = position[1];
    position[1] += verticalVelocity * dt;

    if (verticalVelocity < 0.0f && playerHeight > 0.0f)
    {
        // Sweep the top of the player upward, stopping at the first overhead
        // surface. Collision DATs use both windings for horizontal surfaces.
        const float oldHeadY = previousY - playerHeight;
        const float newHeadY = position[1] - playerHeight;
        float ceilingY = newHeadY;
        bool hitCeiling = false;
        std::vector<int> candidates;
        mesh.Index().Query(position[0], position[2], playerRadius, candidates);
        for (int index : candidates)
        {
            const Triangle& triangle = mesh.Triangles()[index];
            if (std::fabs(triangle.normal[1]) < 0.35f ||
                !PointNearTriangleXZ(position[0], position[2], triangle, playerRadius))
                continue;
            const float y = triangle.p[0][1] -
                (triangle.normal[0] * (position[0] - triangle.p[0][0]) +
                 triangle.normal[2] * (position[2] - triangle.p[0][2])) / triangle.normal[1];
            if (y >= newHeadY && y <= oldHeadY && (!hitCeiling || y > ceilingY))
            {
                ceilingY = y;
                hitCeiling = true;
            }
        }
        if (hitCeiling)
        {
            position[1] = ceilingY + playerHeight;
            verticalVelocity = 0.0f;
            onGround = false;
            return false;
        }
    }

    if (hasFloor && verticalVelocity >= 0.0f && position[1] >= floorY - 0.04f)
    {
        position[1] = floorY;
        verticalVelocity = 0.0f;
        onGround = true;
        return true;
    }

    onGround = false;
    return false;
}

void DrawOverlay(IDirect3DDevice9 *device, const Mesh& mesh)
{
    if (!device || mesh.Empty())
        return;

    struct OverlayVertex
    {
        float x, y, z;
        DWORD color;
    };

    const std::vector<Triangle>& triangles = mesh.Triangles();
    std::vector<OverlayVertex> vertices;
    vertices.reserve(triangles.size() * 3);
    const DWORD color = D3DCOLOR_ARGB(88, 255, 128, 24);
    for (const Triangle& triangle : triangles)
    {
        for (int vertex = 0; vertex < 3; ++vertex)
        {
            OverlayVertex output = {};
            output.x = triangle.p[vertex][0];
            output.y = triangle.p[vertex][1];
            output.z = triangle.p[vertex][2];
            output.color = color;
            vertices.push_back(output);
        }
    }

    if (vertices.empty())
        return;

    D3DMATRIX world = {};
    world._11 = world._22 = world._33 = world._44 = 1.0f;
    device->SetTransform(D3DTS_WORLD, &world);
    device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
    device->SetTexture(0, nullptr);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_ZENABLE, TRUE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, static_cast<UINT>(triangles.size()),
                            vertices.data(), sizeof(OverlayVertex));
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
}
}
