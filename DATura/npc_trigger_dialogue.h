#pragma once

#include <cstdint>
#include <string>

// What the server does when a retail NPC is clicked, generated from
// LandSandBoat by tools/generate_npc_trigger_dialogue.py into
// npc_trigger_dialogue.csv (copied beside DATura.exe).
namespace NpcTriggerDialogue
{
enum class Kind
{
    None,
    Event, // Run this event id from the NPC's client event script.
    Text,  // Show this message id from the zone's dialog DAT.
};

struct Entry
{
    Kind kind = Kind::None;
    std::uint32_t id = 0;
};

// Looks up zone + NPC name. catalogPath defaults to the file beside the exe.
bool Find(int zoneId, const std::string& npcName, Entry& out, const char* catalogPath = nullptr);
}
