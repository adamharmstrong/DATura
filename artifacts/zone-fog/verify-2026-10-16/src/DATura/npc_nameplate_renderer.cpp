#include "stdafx.h"
#include "d3d9_graphics.h"
#include "npc_nameplate_renderer.h"

#include "npc_render_geometry.h"
#include "ffxi_nameplate_font.h"
#include "ffxi_player_icons.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>

namespace
{
struct CachedTexture
{
    RendererBackend::Texture* texture = nullptr;
    int textWidth = 0;
    int textHeight = 0;
    int textureWidth = 0;
    int textureHeight = 0;
};

struct Vertex
{
    float x, y, z, rhw;
    DWORD color;
    float u, v;
};


constexpr float kFullSizeDistance = 12.0f;
constexpr float kMinimumDistanceScale = 0.25f;

using CacheKey = std::tuple<std::string, std::string, std::string, DWORD, DWORD, bool, bool, std::string, int, DWORD>;
std::map<CacheKey, CachedTexture> g_cachedTextures;

float DistanceScale(const float distanceSquared)
{
    if (!(distanceSquared > kFullSizeDistance * kFullSizeDistance))
        return 1.0f;

    const float distance = std::sqrt(distanceSquared);
    return std::max(kMinimumDistanceScale, kFullSizeDistance / distance);
}

std::wstring Utf8Text(const std::string& text)
{
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0);
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), result.data(), size);
    return result;
}

