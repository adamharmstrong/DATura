#include "stdafx.h"
#include "ffxi_dat_set_builder.h"
#include "ffxi_resource.h"

namespace FFXIDatSet
{
bool BuildNpc(const char* root, const std::vector<std::uint8_t>& look,
              char* out, std::size_t outSize)
{
    if (!out || !outSize) return false;
    out[0] = 0;
    if (!root || !root[0] || look.size() != 20 || look[0] != 1 || look[1] != 0 ||
        look[3] < 1 || look[3] > 8 || look[2] >= 32) return false;

    // Logical skeleton/face/head/weapon bases, verified against installed
    // VTABLE/FTABLE. Tarutaru share equipment, but have separate face banks.
    static constexpr int skeletons[] = {7072,10248,13424,16600,19776,19776,23176,26352};
    static constexpr int faces[] = {7080,10256,13432,16608,19784,22952,23184,26360};
    static constexpr int heads[] = {7112,10288,13464,16640,19816,19816,23216,26392};
    static constexpr int weapons[] = {8392,11568,14744,17920,21096,21096,24496,27672};
    const int race = look[3] - 1;
    const int equipmentRace = race >= 5 ? race - 1 : race;
    std::string assetRoot(root);
    if (assetRoot.back() != '/' && assetRoot.back() != '\\') assetRoot += '/';
    std::string result = "NOESIS_FF11_DAT_SET\nsetPathAbs \"" + assetRoot + "\"\n";
    const auto append = [&](const char* name, int fileId)
    {
        FFXIResource::ResolvedFile file;
        if (!FFXIResource::ResolveFileId(root, fileId, file)) return false;
        result += "dat \"" + std::string(name) + "\" \"" + file.relativePath + "\"\n";
        return true;
    };
    if (!append("__skeleton", skeletons[race]) ||
        !append("__animation", skeletons[race] + 1) ||
        !append("face", faces[race] + look[2])) return false;
    static constexpr const char* slots[] = {"head","body","hands","legs","feet","main","sub","ranged"};
    for (int slot = 0; slot < 8; ++slot)
    {
        const int offset = 4 + slot * 2;
        const int model = (look[offset] | (look[offset + 1] << 8)) & 0x0fff;
        int fileId;
        if (slot < 5)
        {
            // Expansion armor uses a 64-entry bank, then a 256-entry bank.
            // These are separate logical ranges, not adjacent physical DATs.
            // Never let an unknown ID spill into another slot.
            if (model >= 512) return false;
            if (model < 256) fileId = heads[race] + slot * 256 + model;
            else if (model < 320) fileId = 63323 + equipmentRace * 448 + slot * 64 + model - 256;
            else fileId = 71247 + equipmentRace * 1536 + slot * 256 + model - 320;
        }
        else
        {
            if (model == 0) continue;
            if (model >= 1280) return false;
            fileId = weapons[race] + model;
        }
        if (!append(slots[slot], fileId)) return false;
    }
    if (result.size() >= outSize) return false;
    memcpy(out, result.c_str(), result.size() + 1);
    return true;
}
}
