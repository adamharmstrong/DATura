#include "stdafx.h"
#include "ffxi_creation_export.h"

#include "ffxi_creation_animation_paths.h"
#include "ffxi_creation_scene_builder.h"
#include "ffxi_file_io.h"

#include <cstdio>

namespace FFXICreationExport
{
void MakeDefaultNoesisPath(const char* characterName, char* outPath, std::size_t outPathSize)
{
    char exeDir[MAX_PATH] = {};
    char stem[64] = {};
    FFXIFileIO::GetExecutableDirectory(exeDir, sizeof(exeDir));
    FFXIFileIO::MakeSafeFileStem(characterName, stem, sizeof(stem));
    sprintf_s(outPath, outPathSize, "%s%s.noesis", exeDir, stem);
}

void BuildNoesisScene(const Inputs& inputs, char* out, std::size_t outSize)
{
    const char* bodyMeshDat = inputs.bodyMeshDat;
    char initialBodyMeshDat[32] = {};
    if (inputs.equipmentIndex == 1 && bodyMeshDat)
    {
        int meshRom = 0;
        int meshDat = 0;
        if (sscanf_s(bodyMeshDat, "ROM/%i/%i.dat", &meshRom, &meshDat) == 2)
        {
            sprintf_s(initialBodyMeshDat, "ROM/%i/%i.dat", meshRom, meshDat + 2);
            bodyMeshDat = initialBodyMeshDat;
        }
    }

    char bodyAnimation[64] = {};
    char headAnimation[64] = {};
    FFXICreationAnimationPaths::MakeRelativeOption(
        FFXICreationAnimationPaths::BodyBase(inputs.raceIndex), bodyAnimation, sizeof(bodyAnimation));
    FFXICreationAnimationPaths::MakeRelativeOption(
        FFXICreationAnimationPaths::HeadBase(inputs.raceIndex, inputs.headMeshDat),
        headAnimation, sizeof(headAnimation));

    FFXICreationScene::Inputs sceneInputs;
    sceneInputs.bodyMesh = bodyMeshDat ? bodyMeshDat : "";
    sceneInputs.headMesh = inputs.headMeshDat ? inputs.headMeshDat : "";
    sceneInputs.bodyAnimation = bodyAnimation;
    sceneInputs.headAnimation = headAnimation;
    FFXICreationScene::Build(sceneInputs, out, outSize);
}
}
