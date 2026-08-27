#pragma once

#include "ffxi_resource.h"

#include <string>
#include <vector>

namespace FFXIZoneResources
{
    struct BrowseData
    {
        int zoneId = -1;
        std::string zoneName;
        std::vector<FFXIResource::Row> rows;
        std::wstring formats;
        std::wstring warnings;
        int parsedFileCount = 0;
    };

    // Builds the data presented by the current-zone resource browser. Parsing
    // is read-only; the caller owns all windowing and presentation decisions.
    bool BuildBrowseData(const char* ffxiRoot, const char* zoneModelPath, BrowseData& outData);
}
