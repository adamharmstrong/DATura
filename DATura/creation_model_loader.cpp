#include "stdafx.h"
#include "creation_model_loader.h"

#include "ffxi_file_io.h"
#include "ffxi_parser_diagnostics.h"
#include "ffxi_paths.h"
#include "noesis_rapi.h"
#include "model_ff11.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <memory>

namespace
{
bool UsesInitialEquipmentAdjustment(const char* label) noexcept
{
    return label && std::strncmp(label, "Initial Equipment", 17) == 0;
}

void ResolveBodyPaths(
    const CreationModelLoader::Request& request,
    char adjustedMesh[32], char adjustedMaterial[32],
    const char*& bodyMesh, const char*& bodyMaterial)
{
    bodyMesh = request.bodyMeshDat;
    bodyMaterial = request.bodyMaterialDat;
    if (!UsesInitialEquipmentAdjustment(request.label) ||
        !request.bodyMeshDat || !request.bodyMaterialDat)
    {
        return;
    }

    int meshRom = 0;
    int meshDat = 0;
    int materialRom = 0;
    int materialDat = 0;
    if (sscanf_s(request.bodyMeshDat, "ROM/%i/%i.dat", &meshRom, &meshDat) == 2 &&
        sscanf_s(
            request.bodyMaterialDat, "ROM/%i/%i.dat",
            &materialRom, &materialDat) == 2)
    {
        sprintf_s(adjustedMesh, 32, "ROM/%i/%i.dat", meshRom, meshDat + 2);
        sprintf_s(
            adjustedMaterial, 32, "ROM/%i/%i.dat",
            materialRom, materialDat + 2);
        bodyMesh = adjustedMesh;
        bodyMaterial = adjustedMaterial;
    }
}
}

