#include "stdafx.h"
#include "noesis_rapi.h"
#include "model_ff11.h"
#include "model_ff11_water.h"
#include "ffxi_file_io.h"
#include "zone_room_loader.h"
#include "zone_model_transform.h"
#include "zone_collision_geometry.h"
#include "zone_scene_diagnostics.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>

namespace
{
int failures = 0, checks = 0;
void Check(bool ok, const char* message)
{ ++checks; if (!ok) { ++failures; std::cerr << "FAIL: " << message << '\n'; } }
noesisModel_t::Submesh Mesh(const char* name)
{
    noesisModel_t::Submesh mesh;
    mesh.objectName = name;
    mesh.cpuVerts.resize(3);
    mesh.cpuVerts[1].pos[0]=8; mesh.cpuVerts[2].pos[2]=8;
    mesh.cpuIndices={0,1,2};
    return mesh;
}
void TestCaptureAndExport()
{
    noesisModel_t model;
    model.submeshes={Mesh("floor"),Mesh("env: sky"),Mesh("hidden"),Mesh("far LOD"),Mesh("water"),Mesh("missing"),Mesh("bad indices")};
    auto& lod=model.submeshes[3].zoneLod;
    lod.enabled=true; lod.highDistance=10; lod.midDistance=100; lod.levelMask=4;
    model.submeshes[4].water=std::make_shared<ZoneWater::Surface>();
    model.submeshes[5].cpuVerts.clear();
    model.submeshes[6].cpuIndices={0,1,50,0};
    ZoneCollision::Mesh collision;
    ZoneCollision::Triangle triangle;
    const float points[9]={0,0,0,8,0,0,0,0,8};
    ZoneCollision::BuildTriangle(points,false,triangle);
    collision.AddTriangle(triangle,0);
    std::map<std::string,ZoneObjectTransform::DebugTransform> overrides;
    overrides["floor"]={{4,-2,6},{0,0,0},{2,1,3}};
    auto snapshot=ZoneSceneDiagnostics::Capture(model,collision,{"hidden"},overrides,
        "source </pre><script>bad()</script> & \"test\"",false,{0,0,0});
    Check(snapshot.triangles.size()==3,"capture includes render, water and collision only");
    Check(snapshot.environmentMeshes==1 && snapshot.hiddenMeshes==1 && snapshot.lodMeshes==1,"capture respects scene selection policy");
    Check(snapshot.missingCpuMeshes==1 && snapshot.invalidIndexTriangles==1 && snapshot.trailingIndices==1,"incomplete source geometry is reported");
    Check(!snapshot.unresolvedMaterials && snapshot.untexturedMeshes==snapshot.includedMeshes,
        "unnamed vertex-color meshes are not broken material references");
    Check(snapshot.triangles[0].points[0].x==4 && snapshot.triangles[0].points[0].y==-2 &&
        snapshot.triangles[0].points[1].x==20 && snapshot.triangles[0].points[2].z==30,"runtime SRT matches rendered baked vertices");
    Check(snapshot.triangles[1].layer==ZoneCoverage::Layer::Water && snapshot.triangles[2].layer==ZoneCoverage::Layer::Collision,"water is never mistaken for structural geometry");
    model.submeshes.clear(); collision.Clear();
    Check(snapshot.triangles[2].points[1].x==8,"snapshot survives source unload");
    ZoneCoverage::Options options;
    auto result=ZoneCoverage::Build(snapshot.triangles,options);
    Check(result.success && result.render.coveredCells && result.collision.coveredCells && result.water.coveredCells,"captured geometry produces all layers");
    std::ostringstream html,csv;
    ZoneSceneDiagnostics::WriteHtml(html,snapshot,options,result);
    ZoneSceneDiagnostics::WriteCsv(csv,result);
    Check(html.str().find("</pre><script>bad()") == std::string::npos &&
        html.str().find("&lt;/pre&gt;&lt;script&gt;bad()") != std::string::npos,"source names cannot inject report scripts");
    Check(html.str().find("https://")==std::string::npos,"report requires no external assets");
    size_t occupied=0; for(auto cell:result.cells) occupied+=cell!=0;
    const std::string csvText=csv.str();
    Check(std::count(csvText.begin(),csvText.end(),'\n')==occupied+1,"CSV exports each occupied cell exactly once");
    options.useHeightRange=true; options.minY=-3; options.maxY=-1;
    result=ZoneCoverage::Build(snapshot.triangles,options);
    Check(result.success && result.render.coveredCells && !result.water.coveredCells && !result.collision.coveredCells,"Y slice isolates elevated geometry");
    result.success=false;
    bool rejected=false;
    try { ZoneSceneDiagnostics::WriteHtml(html,snapshot,options,result); } catch(...) { rejected=true; }
    Check(rejected,"failed analysis cannot masquerade as complete export");
}

void TestInstalled(const std::filesystem::path& root, const std::filesystem::path& output)
{
    const std::string path=(root/"ROM/1/35.DAT").string();
    BYTE* raw=nullptr; DWORD size=0;
    Check(FFXIFileIO::ReadWholeFile(path.c_str(),&raw,&size),"read installed Bastok Markets");
    if(!raw) return;
    std::unique_ptr<BYTE[]> bytes(raw);
    noeRAPI_t rapi(nullptr); rapi.SetCurrentFilePath(path.c_str());
    ff11Opts_t parserOptions={}; parserOptions.collectCollision=true;
    parserOptions.collectCollisionUnreferenced=true; parserOptions.renderWater=true;
    gpFF11Opts=&parserOptions;
    int count=0;
    auto* model=Model_FF11_LoadDAT(bytes.get(),static_cast<int>(size),count,&rapi);
    Check(model!=nullptr,"parse zone for diagnostic fixture");
    if(!model) { gpFF11Opts=nullptr; return; }
    Check(ZoneRoomLoader::Append(model,&rapi,path.c_str())==14,"coverage includes loaded city interiors");
    ZoneCollision::Mesh collision;
    for(const auto& rawTriangle:gFF11LastCollisionTriangles)
    {
        ZoneCollision::Triangle triangle;
        if(ZoneCollision::BuildTriangle(&rawTriangle.p[0][0],true,triangle)) collision.AddTriangle(triangle,0);
    }
    ZoneModelTransform::MirrorOnX(model,nullptr);
    const auto snapshot=ZoneSceneDiagnostics::Capture(*model,collision,{}, {},path,true,{0,0,0});
    Check(snapshot.waterMeshes>0 && snapshot.triangles.size()>10000,"real fixture includes authored water and substantial terrain");
    Check(!snapshot.missingCpuMeshes && !snapshot.invalidIndexTriangles && !snapshot.trailingIndices,"real fixture has complete CPU geometry");
    for(bool slice:{false,true})
    {
        ZoneCoverage::Options options;
        options.useHeightRange=slice; options.minY=-4; options.maxY=4;
        const auto result=ZoneCoverage::Build(snapshot.triangles,options);
        Check(result.success && result.render.coveredCells && result.collision.coveredCells,"installed coverage fits bounded work budget");
        if(!result.success) { std::cerr<<result.error<<'\n'; continue; }
        const auto name=slice?"bastok-markets-y-minus4-to4":"bastok-markets";
        std::ofstream html(output/(std::string(name)+".html"),std::ios::binary);
        std::ofstream csv(output/(std::string(name)+".csv"),std::ios::binary);
        ZoneSceneDiagnostics::WriteHtml(html,snapshot,options,result);
        ZoneSceneDiagnostics::WriteCsv(csv,result);
        std::cout<<ZoneSceneDiagnostics::Summary(snapshot,options,result)<<'\n';
    }
    gpFF11Opts=nullptr;
}
}
int main(int argc,char** argv)
{
    TestCaptureAndExport();
    if(argc>1)
    {
        const std::filesystem::path output=argc>2?argv[2]:"artifacts/zone-diagnostics";
        std::filesystem::create_directories(output);
        TestInstalled(argv[1],output);
    }
    std::cout<<"Zone diagnostics: "<<checks<<" checks, "<<failures<<" failures\n";
    return failures?1:0;
}
