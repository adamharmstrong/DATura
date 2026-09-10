#include "stdafx.h"
#include "npc_placement.h"
#include <filesystem>
#include <fstream>
#include <set>

bool TestStarterQuests()
{
    namespace fs = std::filesystem;
    namespace npc = FFXINpcPlacement;
    const fs::path root = fs::absolute(fs::path(__FILE__)).parent_path().parent_path();
    const auto catalog = (root / "DATura/npc_placements.csv").string();
    const auto quests = (root / "DATura/npc_starter_quests.csv").string();
    int total = 0;
    const std::set<std::string> excluded = {"Balasiel", "Grin", "Eddy", "Pygmalion", "Gulemont", "Maysoon"};
    for (int zone : {230, 231, 232, 234, 235, 236, 237, 238, 239, 240, 241})
    {
        if (!npc::LoadCatalogForZone(zone, catalog.c_str())) return false;
        int marked = 0;
        for (const auto& placement : npc::Snapshot())
        {
            if (placement.starterQuestAvailable)
            {
                if (excluded.count(placement.name)) return false;
                if (placement.starterQuestName.empty()) return false;
                ++marked;
            }
        }
        if (!marked) return false;
        total += marked;
    }
    if (total != 70) return false;
    if (!npc::LoadCatalogForZone(235, catalog.c_str(), nullptr, "missing-starter-quest-catalog.csv")) return false;
    for (const auto& placement : npc::Snapshot())
        if (placement.starterQuestAvailable || !placement.starterQuestName.empty()) return false;

    // Exercise identity collisions, malformed IDs, review status, quoted fields,
    // and repeated quest rows using the production placement loader.
    const fs::path temporary = fs::temp_directory_path() /
        ("datura-starter-quest-test-" + std::to_string(GetCurrentProcessId()));
    fs::create_directories(temporary);
    struct Cleanup { fs::path path; ~Cleanup() { std::error_code e; fs::remove_all(path, e); } } cleanup{temporary};
    const auto fixture = (temporary / "placements.csv").string();
    const auto review = (temporary / "quests.csv").string();
    {
        std::ofstream p(fixture);
        for (int id = 1; id <= 7; ++id)
            p << "235," << id << ',' << id << ",Same Name,0,0,0,0,0,0100010414101720003067400350006000700000\n";
        std::ofstream q(review);
        q << "235,1,Same Name,\"Quest, with comma\",verified,https://example.com,2026-09-09,reviewed\r\n"
          << "235,1,Same Name,Another quest,verified,https://example.com,2026-09-09,reviewed\n"
          << "234,2,Same Name,Wrong zone,verified,https://example.com,2026-09-09,reviewed\n"
          << "235,3,Other Name,Wrong name,verified,https://example.com,2026-09-09,reviewed\n"
          << "235,4,Same Name,Unreviewed,pending,https://example.com,2026-09-09,reviewed\n"
          << "235,5,Same Name,No source,verified,,2026-09-09,reviewed\n"
          << "235,4294967302,Same Name,Overflow,verified,https://example.com,2026-09-09,reviewed\n"
          << "235,7oops,Same Name,Invalid ID,verified,https://example.com,2026-09-09,reviewed\n";
    }
    if (!npc::LoadCatalogForZone(235, fixture.c_str(), nullptr, review.c_str())) return false;
    for (const auto& placement : npc::Snapshot())
    {
        if (placement.starterQuestAvailable != (placement.entityId == 1)) return false;
        if ((placement.starterQuestName == "Another quest") != (placement.entityId == 1)) return false;
    }
    npc::ResetForZone(230);
    if (npc::Count() != 0) return false;
    return true;
}
