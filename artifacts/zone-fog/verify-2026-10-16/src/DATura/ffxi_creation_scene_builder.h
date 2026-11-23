#pragma once

#include <cstddef>

namespace FFXICreationScene
{
    struct Inputs
    {
        const char* bodyMesh = "";
        const char* headMesh = "";
        const char* bodyAnimation = "";
        const char* headAnimation = "";
    };

    void Build(const Inputs& inputs, char* out, std::size_t outSize);
}
