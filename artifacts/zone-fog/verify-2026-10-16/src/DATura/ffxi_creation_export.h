#pragma once

#include <cstddef>

namespace FFXICreationExport
{
    struct Inputs
    {
        int raceIndex = 0;
        int equipmentIndex = 0;
        const char* bodyMeshDat = nullptr;
        const char* headMeshDat = nullptr;
    };

    void MakeDefaultNoesisPath(const char* characterName, char* outPath, std::size_t outPathSize);
    void BuildNoesisScene(const Inputs& inputs, char* out, std::size_t outSize);
}
