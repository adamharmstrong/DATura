#include "stdafx.h"
#include "model_renderer.h"

#include "d3d_math.h"
#include "d3d_model_buffers.h"
#include "d3d_model_render_state.h"
#include "noesis_rapi.h"
#include "zone_vegetation_animation.h"

#include <cmath>
#include <algorithm>

namespace ModelRenderer
{
namespace
{
void ApplyWaterMaterial(IDirect3DDevice9* device, const noesisModel_t::Submesh& mesh,
                        const ZoneWater::State& state)
{
    const auto& water = *mesh.water;
    const auto* texture = mesh.pResolvedTexture;
    IDirect3DTexture9* d3dTexture = texture ? texture->pD3DTex : nullptr;
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_CULLMODE,
        water.twoSided || !mesh.pResolvedMaterial || (mesh.pResolvedMaterial->flags & NMATFLAG_TWOSIDED)
            ? D3DCULL_NONE : D3DCULL_CW);
    device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    D3DBLEND source = D3DBLEND_SRCALPHA;
    D3DBLEND destination = D3DBLEND_INVSRCALPHA;
    switch (water.blendMode & 0xff)
    {
    case 0x00: source = D3DBLEND_ONE; destination = D3DBLEND_ZERO; break;
    case 0x46: case 0x47: source = D3DBLEND_ZERO; break;
    case 0x48: case 0x49: case 0x68: destination = D3DBLEND_ONE; break;
    }
    device->SetRenderState(D3DRS_SRCBLEND, source);
    device->SetRenderState(D3DRS_DESTBLEND, destination);
    D3DModelRenderState::SetTextureStageForOptionalTexture(device, d3dTexture, D3DTOP_MODULATE2X);
    const bool shader = D3DModelRenderState::SetFfxiTexturePixelShader(device, true,
        texture && texture->texType == NOESISTEX_DXT3, state.opacity, state.colorScale,
        d3dTexture != nullptr);
    if (!shader)
    {
        // Without a texture, stage zero must still expand the authored vertex
        // alpha. Stage one supplies the shader's RGB doubling and generator tint.
        if (!d3dTexture)
        {
            device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_ADD);
            device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
            device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        }
        const float fallbackScale = d3dTexture ? 0.5f : 1.0f;
        device->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_COLORVALUE(
            std::clamp(state.colorScale[0] * fallbackScale, 0.0f, 1.0f),
            std::clamp(state.colorScale[1] * fallbackScale, 0.0f, 1.0f),
            std::clamp(state.colorScale[2] * fallbackScale, 0.0f, 1.0f), state.opacity));
        device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE2X);
        device->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_CURRENT);
        device->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_TFACTOR);
        device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        device->SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
        device->SetTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
    }
    D3DMATRIX uv = D3DMath::BuildIdentity();
    uv._31 = state.uvOffset[0];
    uv._32 = state.uvOffset[1];
    device->SetTransform(D3DTS_TEXTURE0, &uv);
    device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
}
}

void TextureScrollState::Reset(IDirect3DDevice9* const device)
{
    enabled = false;
    speedU = 0.0f;
    speedV = 0.0f;
    D3DModelRenderState::SetTextureScroll(device, false);
}

void TextureScrollState::Update(IDirect3DDevice9* const device,
                                const bool newEnabled, const float newSpeedU,
                                const float newSpeedV)
{
    if (newEnabled == enabled &&
        (!newEnabled || (newSpeedU == speedU && newSpeedV == speedV)))
    {
        return;
    }
    D3DModelRenderState::SetTextureScroll(device, newEnabled, newSpeedU, newSpeedV);
    enabled = newEnabled;
    speedU = newSpeedU;
    speedV = newSpeedV;
}

