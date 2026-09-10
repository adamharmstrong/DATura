#include "stdafx.h"
#include "zone_weather_particles.h"

#include "noesis_rapi.h"
#include "model_ff11.h"
#include "d3d_math.h"
#include "d3d_model_buffers.h"
#include "d3d_model_render_state.h"
#include "zone_environment_render_state.h"
#include "zone_environment_identity.h"
#include "zone_environment_state.h"
#include "zone_environment_animation.h"
#include "zone_model_render_metadata.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace ZoneWeatherParticles
{
namespace
{
float Hash(const unsigned int valueInput)
{
    unsigned int value = valueInput;
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return (float)(value & 0xffffu) / 65535.0f;
}

bool Active(const ff11GeneratorRecord_t &g, const char *weatherPath)
{
    return weatherPath && weatherPath[0] && (g.moreFlags & 0x20) &&
        (g.generatorFlags & 0x10) && g.particlesPerEmission &&
        ZoneEnvironmentIdentity::EnvironmentWeatherRootsMatch(g.directoryPath, weatherPath);
}

bool DrawAuthoredBatchedWeather(
    IDirect3DDevice9 *device, noesisModel_t *model, const bool enableMipMapping,
    const char *weatherPath, const std::vector<ff11GeneratorRecord_t> &generators,
    const std::vector<ff11KeyframeRecord_t> &keyframes,
    const float cameraX, const float cameraY, const float cameraZ,
    const ff11GeneratorRecord_t *generator, double seconds, int vanadielMinute)
{
    if (!device || !model || !generator || !generator->linkedResource[0])
        return false;

    ZoneModelRenderMetadata::Prepare(model, device);
    std::vector<noesisModel_t::Submesh *> batches;
    for (noesisModel_t::Submesh &submesh : model->submeshes)
    {
        std::string directoryPath;
        std::string resourceName;
        std::string generatorName;
        if (!submesh.environmentObject ||
            !ZoneEnvironmentIdentity::EnvironmentMeshMatchesWeather(submesh.objectName, weatherPath) ||
            !ZoneEnvironmentIdentity::ParseEnvironmentMeshIdentity(
                submesh.objectName, directoryPath, resourceName, generatorName) ||
            !ZoneEnvironmentIdentity::EnvironmentNamesMatch(generatorName.c_str(), generator->name) ||
            !ZoneEnvironmentIdentity::EnvironmentNamesMatch(resourceName.c_str(), generator->linkedResource) ||
            !D3DModelBuffers::HasDrawBuffers(model, submesh))
        {
            continue;
        }
        batches.push_back(&submesh);
    }
    if (batches.empty())
        return false;

    // Batched precipitation emits one complete authored card field per cycle.
    // The header and updater stream provide the interval, lifetime, initial
    // velocity, and per-frame acceleration in FFXI's native Y-down space.
    const unsigned int emissionFrames =
        (unsigned int)generator->framesPerEmission + 1u;
    const unsigned int lifetimeFrames = generator->particleLifetimeFrames > 0 ?
        (unsigned int)generator->particleLifetimeFrames : emissionFrames;
    // Retain sub-frame time so rendering above 60 Hz does not repeat positions.
    // Reduce in double precision before converting to float for long uptimes.
    const double currentFrame = seconds * 60.0;
    const float emissionPhase = (float)fmod(currentFrame, (double)emissionFrames);

    D3DModelRenderState::ApplyFixedFunctionModelState(device, enableMipMapping);
    device->SetFVF(FFXI_VERTEX_FVF);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_ZENABLE, TRUE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);

    for (noesisModel_t::Submesh *submesh : batches)
    {
        const ZoneEnvironmentRenderState::Data controller =
            ZoneEnvironmentRenderState::Build(
                submesh->objectName, vanadielMinute, generators, keyframes);
        if (!controller.generator || controller.opacity <= 0.001f)
            continue;

        IDirect3DTexture9 *texture = submesh->pResolvedTexture ?
            submesh->pResolvedTexture->pD3DTex : nullptr;
        const bool expandDxt3Alpha = submesh->pResolvedTexture &&
            submesh->pResolvedTexture->texType == NOESISTEX_DXT3;
        ZoneEnvironmentRenderState::ApplyBlendMode(device, controller.blendMode);
        D3DModelRenderState::SetTextureScroll(device,
            controller.uvScrollU != 0.0f || controller.uvScrollV != 0.0f,
            controller.uvScrollU, controller.uvScrollV);
        const bool shaderActive = texture && D3DModelRenderState::SetFfxiTexturePixelShader(
            device, true, expandDxt3Alpha, controller.opacity, controller.colorScale);
        D3DModelRenderState::SetTextureStageForOptionalTexture(
            device, texture, shaderActive ? D3DTOP_SELECTARG1 : D3DTOP_MODULATE4X);

        const D3DMATRIX baseWorld = ZoneEnvironmentRenderState::BuildCameraShellWorld(
            controller, cameraX, cameraY, cameraZ);
        for (float ageFrames = emissionPhase; ageFrames < (float)lifetimeFrames;
             ageFrames += (float)emissionFrames)
        {
            D3DMATRIX world = baseWorld;
            for (int axis = 0; axis < 3; ++axis)
            {
                const float velocity = generator->hasLinearVelocity ?
                    generator->linearVelocity[axis] : 0.0f;
                const float acceleration = generator->hasLinearAcceleration ?
                    generator->linearAcceleration[axis] : 0.0f;
                // The updater stream applies position-from-velocity before it
                // adds acceleration to velocity each frame (opcodes 0x02,
                // then 0x03). Reproduce that discrete update order exactly.
                const float wholeFrames = floorf(ageFrames);
                const float fraction = ageFrames - wholeFrames;
                const float displacement = velocity * ageFrames + acceleration *
                    (0.5f * wholeFrames * (wholeFrames - 1.0f) + fraction * wholeFrames);
                (&world._41)[axis] += displacement;
            }
            device->SetTransform(D3DTS_WORLD, &world);
            D3DModelBuffers::DrawSubmesh(device, model, *submesh);
        }
    }

    D3DModelRenderState::SetTextureScroll(device, false);
    device->SetPixelShader(nullptr);
    device->SetTexture(0, nullptr);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    device->SetStreamSource(0, nullptr, 0, 0);
    device->SetIndices(nullptr);
    return true;
}

void DrawSprites(IDirect3DDevice9 *device, noesisModel_t *model,
                 const ff11GeneratorRecord_t &g, const std::vector<ff11KeyframeRecord_t> &curves,
                 double seconds, int minute, const float camera[3])
{
    const noesisModel_t::WeatherSprite *sprite = nullptr;
    for (const auto &candidate : model->weatherSprites)
    {
        if (candidate.resourceName != g.linkedResource) continue;
        if (candidate.directoryPath == g.directoryPath) { sprite = &candidate; break; }
        if (ZoneEnvironmentIdentity::EnvironmentWeatherRootsMatch(candidate.directoryPath.c_str(), g.directoryPath))
            sprite = &candidate;
        else if (!sprite && !Model_FF11_IsWeatherDirectory(candidate.directoryPath.c_str())) sprite = &candidate;
    }
    if (!sprite || sprite->vertices.size() < 6 || !model->pMatData) return;
    noesisTex_t *texture = nullptr;
    for (int i = 0; i < model->pMatData->texCount; ++i)
        if (model->pMatData->textures[i] && model->pMatData->textures[i]->name &&
            sprite->textureName == model->pMatData->textures[i]->name)
            texture = model->pMatData->textures[i];
    if (!texture || !texture->pD3DTex) return;
    float origin[3] = {};
    float distanceSquared = 0;
    for (int axis = 0; axis < 3; ++axis)
    {
        origin[axis] = g.hasSpawnPosition ? g.spawnPosition[axis] : 0;
        if (g.standardParticleFlags & 4) origin[axis] += camera[axis];
        distanceSquared += (origin[axis]-camera[axis])*(origin[axis]-camera[axis]);
    }
    if (g.hasCullDistance && g.cullDistance > 0 && distanceSquared > g.cullDistance*g.cullDistance) return;
    auto curve = [&](const char *name, float t, float fallback) {
        return ZoneEnvironmentAnimation::EvaluateKeyframe(
            ZoneEnvironmentAnimation::FindKeyframe(g, name, curves), t, fallback);
    };
    float colors[3] = {1,1,1};
    const char *tracks[3] = {g.redKeyframe, g.greenKeyframe, g.blueKeyframe};
    for (int axis = 0; axis < 3; ++axis)
        colors[axis] = curve(tracks[axis], minute / 1440.0f,
            g.hasColor ? ((g.colorBgra >> (16-axis*8)) & 255) / 128.0f : 1.0f);
    const float dayAlpha = curve(g.alphaKeyframe, minute / 1440.0f, 1);
    const double interval = g.framesPerEmission + 1.0;
    const double life = g.particleLifetimeFrames ? g.particleLifetimeFrames : interval;
    const double frame = seconds * 60;
    const auto cycle = static_cast<unsigned long long>(floor(frame / interval));
    const double phase = fmod(frame, interval);
    D3DMATRIX view;
    device->GetTransform(D3DTS_VIEW, &view);
    const float right[3] = {view._11, view._21, view._31};
    const float down[3] = {-view._12, -view._22, -view._32};
    struct Particle { float center[3]; float alpha; float depth; size_t spriteOffset; };
    std::vector<Particle> particles;
    for (unsigned int generation = 0; generation * interval + phase < life && generation < 1024; ++generation)
    {
        const float age = static_cast<float>(generation * interval + phase);
        const float alpha = g.updateLifetimeAlpha ? curve(g.lifetimeAlphaKeyframe, age / (float)life,
            g.hasColor ? ((g.colorBgra >> 24) & 255) / 128.0f : 1.0f) :
            (g.hasColor ? ((g.colorBgra >> 24) & 255) / 128.0f : 1.0f);
        if (!std::isfinite(alpha) || alpha * dayAlpha <= 0.001f) continue;
        for (unsigned int i = 0; i < g.particlesPerEmission && particles.size() < 32768; ++i)
        {
            const unsigned int seed = g.sourceDataOffset ^ (unsigned int)(cycle-generation)*747796405u ^ i*2891336453u;
            const float radius = g.hasPositionVariance ? g.spawnRadius * Hash(seed+1) : 0;
            const float yaw = (Hash(seed+2)*2-1)*3.14159265f;
            const float pitch = (Hash(seed+3)*2-1)*3.14159265f;
            const float direction[3] = {cosf(pitch)*cosf(yaw), sinf(pitch), cosf(pitch)*sinf(yaw)};
            Particle particle = {};
            particle.alpha = std::clamp(alpha * dayAlpha, 0.0f, 1.0f);
            const size_t frames = sprite->vertices.size()/6;
            particle.spriteOffset = g.animateSprite ?
                std::min(frames-1, static_cast<size_t>((frames+1)*age/life))*6 : 0;
            for (int axis = 0; axis < 3; ++axis)
            {
                const float speed = (g.hasLinearVelocity ? g.linearVelocity[axis] : 0) +
                    (g.hasVelocityVariance ? (Hash(seed+4+axis)*2-1)*g.velocityVariance[axis] : 0);
                const float whole = floorf(age), fraction = age-whole;
                particle.center[axis] = origin[axis] + radius*direction[axis]*g.spawnAxisScale[axis] + speed*age +
                    (g.hasLinearAcceleration ? g.linearAcceleration[axis]*(whole*(whole-1)*0.5f+fraction*whole) : 0);
            }
            particle.depth = particle.center[0]*view._13 + particle.center[1]*view._23 + particle.center[2]*view._33 + view._43;
            if (g.hasParticleDistanceFade && g.particleFadeFar > g.particleFadeNear)
            {
                float distance = 0;
                for (int axis = 0; axis < 3; ++axis)
                    distance += (particle.center[axis]-camera[axis])*(particle.center[axis]-camera[axis]);
                particle.alpha *= std::clamp((g.particleFadeFar-sqrtf(distance))/
                    (g.particleFadeFar-g.particleFadeNear), 0.0f, 1.0f);
            }
            if (particle.depth > 0) particles.push_back(particle);
        }
    }
    std::stable_sort(particles.begin(), particles.end(), [](const Particle &a, const Particle &b) {return a.depth > b.depth;});
    std::vector<FFXIVertex> vertices;
    for (const auto &particle : particles)
        for (int i = 0; i < 6; ++i)
        {
            auto v = sprite->vertices[particle.spriteOffset+i];
            const float x = v.pos[0]*(g.hasScale ? g.scale[0] : 1);
            const float y = v.pos[1]*(g.hasScale ? g.scale[1] : 1);
            const float angle = g.hasRotation ? g.rotation[2] : 0;
            const float rotatedX = x*cosf(angle)-y*sinf(angle);
            const float rotatedY = x*sinf(angle)+y*cosf(angle);
            for (int axis = 0; axis < 3; ++axis) v.pos[axis] = particle.center[axis]+right[axis]*rotatedX+down[axis]*rotatedY;
            const DWORD alpha = static_cast<DWORD>(((v.diffuse >> 24) & 255)*particle.alpha);
            v.diffuse = (v.diffuse & 0xffffff) | (alpha << 24);
            vertices.push_back(v);
        }
    if (vertices.empty()) return;
    D3DModelRenderState::ApplyFixedFunctionModelState(device, true);
    const auto identity = D3DMath::BuildIdentity();
    device->SetTransform(D3DTS_WORLD, &identity);
    device->SetFVF(FFXI_VERTEX_FVF);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_ZENABLE, TRUE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    ZoneEnvironmentRenderState::ApplyBlendMode(device, g.hasBlendMode ? g.blendMode : 0x44);
    const bool shader = D3DModelRenderState::SetFfxiTexturePixelShader(device, true, texture->texType == NOESISTEX_DXT3, 1, colors);
    D3DModelRenderState::SetTextureStageForOptionalTexture(device, texture->pD3DTex, shader ? D3DTOP_SELECTARG1 : D3DTOP_MODULATE2X);
    device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, (UINT)vertices.size()/3, vertices.data(), sizeof(FFXIVertex));
}
}

