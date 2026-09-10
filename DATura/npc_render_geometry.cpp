#include "stdafx.h"
#include "npc_render_geometry.h"

#include "noesis_rapi.h"

namespace NpcRenderGeometry
{
bool SelectIdleAnimation(noesisModel_t* model)
{
    if (!model) return false;
    for (const char* name : { "idl_relaxed", "idl", "idle", "idl0", "idl1" })
    {
        if (noesisAnim_t* clip = model->FindAnimation(name))
        {
            model->pAnim = clip;
            return true;
        }
    }
    // An unknown action is not an idle fallback (it may walk, attack, or die).
    model->pAnim = nullptr;
    return false;
}

float ComputeNameplateLocalY(const noesisModel_t* model)
{
    if (!model)
        return -2.5f;

    bool haveVertex = false;
    float topY = 0.0f;
    for (const noesisModel_t::Submesh& submesh : model->submeshes)
    {
        // Use the same posed vertices rendered by the model. Composite DATs
        // can have component-local bind vertices far above the assembled mesh.
        const std::vector<FFXIVertex>& vertices =
            !submesh.cpuVerts.empty() ? submesh.cpuVerts : submesh.cpuBindVerts;
        for (const FFXIVertex& vertex : vertices)
        {
            // FFXI/DATura uses downward-positive model Y, so the top is the
            // minimum local value.
            if (!haveVertex || vertex.pos[1] < topY)
            {
                topY = vertex.pos[1];
                haveVertex = true;
            }
        }
    }
    return haveVertex ? topY - 0.35f : -2.5f;
}

bool ProjectNpcNameplate(const D3DMATRIX& world, const float localY,
                         const D3DMATRIX& viewProjection, const float viewportWidth,
                         const float viewportHeight, float* screenX, float* screenY,
                         float* depth, float* worldX, float* worldY, float* worldZ)
{
    const float x = localY * world._21 + world._41;
    const float y = localY * world._22 + world._42;
    const float z = localY * world._23 + world._43;
    const float clipX = x * viewProjection._11 + y * viewProjection._21 +
                        z * viewProjection._31 + viewProjection._41;
    const float clipY = x * viewProjection._12 + y * viewProjection._22 +
                        z * viewProjection._32 + viewProjection._42;
    const float clipZ = x * viewProjection._13 + y * viewProjection._23 +
                        z * viewProjection._33 + viewProjection._43;
    const float clipW = x * viewProjection._14 + y * viewProjection._24 +
                        z * viewProjection._34 + viewProjection._44;
    if (!(clipW > 0.001f) || clipZ < 0.0f || clipZ > clipW)
        return false;

    const float projectedX = (clipX / clipW * 0.5f + 0.5f) * viewportWidth;
    const float projectedY = (0.5f - clipY / clipW * 0.5f) * viewportHeight;
    if (projectedX < -160.0f || projectedX > viewportWidth + 160.0f ||
        projectedY < -32.0f || projectedY > viewportHeight + 32.0f)
    {
        return false;
    }

    if (screenX) *screenX = projectedX;
    if (screenY) *screenY = projectedY;
    if (depth) *depth = clipZ / clipW;
    if (worldX) *worldX = x;
    if (worldY) *worldY = y;
    if (worldZ) *worldZ = z;
    return true;
}

int NameplateTextureExtent(const int value)
{
    int extent = 1;
    while (extent < value)
        extent <<= 1;
    return extent;
}
}
