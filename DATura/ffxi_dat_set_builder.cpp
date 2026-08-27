#include "stdafx.h"
#include "ffxi_dat_set_builder.h"

#include "character_dat_table.h"

#include <cstdio>
#include <cstring>

namespace FFXIDatSet
{
void AppendLine(char* out, std::size_t outSize, const char* name, const char* path)
{
    strcat_s(out, outSize, "dat \"");
    strcat_s(out, outSize, name);
    strcat_s(out, outSize, "\" \"");
    strcat_s(out, outSize, path);
    strcat_s(out, outSize, "\"\n");
}

bool MakeOffsetPath(const char* basePath, int offset, char* outPath, std::size_t outPathSize)
{
    int rom = 0;
    int dat = 0;
    if (!basePath || sscanf_s(basePath, "ROM/%i/%i.dat", &rom, &dat) != 2)
        return false;

    dat += offset;
    while (dat > 127)
    {
        dat -= 128;
        ++rom;
    }
    while (dat < 0 && rom > 0)
    {
        dat += 128;
        --rom;
    }
    if (dat < 0)
        return false;

    sprintf_s(outPath, outPathSize, "ROM/%i/%i.dat", rom, dat);
    return true;
}

void AppendVariantLine(char* out, std::size_t outSize, const char* name,
                       const FFXICharRace& race, int entryIndex, int variantIndex)
{
    if (entryIndex < 0 || entryIndex >= race.count || variantIndex < 0)
        return;

    char path[64] = {};
    if (MakeOffsetPath(race.entries[entryIndex].dat, variantIndex, path, sizeof(path)))
        AppendLine(out, outSize, name, path);
}

void AppendInferredLine(char* out, std::size_t outSize, const char* name,
                        const FFXICharRace& race, int entryIndex, int offset)
{
    if (entryIndex < 0 || entryIndex >= race.count)
        return;
    if (offset <= 0)
        AppendLine(out, outSize, name, race.entries[entryIndex].dat);
    else
        AppendVariantLine(out, outSize, name, race, entryIndex, offset);
}

void BuildLowPoly(const LowPolyOptions& options, char* out, std::size_t outSize)
{
    const int raceIndex = options.raceIndex >= 0 && options.raceIndex < kFFXICharRaceCount ? options.raceIndex : 0;
    const FFXICharRace& race = kFFXICharRaces[raceIndex];
    const int equipmentOffset = options.equipmentIndex == 1 ? 1 : 0;

    strcpy_s(out, outSize,
        "NOESIS_FF11_DAT_SET\n"
        ";^ must be the first line of the file\n"
        "\n"
        ";search for dats using a path retrieved from a registry key\n"
        "setPathKey\t\"HKEY_LOCAL_MACHINE\" \"SOFTWARE\\PlayOnlineUS\\InstallFolder\" \"0001\"\n"
        "\n"
        ";search for dats on a path relative to this file \n"
        ";setPathRel\t\"./\"\n"
        "\n"
        ";search for dats on an absolute path\n"
        ";setPathAbs\t\"c:/whatever/ff11/\"\n"
        "\n");

    AppendLine(out, outSize, "__skeleton", race.entries[0].dat);
    if (race.count > 8)
        AppendLine(out, outSize, "__animation", race.entries[8].dat);

    AppendInferredLine(out, outSize, "face", race, 1, options.faceVariant);
    AppendInferredLine(out, outSize, "head", race, 2, options.faceVariant);
    AppendInferredLine(out, outSize, "body", race, 3, equipmentOffset);
    AppendInferredLine(out, outSize, "hands", race, 4, equipmentOffset);
    AppendInferredLine(out, outSize, "waist", race, 5, equipmentOffset);
    AppendInferredLine(out, outSize, "legs", race, 6, equipmentOffset);
    AppendLine(out, outSize, "weapon", race.entries[7].dat);
}

bool BuildPlayer(
    const char* ffxiRoot, const PlayerOptions& options,
    char* out, const std::size_t outSize)
{
    if (!ffxiRoot || !ffxiRoot[0] || !out || outSize == 0 ||
        options.raceIndex < 0 || options.raceIndex >= kFFXICharRaceCount)
    {
        if (out && outSize > 0)
            out[0] = '\0';
        return false;
    }

    const FFXICharRace& race = kFFXICharRaces[options.raceIndex];
    if (race.count < 8)
    {
        out[0] = '\0';
        return false;
    }

    strcpy_s(out, outSize, "NOESIS_FF11_DAT_SET\nsetPathAbs \"");
    strcat_s(out, outSize, ffxiRoot);
    strcat_s(out, outSize, "\"\n");
    AppendLine(out, outSize, "__skeleton", race.entries[0].dat);
    AppendVariantLine(
        out, outSize, "__animation", race, 8, options.animationBank);

    if (options.faceVariant <= 0)
        AppendLine(out, outSize, "face", race.entries[1].dat);
    else
        AppendVariantLine(
            out, outSize, "face", race, 1, options.faceVariant);

    if (options.headItem <= 0)
        AppendLine(out, outSize, "head", race.entries[2].dat);
    else
        AppendVariantLine(
            out, outSize, "head", race, 2, options.headItem - 1);

    AppendVariantLine(out, outSize, "body", race, 3, options.bodyItem);
    AppendVariantLine(out, outSize, "hands", race, 4, options.handsItem);
    AppendVariantLine(out, outSize, "legs", race, 5, options.legsItem);
    AppendVariantLine(out, outSize, "feet", race, 6, options.feetItem);
    if (options.mainItem > 0)
        AppendVariantLine(out, outSize, "main", race, 7, options.mainItem - 1);
    if (options.subItem > 0)
        AppendVariantLine(out, outSize, "sub", race, 7, options.subItem - 1);
    if (options.rangedItem > 0)
        AppendVariantLine(
            out, outSize, "ranged", race, 7, options.rangedItem - 1);
    return true;
}
}
