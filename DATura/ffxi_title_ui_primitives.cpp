#include "stdafx.h"
#include "ffxi_title_ui_primitives.h"

#include "d3d_ui_renderer.h"
#include "noesis_rapi.h"

namespace FFXITitleUiPrimitives
{
void DrawButton(IDirect3DDevice9* device, const bool enableMipMapping,
                noesisTex_t* texture, const float x, const float y,
                const float width, const float height, const bool hover)
{
    if (!texture || !texture->pD3DTex || texture->w <= 0 || texture->h <= 0)
        return;

    // Compose the title button from atlas layers. The circular button is used as
    // the rounded end cap; the long button body supplies the center fill.
    const float rowY = hover ? 32.0f : 0.0f;
    const float rowH = 16.0f;
    const float circleX = 0.0f;
    const float circleW = 16.0f;
    const float bodyX = 16.0f;
    const float bodyW = 48.0f;
    const float bodyEndInset = 8.0f;

    const float v0 = rowY / (float)texture->h;
    const float v1 = (rowY + rowH) / (float)texture->h;

    const float capOuterU0 = circleX / (float)texture->w;
    const float capMidU = (circleX + circleW * 0.5f) / (float)texture->w;
    const float bodyU0 = (bodyX + bodyEndInset) / (float)texture->w;
    const float bodyU1 = (bodyX + bodyW - bodyEndInset) / (float)texture->w;

    float destinationCapWidth = height * 0.5f;
    if (destinationCapWidth * 2.0f > width)
        destinationCapWidth = width * 0.5f;

    const DWORD color = hover ? 0xFFFFB060 : 0xFFFFFFFF;
    const bool expandDxt3Alpha = D3DUiRenderer::UsesFfxiDxt3Alpha(texture);

    // Left cap uses the left half of the circular source.
    D3DUiRenderer::DrawTexturedQuadUV(
        device, enableMipMapping, texture->pD3DTex, x, y, destinationCapWidth, height,
        capOuterU0, v0, capMidU, v1, color, expandDxt3Alpha);

    // Center stretches only the flat middle of the long source button.
    D3DUiRenderer::DrawTexturedQuadUV(
        device, enableMipMapping, texture->pD3DTex,
        x + destinationCapWidth, y, width - destinationCapWidth * 2.0f, height,
        bodyU0, v0, bodyU1, v1, color, expandDxt3Alpha);

    // Right cap mirrors the rounded outer half of the circular source.
    D3DUiRenderer::DrawTexturedQuadUV(
        device, enableMipMapping, texture->pD3DTex,
        x + width - destinationCapWidth, y, destinationCapWidth, height,
        capMidU, v0, capOuterU0, v1, color, expandDxt3Alpha);
}

void DrawExpansionAtlas(IDirect3DDevice9* device, const bool enableMipMapping,
                        noesisTex_t* texture, const int rows, const int startRow,
                        const float x, const float y, const float width,
                        const float height, const float gap)
{
    if (!texture || rows <= 0)
        return;

    const float sourceRowHeight = (float)texture->h / (float)rows;
    for (int row = 0; row < rows; ++row)
    {
        D3DUiRenderer::DrawTextureRegion(
            device, enableMipMapping, texture,
            x, y + (height + gap) * (float)(startRow + row), width, height,
            0.0f, sourceRowHeight * (float)row, (float)texture->w,
            sourceRowHeight, 0xFFFFFFFF);
    }
}
}
