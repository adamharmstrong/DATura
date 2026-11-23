#pragma once

#include "ffxi_model_lifetime.h"

struct IDirect3DDevice9;

namespace SceneModelLoader
{
enum class ContentKind
{
    Unknown,
    StandardDat,
    CreationDat,
    CreationMaterialDat,
    SqleAnimationDat,
    DatSet,
};

enum class Error
{
    None,
    InvalidRequest,
    FileOpenFailed,
    FileEmptyOrUnreadable,
    FileReadFailed,
    UnrecognizedDat,
    StandardDatHasNoGeometry,
    CreationDatHasNoGeometry,
    CreationMaterialOnly,
    SqleAnimationOnly,
    UnrecognizedDatSet,
    DatSetHasNoGeometry,
};

struct DatOptions
{
    bool enableTextureCompression = false;
    bool userContentLoad = true;
    bool renderEnvironment = false;
    bool renderUnreferenced = false;
};

struct Result
{
    FFXIModelLifetime::OwnedModel asset;
    ContentKind kind = ContentKind::Unknown;
    Error error = Error::None;
    int modelCount = 0;

    bool Succeeded() const noexcept;
};

Result LoadDat(IDirect3DDevice9* device, const char* path, const DatOptions& options);
Result LoadDatSet(
    IDirect3DDevice9* device, const char* path, bool enableTextureCompression);
}