void DrawAtTime(IDirect3DDevice9 *device, noesisModel_t *model, bool environmentValid,
          bool enableMipMapping, const char *weatherPath,
          const std::vector<ff11GeneratorRecord_t> &generators,
          const std::vector<ff11KeyframeRecord_t> &keyframes,
          float cameraX, float cameraY, float cameraZ, double seconds, int minute)
{
    if (!device || !model || !environmentValid || !std::isfinite(seconds) || seconds < 0) return;
    IDirect3DStateBlock9 *saved = nullptr;
    if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &saved))) return;
    const float camera[3] = {cameraX,cameraY,cameraZ};
    for (const auto &g : generators)
    {
        if (!Active(g, weatherPath)) continue;
        // Some 0x0E resources are complete rain card fields already instantiated
        // as environment meshes. Keep that path before trying a sprite atlas.
        const bool hasMeshBatch = DrawAuthoredBatchedWeather(device, model, enableMipMapping, weatherPath, generators,
            keyframes, cameraX, cameraY, cameraZ, &g, seconds, minute);
        if (!hasMeshBatch && g.linkedDataType == 0x0e)
            DrawSprites(device, model, g, keyframes, seconds, minute, camera);
    }
    saved->Apply();
    saved->Release();
}

void Draw(IDirect3DDevice9 *device, noesisModel_t *model, bool environmentValid,
          bool enableMipMapping, const char *weatherPath,
          const std::vector<ff11GeneratorRecord_t> &generators,
          const std::vector<ff11KeyframeRecord_t> &keyframes,
          float cameraX, float cameraY, float cameraZ)
{
    DrawAtTime(device, model, environmentValid, enableMipMapping, weatherPath, generators,
        keyframes, cameraX, cameraY, cameraZ, GetTickCount64()*0.001, ZoneEnvironmentState::CurrentMinuteOfDay());
}
}
