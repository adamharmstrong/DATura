#include "../DATura/zone_transition.h"
#include "../DATura/zone_entry_message.h"
#include "mainland_exit_cases.h"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <iterator>
#include <filesystem>

static void Check(bool value, const char* message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

int main(int argc, char** argv)
{
    using namespace ZoneTransition;
    {
        const ZoneTriggerVolume::Volume box = {{0,0,0},{2,4,6},0,0,1,235};
        Check(ZoneTriggerVolume::Hit(box,{-2,0,0},{2,0,0}), "Fast segment skipped volume");
        Check(!ZoneTriggerVolume::Hit(box,{-2,2,0},{2,2,0}), "Strict face contact triggered");
        Check(!ZoneTriggerVolume::Hit(box,{1,0,0},{2,0,0}), "Moving away from face triggered");
        Check(ZoneTriggerVolume::Hit(box,{0,0,0},{0,0,0}), "Stationary containment failed");
    }

    Check(EncodeHeading(0.0f) == 0 && EncodeHeading(1.570796f) == 64 &&
          EncodeHeading(3.141593f) == 128 && EncodeHeading(4.712389f) == 192 &&
          EncodeHeading(5.497787f) == 224,
        "FFXI packet heading encoding is incorrect");
    Check(EncodeHeading(kHeadingTurn) == 0 && EncodeHeading(-1.570796f) == 192,
        "FFXI packet heading wrapping is incorrect");

    Check(ZoneEntryMessage::ForZone(235) == "You have entered Bastok Markets.",
        "Known zone entry message is incorrect");
    Check(ZoneEntryMessage::ForZone(-1).empty(),
        "Unknown zones should not produce an entry message");
    for (const auto& portal : towerPortals)
    {
        for (bool mirror : {false,true})
        {
            auto scene=[mirror](Point p){return FFXICoordinateFrame::NativeDatToScene(p,mirror);};
            auto start=portal.center,end=portal.center;
            start[2]+=portal.zone==239 ? -3 : 3;
            const auto* route=Crossed(portal.zone,scene(start),scene(end),mirror);
            const int destination=portal.id==ZoneTriggerVolume::Token("hret") ? 242 :
                portal.zone==239 ? 242 : 239;
            Check(route && route->toZone==destination,"Tower portal crossing failed");
            auto floor=portal.center; floor[1]-=portal.size[1]*0.5f;
            Check(Crossed(portal.zone,scene(floor),scene(floor),mirror)==route,"Tower portal floor contact missed");
            auto high=portal.center; high[1]+=5;
            Check(!Crossed(portal.zone,scene(high),scene(high),mirror),"Tower portal wrong floor triggered");
            auto wide=portal.center; wide[0]+=5;
            Check(!Crossed(portal.zone,scene(wide),scene(wide),mirror),"Tower portal outside width triggered");
            Check(VolumeFor(*route)!=nullptr,"Tower portal marker volume missing");
            const auto* returnRoute=Crossed(route->toZone,scene(route->arrival),scene(route->arrival),mirror);
            Check(!returnRoute,"Tower portal arrival bounced back");
        }
    }
    for (const auto& line : lines)
    {
        if (std::strcmp(line.token,"htin")==0 || std::strcmp(line.token,"hout")==0 ||
            std::strcmp(line.token,"hret")==0) continue;
        for (bool mirror : { false, true })
        {
            auto inside = line.threshold, outside = line.threshold;
            for (int axis : {0, 2})
            {
                inside[axis] -= line.outward[axis]*2;
                outside[axis] += line.outward[axis]*2;
            }
            auto scene = [mirror](Point p) { return FFXICoordinateFrame::NativeDatToScene(p, mirror); };
            Check(Crossed(line.fromZone, scene(inside), scene(outside), mirror) == &line, "Crossing missed");
            Check(!Crossed(line.fromZone, scene(outside), scene(inside), mirror), "Inward movement triggered");
            Check(!Crossed(line.fromZone, scene(inside), scene(inside), mirror), "Stationary player triggered");
            Check(!Crossed(-1, scene(inside), scene(outside), mirror), "Unrelated zone triggered");
            auto highStart = inside, highEnd = outside;
            highStart[1] -= 10; highEnd[1] -= 10;
            Check(!Crossed(line.fromZone, scene(highStart), scene(highEnd), mirror), "Wrong floor triggered");
            auto wideStart = inside, wideEnd = outside;
            for (auto* p : { &wideStart, &wideEnd })
            {
                (*p)[0] += line.outward[2]*(line.halfWidth+2);
                (*p)[2] -= line.outward[0]*(line.halfWidth+2);
            }
            Check(!Crossed(line.fromZone, scene(wideStart), scene(wideEnd), mirror), "Outside corridor triggered");
            const float yaw = ArrivalYaw(line, mirror);
            const float heading = DecodeHeading(line.heading);
            const auto forward = scene({ std::cos(heading), 0, -std::sin(heading) });
            Check(std::fabs(-std::sin(yaw)-forward[0]) < 0.00001f &&
                std::fabs(-std::cos(yaw)-forward[2]) < 0.00001f, "Arrival heading incorrect");
            for (const auto& reverse : lines)
            {
                if (reverse.fromZone != line.toZone || reverse.toZone != line.fromZone) continue;
                if (std::fabs(line.arrival[1]-reverse.threshold[1])>4) continue;
                // Multiple passages can connect the same two zones. Only the
                // receiving passage constrains this arrival's safe side.
                const float dx = line.arrival[0] - reverse.threshold[0];
                const float dz = line.arrival[2] - reverse.threshold[2];
                if (dx*dx + dz*dz > 50.0f*50.0f) continue;
                bool closest=true;
                for (const auto& other : lines)
                {
                    if (other.fromZone!=reverse.fromZone || other.toZone!=reverse.toZone) continue;
                    if (std::fabs(line.arrival[1]-other.threshold[1])>4) continue;
                    const float ox=line.arrival[0]-other.threshold[0];
                    const float oz=line.arrival[2]-other.threshold[2];
                    if (ox*ox+oz*oz < dx*dx+dz*dz) { closest=false; break; }
                }
                if (!closest) continue;
                const float side = (line.arrival[0]-reverse.threshold[0])*reverse.outward[0] +
                    (line.arrival[2]-reverse.threshold[2])*reverse.outward[2];
                if (side >= -1) std::cerr << line.token << " arrival vs " << reverse.token << " side=" << side << '\n';
                Check(side < -1, "Arrival is not safely inside destination");
                Check(!Crossed(line.toZone, scene(line.arrival), scene(line.arrival), mirror), "Arrival bounced back");
            }
        }
    }
    if (argc > 1)
    {
        std::ifstream input(argv[1], std::ios::binary);
        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)), {});
        Check(!bytes.empty(), "Cannot read test zone DAT");
        Check(ZoneTriggerVolume::Decode(bytes.data(), bytes.size(), authoredVolumes), "RID decode failed");
        for (const char* token : {"mh_bm_n", "mh_bm_s"})
        {
            const Line* route = nullptr;
            for (const auto& line : lines) if (std::strcmp(line.token, token) == 0) route = &line;
            Check(route != nullptr, "Missing residential route");
            const auto* v = VolumeFor(*route);
            Check(v && std::fabs(v->center[0]+146.16f) < 0.02f &&
                v->size == Point{10,8,2}, "Residential authored dimensions incorrect");
            for (bool mirror : {false,true})
            {
                auto p = v->center;
                p[1] = -4; p[0] -= 2;
                auto start = p; start[0] += 4;
                auto scene = [mirror](Point q) { return FFXICoordinateFrame::NativeDatToScene(q,mirror); };
                Check(Crossed(235,scene(start),scene(p),mirror) == route, "Residential volume crossing missed");
                Check(Crossed(235,scene(v->center),scene(v->center),mirror) == route, "Retail stationary containment missed");
                auto high = v->center; high[1] += 5;
                Check(!Crossed(235,scene(high),scene(high),mirror), "Residential wrong floor triggered");
                auto wide = start; wide[2] += 6; auto wideEnd = p; wideEnd[2] += 6;
                Check(!Crossed(235,scene(wide),scene(wideEnd),mirror), "Residential outside arch triggered");
            }
        }
        auto truncated = bytes;
        truncated.resize(100);
        std::vector<ZoneTriggerVolume::Volume> invalid;
        Check(!ZoneTriggerVolume::Decode(truncated.data(),truncated.size(),invalid) && invalid.empty(),
            "Truncated RID accepted");
        authoredVolumes.clear();
        std::cout << "Authored Bastok residential volume checks passed.\n";
    }
    if (argc > 2)
    {
        int audited = 0;
        for (int zone : {234,235,236,237})
        {
            const auto path = std::filesystem::path(argv[2]) / "ROM" / "1" /
                (std::to_string(zone-200)+".DAT");
            std::ifstream file(path,std::ios::binary);
            std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)), {});
            Check(!bytes.empty() && ZoneTriggerVolume::Decode(bytes.data(),bytes.size(),authoredVolumes),
                "Bastok zone RID decode failed");
            const int expected = zone == 234 ? 4 : zone == 235 ? 6 : zone == 236 ? 3 : 1;
            Check(authoredVolumes.size() == expected, "Unexpected Bastok active trigger count");
            std::vector<ZoneTriggerVolume::Volume> allVolumes;
            Check(ZoneTriggerVolume::Decode(bytes.data(),bytes.size(),allVolumes,true), "Arrival decode failed");
            int arrivals = 0;
            for (const auto& volume : allVolumes) if (!volume.target) ++arrivals;
            Check(arrivals == expected && allVolumes.size() == expected*2,
                "Zero-target Bastok volume coverage incomplete");
            for (const auto& v : authoredVolumes)
            {
                const Line* route = nullptr;
                int matches = 0;
                for (const auto& line : lines)
                    if (line.fromZone == zone && VolumeFor(line) == &v) { route = &line; ++matches; }
                Check(matches == 1, "Bastok trigger does not have exactly one destination route");
                const auto token = v.id;
                const int expectedDestination = (token & 0xffffffu) == ZoneTriggerVolume::Token("zmr") ? 301 :
                    token == ZoneTriggerVolume::Token("z6i0") ? 107 :
                    token == ZoneTriggerVolume::Token("z6i4") ? 172 :
                    token == ZoneTriggerVolume::Token("z6j0") ? 234 :
                    token == ZoneTriggerVolume::Token("z6j2") ? 236 :
                    token == ZoneTriggerVolume::Token("z6j4") ? 107 :
                    token == ZoneTriggerVolume::Token("z6j6") ? 237 :
                    token == ZoneTriggerVolume::Token("z6k0") ? 106 : 235;
                Check(route->toZone == expectedDestination, "Wrong Bastok trigger destination");
                for (bool mirror : {false,true})
                {
                    auto a=v.center,b=v.center;
                    // Cross along the authored thin axis in both directions.
                    const bool thinX = v.size[0] < v.size[2];
                    Point normal = thinX ? Point{std::cos(v.yaw),0,-std::sin(v.yaw)} :
                        Point{std::sin(v.yaw),0,std::cos(v.yaw)};
                    const float extent = (std::min)(v.size[0],v.size[2])*0.5f+1;
                    for (int axis : {0,2}) { a[axis]-=normal[axis]*extent; b[axis]+=normal[axis]*extent; }
                    auto scene=[mirror](Point p) { return FFXICoordinateFrame::NativeDatToScene(p,mirror); };
                    Check(Crossed(zone,scene(a),scene(b),mirror)==route &&
                        Crossed(zone,scene(b),scene(a),mirror)==route,"Bastok authored crossing missed");
                    auto high=v.center; high[1]+=v.size[1];
                    Check(!Crossed(zone,scene(high),scene(high),mirror),"Wrong Bastok floor triggered");
                }
                char label[5]={}; std::memcpy(label,&v.id,4);
                std::cout << zone << " " << label << " -> " << route->toZone << " verified\n";
                ++audited;
            }
        }
        authoredVolumes.clear();
        Check(audited==14,"Bastok trigger coverage incomplete");
        struct NearbyCase { int zone; const char* path; const char* token; int destination; };
        const NearbyCase nearby[] = {
            {115, "ROM/0/127.DAT", "z370", 238},
            {115, "ROM/0/127.DAT", "z372", 240},
            {115, "ROM/0/127.DAT", "z374", 116},
            {115, "ROM/0/127.DAT", "z376", 116},
            {115, "ROM/0/127.DAT", "z378", 145},
            {115, "ROM/0/127.DAT", "z37a", 194},
            {115, "ROM/0/127.DAT", "z37c", 194},
            {115, "ROM/0/127.DAT", "z37e", 192},
            {115, "ROM/0/127.DAT", "z37g", 146},
            {116, "ROM/1/0.DAT", "z380", 241},
            {116, "ROM/1/0.DAT", "z382", 115},
            {116, "ROM/1/0.DAT", "z384", 115},
            {116, "ROM/1/0.DAT", "z386", 117},
            {116, "ROM/1/0.DAT", "z388", 194},
            {116, "ROM/1/0.DAT", "z38a", 192},
            {116, "ROM/1/0.DAT", "z38c", 194},
            {117, "ROM/1/1.DAT", "z390", 116},
            {117, "ROM/1/1.DAT", "z392", 119},
            {117, "ROM/1/1.DAT", "z394", 198},
            {117, "ROM/1/1.DAT", "z396", 118},
            {118, "ROM/0/71.DAT", "z3a0", 117},
            {118, "ROM/0/71.DAT", "z3a2", 249},
            {118, "ROM/0/71.DAT", "z3a4", 198},
            {118, "ROM/0/71.DAT", "z3a6", 213},
            {118, "ROM/0/71.DAT", "z3a8", 4},
            {145, "ROM/0/104.DAT", "z410", 115},
            {145, "ROM/0/104.DAT", "z412", 146},
            {146, "ROM/0/105.DAT", "z420", 145},
            {146, "ROM/0/105.DAT", "z422", 115},
            {194, "ROM/1/15.DAT", "z5e0", 115},
            {194, "ROM/1/15.DAT", "z5e2", 116},
            {194, "ROM/1/15.DAT", "z5e6", 116},
            {194, "ROM/1/15.DAT", "z5e8", 115},
            {192, "ROM/0/112.DAT", "z5c4", 116},
            {192, "ROM/0/112.DAT", "z5ca", 115},
            {192, "ROM/0/112.DAT", "z5cc", 169},
            {169, "ROM/0/65.DAT", "z4p0", 192},
            {169, "ROM/0/65.DAT", "z4p2", 239},
            {169, "ROM/0/65.DAT", "z4p4", 170},
            {169, "ROM/0/65.DAT", "z4p6", 170},
            {170, "ROM2/0/12.DAT", "z4q0", 169},
            {170, "ROM2/0/12.DAT", "z4q2", 169},
            {198, "ROM/0/75.DAT", "z5i0", 117},
            {198, "ROM/0/75.DAT", "z5i2", 118},
            {198, "ROM/0/75.DAT", "z5i4", 7},
            {238, "ROM/0/78.DAT", "z6m0", 115},
            {240, "ROM/0/80.DAT", "z6o0", 115},
            {241, "ROM/0/81.DAT", "z6p0", 116},
            {119, "ROM/0/101.DAT", "z3b0", 117},
            {249, "ROM/1/44.DAT", "z6x0", 118},
            {213, "ROM2/12/122.DAT", "z5x0", 118},
            {4, "ROM3/0/3.DAT", "z040", 118},
            {239, "ROM/0/79.DAT", "z6n6", 169},
            {7, "ROM3/0/6.DAT", "z070", 198},
        {238, "ROM/0/78.DAT", "z6m0", 115},
        {238, "ROM/0/78.DAT", "z6m2", 240},
        {238, "ROM/0/78.DAT", "z6m4", 239},
        {238, "ROM/0/78.DAT", "zmr6", 302},
        {239, "ROM/0/79.DAT", "z6n0", 238},
        {239, "ROM/0/79.DAT", "z6n2", 241},
        {239, "ROM/0/79.DAT", "z6n6", 169},
        {239, "ROM/0/79.DAT", "zmr8", 302},
        {240, "ROM/0/80.DAT", "z6o0", 115},
        {240, "ROM/0/80.DAT", "z6o2", 238},
        {240, "ROM/0/80.DAT", "z6o4", 241},
        {240, "ROM/0/80.DAT", "zmre", 302},
        {241, "ROM/0/81.DAT", "z6p0", 116},
        {241, "ROM/0/81.DAT", "z6p2", 240},
        {241, "ROM/0/81.DAT", "z6p4", 239},
        {241, "ROM/0/81.DAT", "zmrk", 302},
        {115, "ROM/0/127.DAT", "z370", 238},
        {115, "ROM/0/127.DAT", "z372", 240},
        {116, "ROM/1/0.DAT", "z380", 241},
        {169, "ROM/0/65.DAT", "z4p2", 239},
        {230, "ROM/1/31.DAT", "z6e0", 101},
        {230, "ROM/1/31.DAT", "zmr0", 300},
        {230, "ROM/1/31.DAT", "z6e2", 100},
        {230, "ROM/1/31.DAT", "z6e4", 231},
        {231, "ROM/1/32.DAT", "z6f0", 230},
        {231, "ROM/1/32.DAT", "z6f2", 100},
        {231, "ROM/1/32.DAT", "zmr2", 300},
        {231, "ROM/1/32.DAT", "z6f6", 232},
        {231, "ROM/1/32.DAT", "z6f8", 100},
        {231, "ROM/1/32.DAT", "z6fa", 2},
        {232, "ROM/0/113.DAT", "z6g0", 231},
        {232, "ROM/0/113.DAT", "zmr4", 300},
        {233, "ROM/1/33.DAT", "z6h2", 167},
        {101, "ROM/0/121.DAT", "z2t2", 230},
        {100, "ROM/0/120.DAT", "z2s0", 230},
        {100, "ROM/0/120.DAT", "z2s2", 231},
        {100, "ROM/0/120.DAT", "z2sa", 231},
        {2, "ROM3/0/1.DAT", "z024", 231},
        {167, "ROM/0/108.DAT", "z4n0", 233},
        {12, "ROM3/0/11.DAT", "z0c0", 11},
        {12, "ROM3/0/11.DAT", "z0c2", 11},
        {12, "ROM3/0/11.DAT", "z0c4", 11},
        {12, "ROM3/0/11.DAT", "z0c6", 11},
        {12, "ROM3/0/11.DAT", "z0c8", 11},
        {12, "ROM3/0/11.DAT", "z0ca", 11},
        {109, "ROM/0/125.DAT", "z310", 108},
        {103, "ROM/0/102.DAT", "z2v0", 108},
        {114, "ROM2/0/1.DAT", "z360", 173},
        {125, "ROM2/0/6.DAT", "z3h2", 173},
        {106, "ROM/0/123.DAT", "z2y0", 236},
        {106, "ROM/0/123.DAT", "z2y2", 107},
        {106, "ROM/0/123.DAT", "z2y4", 107},
        {106, "ROM/0/123.DAT", "z2y6", 108},
        {106, "ROM/0/123.DAT", "z2y8", 143},
        {106, "ROM/0/123.DAT", "z2ya", 191},
        {106, "ROM/0/123.DAT", "z2yc", 11},
        {106, "ROM/0/123.DAT", "z2ye", 11},
        {107, "ROM/0/124.DAT", "z2z0", 234},
        {107, "ROM/0/124.DAT", "z2z2", 235},
        {107, "ROM/0/124.DAT", "z2z4", 106},
        {107, "ROM/0/124.DAT", "z2z6", 106},
        {107, "ROM/0/124.DAT", "z2z8", 191},
        {108, "ROM/0/90.DAT", "z300", 106},
        {108, "ROM/0/90.DAT", "z302", 109},
        {108, "ROM/0/90.DAT", "z304", 103},
        {108, "ROM/0/90.DAT", "z306", 196},
        {191, "ROM/0/52.DAT", "z5b0", 107},
        {191, "ROM/0/52.DAT", "z5b2", 106},
        {143, "ROM/0/88.DAT", "z3z0", 106},
        {143, "ROM/0/88.DAT", "z3z2", 144},
        {143, "ROM/0/88.DAT", "z3z4", 144},
        {172, "ROM/1/11.DAT", "z4s0", 234},
        {172, "ROM/1/11.DAT", "z4s2", 173},
        {173, "ROM2/0/13.DAT", "z4t0", 172},
        {173, "ROM2/0/13.DAT", "z4t2", 114},
        {173, "ROM2/0/13.DAT", "z4t4", 125},
        {196, "ROM/1/16.DAT", "z5g0", 108},
        {196, "ROM/1/16.DAT", "z5g2", 11},
        {11, "ROM3/0/10.DAT", "z0b0", 106},
        {11, "ROM3/0/10.DAT", "z0b2", 106},
        {11, "ROM3/0/10.DAT", "z0b4", 196},
        {11, "ROM3/0/10.DAT", "z0b6", 12},
        {11, "ROM3/0/10.DAT", "z0b8", 12},
        {11, "ROM3/0/10.DAT", "z0ba", 12},
        {11, "ROM3/0/10.DAT", "z0bc", 12},
        {11, "ROM3/0/10.DAT", "z0be", 12},
        {11, "ROM3/0/10.DAT", "z0bg", 12},
        {144, "ROM/0/89.DAT", "z400", 143},
        {144, "ROM/0/89.DAT", "z402", 143},
        };
        for (const auto& test : nearby)
        {
            std::ifstream file(std::filesystem::path(argv[2]) / test.path, std::ios::binary);
            std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)), {});
            Check(!bytes.empty() && ZoneTriggerVolume::Decode(bytes.data(),bytes.size(),authoredVolumes),
                "Nearby zone RID decode failed");
            for (const auto& volume : authoredVolumes)
            {
                if (test.zone==12 || test.zone==109 || test.zone==103 || test.zone==114 || test.zone==125 ||
                    test.zone==100 || test.zone==101 || test.zone==2 || test.zone==167 ||
                    test.zone==119 || test.zone==7) break;
                int covered=0;
                for (const auto& line : lines)
                    if (line.fromZone==test.zone && VolumeFor(line)==&volume) ++covered;
                Check(covered==1,"Nearby active exit coverage incomplete or duplicated");
            }
            const Line* route = nullptr;
            int matches = 0;
            for (const auto& line : lines)
                if (line.fromZone == test.zone && std::strcmp(line.token,test.token)==0)
                { route=&line; ++matches; }
            Check(matches==1 && route->toZone==test.destination, "Nearby route destination incorrect");
            const auto* v=VolumeFor(*route);
            Check(v!=nullptr,"Nearby route has no authored exit volume");
            for (bool mirror : {false,true})
            {
                auto a=v->center,b=v->center;
                const bool thinX=v->size[0]<v->size[2];
                const Point normal=thinX ? Point{std::cos(v->yaw),0,-std::sin(v->yaw)} :
                    Point{std::sin(v->yaw),0,std::cos(v->yaw)};
                const float extent=(std::min)(v->size[0],v->size[2])*0.5f+1;
                for (int axis : {0,2}) { a[axis]-=normal[axis]*extent; b[axis]+=normal[axis]*extent; }
                auto scene=[mirror](Point p){return FFXICoordinateFrame::NativeDatToScene(p,mirror);};
                Check(Crossed(test.zone,scene(a),scene(b),mirror)==route &&
                    Crossed(test.zone,scene(b),scene(a),mirror)==route,"Nearby authored crossing missed");
                auto high=v->center; high[1]+=v->size[1]+1;
                Check(!Crossed(test.zone,scene(high),scene(high),mirror),"Nearby wrong-floor trigger fired");
            }
            const auto windurstRegion=[](int zone) {
                switch(zone) {
                case 115: case 116: case 117: case 118: case 145: case 146:
                case 194: case 192: case 169: case 170: case 198: return true;
                default: return false;
                }
            };
            if (windurstRegion(test.zone) || windurstRegion(route->toZone))
            {
                const NearbyCase* destination=nullptr;
                for (const auto& candidate : nearby)
                    if (candidate.zone==route->toZone) { destination=&candidate; break; }
                Check(destination!=nullptr,"Missing Windurst-region destination test DAT");
                std::ifstream arrivalFile(std::filesystem::path(argv[2])/destination->path,std::ios::binary);
                std::vector<unsigned char> arrivalBytes((std::istreambuf_iterator<char>(arrivalFile)),{});
                Check(!arrivalBytes.empty() && ZoneTriggerVolume::Decode(arrivalBytes.data(),arrivalBytes.size(),authoredVolumes),
                    "Windurst-region arrival RID decode failed");
                for (bool mirror : {false,true})
                {
                    const auto arrival=FFXICoordinateFrame::NativeDatToScene(route->arrival,mirror);
                    Check(!Crossed(route->toZone,arrival,arrival,mirror),"Windurst-region arrival immediately re-zoned");
                }
            }
        }
        // Jeuno: every city RID exit, four immediate field returns, and safe
        // arrivals. The four Mog routes share local interior 0.
        struct JeunoCase {
            int zone; const char* path; const char* token; int destination;
            const char* destinationPath; Point arrival; float heading;
        };
        const JeunoCase jeuno[] = {
            {243, "ROM/1/39.DAT", "z6r0", 244, "ROM/1/40.DAT", {42.996000f,-4.682000f,-32.190000f}, 2.719870f},
            {243, "ROM/1/39.DAT", "zmrm", 0, "ROM/1/20.DAT", {0.000000f,0.000000f,-10.000000f}, 4.712389f},
            {244, "ROM/1/40.DAT", "z6s0", 105, "ROM/0/122.DAT", {486.298000f,4.169000f,-159.953000f}, 3.141593f},
            {244, "ROM/1/40.DAT", "z6s2", 245, "ROM/1/41.DAT", {2.830000f,-4.799000f,57.689000f}, 0.633630f},
            {244, "ROM/1/40.DAT", "z6s4", 243, "ROM/1/39.DAT", {-0.025000f,-1.585000f,-107.260000f}, 4.712389f},
            {244, "ROM/1/40.DAT", "zmro", 0, "ROM/1/20.DAT", {0.000000f,0.000000f,-10.000000f}, 4.712389f},
            {245, "ROM/1/41.DAT", "z6t0", 110, "ROM/0/126.DAT", {339.741000f,21.798000f,608.166000f}, 1.570796f},
            {245, "ROM/1/41.DAT", "z6t2", 244, "ROM/1/40.DAT", {7.042000f,-0.642000f,-53.100000f}, 5.652728f},
            {245, "ROM/1/41.DAT", "z6t4", 246, "ROM/1/42.DAT", {-150.730000f,-3.899000f,-19.900000f}, 4.823146f},
            {245, "ROM/1/41.DAT", "zmru", 0, "ROM/1/20.DAT", {0.000000f,0.000000f,-10.000000f}, 4.712389f},
            {246, "ROM/1/42.DAT", "z6u0", 120, "ROM/1/2.DAT", {-574.647000f,-0.680000f,399.974000f}, 0.000000f},
            {246, "ROM/1/42.DAT", "z6u2", 245, "ROM/1/41.DAT", {27.891000f,-2.635000f,42.423000f}, 3.653041f},
            {246, "ROM/1/42.DAT", "z6u4", 126, "ROM/0/58.DAT", {139.997000f,8.903000f,-476.956000f}, 4.712389f},
            {246, "ROM/1/42.DAT", "zmrs", 0, "ROM/1/20.DAT", {0.000000f,0.000000f,-10.000000f}, 4.712389f},
            {105, "ROM/0/122.DAT", "z2x4", 244, "ROM/1/40.DAT", {-104.576000f,-2.286000f,187.365000f}, 1.047198f},
            {110, "ROM/0/126.DAT", "z326", 245, "ROM/1/41.DAT", {-121.659000f,-2.053000f,-198.247000f}, 5.235987f},
            {120, "ROM/1/2.DAT", "z3c4", 246, "ROM/1/42.DAT", {52.497000f,-1.579000f,-0.004000f}, 3.141593f},
            {126, "ROM/0/58.DAT", "z3i0", 246, "ROM/1/42.DAT", {-158.194000f,10.315000f,97.573000f}, 2.320520f},
        };
        for (const auto& test : jeuno)
        {
            std::ifstream sourceFile(std::filesystem::path(argv[2])/test.path,std::ios::binary);
            std::vector<unsigned char> sourceBytes((std::istreambuf_iterator<char>(sourceFile)),{});
            Check(!sourceBytes.empty() && ZoneTriggerVolume::Decode(sourceBytes.data(),sourceBytes.size(),authoredVolumes),
                "Jeuno source RID decode failed");
            if (test.zone>=243 && test.zone<=246)
            {
                const std::size_t expectedCount=test.zone==243 ? 2 : 4;
                Check(authoredVolumes.size()==expectedCount,"Jeuno active exit count changed");
                for (const auto& volume : authoredVolumes)
                {
                    int coverage=0;
                    for (const auto& line : lines)
                        if (line.fromZone==test.zone && VolumeFor(line)==&volume) ++coverage;
                    Check(coverage==1,"Jeuno active exit coverage missing or duplicated");
                }
            }
            const Line* route=nullptr;
            int matches=0;
            for (const auto& line : lines)
                if (line.fromZone==test.zone && std::strcmp(line.token,test.token)==0)
                { route=&line; ++matches; }
            Check(matches==1 && route->toZone==test.destination,"Jeuno destination incorrect");
            for (int axis=0;axis<3;++axis)
                Check(std::fabs(route->arrival[axis]-test.arrival[axis])<0.00002f,"Jeuno arrival reference mismatch");
            Check(route->heading==EncodeHeading(test.heading),"Jeuno heading reference mismatch");
            const auto* volume=VolumeFor(*route);
            Check(volume!=nullptr,"Jeuno authored volume missing");
            const auto v=*volume;
            for (float sign : {-1.f,1.f})
            {
                auto inside=v.center,face=v.center,outside=v.center;
                inside[1]+=sign*(v.size[1]*.5f-.01f);
                face[1]+=sign*v.size[1]*.5f;
                outside[1]+=sign*(v.size[1]*.5f+.01f);
                Check(MarkerFloorEligible(*route,inside),"Jeuno dots hidden inside authored floor range");
                Check(MarkerFloorEligible(*route,face)==ZoneTriggerVolume::Contains(v,face),
                      "Jeuno marker face differs from RID floating-point bounds");
                Check(!MarkerFloorEligible(*route,outside),
                      "Jeuno dots leak outside authored floor range");
            }
            if (test.zone==244 && std::strcmp(test.token,"z6s0")==0)
            {
                auto street=v.center;street[1]=0;
                Check(std::fabs(street[1]-v.center[1])>4 && MarkerFloorEligible(*route,street),
                      "Upper Jeuno slope still rejects dots with legacy four-unit cutoff");
            }
            for (bool mirror : {false,true})
            {
                auto scene=[mirror](Point point){return FFXICoordinateFrame::NativeDatToScene(point,mirror);};
                auto start=v.center,end=v.center;
                const Point normal=v.size[0]<v.size[2] ? Point{std::cos(v.yaw),0,-std::sin(v.yaw)} :
                    Point{std::sin(v.yaw),0,std::cos(v.yaw)};
                const float distance=(std::min)(v.size[0],v.size[2])*0.5f+1;
                for (int axis : {0,2}) { start[axis]-=normal[axis]*distance; end[axis]+=normal[axis]*distance; }
                Check(Crossed(test.zone,scene(start),scene(end),mirror)==route &&
                      Crossed(test.zone,scene(end),scene(start),mirror)==route,"Jeuno volume crossing failed");
                auto high=v.center; high[1]+=v.size[1]+1;
                Check(!Crossed(test.zone,scene(high),scene(high),mirror),"Jeuno wrong-floor trigger fired");
                if (test.destination==0)
                {
                    auto returnPoint=v.center;
                    for (int axis : {0,2})
                        returnPoint[axis]-=route->outward[axis]*((std::min)(v.size[0],v.size[2])*0.5f+3);
                    Check(!Crossed(test.zone,scene(returnPoint),scene(returnPoint),mirror),
                        "Jeuno remembered Mog return overlaps exit");
                }
            }
            std::ifstream destinationFile(std::filesystem::path(argv[2])/test.destinationPath,std::ios::binary);
            std::vector<unsigned char> destinationBytes((std::istreambuf_iterator<char>(destinationFile)),{});
            Check(!destinationBytes.empty() && ZoneTriggerVolume::Decode(destinationBytes.data(),destinationBytes.size(),authoredVolumes),
                "Jeuno destination RID decode failed");
            for (bool mirror : {false,true})
            {
                const auto arrival=FFXICoordinateFrame::NativeDatToScene(route->arrival,mirror);
                Check(!Crossed(test.destination,arrival,arrival,mirror),"Jeuno arrival immediately re-zoned");
            }
        }
        std::cout << "Jeuno: 14 authored city exits and 4 field returns verified.\n";
        // The surrounding fields have 26 exits total (including the four
        // city returns above); every active RID must resolve exactly once.
        const JeunoCase surroundingJeuno[] = {
            {105, "ROM/0/122.DAT", "z2x0", 104, "ROM/0/114.DAT", {584.034000f,-8.476000f,558.296000f}, 3.141593f},
            {105, "ROM/0/122.DAT", "z2x2", 110, "ROM/0/126.DAT", {-39.230000f,21.178000f,635.328000f}, 1.570796f},
            {105, "ROM/0/122.DAT", "z2x6", 111, "ROM/0/72.DAT", {17.331000f,-2.813000f,-654.230000f}, 4.712389f},
            {105, "ROM/0/122.DAT", "z2x8", 195, "ROM/0/77.DAT", {-438.878000f,-26.091000f,540.004000f}, 3.141593f},
            {105, "ROM/0/122.DAT", "z2xa", 195, "ROM/0/77.DAT", {-160.776000f,-34.077000f,259.998000f}, 0.000000f},
            {105, "ROM/0/122.DAT", "z2xc", 195, "ROM/0/77.DAT", {339.992000f,-50.075000f,321.016000f}, 1.570796f},
            {105, "ROM/0/122.DAT", "z2xe", 195, "ROM/0/77.DAT", {59.989000f,-34.062000f,160.973000f}, 1.570796f},
            {105, "ROM/0/122.DAT", "z2xg", 195, "ROM/0/77.DAT", {-300.010000f,-26.100000f,119.191000f}, 4.712389f},
            {105, "ROM/0/122.DAT", "z2xi", 195, "ROM/0/77.DAT", {420.035000f,-58.114000f,-119.510000f}, 4.712389f},
            {105, "ROM/0/122.DAT", "z2xk", 195, "ROM/0/77.DAT", {439.955000f,-49.607000f,-300.015000f}, 3.141593f},
            {105, "ROM/0/122.DAT", "z2xm", 195, "ROM/0/77.DAT", {-558.931000f,-18.096000f,99.999000f}, 3.141593f},
            {110, "ROM/0/126.DAT", "z320", 109, "ROM/0/125.DAT", {547.841000f,23.192000f,696.323000f}, 3.141593f},
            {110, "ROM/0/126.DAT", "z322", 105, "ROM/0/122.DAT", {479.744000f,3.338000f,-359.840000f}, 4.712389f},
            {110, "ROM/0/126.DAT", "z324", 120, "ROM/1/2.DAT", {-475.679000f,-3.228000f,201.675000f}, 5.934119f},
            {110, "ROM/0/126.DAT", "z328", 197, "ROM/0/76.DAT", {380.617000f,-34.610000f,4.581000f}, 1.570796f},
            {110, "ROM/0/126.DAT", "z32a", 197, "ROM/0/76.DAT", {84.741000f,-2.471000f,379.574000f}, 3.141593f},
            {120, "ROM/1/2.DAT", "z3c0", 119, "ROM/0/101.DAT", {-454.135000f,28.409000f,657.790000f}, 1.570796f},
            {120, "ROM/1/2.DAT", "z3c2", 110, "ROM/0/126.DAT", {372.582000f,19.363000f,475.631000f}, 2.530727f},
            {120, "ROM/1/2.DAT", "z3c6", 200, "ROM/1/17.DAT", {-380.035000f,-13.548000f,398.032000f}, 1.570796f},
            {120, "ROM/1/2.DAT", "z3c8", 200, "ROM/1/17.DAT", {19.991000f,-19.566000f,438.008000f}, 1.570796f},
            {126, "ROM/0/58.DAT", "z3i2", 127, "ROM/1/3.DAT", {358.134000f,24.806000f,-60.001000f}, 3.141593f},
            {126, "ROM/0/58.DAT", "z3i4", 184, "ROM/1/13.DAT", {460.022000f,-1.770000f,-103.442000f}, 4.712389f},
            {104, "ROM/0/114.DAT", "z2w2", 105, "ROM/0/122.DAT", {-439.393000f,-11.765000f,-239.958000f}, 0.000000f},
            {111, "ROM/0/72.DAT", "z334", 105, "ROM/0/122.DAT", {-500.451000f,-39.710000f,504.894000f}, 1.119254f},
            {195, "ROM/0/77.DAT", "z5f0", 105, "ROM/0/122.DAT", {-693.609000f,-14.583000f,173.590000f}, 0.785398f},
            {195, "ROM/0/77.DAT", "z5f2", 105, "ROM/0/122.DAT", {-464.853000f,-22.563000f,335.136000f}, 5.497787f},
            {195, "ROM/0/77.DAT", "z5f4", 105, "ROM/0/122.DAT", {-214.550000f,-6.542000f,134.555000f}, 3.926991f},
            {195, "ROM/0/77.DAT", "z5f6", 105, "ROM/0/122.DAT", {54.594000f,0.978000f,134.594000f}, 5.497787f},
            {195, "ROM/0/77.DAT", "z5f8", 105, "ROM/0/122.DAT", {-133.604000f,-6.624000f,-13.814000f}, 5.497787f},
            {195, "ROM/0/77.DAT", "z5fa", 105, "ROM/0/122.DAT", {-226.393000f,-22.628000f,-66.378000f}, 2.356194f},
            {195, "ROM/0/77.DAT", "z5fc", 105, "ROM/0/122.DAT", {134.402000f,9.438000f,-374.405000f}, 0.785398f},
            {195, "ROM/0/77.DAT", "z5fe", 105, "ROM/0/122.DAT", {165.521000f,22.218000f,-603.006000f}, 5.174027f},
            {109, "ROM/0/125.DAT", "z312", 110, "ROM/0/126.DAT", {-117.506000f,-3.282000f,-920.604000f}, 0.000000f},
            {119, "ROM/0/101.DAT", "z3b2", 120, "ROM/1/2.DAT", {520.391000f,-15.885000f,-502.917000f}, 4.712389f},
            {197, "ROM/0/76.DAT", "z5h0", 110, "ROM/0/126.DAT", {-381.747000f,-31.068000f,-788.092000f}, 4.945691f},
            {197, "ROM/0/76.DAT", "z5h2", 110, "ROM/0/126.DAT", {-788.664000f,-39.278000f,-502.067000f}, 6.062613f},
            {200, "ROM/1/17.DAT", "z5k0", 120, "ROM/1/2.DAT", {-103.991000f,-25.956000f,-425.221000f}, 4.712389f},
            {200, "ROM/1/17.DAT", "z5k2", 120, "ROM/1/2.DAT", {264.002000f,-1.748000f,-387.444000f}, 4.712389f},
            {127, "ROM/1/3.DAT", "z3j0", 126, "ROM/0/58.DAT", {-638.714000f,16.865000f,459.966000f}, 0.000000f},
            {184, "ROM/1/13.DAT", "z540", 126, "ROM/0/58.DAT", {-286.271000f,-21.619000f,320.084000f}, 0.000000f},
        };
        for (const auto& test : surroundingJeuno)
        {
            std::cout << "Checking nearby Jeuno exit " << test.zone << ':' << test.token << '\n';
            std::ifstream sourceFile(std::filesystem::path(argv[2])/test.path,std::ios::binary);
            std::vector<unsigned char> sourceBytes((std::istreambuf_iterator<char>(sourceFile)),{});
            Check(!sourceBytes.empty() && ZoneTriggerVolume::Decode(sourceBytes.data(),sourceBytes.size(),authoredVolumes),
                "Nearby Jeuno source RID decode failed");
            const int expectedCount=test.zone==105 ? 12 : test.zone==110 ? 6 :
                test.zone==120 ? 5 : test.zone==126 ? 3 : 0;
            if (expectedCount)
            {
                Check(authoredVolumes.size()==expectedCount,"Nearby Jeuno active exit count changed");
                for (const auto& volume : authoredVolumes)
                {
                    int coverage=0;
                    for (const auto& line : lines)
                        if (line.fromZone==test.zone && VolumeFor(line)==&volume) ++coverage;
                    Check(coverage==1,"Nearby Jeuno exit coverage missing or duplicated");
                }
            }
            const Line* route=nullptr;
            int matches=0;
            for (const auto& line : lines)
                if (line.fromZone==test.zone && std::strcmp(line.token,test.token)==0)
                { route=&line; ++matches; }
            Check(matches==1 && route->toZone==test.destination,"Nearby Jeuno destination incorrect");
            for (int axis=0;axis<3;++axis)
                Check(std::fabs(route->arrival[axis]-test.arrival[axis])<0.00002f,"Nearby Jeuno arrival mismatch");
            Check(route->heading==EncodeHeading(test.heading),"Nearby Jeuno heading mismatch");
            const auto* volume=VolumeFor(*route);
            Check(volume!=nullptr,"Nearby Jeuno authored volume missing");
            const auto v=*volume;
            for (bool mirror : {false,true})
            {
                auto scene=[mirror](Point point){return FFXICoordinateFrame::NativeDatToScene(point,mirror);};
                auto start=v.center,end=v.center;
                const Point normal=v.size[0]<v.size[2] ? Point{std::cos(v.yaw),0,-std::sin(v.yaw)} :
                    Point{std::sin(v.yaw),0,std::cos(v.yaw)};
                const float distance=(std::min)(v.size[0],v.size[2])*0.5f+1;
                for (int axis : {0,2}) { start[axis]-=normal[axis]*distance; end[axis]+=normal[axis]*distance; }
                Check(Crossed(test.zone,scene(start),scene(end),mirror)==route &&
                      Crossed(test.zone,scene(end),scene(start),mirror)==route,"Nearby Jeuno crossing failed");
                auto high=v.center; high[1]+=v.size[1]+1;
                Check(!Crossed(test.zone,scene(high),scene(high),mirror),"Nearby Jeuno wrong-floor trigger fired");
            }
            std::ifstream destinationFile(std::filesystem::path(argv[2])/test.destinationPath,std::ios::binary);
            std::vector<unsigned char> destinationBytes((std::istreambuf_iterator<char>(destinationFile)),{});
            Check(!destinationBytes.empty() && ZoneTriggerVolume::Decode(destinationBytes.data(),destinationBytes.size(),authoredVolumes),
                "Nearby Jeuno destination RID decode failed");
            for (const auto& exit : authoredVolumes)
                Check(!ZoneTriggerVolume::Contains(exit,route->arrival),"Nearby Jeuno arrival overlaps an exit volume");
            for (bool mirror : {false,true})
            {
                const auto arrival=FFXICoordinateFrame::NativeDatToScene(route->arrival,mirror);
                Check(!Crossed(test.destination,arrival,arrival,mirror),"Nearby Jeuno arrival immediately re-zoned");
            }
        }
        std::cout << "Nearby Jeuno: all 26 field exits and 18 reciprocal returns verified.\n";
        // Complete San d'Oria countryside/dungeon coverage and four peripheral returns.
        const JeunoCase surroundingSandoria[] = {
            {100, "ROM/0/120.DAT", "z2s0", 230, "ROM/1/31.DAT", {-110.465000f,-2.083000f,-54.469000f}, 5.497787f},
            {100, "ROM/0/120.DAT", "z2s2", 231, "ROM/1/32.DAT", {-248.893000f,4.640000f,44.026000f}, 0.000000f},
            {100, "ROM/0/120.DAT", "z2s4", 101, "ROM/0/121.DAT", {-16.472000f,-54.180000f,140.003000f}, 0.000000f},
            {100, "ROM/0/120.DAT", "z2s6", 101, "ROM/0/121.DAT", {81.582000f,-29.242000f,-199.921000f}, 0.000000f},
            {100, "ROM/0/120.DAT", "z2s8", 140, "ROM/0/95.DAT", {100.002000f,-2.062000f,-38.021000f}, 4.712389f},
            {100, "ROM/0/120.DAT", "z2sa", 231, "ROM/1/32.DAT", {-234.393000f,-5.741000f,106.009000f}, 0.000000f},
            {100, "ROM/0/120.DAT", "z2sc", 102, "ROM/0/115.DAT", {-559.978000f,-3.098000f,680.592000f}, 1.570796f},
            {100, "ROM/0/120.DAT", "z2se", 167, "ROM/0/108.DAT", {-379.994000f,-9.351000f,-237.249000f}, 4.712389f},
            {101, "ROM/0/121.DAT", "z2t0", 100, "ROM/0/120.DAT", {-24.427000f,-53.107000f,140.000000f}, 3.141593f},
            {101, "ROM/0/121.DAT", "z2t2", 230, "ROM/1/31.DAT", {110.234000f,-2.127000f,-54.223000f}, 3.926991f},
            {101, "ROM/0/121.DAT", "z2t4", 100, "ROM/0/120.DAT", {83.913000f,-26.756000f,-200.033000f}, 3.141593f},
            {101, "ROM/0/121.DAT", "z2t6", 190, "ROM/1/14.DAT", {-129.996000f,-2.529000f,256.499000f}, 1.570796f},
            {101, "ROM/0/121.DAT", "z2t8", 190, "ROM/1/14.DAT", {57.415000f,5.482000f,300.784000f}, 1.570796f},
            {101, "ROM/0/121.DAT", "z2ta", 166, "ROM/1/10.DAT", {-139.986000f,2.162000f,-321.508000f}, 4.712389f},
            {102, "ROM/0/115.DAT", "z2u0", 100, "ROM/0/120.DAT", {-559.963000f,-7.062000f,-602.479000f}, 4.712389f},
            {102, "ROM/0/115.DAT", "z2u2", 104, "ROM/0/114.DAT", {-583.857000f,-9.344000f,-441.110000f}, 0.000000f},
            {102, "ROM/0/115.DAT", "z2u4", 103, "ROM/0/102.DAT", {760.069000f,-9.949000f,409.269000f}, 1.570796f},
            {102, "ROM/0/115.DAT", "z2u6", 193, "ROM/0/92.DAT", {-76.839000f,-1.696000f,659.969000f}, 3.141593f},
            {102, "ROM/0/115.DAT", "z2u8", 193, "ROM/0/92.DAT", {179.906000f,30.349000f,596.605000f}, 1.570796f},
            {102, "ROM/0/115.DAT", "z2ua", 193, "ROM/0/92.DAT", {-200.326000f,-29.608000f,-219.847000f}, 0.000000f},
            {103, "ROM/0/102.DAT", "z2v0", 108, "ROM/0/90.DAT", {-160.124000f,68.250000f,883.812000f}, 1.570796f},
            {103, "ROM/0/102.DAT", "z2v2", 102, "ROM/0/115.DAT", {159.989000f,28.406000f,-760.190000f}, 4.712389f},
            {103, "ROM/0/102.DAT", "z2v4", 248, "ROM/1/43.DAT", {17.981000f,-16.806000f,99.830000f}, 1.570796f},
            {103, "ROM/0/102.DAT", "z2v6", 212, "ROM2/12/121.DAT", {280.502000f,-51.820000f,100.019000f}, 0.000000f},
            {104, "ROM/0/114.DAT", "z2w0", 102, "ROM/0/115.DAT", {795.118000f,28.221000f,-39.978000f}, 3.141593f},
            {104, "ROM/0/114.DAT", "z2w2", 105, "ROM/0/122.DAT", {-439.393000f,-11.765000f,-239.958000f}, 0.000000f},
            {104, "ROM/0/114.DAT", "z2w4", 190, "ROM/1/14.DAT", {242.012000f,5.305000f,340.059000f}, 3.141593f},
            {104, "ROM/0/114.DAT", "z2w6", 149, "ROM/0/99.DAT", {218.468000f,-2.302000f,-3.871000f}, 1.570796f},
            {104, "ROM/0/114.DAT", "z2w8", 2, "ROM3/0/1.DAT", {6.509000f,-9.163000f,-819.333000f}, 5.846853f},
            {104, "ROM/0/114.DAT", "z2wa", 2, "ROM3/0/1.DAT", {-419.796000f,-8.943000f,-554.553000f}, 5.061455f},
            {139, "ROM/1/6.DAT", "z3v0", 142, "ROM/1/8.DAT", {439.814000f,-42.481000f,169.755000f}, 3.141593f},
            {139, "ROM/1/6.DAT", "z3v2", 142, "ROM/1/8.DAT", {437.235000f,-42.063000f,68.564000f}, 2.587814f},
            {140, "ROM/0/95.DAT", "z3w0", 100, "ROM/0/120.DAT", {-738.178000f,-67.173000f,619.325000f}, 0.905795f},
            {140, "ROM/0/95.DAT", "z3w2", 142, "ROM/1/8.DAT", {-255.961000f,-2.711000f,-69.828000f}, 3.141593f},
            {140, "ROM/0/95.DAT", "z3w4", 141, "ROM/1/7.DAT", {-161.497000f,-21.977000f,19.985000f}, 0.000000f},
            {141, "ROM/1/7.DAT", "z3x0", 140, "ROM/0/95.DAT", {125.852000f,-22.097000f,180.403000f}, 3.141593f},
            {141, "ROM/1/7.DAT", "z3x2", 142, "ROM/1/8.DAT", {276.235000f,-42.359000f,149.944000f}, 0.000000f},
            {141, "ROM/1/7.DAT", "z3x4", 142, "ROM/1/8.DAT", {295.374000f,-26.691000f,249.657000f}, 0.000000f},
            {141, "ROM/1/7.DAT", "z3x6", 142, "ROM/1/8.DAT", {55.237000f,-14.923000f,70.061000f}, 0.000000f},
            {142, "ROM/1/8.DAT", "z3y0", 141, "ROM/1/7.DAT", {219.949000f,-86.032000f,19.489000f}, 3.141593f},
            {142, "ROM/1/8.DAT", "z3y2", 141, "ROM/1/7.DAT", {226.688000f,-46.417000f,101.045000f}, 3.141593f},
            {142, "ROM/1/8.DAT", "z3y4", 141, "ROM/1/7.DAT", {6.587000f,-22.088000f,-107.077000f}, 3.141593f},
            {142, "ROM/1/8.DAT", "z3y6", 140, "ROM/0/95.DAT", {-2.049000f,-2.202000f,-60.185000f}, 0.000000f},
            {142, "ROM/1/8.DAT", "z3y8", 139, "ROM/1/6.DAT", {-536.363000f,157.713000f,-210.819000f}, 0.000000f},
            {142, "ROM/1/8.DAT", "z3ya", 139, "ROM/1/6.DAT", {444.270000f,-42.210000f,68.513000f}, 0.460064f},
            {149, "ROM/0/99.DAT", "z450", 104, "ROM/0/114.DAT", {-239.556000f,-10.381000f,-591.281000f}, 4.712389f},
            {149, "ROM/0/99.DAT", "z452", 150, "ROM/0/100.DAT", {261.354000f,-8.792000f,23.124000f}, 4.375854f},
            {149, "ROM/0/99.DAT", "z454", 150, "ROM/0/100.DAT", {203.232000f,-17.416000f,-260.029000f}, 0.000000f},
            {149, "ROM/0/99.DAT", "z456", 150, "ROM/0/100.DAT", {-43.718000f,-18.034000f,-101.104000f}, 2.775247f},
            {149, "ROM/0/99.DAT", "z458", 150, "ROM/0/100.DAT", {37.269000f,-17.565000f,-380.190000f}, 3.141593f},
            {149, "ROM/0/99.DAT", "z45a", 150, "ROM/0/100.DAT", {77.537000f,-17.565000f,-180.037000f}, 3.141593f},
            {150, "ROM/0/100.DAT", "z460", 149, "ROM/0/99.DAT", {282.292000f,2.498000f,-17.908000f}, 0.000000f},
            {150, "ROM/0/100.DAT", "z462", 149, "ROM/0/99.DAT", {177.745000f,2.918000f,-238.573000f}, 4.712389f},
            {150, "ROM/0/100.DAT", "z464", 149, "ROM/0/99.DAT", {-22.086000f,3.454000f,-80.150000f}, 4.712389f},
            {150, "ROM/0/100.DAT", "z466", 149, "ROM/0/99.DAT", {57.952000f,3.204000f,-359.422000f}, 4.712389f},
            {150, "ROM/0/100.DAT", "z468", 149, "ROM/0/99.DAT", {93.179000f,2.233000f,-158.973000f}, 4.712389f},
            {166, "ROM/1/10.DAT", "z4m0", 101, "ROM/0/121.DAT", {818.507000f,-66.774000f,659.680000f}, 2.181662f},
            {166, "ROM/1/10.DAT", "z4m2", 111, "ROM/0/72.DAT", {-538.233000f,-42.189000f,-377.391000f}, 0.000000f},
            {167, "ROM/0/108.DAT", "z4n0", 233, "ROM/1/33.DAT", {14.872000f,8.918000f,24.002000f}, 0.000000f},
            {167, "ROM/0/108.DAT", "z4n2", 100, "ROM/0/120.DAT", {-712.746000f,-40.500000f,18.408000f}, 0.000000f},
            {190, "ROM/1/14.DAT", "z5a0", 101, "ROM/0/121.DAT", {200.015000f,-3.187000f,-536.074000f}, 4.712389f},
            {190, "ROM/1/14.DAT", "z5a2", 101, "ROM/0/121.DAT", {660.078000f,-21.883000f,-588.047000f}, 4.712389f},
            {190, "ROM/1/14.DAT", "z5a4", 104, "ROM/0/114.DAT", {-339.471000f,-6.782000f,462.074000f}, 1.047198f},
            {193, "ROM/0/92.DAT", "z5d0", 102, "ROM/0/115.DAT", {-272.118000f,21.715000f,98.859000f}, 0.000000f},
            {193, "ROM/0/92.DAT", "z5d2", 102, "ROM/0/115.DAT", {-59.413000f,28.699000f,151.634000f}, 4.712389f},
            {193, "ROM/0/92.DAT", "z5d4", 102, "ROM/0/115.DAT", {60.020000f,37.761000f,-316.183000f}, 1.570796f},
            {248, "ROM/1/43.DAT", "z6w0", 103, "ROM/0/102.DAT", {60.989000f,-4.898000f,-151.001000f}, 4.712389f},
            {2, "ROM3/0/1.DAT", "z020", 104, "ROM/0/114.DAT", {341.844000f,-5.005000f,15.117000f}, 4.188790f},
            {2, "ROM3/0/1.DAT", "z022", 104, "ROM/0/114.DAT", {-536.152000f,1.163000f,336.587000f}, 0.872665f},
            {111, "ROM/0/72.DAT", "z330", 166, "ROM/1/10.DAT", {302.778000f,-68.131000f,257.759000f}, 3.141593f},
            {212, "ROM2/12/121.DAT", "z5w2", 103, "ROM/0/102.DAT", {-772.018000f,-9.873000f,19.101000f}, 3.356547f},
        };
        for (const auto& test : surroundingSandoria)
        {
            std::cout << "Checking San d'Oria-region exit " << test.zone << ':' << test.token << '\n';
            std::ifstream sourceFile(std::filesystem::path(argv[2])/test.path,std::ios::binary);
            std::vector<unsigned char> sourceBytes((std::istreambuf_iterator<char>(sourceFile)),{});
            Check(!sourceBytes.empty() && ZoneTriggerVolume::Decode(sourceBytes.data(),sourceBytes.size(),authoredVolumes),
                "San d'Oria-region source RID decode failed");
            const int expectedCount=test.zone==100 ? 8 :
                test.zone==101 ? 6 :
                test.zone==102 ? 6 :
                test.zone==103 ? 4 :
                test.zone==104 ? 6 :
                test.zone==139 ? 2 :
                test.zone==140 ? 3 :
                test.zone==141 ? 4 :
                test.zone==142 ? 6 :
                test.zone==149 ? 6 :
                test.zone==150 ? 5 :
                test.zone==166 ? 2 :
                test.zone==167 ? 2 :
                test.zone==190 ? 3 :
                test.zone==193 ? 3 :
                test.zone==248 ? 1 : 0;
            if (expectedCount)
            {
                Check(authoredVolumes.size()==expectedCount,"San d'Oria-region active exit count changed");
                for (const auto& volume : authoredVolumes)
                {
                    int coverage=0;
                    for (const auto& line : lines)
                        if (line.fromZone==test.zone && VolumeFor(line)==&volume) ++coverage;
                    Check(coverage==1,"San d'Oria-region exit coverage missing or duplicated");
                }
            }
            const Line* route=nullptr;
            int matches=0;
            for (const auto& line : lines)
                if (line.fromZone==test.zone && std::strcmp(line.token,test.token)==0)
                { route=&line; ++matches; }
            Check(matches==1 && route->toZone==test.destination,"San d'Oria-region destination incorrect");
            for (int axis=0;axis<3;++axis)
                Check(std::fabs(route->arrival[axis]-test.arrival[axis])<0.00002f,"San d'Oria-region arrival mismatch");
            Check(route->heading==EncodeHeading(test.heading),"San d'Oria-region heading mismatch");
            const auto* volume=VolumeFor(*route);
            Check(volume!=nullptr,"San d'Oria-region authored volume missing");
            const auto v=*volume;
            for (bool mirror : {false,true})
            {
                auto scene=[mirror](Point point){return FFXICoordinateFrame::NativeDatToScene(point,mirror);};
                auto start=v.center,end=v.center;
                const Point normal=v.size[0]<v.size[2] ? Point{std::cos(v.yaw),0,-std::sin(v.yaw)} :
                    Point{std::sin(v.yaw),0,std::cos(v.yaw)};
                const float distance=(std::min)(v.size[0],v.size[2])*0.5f+1;
                for (int axis : {0,2}) { start[axis]-=normal[axis]*distance; end[axis]+=normal[axis]*distance; }
                Check(Crossed(test.zone,scene(start),scene(end),mirror)==route &&
                      Crossed(test.zone,scene(end),scene(start),mirror)==route,"San d'Oria-region crossing failed");
                auto high=v.center; high[1]+=v.size[1]+1;
                Check(!Crossed(test.zone,scene(high),scene(high),mirror),"San d'Oria-region wrong-floor trigger fired");
            }
            std::ifstream destinationFile(std::filesystem::path(argv[2])/test.destinationPath,std::ios::binary);
            std::vector<unsigned char> destinationBytes((std::istreambuf_iterator<char>(destinationFile)),{});
            Check(!destinationBytes.empty() && ZoneTriggerVolume::Decode(destinationBytes.data(),destinationBytes.size(),authoredVolumes),
                "San d'Oria-region destination RID decode failed");
            for (const auto& exit : authoredVolumes)
                Check(!ZoneTriggerVolume::Contains(exit,route->arrival),"San d'Oria-region arrival overlaps an exit volume");
            for (bool mirror : {false,true})
            {
                const auto arrival=FFXICoordinateFrame::NativeDatToScene(route->arrival,mirror);
                Check(!Crossed(test.destination,arrival,arrival,mirror),"San d'Oria-region arrival immediately re-zoned");
            }
        }
        std::cout << "San d\'Oria region: all 67 exits and four peripheral returns verified.\n";
        authoredVolumes.clear();
        for (const auto& test : mainlandExitCases)
        {
            std::cout << "Checking mainland exit " << test.zone << ':' << test.token << '\n';
            std::ifstream sourceFile(std::filesystem::path(argv[2])/test.path,std::ios::binary);
            std::vector<unsigned char> sourceBytes((std::istreambuf_iterator<char>(sourceFile)),{});
            std::vector<ZoneTriggerVolume::Volume> allSource;
            Check(!sourceBytes.empty() && ZoneTriggerVolume::Decode(sourceBytes.data(),sourceBytes.size(),allSource,true),
                  "Mainland source RID decode failed");
            Check(ZoneTriggerVolume::Decode(sourceBytes.data(),sourceBytes.size(),authoredVolumes),
                  "Mainland active RID decode failed");
            if (test.expectedExitCount)
            {
                Check(authoredVolumes.size()==test.expectedExitCount,"Mainland active exit count changed");
                for (const auto& volume : authoredVolumes)
                {
                    int matches=0;
                    for (const auto& route : lines)
                        if (route.fromZone==test.zone && VolumeFor(route)==&volume) ++matches;
                    Check(matches==1,"Mainland exit mapping missing or duplicated");
                }
                for (const auto& route : lines)
                    if (route.fromZone==test.zone)
                        Check(VolumeFor(route)!=nullptr,"Mainland source still has a placeholder route");
            }
            const Line* route=nullptr;
            int matches=0;
            for (const auto& candidate : lines)
                if (candidate.fromZone==test.zone && std::strcmp(candidate.token,test.token)==0)
                {route=&candidate; ++matches;}
            Check(matches==1 && route && route->toZone==test.destination,"Mainland destination mismatch");
            for (int axis=0;axis<3;++axis)
                Check(std::fabs(route->arrival[axis]-test.arrival[axis])<.00002f,"Mainland arrival mismatch");
            Check(route->heading==EncodeHeading(test.heading),"Mainland heading mismatch");
            const auto* volume=VolumeFor(*route);
            Check(volume!=nullptr,"Mainland marker has no authored volume");
            const auto v=*volume;
            Point normal=v.size[0]<v.size[2] ? Point{std::cos(v.yaw),0,-std::sin(v.yaw)} :
                Point{std::sin(v.yaw),0,std::cos(v.yaw)};
            const float dot=normal[0]*route->outward[0]+normal[2]*route->outward[2];
            Check(std::fabs(dot)>.98f,"Mainland marker normal not aligned to trigger");
            if (dot<0) for (auto& value : normal) value=-value;
            const ZoneTriggerVolume::Volume* closest=nullptr;
            float closestDistance=1e30f;
            for (const auto& arrival : allSource)
            {
                if (arrival.target) continue;
                float distance=0;
                for (int axis=0;axis<3;++axis) distance+=(arrival.center[axis]-v.center[axis])*(arrival.center[axis]-v.center[axis]);
                if (distance<closestDistance) {closest=&arrival;closestDistance=distance;}
            }
            Check(closest && (closest->center[0]-v.center[0])*normal[0]+
                (closest->center[2]-v.center[2])*normal[2]<0,"Mainland dots approach side reversed");
            for (bool mirror : {false,true})
            {
                auto scene=[mirror](Point p){return FFXICoordinateFrame::NativeDatToScene(p,mirror);};
                auto start=v.center,end=v.center;
                const float distance=(std::min)(v.size[0],v.size[2])*.5f+1;
                for (int axis : {0,2}) {start[axis]-=normal[axis]*distance;end[axis]+=normal[axis]*distance;}
                Check(Crossed(test.zone,scene(start),scene(end),mirror)==route &&
                      Crossed(test.zone,scene(end),scene(start),mirror)==route,"Mainland volume crossing failed");
                Check(Crossed(test.zone,scene(v.center),scene(v.center),mirror)==route,"Mainland stationary hit failed");
                auto high=v.center;high[1]+=v.size[1]+1;
                Check(!Crossed(test.zone,scene(high),scene(high),mirror),"Mainland wrong-floor hit");
            }
            std::ifstream destinationFile(std::filesystem::path(argv[2])/test.destinationPath,std::ios::binary);
            std::vector<unsigned char> destinationBytes((std::istreambuf_iterator<char>(destinationFile)),{});
            std::vector<ZoneTriggerVolume::Volume> allDestination;
            Check(!destinationBytes.empty() && ZoneTriggerVolume::Decode(destinationBytes.data(),destinationBytes.size(),allDestination,true),
                  "Mainland destination RID decode failed");
            bool targetFound=false;
            for (const auto& arrival : allDestination)
                if (arrival.id==v.target && arrival.target==0)
                {
                    targetFound=true;
                    for (int axis=0;axis<3;++axis)
                        Check(std::fabs(arrival.center[axis]-route->arrival[axis])<.002f,"Mainland RID target arrival mismatch");
                }
            Check(targetFound,"Mainland RID target missing from destination");
            Check(ZoneTriggerVolume::Decode(destinationBytes.data(),destinationBytes.size(),authoredVolumes),
                  "Mainland destination exits decode failed");
            for (const auto& exit : authoredVolumes)
                Check(!ZoneTriggerVolume::Contains(exit,route->arrival),"Mainland arrival inside an exit");
            for (bool mirror : {false,true})
            {
                const auto arrival=FFXICoordinateFrame::NativeDatToScene(route->arrival,mirror);
                Check(!Crossed(test.destination,arrival,arrival,mirror),"Mainland arrival immediately re-zoned");
            }
        }
        authoredVolumes.clear();
        std::cout << "Mainland/minor towns: 53 exits in 18 zones and 21 peripheral returns verified.\n";
        std::cout << "All audited city/nearby authored exits and reciprocal connections verified.\n";
    }
    std::cout << "All zone transition checks passed ("
              << (sizeof(lines) / sizeof(lines[0]))
              << " routes, both coordinate modes).\n";
}
