#include "stdafx.h"
#include "scene_model_loader.h"

#include "ffxi_file_io.h"
#include "noesis_rapi.h"
#include "model_ff11.h"
#include "zone_room_loader.h"

#include <cstring>
#include <memory>

namespace
{
SceneModelLoader::Error MapReadError(const FFXIFileIO::ReadResult result)
{
    switch (result)
    {
    case FFXIFileIO::ReadResult::OpenFailed:
        return SceneModelLoader::Error::FileOpenFailed;
    case FFXIFileIO::ReadResult::EmptyOrUnreadable:
        return SceneModelLoader::Error::FileEmptyOrUnreadable;
    case FFXIFileIO::ReadResult::ReadFailed:
        return SceneModelLoader::Error::FileReadFailed;
    case FFXIFileIO::ReadResult::InvalidArguments:
    default:
        return SceneModelLoader::Error::InvalidRequest;
    }
}

class ScopedParserOptions final
{
public:
    explicit ScopedParserOptions(ff11Opts_t* options) noexcept
        : previous_(gpFF11Opts), active_(options != nullptr)
    {
        if (active_)
            gpFF11Opts = options;
    }

    ~ScopedParserOptions()
    {
        if (active_)
            gpFF11Opts = previous_;
    }

    ScopedParserOptions(const ScopedParserOptions&) = delete;
    ScopedParserOptions& operator=(const ScopedParserOptions&) = delete;

private:
    ff11Opts_t* previous_ = nullptr;
    bool active_ = false;
};

void BeginAsset(
    SceneModelLoader::Result& result, IDirect3DDevice9* device,
    const char* path, const bool enableTextureCompression)
{
    result.asset.Adopt(nullptr, new noeRAPI_t(device));
    result.asset.ParserContext()->SetTextureCompressionEnabled(enableTextureCompression);
    result.asset.ParserContext()->SetCurrentFilePath(path);
}

void SetFailure(SceneModelLoader::Result& result, const SceneModelLoader::Error error)
{
    result.asset.Reset();
    result.error = error;
    result.modelCount = 0;
}
}

namespace SceneModelLoader
{
bool Result::Succeeded() const noexcept
{
    return error == Error::None && static_cast<bool>(asset);
}

Result LoadDat(IDirect3DDevice9* device, const char* path, const DatOptions& options)
{
    Result result;
    if (!device || !path || !path[0])
    {
        result.error = Error::InvalidRequest;
        return result;
    }

    BYTE* rawBuffer = nullptr;
    DWORD fileSize = 0;
    FFXIFileIO::ReadResult readResult = FFXIFileIO::ReadResult::InvalidArguments;
    if (!FFXIFileIO::ReadWholeFile(path, &rawBuffer, &fileSize, &readResult))
    {
        result.error = MapReadError(readResult);
        return result;
    }
    std::unique_ptr<BYTE[]> buffer(rawBuffer);

    BeginAsset(result, device, path, options.enableTextureCompression);
    if (Model_FF11_CheckCreationDAT(
            buffer.get(), static_cast<int>(fileSize), result.asset.ParserContext()))
    {
        result.kind = ContentKind::CreationDat;
        noesisModel_t* model = Model_FF11_LoadCreationDAT(
            buffer.get(), static_cast<int>(fileSize), result.modelCount,
            result.asset.ParserContext());
        if (!model || result.modelCount == 0)
        {
            SetFailure(result, Error::CreationDatHasNoGeometry);
            return result;
        }

        result.asset.AttachModel(model);
        return result;
    }

    if (fileSize >= 4 && std::memcmp(buffer.get(), "DMB\0", 4) == 0)
    {
        result.kind = ContentKind::CreationMaterialDat;
        SetFailure(result, Error::CreationMaterialOnly);
        return result;
    }

    if (fileSize >= 4 && std::memcmp(buffer.get(), "SQLE", 4) == 0)
    {
        result.kind = ContentKind::SqleAnimationDat;
        SetFailure(result, Error::SqleAnimationOnly);
        return result;
    }

    result.kind = ContentKind::StandardDat;
    if (!Model_FF11_CheckDAT(
            buffer.get(), static_cast<int>(fileSize), result.asset.ParserContext()))
    {
        SetFailure(result, Error::UnrecognizedDat);
        return result;
    }

    ff11Opts_t parserOptions = {};
    ff11Opts_t* activeOptions = nullptr;
    if (options.renderEnvironment || options.renderUnreferenced || options.userContentLoad)
    {
        parserOptions.renderUnreferenced = options.renderUnreferenced;
        parserOptions.renderEnvironment =
            options.renderEnvironment || options.userContentLoad;
        parserOptions.renderEffectMeshes = false;
        parserOptions.renderWater = options.renderEnvironment || options.userContentLoad;
        parserOptions.collectCollision =
            options.userContentLoad || options.renderEnvironment;
        parserOptions.collectCollisionUnreferenced =
            options.userContentLoad || options.renderEnvironment;
        activeOptions = &parserOptions;
    }

    noesisModel_t* model = nullptr;
    {
        ScopedParserOptions parserOptionsScope(activeOptions);
        model = Model_FF11_LoadDAT(
            buffer.get(), static_cast<int>(fileSize), result.modelCount,
            result.asset.ParserContext());
        if (model && options.userContentLoad)
            ZoneRoomLoader::Append(model, result.asset.ParserContext(), path);
    }
    if (!model || result.modelCount == 0)
    {
        SetFailure(result, Error::StandardDatHasNoGeometry);
        return result;
    }

    result.asset.AttachModel(model);
    return result;
}

Result LoadDatSet(
    IDirect3DDevice9* device, const char* path, const bool enableTextureCompression)
{
    Result result;
    result.kind = ContentKind::DatSet;
    if (!device || !path || !path[0])
    {
        result.error = Error::InvalidRequest;
        return result;
    }

    char* rawText = nullptr;
    DWORD fileSize = 0;
    FFXIFileIO::ReadResult readResult = FFXIFileIO::ReadResult::InvalidArguments;
    if (!FFXIFileIO::ReadWholeTextFile(path, &rawText, &fileSize, &readResult))
    {
        result.error = MapReadError(readResult);
        return result;
    }
    std::unique_ptr<char[]> text(rawText);

    BeginAsset(result, device, path, enableTextureCompression);
    if (!Model_FF11_CheckDATSet(
            reinterpret_cast<BYTE*>(text.get()), static_cast<int>(fileSize),
            result.asset.ParserContext()))
    {
        SetFailure(result, Error::UnrecognizedDatSet);
        return result;
    }

    noesisModel_t* model = Model_FF11_LoadDATSet(
        reinterpret_cast<BYTE*>(text.get()), static_cast<int>(fileSize),
        result.modelCount, result.asset.ParserContext());
    if (!model || result.modelCount == 0)
    {
        SetFailure(result, Error::DatSetHasNoGeometry);
        return result;
    }

    result.asset.AttachModel(model);
    return result;
}
}
