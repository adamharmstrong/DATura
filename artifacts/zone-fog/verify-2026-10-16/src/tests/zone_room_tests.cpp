#include "stdafx.h"
#include "noesis_rapi.h"
#include "model_ff11.h"
#include "zone_room_catalog.h"
#include "zone_room_loader.h"
#include "ffxi_file_io.h"
#include "zone_object_visibility.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>

namespace
{
int failures = 0;
void Check(bool condition, const char* message)
{
    if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}

void TestCatalog()
{
    const auto rooms = ZoneRoomCatalog::Resolve("C:\\game\\ROM\\1\\35.DAT");
    Check(rooms.size() == 14, "Markets has fourteen companions");
    Check(rooms.front().path == "C:\\game\\ROM\\1\\61.DAT" &&
          rooms.back().path == "C:\\game\\ROM\\1\\74.DAT", "Markets room paths");
    Check(rooms.front().rootName == "r_2b", "Markets validates the room group");
    Check(rooms.front().subAreaId == 274 && rooms.back().subAreaId == 287,
          "room sub-area IDs differ from physical file numbers");
    Check(ZoneRoomCatalog::Resolve("rom/1/34.dat").size() == 13, "Mines room group");
    Check(ZoneRoomCatalog::Resolve("rom/1/36.dat").size() == 9, "Port room group");
    Check(ZoneRoomCatalog::Resolve("rom/1/37.dat").size() == 19, "Metalworks room group");
    Check(ZoneRoomCatalog::Resolve("C:/game/ROM5/0/7.DAT").empty(), "past Markets is separate");
    Check(ZoneRoomCatalog::Resolve("C:/game/otherrom/1/35.DAT").empty(), "path segment boundary");
    Check(ZoneRoomCatalog::Resolve("C:/game/ROM/1/61.DAT").empty(), "opening a room does not recurse");
    Check(ZoneRoomCatalog::Resolve("ROM/0/79.DAT").size() == 7,
          "Walls includes the authored environment-only room reference");
    const auto north = ZoneRoomCatalog::Resolve("C:/game/ROM/1/32.DAT");
    Check(north.size() == 13 && north[7].path == "C:/game/ROM/2/0.DAT", "rooms cross a DAT folder boundary");
    const auto south = ZoneRoomCatalog::Resolve("C:/game/ROM/1/31.DAT");
    Check(south.size() == 18 && south[8].subAreaId == 335, "sparse replacement IDs are preserved");
    const auto adoulin = ZoneRoomCatalog::Resolve("C:/game/ROM9/0/3.DAT");
    Check(adoulin.size() == 4 && adoulin.front().path == "C:/game/ROM9/5/49.DAT", "Adoulin trigger-only rooms live in another folder");
    Check(ZoneRoomCatalog::Resolve("C:/game/ROM/1/43.DAT").size() == 5, "Selbina root name does not alias San d'Oria");
    Check(ZoneRoomCatalog::Resolve("35.DAT").empty(), "unidentified standalone DAT is not guessed");
}

void TestLod()
{
    const int available[] = { 10, 20, 30 };
    Check(ZoneLod::ResolveLevel(0, available) == 10 && ZoneLod::ResolveLevel(2, available) == 30,
          "LOD selects available high and low resources");
    const int missing[] = { 10, -1, -1 };
    Check(ZoneLod::ResolveLevel(1, missing) == 10 && ZoneLod::ResolveLevel(2, missing) == 10,
          "missing medium and low levels fall back without holes");
    const int missingHigh[] = { -1, 20, 30 };
    Check(ZoneLod::ResolveLevel(0, missingHigh) == 20, "missing high level falls back to medium");
    const char high[17] = "tree_h          ", medium[17] = "tree_m          ";
    Check(ZoneLod::SameFamily(high, medium), "alternate LOD is referenced, not an environment object");
    ZoneLod::Data lod;
    lod.enabled = true;
    lod.highDistance = 30;
    lod.midDistance = 100;
    lod.drawDistance = 120;
    for (float distance : { 0.0f, 29.9f, 30.0f, 99.9f, 100.0f, 120.0f, 120.1f })
    {
        const float viewer[] = { distance, 0, 0 };
        int visibleCount = 0;
        for (int level = 0; level < 3; ++level)
        {
            lod.levelMask = 1 << level;
            const bool visible = ZoneLod::Visible(lod, viewer);
            visibleCount += visible;
            Check(visible == (distance <= 120 && level == (distance < 30 ? 0 : distance < 100 ? 1 : 2)),
                  "LOD boundaries select the expected single level");
        }
        Check(visibleCount == (distance > 120 ? 0 : 1), "LOD levels never overlap");
    }
    lod.drawDistance = 0;
    lod.levelMask = 7;
    const float distantViewer[] = { 10000, 0, 0 };
    Check(ZoneLod::Visible(lod, distantViewer), "zero draw distance does not cull rooms");
    // Equal object/material names must not coalesce different LOD geometry.
    noeRAPI_t rapi(nullptr);
    void* context = rapi.rpgCreateContext();
    char objectName[] = "000: tree_m";
    char materialName[] = "bark";
    rapi.rpgSetName(objectName);
    rapi.rpgSetMaterial(materialName);
    for (int level = 0; level < 3; ++level)
    {
        lod.levelMask = 1 << level;
        rapi.rpgSetZoneLod(lod);
        rapi.rpgBegin(RPGEO_TRIANGLE);
        float points[3][3] = { {0, 0, 0}, {1, 0, 0}, {0, 1, 0} };
        for (auto& point : points) rapi.rpgVertex3f(point);
        rapi.rpgEnd();
    }
    auto* model = rapi.rpgConstructModel();
    Check(model && model->submeshes.size() == 3 && model->hasZoneLod,
          "accumulator keeps LOD variants separate");
    rapi.rpgDestroyContext(context);
}

void TestInstalledZone(const std::filesystem::path& root, const char* relative, int expectedRooms,
                       IDirect3DDevice9* device)
{
    const std::string path = (root / relative).string();
    BYTE* raw = nullptr; DWORD size = 0;
    Check(FFXIFileIO::ReadWholeFile(path.c_str(), &raw, &size), "read installed zone");
    if (!raw) return;
    std::unique_ptr<BYTE[]> bytes(raw);
    noeRAPI_t rapi(device);
    rapi.SetCurrentFilePath(path.c_str());
    ff11Opts_t options = {};
    options.collectCollision = true;
    options.collectCollisionUnreferenced = true;
    options.renderEnvironment = true;
    gpFF11Opts = &options;
    gFF11LastCollisionTriangles.clear();
    gFF11LastCollisionMeshes.clear();
    int count = 0;
    auto* zone = Model_FF11_LoadDAT(bytes.get(), static_cast<int>(size), count, &rapi);
    Check(zone != nullptr, "load base zone");
    if (!zone) { gpFF11Opts = nullptr; return; }
    const auto baseObjects = gFF11LastMapObjects.size();
    const auto baseMeshes = zone->submeshes.size();
    const auto baseCollision = gFF11LastCollisionTriangles.size();
    const auto baseChunks = gFF11LastDatChunks.size();
    const auto baseEnvironment = gFF11LastEnvironmentRecords.size();
    const auto baseVisibility = gFF11LastZoneVisibilityRecords.size();
    const auto baseHeader = gFF11LastMapHeader;
    if (!expectedRooms)
    {
        Check(ZoneRoomCatalog::Resolve(path).empty(), "audited embedded city has no companion catalog");
        Check(ZoneRoomLoader::Append(zone, &rapi, path.c_str()) == 0 && zone->submeshes.size() == baseMeshes,
              "embedded city geometry is left intact");
        std::cout << relative << ": embedded city, meshes=" << baseMeshes << '\n';
        gpFF11Opts = nullptr;
        return;
    }
    std::set<unsigned int> roomIds;
    for (const auto& room : ZoneRoomCatalog::Resolve(path)) roomIds.insert(room.subAreaId);
    const auto expectedProxies = std::count_if(gFF11LastMapObjects.begin(), gFF11LastMapObjects.end(),
        [&](const auto& object) { return roomIds.count(object.data2[3]) != 0; });
    const std::string firstObject = gFF11LastMapObjects.front().displayName;
    // Missing companions must retain proxies, even when a valid main DAT has room links.
    const auto missingRoot = std::filesystem::temp_directory_path() /
        ("datura-missing-room-" + std::to_string(GetCurrentProcessId()));
    const auto missingZone = (missingRoot / relative).string();
    Check(ZoneRoomLoader::Append(zone, &rapi, missingZone.c_str()) == 0,
          "missing room files keep the outdoor proxy");
    Check(zone->submeshes.size() == baseMeshes &&
          std::none_of(gFF11LastMapObjects.begin(), gFF11LastMapObjects.end(),
                       [](const auto& object) { return object.replacedByRoom; }),
          "no proxy is removed before a room successfully loads");
    const int rooms = ZoneRoomLoader::Append(zone, &rapi, path.c_str());
    Check(rooms == expectedRooms, "all installed companions load");
    Check(zone->submeshes.size() > baseMeshes, "room geometry is appended");
    Check(gFF11LastMapObjects.size() > baseObjects, "room objects are inspectable");
    Check(gFF11LastCollisionTriangles.size() > baseCollision, "room collision is appended");
    Check(gFF11LastDatChunks.size() == baseChunks &&
          gFF11LastEnvironmentRecords.size() == baseEnvironment &&
          gFF11LastZoneVisibilityRecords.size() == baseVisibility &&
          gFF11LastMapHeader.parsedObjectCount == baseHeader.parsedObjectCount,
          "primary environment, chunk inventory and visibility survive room parsing");
    Check(firstObject == gFF11LastMapObjects.front().displayName, "primary object identities survive");
    Check(gpFF11Opts == &options, "parser options restored");
    std::set<std::string> names;
    std::set<std::string> removedNames;
    for (size_t i = 0; i < gFF11LastMapObjects.size(); ++i)
    {
        const auto& object = gFF11LastMapObjects[i];
        if (object.replacedByRoom)
        {
            removedNames.insert(object.displayName);
            Check(object.visualCollisionCount == 0, "proxy visual collision geometry is released");
        }
        const bool unique = names.insert(object.displayName).second;
        if (i >= baseObjects) Check(unique, "room object identities are unique");
        if (i >= baseObjects)
            Check(object.roomObject && object.mapRecordIndex == i, "room indices do not alias outdoor visibility");
    }
    size_t unresolved = 0;
    size_t firstRoomMesh = zone->submeshes.size();
    size_t lodVariants = 0;
    Check(removedNames.size() == expectedProxies, "only authored outdoor proxies are retired");
    for (size_t i = 0; i < zone->submeshes.size(); ++i)
    {
        const auto& mesh = zone->submeshes[i];
        Check(removedNames.count(mesh.objectName) == 0, "replaced mesh is absent from CPU/GPU source geometry");
        lodVariants += mesh.zoneLod.enabled && mesh.zoneLod.levelMask != 7;
        if (mesh.objectName.find(": room ") == std::string::npos) continue;
        firstRoomMesh = std::min(firstRoomMesh, i);
        Check(names.count(mesh.objectName) != 0, "room mesh has a matching inspectable object");
        auto* material = zone->pMatData->FindMaterial(mesh.materialName.c_str());
        if (!material)
        {
            // An all-space DAT texture tag explicitly draws untextured geometry.
            if (mesh.materialName.compare(0, 16, std::string(16, ' ')) != 0)
                ++unresolved;
            continue;
        }
        Check(material->texIdx >= 0 && material->texIdx < zone->pMatData->texCount,
              "room material texture index points into combined texture table");
    }
    Check(unresolved == 0, "all room materials resolve");
    Check(firstRoomMesh < zone->submeshes.size(), "room geometry survives proxy removal");
    if (std::string(relative) == "ROM/1/35.DAT") Check(lodVariants > 0, "Markets contains loaded distance-selected LOD variants");
    std::vector<unsigned char> invisible(gFF11LastMapObjects.size(), 0);
    ZoneObjectVisibility::RenderContext visibility;
    visibility.viewerPointValid = true;
    visibility.visibleMapObjects = &invisible;
    Check(!ZoneObjectVisibility::PassesRenderVisibility(zone, zone->submeshes.front(), true, visibility),
          "outdoor objects still obey the zone visibility table");
    const auto& roomMesh = zone->submeshes[firstRoomMesh];
    Check(ZoneObjectVisibility::PassesRenderVisibility(zone, roomMesh, true, visibility),
          "outdoor visibility cannot hide room objects");
    std::vector<std::string> hidden = { roomMesh.objectName };
    visibility.hiddenNames = &hidden;
    Check(!ZoneObjectVisibility::PassesRenderVisibility(zone, roomMesh, true, visibility),
          "room objects can still be hidden manually");
    if (device)
    {
        Check(!zone->staticBufferGroups.empty(), "combined scene has GPU buffers");
        for (const auto& mesh : zone->submeshes)
        {
            Check(mesh.staticBufferGroupIndex >= 0 &&
                  mesh.staticBufferGroupIndex < zone->staticBufferGroups.size(), "GPU buffer group remapped");
            if (mesh.staticBufferGroupIndex >= 0 && mesh.staticBufferGroupIndex < zone->staticBufferGroups.size())
            {
                const auto& group = zone->staticBufferGroups[mesh.staticBufferGroupIndex];
                Check(group.pVB && group.pIB, "transferred GPU buffers remain alive");
                Check(mesh.staticVertexOffset + mesh.vertCount <= group.vertCount &&
                      mesh.staticStartIndex + mesh.triCount * 3 <= group.indexCount,
                      "room draw fits its transferred buffers");
            }
        }
        zone->ReleaseD3DBuffers();
        zone->BuildD3DBuffers(device);
        Check(!zone->staticBufferGroups.empty(), "combined buffers rebuild for device reset");
    }
    std::cout << relative << ": rooms=" << rooms
              << " meshes=" << baseMeshes << "->" << zone->submeshes.size()
              << " objects=" << baseObjects << "->" << gFF11LastMapObjects.size()
              << " collision=" << baseCollision << "->" << gFF11LastCollisionTriangles.size()
              << " unresolved-room-materials=" << unresolved << '\n';
    std::cout << "  replaced objects=" << removedNames.size() << " LOD variant meshes=" << lodVariants << '\n';

    // No room files, then a file with an unrelated root, must leave the model intact.
    const auto fixture = std::filesystem::temp_directory_path() /
        ("datura-room-test-" + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(fixture);
    const auto meshCount = zone->submeshes.size();
    Check(ZoneRoomLoader::Append(zone, &rapi, (fixture / relative).string().c_str()) == 0,
          "missing optional rooms are nonfatal");
    const auto candidates = ZoneRoomCatalog::Resolve((fixture / relative).string());
    std::filesystem::create_directories(std::filesystem::path(candidates.front().path).parent_path());
    { std::ofstream bad(candidates.front().path, std::ios::binary); bad.write("not a room DAT!!", 16); }
    Check(ZoneRoomLoader::Append(zone, &rapi, (fixture / relative).string().c_str()) == 0,
          "unrelated file is rejected");
    Check(zone->submeshes.size() == meshCount, "failed companions preserve the zone");
    std::filesystem::remove(candidates.front().path);
    gpFF11Opts = nullptr;
}
}

int main(int argc, char** argv)
{
    std::cout << std::unitbuf;
    TestCatalog();
    TestLod();
    IDirect3D9* d3d = nullptr;
    IDirect3DDevice9* device = nullptr;
    HWND window = nullptr;
    if (argc > 2 && std::string(argv[2]) == "--gpu")
    {
        window = CreateWindowExA(0, "STATIC", "DATura room tests", WS_POPUP,
                                 0, 0, 640, 480, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
        d3d = Direct3DCreate9(D3D_SDK_VERSION);
        D3DPRESENT_PARAMETERS pp = {};
        pp.Windowed = TRUE;
        pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        pp.hDeviceWindow = window;
        pp.BackBufferWidth = 640;
        pp.BackBufferHeight = 480;
        if (d3d && window)
            d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
                D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device);
        Check(device != nullptr, "create hidden D3D9 validation device");
    }
    if (argc > 1)
    {
        TestInstalledZone(argv[1], "ROM3/0/25.DAT", 0, device); // Tavnazian Safehold
        TestInstalledZone(argv[1], "ROM4/0/2.DAT", 0, device); // Al Zahbi
        TestInstalledZone(argv[1], "ROM4/0/3.DAT", 11, device); // Aht Urhgan Whitegate
        TestInstalledZone(argv[1], "ROM4/0/6.DAT", 1, device); // Nashmau
        TestInstalledZone(argv[1], "ROM5/0/0.DAT", 0, device); // Southern San d'Oria [S]
        TestInstalledZone(argv[1], "ROM5/0/7.DAT", 0, device); // Bastok Markets [S]
        TestInstalledZone(argv[1], "ROM5/0/14.DAT", 0, device); // Windurst Waters [S]
        TestInstalledZone(argv[1], "ROM/1/31.DAT", 18, device); // Southern San d'Oria
        TestInstalledZone(argv[1], "ROM/1/32.DAT", 13, device); // Northern San d'Oria
        TestInstalledZone(argv[1], "ROM/0/113.DAT", 6, device); // Port San d'Oria
        TestInstalledZone(argv[1], "ROM/1/33.DAT", 6, device); // Chateau d'Oraguille
        TestInstalledZone(argv[1], "ROM/1/34.DAT", 13, device); // Bastok Mines
        TestInstalledZone(argv[1], "ROM/1/35.DAT", 14, device); // Bastok Markets
        TestInstalledZone(argv[1], "ROM/1/36.DAT", 9, device); // Port Bastok
        TestInstalledZone(argv[1], "ROM/1/37.DAT", 19, device); // Metalworks
        TestInstalledZone(argv[1], "ROM/0/78.DAT", 21, device); // Windurst Waters
        TestInstalledZone(argv[1], "ROM/0/79.DAT", 6, device); // Windurst Walls
        TestInstalledZone(argv[1], "ROM/0/80.DAT", 7, device); // Port Windurst
        TestInstalledZone(argv[1], "ROM/0/81.DAT", 8, device); // Windurst Woods
        TestInstalledZone(argv[1], "ROM/1/38.DAT", 4, device); // Heavens Tower
        TestInstalledZone(argv[1], "ROM/1/39.DAT", 10, device); // Ru'Lude Gardens
        TestInstalledZone(argv[1], "ROM/1/40.DAT", 10, device); // Upper Jeuno
        TestInstalledZone(argv[1], "ROM/1/41.DAT", 13, device); // Lower Jeuno
        TestInstalledZone(argv[1], "ROM/1/42.DAT", 14, device); // Port Jeuno
        TestInstalledZone(argv[1], "ROM2/12/123.DAT", 0, device); // Rabao
        TestInstalledZone(argv[1], "ROM/1/43.DAT", 5, device); // Selbina
        TestInstalledZone(argv[1], "ROM/1/44.DAT", 6, device); // Mhaura
        TestInstalledZone(argv[1], "ROM2/0/25.DAT", 8, device); // Kazham
        TestInstalledZone(argv[1], "ROM2/0/27.DAT", 2, device); // Norg
        TestInstalledZone(argv[1], "ROM9/0/3.DAT", 4, device); // Western Adoulin
        TestInstalledZone(argv[1], "ROM9/0/4.DAT", 2, device); // Eastern Adoulin
    }
    else std::cout << "Installed-DAT checks skipped: pass the FFXI installation root to enable them.\n";
    if (device) device->Release();
    if (d3d) d3d->Release();
    if (window) DestroyWindow(window);
    std::cout << (failures ? "FAIL" : "PASS") << ": room loading checks\n";
    return failures ? 1 : 0;
}