CachedTexture* GetCachedTexture(IDirect3DDevice9* device, const NpcNameplateRenderer::DrawItem& item)
{
    const auto& name = item.name;
    const auto& title = item.roleTitle;
    const auto* icon = FFXIPlayerIcons::Available(item.icon);
    const bool jobMaster = item.jobMaster && FFXIPlayerIcons::Available("linkshell");
    // This is an authored marker made from the retail punctuation glyph.
    // Suppress it if the official atlas is unavailable; never substitute Arial.
    const bool questMarker = item.starterQuestAvailable && FFXINameplateFont::Supports(L"!");
    const auto key = std::make_tuple(name, title, icon ? icon->id : "",
        DWORD(item.nameColor), icon && icon->tintable ? DWORD(item.iconColor) : 0xFFFFFFFF, jobMaster, questMarker, item.font, std::clamp(item.fontSize, 10, 32), DWORD(item.titleColor));
    const auto cached = g_cachedTextures.find(key);
    if (cached != g_cachedTextures.end())
        return &cached->second;

    CachedTexture result;
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc)
        return nullptr;
    const auto nameText = Utf8Text(name);
    const auto titleText = Utf8Text(title);
    const int nameHeight = std::clamp(item.fontSize, 10, 32);
    const int titleHeight = std::max(10, nameHeight * 13 / 16);
    const bool official = item.font.empty() || _stricmp(item.font.c_str(), "FFXI") == 0;
    const char* face = official ? "Arial" : item.font.c_str();
    const bool bitmapName = official && FFXINameplateFont::Supports(nameText);
    const bool bitmapTitle = official && FFXINameplateFont::Supports(titleText);
    HFONT font = bitmapName ? nullptr : CreateFontA(-nameHeight, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, face);
    HFONT titleFont = bitmapTitle ? nullptr : CreateFontA(-titleHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, face);
    HFONT oldFont = (HFONT)GetCurrentObject(dc, OBJ_FONT);
    SIZE textSize = FFXINameplateFont::Measure(nameText, nameHeight);
    SIZE titleSize = FFXINameplateFont::Measure(titleText, titleHeight);
    const int iconWidth = icon ? FFXIPlayerIcons::Width(*icon) + 2 : 0;
    constexpr int markerHeight = 56;
    const SIZE markerSize = questMarker ? FFXINameplateFont::Measure(L"!", markerHeight) : SIZE{};
    const int markerRow = questMarker ? markerHeight + 5 : 0;
    const int nameY = (jobMaster ? 13 : 3) + markerRow;
    if (!bitmapName)
    {
        SelectObject(dc, font);
        GetTextExtentPoint32W(dc, nameText.c_str(), (int)nameText.size(), &textSize);
    }
    if (!bitmapTitle && !titleText.empty())
    {
        SelectObject(dc, titleFont);
        GetTextExtentPoint32W(dc, titleText.c_str(), (int)titleText.size(), &titleSize);
    }
    const int titleY = nameY + textSize.cy + 2;
    result.textWidth = std::max(textSize.cx + iconWidth, titleSize.cx) + 6;
    if (jobMaster) result.textWidth = std::max(result.textWidth, 32);
    if (questMarker) result.textWidth = std::max(result.textWidth, int(markerSize.cx) + 6);
    result.textHeight = nameY + textSize.cy + 3 + (titleText.empty() ? 0 : titleSize.cy + 2);
    result.textureWidth = NpcRenderGeometry::NameplateTextureExtent(result.textWidth);
    result.textureHeight = NpcRenderGeometry::NameplateTextureExtent(result.textHeight);

    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = result.textureWidth;
    info.bmiHeader.biHeight = -result.textureHeight;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* dibBits = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &dibBits, nullptr, 0);
    HBITMAP oldBitmap = bitmap ? (HBITMAP)SelectObject(dc, bitmap) : nullptr;
    if (!bitmap || !dibBits)
    {
        if (oldBitmap) SelectObject(dc, oldBitmap);
        SelectObject(dc, oldFont);
        if (bitmap) DeleteObject(bitmap);
        DeleteObject(font);
        DeleteObject(titleFont);
        DeleteDC(dc);
        return nullptr;
    }

    const size_t pixelCount = (size_t)result.textureWidth * (size_t)result.textureHeight;
    std::vector<BYTE> outlineAlpha(pixelCount, 0);
    memset(dibBits, 0, pixelCount * sizeof(DWORD));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    const auto drawText = [&](int x, int y)
    {
        if (questMarker)
            FFXINameplateFont::Draw(dc, L"!", (result.textWidth - markerSize.cx) / 2 + x,
                2 + y, markerHeight, RGB(255, 255, 255));
        if (bitmapName)
            FFXINameplateFont::Draw(dc, nameText, (result.textWidth - textSize.cx - iconWidth) / 2 + iconWidth + x,
                nameY + y, nameHeight, RGB(255, 255, 255));
        else
        {
            SelectObject(dc, font);
            TextOutW(dc, (result.textWidth - textSize.cx - iconWidth) / 2 + iconWidth + x, nameY + y,
                nameText.c_str(), (int)nameText.size());
        }
        if (!titleText.empty())
        {
            if (bitmapTitle)
                FFXINameplateFont::Draw(dc, titleText, (result.textWidth - titleSize.cx) / 2 + x,
                    titleY + y, titleHeight, RGB(255, 255, 255));
            else
            {
                SelectObject(dc, titleFont);
                TextOutW(dc, (result.textWidth - titleSize.cx) / 2 + x, titleY + y,
                    titleText.c_str(), (int)titleText.size());
            }
        }
    };
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
            if (x != 0 || y != 0)
                drawText(x, y);
    GdiFlush();
    const DWORD* pixels = static_cast<const DWORD*>(dibBits);
    for (size_t i = 0; i < pixelCount; ++i)
        outlineAlpha[i] = (BYTE)(pixels[i] & 0xFF);

    memset(dibBits, 0, pixelCount * sizeof(DWORD));
    drawText(0, 0);
    GdiFlush();
    std::vector<DWORD> composed(pixelCount, 0);
    pixels = static_cast<const DWORD*>(dibBits);
    for (size_t i = 0; i < pixelCount; ++i)
    {
        const BYTE centerAlpha = (BYTE)(pixels[i] & 0xFF);
        const BYTE alpha = (centerAlpha > outlineAlpha[i]) ? centerAlpha : outlineAlpha[i];
        if (alpha == 0)
            continue;
        const float greenWeight = (float)centerAlpha / (float)alpha;
        const bool titlePixel = !titleText.empty() && i / result.textureWidth >= (size_t)(titleY - 1);
        const bool markerPixel = questMarker && i / result.textureWidth < size_t(markerRow);
        const DWORD color = markerPixel ? 0xFFFFD45A : titlePixel ? item.titleColor : item.nameColor;
        const BYTE red = (BYTE)(float((color >> 16) & 255) * greenWeight);
        const BYTE green = (BYTE)(float((color >> 8) & 255) * greenWeight);
        const BYTE blue = (BYTE)(float(color & 255) * greenWeight);
        composed[i] = D3DCOLOR_ARGB(alpha, red, green, blue);
    }

    if (icon)
        FFXIPlayerIcons::Composite(composed, result.textureWidth, result.textureHeight,
            (result.textWidth - textSize.cx - iconWidth) / 2, nameY, *icon, item.iconColor);
    if (jobMaster)
        FFXIPlayerIcons::CompositeJobMaster(composed, result.textureWidth, result.textureHeight,
            (result.textWidth - 26) / 2, 2 + markerRow);

    SelectObject(dc, oldBitmap);
    SelectObject(dc, oldFont);
    DeleteObject(bitmap);
    DeleteObject(font);
    DeleteObject(titleFont);
    DeleteDC(dc);

    D3D9Graphics::Context graphics(device);
    RendererBackend::TextureUpload upload;
    upload.width = result.textureWidth;
    upload.height = result.textureHeight;
    upload.data = composed.data();
    upload.dataSize = composed.size() * sizeof(DWORD);
    upload.generateMips = false;
    result.texture = graphics.CreateTexture(upload);
    if (!result.texture) return nullptr;
    const auto inserted = g_cachedTextures.emplace(key, result);
    return &inserted.first->second;
}
}

