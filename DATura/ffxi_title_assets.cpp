#include "stdafx.h"
#include "ffxi_title_assets.h"

#include "ffxi_file_io.h"
#include "ffxi_model_lifetime.h"
#include "noesis_rapi.h"
#include "model_ff11.h"

#include <cctype>
#include <cstring>

namespace
{
void CompactAssetName(const char* source, char* destination, const int destinationSize)
{
    if (!source || !destination || destinationSize <= 0)
        return;

    int output = 0;
    for (int index = 0; source[index] && output < destinationSize - 1; ++index)
    {
        if (source[index] != ' ')
            destination[output++] = (char)std::tolower((unsigned char)source[index]);
    }
    destination[output] = '\0';
}
}

namespace FFXITitleAssets
{
noesisTex_t* FindTitleTexture(noesisModel_t* model, const char* needle)
{
    if (!model || !model->pMatData || !needle)
        return nullptr;

    char compactNeedle[64] = {};
    CompactAssetName(needle, compactNeedle, sizeof(compactNeedle));

    noesisMatData_t* materialData = model->pMatData;
    for (int index = 0; index < materialData->texCount; ++index)
    {
        noesisTex_t* texture = materialData->textures[index];
        if (!texture || !texture->name)
            continue;

        char compactName[128] = {};
        CompactAssetName(texture->name, compactName, sizeof(compactName));
        if (std::strstr(compactName, compactNeedle))
            return texture;
    }
    return nullptr;
}

noesisModel_t* LoadTextureDat(IDirect3DDevice9 *device, const char *ffxiRoot,
                              const char *relativePath, const bool enableTextureCompression,
                              noeRAPI_t **outRapi)
{
    if (!device || !ffxiRoot || !relativePath || !outRapi)
        return nullptr;

    char fullPath[MAX_PATH] = {};
    sprintf_s(fullPath, "%s%s", ffxiRoot, relativePath);

    BYTE *buffer = nullptr;
    DWORD fileSize = 0;
    if (!FFXIFileIO::ReadWholeFile(fullPath, &buffer, &fileSize))
        return nullptr;

    noeRAPI_t *rapi = new noeRAPI_t(device);
    rapi->SetTextureCompressionEnabled(enableTextureCompression);
    rapi->SetCurrentFilePath(fullPath);

    if (!Model_FF11_CheckDAT(buffer, static_cast<int>(fileSize), rapi))
    {
        delete[] buffer;
        FFXIModelLifetime::ReleaseParserContext(rapi);
        return nullptr;
    }

    int modelCount = 0;
    noesisModel_t *model = Model_FF11_LoadDAT(buffer, static_cast<int>(fileSize), modelCount, rapi);
    delete[] buffer;

    if (!model || !model->pMatData || model->pMatData->texCount <= 0)
    {
        FFXIModelLifetime::ReleaseParserContext(rapi);
        return nullptr;
    }

    *outRapi = rapi;
    return model;
}
}
