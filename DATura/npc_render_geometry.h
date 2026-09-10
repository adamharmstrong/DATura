#pragma once

#include <d3d9.h>

struct noesisModel_t;

namespace NpcRenderGeometry
{
    // Standing NPCs must not inherit the model viewer's locomotion default.
    bool SelectIdleAnimation(noesisModel_t* model);
    // Returns the local Y coordinate used to place an NPC nameplate above an
    // assembled model in DATura's downward-positive model coordinate system.
    float ComputeNameplateLocalY(const noesisModel_t* model);

    bool ProjectNpcNameplate(const D3DMATRIX& world, float localY,
                             const D3DMATRIX& viewProjection, float viewportWidth,
                             float viewportHeight, float* screenX, float* screenY,
                             float* depth, float* worldX, float* worldY, float* worldZ);
    int NameplateTextureExtent(int value);
}
