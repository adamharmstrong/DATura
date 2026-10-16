#include "stdafx.h"
#include "d3d9_graphics.h"
#include "world_item_renderer.h"
#include "d3d_math.h"

#include <vector>

namespace
{
struct Vertex { float x, y, z; DWORD color; };


DWORD BaseColor(const InventorySystem::ItemDefinition* item)
{
    if (!item) return D3DCOLOR_XRGB(220, 80, 220);
    using InventorySystem::ItemCategory;
    switch (item->category)
    {
    case ItemCategory::Crystal: return D3DCOLOR_XRGB(110, 205, 255);
    case ItemCategory::Medicine: return D3DCOLOR_XRGB(220, 75, 85);
    case ItemCategory::Food: return D3DCOLOR_XRGB(220, 170, 75);
    case ItemCategory::Weapon: return D3DCOLOR_XRGB(185, 195, 210);
    case ItemCategory::Armor: return D3DCOLOR_XRGB(115, 145, 205);
    case ItemCategory::Quest: return D3DCOLOR_XRGB(235, 210, 80);
    default: return D3DCOLOR_XRGB(170, 135, 85);
    }
}

DWORD Shade(const DWORD color, const float amount)
{
    const int r = static_cast<int>(((color >> 16) & 0xff) * amount);
    const int g = static_cast<int>(((color >> 8) & 0xff) * amount);
    const int b = static_cast<int>((color & 0xff) * amount);
    return D3DCOLOR_XRGB(r, g, b);
}

void AddFace(std::vector<Vertex>& vertices, const float p[8][3],
    const int a, const int b, const int c, const int d, const DWORD color)
{
    vertices.push_back({p[a][0], p[a][1], p[a][2], color});
    vertices.push_back({p[b][0], p[b][1], p[b][2], color});
    vertices.push_back({p[c][0], p[c][1], p[c][2], color});
    vertices.push_back({p[a][0], p[a][1], p[a][2], color});
    vertices.push_back({p[c][0], p[c][1], p[c][2], color});
    vertices.push_back({p[d][0], p[d][1], p[d][2], color});
}
}

namespace WorldItemRenderer
{
void Draw(IDirect3DDevice9* device, const WorldItemSystem::State& state,
    const InventorySystem::ItemCatalog& catalog, const int zoneId)
{
    if (!device || zoneId < 0) return;
    std::vector<Vertex> vertices;
    vertices.reserve(WorldItemSystem::CountInZone(state, zoneId) * 36);
    constexpr float half = 0.25f;
    constexpr float height = 0.5f;
    for (const auto& entity : state.entities)
    {
        if (entity.zoneId != zoneId) continue;
        const float x = entity.position[0], y = entity.position[1], z = entity.position[2];
        const float p[8][3] = {
            {x-half,y,z-half}, {x+half,y,z-half}, {x+half,y,z+half}, {x-half,y,z+half},
            {x-half,y-height,z-half}, {x+half,y-height,z-half},
            {x+half,y-height,z+half}, {x-half,y-height,z+half}
        };
        const DWORD base = BaseColor(catalog.Find(entity.stack.itemId));
        AddFace(vertices, p, 4, 7, 6, 5, Shade(base, 1.0f));
        AddFace(vertices, p, 0, 1, 2, 3, Shade(base, 0.55f));
        AddFace(vertices, p, 0, 4, 5, 1, Shade(base, 0.78f));
        AddFace(vertices, p, 1, 5, 6, 2, Shade(base, 0.88f));
        AddFace(vertices, p, 2, 6, 7, 3, Shade(base, 0.68f));
        AddFace(vertices, p, 3, 7, 4, 0, Shade(base, 0.82f));
    }
    if (vertices.empty()) return;

    RendererBackend::PipelineSnapshot* saved = nullptr;
    saved = D3D9Graphics::Context(device).CapturePipelineState();
    const D3DMATRIX identity = D3DMath::BuildIdentity();
    D3D9Graphics::Context(device).SetTransform(RendererBackend::Transform::World, &identity._11);
    D3D9Graphics::Context(device).BindShader(RendererBackend::ShaderStage::Vertex, nullptr);
    D3D9Graphics::Context(device).BindShader(RendererBackend::ShaderStage::Pixel, nullptr);
    D3D9Graphics::Context(device).BindBuiltinVertexLayout(RendererBackend::BuiltinVertexLayout::PositionColor);
    D3D9Graphics::Context(device).BindTexture(0, nullptr);
    D3D9Graphics::Context(device).BindTexture(1, nullptr);
    D3D9Graphics::Context(device).SetLightingField(&RendererBackend::LightingState::enabled, false);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthTest, ((TRUE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthWrite, ((TRUE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::blend, ((FALSE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::alphaTest, ((FALSE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::cull, RendererBackend::Cull::None);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::colorOperation, RendererBackend::TextureOperation::SelectArg1);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::colorArg1, RendererBackend::TextureArgument::Diffuse);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::alphaOperation, RendererBackend::TextureOperation::Disable);
    D3D9Graphics::Context(device).SetTextureStageField(1, &RendererBackend::TextureStageState::colorOperation, RendererBackend::TextureOperation::Disable);
    D3D9Graphics::Context(device).DrawImmediate(RendererBackend::Primitive::TriangleList,
        static_cast<UINT>(vertices.size() / 3), vertices.data(), sizeof(Vertex));
    if (saved) { saved->Restore(); delete saved; }
}
}
