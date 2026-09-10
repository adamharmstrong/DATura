#include "stdafx.h"
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
    IDirect3DTexture9* texture = nullptr;
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

constexpr DWORD kVertexFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;
constexpr float kFullSizeDistance = 12.0f;
constexpr float kMinimumDistanceScale = 0.25f;

using CacheKey = std::tuple<std::string, std::string, std::string, DWORD, DWORD, bool, bool>;
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
        DWORD(item.nameColor), icon && icon->tintable ? DWORD(item.iconColor) : 0xFFFFFFFF, jobMaster, questMarker);
    const auto cached = g_cachedTextures.find(key);
    if (cached != g_cachedTextures.end())
        return &cached->second;

    CachedTexture result;
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc)
        return nullptr;
    const auto nameText = Utf8Text(name);
    const auto titleText = Utf8Text(title);
    const bool bitmapName = FFXINameplateFont::Supports(nameText);
    const bool bitmapTitle = FFXINameplateFont::Supports(titleText);
    HFONT font = bitmapName ? nullptr : CreateFontA(-16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, "Arial");
    HFONT titleFont = bitmapTitle ? nullptr : CreateFontA(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, "Arial");
    HFONT oldFont = (HFONT)GetCurrentObject(dc, OBJ_FONT);
    SIZE textSize = FFXINameplateFont::Measure(nameText, 16);
    SIZE titleSize = FFXINameplateFont::Measure(titleText, 13);
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
                nameY + y, 16, RGB(255, 255, 255));
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
                    titleY + y, 13, RGB(255, 255, 255));
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
        const DWORD color = markerPixel ? 0xFFFFD45A : titlePixel ? 0xFFD2DCE6 : item.nameColor;
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

    if (!device || FAILED(device->CreateTexture(result.textureWidth, result.textureHeight, 1, 0,
                                                  D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                                                  &result.texture, nullptr)))
    {
        return nullptr;
    }
    D3DLOCKED_RECT locked = {};
    if (FAILED(result.texture->LockRect(0, &locked, nullptr, 0)))
    {
        result.texture->Release();
        return nullptr;
    }
    for (int y = 0; y < result.textureHeight; ++y)
        memcpy((BYTE*)locked.pBits + y * locked.Pitch,
               &composed[(size_t)y * result.textureWidth],
               (size_t)result.textureWidth * sizeof(DWORD));
    result.texture->UnlockRect(0);
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
    IDirect3DStateBlock9* previousState = nullptr;
    if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &previousState)))
        return;

    std::sort(items.begin(), items.end(),
        [](const DrawItem& a, const DrawItem& b)
        {
            return a.distanceSquared > b.distanceSquared;
        });

    device->SetVertexShader(nullptr);
    device->SetPixelShader(nullptr);
    device->SetFVF(kVertexFvf);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
    device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
    device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_DEPTHBIAS, 0);
    device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);
    device->SetRenderState(D3DRS_ZENABLE, TRUE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
    device->SetRenderState(D3DRS_ALPHAREF, 4);
    device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
    device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    device->SetTextureStageState(0, D3DTSS_RESULTARG, D3DTA_CURRENT);
    device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);

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

        device->SetTexture(0, label->texture);
        device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(Vertex));
    }
    previousState->Apply();
    previousState->Release();
}

void ClearCachedTextures()
{
    for (auto& entry : g_cachedTextures)
    {
        if (entry.second.texture)
            entry.second.texture->Release();
    }
    g_cachedTextures.clear();
}
}