void DrawActorPlanarShadow(const Context& context, noesisModel_t* const model,
                           const D3DMATRIX& baseWorld)
{
    IDirect3DDevice9* const device = context.device;
    if (!device || !model || context.rendersZoneObjects ||
        !context.dynamicActorShadows)
        return;

    float groundY = 0.0f;
    bool hasGround = false;
    for (const noesisModel_t::Submesh& submesh : model->submeshes)
    {
        const std::vector<FFXIVertex>& vertices = !submesh.cpuVerts.empty()
            ? submesh.cpuVerts : submesh.cpuBindVerts;
        for (const FFXIVertex& vertex : vertices)
        {
            if (!hasGround || vertex.pos[1] < groundY)
            {
                groundY = vertex.pos[1];
                hasGround = true;
            }
        }
    }
    if (!hasGround)
        return;

    const float angle = (static_cast<float>(context.minuteOfDay) / 1440.0f) * 6.2831853f;
    const float lightX = -0.45f * cosf(angle);
    const float lightY = -0.35f - 0.65f * fmaxf(sinf(angle - 1.5707963f), 0.15f);
    const float lightZ = 0.45f * sinf(angle);
    D3DMATRIX shadow = {};
    shadow._11 = lightY;
    shadow._21 = -lightX;
    shadow._23 = -lightZ;
    shadow._33 = lightY;
    shadow._41 = lightX * groundY;
    shadow._42 = groundY * lightY;
    shadow._43 = lightZ * groundY;
    shadow._44 = lightY;

    const D3DMATRIX shadowWorld = D3DMath::Multiply(baseWorld, shadow);
    D3DMATERIAL9 shadowMaterial = {};
    shadowMaterial.Diffuse = { 0.0f, 0.0f, 0.0f, 0.32f };
    shadowMaterial.Ambient = shadowMaterial.Diffuse;
    device->SetFVF(FFXI_VERTEX_FVF);
    device->SetTransform(D3DTS_WORLD, &shadowWorld);
    device->SetMaterial(&shadowMaterial);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_COLORVERTEX, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    device->SetRenderState(D3DRS_DEPTHBIAS,
        D3DModelRenderState::FloatBits(-0.00001f));
    device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetTexture(0, nullptr);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    for (const size_t index : model->opaqueSubmeshOrder)
        D3DModelBuffers::DrawSubmesh(device, model, model->submeshes[index]);
    device->SetTransform(D3DTS_WORLD, &baseWorld);
}

void PrepareFixedFunctionPass(const Context& context, const D3DMATRIX& baseWorld)
{
    if (!context.device)
        return;
    context.device->SetFVF(FFXI_VERTEX_FVF);
    D3DModelRenderState::ApplyFixedFunctionModelState(
        context.device, context.enableMipMapping);
    D3DModelRenderState::ApplyDynamicLighting(
        context.device, context.lightingQuality, context.minuteOfDay,
        context.rendersZoneObjects && context.indoorZone);
    context.device->SetRenderState(D3DRS_ZENABLE, TRUE);
    context.device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    context.device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    context.device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    context.device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESS);
    context.device->SetRenderState(D3DRS_DEPTHBIAS, 0);
    context.device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);
    context.device->SetTransform(D3DTS_WORLD, &baseWorld);
}

void DrawOpaqueBatch(const Context& context,
                     D3DModelRenderState::MaterialBindingCache& cache,
                     TextureScrollState& textureAnimation, noesisModel_t* const model,
                     const noesisModel_t::OpaqueBatch& batch)
{
    if (!context.device || !model)
        return;
    D3DModelRenderState::ApplyOpaqueMaterial(
        context.device, cache, batch.pMaterial, batch.pTexture);
    textureAnimation.Update(
        context.device, context.rendersZoneObjects && batch.animatedWater,
        0.012f, 0.006f);
    D3DModelBuffers::DrawOpaqueBatch(context.device, model, batch);
}

