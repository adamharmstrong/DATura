#pragma once

#include "zone_dat_table.h"

#include <string>

namespace ZoneEntryMessage
{
inline std::string ForZone(const int zoneId)
{
    const FFXIZoneEntry* zone = FFXIZone::FindByID(zoneId);
    if (!zone || !zone->name || !zone->name[0])
        return {};
    return std::string("You have entered ") + zone->name + ".";
}
}
