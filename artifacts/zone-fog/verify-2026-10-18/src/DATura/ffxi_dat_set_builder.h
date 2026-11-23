#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

struct FFXICharRace;

// Helpers for constructing Noesis FFXI DAT-set text. They operate only on
// caller-owned buffers and character-table entries.
namespace FFXIDatSet
{
    struct LowPolyOptions
    {
        int raceIndex = 0;
        int faceVariant = 0;
        int equipmentIndex = 0;
    };

    struct PlayerOptions
    {
        int raceIndex = 0;
        int faceVariant = 0;
        int animationBank = 0;
        int headItem = 0;
        int bodyItem = 0;
        int handsItem = 0;
        int legsItem = 0;
        int feetItem = 0;
        int mainItem = 0;
        int subItem = 0;
        int rangedItem = 0;
    };

    void AppendLine(char* out, std::size_t outSize, const char* name, const char* path);
    bool MakeOffsetPath(const char* basePath, int offset, char* outPath, std::size_t outPathSize);
    void AppendVariantLine(char* out, std::size_t outSize, const char* name,
                           const FFXICharRace& race, int entryIndex, int variantIndex);
    void AppendInferredLine(char* out, std::size_t outSize, const char* name,
                            const FFXICharRace& race, int entryIndex, int offset);
    void BuildLowPoly(const LowPolyOptions& options, char* out, std::size_t outSize);
    bool BuildPlayer(const char* ffxiRoot, const PlayerOptions& options,
                     char* out, std::size_t outSize);
    // Retail look IDs are logical file-table indices, not physical DAT offsets.
    bool BuildNpc(const char* ffxiRoot, const std::vector<std::uint8_t>& look,
                  char* out, std::size_t outSize);
}
