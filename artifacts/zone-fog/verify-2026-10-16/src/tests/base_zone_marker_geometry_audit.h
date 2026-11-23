#pragma once

// Read-only diagnostic mode; no route or renderer behavior is changed.
static int AuditBaseMarkerGeometry(const char* root)
{
    using namespace ZoneTransition;
    int checked=0,missingFloor=0,rejected=0,buried=0;
    for(int zone : {100,101,102,103,104,105,106,107,108,109,110,111,112,115,116,117,118,119,120,
        126,127,139,140,141,142,143,144,145,146,147,148,149,150,151,152,157,158,161,162,165,166,167,
        169,170,172,184,190,191,192,193,194,195,196,197,198,200,204,206,
        230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,248,249})
    {
        const auto path=(std::filesystem::path(root)/FFXIZone::FindByID(zone)->modelDat).string();
        BYTE* raw=nullptr;DWORD size=0;
        if(!FFXIFileIO::ReadWholeFile(path.c_str(),&raw,&size)) return 2;
        std::unique_ptr<BYTE[]> bytes(raw);
        std::vector<ZoneTriggerVolume::Volume> all;
        if(!ZoneTriggerVolume::Decode(raw,size,all,true) ||
           !ZoneTriggerVolume::Decode(raw,size,authoredVolumes)) return 2;
        if(authoredVolumes.empty()) continue;
        noeRAPI_t rapi(nullptr);rapi.SetCurrentFilePath(path.c_str());
        ff11Opts_t options={};options.collectCollision=true;options.collectCollisionUnreferenced=true;
        gpFF11Opts=&options;gFF11LastCollisionTriangles.clear();gFF11LastCollisionMeshes.clear();
        int count=0;if(!Model_FF11_LoadDAT(raw,static_cast<int>(size),count,&rapi)) return 2;
        ZoneCollision::Mesh mesh;
        for(const auto& source : gFF11LastCollisionTriangles)
        {
            ZoneCollision::Triangle t;
            if(ZoneCollision::BuildTriangle(&source.p[0][0],false,t)) mesh.AddTriangle(t,PlayerController::kCollisionRadius);
        }
        auto floorAt=[&](const Point& p,float reference,float& y)
        {
            bool found=false;float best=1e30f;
            for(const auto& t : mesh.Triangles())
            {
                if(std::fabs(t.normal[1])<.35f || !ZoneCollision::PointInTriangleXZ(p[0],p[2],t)) continue;
                const float value=t.p[0][1]-(t.normal[0]*(p[0]-t.p[0][0])+t.normal[2]*(p[2]-t.p[0][2]))/t.normal[1];
                const float distance=std::fabs(value-reference);
                if(distance<20 && distance<best){best=distance;y=value;found=true;}
            }
            return found;
        };
        for(const auto& route : lines)
        {
            if(route.fromZone!=zone) continue;
            // Loaded Tower circle assets intentionally replace these rows.
            if(std::strcmp(route.token,"htin")==0 || std::strcmp(route.token,"hout")==0 ||
               std::strcmp(route.token,"hret")==0) continue;
            const auto* v=VolumeFor(route);if(!v) continue;
            ++checked;
            Point normal=v->size[0]<v->size[2] ? Point{std::cos(v->yaw),0,-std::sin(v->yaw)} : Point{std::sin(v->yaw),0,std::cos(v->yaw)};
            if(normal[0]*route.outward[0]+normal[2]*route.outward[2]<0)for(auto& value : normal)value=-value;
            const ZoneTriggerVolume::Volume* arrival=nullptr;float best=1e30f;
            for(const auto& a : all)
            {
                if(a.target)continue;float distance=0;
                for(int axis=0;axis<3;++axis)distance+=(a.center[axis]-v->center[axis])*(a.center[axis]-v->center[axis]);
                if(distance<best){best=distance;arrival=&a;}
            }
            const float reference=arrival ? arrival->center[1] : v->center[1];
            const float depth=(std::min)(v->size[0],v->size[2])*.5f;
            for(float distance : {3.f,10.f,20.f})
            {
                auto player=v->center;
                for(int axis : {0,2})player[axis]-=normal[axis]*(depth+distance);
                float floor=0;
                if(!floorAt(player,reference,floor)){++missingFloor;std::cout<<"NO_FLOOR "<<zone<<':'<<route.token<<" distance="<<distance<<std::endl;continue;}
                player[1]=floor;
                if(!MarkerFloorEligible(route,player)){++rejected;std::cout<<"FLOOR_REJECT "<<zone<<':'<<route.token<<" distance="<<distance<<" floor="<<floor<<" bounds="<<v->center[1]-v->size[1]*.5f<<','<<v->center[1]+v->size[1]*.5f<<std::endl;continue;}
                auto dot=v->center;
                for(int axis : {0,2})dot[axis]-=normal[axis]*(depth+1.5f);
                float dotFloor=0;
                if(floorAt(dot,floor,dotFloor) && floor-1>dotFloor+.05f)
                {++buried;std::cout<<"BELOW_FLOOR "<<zone<<':'<<route.token<<" distance="<<distance<<" dotY="<<floor-1<<" surface="<<dotFloor<<std::endl;}
            }
        }
        gpFF11Opts=nullptr;
        std::cout<<"ZONE_DONE "<<zone<<std::endl;
    }
    authoredVolumes.clear();
    std::cout<<"GEOMETRY_AUDIT checked="<<checked<<" noFloor="<<missingFloor<<" floorRejected="<<rejected<<" belowFloor="<<buried<<std::endl;
    return 0; // Findings are diagnostics, not an assertion of an in-game failure.
}
