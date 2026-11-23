#pragma once

struct noesisModel_t;
struct noesisTex_t;
class noeRAPI_t;
struct IDirect3DDevice9;

namespace FFXITitleAssets
{
noesisTex_t* FindTitleTexture(noesisModel_t* model, const char* needle);
noesisModel_t* LoadTextureDat(IDirect3DDevice9 *device, const char *ffxiRoot,
                              const char *relativePath, bool enableTextureCompression,
                              noeRAPI_t **outRapi);
}
