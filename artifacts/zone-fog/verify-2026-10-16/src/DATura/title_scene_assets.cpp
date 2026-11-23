#include "stdafx.h"
#include "title_scene_assets.h"

#include "ffxi_title_assets.h"
#include "noesis_rapi.h"

namespace
{
void LoadAsset(
    FFXIModelLifetime::OwnedModel& destination,
    IDirect3DDevice9* device, const char* ffxiRoot,
    const char* relativePath, const bool enableTextureCompression)
{
    noeRAPI_t* parserContext = nullptr;
    noesisModel_t* model = FFXITitleAssets::LoadTextureDat(
        device, ffxiRoot, relativePath, enableTextureCompression, &parserContext);
    destination.Adopt(model, parserContext);
}

void SetTextureCompression(
    FFXIModelLifetime::OwnedModel& asset, const bool enabled) noexcept
{
    if (asset.ParserContext())
        asset.ParserContext()->SetTextureCompressionEnabled(enabled);
}
}

namespace TitleSceneAssets
{
void State::Reset() noexcept
{
    logo_.Reset();
    atlas_.Reset();
    ui_.Reset();
}

void State::LoadAll(
    IDirect3DDevice9* device, const char* ffxiRoot,
    const Paths& paths, const bool enableTextureCompression)
{
    LoadAsset(logo_, device, ffxiRoot, paths.logoDat, enableTextureCompression);
    LoadAsset(atlas_, device, ffxiRoot, paths.atlasDat, enableTextureCompression);
    LoadAsset(ui_, device, ffxiRoot, paths.uiDat, enableTextureCompression);
}

bool State::LoadUi(
    IDirect3DDevice9* device, const char* ffxiRoot,
    const char* relativePath, const bool enableTextureCompression)
{
    if (!HasUi())
        LoadAsset(ui_, device, ffxiRoot, relativePath, enableTextureCompression);
    return HasUi();
}

void State::SetTextureCompressionEnabled(const bool enabled) noexcept
{
    SetTextureCompression(logo_, enabled);
    SetTextureCompression(atlas_, enabled);
    SetTextureCompression(ui_, enabled);
}

noesisModel_t* State::LogoModel() const noexcept
{
    return logo_.Model();
}

noesisModel_t* State::AtlasModel() const noexcept
{
    return atlas_.Model();
}

noesisModel_t* State::UiModel() const noexcept
{
    return ui_.Model();
}

bool State::HasUi() const noexcept
{
    return static_cast<bool>(ui_) && ui_.ParserContext();
}
}
