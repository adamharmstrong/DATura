#pragma once

#include <d3d9.h>
#include <string>

struct noesisTex_t;

namespace CustomTextureAssets
{
struct Texture
{
    IDirect3DTexture9* texture = nullptr;
    int width = 0;
    int height = 0;
};

void Configure(IDirect3DDevice9* device, bool enableHdTextures,
               const std::string& hdFolder, bool enablePbr,
               const std::string& pbrFolder);
Texture ResolveBase(const noesisTex_t* original);
bool SupportsPbr(IDirect3DDevice9* device);
bool ApplyPbrShader(IDirect3DDevice9* device, const noesisTex_t* original,
                    IDirect3DTexture9* baseTexture, bool useAuthoredAlpha,
                    bool expandDxt3Alpha, float opacityScale);
void Release();
}
