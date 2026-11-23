#include "stdafx.h"
#include "zone_weather_schedule.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>

namespace ZoneWeatherSchedule
{
namespace
{
constexpr char kMagic[4] = { 'D', 'W', 'S', 'C' };
constexpr uint32_t kVersion = 1;
constexpr long long kVanadielEpochUnix = 1009810800LL;
// One Vana'diel day is 24 * 60 * 60 / 25 real seconds.
constexpr long long kRealSecondsPerVanadielDay = 3456LL;
// LSB: xirand::GetRandomNumber(180, 1801) seconds between weather rolls.
constexpr long long kMinimumSegmentSeconds = 180LL;
constexpr long long kSegmentSecondsRange = 1621LL;

uint64_t Mix(uint64_t value)
{
    // splitmix64 finalizer: stable across compilers and platforms.
    value += 0x9E3779B97F4A7C15ULL;
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31);
}

uint64_t Hash(const int zoneId, const long long day, const int segment, const int salt)
{
    uint64_t value = Mix(static_cast<uint64_t>(zoneId) * 0x100000001B3ULL);
    value = Mix(value ^ static_cast<uint64_t>(day));
    value = Mix(value ^ (static_cast<uint64_t>(segment) << 8) ^ static_cast<uint64_t>(salt));
    return value;
}

uint32_t ReadU32(const unsigned char *data)
{
    return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
}

const std::vector<std::vector<std::string>> &CandidateTable()
{
    // The client maps the server's weather number to a /weat resource ID
    // through WeatherResId (RecWeatherTask), whose data the PS2 decompilation
    // does not recover. The primary names follow the DAT directory families
    // in tools/zone_weather_database.sqlite; "1"-suffixed names are DATura
    // aliases for zones that author only those variants.
    static const std::vector<std::vector<std::string>> table =
    {
        { "fine" },
        { "suny", "sun1" },
        { "clod", "clo1" },
        { "mist", "mis1" },
        { "dryw", "dry1" },
        { "heat" },
        { "rain", "rai1" },
        { "squl", "squ1", "squa" },
        { "dust", "dus1" },
        { "sand" },
        { "wind", "win1" },
        { "stom" },
        { "snow" },
        { "bliz" },
        { "thdr", "thd1" },
        { "bolt" },
        { "aura", "aur1" },
        { "ligt", "ligh" },
        { "fogd", "fog1" },
        { "dark" },
    };
    return table;
}

const std::vector<std::string> &ClearTail()
{
    // YmCombineWeather's constructor falls back to "suny" when the requested
    // branch is missing (and to the bare weat directory after that).
    static const std::vector<std::string> tail = { "suny", "sun1", "fine" };
    return tail;
}

std::string LeafName(const std::string &directory)
{
    std::string lower = directory;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return static_cast<char>(tolower(c)); });
    const size_t weat = lower.find("/weat/");
    if (weat == std::string::npos)
        return std::string();
    std::string leaf = lower.substr(weat + 6);
    const size_t slash = leaf.find('/');
    if (slash != std::string::npos)
        leaf.resize(slash);
    return leaf;
}
}

const char *WeatherName(const int weatherId)
{
    static const char *const names[kWeatherTypeCount] =
    {
        "none", "sunshine", "clouds", "fog", "hot spell", "heat wave", "rain",
        "squalls", "dust storm", "sandstorm", "wind", "gales", "snow", "blizzard",
        "thunder", "thunderstorms", "aurora", "stellar glare", "gloom", "darkness",
    };
    return weatherId >= 0 && weatherId < kWeatherTypeCount ? names[weatherId] : "unknown";
}

DayEntry Unpack(const uint16_t packed)
{
    DayEntry entry;
    entry.normal = (packed >> 10) & 0x1F;
    entry.common = (packed >> 5) & 0x1F;
    entry.rare = packed & 0x1F;
    return entry;
}

long long VanadielDay(const long long unixSeconds)
{
    const long long vanadielSeconds = (unixSeconds - kVanadielEpochUnix) * 25LL;
    long long day = vanadielSeconds / 86400LL;
    if (vanadielSeconds < 0 && vanadielSeconds % 86400LL)
        --day;
    return day;
}

bool Table::LoadFile(const char *path)
{
    Clear();
    if (!path)
        return false;
    FILE *file = nullptr;
    if (fopen_s(&file, path, "rb") != 0 || !file)
        return false;
    std::vector<unsigned char> bytes;
    unsigned char buffer[65536];
    size_t count = 0;
    while ((count = fread(buffer, 1, sizeof(buffer), file)) > 0)
        bytes.insert(bytes.end(), buffer, buffer + count);
    fclose(file);
    return LoadBytes(bytes.data(), bytes.size());
}

bool Table::LoadBytes(const unsigned char *data, const size_t size)
{
    Clear();
    if (!data || size < 16 || memcmp(data, kMagic, 4) != 0 || ReadU32(data + 4) != kVersion)
        return false;
    const uint32_t zoneCount = ReadU32(data + 8);
    const uint32_t days = ReadU32(data + 12);
    const size_t record = 4 + static_cast<size_t>(days) * 2;
    if (days != kCycleDays || zoneCount > 4096 || size != 16 + record * zoneCount)
        return false;
    for (uint32_t zone = 0; zone < zoneCount; ++zone)
    {
        const unsigned char *entry = data + 16 + record * zone;
        const uint32_t zoneId = ReadU32(entry);
        if (zoneId >= 4096)
        {
            Clear();
            return false;
        }
        std::vector<uint16_t> packed(days);
        for (uint32_t day = 0; day < days; ++day)
            packed[day] = static_cast<uint16_t>(entry[4 + day * 2] | (entry[5 + day * 2] << 8));
        SetZone(static_cast<int>(zoneId), packed);
    }
    return true;
}

