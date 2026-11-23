#include "stdafx.h"
#include "npc_trigger_dialogue.h"

#include <cstdlib>
#include <fstream>
#include <map>
#include <mutex>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace NpcTriggerDialogue
{
namespace
{
std::mutex g_mutex;
std::string g_loadedPath;
bool g_loaded = false;
std::map<std::pair<int, std::string>, Entry> g_entries;

std::string DefaultPath()
{
#ifdef _WIN32
    char exePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH))
    {
        std::string path(exePath);
        const std::size_t slash = path.find_last_of("\\/");
        return (slash == std::string::npos ? std::string() : path.substr(0, slash + 1)) +
            "npc_trigger_dialogue.csv";
    }
#endif
    return "npc_trigger_dialogue.csv";
}

std::vector<std::string> SplitCsv(const std::string& line)
{
    std::vector<std::string> fields;
    std::string field;
    bool quoted = false;
    for (std::size_t i = 0; i < line.size(); ++i)
    {
        const char c = line[i];
        if (quoted)
        {
            if (c == '"' && i + 1 < line.size() && line[i + 1] == '"') { field.push_back('"'); ++i; }
            else if (c == '"') quoted = false;
            else field.push_back(c);
        }
        else if (c == '"') quoted = true;
        else if (c == ',') { fields.push_back(field); field.clear(); }
        else field.push_back(c);
    }
    fields.push_back(field);
    return fields;
}

void LoadLocked(const std::string& path)
{
    g_entries.clear();
    g_loadedPath = path;
    g_loaded = true;
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#' || line.compare(0, 7, "zone_id") == 0) continue;
        // zone_id,name,kind,id,source
        const std::vector<std::string> fields = SplitCsv(line);
        if (fields.size() < 4 || fields[1].empty()) continue;
        char* end = nullptr;
        const long zone = std::strtol(fields[0].c_str(), &end, 10);
        if (fields[0].empty() || !end || *end) continue;
        const unsigned long id = std::strtoul(fields[3].c_str(), &end, 10);
        if (fields[3].empty() || !end || *end) continue;
        Entry entry;
        entry.id = static_cast<std::uint32_t>(id);
        if (fields[2] == "event") entry.kind = Kind::Event;
        else if (fields[2] == "text") entry.kind = Kind::Text;
        else continue;
        g_entries[{ static_cast<int>(zone), fields[1] }] = entry;
    }
}
}

bool Find(const int zoneId, const std::string& npcName, Entry& out, const char* catalogPath)
{
    const std::string path = catalogPath && catalogPath[0] ? catalogPath : DefaultPath();
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_loaded || g_loadedPath != path)
        LoadLocked(path);
    const auto found = g_entries.find({ zoneId, npcName });
    if (found == g_entries.end())
        return false;
    out = found->second;
    return true;
}
}
