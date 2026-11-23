#include "stdafx.h"
#include "ffxi_creation_scene_builder.h"

#include <cstdio>

namespace FFXICreationScene
{
void Build(const Inputs& inputs, char* out, std::size_t outSize)
{
    sprintf_s(out, outSize,
        "NOESIS_SCENE_FILE\n"
        "version 1\n"
        "physicslib\t\t\"\"\n"
        "defaultAxis\t\t\"0\"\n"
        "\n"
        "object\n"
        "{\n"
        "\tname\t\t\t\"body\"\n"
        "\tmodel\t\t\t\"%s\"\n"
        "\tloadOptions\t\t\"-ff11sqleanim %s\"\n"
        "}\n"
        "object\n"
        "{\n"
        "\tname\t\t\t\"head\"\n"
        "\tmodel\t\t\t\"%s\"\n"
        "\tloadOptions\t\t\"-ff11sqleanim %s\"\n"
        "\t;this uses the relative positions of the neck joints on each skeleton to place the head\n"
        "\toffsetWithBones\t\"bone0001\" \"body\" \"bone0004\"\n"
        "\t;combine both objects into a single model\n"
        "\tmergeTo\t\t\t\"body\"\n"
        "}\n",
        inputs.bodyMesh, inputs.bodyAnimation, inputs.headMesh, inputs.headAnimation);
}
}
