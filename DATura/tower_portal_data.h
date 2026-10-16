#pragma once
#include "home_point_data.h"
#include "effect_model_layout.h"
#include <set>

namespace TowerPortal
{
// Select by resource type and embedded material identity, never by the
// ambiguous four-byte texture chunk name in the common DAT.
struct Chunk { size_t offset, length; unsigned int type; std::string name; };
inline bool Chunks(const std::vector<uint8_t>& bytes, std::vector<Chunk>& out)
{
    out.clear();
    for (size_t p=0;p<bytes.size();)
    {
        if (bytes.size()-p<16) return false;
        const auto info=HomePoint::Read<uint32_t>(bytes.data()+p+4);
        const size_t length=(info>>3)&0x7ffff0;
        if (length<16 || length>bytes.size()-p) return false;
        out.push_back({p,length,info&127,HomePoint::Tag(bytes.data()+p)});
        p+=length;
    }
    return true;
}
inline void Append(const std::vector<uint8_t>& src, const Chunk& c, std::vector<uint8_t>& dst)
{
    dst.insert(dst.end(),src.begin()+c.offset,src.begin()+c.offset+c.length);
}
inline bool Geometry(const std::vector<uint8_t>& bytes, bool rings, std::vector<uint8_t>& out)
{
    std::vector<Chunk> chunks;
    if (!Chunks(bytes,chunks)) return false;
    std::set<std::string> materials;
    out.clear();
    int ringCount=0;
    for (const auto& c : chunks)
    {
        if (c.type!=0x1f && (rings || c.type!=0x21)) continue;
        if (rings && c.name!="eis2" && c.name!="ei30" && c.name!="hi18") continue;
        const auto* p=bytes.data()+c.offset+16;
        const size_t n=c.length-16;
        if (n<32) continue;
        int materialOffset=0;
        if (c.type==0x1f)
        {
            if (p[0]!=3 && p[0]!=6) continue;
            materialOffset=p[0]==3 ? 16 : EffectModelLayout::MaterialOffset6(p[4],p[5]);
        }
        else materialOffset=12;
        if (size_t(materialOffset)+16>n) return false;
        materials.insert(HomePoint::Tag(p+materialOffset,16));
        Append(bytes,c,out);
        if (rings) ++ringCount;
    }
    for (const auto& c : chunks)
        if (c.type==0x20 && c.length>=33 &&
            materials.count(HomePoint::Tag(bytes.data()+c.offset+17,16)))
            Append(bytes,c,out);
    return !out.empty() && (!rings || ringCount==3);
}
inline bool RingData(const std::vector<uint8_t>& bytes, HomePoint::Data& out)
{
    std::vector<Chunk> chunks;
    if (!Chunks(bytes,chunks)) return false;
    std::vector<uint8_t> selected;
    for (const auto& c : chunks)
        if (c.type==0x19 || (c.type==5 && (c.name=="rig1" || c.name=="rig2" || c.name=="rig3")))
            Append(bytes,c,selected);
    if (!HomePoint::Parse(selected,out,false)) return false;
    out.idleSchedule="portal";
    out.activationSchedule="main";
    out.repeatIdleMeshes=true;
    for (const char* name : {"rig1","rig2","rig3"})
    {
        const auto g=out.generators.find(name);
        if (g==out.generators.end() || g->second.resource.empty() ||
            !out.curves.count(g->second.alphaCurve)) return false;
        // Spin in the mesh plane before the authored X tilt lays it on the floor.
        g->second.localSpin=true;
        out.schedules["portal"].push_back({name,0,0,false});
    }
    return true;
}
inline bool ActivationData(const std::vector<uint8_t>& bytes, HomePoint::Data& out)
{
    std::vector<Chunk> chunks;
    if (!Chunks(bytes,chunks)) return false;
    std::vector<uint8_t> selected;
    for (const auto& c : chunks)
        if (c.type==5 || c.type==0x19 || c.type==0x3d || (c.type==7 && c.name=="main"))
            Append(bytes,c,selected);
    if (!HomePoint::Parse(selected,out,false) || !out.schedules.count("main")) return false;
    out.idleSchedule="none";
    out.activationSchedule="main";
    return out.sounds.count("6001") && out.sounds.at("6001")==16001 &&
        std::any_of(out.schedules.at("main").begin(),out.schedules.at("main").end(),
            [](const auto& event){return event.sound && event.resource=="6001" && event.frame==25;});
}
// A declined query must not reopen until the player leaves its circle.
struct Interaction
{
    const void* suppressed=nullptr;
    bool query=false, activating=false, soundPlayed=false;
    int selected=0;
    double elapsed=0;
    void Cancel(const void* route) { *this={}; suppressed=route; }
    void Begin() { query=true; selected=0; }
    void Confirm() { query=false; activating=true; elapsed=0; soundPlayed=false; }
    bool Tick(double dt)
    {
        if (!activating || !std::isfinite(dt) || dt<0) return false;
        elapsed+=dt;
        if (!soundPlayed && elapsed>=25.0/60.0) { soundPlayed=true; return true; }
        return false;
    }
};
}
