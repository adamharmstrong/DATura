#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace FFXIEventTable
{
    struct Event
    {
        std::uint16_t id = 0xffff;
        std::vector<std::uint8_t> code;
        std::vector<std::uint32_t> references;
    };

    struct EntityScripts
    {
        std::uint32_t entityId = 0;
        std::vector<Event> events;
    };

    // Parses the resolved payload of a zone event DAT. The returned table is
    // indexed by the entity IDs stored at the start of each event block.
    bool Parse(const std::vector<std::uint8_t>& bytes, std::vector<EntityScripts>& out);
    bool Load(const char* path, std::vector<EntityScripts>& out);

    // Extracts message-table indices from the common NPC speech opcodes.
    std::vector<std::uint16_t> MessageIds(const Event& event);
}
