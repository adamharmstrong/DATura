#pragma once

#include <map>
#include <vector>
#include <cstdint>

struct IDirect3DDevice9;

namespace ZoneCollision
{
    struct Triangle
    {
        float p[3][3];
        float normal[3];
        float minX, maxX;
        float minY, maxY;
        float minZ, maxZ;
    };

    class SpatialIndex
    {
    public:
        explicit SpatialIndex(float cellSize = 8.0f);

        void Clear();
        void AddTriangle(int triangleIndex, const Triangle& triangle, float expansionRadius);
        void Query(float x, float z, float radius, std::vector<int>& outIndices) const;

    private:
        int GridCoordinate(float value) const;
        static long long GridKey(int x, int z);

        float cellSize_;
        std::map<long long, std::vector<int>> cells_;
        // Query stamps preserve first-seen ordering without linearly searching
        // the candidate vector for every overlapping grid-cell entry.
        mutable std::vector<std::uint32_t> queryMarks_;
        mutable std::uint32_t queryMark_ = 0;
    };

    class Mesh
    {
    public:
        void Clear();
        void Reserve(int triangleCount);
        void AddTriangle(const Triangle& triangle, float indexExpansionRadius);

        bool Empty() const;
        const std::vector<Triangle>& Triangles() const;
        std::vector<Triangle>& Triangles();
        const SpatialIndex& Index() const;
        SpatialIndex& Index();
        bool& HasBounds();
        const bool& HasBounds() const;
        float (&MinBounds())[3];
        float (&MaxBounds())[3];
        const float (&MinBounds() const)[3];
        const float (&MaxBounds() const)[3];

    private:
        std::vector<Triangle> triangles_;
        SpatialIndex index_;
        float minBounds_[3] = {};
        float maxBounds_[3] = {};
        bool hasBounds_ = false;
    };

    bool BuildTriangle(const float* sourcePoints, bool mirrorX, Triangle& outTriangle);
    bool PointInTriangleXZ(float x, float z, const Triangle& triangle);
    bool PointNearTriangleXZ(float x, float z, const Triangle& triangle, float radius);
    bool FindFloorAt(const std::vector<Triangle>& triangles, const std::vector<int>& candidates,
                     float x, float z, float minY, float maxY,
                     float* outY, float outNormal[3]);
    bool FindFloorAt(const std::vector<Triangle>& triangles, const SpatialIndex& index,
                     float queryRadius, float x, float z, float minY, float maxY,
                     float* outY, float outNormal[3]);
    bool OverlapsWallAt(const std::vector<Triangle>& triangles, const SpatialIndex& index,
                        float x, float y, float z, float playerRadius,
                        float playerHeight, float stepHeight);
    void ResolveHorizontalCollision(const std::vector<Triangle>& triangles, const SpatialIndex& index,
                                    float playerRadius, float playerHeight, float stepHeight,
                                    bool playerOnGround, float oldX, float oldZ,
                                    float& newX, float playerY, float& newZ);
    bool FindNearestSafeFloor(const std::vector<Triangle>& triangles, const SpatialIndex& index,
                              float playerRadius, float playerHeight, float stepHeight,
                              float originX, float originY, float originZ,
                              float* outX, float* outY, float* outZ);
    bool TryMoveHorizontal(const Mesh& mesh, float playerRadius, float playerHeight,
                           float stepHeight, float maxStepDown, float position[3],
                           float& verticalVelocity, bool& onGround,
                           float targetX, float targetZ);
    void MoveHorizontal(const Mesh& mesh, float playerRadius, float playerHeight,
                        float stepHeight, float maxStepDown, float position[3],
                        float& verticalVelocity, bool& onGround,
                        float deltaX, float deltaZ);
    bool UpdateVerticalMotion(const Mesh& mesh, float playerRadius, float stepHeight,
                              float initialGroundStep, float floorSearchDistance,
                              float gravity, float maximumFallSpeed, float dt,
                              float position[3], float& verticalVelocity, bool& onGround,
                              float playerHeight = 0.0f);
    void DrawOverlay(IDirect3DDevice9 *device, const Mesh& mesh);
}
