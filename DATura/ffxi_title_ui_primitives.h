#pragma once

struct IDirect3DDevice9;
struct noesisTex_t;

namespace FFXITitleUiPrimitives
{
void DrawButton(IDirect3DDevice9* device, bool enableMipMapping,
                noesisTex_t* texture, float x, float y, float width, float height,
                bool hover);
void DrawExpansionAtlas(IDirect3DDevice9* device, bool enableMipMapping,
                        noesisTex_t* texture, int rows, int startRow,
                        float x, float y, float width, float height, float gap);
}
