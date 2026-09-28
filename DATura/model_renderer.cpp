#include "stdafx.h"
#include "model_renderer.h"

#include "d3d_math.h"
#include "d3d_model_buffers.h"
#include "d3d_model_render_state.h"
#include "model_ff11.h"
#include "noesis_rapi.h"
#include "zone_object_transform.h"
#include "zone_environment_animation.h"
#include "zone_vegetation_animation.h"

#include <cmath>
#include <algorithm>

namespace ModelRenderer
{
namespace
{
float EvaluatePointLightKeyframe(const ff11KeyframeRecord_t* keyframe,
                                 float phase, const float fallback)
{
    if (!keyframe || keyframe->pairCount <= 0)
        return fallback;
    if (keyframe->pairCount == 1)
        return fallback;

    phase = std::clamp(phase, 0.0f, 1.0f);
    const int last = keyframe->pairCount - 1;
    int previous = 0;
    for (int next = 1; next <= last; ++next)
    {
        if (!std::isfinite(keyframe->times[next]) || !std::isfinite(keyframe->values[next]))
            break;
        if (keyframe->times[next] >= phase)
        {
            const float previousTime = previous == 0 ? 0.0f : keyframe->times[previous];
            const float previousValue = previous == 0 ? fallback : keyframe->values[previous];
            const float span = keyframe->times[next] - previousTime;
            if (span <= 0.0f)
                return keyframe->values[next];
            const float blend = (phase - previousTime) / span;
            return previousValue + (keyframe->values[next] - previousValue) * blend;
        }
        previous = next;
    }
    return keyframe->values[previous];
}

float AnimatedPointLightValue(const Context& context, const ff11GeneratorRecord_t& generator,
                              const char* keyframeName, const float fallback,
                              const double seconds)
{
    if (!context.authoredKeyframes || !keyframeName || !keyframeName[0])
        return fallback;
    const float cycleFrames = generator.particleLifetimeFrames > 0
        ? static_cast<float>(generator.particleLifetimeFrames) : 60.0f;
    const float phase = static_cast<float>(
        fmod(seconds * 60.0, static_cast<double>(cycleFrames)) / cycleFrames);
    return EvaluatePointLightKeyframe(
        ZoneEnvironmentAnimation::FindKeyframe(
            generator, keyframeName, *context.authoredKeyframes), phase, fallback);
}

void TransformPoint(const D3DMATRIX& world, const float local[3], float out[3])
{
    out[0] = local[0] * world._11 + local[1] * world._21 + local[2] * world._31 + world._41;
    out[1] = local[0] * world._12 + local[1] * world._22 + local[2] * world._32 + world._42;
    out[2] = local[0] * world._13 + local[1] * world._23 + local[2] * world._33 + world._43;
}

void ApplyAuthoredPointLights(const Context& context, const float referencePosition[3])
{
    constexpr int kFirstPointLight = 2;
    constexpr int kPointLightSlots = 6;
    for (int slot = 0; slot < kPointLightSlots; ++slot)
        context.device->LightEnable(kFirstPointLight + slot, FALSE);

    if (context.lightingQuality <= 0 || !context.authoredGenerators)
        return;

    const double seconds = context.animationSeconds >= 0.0 && std::isfinite(context.animationSeconds)
        ? context.animationSeconds
        : static_cast<double>(GetTickCount64()) / 1000.0;

    struct Candidate
    {
        D3DVECTOR position;
        float distanceSquared;
        float range;
        float red;
        float green;
        float blue;
    };
    std::vector<Candidate> candidates;
    for (const ff11GeneratorRecord_t& generator : *context.authoredGenerators)
    {
        if (generator.linkedDataType != 0x47 || !generator.hasPointLightSetup ||
            !generator.hasSpawnPosition || !generator.hasColor)
            continue;

        const float authoredRange = AnimatedPointLightValue(context, generator,
            generator.pointLightRangeKeyframe, generator.pointLightRange, seconds);
        const float authoredPower = AnimatedPointLightValue(context, generator,
            generator.pointLightPowerKeyframe, generator.pointLightPower, seconds);
        const float rangeRatio = AnimatedPointLightValue(context, generator,
            generator.pointLightRangeRatioKeyframe, generator.pointLightRangeRatio, seconds);
        const float powerRatio = AnimatedPointLightValue(context, generator,
            generator.pointLightPowerRatioKeyframe, generator.pointLightPowerRatio, seconds);
        const float range = authoredRange * rangeRatio / 16.0f;
        const float intensity = AnimatedPointLightValue(context, generator,
            generator.alphaKeyframe, 1.0f, seconds);
        const float power = authoredPower * powerRatio * intensity;
        if (!std::isfinite(range) || !std::isfinite(power) || range <= 0.01f || power <= 0.0f)
            continue;

        const float cycleFrames = generator.particleLifetimeFrames > 0
            ? static_cast<float>(generator.particleLifetimeFrames) : 60.0f;
        const float ageFrames = static_cast<float>(
            fmod(seconds * 60.0, static_cast<double>(cycleFrames)));
        const float wholeFrames = floorf(ageFrames);
        const float fraction = ageFrames - wholeFrames;
        float animatedPosition[3] = {};
        for (int axis = 0; axis < 3; ++axis)
        {
            const float velocity = generator.hasLinearVelocity
                ? generator.linearVelocity[axis] : 0.0f;
            const float acceleration = generator.hasLinearAcceleration
                ? generator.linearAcceleration[axis] : 0.0f;
            animatedPosition[axis] = generator.spawnPosition[axis] + velocity * ageFrames +
                acceleration * (0.5f * wholeFrames * (wholeFrames - 1.0f) +
                                fraction * wholeFrames);
        }
        const float x = context.mirrorAuthoredLightX
            ? -animatedPosition[0] : animatedPosition[0];
        const float y = animatedPosition[1];
        const float z = animatedPosition[2];
        const D3DVECTOR position = { x, y, z };
        const float dx = position.x - referencePosition[0];
        const float dy = position.y - referencePosition[1];
        const float dz = position.z - referencePosition[2];
        const float distanceSquared = dx * dx + dy * dy + dz * dz;
        if (distanceSquared <= range * range)
        {
            const DWORD color = generator.colorBgra;
            const float redScale = AnimatedPointLightValue(context, generator,
                generator.redKeyframe, 1.0f, seconds);
            const float greenScale = AnimatedPointLightValue(context, generator,
                generator.greenKeyframe, 1.0f, seconds);
            const float blueScale = AnimatedPointLightValue(context, generator,
                generator.blueKeyframe, 1.0f, seconds);
            candidates.push_back({ position, distanceSquared, range,
                static_cast<float>((color >> 16) & 0xff) / 128.0f * power * redScale,
                static_cast<float>((color >> 8) & 0xff) / 128.0f * power * greenScale,
                static_cast<float>(color & 0xff) / 128.0f * power * blueScale });
        }
    }
    std::sort(candidates.begin(), candidates.end(),
        [](const Candidate& a, const Candidate& b) { return a.distanceSquared < b.distanceSquared; });

    const int count = std::min<int>(kPointLightSlots, static_cast<int>(candidates.size()));
    for (int index = 0; index < count; ++index)
    {
        const Candidate& candidate = candidates[index];
        D3DLIGHT9 light = {};
        light.Type = D3DLIGHT_POINT;
        light.Diffuse = { candidate.red, candidate.green, candidate.blue, 1.0f };
        light.Position = candidate.position;
        light.Range = candidate.range;
        light.Attenuation0 = 1.0f;
        light.Attenuation1 = 1.0f / candidate.range;
        context.device->SetLight(kFirstPointLight + index, &light);
        context.device->LightEnable(kFirstPointLight + index, TRUE);
    }
}

D3DMATRIX BuildPlanarShadowWorldAtY(const D3DMATRIX& baseWorld, const float groundY,
                                    const Context& context)
{
    float lightDirection[3] = {};
    if (context.automaticLightDirection && context.useAuthoredLightDirection)
    {
        lightDirection[0] = context.authoredMainLightDirection[0];
        lightDirection[1] = context.authoredMainLightDirection[1];
        lightDirection[2] = context.authoredMainLightDirection[2];
    }
    else
    {
        D3DModelRenderState::CalculateDirectionalLight(
            context.minuteOfDay, context.automaticLightDirection,
            context.lightAzimuthDegrees, context.lightElevationDegrees, lightDirection);
        // The retail celestial setup uses the inverse secondary light after
        // the main light passes below the horizon. Project shadows from the
        // source that is currently above the scene.
        if (context.automaticLightDirection && lightDirection[1] > 0.0f)
        {
            lightDirection[0] = -lightDirection[0];
            lightDirection[1] = -lightDirection[1];
            lightDirection[2] = -lightDirection[2];
        }
    }
    float lightX = lightDirection[0];
    const float lightY = lightDirection[1];
    float lightZ = lightDirection[2];
    if (fabsf(lightY) < 0.0001f)
        return {};

    const float horizontalScale = sqrtf(lightX * lightX + lightZ * lightZ) / fabsf(lightY);
    if (context.shadowMaximumLength > 0.0f && horizontalScale > context.shadowMaximumLength)
    {
        const float scale = context.shadowMaximumLength / horizontalScale;
        lightX *= scale;
        lightZ *= scale;
    }

    const float inverseLightY = 1.0f / lightY;
    D3DMATRIX shadow = D3DMath::BuildIdentity();
    shadow._21 = -lightX * inverseLightY;
    shadow._22 = 0.0f;
    shadow._23 = -lightZ * inverseLightY;
    shadow._41 = groundY * lightX * inverseLightY;
    shadow._42 = groundY;
    shadow._43 = groundY * lightZ * inverseLightY;
    return D3DMath::Multiply(baseWorld, shadow);
}

D3DMATRIX BuildPlanarShadowWorld(const D3DMATRIX& baseWorld, const float localGroundY,
                                 const Context& context)
{
    return BuildPlanarShadowWorldAtY(
        baseWorld, localGroundY * baseWorld._22 + baseWorld._42, context);
}

bool IsGroundLikeShadowCaster(const noesisModel_t::Submesh& submesh)
{
    const float width = submesh.boundsMax[0] - submesh.boundsMin[0];
    const float height = submesh.boundsMax[1] - submesh.boundsMin[1];
    const float depth = submesh.boundsMax[2] - submesh.boundsMin[2];
    const float footprint = fmaxf(width, depth);

    // Decal shells and simple ground overlays are effectively flat even when
    // their authored vertices are not perfectly coplanar.
    if (height <= 0.08f && footprint >= 0.25f)
        return true;

    const std::vector<FFXIVertex>& vertices = !submesh.cpuVerts.empty()
        ? submesh.cpuVerts : submesh.cpuBindVerts;
    if (vertices.empty() || submesh.cpuIndices.size() < 3)
        return false;

    double totalArea = 0.0;
    double horizontalArea = 0.0;
    for (size_t i = 0; i + 2 < submesh.cpuIndices.size(); i += 3)
    {
        const DWORD ia = submesh.cpuIndices[i];
        const DWORD ib = submesh.cpuIndices[i + 1];
        const DWORD ic = submesh.cpuIndices[i + 2];
        if (ia >= vertices.size() || ib >= vertices.size() || ic >= vertices.size())
            continue;

        const float* const a = vertices[ia].pos;
        const float* const b = vertices[ib].pos;
        const float* const c = vertices[ic].pos;
        const float abX = b[0] - a[0], abY = b[1] - a[1], abZ = b[2] - a[2];
        const float acX = c[0] - a[0], acY = c[1] - a[1], acZ = c[2] - a[2];
        const float nx = abY * acZ - abZ * acY;
        const float ny = abZ * acX - abX * acZ;
        const float nz = abX * acY - abY * acX;
        const double area = sqrt(
            static_cast<double>(nx) * nx + static_cast<double>(ny) * ny +
            static_cast<double>(nz) * nz);
        totalArea += area;
        if (fabsf(ny) >= 1.5f * fmaxf(fabsf(nx), fabsf(nz)))
            horizontalArea += area;
    }

    if (totalArea <= 0.000001 || horizontalArea / totalArea < 0.82)
        return false;

    // Preserve compact props with broad tops, such as tables and planters.
    // Broad shallow meshes are terrain, roads, roofs used as ground, or decal
    // layers and must remain receivers only.
    return height <= 0.5f ||
        (footprint >= 6.0f && height <= footprint * 0.22f);
}

void ApplyPlanarShadowState(IDirect3DDevice9* device, const D3DMATRIX& shadowWorld,
                            const Context& context)
{
    device->SetFVF(FFXI_VERTEX_FVF);
    device->SetTransform(D3DTS_WORLD, &shadowWorld);
    device->SetPixelShader(nullptr);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_COLORVERTEX, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    device->SetRenderState(D3DRS_DEPTHBIAS,
        D3DModelRenderState::FloatBits(-0.00001f));
    device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetTexture(0, nullptr);
    device->SetTexture(1, nullptr);
    const DWORD alpha = static_cast<DWORD>(std::clamp(context.shadowOpacity, 0.0f, 1.0f) * 255.0f);
    device->SetRenderState(D3DRS_TEXTUREFACTOR, context.shadowDebugVisualization
        ? D3DCOLOR_ARGB(alpha, 220, 24, 24) : D3DCOLOR_ARGB(alpha, 0, 0, 0));
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
    device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
}

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
    device->SetTexture(1, d3dTexture);
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

