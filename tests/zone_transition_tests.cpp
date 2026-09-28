#include "../DATura/zone_transition.h"
#include "../DATura/zone_entry_message.h"
#include <cstdlib>
#include <iostream>

static void Check(bool value, const char* message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

int main()
{
    using namespace ZoneTransition;

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
            const float heading = DecodeHeading(line.heading);
            const auto forward = scene({ std::cos(heading), 0, -std::sin(heading) });
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
    std::cout << "All zone transition checks passed ("
              << (sizeof(lines) / sizeof(lines[0]))
              << " routes, both coordinate modes).\n";
}
