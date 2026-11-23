#include "stdafx.h"
#include "d3d9_graphics.h"
#include "zone_environment_render_state.h"

#include "noesis_rapi.h"
#include "model_ff11.h"
#include "d3d_math.h"
#include "d3d_model_buffers.h"
#include "d3d_model_render_state.h"
#include "zone_environment_animation.h"
#include "zone_environment_identity.h"
#include "zone_environment_state.h"
#include "zone_model_render_metadata.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ZoneEnvironmentRenderState
{
Data Build(const std::string &objectName, const int vanadielMinute,
           const std::vector<ff11GeneratorRecord_t> &generators,
           const std::vector<ff11KeyframeRecord_t> &keyframes)
{
    Data state;
    state.generator = ZoneEnvironmentAnimation::FindMeshGenerator(objectName, generators);
    if (!state.generator)
        return state;

    const ff11GeneratorRecord_t &generator = *state.generator;
    if (generator.hasColor)
    {
        constexpr float kAuthoredChannelScale = 1.0f / 128.0f;
        state.colorScale[0] = ((generator.colorBgra >> 16) & 0xff) * kAuthoredChannelScale;
        state.colorScale[1] = ((generator.colorBgra >> 8) & 0xff) * kAuthoredChannelScale;
        state.colorScale[2] = (generator.colorBgra & 0xff) * kAuthoredChannelScale;
        // Retail composes generator alpha as MODULATE2X followed by the
        // half-range vertex alpha through MODULATE4X. The shader already
        // expands vertex alpha by 2, leaving another factor of 2 here.
        state.opacity = ((generator.colorBgra >> 24) & 0xff) * (1.0f / 64.0f);
    }
    if (generator.hasBlendMode)
        state.blendMode = generator.blendMode;

    const float dayFraction = vanadielMinute / 1440.0f;
    state.colorScale[0] *= ZoneEnvironmentAnimation::EvaluateKeyframe(
        ZoneEnvironmentAnimation::FindKeyframe(generator, generator.redKeyframe, keyframes),
        dayFraction, 1.0f);
    state.colorScale[1] *= ZoneEnvironmentAnimation::EvaluateKeyframe(
        ZoneEnvironmentAnimation::FindKeyframe(generator, generator.greenKeyframe, keyframes),
        dayFraction, 1.0f);
    state.colorScale[2] *= ZoneEnvironmentAnimation::EvaluateKeyframe(
        ZoneEnvironmentAnimation::FindKeyframe(generator, generator.blueKeyframe, keyframes),
        dayFraction, 1.0f);
    state.opacity *= ZoneEnvironmentAnimation::EvaluateKeyframe(
        ZoneEnvironmentAnimation::FindKeyframe(generator, generator.alphaKeyframe, keyframes),
        dayFraction, 1.0f);
    state.opacity = std::clamp(state.opacity, 0.0f, 1.0f);
    state.uvScrollU = generator.hasUvScrollU ? generator.uvScrollU * 60.0f : 0.0f;
    state.uvScrollV = generator.hasUvScrollV ? generator.uvScrollV * 60.0f : 0.0f;
    if (generator.hasRotation)
        memcpy(state.rotation, generator.rotation, sizeof(state.rotation));
    if (generator.hasSpawnPosition)
        memcpy(state.rotationPivot, generator.spawnPosition, sizeof(state.rotationPivot));
    if (generator.hasRotationVelocity)
    {
        // Generator update rates are authored in the original 60 Hz element
        // clock, as are the UV scroll opcodes above. Accumulate the Euler angles
        // in that clock and reduce them before converting back to float.
        const double elapsedSeconds = GetTickCount64() * 0.001;
        for (int axis = 0; axis < 3; ++axis)
        {
            const double animatedAngle = elapsedSeconds * generator.rotationVelocity[axis] * 60.0;
            state.rotation[axis] += (float)fmod(animatedAngle, 2.0 * 3.14159265358979323846);
        }
    }
    return state;
}

bool IsCameraShell(const Data &state)
{
    if (!state.generator || !state.generator->hasStandardParticleSetup)
        return false;

    // CMoElem::SomeMatrixCalc only takes the translation-free camera path when
    // StandardParticleSetup field_10C has bit 2 (0x04). Clouds and star shells
    // use that path. Sun/moon "sunsphere" resources instead use the 0xC0
    // CalcRotAhead path plus generator attachment mode 14/15, which positions
    // and faces them from the zone's celestial vectors. Treating those resources
    // as camera-centred shells places their half-sphere boundary through the eye
    // and produces the bright vertical slab that looked like a cloud-dome seam.
    return (state.generator->standardParticleFlags & 0x04u) != 0;
}

bool IsCelestial(const Data &state)
{
    return state.generator && state.generator->hasStandardParticleSetup &&
        (state.generator->attachmentMode == 14 || state.generator->attachmentMode == 15);
}

D3DMATRIX BuildCelestialWorld(const Data &state, const int minuteOfDay,
                              const float cameraX, const float cameraY, const float cameraZ)
{
    // PS2 XiArea::SetSunMoonTime: main at 1000, inverse secondary at 900.
    // YmAttachments modes 14/15 translate to these positions. CalcRotAhead
    // mode 3 then points the mesh's local +X axis back toward the camera.
    const float angle = ((minuteOfDay % 1440 + 1440) % 1440) *
        (6.28318530718f / 1440.0f);
    const bool moon = state.generator && state.generator->attachmentMode == 15;
    // SetSunMoonTime builds secondary first, then adds pi for the main sun.
    const float sign = moon ? 1.0f : -1.0f;
    const float towardCameraX = sign * sinf(angle);
    const float towardCameraY = sign * cosf(angle);
    const float distance = moon ? 900.0f : 1000.0f;
    D3DMATRIX facing = D3DMath::BuildIdentity();
    facing._11 = towardCameraX;
    facing._12 = towardCameraY;
    facing._21 = -towardCameraY;
    facing._22 = towardCameraX;

    D3DMATRIX rotation = D3DMath::Multiply(
        D3DMath::BuildRotationZ(state.rotation[2]), D3DMath::BuildRotationY(state.rotation[1]));
    rotation = D3DMath::Multiply(rotation, D3DMath::BuildRotationX(state.rotation[0]));
    // Scale and spawn position are already baked into the loaded vertices.
    // Remove that position before facing, then restore it in attachment space.
    D3DMATRIX pivot = D3DMath::BuildIdentity();
    pivot._41 = -state.rotationPivot[0];
    pivot._42 = -state.rotationPivot[1];
    pivot._43 = -state.rotationPivot[2];
    D3DMATRIX world = D3DMath::Multiply(D3DMath::Multiply(pivot, rotation), facing);
    world._41 += cameraX - towardCameraX * distance + state.rotationPivot[0];
    world._42 += cameraY - towardCameraY * distance + state.rotationPivot[1];
    world._43 += cameraZ + state.rotationPivot[2];
    return world;
}

D3DMATRIX BuildCameraShellWorld(const Data &state, const float cameraX,
                                const float cameraY, const float cameraZ)
{
    // CMoElem::CalcMatrix composes generator Euler rotation as Rz * Ry * Rx.
    // The parsed MapGeo vertices already contain generator scale and spawn
    // translation, so rotate about that spawn point before attaching the shell
    // to the camera.
    D3DMATRIX rotation = D3DMath::Multiply(
        D3DMath::BuildRotationZ(state.rotation[2]), D3DMath::BuildRotationY(state.rotation[1]));
    rotation = D3DMath::Multiply(rotation, D3DMath::BuildRotationX(state.rotation[0]));

    D3DMATRIX toPivot = D3DMath::BuildIdentity();
    toPivot._41 = -state.rotationPivot[0];
    toPivot._42 = -state.rotationPivot[1];
    toPivot._43 = -state.rotationPivot[2];
    D3DMATRIX fromPivot = D3DMath::BuildIdentity();
    fromPivot._41 = state.rotationPivot[0] + cameraX;
    fromPivot._42 = state.rotationPivot[1] + cameraY;
    fromPivot._43 = state.rotationPivot[2] + cameraZ;
    return D3DMath::Multiply(D3DMath::Multiply(toPivot, rotation), fromPivot);
}

void ApplyBlendMode(IDirect3DDevice9 *device, const unsigned short blendMode)
{
    if (!device)
        return;

    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::blendOperation, RendererBackend::BlendOperation::Add);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::blend, ((TRUE) != FALSE));
    switch (blendMode & 0xff)
    {
    case 0x00:
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::sourceBlend, RendererBackend::BlendFactor::One);
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::destinationBlend, RendererBackend::BlendFactor::Zero);
        break;
    case 0x46:
    case 0x47:
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::sourceBlend, RendererBackend::BlendFactor::Zero);
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::destinationBlend, RendererBackend::BlendFactor::InverseSourceAlpha);
        break;
    case 0x48:
    case 0x49:
    case 0x68:
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::sourceBlend, RendererBackend::BlendFactor::SourceAlpha);
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::destinationBlend, RendererBackend::BlendFactor::One);
        break;
    case 0x03:
    case 0x44:
    case 0x64:
    default:
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::sourceBlend, RendererBackend::BlendFactor::SourceAlpha);
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::destinationBlend, RendererBackend::BlendFactor::InverseSourceAlpha);
        break;
    }
}