void DrawGeometry(const Context& context, noesisModel_t* pModel,
                  const D3DMATRIX& baseWorld)
{
    if (!context.device || !pModel)
        return;
    ModelRenderer::TextureScrollState textureAnimation;
    textureAnimation.Reset(context.device);

    D3DModelRenderState::MaterialBindingCache materialCache;

    const auto& renderVisibility = context.visibility;
    const float wind = ZoneVegetationAnimation::Weight(
        context.vegetationAnimationMode, GetTickCount64() * 0.001);
    for (auto& sm : pModel->submeshes)
        if (context.geometryPass != GeometryPass::Transparent &&
            !sm.windDisplacements.empty() && !sm.environmentObject &&
            ZoneObjectVisibility::PassesRenderVisibility(
                pModel, sm, context.rendersZoneObjects, renderVisibility))
            D3DModelBuffers::UploadVegetation(pModel, sm, wind);

    const bool hasOverrides =
        renderVisibility.overrides && !renderVisibility.overrides->empty();
    const bool hasHiddenObjects =
        renderVisibility.hiddenNames && !renderVisibility.hiddenNames->empty();
    const bool hasVisibilityMask =
        renderVisibility.visibleMapObjects && !renderVisibility.visibleMapObjects->empty();
    const bool editedObjectTransforms = context.rendersZoneObjects && hasOverrides;
    const bool canUseOpaqueBatches = !pModel->opaqueBatches.empty() &&
        (!context.rendersZoneObjects ||
            (!hasHiddenObjects && !hasOverrides && !hasVisibilityMask && !pModel->hasZoneLod));
    if (context.geometryPass != GeometryPass::Transparent && canUseOpaqueBatches)
    {
        for (const noesisModel_t::OpaqueBatch &batch : pModel->opaqueBatches)
        {
            if (context.rendersZoneObjects && renderVisibility.frustum &&
                !ZoneRenderFrustum::IntersectsBounds(
                    *renderVisibility.frustum, batch.boundsMin, batch.boundsMax, batch.hasBounds))
                continue;
            ModelRenderer::DrawOpaqueBatch(
                context, materialCache, textureAnimation, pModel, batch);
        }
    }
    else if (context.geometryPass != GeometryPass::Transparent)
    {
        for (size_t index : pModel->opaqueSubmeshOrder)
        {
            noesisModel_t::Submesh &sm = pModel->submeshes[index];
            if (!D3DModelBuffers::HasDrawBuffers(pModel, sm)) continue;
            if (context.rendersZoneObjects && sm.environmentObject) continue;
            if (!ZoneObjectVisibility::PassesRenderVisibility(
                    pModel, sm, context.rendersZoneObjects, renderVisibility)) continue;
            if (editedObjectTransforms)
                ZoneObjectTransform::ApplyWorldTransform(
                    context.device, sm.objectName, true, *renderVisibility.overrides, baseWorld);
            D3DModelRenderState::ApplyOpaqueMaterial(
                context.device, materialCache, sm.pResolvedMaterial, sm.pResolvedTexture);
            textureAnimation.Update(
                context.device, context.rendersZoneObjects && sm.animatedWater, 0.012f, 0.006f);
            D3DModelBuffers::DrawSubmesh(context.device, pModel, sm);
        }
    }

    if (context.geometryPass == GeometryPass::Opaque)
    {
        RestoreModelPassState(context.device, baseWorld, textureAnimation);
        return;
    }

    struct SortedSoftBlend
    {
        size_t index;
        float distanceSq;
    };
    std::vector<SortedSoftBlend> visibleSoftBlend;
    visibleSoftBlend.reserve(pModel->softBlendSubmeshOrder.size());
    const float cameraX = context.cameraPosition[0];
    const float cameraY = context.cameraPosition[1];
    const float cameraZ = context.cameraPosition[2];
    for (size_t index : pModel->softBlendSubmeshOrder)
    {
        const noesisModel_t::Submesh &sm = pModel->submeshes[index];
        if (sm.water && !context.waterRenderingEnabled) continue;
        if (!D3DModelBuffers::HasDrawBuffers(pModel, sm)) continue;
        if (context.rendersZoneObjects && sm.environmentObject) continue;
        if (!ZoneObjectVisibility::PassesRenderVisibility(
                pModel, sm, context.rendersZoneObjects, renderVisibility)) continue;
        const float dx = sm.boundsCenter[0] - cameraX;
        const float dy = sm.boundsCenter[1] - cameraY;
        const float dz = sm.boundsCenter[2] - cameraZ;
        visibleSoftBlend.push_back({ index, dx * dx + dy * dy + dz * dz });
    }
    std::stable_sort(visibleSoftBlend.begin(), visibleSoftBlend.end(),
        [&](const SortedSoftBlend &a, const SortedSoftBlend &b)
        {
            // MMB terrain overlays can lie below a wide water sheet even when
            // their centroids sort nearer to the eye. Finish world transparency
            // before blending water, as with the actor/zone pass boundary.
            const bool aWater = pModel->submeshes[a.index].water != nullptr;
            const bool bWater = pModel->submeshes[b.index].water != nullptr;
            if (aWater != bWater) return !aWater;
            return a.distanceSq > b.distanceSq;
        });

    context.device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    context.device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    context.device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    context.device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    context.device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    context.device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    context.device->SetRenderState(D3DRS_DEPTHBIAS, D3DModelRenderState::FloatBits(-0.000001f));
    materialCache.Invalidate();

    DWORD sceneLighting = FALSE;
    context.device->GetRenderState(D3DRS_LIGHTING, &sceneLighting);
    const double seconds = context.animationSeconds >= 0.0 && std::isfinite(context.animationSeconds)
        ? context.animationSeconds : GetTickCount64() * 0.001;
    bool previousWater = false;

    for (const SortedSoftBlend &draw : visibleSoftBlend)
    {
        noesisModel_t::Submesh &sm = pModel->submeshes[draw.index];
        if (editedObjectTransforms)
            ZoneObjectTransform::ApplyWorldTransform(
                context.device, sm.objectName, true, *renderVisibility.overrides, baseWorld);
        if (sm.water)
        {
            const auto state = ZoneWater::Evaluate(*sm.water, context.minuteOfDay, seconds);
            if (state.opacity <= 0.0f) continue;
            ApplyWaterMaterial(context.device, sm, state);
            materialCache.Invalidate();
            previousWater = true;
        }
        else
        {
            if (previousWater)
            {
                context.device->SetRenderState(D3DRS_LIGHTING, sceneLighting);
                context.device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
                context.device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
                context.device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
                context.device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
                context.device->SetPixelShader(nullptr);
                textureAnimation.Reset(context.device);
                previousWater = false;
            }
            D3DModelRenderState::ApplyTransparentMaterial(
                context.device, materialCache, sm.pResolvedMaterial, sm.pResolvedTexture);
            textureAnimation.Update(
                context.device, context.rendersZoneObjects && sm.animatedWater, -0.009f, 0.004f);
        }
        D3DModelBuffers::DrawSubmesh(context.device, pModel, sm);
    }

    if (previousWater)
    {
        context.device->SetRenderState(D3DRS_LIGHTING, sceneLighting);
        context.device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        context.device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
        textureAnimation.Reset(context.device);
    }

    ModelRenderer::RestoreModelPassState(
        context.device, baseWorld, textureAnimation);
}

void RestoreModelPassState(IDirect3DDevice9* const device,
                           const D3DMATRIX& baseWorld,
                           TextureScrollState& textureAnimation)
{
    if (!device)
        return;
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESS);
    device->SetRenderState(D3DRS_DEPTHBIAS, 0);
    device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);
    device->SetTexture(0, nullptr);
    textureAnimation.Update(device, false, 0.0f, 0.0f);
    device->SetPixelShader(nullptr);
    device->SetTransform(D3DTS_WORLD, &baseWorld);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE2X);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE2X);
    device->SetStreamSource(0, nullptr, 0, 0);
    device->SetIndices(nullptr);
}
}
