#pragma once

#include "ffxi_model_lifetime.h"

struct IDirect3DDevice9;
struct noesisModel_t;

namespace TitleSceneAssets
{
struct Paths
{
    const char* logoDat = nullptr;
    const char* atlasDat = nullptr;
    const char* uiDat = nullptr;
};

class State final
{
public:
    void Reset() noexcept;
    void LoadAll(
        IDirect3DDevice9* device, const char* ffxiRoot,
        const Paths& paths, bool enableTextureCompression);
    bool LoadUi(
        IDirect3DDevice9* device, const char* ffxiRoot,
        const char* relativePath, bool enableTextureCompression);
    void SetTextureCompressionEnabled(bool enabled) noexcept;

    noesisModel_t* LogoModel() const noexcept;
    noesisModel_t* AtlasModel() const noexcept;
    noesisModel_t* UiModel() const noexcept;
    bool HasUi() const noexcept;

private:
    FFXIModelLifetime::OwnedModel logo_;
    FFXIModelLifetime::OwnedModel atlas_;
    FFXIModelLifetime::OwnedModel ui_;
};
}
