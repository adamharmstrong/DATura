#pragma once

#include "ffxi_model_lifetime.h"

struct IDirect3DDevice9;

namespace CreationModelLoader
{
enum class Error
{
    None,
    InvalidRequest,
    MeshFileOpenFailed,
    NoMeshPaths,
    NoDisplayableGeometry,
};

struct Request
{
    const char* ffxiRoot = nullptr;
    const char* label = nullptr;
    const char* bodyMeshDat = nullptr;
    const char* bodyMaterialDat = nullptr;
    const char* headMeshDat = nullptr;
    const char* headMaterialDat = nullptr;
    const char* bodyAnimationDat = nullptr;
    const char* headAnimationDat = nullptr;
    int headAlphaMode = 0;
    float headYOffset = 0.0f;
    bool enableTextureCompression = false;
};

struct Result
{
    FFXIModelLifetime::OwnedModel asset;
    Error error = Error::None;
    int modelCount = 0;
    bool skeletonInspectionPerformed = false;

    bool Succeeded() const noexcept;
};

Result Load(IDirect3DDevice9* device, const Request& request);
}
