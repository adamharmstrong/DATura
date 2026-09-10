#include "stdafx.h"
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

    device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    switch (blendMode & 0xff)
    {
    case 0x00:
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
        break;
    case 0x46:
    case 0x47:
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        break;
    case 0x48:
    case 0x49:
    case 0x68:
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
        break;
    case 0x03:
    case 0x44:
    case 0x64:
    default:
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        break;
    }
}

void DrawCameraShells(IDirect3DDevice9 *device, noesisModel_t *model,
                      const bool enableMipMapping, const char *weatherPath,
                      const std::vector<ff11GeneratorRecord_t> &generators,
                      const std::vector<ff11KeyframeRecord_t> &keyframes,
                      const float cameraX, const float cameraY, const float cameraZ)
{
    if (!model || !device)
        return;

    ZoneModelRenderMetadata::Prepare(model, device);

    D3DModelRenderState::ApplyFixedFunctionModelState(device, enableMipMapping);
    // Large sky texels make the original stipple conspicuous. Cloud textures
    // are uploaded with generated mip levels, and the sky pass always samples
    // them trilinearly regardless of the general model-viewer mip toggle.
    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
    device->SetFVF(FFXI_VERTEX_FVF);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_ZENABLE, FALSE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

    // Mesh motion comes from the generator's authored rotation and UV commands;
    // names and weather-directory membership only select the active resource set.
    D3DModelRenderState::SetTextureScroll(device, false);
    const int vanadielMinute = ZoneEnvironmentState::CurrentMinuteOfDay();
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
        if (!IsCameraShell(controller) ||
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
        const D3DMATRIX world = BuildCameraShellWorld(controller, cameraX, cameraY, cameraZ);
        device->SetTransform(D3DTS_WORLD, &world);

        IDirect3DTexture9 *texture = submesh.pResolvedTexture ? submesh.pResolvedTexture->pD3DTex : nullptr;
        const bool expandDxt3Alpha = submesh.pResolvedTexture &&
            submesh.pResolvedTexture->texType == NOESISTEX_DXT3;

        // Generator-owned sky shells use the same translucent compositing path as
        // FFXI's zone overlays. Alpha testing turns their sparse star/cloud texels
        // into the black wedges seen when these meshes are treated as cutouts.
        ApplyBlendMode(device, controller.blendMode);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        D3DModelRenderState::SetTextureScroll(device,
            controller.uvScrollU != 0.0f || controller.uvScrollV != 0.0f,
            controller.uvScrollU, controller.uvScrollV);

        const bool shaderActive = texture && D3DModelRenderState::SetFfxiTexturePixelShader(
            device, true, expandDxt3Alpha, controller.opacity, controller.colorScale);
        D3DModelRenderState::SetTextureStageForOptionalTexture(
            device, texture, shaderActive ? D3DTOP_SELECTARG1 : D3DTOP_MODULATE4X);
        D3DModelBuffers::DrawSubmesh(device, model, submesh);
    }

    D3DModelRenderState::SetTextureScroll(device, false);
    device->SetPixelShader(nullptr);
    device->SetTexture(0, nullptr);
    device->SetRenderState(D3DRS_ZENABLE, TRUE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetStreamSource(0, nullptr, 0, 0);
    device->SetIndices(nullptr);
}
}
