#pragma once

#include "ffxi_dat_set_builder.h"
#include "ffxi_model_lifetime.h"

struct IDirect3DDevice9;

namespace PlayerModelLoader
{
enum class Error
{
    None,
    InvalidRequest,
    IncompleteRaceEntry,
    NoDisplayableGeometry,
};

struct Request
{
    const char* ffxiRoot = nullptr;
    FFXIDatSet::PlayerOptions customization;
    bool enableTextureCompression = false;
    float footContactAdjustment = 0.212f;
};

struct Result
{
    FFXIModelLifetime::OwnedModel asset;
    Error error = Error::None;
    int modelCount = 0;
    float groundOffset = 0.0f;
    float cameraTargetLocalY = -2.0f;

    bool Succeeded() const noexcept;
};

Result Load(IDirect3DDevice9* device, const Request& request);
}
