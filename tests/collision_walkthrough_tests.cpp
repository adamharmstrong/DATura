// Automated collision walkthrough: drives the production player controller over
// each zone's authored collision (outdoor MZB plus companion rooms, as the app
// loads them). Two probes per zone and orientation:
//  1. Placement crossings: walk straight across sampled static placements from
//     four directions and classify crossed / blocked / fell / no floor. Object
//     names only label the report; they never affect collision.
//  2. Reachable wandering: agents start at authored zone-line arrival points and
//     catalog NPC positions (places a player can actually stand) and run in
//     random directions. Leaving the world (respawn) from reachable ground is a
//     hole or a missing boundary; long drops are reported for inspection.
#include "stdafx.h"
#include "zone_dat_table.h"
#include "zone_transition.h"
#include "zone_collision_geometry.h"
#include "zone_room_loader.h"
#include "player_controller.h"
#include "npc_placement.h"
#include "ffxi_coordinate_frame.h"
#include "ffxi_file_io.h"
#include "noesis_rapi.h"
#include "model_ff11.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <string_view>

namespace
{
std::filesystem::path gCatalog;
std::vector<ZoneTriggerVolume::Volume> gVolumes; // authored RID zone-line volumes, native space

struct Outcome { int crossed = 0, blocked = 0, fell = 0, noFloor = 0; };

std::string Category(std::string name)
{
    for (char& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (name.find("kusa") != std::string::npos || name.find("grass") != std::string::npos) return "grass";
    if (name.find("hana") != std::string::npos) return "flower";
    if (name.starts_with("_ron_k")) return "shrub";
    return "other";
}

bool Ground(const ZoneCollision::Mesh& mesh, float& x, float& y, float& z)
{
    return ZoneCollision::FindNearestSafeFloor(mesh.Triangles(), mesh.Index(),
        PlayerController::kCollisionRadius, PlayerController::kCollisionHeight,
        PlayerController::kStepHeight, x, y, z, &x, &y, &z);
}

bool LoadMesh(const std::string& path, const bool mirror, ZoneCollision::Mesh& mesh)
{
    BYTE* raw = nullptr; DWORD size = 0;
    if (!FFXIFileIO::ReadWholeFile(path.c_str(), &raw, &size)) return false;
    std::unique_ptr<BYTE[]> bytes(raw);
    static noeRAPI_t* rapi = nullptr; // models stay alive for the process; rooms use its allocator
    rapi = new noeRAPI_t(nullptr);
    rapi->SetCurrentFilePath(path.c_str());
    ff11Opts_t options = {};
    options.collectCollision = true;
    gpFF11Opts = &options;
    gFF11LastCollisionTriangles.clear();
    gFF11LastCollisionMeshes.clear();
    gFF11LastMapObjects.clear();
    int count = 0;
    auto* model = Model_FF11_LoadDAT(bytes.get(), static_cast<int>(size), count, rapi);
    if (model) ZoneRoomLoader::Append(model, rapi, path.c_str());
    // The existing Metalworks lift bindings register their platforms as moving
    // collision at rest, as the app does; without them the shafts are holes.
    if (model)
        for (auto& object : gFF11LastMapObjects)
            if ((object.mapRecordIndex == 422 || object.mapRecordIndex == 423) &&
                std::string_view(object.objectName).starts_with("liftall"))
                Model_FF11_CollectMovingObjectCollision(model, object, false);
    gVolumes.clear();
    ZoneTriggerVolume::Decode(bytes.get(), size, gVolumes);
    gpFF11Opts = nullptr;
    for (const auto& rawTriangle : gFF11LastCollisionTriangles)
    {
        ZoneCollision::Triangle triangle;
        if (ZoneCollision::BuildTriangle(&rawTriangle.p[0][0], mirror, triangle))
            mesh.AddTriangle(triangle, PlayerController::kCollisionRadius);
    }
    return !mesh.Empty();
}

std::string Env(const char* name)
{
    char* value = nullptr; size_t length = 0;
    std::string result;
    if (_dupenv_s(&value, &length, name) == 0 && value) result = value;
    free(value);
    return result;
}

int gBlockSlope[4] = {}; // blocking face |normal.y|: <0.1, <0.35, <=0.45, none found

void ClassifyBlock(const ZoneCollision::Mesh& mesh, const float p[3])
{
    std::vector<int> candidates;
    mesh.Index().Query(p[0], p[2], PlayerController::kCollisionRadius + 0.3f, candidates);
    float steepest = 2;
    for (int i : candidates)
    {
        const auto& t = mesh.Triangles()[i];
        const float ny = std::fabs(t.normal[1]);
        if (ny > 0.45f || p[1] - 0.001f < t.minY || p[1] - PlayerController::kCollisionHeight > t.maxY ||
            t.minY >= p[1] - PlayerController::kStepHeight ||
            !ZoneCollision::PointNearTriangleXZ(p[0], p[2], t, PlayerController::kCollisionRadius + 0.3f)) continue;
        steepest = (std::min)(steepest, ny);
    }
    ++gBlockSlope[steepest < 0.1f ? 0 : steepest < 0.35f ? 1 : steepest <= 0.45f ? 2 : 3];
}

// Returns 0 crossed, 1 blocked, 2 fell, 3 no floor.
int Cross(const ZoneCollision::Mesh& mesh, const float center[3], float dx, float dz)
{
    float x = center[0] - dx * 4, y = center[1] - 1, z = center[2] - dz * 4;
    if (!Ground(mesh, x, y, z)) return 3;
    if (std::fabs(x - (center[0] - dx * 4)) + std::fabs(z - (center[2] - dz * 4)) > 1.5f) return 3;
    PlayerController::State player;
    PlayerController::SetPose(player, x, y, z, std::atan2(-dx, -dz), true);
    PlayerController::SetRespawnPoint(player);
    PlayerController::InputSnapshot input;
    input.forward = 1;
    const PlayerController::SimulationContext context{ &mesh };
    const float startY = y;
    float best = 0;
    int stalled = 0;
    for (int frame = 0; frame < 240; ++frame)
    {
        const auto result = PlayerController::UpdateMovement(player, input, 1.0f / 60, context);
        if (result.respawned || player.position[1] - startY > 6) return 2;
        const float progress = (player.position[0] - x) * dx + (player.position[2] - z) * dz;
        if (progress >= 8) return 0;
        if (progress > best + 0.01f) { best = progress; stalled = 0; }
        else if (++stalled > 30) { ClassifyBlock(mesh, player.position); return 1; }
    }
    ClassifyBlock(mesh, player.position);
    return 1;
}

int Trace(const std::string& path, float x, float y, float z, float yaw)
{
    ZoneCollision::Mesh mesh;
    if (!LoadMesh(path, false, mesh)) return 2;
    std::cout << "bounds " << mesh.MinBounds()[0] << ',' << mesh.MinBounds()[1] << ',' << mesh.MinBounds()[2]
              << " .. " << mesh.MaxBounds()[0] << ',' << mesh.MaxBounds()[1] << ',' << mesh.MaxBounds()[2] << '\n';
    const bool grounded = Ground(mesh, x, y, z);
    PlayerController::State player;
    PlayerController::SetPose(player, x, y, z, yaw, grounded);
    PlayerController::SetRespawnPoint(player);
    PlayerController::InputSnapshot input;
    input.forward = 1;
    const PlayerController::SimulationContext context{ &mesh };
    for (int frame = 0; frame < 600; ++frame)
    {
        const float before[3] = { player.position[0], player.position[1], player.position[2] };
        const auto result = PlayerController::UpdateMovement(player, input, 1.0f / 60, context);
        float floorY = 0, normal[3] = {};
        const bool floor = ZoneCollision::FindFloorAt(mesh.Triangles(), mesh.Index(), PlayerController::kCollisionRadius,
            before[0], before[2], before[1] - 3, before[1] + 200, &floorY, normal);
        std::cout << frame << ' ' << player.position[0] << ',' << player.position[1] << ',' << player.position[2]
                  << " ground=" << player.onGround << " vy=" << player.verticalVelocity
                  << " floorBelow=" << (floor ? floorY : NAN) << " n=" << normal[0] << ',' << normal[1] << ',' << normal[2]
                  << (result.respawned ? " RESPAWNED" : "") << '\n';
        if (result.respawned) break;
    }
    return 0;
}

int Audit(const std::filesystem::path& root, const int zoneId, const char* label, const bool mirror, int& failures)
{
    const auto* zone = FFXIZone::FindByID(zoneId);
    const auto path = (root / zone->modelDat).string();
    ZoneCollision::Mesh mesh;
    if (!LoadMesh(path, mirror, mesh)) { std::cout << label << " NO_COLLISION " << path << '\n'; return ++failures; }

    // Probe 1: placement crossings (deterministic sample, at most 1500 placements).
    std::map<std::string, Outcome> byCategory;
    std::map<std::string, int> softBlocked; // grass/flower names that block
    const size_t objects = gFF11LastMapObjects.size();
    const size_t stride = (std::max)(size_t(1), objects / 1500);
    for (size_t i = 0; i < objects; i += stride)
    {
        const auto& object = gFF11LastMapObjects[i];
        if (object.roomObject) continue;
        float center[3];
        FFXICoordinateFrame::NativeDatToScene(object.trans, mirror, center);
        const std::string category = Category(object.objectName);
        static const float dirs[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
        for (const auto& d : dirs)
        {
            const int r = Cross(mesh, center, d[0], d[1]);
            if (!Env("WALK_DUMP").empty())
                std::cout << "X " << i << ' ' << d[0] << ',' << d[1] << ' ' << r << ' ' << object.objectName << ' '
                          << center[0] << ',' << center[1] << ',' << center[2] << '\n';
            auto& o = byCategory[category];
            (r == 0 ? o.crossed : r == 1 ? o.blocked : r == 2 ? o.fell : o.noFloor)++;
            if ((category == "grass" || category == "flower") && r == 1 && softBlocked[object.objectName]++ < 2)
                std::cout << label << " SOFT_BLOCK " << object.objectName << " at " << center[0] << ','
                          << center[1] << ',' << center[2] << " dir " << d[0] << ',' << d[1] << '\n';
        }
    }

    // Probe 2: wander from reachable seeds.
    std::vector<std::array<float, 3>> seeds;
    for (const auto& line : ZoneTransition::lines)
        if (line.toZone == zoneId) seeds.push_back({ line.arrival[0], line.arrival[1], line.arrival[2] });
    const size_t arrivals = seeds.size();
    if (FFXINpcPlacement::LoadCatalogForZone(zoneId, gCatalog.string().c_str()))
        for (const auto& placement : FFXINpcPlacement::Snapshot())
            seeds.push_back({ placement.transform.x, placement.transform.y, placement.transform.z });
    FFXINpcPlacement::Clear();
    std::mt19937 rng(1234 + zoneId);
    std::shuffle(seeds.begin() + arrivals, seeds.end(), rng);
    if (seeds.size() > 40) seeds.resize((std::max)(arrivals, size_t(40)));
    int agents = 0, ungrounded = 0, legs = 0, respawns = 0, longFalls = 0, stuckLegs = 0, zoned = 0;
    for (auto seed : seeds)
    {
        float x, y, z;
        { const auto s = FFXICoordinateFrame::NativeDatToScene({ seed[0], seed[1], seed[2] }, mirror); x = s[0]; y = s[1] - 0.5f; z = s[2]; }
        if (!Ground(mesh, x, y, z)) { ++ungrounded; continue; }
        ++agents;
        PlayerController::State player;
        PlayerController::SetPose(player, x, y, z, 0, true);
        PlayerController::SetRespawnPoint(player);
        const PlayerController::SimulationContext context{ &mesh };
        std::vector<std::array<float, 6>> drops; // ground before a >2 unit drop -> landing
        std::vector<std::array<float, 3>> trail;  // sampled path, for the first exits only
        float airStart[3] = {};
        for (int leg = 0; leg < 40; ++leg, ++legs)
        {
            PlayerController::InputSnapshot input;
            input.forward = 1;
            player.yaw = std::uniform_real_distribution<float>(0, 6.2831853f)(rng);
            const float legStart[3] = { player.position[0], player.position[1], player.position[2] };
            float airborneFrom = 0, lastGround[3] = { legStart[0], legStart[1], legStart[2] };
            bool airborne = false;
            for (int frame = 0; frame < 180; ++frame)
            {
                const float before[3] = { player.position[0], player.position[1], player.position[2] };
                const auto result = PlayerController::UpdateMovement(player, input, 1.0f / 60, context);
                bool hitZoneLine = false;
                if (!result.respawned)
                {
                    const auto a = FFXICoordinateFrame::SceneToNativeDat({ before[0], before[1], before[2] }, mirror);
                    const auto b = FFXICoordinateFrame::SceneToNativeDat({ player.position[0], player.position[1], player.position[2] }, mirror);
                    for (const auto& volume : gVolumes) hitZoneLine |= ZoneTriggerVolume::Hit(volume, a, b);
                }
                if (hitZoneLine)
                {
                    // The app zones out here; restart this agent from its seed.
                    ++zoned;
                    PlayerController::SetPose(player, x, y, z, 0, true);
                    break;
                }
                if (result.respawned)
                {
                    if (++respawns <= 6)
                        std::cout << label << " LEFT_WORLD last ground " << lastGround[0] << ',' << lastGround[1] << ','
                                  << lastGround[2] << " yaw " << player.yaw << " (native x " << (mirror ? -lastGround[0] : lastGround[0]) << ")\n";
                    if (respawns <= 6)
                        for (size_t d = drops.size() > 3 ? drops.size() - 3 : 0; d < drops.size(); ++d)
                            std::cout << label << "     earlier drop " << drops[d][0] << ',' << drops[d][1] << ',' << drops[d][2]
                                      << " -> " << drops[d][3] << ',' << drops[d][4] << ',' << drops[d][5] << '\n';
                    if (respawns <= 2)
                    {
                        std::cout << label << "     trail";
                        for (size_t t = trail.size() > 40 ? trail.size() - 40 : 0; t < trail.size(); ++t)
                            std::cout << " (" << trail[t][0] << ',' << trail[t][1] << ',' << trail[t][2] << ')';
                        std::cout << '\n';
                    }
                    drops.clear();
                    trail.clear();
                    break;
                }
                if (frame % 30 == 0) trail.push_back({ player.position[0], player.position[1], player.position[2] });
                if (player.onGround)
                {
                    if (airborne && player.position[1] - airStart[1] > 2)
                        drops.push_back({ airStart[0], airStart[1], airStart[2], player.position[0], player.position[1], player.position[2] });
                    airborne = false;
                    for (int a = 0; a < 3; ++a) lastGround[a] = player.position[a];
                }
                else if (!airborne) { airborne = true; airborneFrom = player.position[1]; for (int a = 0; a < 3; ++a) airStart[a] = lastGround[a]; }
                if (airborne && player.position[1] - airborneFrom > 30)
                {
                    if (++longFalls <= 6)
                        std::cout << label << " LONG_DROP>30 from " << lastGround[0] << ',' << lastGround[1] << ','
                                  << lastGround[2] << " yaw " << player.yaw << '\n';
                    airborne = false; airborneFrom = 1e9f;
                }
            }
            if (std::hypot(player.position[0] - legStart[0], player.position[2] - legStart[2]) < 0.5f) ++stuckLegs;
        }
    }
    std::cout << label << " zone=" << zoneId << " dat=" << zone->modelDat << " mirror=" << mirror
              << " triangles=" << mesh.Triangles().size() << " placements=" << objects << '\n';
    for (const auto& [category, o] : byCategory)
        std::cout << label << "   cross " << category << ": crossed=" << o.crossed << " blocked=" << o.blocked
                  << " fell=" << o.fell << " nofloor=" << o.noFloor << '\n';
    std::cout << label << "   blocked-by face |ny| <0.1=" << gBlockSlope[0] << " 0.1-0.35=" << gBlockSlope[1]
              << " 0.35-0.45=" << gBlockSlope[2] << " none=" << gBlockSlope[3] << '\n';
    for (int& b : gBlockSlope) b = 0;
    std::cout << label << "   seeds=" << seeds.size() << " (arrivals=" << arrivals << ") agents=" << agents
              << " ungrounded_seeds=" << ungrounded << " legs=" << legs << " left_world=" << respawns
              << " long_drops=" << longFalls << " zoned=" << zoned << " blocked_legs=" << stuckLegs << '\n';
    if (respawns) ++failures;
    return 0;
}
}

int main(int argc, char** argv)
{
    if (argc < 2) return 2;
    const std::filesystem::path root(argv[1]);
    gCatalog = std::filesystem::path(__FILE__).parent_path().parent_path() / "DATura/npc_placements.csv";
    if (argc == 6 && std::string(argv[2]) == "--where")
    {
        // Lists authored collision meshes and placements around a native x,z.
        ZoneCollision::Mesh mesh;
        if (!LoadMesh((root / argv[3]).string(), false, mesh)) return 2;
        const float x = std::stof(argv[4]), z = std::stof(argv[5]);
        for (const auto& m : gFF11LastCollisionMeshes)
            if (x >= m.boundsMin[0] - 2 && x <= m.boundsMax[0] + 2 && z >= m.boundsMin[2] - 2 && z <= m.boundsMax[2] + 2)
                std::cout << "mesh " << m.displayName << " tris=" << m.triCount << " flags=" << std::hex << m.bucketFlags
                          << std::dec << " bounds " << m.boundsMin[0] << ',' << m.boundsMin[1] << ',' << m.boundsMin[2]
                          << " .. " << m.boundsMax[0] << ',' << m.boundsMax[1] << ',' << m.boundsMax[2] << '\n';
        for (const auto& m : gFF11LastCollisionMeshes)
            if (m.triCount <= 32 && x >= m.boundsMin[0] - 2 && x <= m.boundsMax[0] + 2 && z >= m.boundsMin[2] - 2 && z <= m.boundsMax[2] + 2)
                for (int t = m.triStart; t < m.triStart + m.triCount; ++t)
                {
                    const auto& tri = gFF11LastCollisionTriangles[t];
                    std::cout << "  " << m.displayName << " tri";
                    for (int v = 0; v < 3; ++v) std::cout << " (" << tri.p[v][0] << ',' << tri.p[v][1] << ',' << tri.p[v][2] << ' f' << int(tri.indexFlags[v]) << ')';
                    std::cout << '\n';
                }
        for (const auto& o : gFF11LastMapObjects)
            if (std::fabs(o.trans[0] - x) < 12 && std::fabs(o.trans[2] - z) < 12)
                std::cout << "object " << o.objectName << " room=" << o.roomObject << " at " << o.trans[0] << ','
                          << o.trans[1] << ',' << o.trans[2] << " vcol=" << o.visualCollisionCount << '\n';
        return 0;
    }
    if (argc == 7 && std::string(argv[2]) == "--walls")
    {
        // Wall faces a player standing at scene x,y,z would test against.
        ZoneCollision::Mesh mesh;
        if (!LoadMesh((root / argv[3]).string(), false, mesh)) return 2;
        const float p[3] = { std::stof(argv[4]), std::stof(argv[5]), std::stof(argv[6]) };
        std::vector<int> candidates;
        mesh.Index().Query(p[0], p[2], PlayerController::kCollisionRadius + 0.5f, candidates);
        for (int i : candidates)
        {
            const auto& t = mesh.Triangles()[i];
            if (std::fabs(t.normal[1]) > 0.45f || p[1] - 0.001f < t.minY || p[1] - PlayerController::kCollisionHeight > t.maxY ||
                !ZoneCollision::PointNearTriangleXZ(p[0], p[2], t, PlayerController::kCollisionRadius + 0.3f)) continue;
            std::cout << "wall " << i << " n=" << t.normal[0] << ',' << t.normal[1] << ',' << t.normal[2]
                      << " y=" << t.minY << ".." << t.maxY << " short=" << (t.minY >= p[1] - PlayerController::kStepHeight);
            for (int v = 0; v < 3; ++v) std::cout << " (" << t.p[v][0] << ',' << t.p[v][1] << ',' << t.p[v][2] << ')';
            std::cout << '\n';
        }
        return 0;
    }
    if (argc == 8 && std::string(argv[2]) == "--trace")
        return Trace((root / argv[3]).string(), std::stof(argv[4]), std::stof(argv[5]), std::stof(argv[6]), std::stof(argv[7]));
    const std::pair<int, const char*> zones[] = {
        { 108, "Konschtat" }, { 102, "LaTheine" }, { 100, "WRonfaure" }, { 24, "Lufaise" }, { 248, "Selbina" },
        { 236, "PortBastok" }, { 246, "PortJeuno" }, { 235, "BastokMkts" }, { 237, "Metalworks" }, { 233, "Chateau" } };
    int failures = 0;
    for (const auto& [id, label] : zones)
        if (Env("WALK_ZONE").empty() || std::atoi(Env("WALK_ZONE").c_str()) == id)
        for (bool mirror : { false, true })
        {
            if (mirror && argc > 2) continue; // any extra argument: native orientation only
            Audit(root, id, label, mirror, failures);
            std::cout.flush();
        }
    std::cout << (failures ? "FAIL" : "PASS") << ": collision walkthrough zones-with-world-exits=" << failures << '\n';
    return failures ? 1 : 0;
}
