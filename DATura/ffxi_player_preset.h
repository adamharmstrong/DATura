#pragma once

#include <cstddef>

namespace FFXIPlayerPreset
{
    struct Data
    {
        int raceIndex = 0;
        int faceVariant = 0;
        int mainType = 0;
        int mainItem = 0;
        int subType = 0;
        int subItem = 0;
        int rangedType = 0;
        int rangedItem = 0;
        int headItem = 0;
        int bodyItem = 0;
        int handsItem = 0;
        int legsItem = 0;
        int feetItem = 0;
        int animationBank = 0;
        int animationMode = 0;
    };

    void BuildText(const Data& preset, char* outText, std::size_t outTextSize);
    bool ParseText(const char* text, Data* inOutPreset);
}