void Table::SetZone(const int zoneId, const std::vector<uint16_t> &packedDays)
{
    if (zoneId < 0)
        return;
    if (zones_.size() <= static_cast<size_t>(zoneId))
        zones_.resize(static_cast<size_t>(zoneId) + 1);
    zones_[static_cast<size_t>(zoneId)] = packedDays;
    zones_[static_cast<size_t>(zoneId)].resize(kCycleDays, 0);
}

void Table::Clear()
{
    zones_.clear();
}

bool Table::HasZone(const int zoneId) const
{
    if (zoneId < 0 || static_cast<size_t>(zoneId) >= zones_.size())
        return false;
    const auto &days = zones_[static_cast<size_t>(zoneId)];
    return std::any_of(days.begin(), days.end(), [](uint16_t value) { return value != 0; });
}

DayEntry Table::EntryForDay(const int zoneId, int cycleDay) const
{
    if (zoneId < 0 || static_cast<size_t>(zoneId) >= zones_.size())
        return {};
    const auto &days = zones_[static_cast<size_t>(zoneId)];
    if (days.empty())
        return {};
    cycleDay = ((cycleDay % kCycleDays) + kCycleDays) % kCycleDays;
    for (int day = cycleDay; day >= 0; --day)
        if (days[static_cast<size_t>(day)])
            return Unpack(days[static_cast<size_t>(day)]);
    return {};
}

bool Table::IsStatic(const int zoneId) const
{
    const std::vector<int> natural = NaturalWeather(zoneId);
    return natural.size() <= 1;
}

std::vector<int> Table::NaturalWeather(const int zoneId) const
{
    std::vector<int> result;
    if (zoneId < 0 || static_cast<size_t>(zoneId) >= zones_.size())
        return result;
    bool seen[32] = {};
    for (const uint16_t packed : zones_[static_cast<size_t>(zoneId)])
    {
        if (!packed)
            continue;
        const DayEntry entry = Unpack(packed);
        seen[entry.normal] = seen[entry.common] = seen[entry.rare] = true;
    }
    for (int id = 0; id < 32; ++id)
        if (seen[id])
            result.push_back(id);
    return result;
}

bool Table::AllowsWeather(const int zoneId, const int weatherId) const
{
    const std::vector<int> natural = NaturalWeather(zoneId);
    return std::find(natural.begin(), natural.end(), weatherId) != natural.end();
}

Selection Table::Select(const int zoneId, const long long unixSeconds) const
{
    Selection selection;
    const long long day = VanadielDay(unixSeconds);
    selection.cycleDay = static_cast<int>(((day % kCycleDays) + kCycleDays) % kCycleDays);
    // Real-time start of this Vana'diel day; day * 3456 is exact because the
    // Vana'diel day is 86400 / 25 real seconds.
    const long long dayStart = kVanadielEpochUnix + day * kRealSecondsPerVanadielDay;
    const long long dayEnd = dayStart + kRealSecondsPerVanadielDay;
    long long segmentStart = dayStart;
    int segment = 0;
    long long segmentEnd = dayEnd;
    for (;; ++segment)
    {
        const long long length = kMinimumSegmentSeconds +
            static_cast<long long>(Hash(zoneId, day, segment, 0) % kSegmentSecondsRange);
        segmentEnd = std::min(segmentStart + length, dayEnd);
        if (unixSeconds < segmentEnd || segmentEnd >= dayEnd)
            break;
        segmentStart = segmentEnd;
    }
    selection.segmentStartUnix = segmentStart;
    selection.segmentEndUnix = segmentEnd;

    const DayEntry entry = EntryForDay(zoneId, selection.cycleDay);
    const int chance = static_cast<int>(Hash(zoneId, day, segment, 1) % 100ULL);
    selection.weatherId = chance < 15 ? entry.rare : (chance < 50 ? entry.common : entry.normal);
    if (selection.weatherId < 0 || selection.weatherId >= kWeatherTypeCount)
        selection.weatherId = None;
    return selection;
}

const std::vector<std::string> &BranchCandidates(const int weatherId)
{
    static const std::vector<std::string> empty;
    const auto &table = CandidateTable();
    if (weatherId < 0 || static_cast<size_t>(weatherId) >= table.size())
        return empty;
    return table[static_cast<size_t>(weatherId)];
}

int ResolveBranch(const int weatherId, const std::vector<std::string> &weatherDirectories,
                  bool *exact)
{
    if (exact)
        *exact = false;
    if (weatherDirectories.empty())
        return -1;
    std::vector<std::string> leaves;
    leaves.reserve(weatherDirectories.size());
    for (const std::string &directory : weatherDirectories)
        leaves.push_back(LeafName(directory));
    auto find = [&](const std::string &name) -> int
    {
        for (size_t index = 0; index < leaves.size(); ++index)
            if (leaves[index] == name)
                return static_cast<int>(index);
        return -1;
    };

    const auto &table = CandidateTable();
    if (weatherId >= 0 && static_cast<size_t>(weatherId) < table.size())
    {
        for (const std::string &name : table[static_cast<size_t>(weatherId)])
        {
            const int index = find(name);
            if (index >= 0)
            {
                if (exact)
                    *exact = true;
                return index;
            }
        }
    }
    for (const std::string &name : ClearTail())
    {
        const int index = find(name);
        if (index >= 0)
            return index;
    }
    return 0;
}
}
