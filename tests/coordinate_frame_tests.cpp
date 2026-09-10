#include "stdafx.h"
#include "ffxi_coordinate_frame.h"
#include "noesis_rapi.h"
#include "model_ff11.h"
#include "model_ff11_water.h"
#include "zone_model_transform.h"
#include "zone_collision_geometry.h"
#include "zone_room_loader.h"
#include "ffxi_file_io.h"
#include "npc_placement.h"
#include "d3d_math.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>

namespace
{
int failures = 0;
using FFXICoordinateFrame::Vector;
void Check(bool condition, const char* message)
{
    if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
bool Near(float first, float second, float epsilon = 0.0001f)
{
    return std::fabs(first - second) <= epsilon;
}
bool Same(const float* first, const float* second)
{
    return Near(first[0], second[0]) && Near(first[1], second[1]) && Near(first[2], second[2]);
}

void TestBasisAndHeading()
{
    const Vector basis[] = {{1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}, {12,-3,27}};
    for (bool mirror : {false, true})
    {
        for (const Vector& native : basis)
        {
            const auto scene = FFXICoordinateFrame::NativeDatToScene(native, mirror);
            Check(scene[0] == (mirror ? -native[0] : native[0]) &&
                  scene[1] == native[1] && scene[2] == native[2], "axis directions preserve native Y and Z");
            Check(FFXICoordinateFrame::SceneToNativeDat(scene, mirror) == native, "frame conversion is invertible");
            float alias[3] = {native[0], native[1], native[2]};
            FFXICoordinateFrame::NativeDatToScene(alias, mirror, alias);
            Check(Same(alias, scene.data()), "point/normal/displacement conversion supports in-place storage");
            FFXICoordinateFrame::SceneToNativeDat(alias, mirror, alias);
            Check(Same(alias, native.data()), "in-place inverse recovers the native query point");
        }
        for (int rotation = -256; rotation <= 512; ++rotation)
        {
            const float heading = rotation * (6.28318530718f / 256.0f);
            const float scene = FFXICoordinateFrame::NativeDatHeadingToScene(heading, mirror);
            Check(Near(std::cos(scene), std::cos(heading) * (mirror ? -1.f : 1.f)) &&
                  Near(-std::sin(scene), -std::sin(heading)), "catalog heading reflects its actual +X-facing direction");
            const float native = FFXICoordinateFrame::SceneHeadingToNativeDat(scene, mirror);
            Check(Near(std::sin(native), std::sin(heading)) && Near(std::cos(native), std::cos(heading)),
                  "heading round trip preserves facing across angle wrap");
        }
    }
}

void TestTriangleAndModelReflection()
{
    // A slope with every normal component nonzero catches winding errors that
    // horizontal-only tests miss. Geometry normals must match stored normals.
    const float points[3][3] = {{12,-3,27}, {15,-2,27}, {12,-1,31}};
    ZoneCollision::Triangle native, scene;
    Check(ZoneCollision::BuildTriangle(&points[0][0], false, native), "native slope is valid");
    Check(ZoneCollision::BuildTriangle(&points[0][0], true, scene), "reflected slope is valid");
    const auto expectedNormal = FFXICoordinateFrame::NativeDatToScene(
        {native.normal[0], native.normal[1], native.normal[2]}, true);
    Check(Same(scene.normal, expectedNormal.data()), "reflection plus winding preserves normal orientation");
    Check(scene.minX == -native.maxX && scene.maxX == -native.minX &&
          scene.minY == native.minY && scene.maxZ == native.maxZ, "reflected triangle bounds stay ordered");

    noesisModel_t model;
    model.submeshes.emplace_back();
    auto& mesh = model.submeshes.front();
    mesh.cpuVerts.resize(3);
    mesh.cpuIndices = {0,1,2};
    mesh.windDisplacements = {{{1,2,3}}, {{-2,1,4}}, {{3,-4,1}}};
    mesh.water = std::make_shared<ZoneWater::Surface>();
    mesh.water->uvVelocity[1] = -0.12f;
    mesh.zoneLod.enabled = true;
    mesh.zoneLod.origin[0] = 12;
    mesh.zoneLod.origin[1] = -3;
    mesh.zoneLod.origin[2] = 27;
    for (int i = 0; i < 3; ++i)
    {
        memcpy(mesh.cpuVerts[i].pos, points[i], sizeof(points[i]));
        memcpy(mesh.cpuVerts[i].nrm, native.normal, sizeof(native.normal));
    }
    mesh.cpuBindVerts = mesh.cpuVerts;
    const auto originalVertices = mesh.cpuVerts;
    const auto originalWind = mesh.windDisplacements;
    ZoneModelTransform::MirrorOnX(&model, nullptr);
    Check(mesh.cpuIndices == std::vector<DWORD>({0,2,1}), "model reflection reverses triangle winding once");
    for (int i = 0; i < 3; ++i)
    {
        Check(Same(mesh.cpuVerts[mesh.cpuIndices[i]].pos, scene.p[i]), "render and collision triangles align after reflection");
        Check(Same(mesh.cpuVerts[i].nrm, expectedNormal.data()), "stored render normals match reflected geometry");
        Check(Same(mesh.cpuVerts[i].pos, mesh.cpuBindVerts[i].pos), "animation bind vertices use the same scene frame");
        const auto wind = FFXICoordinateFrame::NativeDatToScene(originalWind[i], true);
        Check(mesh.windDisplacements[i] == wind, "vegetation displacement follows scene reflection");
    }
    Check(mesh.zoneLod.origin[0] == 12 && Near(mesh.water->uvVelocity[1], -0.12f),
          "native LOD metadata and authored texture flow are not reflected twice");
    Check(mesh.hasBounds && mesh.boundsMin[0] == -15 && mesh.boundsMax[0] == -12,
          "model reflection rebuilds CPU bounds");
    ZoneModelTransform::MirrorOnX(&model, nullptr);
    Check(mesh.cpuIndices == std::vector<DWORD>({0,1,2}) && mesh.windDisplacements == originalWind,
          "second model reflection restores winding and wind");
    for (int i = 0; i < 3; ++i)
        Check(Same(mesh.cpuVerts[i].pos, originalVertices[i].pos) && Same(mesh.cpuVerts[i].nrm, originalVertices[i].nrm),
              "second model reflection restores vertex positions and normals");
}

void TestPlacedGeometry()
{
    ff11GeneratorRecord_t generator = {};
    generator.hasRotation = generator.hasScale = generator.hasSpawnPosition = true;
    for (int axis = 0; axis < 3; ++axis) generator.rotation[axis] = 1.57079632679f;
    generator.scale[0] = 2; generator.scale[1] = 3; generator.scale[2] = 4;
    generator.spawnPosition[0] = 11; generator.spawnPosition[1] = -13; generator.spawnPosition[2] = 17;
    const RichMat43 water = FF11Water::PlacementTransform(generator);
    const D3DMATRIX object = D3DMath::BuildScaleRotateTranslate(generator.scale, generator.rotation, generator.spawnPosition);
    const float local[3] = {1,2,3};
    float native[3] = {};
    for (int axis = 0; axis < 3; ++axis)
    {
        native[axis] = local[0] * water[0][axis] + local[1] * water[1][axis] + local[2] * water[2][axis] + water[3][axis];
        for (int row = 0; row < 4; ++row)
            Check(Near(water[row][axis], object.m[row][axis]), "water and object SRT agree for nonzero XYZ rotation and nonuniform scale");
    }
    const float expectedNative[3] = {23,-7,15}; // Scale, Rx, Ry, Rz, then translate.
    Check(Same(native, expectedNative), "authored placement applies every transform exactly once in native space");
    float scene[3];
    FFXICoordinateFrame::NativeDatToScene(native, true, scene);
    const float expectedScene[3] = {-23,-7,15};
    Check(Same(scene, expectedScene), "scene conversion follows completed placement instead of preceding rotation");
    generator.scale[1] = 0;
    const auto collapsed = FF11Water::PlacementTransform(generator);
    Check(collapsed[1][0] == 0 && collapsed[1][1] == 0 && collapsed[1][2] == 0,
          "authored zero water scale remains valid in the shared coordinate contract");
}

struct VertexSample
{
    size_t mesh, vertex;
    FFXIVertex native;
};

void TestInstalledZone(const std::filesystem::path& root)
{
    const std::string path = (root / "ROM/1/35.DAT").string();
    BYTE* raw = nullptr; DWORD size = 0;
    Check(FFXIFileIO::ReadWholeFile(path.c_str(), &raw, &size), "read installed Bastok Markets");
    if (!raw) return;
    std::unique_ptr<BYTE[]> bytes(raw);
    noeRAPI_t rapi(nullptr);
    rapi.SetCurrentFilePath(path.c_str());
    ff11Opts_t options = {};
    options.collectCollision = true;
    options.collectCollisionUnreferenced = true;
    options.renderWater = true;
    gpFF11Opts = &options;
    gFF11LastCollisionTriangles.clear();
    gFF11LastCollisionMeshes.clear();
    int count = 0;
    auto* model = Model_FF11_LoadDAT(bytes.get(), static_cast<int>(size), count, &rapi);
    Check(model != nullptr, "parse native city geometry");
    if (!model) { gpFF11Opts = nullptr; return; }
    Check(ZoneRoomLoader::Append(model, &rapi, path.c_str()) == 14, "append all fourteen authored room DATs before scene reflection");

    std::vector<VertexSample> samples;
    size_t waterMeshes = 0, roomMeshes = 0;
    for (size_t i = 0; i < model->submeshes.size(); ++i)
    {
        const auto& mesh = model->submeshes[i];
        waterMeshes += mesh.water ? 1 : 0;
        roomMeshes += mesh.objectName.find(": room ") != std::string::npos ? 1 : 0;
        if (!mesh.cpuVerts.empty())
            samples.push_back({i, mesh.cpuVerts.size() / 2, mesh.cpuVerts[mesh.cpuVerts.size() / 2]});
    }
    Check(waterMeshes > 0 && roomMeshes > 0, "real scene includes water and companion-room vertices");

    // Room visual collision is exported from the placed triangles. Validate
    // its coordinates against the actual rendered mesh, before any reflection.
    size_t alignedRooms = 0;
    for (const auto& object : gFF11LastMapObjects)
    {
        if (!object.roomObject || !object.visualCollisionCount ||
            object.visualCollisionStart >= gFF11LastCollisionTriangles.size()) continue;
        const auto& triangle = gFF11LastCollisionTriangles[object.visualCollisionStart];
        bool matches[3] = {};
        for (const auto& mesh : model->submeshes)
            if (mesh.objectName == object.displayName)
                for (const auto& vertex : mesh.cpuVerts)
                    for (int i = 0; i < 3; ++i) matches[i] = matches[i] || Same(vertex.pos, triangle.p[i]);
        Check(matches[0] && matches[1] && matches[2], "room collision and rendered placement share native coordinates");
        if (++alignedRooms == 16) break;
    }
    Check(alignedRooms > 0, "room collision-to-render alignment is exercised");

    const auto catalog = std::filesystem::path(__FILE__).parent_path().parent_path() / "DATura/npc_placements.csv";
    Check(FFXINpcPlacement::LoadCatalogForZone(235, catalog.string().c_str()), "load real Bastok NPC placements");
    const auto placements = FFXINpcPlacement::Snapshot();
    std::vector<float> nativeFloor(placements.size(), 0);
    std::vector<bool> hasNativeFloor(placements.size(), false);
    size_t grounded = 0;
    for (bool mirror : {false, true})
    {
        ZoneCollision::Mesh collision;
        collision.Reserve(static_cast<int>(gFF11LastCollisionTriangles.size()));
        for (const auto& rawTriangle : gFF11LastCollisionTriangles)
        {
            ZoneCollision::Triangle triangle;
            if (ZoneCollision::BuildTriangle(&rawTriangle.p[0][0], mirror, triangle))
                collision.AddTriangle(triangle, 0.4f);
        }
        for (size_t i = 0; i < placements.size(); ++i)
        {
            const auto& native = placements[i].transform;
            const auto scene = FFXINpcPlacement::ToSceneTransform(native, mirror);
            const auto expected = FFXICoordinateFrame::NativeDatToScene({native.x, native.y, native.z}, mirror);
            Check(scene.x == expected[0] && scene.y == expected[1] && scene.z == expected[2],
                  "catalog NPCs use the same scene frame as geometry and collision");
            float floor = 0;
            const bool found = ZoneCollision::FindFloorAt(collision.Triangles(), collision.Index(), 0.4f,
                scene.x, scene.z, scene.y - 1.4f, scene.y + 12.0f, &floor, nullptr);
            if (!mirror)
            {
                hasNativeFloor[i] = found;
                nativeFloor[i] = floor;
                grounded += found ? 1 : 0;
            }
            else
                Check(found == hasNativeFloor[i] && (!found || Near(floor, nativeFloor[i], 0.001f)),
                      "NPC grounding finds identical authored floors in both scene orientations");
        }
    }
    Check(grounded >= 20, "grounding comparison covers real city NPCs instead of only empty queries");

    ZoneModelTransform::MirrorOnX(model, nullptr);
    for (const auto& sample : samples)
    {
        const auto& scene = model->submeshes[sample.mesh].cpuVerts[sample.vertex];
        float position[3], normal[3];
        FFXICoordinateFrame::NativeDatToScene(sample.native.pos, true, position);
        FFXICoordinateFrame::NativeDatToScene(sample.native.nrm, true, normal);
        Check(Same(scene.pos, position) && Same(scene.nrm, normal),
              "all installed terrain, room and water meshes receive the identical final scene reflection");
    }
    std::cout << "Bastok Markets: " << samples.size() << " mesh samples, " << alignedRooms
              << " room collision checks, " << grounded << '/' << placements.size() << " grounded NPCs\n";
    FFXINpcPlacement::Clear();
    gpFF11Opts = nullptr;
}
}

int main(int argc, char** argv)
{
    TestBasisAndHeading();
    TestTriangleAndModelReflection();
    TestPlacedGeometry();
    if (argc > 1) TestInstalledZone(argv[1]);
    else std::cout << "Installed-DAT checks skipped: pass the FFXI installation root to enable them.\n";
    std::cout << (failures ? "FAIL" : "PASS") << ": coordinate frame checks\n";
    return failures ? 1 : 0;
}