namespace NpcNameplateRenderer
{
void Draw(IDirect3DDevice9* device, std::vector<DrawItem>& items)
{
    if (!device || items.empty())
        return;

    // Scene materials (including weather and translucent surfaces) leave state
    // behind. A nameplate must not inherit their fog, winding, or texture UVs.
    RendererBackend::PipelineSnapshot* previousState = nullptr;
    if (!((previousState = D3D9Graphics::Context(device).CapturePipelineState())))
        return;

    std::sort(items.begin(), items.end(),
        [](const DrawItem& a, const DrawItem& b)
        {
            return a.distanceSquared > b.distanceSquared;
        });

    D3D9Graphics::Context(device).BindShader(RendererBackend::ShaderStage::Vertex, nullptr);
    D3D9Graphics::Context(device).BindShader(RendererBackend::ShaderStage::Pixel, nullptr);
    D3D9Graphics::Context(device).BindBuiltinVertexLayout(RendererBackend::BuiltinVertexLayout::ScreenColorTexture);
    D3D9Graphics::Context(device).SetLightingField(&RendererBackend::LightingState::enabled, false);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::fog, ((FALSE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::cull, RendererBackend::Cull::None);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::fill, RendererBackend::Fill::Solid);
    D3D9Graphics::Context(device).SetStencilField(&RendererBackend::StencilState::enabled, false);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::scissor, ((FALSE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::clipPlaneMask, uint32_t(0));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::colorWriteMask, uint32_t(0xF));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::srgbWrite, ((FALSE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthBias, RendererBackend::FloatFromBits(0));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::slopeDepthBias, RendererBackend::FloatFromBits(0));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthTest, ((TRUE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthWrite, ((FALSE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthCompare, RendererBackend::Compare::LessEqual);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::blend, ((TRUE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::blendOperation, RendererBackend::BlendOperation::Add);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::separateAlphaBlend, ((FALSE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::sourceBlend, RendererBackend::BlendFactor::SourceAlpha);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::destinationBlend, RendererBackend::BlendFactor::InverseSourceAlpha);
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::alphaTest, ((TRUE) != FALSE));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::alphaReference, uint32_t(4));
    D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::alphaCompare, RendererBackend::Compare::Greater);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::colorOperation, RendererBackend::TextureOperation::Modulate);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::colorArg1, RendererBackend::TextureArgument::Texture);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::colorArg2, RendererBackend::TextureArgument::Diffuse);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::alphaOperation, RendererBackend::TextureOperation::Modulate);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::alphaArg1, RendererBackend::TextureArgument::Texture);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::alphaArg2, RendererBackend::TextureArgument::Diffuse);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::coordinateIndex, uint32_t(0));
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::transform, RendererBackend::TextureTransform::Disabled);
    D3D9Graphics::Context(device).SetTextureStageField(0, &RendererBackend::TextureStageState::result, RendererBackend::TextureArgument::Current);
    D3D9Graphics::Context(device).SetTextureStageField(1, &RendererBackend::TextureStageState::colorOperation, RendererBackend::TextureOperation::Disable);
    D3D9Graphics::Context(device).SetTextureStageField(1, &RendererBackend::TextureStageState::alphaOperation, RendererBackend::TextureOperation::Disable);
    D3D9Graphics::Context(device).SetSamplerField(0, &RendererBackend::SamplerState::addressU, RendererBackend::TextureAddress::Clamp);
    D3D9Graphics::Context(device).SetSamplerField(0, &RendererBackend::SamplerState::addressV, RendererBackend::TextureAddress::Clamp);
    D3D9Graphics::Context(device).SetSamplerField(0, &RendererBackend::SamplerState::minFilter, RendererBackend::TextureFilter::Linear);
    D3D9Graphics::Context(device).SetSamplerField(0, &RendererBackend::SamplerState::magFilter, RendererBackend::TextureFilter::Linear);
    D3D9Graphics::Context(device).SetSamplerField(0, &RendererBackend::SamplerState::mipFilter, RendererBackend::TextureFilter::None);
    D3D9Graphics::Context(device).SetSamplerField(0, &RendererBackend::SamplerState::srgb, false);

    for (const DrawItem& item : items)
    {
        CachedTexture* label = GetCachedTexture(device, item);
        if (!label || !label->texture)
            continue;
        // Keep labels at their authored size nearby, then follow the same
        // inverse-distance relationship as perspective-projected geometry.
        // Scaling the cached quad preserves the one-texture-per-label cache and
        // avoids recreating fonts or textures as the camera moves.
        const float scale = DistanceScale(item.distanceSquared);
        const float width = label->textWidth * scale;
        const float height = label->textHeight * scale;
        const float left = item.screenX - width * 0.5f - 0.5f;
        const float top = item.screenY - height - 0.5f;
        const float right = left + width;
        const float bottom = top + height;
        const float u = (float)label->textWidth / (float)label->textureWidth;
        const float v = (float)label->textHeight / (float)label->textureHeight;
        const Vertex vertices[4] =
        {
            { left,  top,    item.depth, 1.0f, 0xFFFFFFFF, 0.0f, 0.0f },
            { right, top,    item.depth, 1.0f, 0xFFFFFFFF, u,    0.0f },
            { left,  bottom, item.depth, 1.0f, 0xFFFFFFFF, 0.0f, v },
            { right, bottom, item.depth, 1.0f, 0xFFFFFFFF, u,    v },
        };

        D3D9Graphics::Context(device).BindTexture(0, label->texture);
        D3D9Graphics::Context(device).DrawImmediate(RendererBackend::Primitive::TriangleStrip, 2, vertices, sizeof(Vertex));
    }
    previousState->Restore();
    delete previousState;
}

void ClearCachedTextures()
{
    for (auto& entry : g_cachedTextures)
    {
        if (entry.second.texture)
            delete entry.second.texture;
    }
    g_cachedTextures.clear();
}
}