    const float dx = baseWorld._41 - context.cameraPosition[0];
    const float dy = baseWorld._42 - context.cameraPosition[1];
    const float dz = baseWorld._43 - context.cameraPosition[2];
    if (context.shadowMaxDistance > 0.0f &&
        dx * dx + dy * dy + dz * dz > context.shadowMaxDistance * context.shadowMaxDistance)
        return;

    float localGroundY = 0.0f;
    bool hasGround = false;
    for (const noesisModel_t::Submesh& submesh : model->submeshes)
    {
        const std::vector<FFXIVertex>& vertices = !submesh.cpuVerts.empty()
            ? submesh.cpuVerts : submesh.cpuBindVerts;
        for (const FFXIVertex& vertex : vertices)
        {
            // DATura scene Y increases downward, so actor feet are at the
            // largest local Y rather than the smallest.
            if (!hasGround || vertex.pos[1] > localGroundY)
            {
                localGroundY = vertex.pos[1];
                hasGround = true;
            }
        }
    }
    if (!hasGround)
        return;

    const D3DMATRIX shadowWorld =
        BuildPlanarShadowWorld(baseWorld, localGroundY, context);
    if (shadowWorld._44 == 0.0f)
        return;

    ApplyPlanarShadowState(device, shadowWorld, context);
    for (const size_t index : model->opaqueSubmeshOrder)
        D3DModelBuffers::DrawSubmesh(device, model, model->submeshes[index]);
    device->SetTransform(D3DTS_WORLD, &baseWorld);
}

void DrawZoneObjectPlanarShadows(const Context& context, noesisModel_t* const model,
                                 const D3DMATRIX& baseWorld)
{
    IDirect3DDevice9* const device = context.device;
    if (!device || !model || !context.rendersZoneObjects ||
        !context.dynamicObjectShadows)
        return;

    const auto& renderVisibility = context.visibility;
    int drawnCasters = 0;
    int receiverQueries = 0;
    const unsigned long long passStart = GetTickCount64();
    static std::map<std::string, std::pair<unsigned long long, float>> receiverCache;
    const unsigned long long frame = GetTickCount64() / 16ULL;
    for (const size_t index : model->opaqueSubmeshOrder)
    {
        const noesisModel_t::Submesh& submesh = model->submeshes[index];
        if (!D3DModelBuffers::HasDrawBuffers(model, submesh) ||
            submesh.objectName.empty() ||
            submesh.environmentObject ||
            submesh.water ||
            !submesh.hasBounds ||
            (!context.shadowGroundLikeObjects && IsGroundLikeShadowCaster(submesh)))
        {
            continue;
        }
        if (!context.shadowAlphaTestedObjects && submesh.pResolvedTexture &&
            submesh.pResolvedTexture->texType == NOESISTEX_DXT3)
            continue;
        const float width = submesh.boundsMax[0] - submesh.boundsMin[0];
        const float height = submesh.boundsMax[1] - submesh.boundsMin[1];
        const float depth = submesh.boundsMax[2] - submesh.boundsMin[2];
        const float size = fmaxf(width, fmaxf(height, depth));
        if (size < context.shadowMinimumSize ||
            (context.shadowMaximumSize > 0.0f && size > context.shadowMaximumSize))
            continue;
        const float dx = submesh.boundsCenter[0] - context.cameraPosition[0];
        const float dy = submesh.boundsCenter[1] - context.cameraPosition[1];
        const float dz = submesh.boundsCenter[2] - context.cameraPosition[2];
        if (context.shadowMaxDistance > 0.0f &&
            dx * dx + dy * dy + dz * dz > context.shadowMaxDistance * context.shadowMaxDistance)
            continue;
        if (context.shadowObjectLimit >= 0 && drawnCasters >= context.shadowObjectLimit)
            break;
        if (!ZoneObjectVisibility::PassesRenderVisibility(
                model, submesh, context.rendersZoneObjects, renderVisibility))
        {
            continue;
        }

        D3DMATRIX objectWorld = baseWorld;
        if (renderVisibility.overrides)
        {
            const auto overrideIt = renderVisibility.overrides->find(submesh.objectName);
            if (overrideIt != renderVisibility.overrides->end())
                objectWorld = ZoneObjectTransform::BuildWorldMatrix(overrideIt->second);
        }

        if (!context.findZoneShadowReceiverY)
            continue;

        float receiverY = 0.0f;
        {
            float floorY = 0.0f;
            const float queryY = (submesh.boundsMin[1] + submesh.boundsMax[1]) * 0.5f;
            auto& cached = receiverCache[submesh.objectName];
            const unsigned long long interval = static_cast<unsigned long long>(
                (std::max)(1, context.shadowReceiverUpdateFrames));
            if (cached.first != 0 && frame - cached.first < interval)
                floorY = cached.second;
            else if (!context.findZoneShadowReceiverY(
                submesh.boundsCenter[0], queryY, submesh.boundsCenter[2], &floorY))
            {
                ++receiverQueries;
                continue;
            }
            else
            {
                ++receiverQueries;
                cached = { frame, floorY };
            }

            // Scene Y increases downward. If the collision lookup returns a
            // surface above the object's own bottom, it is usually a roof,
            // bridge, or stacked-floor false receiver; drawing there creates
            // the visible "floating sheet" artifact.
            if (floorY < submesh.boundsMax[1] - 0.25f)
                continue;
            receiverY = floorY;
        }

        const D3DMATRIX shadowWorld =
            BuildPlanarShadowWorldAtY(objectWorld, receiverY, context);
        if (shadowWorld._44 == 0.0f)
            continue;

        ApplyPlanarShadowState(device, shadowWorld, context);
        D3DModelBuffers::DrawSubmesh(device, model, submesh);
        ++drawnCasters;
    }
    if (context.shadowPerformanceCounters)
    {
        static unsigned long long lastReport = 0;
        const unsigned long long now = GetTickCount64();
        if (now - lastReport >= 1000)
        {
            char message[160] = {};
            sprintf_s(message, "DATura shadows: objects=%d receiverQueries=%d pass=%llums\n",
                drawnCasters, receiverQueries, now - passStart);
            OutputDebugStringA(message);
            lastReport = now;
        }
    }
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
        context.rendersZoneObjects && context.indoorZone,
        context.automaticLightDirection, context.lightAzimuthDegrees,
        context.lightElevationDegrees, context.useAuthoredLightColor,
        context.authoredMainLightColor, context.authoredSecondaryLightColor,
        context.authoredAmbientLightColor, context.authoredLightPower,
        context.useAuthoredLightDirection, context.authoredMainLightDirection);
    const bool fogEnabled = context.useAuthoredFog &&
        std::isfinite(context.authoredFogNear) && std::isfinite(context.authoredFogFar) &&
        context.authoredFogFar > context.authoredFogNear;
    context.device->SetRenderState(D3DRS_FOGENABLE, fogEnabled ? TRUE : FALSE);
    if (fogEnabled)
    {
        context.device->SetRenderState(D3DRS_FOGCOLOR, context.authoredFogColor);
        context.device->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_NONE);
        context.device->SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR);
        context.device->SetRenderState(
            D3DRS_FOGSTART, D3DModelRenderState::FloatBits(context.authoredFogNear));
        context.device->SetRenderState(
            D3DRS_FOGEND, D3DModelRenderState::FloatBits(context.authoredFogFar));
    }
    ApplyAuthoredPointLights(context, context.cameraPosition);
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
        context.device, cache, batch.pMaterial, batch.pTexture,
        context.enableBumpMapping && context.rendersZoneObjects,
        context.bumpMappingIntensity);
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
            if (batch.hasBounds)
            {
                const float localCenter[3] = {
                    (batch.boundsMin[0] + batch.boundsMax[0]) * 0.5f,
                    (batch.boundsMin[1] + batch.boundsMax[1]) * 0.5f,
                    (batch.boundsMin[2] + batch.boundsMax[2]) * 0.5f
                };
                float worldCenter[3] = {};
                TransformPoint(baseWorld, localCenter, worldCenter);
                ApplyAuthoredPointLights(context, worldCenter);
            }
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
            if (sm.hasBounds)
            {
                D3DMATRIX objectWorld = baseWorld;
                context.device->GetTransform(D3DTS_WORLD, &objectWorld);
                float worldCenter[3] = {};
                TransformPoint(objectWorld, sm.boundsCenter, worldCenter);
                ApplyAuthoredPointLights(context, worldCenter);
            }
            D3DModelRenderState::ApplyOpaqueMaterial(
                context.device, materialCache, sm.pResolvedMaterial, sm.pResolvedTexture,
                context.enableBumpMapping && context.rendersZoneObjects,
                context.bumpMappingIntensity);
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
        if (sm.hasBounds)
        {
            D3DMATRIX objectWorld = baseWorld;
            context.device->GetTransform(D3DTS_WORLD, &objectWorld);
            float worldCenter[3] = {};
            TransformPoint(objectWorld, sm.boundsCenter, worldCenter);
            ApplyAuthoredPointLights(context, worldCenter);
        }
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
                context.device->SetTexture(1, nullptr);
                context.device->SetPixelShader(nullptr);
                textureAnimation.Reset(context.device);
                previousWater = false;
            }
            D3DModelRenderState::ApplyTransparentMaterial(
                context.device, materialCache, sm.pResolvedMaterial, sm.pResolvedTexture,
                context.enableBumpMapping && context.rendersZoneObjects,
                context.bumpMappingIntensity);
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
        context.device->SetTexture(1, nullptr);
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
    device->SetTexture(1, nullptr);
    textureAnimation.Update(device, false, 0.0f, 0.0f);
    device->SetPixelShader(nullptr);
    device->SetTransform(D3DTS_WORLD, &baseWorld);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE2X);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE2X);
    device->SetStreamSource(0, nullptr, 0, 0);
    device->SetIndices(nullptr);
}
}
