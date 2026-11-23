#include "stdafx.h"
#include "ffxi_creation_grounding.h"

#include <cfloat>
#include <cmath>

namespace FFXICreationGrounding
{
bool FindHorizontalPlacement(const noesisModel_t* model,
                             const ZoneCollision::Mesh& collisionMesh,
                             float outTranslation[3])
{
    if (outTranslation)
        outTranslation[0] = outTranslation[1] = outTranslation[2] = 0.0f;
    if (!model || !outTranslation || !collisionMesh.HasBounds())
        return false;

    bool haveVertex = false;
    float footY = 0.0f;
    float minY = 0.0f;
    for (const noesisModel_t::Submesh& submesh : model->submeshes)
    {
        const std::vector<FFXIVertex>& vertices =
            submesh.cpuBindVerts.empty() ? submesh.cpuVerts : submesh.cpuBindVerts;
        for (const FFXIVertex& vertex : vertices)
        {
            if (!haveVertex)
            {
                minY = footY = vertex.pos[1];
                haveVertex = true;
            }
            else
            {
                minY = std::min(minY, vertex.pos[1]);
                footY = std::max(footY, vertex.pos[1]);
            }
        }
    }
    if (!haveVertex)
        return false;

    // Average the lowest band rather than relying on one stray skirt/hair
    // vertex. In FFXI's Y-down model space the feet have the largest Y value.
    const float footBand = std::max(0.05f, (footY - minY) * 0.015f);
    float footX = 0.0f;
    float footZ = 0.0f;
    int footCount = 0;
    for (const noesisModel_t::Submesh& submesh : model->submeshes)
    {
        const std::vector<FFXIVertex>& vertices =
            submesh.cpuBindVerts.empty() ? submesh.cpuVerts : submesh.cpuBindVerts;
        for (const FFXIVertex& vertex : vertices)
        {
            if (vertex.pos[1] >= footY - footBand)
            {
                footX += vertex.pos[0];
                footZ += vertex.pos[2];
                ++footCount;
            }
        }
    }
    if (footCount <= 0)
        return false;
    footX /= static_cast<float>(footCount);
    footZ /= static_cast<float>(footCount);

    bool found = false;
    float bestX = 0.0f;
    float bestZ = 0.0f;
    float bestDistanceSquared = FLT_MAX;
    for (const ZoneCollision::Triangle& triangle : collisionMesh.Triangles())
    {
        // Accept ordinary walkable ground but reject walls and steep cliffs.
        if (std::fabs(triangle.normal[1]) < 0.75f ||
            footY < triangle.minY - 0.02f || footY > triangle.maxY + 0.02f)
        {
            continue;
        }

        float intersections[3][2] = {};
        int intersectionCount = 0;
        for (int edge = 0; edge < 3; ++edge)
        {
            const float* a = triangle.p[edge];
            const float* b = triangle.p[(edge + 1) % 3];
            const float dy = b[1] - a[1];
            if (std::fabs(dy) < 0.00001f)
                continue;
            const float t = (footY - a[1]) / dy;
            if (t >= 0.0f && t <= 1.0f && intersectionCount < 3)
            {
                intersections[intersectionCount][0] = a[0] + (b[0] - a[0]) * t;
                intersections[intersectionCount][1] = a[2] + (b[2] - a[2]) * t;
                ++intersectionCount;
            }
        }

        float candidateX = 0.0f;
        float candidateZ = 0.0f;
        if (intersectionCount > 0)
        {
            for (int i = 0; i < intersectionCount; ++i)
            {
                candidateX += intersections[i][0];
                candidateZ += intersections[i][1];
            }
            candidateX /= static_cast<float>(intersectionCount);
            candidateZ /= static_cast<float>(intersectionCount);
        }
        else
        {
            // A horizontal triangle can intersect the whole foot plane without
            // producing an edge crossing; use its centroid when levels match.
            const float centroidY = (triangle.p[0][1] + triangle.p[1][1] + triangle.p[2][1]) / 3.0f;
            if (std::fabs(centroidY - footY) > 0.02f)
                continue;
            candidateX = (triangle.p[0][0] + triangle.p[1][0] + triangle.p[2][0]) / 3.0f;
            candidateZ = (triangle.p[0][2] + triangle.p[1][2] + triangle.p[2][2]) / 3.0f;
        }

        const float dx = candidateX - footX;
        const float dz = candidateZ - footZ;
        const float distanceSquared = dx * dx + dz * dz;
        if (!found || distanceSquared < bestDistanceSquared)
        {
            found = true;
            bestDistanceSquared = distanceSquared;
            bestX = candidateX;
            bestZ = candidateZ;
        }
    }

    if (!found)
        return false;
    outTranslation[0] = bestX - footX;
    outTranslation[1] = 0.0f;
    outTranslation[2] = bestZ - footZ;
    return true;
}
}
