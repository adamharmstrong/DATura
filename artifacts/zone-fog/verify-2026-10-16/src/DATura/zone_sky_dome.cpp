#include "stdafx.h"
#include "d3d9_graphics.h"
#include "zone_sky_dome.h"

#include "d3d_math.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ZoneSkyDome
{
namespace
{
struct Vertex
{
    float x, y, z;
    DWORD color;
};


}

void Draw(IDirect3DDevice9 *device, const Parameters &parameters,
          const float cameraX, const float cameraY, const float cameraZ, const float farPlane)
{
    if (!device || !parameters.environmentValid || parameters.indoor || parameters.ringCount < 2 ||
        !parameters.ringElevations || !parameters.ringColors)
    {
        return;
    }

    const int spokes = std::clamp(parameters.spokeCount > 0 ? parameters.spokeCount : 16, 3, 64);
    std::vector<Vertex> vertices;
    vertices.reserve((size_t)(parameters.ringCount - 1) * spokes * 6);
    // Keep the camera-attached dome safely inside the active projection plane.
    const float radius = std::min(parameters.radius, farPlane * 0.9f);
    for (int ring = 0; ring + 1 < parameters.ringCount; ++ring)
    {
        float phi0 = 0.5f * 3.14159265f * parameters.ringElevations[ring];
        float phi1 = 0.5f * 3.14159265f * parameters.ringElevations[ring + 1];
        phi0 = std::clamp(phi0, -0.2f, 1.5707963f);
        phi1 = std::clamp(phi1, -0.2f, 1.5707963f);
        for (int spoke = 0; spoke < spokes; ++spoke)
        {
            const float a0 = 2.0f * 3.14159265f * spoke / spokes;
            const float a1 = 2.0f * 3.14159265f * (spoke + 1) / spokes;
            const Vertex v00 = { cameraX + radius * cosf(phi0) * cosf(a0),
                cameraY - radius * sinf(phi0), cameraZ + radius * cosf(phi0) * sinf(a0),
                parameters.ringColors[ring] };
            const Vertex v01 = { cameraX + radius * cosf(phi0) * cosf(a1),
                cameraY - radius * sinf(phi0), cameraZ + radius * cosf(phi0) * sinf(a1),
                parameters.ringColors[ring] };
            const Vertex v10 = { cameraX + radius * cosf(phi1) * cosf(a0),
                cameraY - radius * sinf(phi1), cameraZ + radius * cosf(phi1) * sinf(a0),
                parameters.ringColors[ring + 1] };
            const Vertex v11 = { cameraX + radius * cosf(phi1) * cosf(a1),
                cameraY - radius * sinf(phi1), cameraZ + radius * cosf(phi1) * sinf(a1),
                parameters.ringColors[ring + 1] };
            vertices.push_back(v00); vertices.push_back(v10); vertices.push_back(v11);
            vertices.push_back(v00); vertices.push_back(v11); vertices.push_back(v01);
        }
    }

    const D3DMATRIX identity = D3DMath::BuildIdentity();
    D3D9Graphics::Context(device).SetTransform(RendererBackend::Transform::World, &identity._11);
    D3D9Graphics::Context(device).BindShader(RendererBackend::ShaderStage::Vertex, nullptr);
    D3D9Graphics::Context(device).BindBuiltinVertexLayout(RendererBackend::BuiltinVertexLayout::PositionColor);
    D3D9Graphics::Context(device).BindTexture(0, nullptr);
    D3D9Graphics::Context(device).BindTexture(1, nullptr);
    D3D9Graphics::Context(device).BindShader(RendererBackend::ShaderStage::Pixel, nullptr);
    D3D9Graphics::Context(device).SetLightingField(&RendererBackend::LightingState::enabled, false);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::fog, ((FALSE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthTest, ((FALSE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthWrite, ((FALSE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::blend, ((FALSE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::alphaTest, ((FALSE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::cull, RendererBackend::Cull::None);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::colorOperation, RendererBackend::TextureOperation::SelectArg1);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::colorArg1, RendererBackend::TextureArgument::Diffuse);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::alphaOperation, RendererBackend::TextureOperation::Disable);
    D3D9Graphics::Context(device).SetTextureStageField(1, &RendererBackend::TextureStageState::colorOperation, RendererBackend::TextureOperation::Disable);
    D3D9Graphics::Context(device).SetTextureStageField(1, &RendererBackend::TextureStageState::alphaOperation, RendererBackend::TextureOperation::Disable);
    D3D9Graphics::Context(device).DrawImmediate(RendererBackend::Primitive::TriangleList, (UINT)(vertices.size() / 3),
                            vertices.data(), sizeof(Vertex));
}
}
