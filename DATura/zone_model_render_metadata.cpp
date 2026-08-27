#include "stdafx.h"
#include "zone_model_render_metadata.h"

#include "d3d_model_buffers.h"
#include "zone_environment_identity.h"

namespace ZoneModelRenderMetadata
{
bool IsAnimatedWaterSurface(const noesisModel_t::Submesh &submesh)
{
    (void)submesh;
    // Placed water is ordinary zone geometry. UV motion is only valid when a
    // decoded generator/controller explicitly supplies the animation.
    return false;
}

void Prepare(noesisModel_t *model, IDirect3DDevice9 *device)
{
    if (!model || model->renderMetadataPrepared)
        return;

    model->UpdateSubmeshBounds();
    model->opaqueSubmeshOrder.clear();
    model->softBlendSubmeshOrder.clear();
    for (size_t index = 0; index < model->submeshes.size(); ++index)
    {
        noesisModel_t::Submesh &submesh = model->submeshes[index];
        submesh.pResolvedMaterial = (model->pMatData && !submesh.materialName.empty()) ?
            model->pMatData->FindMaterial(submesh.materialName.c_str()) : nullptr;
        submesh.pResolvedTexture = nullptr;
        if (submesh.pResolvedMaterial && model->pMatData &&
            submesh.pResolvedMaterial->texIdx >= 0 &&
            submesh.pResolvedMaterial->texIdx < model->pMatData->texCount)
        {
            submesh.pResolvedTexture = model->pMatData->textures[submesh.pResolvedMaterial->texIdx];
        }
        submesh.environmentObject = ZoneEnvironmentIdentity::IsEnvironmentObjectName(submesh.objectName);
        submesh.animatedWater = IsAnimatedWaterSurface(submesh);
        submesh.softBlend = submesh.pResolvedMaterial && !submesh.pResolvedMaterial->noDefaultBlend;
        if (submesh.softBlend)
            model->softBlendSubmeshOrder.push_back(index);
        else
            model->opaqueSubmeshOrder.push_back(index);
    }
    model->renderMetadataPrepared = true;
    D3DModelBuffers::BuildStaticOpaqueBatches(model, device);
}
}
