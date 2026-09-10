#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace ZoneRoomCatalog
{
struct Room
{
    std::string path;
    std::string rootName;
    unsigned int subAreaId;
};

// Verified against installed MZB replacement links and RID room-entry triggers.
// The audit includes rooms with no outdoor proxy (notably Adoulin and Heavens Tower).
// Never infer ownership from proximity or the four-byte root name alone.
inline std::vector<Room> Resolve(const std::string& zonePath)
{
    std::string path = zonePath;
    std::replace(path.begin(), path.end(), '\\', '/');
    std::transform(path.begin(), path.end(), path.begin(), [](unsigned char c) {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 'a' - 'A') : static_cast<char>(c);
    });
    struct Group {
        const char* zone;
        const char* directory; // relative to the installation root, not the main DAT
        int first, last;
        const char* root;
        unsigned int firstArea;
    };
    static const Group groups[] = {
        { "rom4/0/3.dat", "ROM4/0", 39, 49, "r_2g", 560 }, // Aht Urhgan Whitegate
        { "rom4/0/6.dat", "ROM4/0", 50, 50, "r_3g", 571 }, // Nashmau
        { "rom/1/31.dat", "ROM/1", 103, 110, "r_1s", 326 }, // Southern San d'Oria
        { "rom/1/31.dat", "ROM/1", 111, 120, "r_1s", 335 }, // Southern San d'Oria
        { "rom/1/32.dat", "ROM/1", 121, 127, "r_2s", 349 }, // Northern San d'Oria
        { "rom/1/32.dat", "ROM/2", 0, 5, "r_2s", 356 }, // Northern San d'Oria
        { "rom/0/113.dat", "ROM/2", 6, 11, "r_3s", 364 }, // Port San d'Oria
        { "rom/1/33.dat", "ROM/2", 12, 17, "r_4s", 371 }, // Chateau d'Oraguille
        { "rom/1/34.dat", "ROM/1", 48, 60, "r_1b", 259 }, // Bastok Mines
        { "rom/1/35.dat", "ROM/1", 61, 74, "r_2b", 274 }, // Bastok Markets
        { "rom/1/36.dat", "ROM/1", 94, 102, "r_3b", 315 }, // Port Bastok
        { "rom/1/37.dat", "ROM/1", 75, 93, "r_4b", 293 }, // Metalworks
        { "rom/0/78.dat", "ROM/2", 18, 38, "r_1w", 379 }, // Windurst Waters
        { "rom/0/79.dat", "ROM/2", 39, 45, "r_2w", 401 }, // Windurst Walls
        { "rom/0/80.dat", "ROM/2", 46, 51, "r_3w", 408 }, // Port Windurst
        { "rom/0/80.dat", "ROM/2", 52, 52, "r_3w", 416 }, // Port Windurst
        { "rom/0/81.dat", "ROM/2", 53, 60, "r_4w", 417 }, // Windurst Woods
        { "rom/1/38.dat", "ROM/2", 61, 64, "r_5w", 425 }, // Heavens Tower
        { "rom/1/39.dat", "ROM/2", 65, 70, "r_1j", 431 }, // Ru'Lude Gardens
        { "rom/1/39.dat", "ROM/2", 72, 75, "r_1j", 438 }, // Ru'Lude Gardens
        { "rom/1/40.dat", "ROM/2", 76, 85, "r_2j", 443 }, // Upper Jeuno
        { "rom/1/41.dat", "ROM/2", 86, 98, "r_3j", 454 }, // Lower Jeuno
        { "rom/1/42.dat", "ROM/2", 99, 108, "r_4j", 468 }, // Port Jeuno
        { "rom/1/42.dat", "ROM/2", 122, 125, "r_4j", 495 }, // Port Jeuno
        { "rom/1/43.dat", "ROM/2", 109, 111, "r_1s", 480 }, // Selbina
        { "rom/1/43.dat", "ROM/2", 113, 114, "r_1s", 484 }, // Selbina
        { "rom/1/44.dat", "ROM/2", 115, 119, "r_1m", 487 }, // Mhaura
        { "rom/1/44.dat", "ROM/2", 121, 121, "r_1m", 493 }, // Mhaura
        { "rom2/0/25.dat", "ROM2/0", 28, 35, "r_1k", 505 }, // Kazham
        { "rom2/0/27.dat", "ROM2/0", 36, 37, "r_1n", 514 }, // Norg
        { "rom9/0/3.dat", "ROM9/5", 49, 52, "r_1a", 585 }, // Western Adoulin
        { "rom9/0/4.dat", "ROM9/3", 84, 85, "r_2a", 589 }, // Eastern Adoulin
    };
    std::vector<Room> rooms;
    for (const auto& group : groups)
    {
        const std::string suffix = group.zone;
        if (path.size() < suffix.size()) continue;
        const size_t start = path.size() - suffix.size();
        if (path.compare(start, suffix.size(), suffix) || (start && path[start - 1] != '/')) continue;
        const std::string installation = zonePath.substr(0, start);
        std::string directory = group.directory;
        directory += '/';
        if (zonePath.find('\\') != std::string::npos && zonePath.find('/') == std::string::npos)
            std::replace(directory.begin(), directory.end(), '/', '\\');
        for (int file = group.first; file <= group.last; ++file)
            rooms.push_back({ installation + directory + std::to_string(file) + ".DAT", group.root,
                              group.firstArea + static_cast<unsigned int>(file - group.first) });
    }
    return rooms;
}
}
