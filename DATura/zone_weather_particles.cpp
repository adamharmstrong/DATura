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
struct Vertex
{
    float x, y, z;
    DWORD color;
};

constexpr DWORD kVertexFormat = D3DFVF_XYZ | D3DFVF_DIFFUSE;

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

const ff11GeneratorRecord_t *FindActiveGenerator(
    const char *weatherPath, const std::vector<ff11GeneratorRecord_t> &generators)
{
    if (!weatherPath || !weatherPath[0])
        return nullptr;

    const size_t weatherPathLength = strlen(weatherPath);
    const ff11GeneratorRecord_t *best = nullptr;
    for (const ff11GeneratorRecord_t &generator : generators)
    {
        // 0x20 is a batching flag, not a unique weather identifier (footstep
        // generators use it too). Requiring the active weat/<tag> directory is
        // what makes this an authored weather-particle generator.
        if ((generator.moreFlags & 0x20) == 0 ||
            (generator.generatorFlags & 0x10) == 0 ||
            generator.particlesPerEmission == 0 ||
            strncmp(generator.directoryPath, weatherPath, weatherPathLength) != 0 ||
            generator.directoryPath[weatherPathLength] != '/')
        {
            continue;
        }
        if (!best || generator.particlesPerEmission > best->particlesPerEmission)
            best = &generator;
    }
    return best;
}

bool DrawAuthoredBatchedWeather(
    IDirect3DDevice9 *device, noesisModel_t *model, const bool enableMipMapping,
    const char *weatherPath, const std::vector<ff11GeneratorRecord_t> &generators,
    const std::vector<ff11KeyframeRecord_t> &keyframes,
    const float cameraX, const float cameraY, const float cameraZ)
{
    const ff11GeneratorRecord_t *generator = FindActiveGenerator(weatherPath, generators);
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
    const unsigned int currentFrame =
        (unsigned int)(GetTickCount64() * 60ULL / 1000ULL);
    const float emissionPhase = (float)(currentFrame % emissionFrames);

    D3DModelRenderState::ApplyFixedFunctionModelState(device, enableMipMapping);
    device->SetFVF(FFXI_VERTEX_FVF);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_ZENABLE, TRUE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);

    const int vanadielMinute = ZoneEnvironmentState::CurrentMinuteOfDay();
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
                const float displacement = velocity * ageFrames +
                    0.5f * acceleration * ageFrames * (ageFrames - 1.0f);
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
}

void Draw(IDirect3DDevice9 *device, noesisModel_t *model, const bool environmentValid,
          const bool enableMipMapping, const char *weatherPath,
          const std::vector<ff11GeneratorRecord_t> &generators,
          const std::vector<ff11KeyframeRecord_t> &keyframes,
          const float cameraX, const float cameraY, const float cameraZ)
{
    if (!device || !environmentValid)
        return;

    const std::string weather = weatherPath ? weatherPath : "";
    const bool rain = ZoneEnvironmentIdentity::ContainsLowerToken(weather, "rain") ||
                      ZoneEnvironmentIdentity::ContainsLowerToken(weather, "squl") ||
                      ZoneEnvironmentIdentity::ContainsLowerToken(weather, "storm");
    const bool snow = ZoneEnvironmentIdentity::ContainsLowerToken(weather, "snow") ||
                      ZoneEnvironmentIdentity::ContainsLowerToken(weather, "bliz");
    const bool sand = ZoneEnvironmentIdentity::ContainsLowerToken(weather, "sand") ||
                      ZoneEnvironmentIdentity::ContainsLowerToken(weather, "dust");
    if (!rain && !snow && !sand)
        return;

    // Rain is never synthesized here. If the active DAT does not provide an
    // authored batched rain field, there is no precipitation draw for it.
    if (rain)
    {
        DrawAuthoredBatchedWeather(
            device, model, enableMipMapping, weatherPath, generators, keyframes,
            cameraX, cameraY, cameraZ);
        return;
    }

    const ff11GeneratorRecord_t *authoredGenerator = FindActiveGenerator(weatherPath, generators);
    const int particleCount = authoredGenerator ?
        std::clamp((int)authoredGenerator->particlesPerEmission, 1, 1000) :
        420;
    const float time = (float)(GetTickCount64() % 600000ULL) * 0.001f;
    const float fallSpeed = snow ? 4.0f : 8.0f;
    const float streak = snow ? 0.35f : 0.8f;
    const DWORD color = snow ? D3DCOLOR_ARGB(185, 245, 248, 255) :
        D3DCOLOR_ARGB(105, 211, 174, 105);
    std::vector<Vertex> vertices;
    vertices.reserve((size_t)particleCount * 6);
    for (int index = 0; index < particleCount; ++index)
    {
        const float rx = Hash((unsigned int)index * 3u + 1u);
        const float ry = Hash((unsigned int)index * 3u + 2u);
        const float rz = Hash((unsigned int)index * 3u + 3u);
        const float extent = 55.0f;
        const float x = cameraX + (rx * 2.0f - 1.0f) * extent;
        const float z = cameraZ + (rz * 2.0f - 1.0f) * extent;
        // FFXI gravity points toward +Y. Start most particles above the eye
        // (negative Y), advance them toward +Y, and wrap below the camera.
        const float y = cameraY - 60.0f + fmodf(ry * 85.0f + time * fallSpeed, 85.0f);
        const float drift = snow ? sinf(time * 0.7f + index) * 0.7f :
                            (sand ? streak : 0.25f);
        const Vertex topCenter = { x, y, z, color };
        const Vertex bottomCenter = sand ?
            Vertex{ x + streak, y + 0.15f, z + drift, color } :
            Vertex{ x + drift, y + streak, z, color };

        // Build a narrow vertical card facing the eye. The old LINELIST path
        // discarded the authored plane behavior and rasterized inconsistently
        // across resolutions and drivers.
        float viewX = cameraX - x;
        float viewZ = cameraZ - z;
        const float viewLength = sqrtf(viewX * viewX + viewZ * viewZ);
        if (viewLength > 0.0001f)
        {
            viewX /= viewLength;
            viewZ /= viewLength;
        }
        else
        {
            viewX = 0.0f;
            viewZ = 1.0f;
        }
        const float halfWidth = snow ? 0.16f : 0.12f;
        const float rightX = viewZ * halfWidth;
        const float rightZ = -viewX * halfWidth;
        const Vertex topLeft =
            { topCenter.x - rightX, topCenter.y, topCenter.z - rightZ, color };
        const Vertex topRight =
            { topCenter.x + rightX, topCenter.y, topCenter.z + rightZ, color };
        const Vertex bottomLeft =
            { bottomCenter.x - rightX, bottomCenter.y, bottomCenter.z - rightZ, color };
        const Vertex bottomRight =
            { bottomCenter.x + rightX, bottomCenter.y, bottomCenter.z + rightZ, color };
        vertices.push_back(topLeft);
        vertices.push_back(topRight);
        vertices.push_back(bottomRight);
        vertices.push_back(topLeft);
        vertices.push_back(bottomRight);
        vertices.push_back(bottomLeft);
    }

    const D3DMATRIX identity = D3DMath::BuildIdentity();
    device->SetTransform(D3DTS_WORLD, &identity);
    device->SetFVF(kVertexFormat);
    device->SetTexture(0, nullptr);
    device->SetPixelShader(nullptr);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_ZENABLE, TRUE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, (UINT)particleCount * 2,
                            vertices.data(), sizeof(Vertex));
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
}
}
