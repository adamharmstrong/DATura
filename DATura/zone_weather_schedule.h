#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Natural zone weather. The 2,160-day cycle is LandSandBoat's zone_weather
// table (exported by tools/generate_zone_weather_database.py). The retail
// client receives the selected weather from the server; this module stands in
// for that server so offline play follows the zone's natural weather.
namespace ZoneWeatherSchedule
{
constexpr int kCycleDays = 2160;
constexpr int kWeatherTypeCount = 20;
constexpr int kNoWeather = -1;

// LSB weather IDs (data/enums/weather.yaml ordering).
enum WeatherId : int
{
    None = 0, Sunshine, Clouds, Fog, HotSpell, HeatWave, Rain, Squalls,
    DustStorm, Sandstorm, Wind, Gales, Snow, Blizzard, Thunder, Thunderstorms,
    Aurora, StellarGlare, Gloom, Darkness,
};

struct DayEntry
{
    int normal = None;
    int common = None;
    int rare = None;
};

// Offline selection for one moment.
struct Selection
{
    int weatherId = None;
    int cycleDay = 0;
    long long segmentStartUnix = 0;
    long long segmentEndUnix = 0;
};

const char *WeatherName(int weatherId);

// Packed LSB blob value: bits 10-14 normal, 5-9 common, 0-4 rare.
DayEntry Unpack(uint16_t packed);

class Table
{
public:
    // Reads zone_weather_schedule.bin. Returns false and leaves the table empty
    // when the file is missing or malformed.
    bool LoadFile(const char *path);
    bool LoadBytes(const unsigned char *data, size_t size);
    // Tests and tools may inject a zone's raw 2,160-entry blob directly.
    void SetZone(int zoneId, const std::vector<uint16_t> &packedDays);
    void Clear();

    bool Loaded() const { return !zones_.empty(); }
    bool HasZone(int zoneId) const;
    // LSB WeatherContainer::entryForDay: the most recent non-zero day at or
    // before `cycleDay`; all-None when the zone has no entry yet.
    DayEntry EntryForDay(int zoneId, int cycleDay) const;
    // True when every day resolves to one single weather (LSB "static").
    bool IsStatic(int zoneId) const;
    // Every weather ID that can occur naturally in the zone.
    std::vector<int> NaturalWeather(int zoneId) const;
    bool AllowsWeather(int zoneId, int weatherId) const;

    // Repeatable offline weather for a Unix time. Like LSB's UpdateWeather,
    // the zone re-rolls every 3-30 real minutes and picks rare/common/normal
    // with 15/35/50 percent odds; here both the interval and the roll come
    // from a hash of (zone, Vana'diel day, segment) so a fixed time always
    // yields the same weather. Segments restart at each Vana'diel midnight.
    Selection Select(int zoneId, long long unixSeconds) const;

private:
    std::vector<std::vector<uint16_t>> zones_;
};

// Vana'diel day count since the LSB/retail epoch for a Unix time.
long long VanadielDay(long long unixSeconds);

// /weat raw directory names (lower-case, four characters) that represent a
// weather ID, primary name first.
const std::vector<std::string> &BranchCandidates(int weatherId);

// Index into `weatherDirectories` (full DAT directory paths such as
// "f_la/weat/rain") for `weatherId`, or -1 when the list is empty. Like the
// retail client, a missing branch falls back to "suny" (then "fine", then the
// first branch). `exact` reports whether the requested family was found.
int ResolveBranch(int weatherId, const std::vector<std::string> &weatherDirectories,
                  bool *exact = nullptr);
}
