#include "stdafx.h"
#include "npc_nameplate_renderer.h"

#include "npc_render_geometry.h"

#include <algorithm>
#include <map>

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

std::map<std::string, CachedTexture> g_cachedTextures;

CachedTexture* GetCachedTexture(IDirect3DDevice9* device, const std::string& name)
{
    const auto cached = g_cachedTextures.find(name);
    if (cached != g_cachedTextures.end())
        return &cached->second;

    CachedTexture result;
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc)
        return nullptr;
    HFONT font = CreateFontA(-16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, "Arial");
    HFONT oldFont = (HFONT)SelectObject(dc, font);
    SIZE textSize = {};
    GetTextExtentPoint32A(dc, name.c_str(), (int)name.size(), &textSize);
    result.textWidth = textSize.cx + 6;
    result.textHeight = textSize.cy + 6;
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
        DeleteDC(dc);
        return nullptr;
    }

    const size_t pixelCount = (size_t)result.textureWidth * (size_t)result.textureHeight;
    std::vector<BYTE> outlineAlpha(pixelCount, 0);
    memset(dibBits, 0, pixelCount * sizeof(DWORD));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
            if (x != 0 || y != 0)
                TextOutA(dc, 3 + x, 3 + y, name.c_str(), (int)name.size());
    const DWORD* pixels = static_cast<const DWORD*>(dibBits);
    for (size_t i = 0; i < pixelCount; ++i)
        outlineAlpha[i] = (BYTE)(pixels[i] & 0xFF);

    memset(dibBits, 0, pixelCount * sizeof(DWORD));
    TextOutA(dc, 3, 3, name.c_str(), (int)name.size());
    std::vector<DWORD> composed(pixelCount, 0);
    pixels = static_cast<const DWORD*>(dibBits);
    for (size_t i = 0; i < pixelCount; ++i)
    {
        const BYTE centerAlpha = (BYTE)(pixels[i] & 0xFF);
        const BYTE alpha = (centerAlpha > outlineAlpha[i]) ? centerAlpha : outlineAlpha[i];
        if (alpha == 0)
            continue;
        const float greenWeight = (float)centerAlpha / (float)alpha;
        const BYTE red = (BYTE)(164.0f * greenWeight);
        const BYTE green = (BYTE)(255.0f * greenWeight);
        const BYTE blue = (BYTE)(164.0f * greenWeight);
        composed[i] = D3DCOLOR_ARGB(alpha, red, green, blue);
    }

    SelectObject(dc, oldBitmap);
    SelectObject(dc, oldFont);
    DeleteObject(bitmap);
    DeleteObject(font);
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
    const auto inserted = g_cachedTextures.emplace(name, result);
    return &inserted.first->second;
}
}

namespace NpcNameplateRenderer
{
void Draw(IDirect3DDevice9* device, std::vector<DrawItem>& items)
{
    if (!device || items.empty())
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
    device->SetRenderState(D3DRS_ZENABLE, TRUE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
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
    device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

    for (const DrawItem& item : items)
    {
        CachedTexture* label = GetCachedTexture(device, item.name);
        if (!label || !label->texture)
            continue;
        const float left = item.screenX - label->textWidth * 0.5f - 0.5f;
        const float top = item.screenY - label->textHeight - 0.5f;
        const float right = left + label->textWidth;
        const float bottom = top + label->textHeight;
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
    device->SetTexture(0, nullptr);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
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
