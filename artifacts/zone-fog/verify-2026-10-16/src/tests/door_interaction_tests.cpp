#include "stdafx.h"
#include "noesis_rapi.h"
#include "d3d_math.h"
#include "ffxi_file_io.h"
#include "model_ff11.h"
#include "zone_door_interaction.h"
#include "player_controller.h"
#include <array>
#include <iostream>
#include <memory>

int failures = 0, checks = 0;
void Check(bool condition, const char *message)
{
    ++checks;
    if (!condition)
    {
        ++failures;
        std::cerr << "FAIL: " << message << "\n";
    }
}
void AddPanel(noesisModel_t &model, const char *name, float left, float right, float z)
{
    noesisModel_t::Submesh mesh;
    mesh.objectName = name;
    for (auto p : {std::array<float, 3>{left, -2, z}, {right, -2, z}, {right, 0, z}, {left, 0, z}})
    {
        FFXIVertex v = {};
        std::copy(p.begin(), p.end(), v.pos);
        mesh.cpuVerts.push_back(v);
    }
    mesh.cpuIndices = {0, 1, 2, 0, 2, 3};
    mesh.triCount = 2;
    model.submeshes.push_back(mesh);
}
void Synthetic()
{
    gFF11LastMapObjects.clear();
    noesisModel_t model;
    for (int i = 0; i < 2; ++i)
    {
        ff11MapObjectDebug_t object = {};
        sprintf_s(object.displayName, "%03d: door_test", i);
        strcpy_s(object.objectName, "door_test");
        object.trans[0] = i ? 1.0f : -1.0f;
        object.trans[1] = -1;
        object.visualCollisionStart = i * 2;
        object.visualCollisionCount = 2;
        gFF11LastMapObjects.push_back(object);
        AddPanel(model, object.displayName, i ? 0.0f : -1.0f, i ? 1.0f : 0.0f, 0);
    }
    ZoneDoorInteraction::State state;
    state.Initialize(&model, false);
    Check(state.doors.size() == 2 && state.doors[0].partner == 1, "pair leaves meeting at their free edges");
    ZoneCollision::Mesh collision;
    for (int i = 0; i < 4; ++i)
    {
        auto &sm = model.submeshes[i / 2];
        float points[3][3];
        for (int v = 0; v < 3; ++v)
            std::copy_n(sm.cpuVerts[sm.cpuIndices[(i % 2) * 3 + v]].pos, 3, points[v]);
        ZoneCollision::Triangle t;
        ZoneCollision::BuildTriangle(&points[0][0], false, t);
        float padding = state.BindCollision(i, i, t);
        collision.AddTriangle(t, padding + 0.2f);
    }
    const auto view = D3DMath::BuildLookAtLH(0, -1, -5, 0, -1, 0);
    const auto projection = D3DMath::BuildPerspectiveFovLH(0.8f, 1, 0.1f, 100);
    ZoneObjectVisibility::RenderContext visibility;
    auto hit = state.Pick(&model, visibility, view, projection, 512, 512, 220, 256);
    Check(hit.door >= 0, "click ray hits visible door triangle");
    AddPanel(model, "wall", -2, 2, -1);
    model.UpdateSubmeshBounds();
    Check(state.Pick(&model, visibility, view, projection, 512, 512, 220, 256).door < 0,
          "wall occludes door selection");
    model.submeshes.pop_back();
    std::vector<std::string> hidden = {state.doors[0].name, state.doors[1].name};
    visibility.hiddenNames = &hidden;
    Check(state.Pick(&model, visibility, view, projection, 512, 512, 220, 256).door < 0,
          "hidden doors cannot be selected");
    visibility.hiddenNames = nullptr;
    float player[3] = {0, 0, -2};
    Check(!state.Click(0, player) && state.doors[0].targetAngle == 0, "first click selects without opening");
    Check(state.Click(1, player), "second click on paired leaf opens selected door");
    Check(state.doors[0].targetAngle * state.doors[1].targetAngle < 0,
          "paired leaves swing in opposite directions");
    Check(ZoneCollision::OverlapsWallAt(collision.Triangles(), collision.Index(), 0, 0, 0, 0.2f, 1.5f, 0.2f),
          "closed doors block passage");
    float blockedPlayer[] = {0, 0, -2};
    float blockedVelocity = 0;
    bool blockedGrounded = false;
    ZoneCollision::MoveHorizontal(collision, PlayerController::kCollisionRadius,
        PlayerController::kCollisionHeight, PlayerController::kStepHeight, 1.25f,
        blockedPlayer, blockedVelocity, blockedGrounded, 0, 4);
    Check(blockedPlayer[2] < -0.7f, "production player body cannot cross a closed door");
    for (int i = 0; i < 20; ++i)
        state.Update(0.05f, collision);
    Check(!ZoneCollision::OverlapsWallAt(collision.Triangles(), collision.Index(), 0, 0, 0, 0.2f, 1.5f, 0.2f),
          "opened door collision clears passage");
    Check(state.transforms.size() == 2, "both rendered leaves receive hinge transforms");
    visibility.overrides = &state.transforms;
    Check(state.Pick(&model, visibility, view, projection, 512, 512, 256, 256).door < 0,
          "pick ray passes through opened doorway");
    state.Click(-1, player);
    Check(state.selected == -1, "empty click clears selection");
    state.Update(14.0f, collision);
    Check(state.doors[0].targetAngle != 0 && state.doors[1].targetAngle != 0,
          "paired doors remain open before the timeout");
    state.Update(1.0f, collision);
    Check(state.doors[0].targetAngle == 0 && state.doors[1].targetAngle == 0 && state.doors[0].angle != 0,
          "both leaves begin animated closing after fifteen seconds");
    state.Click(0, player);
    state.Click(0, player);
    Check(state.doors[0].targetAngle != 0 && state.doors[1].targetAngle != 0,
          "interacting while closing reopens both leaves");
    for (int i = 0; i < 20; ++i)
        state.Update(0.05f, collision);
    state.Click(1, player);
    Check(state.doors[0].openSeconds == 0 && state.doors[1].openSeconds == 0,
          "interacting with an open door resets both timers");
    state.Update(15.0f, collision);
    for (int i = 0; i < 20; ++i)
        state.Update(0.05f, collision);
    Check(state.doors[0].angle == 0 && state.doors[1].angle == 0 && state.transforms.empty(),
          "automatic close restores both original render transforms");
    Check(ZoneCollision::OverlapsWallAt(collision.Triangles(), collision.Index(), 0, 0, 0, 0.2f, 1.5f, 0.2f),
          "closed doors restore collision across the passage");
    Check(state.Pick(&model, visibility, view, projection, 512, 512, 220, 256).door >= 0,
          "automatically closed door can be selected again");
    {
        auto independent = state;
        independent.SetPhysics(true);
        ZoneCollision::Mesh noCollision;
        float contact[3] = {-0.7f, 0, -0.25f};
        for (int frame = 0; frame < 15; ++frame)
            independent.UpdatePhysics(1.0f / 60, noCollision, contact, 0, 0.03f, 0.25f, 1.5f);
        Check(std::fabs(independent.doors[0].angle) > 0.01f && independent.doors[1].angle == 0,
              "physics moves only the contacted leaf");
        Check(std::fabs(independent.doors[0].angularVelocity) > 0.01f, "contact produces angular momentum");
        const float angle = independent.doors[0].angle;
        independent.UpdatePhysics(1.0f / 120, noCollision);
        Check(independent.doors[0].angle != angle, "door continues swinging after contact ends");
        auto above = state;
        above.SetPhysics(true);
        contact[1] = -10;
        above.UpdatePhysics(0.1f, noCollision, contact, 0, 0.03f, 0.25f, 1.5f);
        Check(above.doors[0].angle == 0, "player on another floor cannot push door");
    }
    state.SetPhysics(true);
    Check(!state.Click(0, player) && state.doors[0].angle == 0, "physics mode clicks select without opening");
    state.Update(30, collision);
    Check(state.doors[0].angle == 0, "classic timer is inactive in physics mode");
    player[0] = 0;
    player[2] = -2;
    float widest = 0;
    for (int frame = 0; frame < 160; ++frame)
    {
        const float oldX = player[0], oldZ = player[2];
        state.UpdatePhysics(1.0f / 60, collision, player, 0, 0.05f, 0.72f, 1.5f);
        float x = oldX, z = oldZ + 0.05f;
        ZoneCollision::ResolveHorizontalCollision(collision.Triangles(), collision.Index(), 0.72f, 1.5f, 0.2f,
                                                  true, oldX, oldZ, x, player[1], z);
        player[0] = x;
        player[2] = z;
        widest = (std::max)(widest, std::fabs(state.doors[0].angle));
    }
    std::cout << "Physics crossing z=" << player[2] << " swing=" << widest << "\n";
    Check(player[2] > 1, "player pushes physics doors and walks through their moving collision");
    Check(widest > 0.5f && widest <= 1.746f, "physics contact swings within hinge limits");
    for (int frame = 0; frame < 2400; ++frame)
        state.UpdatePhysics(1.0f / 120, collision);
    Check(std::fabs(state.doors[0].angle) < 0.005f && std::fabs(state.doors[1].angle) < 0.005f,
          "damped hinge spring settles toward closed without contact");
    player[2] = 2;
    for (int frame = 0; frame < 160; ++frame)
    {
        const float oldX = player[0], oldZ = player[2];
        state.UpdatePhysics(1.0f / 60, collision, player, 0, -0.05f, 0.72f, 1.5f);
        float x = oldX, z = oldZ - 0.05f;
        ZoneCollision::ResolveHorizontalCollision(collision.Triangles(), collision.Index(), 0.72f, 1.5f, 0.2f,
                                                  true, oldX, oldZ, x, player[1], z);
        player[0] = x;
        player[2] = z;
    }
    Check(player[2] < -1, "player can push through from the opposite side");
    const float beforeSwitch = state.doors[0].angle;
    state.SetPhysics(false);
    Check(state.doors[0].angle == beforeSwitch && state.doors[0].angularVelocity == 0,
          "switching to classic preserves pose and clears physics momentum");
    state.Initialize(&model, false);
    player[2] = -20;
    state.Click(0, player);
    Check(!state.Click(0, player) && state.doors[0].targetAngle == 0, "distant player cannot open door");
    state.Initialize(nullptr, false);
    Check(state.doors.empty() && state.selected == -1, "zone unload clears door state");
}
void Retail(const char *path)
{
    BYTE *raw = nullptr;
    DWORD size = 0;
    if (!FFXIFileIO::ReadWholeFile(path, &raw, &size))
    {
        Check(false, "read retail DAT");
        return;
    }
    std::unique_ptr<BYTE[]> bytes(raw);
    noeRAPI_t rapi(nullptr);
    ff11Opts_t opts = {};
    opts.collectCollision = true;
    gpFF11Opts = &opts;
    rapi.SetCurrentFilePath(path);
    int count = 0;
    auto *model = Model_FF11_LoadDAT(raw, size, count, &rapi);
    Check(model != nullptr, "load retail DAT");
    if (!model)
        return;
    ZoneDoorInteraction::State state;
    state.Initialize(model, false);
    Check(state.doors.size() == 30, "discover all 30 Bastok Markets door leaves including LOD doors");
    // Exercise picking in the complete zone, including nearby wall geometry.
    auto &example = state.doors[0];
    float dx = example.center[0] - example.hinge[0], dz = example.center[2] - example.hinge[2];
    float length = std::hypot(dx, dz);
    dx /= length;
    dz /= length;
    bool picked = false;
    for (float side : {-1.0f, 1.0f})
    {
        auto view = D3DMath::BuildLookAtLH(example.center[0] + side * dz * 2, example.center[1],
                                           example.center[2] - side * dx * 2, example.center[0],
                                           example.center[1], example.center[2]);
        auto projection = D3DMath::BuildPerspectiveFovLH(0.8f, 1, D3DMath::ZoneNearPlane, 500);
        ZoneObjectVisibility::RenderContext visibility;
        const auto hit = state.Pick(model, visibility, view, projection, 512, 512, 256, 256);
        if (hit.door == 0)
            picked = true;
    }
    Check(picked, "retail door is selectable amid surrounding zone geometry");
    int paired = 0;
    for (const auto &d : state.doors)
        if (d.partner >= 0)
            ++paired;
    Check(paired == 30, "pair all 15 retail double doors");
    ZoneCollision::Mesh collision;
    for (size_t i = 0; i < gFF11LastCollisionTriangles.size(); ++i)
    {
        ZoneCollision::Triangle t;
        if (!ZoneCollision::BuildTriangle(&gFF11LastCollisionTriangles[i].p[0][0], false, t))
            continue;
        float padding = state.BindCollision(i, (int)collision.Triangles().size(), t);
        collision.AddTriangle(t, padding + 0.2f);
    }
    for (size_t i = 0; i < state.doors.size(); ++i)
    {
        auto &d = state.doors[i];
        Check(std::any_of(state.collision.begin(), state.collision.end(),
                          [&](const auto &c) { return c.door == (int)i; }),
              "retail leaf has moving collision");
        float player[3] = {d.center[0], d.center[1], d.center[2] - 1};
        state.selected = -1;
        state.Click((int)i, player);
        state.Click((int)i, player);
    }
    for (int i = 0; i < 20; ++i)
        state.Update(0.05f, collision);
    int clearPassages = 0;
    for (size_t i = 0; i < state.doors.size(); ++i)
    {
        const auto &d = state.doors[i];
        if (d.partner < (int)i)
            continue;
        const auto &other = state.doors[d.partner];
        float x = (d.center[0] + other.center[0]) * 0.5f, z = (d.center[2] + other.center[2]) * 0.5f;
        float ux = (d.center[0] - d.hinge[0]) * 2 / d.width;
        float uz = (d.center[2] - d.hinge[2]) * 2 / d.width;
        for (float side : {-1.0f, 1.0f})
        {
            float player[] = {x + side * uz * 2, d.maxY, z - side * ux * 2};
            float floor = d.maxY;
            ZoneCollision::FindFloorAt(collision.Triangles(), collision.Index(), 0.72f, player[0], player[2],
                                       d.maxY - 1, d.maxY + 2, &floor, nullptr);
            player[1] = floor;
            float velocity = 0;
            bool grounded = true;
            for (int frame = 0; frame < 80; ++frame)
                ZoneCollision::MoveHorizontal(collision, PlayerController::kCollisionRadius,
                    PlayerController::kCollisionHeight, PlayerController::kStepHeight, 1.25f, player, velocity,
                                              grounded, -side * uz * 0.05f, side * ux * 0.05f);
            float progress = (player[0] - x) * (-side * uz) + (player[2] - z) * (side * ux);
            if (progress < 1)
                std::cout << "Crossing blocked " << d.name << " side=" << side << " progress=" << progress
                          << " y=" << player[1] << " sill=" << d.maxY << "\n";
            Check(progress > 1, "full-size player traverses opened retail doorway from both sides");
        }
        // Keep the probe above the sill: walls beneath upstairs thresholds
        // are static architecture and must not rotate with the door.
        bool blocked = ZoneCollision::OverlapsWallAt(collision.Triangles(), collision.Index(), x,
                                                     d.maxY - 0.25f, z, 0.2f, 1.5f, 0.2f);
        if (!blocked)
            ++clearPassages;
        else
            std::cout << "Still blocked " << d.name << "\n";
    }
    std::cout << "Clear retail passages=" << clearPassages << "\n";
    Check(clearPassages == 15, "all opened retail doorways clear collision at their center");
    Check(state.transforms.size() == 30, "all retail door leaves animate");
    std::cout << "Retail leaves=" << state.doors.size() << " paired=" << paired
              << " moving collision=" << state.collision.size() << "\n";
    for (auto &mesh : model->submeshes)
        for (auto &vertex : mesh.cpuVerts)
            vertex.pos[0] = -vertex.pos[0];
    state.Initialize(model, true);
    Check(state.doors.size() == 30 && state.doors[0].partner == 1,
          "mirrored zone preserves door hinges and pairing");
    Check(std::fabs(state.doors[0].hinge[0] + gFF11LastMapObjects[545].trans[0]) < 0.001f,
          "hinge follows normal scene X mirroring");
    gpFF11Opts = nullptr;
}
int main(int argc, char **argv)
{
    Synthetic();
    if (argc > 1)
        Retail(argv[1]);
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