namespace CreationModelLoader
{
bool Result::Succeeded() const noexcept
{
    return error == Error::None && static_cast<bool>(asset);
}

Result Load(IDirect3DDevice9* device, const Request& request)
{
    Result result;
    if (!device || !request.ffxiRoot || !request.ffxiRoot[0])
    {
        result.error = Error::InvalidRequest;
        return result;
    }

    char adjustedBodyMesh[32] = {};
    char adjustedBodyMaterial[32] = {};
    const char* bodyMesh = nullptr;
    const char* bodyMaterial = nullptr;
    ResolveBodyPaths(
        request, adjustedBodyMesh, adjustedBodyMaterial, bodyMesh, bodyMaterial);

    const char* meshPaths[2] = { bodyMesh, request.headMeshDat };
    const char* materialPaths[2] = { bodyMaterial, request.headMaterialDat };
    std::array<std::unique_ptr<BYTE[]>, 2> meshStorage;
    std::array<std::unique_ptr<BYTE[]>, 2> materialStorage;
    BYTE* meshBuffers[2] = {};
    int meshLengths[2] = {};
    BYTE* materialBuffers[2] = {};
    int materialLengths[2] = {};
    int materialAlphaModes[2] = {};
    float meshOffsets[2][3] = {};
    int fileCount = 0;
    int bodyFileIndex = -1;
    int headFileIndex = -1;
    char firstMeshPath[MAX_PATH] = {};

    for (int meshIndex = 0; meshIndex < 2; ++meshIndex)
    {
        if (!meshPaths[meshIndex])
            continue;

        char fullPath[MAX_PATH] = {};
        FFXIPath::BuildFullPath(
            request.ffxiRoot, meshPaths[meshIndex], fullPath, sizeof(fullPath));

        BYTE* rawMesh = nullptr;
        DWORD meshSize = 0;
        if (!FFXIFileIO::ReadWholeFile(fullPath, &rawMesh, &meshSize))
        {
            result.error = Error::MeshFileOpenFailed;
            return result;
        }

        if (!firstMeshPath[0])
            strcpy_s(firstMeshPath, fullPath);

        meshStorage[fileCount].reset(rawMesh);
        meshBuffers[fileCount] = rawMesh;
        meshLengths[fileCount] = static_cast<int>(meshSize);
        if (meshIndex == 0)
            bodyFileIndex = fileCount;
        else
            headFileIndex = fileCount;

        if (materialPaths[meshIndex])
        {
            char materialPath[MAX_PATH] = {};
            FFXIPath::BuildFullPath(
                request.ffxiRoot, materialPaths[meshIndex],
                materialPath, sizeof(materialPath));
            BYTE* rawMaterial = nullptr;
            DWORD materialSize = 0;
            if (FFXIFileIO::ReadWholeFile(
                    materialPath, &rawMaterial, &materialSize))
            {
                materialStorage[fileCount].reset(rawMaterial);
                materialBuffers[fileCount] = rawMaterial;
                materialLengths[fileCount] = static_cast<int>(materialSize);
                materialAlphaModes[fileCount] = meshIndex == 1 ?
                    request.headAlphaMode : FFXI_CREATION_ALPHA_BODY_CUTOUT;
            }
        }

        if (meshIndex == 1 && request.bodyMeshDat && request.headMeshDat)
            meshOffsets[fileCount][1] = request.headYOffset;
        ++fileCount;
    }

    if (fileCount == 0)
    {
        result.error = Error::NoMeshPaths;
        return result;
    }

    result.asset.Adopt(nullptr, new noeRAPI_t(device));
    result.asset.ParserContext()->SetTextureCompressionEnabled(
        request.enableTextureCompression);
    result.asset.ParserContext()->SetCurrentFilePath(
        firstMeshPath[0] ? firstMeshPath : "DATura character creation model");

    if (bodyFileIndex >= 0 && headFileIndex >= 0)
    {
        result.skeletonInspectionPerformed = true;
        FFXIParserDiagnostics::ScopedSnapshot parserDiagnostics;

        char bodyAnimationPath[MAX_PATH] = {};
        char headAnimationPath[MAX_PATH] = {};
        FFXIPath::BuildFullPath(
            request.ffxiRoot, request.bodyAnimationDat,
            bodyAnimationPath, sizeof(bodyAnimationPath));
        FFXIPath::BuildFullPath(
            request.ffxiRoot, request.headAnimationDat,
            headAnimationPath, sizeof(headAnimationPath));

        BYTE* rawBodyAnimation = nullptr;
        BYTE* rawHeadAnimation = nullptr;
        DWORD bodyAnimationSize = 0;
        DWORD headAnimationSize = 0;
        std::unique_ptr<BYTE[]> bodyAnimation;
        std::unique_ptr<BYTE[]> headAnimation;
        float bodyNeck[3] = {};
        float headNeck[3] = {};
        if (FFXIFileIO::ReadWholeFile(
                bodyAnimationPath, &rawBodyAnimation, &bodyAnimationSize))
        {
            bodyAnimation.reset(rawBodyAnimation);
        }
        if (FFXIFileIO::ReadWholeFile(
                headAnimationPath, &rawHeadAnimation, &headAnimationSize))
        {
            headAnimation.reset(rawHeadAnimation);
        }

        if (bodyAnimation && headAnimation &&
            Model_FF11_GetDATBonePosition(
                bodyAnimation.get(), static_cast<int>(bodyAnimationSize),
                "bone0004", bodyNeck, result.asset.ParserContext()) &&
            Model_FF11_GetDATBonePosition(
                headAnimation.get(), static_cast<int>(headAnimationSize),
                "bone0001", headNeck, result.asset.ParserContext()))
        {
            meshOffsets[headFileIndex][0] = bodyNeck[0] - headNeck[0];
            meshOffsets[headFileIndex][1] = -(bodyNeck[1] - headNeck[1]);
            meshOffsets[headFileIndex][2] = bodyNeck[2] - headNeck[2];
        }
    }

    noesisModel_t* model = Model_FF11_LoadCreationDATList(
        meshBuffers, meshLengths, materialBuffers, materialLengths,
        materialAlphaModes, &meshOffsets[0][0], fileCount,
        result.modelCount, result.asset.ParserContext());
    if (!model || result.modelCount == 0)
    {
        // Attach any parser-returned model before resetting so the rare
        // non-null/zero-count failure shape cannot leak its GPU buffers.
        result.asset.AttachModel(model);
        result.asset.Reset();
        result.error = Error::NoDisplayableGeometry;
        result.modelCount = 0;
        return result;
    }

    result.asset.AttachModel(model);
    return result;
}
}
