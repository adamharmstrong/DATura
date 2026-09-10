#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace FFXIEventMessages
{
    struct Entry
    {
        std::string text;
    };

    // Parses the resolved EventMessage payload used by zone event scripts.
    bool Parse(const std::vector<std::uint8_t>& bytes, std::vector<Entry>& out);
    bool Load(const char* path, std::vector<Entry>& out);
}
