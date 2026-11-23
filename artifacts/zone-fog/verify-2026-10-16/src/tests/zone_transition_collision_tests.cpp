#include "stdafx.h"
#include "zone_transition.h"
#include "zone_dat_table.h"
#include "zone_collision_geometry.h"
#include "player_controller.h"
#include "ffxi_file_io.h"
#include "noesis_rapi.h"
#include "model_ff11.h"
#include <filesystem>
#include <iostream>
#include <memory>
#include "base_zone_marker_geometry_audit.h"

int main(int argc, char** argv)
{
    if(argc==3 && std::strcmp(argv[2],"--markers")==0) return AuditBaseMarkerGeometry(argv[1]);
    if (argc != 2) return 2;
    int failures = 0;
    int cases = 0;
    for (int zone : {235, 234, 236, 237})
    {
        const auto path = (std::filesystem::path(argv[1]) / FFXIZone::FindByID(zone)->modelDat).string();
        BYTE* raw = nullptr; DWORD size = 0;
        if (!FFXIFileIO::ReadWholeFile(path.c_str(), &raw, &size)) return 2;
        std::unique_ptr<BYTE[]> bytes(raw);
        if (!ZoneTriggerVolume::Decode(bytes.get(),size,ZoneTransition::authoredVolumes)) return 2;
        noeRAPI_t rapi(nullptr);
        rapi.SetCurrentFilePath(path.c_str());
        ff11Opts_t options = {};
        options.collectCollision = true;
        options.collectCollisionUnreferenced = true;
        gpFF11Opts = &options;
        gFF11LastCollisionTriangles.clear();
        gFF11LastCollisionMeshes.clear();
        int count = 0;
        if (!Model_FF11_LoadDAT(bytes.get(), static_cast<int>(size), count, &rapi)) return 2;
        for (bool mirror : {false, true})
        {
            ZoneCollision::Mesh mesh;
            for (const auto& rawTriangle : gFF11LastCollisionTriangles)
            {
                ZoneCollision::Triangle triangle;
                if (ZoneCollision::BuildTriangle(&rawTriangle.p[0][0], mirror, triangle))
                    mesh.AddTriangle(triangle, PlayerController::kCollisionRadius);
            }
            for (const auto& line : ZoneTransition::lines)
            {
                if (line.fromZone != zone) continue;
                const auto* volume = ZoneTransition::VolumeFor(line);
                if (!volume) continue;
                ++cases;
                auto start = volume->center;
                const float depth = (std::min)(volume->size[0],volume->size[2])*0.5f;
                for (int axis : {0,2}) start[axis] -= line.outward[axis]*(depth+3);
                start = FFXICoordinateFrame::NativeDatToScene(start, mirror);
                const auto direction = FFXICoordinateFrame::NativeDatToScene(line.outward, mirror);
                float x = start[0], y = start[1], z = start[2];
                const bool grounded = ZoneCollision::FindNearestSafeFloor(mesh.Triangles(), mesh.Index(),
                    PlayerController::kCollisionRadius, PlayerController::kCollisionHeight, PlayerController::kStepHeight,
                    x,y,z,&x,&y,&z);
                PlayerController::State player;
                PlayerController::SetPose(player,x,y,z,std::atan2(-direction[0],-direction[2]),grounded);
                PlayerController::SetRespawnPoint(player);
                PlayerController::InputSnapshot input;
                input.forward = 1;
                bool crossed = false;
                for (int frame = 0; frame < 180 && !crossed; ++frame)
                {
                    const ZoneTransition::Point before = {player.position[0],player.position[1],player.position[2]};
                    const auto result = PlayerController::UpdateMovement(player,input,1.0f/60,{&mesh});
                    const ZoneTransition::Point after = {player.position[0],player.position[1],player.position[2]};
                    crossed = !result.respawned && ZoneTransition::Crossed(zone,before,after,mirror) == &line;
                }
                std::cout << line.token << " mirror=" << mirror << " grounded=" << grounded << " crossed=" << crossed << '\n';
                if (!grounded || !crossed) ++failures;
            }
        }
        gpFF11Opts = nullptr;
    }
    if (cases != 28) { std::cerr << "Incomplete four-zone trigger coverage: " << cases << '\n'; ++failures; }
    return failures ? 1 : 0;
}