void DrawCameraShells(IDirect3DDevice9 *device, noesisModel_t *model,
                      const bool enableMipMapping, const char *weatherPath,
                      const std::vector<ff11GeneratorRecord_t> &generators,
                      const std::vector<ff11KeyframeRecord_t> &keyframes,
                      const float cameraX, const float cameraY, const float cameraZ,
                      const float opacity, const int minuteOfDay)
{
    if (!model || !device || opacity <= 0.001f)
        return;

    ZoneModelRenderMetadata::Prepare(model, device);

    D3DModelRenderState::ApplyFixedFunctionModelState(device, enableMipMapping);
    // Large sky texels make the original stipple conspicuous. Cloud textures
    // are uploaded with generated mip levels, and the sky pass always samples
    // them trilinearly regardless of the general model-viewer mip toggle.
    D3D9Graphics::Context(device).SetSamplerField(0, &RendererBackend::SamplerState::minFilter, RendererBackend::TextureFilter::Linear);
    D3D9Graphics::Context(device).SetSamplerField(0, &RendererBackend::SamplerState::magFilter, RendererBackend::TextureFilter::Linear);
    D3D9Graphics::Context(device).SetSamplerField(0, &RendererBackend::SamplerState::mipFilter, RendererBackend::TextureFilter::Linear);
    D3D9Graphics::Context(device).BindBuiltinVertexLayout(RendererBackend::BuiltinVertexLayout::Model);
    D3D9Graphics::Context(device).SetLightingField(&RendererBackend::LightingState::enabled, false);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::fog, ((FALSE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthTest, ((FALSE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthWrite, ((FALSE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::cull, RendererBackend::Cull::None);

    // Mesh motion comes from the generator's authored rotation and UV commands;
    // names and weather-directory membership only select the active resource set.
    D3DModelRenderState::SetTextureScroll(device, false);
    const int vanadielMinute = minuteOfDay >= 0 ? minuteOfDay : ZoneEnvironmentState::CurrentMinuteOfDay();
    for (noesisModel_t::Submesh &submesh : model->submeshes)
    {
        if (!submesh.environmentObject ||
            !ZoneEnvironmentIdentity::EnvironmentMeshMatchesWeather(submesh.objectName, weatherPath) ||
            !D3DModelBuffers::HasDrawBuffers(model, submesh))
        {
            continue;
        }
        const Data controller = Build(submesh.objectName, vanadielMinute, generators, keyframes);
        // Batched weather generators are camera-relative too, but they are not
        // static sky shells. Draw them after terrain so their authored planes
        // depth-test correctly and can receive their falling animation.
        if ((!IsCameraShell(controller) && !IsCelestial(controller)) ||
            (controller.generator->moreFlags & 0x20u) != 0 ||
            controller.opacity <= 0.001f)
            continue;
        if ((controller.generator->generatorFlags & 0x10u) == 0)
            continue;

        // Non-unit emission intervals are event-like overlays (not persistent
        // cloud shells). In particular, thdr/ligh is authored as a short flash;
        // drawing it continuously produces the yellow geometry fragments that
        // were mistaken for broken rain cards.
        if (controller.generator->framesPerEmission > 0)
        {
            const unsigned int intervalFrames =
                (unsigned int)controller.generator->framesPerEmission + 1u;
            const unsigned int frame =
                (unsigned int)((GetTickCount64() * 60ULL / 1000ULL) % intervalFrames);
            if (frame >= std::min(8u, intervalFrames))
                continue;
        }
        const D3DMATRIX world = IsCelestial(controller)
            ? BuildCelestialWorld(controller, vanadielMinute, cameraX, cameraY, cameraZ)
            : BuildCameraShellWorld(controller, cameraX, cameraY, cameraZ);
        D3D9Graphics::Context(device).SetTransform(RendererBackend::Transform::World, &world._11);

        IDirect3DTexture9 *texture = submesh.pResolvedTexture ? D3D9Graphics::NativeTexture(submesh.pResolvedTexture->gpuTexture) : nullptr;
        const bool expandDxt3Alpha = submesh.pResolvedTexture &&
            submesh.pResolvedTexture->texType == NOESISTEX_DXT3;

        // Generator-owned sky shells use the same translucent compositing path as
        // FFXI's zone overlays. Alpha testing turns their sparse star/cloud texels
        // into the black wedges seen when these meshes are treated as cutouts.
        ApplyBlendMode(device, controller.blendMode);
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::alphaTest, ((FALSE) != FALSE));
        D3DModelRenderState::SetTextureScroll(device,
            controller.uvScrollU != 0.0f || controller.uvScrollV != 0.0f,
            controller.uvScrollU, controller.uvScrollV);

        const bool shaderActive = D3DModelRenderState::SetFfxiTexturePixelShader(
            device, true, expandDxt3Alpha,
            controller.opacity * std::clamp(opacity, 0.0f, 1.0f), controller.colorScale,
            texture != nullptr);
        device->SetTexture(1, shaderActive ? texture : nullptr);
        D3DModelRenderState::SetTextureStageForOptionalTexture(
            device, texture, shaderActive ? D3DTOP_SELECTARG1 : D3DTOP_MODULATE4X);
        D3DModelBuffers::DrawSubmesh(device, model, submesh);
    }

    D3DModelRenderState::SetTextureScroll(device, false);
    D3D9Graphics::Context(device).BindShader(RendererBackend::ShaderStage::Pixel, nullptr);
    D3D9Graphics::Context(device).BindTexture(0, nullptr);
    D3D9Graphics::Context(device).BindTexture(1, nullptr);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthTest, ((TRUE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthWrite, ((TRUE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::blend, ((FALSE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::alphaTest, ((FALSE) != FALSE));
    device->SetStreamSource(0, nullptr, 0, 0);
    device->SetIndices(nullptr);
}
}
