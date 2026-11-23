#include "stdafx.h"
#include "zone_weather_audio.h"
#include "zone_weather_schedule.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace
{
int failures = 0;

void Check(const bool condition, const std::string &message)
{
    if (!condition)
    {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

uint16_t Pack(const int normal, const int common, const int rare)
{
    return static_cast<uint16_t>((normal << 10) | (common << 5) | rare);
}

constexpr long long kEpoch = 1009810800LL;

void TestUnpackAndDays()
{
    using namespace ZoneWeatherSchedule;
    const DayEntry entry = Unpack(1057);
    Check(entry.normal == Sunshine && entry.common == Sunshine && entry.rare == Sunshine,
          "LSB packed 1057 is sunshine/sunshine/sunshine");
    Check(VanadielDay(kEpoch) == 0, "epoch is day 0");
    Check(VanadielDay(kEpoch + 3455) == 0, "last second of day 0");
    Check(VanadielDay(kEpoch + 3456) == 1, "one Vana'diel day is 3456 real seconds");
    Check(VanadielDay(kEpoch - 1) == -1, "times before the epoch floor to day -1");
}

void TestEntryForDay()
{
    using namespace ZoneWeatherSchedule;
    Table table;
    std::vector<uint16_t> days(kCycleDays, 0);
    days[10] = Pack(Rain, Clouds, Squalls);
    days[500] = Pack(Sunshine, Fog, Thunder);
    table.SetZone(7, days);
    Check(table.EntryForDay(7, 3).normal == None, "days before the first entry are None (LSB)");
    Check(table.EntryForDay(7, 10).normal == Rain, "entry applies on its own day");
    Check(table.EntryForDay(7, 499).rare == Squalls, "entry carries forward until the next one");
    Check(table.EntryForDay(7, 2159).normal == Sunshine, "last entry carries to day 2159");
    Check(table.EntryForDay(7, 2160 + 10).normal == Rain, "cycle wraps every 2160 days");
    Check(table.EntryForDay(8, 0).normal == None, "unknown zone has no weather");
    Check(!table.HasZone(8) && table.HasZone(7), "HasZone reflects non-empty blobs");
    const std::vector<int> natural = table.NaturalWeather(7);
    Check(natural == std::vector<int>({ Sunshine, Clouds, Fog, Rain, Squalls, Thunder }),
          "natural weather lists every packed ID");
    Check(table.AllowsWeather(7, Squalls) && !table.AllowsWeather(7, Snow), "AllowsWeather");
}

void TestSelectionDeterminismAndOdds()
{
    using namespace ZoneWeatherSchedule;
    Table table;
    table.SetZone(3, std::vector<uint16_t>(kCycleDays, Pack(Sunshine, Clouds, Rain)));
    int counts[kWeatherTypeCount] = {};
    int segments = 0;
    long long time = kEpoch + 1000LL * 3456LL;
    const long long end = time + 3456LL * 600LL;
    long long previousEnd = 0;
    bool contiguous = true;
    bool lengthsValid = true;
    while (time < end)
    {
        const Selection a = table.Select(3, time);
        const Selection b = table.Select(3, time);
        if (a.weatherId != b.weatherId || a.segmentEndUnix != b.segmentEndUnix)
            Check(false, "selection is repeatable for a fixed time");
        if (a.segmentStartUnix > time || a.segmentEndUnix <= time)
            Check(false, "selection segment contains the queried time");
        if (previousEnd && a.segmentStartUnix != previousEnd)
            contiguous = false;
        const long long length = a.segmentEndUnix - a.segmentStartUnix;
        const bool atMidnight = (a.segmentEndUnix - kEpoch) % 3456LL == 0;
        if (length > 1801 || (length < 180 && !atMidnight))
            lengthsValid = false;
        const Selection mid = table.Select(3, (a.segmentStartUnix + a.segmentEndUnix) / 2);
        if (mid.weatherId != a.weatherId)
            Check(false, "weather is constant within a segment");
        ++counts[a.weatherId];
        ++segments;
        previousEnd = a.segmentEndUnix;
        time = a.segmentEndUnix;
    }
    Check(contiguous, "segments tile time without gaps");
    Check(lengthsValid, "segments last 3-30 real minutes unless cut at midnight");
    Check(counts[Sunshine] + counts[Clouds] + counts[Rain] == segments,
          "only the day's normal/common/rare weather is selected");
    const double rare = static_cast<double>(counts[Rain]) / segments;
    const double common = static_cast<double>(counts[Clouds]) / segments;
    Check(rare > 0.11 && rare < 0.19, "rare weather is selected about 15% of the time");
    Check(common > 0.30 && common < 0.40, "common weather is selected about 35% of the time");
    Check(segments > 1000, "sampled enough weather segments");

    Table other;
    other.SetZone(4, std::vector<uint16_t>(kCycleDays, Pack(Sunshine, Clouds, Rain)));
    int differences = 0;
    for (long long t = kEpoch; t < kEpoch + 3456LL * 30; t += 600)
        differences += table.Select(3, t).weatherId != other.Select(4, t).weatherId;
    Check(differences > 0, "different zones roll independently");
}

void TestInstalledSchedule(const std::filesystem::path &path)
{
    using namespace ZoneWeatherSchedule;
    Table table;
    if (!table.LoadFile(path.string().c_str()))
    {
        Check(false, "load " + path.string());
        return;
    }
    int zones = 0;
    for (int zone = 0; zone < 300; ++zone)
        zones += table.HasZone(zone);
    Check(zones == 300, "runtime schedule covers all 300 zone IDs");
    Check(table.IsStatic(0) && table.NaturalWeather(0) == std::vector<int>({ Sunshine }),
          "zone 0 is static sunshine");
    Check(table.AllowsWeather(102, Rain), "La Theine Plateau naturally rains");
    Check(!table.AllowsWeather(102, Snow), "La Theine Plateau never snows");
    bool impossible = false;
    for (int zone = 0; zone < 300; ++zone)
    {
        const std::vector<int> natural = table.NaturalWeather(zone);
        for (long long t = kEpoch + 7000LL * 3456LL; t < kEpoch + 7040LL * 3456LL; t += 997)
        {
            const int weather = table.Select(zone, t).weatherId;
            if (std::find(natural.begin(), natural.end(), weather) == natural.end())
                impossible = true;
        }
    }
    Check(!impossible, "offline selection never picks weather a zone cannot have");
    Check(!table.LoadBytes(reinterpret_cast<const unsigned char *>("DWSC"), 4) && !table.Loaded(),
          "truncated schedule is rejected");
}

void TestResolver()
{
    using namespace ZoneWeatherSchedule;
    bool exact = false;
    const std::vector<std::string> laTheine = { "f_la/weat/clod", "f_la/weat/fine",
        "f_la/weat/mist", "f_la/weat/rain", "f_la/weat/suny", "f_la/weat/wind" };
    Check(ResolveBranch(Rain, laTheine, &exact) == 3 && exact, "rain resolves to rain");
    Check(ResolveBranch(Wind, laTheine, &exact) == 5 && exact, "wind resolves to wind");
    Check(ResolveBranch(None, laTheine, &exact) == 1 && exact, "none uses the clear branch");
    Check(ResolveBranch(Sunshine, laTheine, &exact) == 4 && exact, "sunshine uses suny");
    // YmCombineWeather falls back to "suny" when the requested branch is missing.
    Check(ResolveBranch(Squalls, laTheine, &exact) == 4 && !exact, "missing squalls fall back to suny");
    Check(ResolveBranch(Gales, laTheine, &exact) == 4 && !exact, "missing gales fall back to suny");
    Check(ResolveBranch(Snow, laTheine, &exact) == 4 && !exact, "missing snow falls back to suny");
    Check(ResolveBranch(99, laTheine, &exact) == 4 && !exact, "unknown IDs use suny");

    // Phomiuna Aqueducts schedules rain but authors no rain branch.
    const std::vector<std::string> phomiuna = { "d_re/weat/clod", "d_re/weat/fogd",
        "d_re/weat/mist", "d_re/weat/suny", "d_re/weat/thdr", "d_re/weat/wind" };
    Check(ResolveBranch(Rain, phomiuna, &exact) == 3 && !exact, "Phomiuna rain shows suny");

    const std::vector<std::string> attohwa = { "d_at/weat/clo1", "d_at/weat/fog1",
        "d_at/weat/rai1", "d_at/weat/sun1", "d_at/weat/thd1", "d_at/weat/squ1" };
    Check(ResolveBranch(Rain, attohwa, &exact) == 2 && exact, "rai1 is a rain alias");
    Check(ResolveBranch(Squalls, attohwa, &exact) == 5 && exact, "squ1 is a squall alias");
    Check(ResolveBranch(Gloom, attohwa, &exact) == 1 && exact, "fog1 is a gloom alias");
    Check(ResolveBranch(Thunderstorms, attohwa, &exact) == 3 && !exact,
          "missing thunderstorms fall back to the sun1 alias");
    const std::vector<std::string> clearOnly = { "x/weat/clod", "x/weat/fine" };
    Check(ResolveBranch(Rain, clearOnly, &exact) == 1 && !exact, "without suny, fine is next");
    const std::vector<std::string> squalls = { "x/weat/rain", "x/weat/squl" };
    Check(ResolveBranch(Squalls, squalls, &exact) == 1 && exact, "squl is the squall branch");
    Check(ResolveBranch(Rain, squalls, &exact) == 0 && exact, "rain keeps the rain branch");
    Check(ResolveBranch(Rain, {}, &exact) == -1, "no branches resolve to -1");
    Check(ResolveBranch(Rain, { "t_ta/weat/pl" }, &exact) == 0 && !exact,
          "unrecognized-only branches use the first branch");
    Check(ResolveBranch(Rain, { "F_LA/WEAT/RAIN/indo" }) == 0, "matching ignores case and subdirectories");
}

std::vector<ZoneWeatherAudio::SoundRecord> LaTheineSounds()
{
    // Order and IDs match ROM/0/115.DAT's f_la/weat branches.
    return {
        { "f_la/weat/suny", 18 * 60, 1003, false },
        { "f_la/weat/suny", 6 * 60, 1001, false },
        { "f_la/weat/suny/indo", 0, 1055, true },
        { "f_la/weat/rain", 0, 1042, false },
        { "f_la/weat/rain/indo", 0, 1045, true },
        { "f_la/weat/wind", 18 * 60, 1004, false },
        { "f_la/weat/wind", 6 * 60, 1002, false },
    };
}

void TestSoundSelection()
{
    using ZoneWeatherAudio::SelectSound;
    const auto records = LaTheineSounds();
    Check(SelectSound(records, "f_la/weat/rain", 12 * 60, false) == 1042, "outdoor rain loop");
    Check(SelectSound(records, "f_la/weat/rain", 12 * 60, true) == 1045, "indoor rain loop");
    Check(SelectSound(records, "f_la/weat/suny", 7 * 60, false) == 1001, "morning sunshine loop");
    Check(SelectSound(records, "f_la/weat/suny", 19 * 60, false) == 1003, "evening sunshine loop");
    Check(SelectSound(records, "f_la/weat/suny", 18 * 60, false) == 1003, "record activates at its minute");
    Check(SelectSound(records, "f_la/weat/suny", 3 * 60, false) == 1001,
          "before the first record, FindPrevSound keeps the last record in file order");
    Check(SelectSound(records, "f_la/weat/wind", 3 * 60, false) == 1002, "wind wraps the same way");
    Check(SelectSound(records, "f_la/weat/mist", 12 * 60, false) == 0, "branch without sounds is silent");
    Check(SelectSound(records, "", 12 * 60, false) == 0, "no weather is silent");
    const std::vector<ZoneWeatherAudio::SoundRecord> outdoorOnly = { { "z/weat/rain", 0, 77, false } };
    Check(SelectSound(outdoorOnly, "z/weat/rain", 600, true) == 77,
          "without an indo set every record is eligible indoors");
}

void TestController(const std::filesystem::path &ffxiRoot)
{
    ZoneWeatherAudio::Controller audio;
    audio.SetVolumeScale(0.0f);
    audio.SetRootPath(ffxiRoot.string().c_str());
    audio.Update(1042, true, 0.016f, 3.0f);
    if (audio.ActiveVoiceCount() == 0)
    {
        std::cout << "SKIP: weather audio controller (no audio device or SeWave 1042)\n";
        return;
    }
    Check(audio.CurrentSoundId() == 1042, "rain loop starts");
    for (int frame = 0; frame < 10; ++frame)
        audio.Update(1042, true, 0.016f, 3.0f);
    Check(audio.ActiveVoiceCount() == 1, "repeating the same loop never stacks voices");
    audio.Update(1001, true, 0.016f, 3.0f);
    Check(audio.ActiveVoiceCount() == 2, "transition cross-fades old and new loops");
    for (int frame = 0; frame < 30; ++frame)
        audio.Update(1001, true, 0.016f, 3.0f);
    audio.Update(1042, true, 0.016f, 3.0f);
    Check(audio.ActiveVoiceCount() == 2 && audio.CurrentSoundId() == 1042,
          "returning to a fading loop resumes it instead of adding a voice");
    for (int frame = 0; frame < 300; ++frame)
        audio.Update(1042, true, 0.016f, 3.0f);
    Check(audio.ActiveVoiceCount() == 1, "the faded loop is released");
    audio.Update(1042, false, 0.016f, 3.0f);
    Check(audio.ActiveVoiceCount() == 0, "disabling sound silences weather immediately");
    audio.Update(1042, true, 0.016f, 3.0f);
    Check(audio.ActiveVoiceCount() == 1 && audio.CurrentSoundId() == 1042,
          "re-enabling restores only the current loop");
    audio.Update(0, true, 0.016f, 3.0f);
    for (int frame = 0; frame < 300; ++frame)
        audio.Update(0, true, 0.016f, 3.0f);
    Check(audio.ActiveVoiceCount() == 0, "clear weather fades the loop out");
    audio.Update(1042, true, 0.016f, 3.0f);
    audio.Reset();
    Check(audio.ActiveVoiceCount() == 0, "zone unload releases weather voices");
    audio.Update(999999, true, 0.016f, 3.0f);
    Check(audio.ActiveVoiceCount() == 0, "a missing SeWave keeps weather silent without failing");
    audio.Shutdown();
}
}

int main(int argc, char **argv)
{
    const std::filesystem::path source = std::filesystem::path(__FILE__).parent_path();
    const std::filesystem::path schedule = argc > 2 ? std::filesystem::path(argv[2]) :
        source / ".." / "DATura" / "zone_weather_schedule.bin";
    TestUnpackAndDays();
    TestEntryForDay();
    TestSelectionDeterminismAndOdds();
    TestInstalledSchedule(schedule);
    TestResolver();
    TestSoundSelection();
    if (argc > 1)
        TestController(argv[1]);
    else
        std::cout << "SKIP: weather audio controller (pass the FFXI root)\n";
    std::cout << "Weather schedule failures: " << failures << '\n';
    return failures ? 1 : 0;
}
