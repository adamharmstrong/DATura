#include "../DATura/zone_transition.h"
#include <cstdlib>
#include <iostream>

static void Check(bool value, const char* message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

int main()
{
    using namespace ZoneTransition;
    for (const auto& line : lines)
    {
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
            const auto forward = scene({ std::cos(line.heading), 0, -std::sin(line.heading) });
            Check(std::fabs(-std::sin(yaw)-forward[0]) < 0.00001f &&
                std::fabs(-std::cos(yaw)-forward[2]) < 0.00001f, "Arrival heading incorrect");
            for (const auto& reverse : lines)
            {
                if (reverse.fromZone != line.toZone || reverse.toZone != line.fromZone) continue;
                const float side = (line.arrival[0]-reverse.threshold[0])*reverse.outward[0] +
                    (line.arrival[2]-reverse.threshold[2])*reverse.outward[2];
                Check(side < -1, "Arrival is not safely inside destination");
                Check(!Crossed(line.toZone, scene(line.arrival), scene(line.arrival), mirror), "Arrival bounced back");
            }
        }
    }
    std::cout << "All zone transition checks passed (8 routes, both coordinate modes).\n";
}
