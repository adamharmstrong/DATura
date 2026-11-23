#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

struct ff11DatChunkDebug_t;
struct IXAudio2;
struct IXAudio2MasteringVoice;
struct IXAudio2SourceVoice;

// Ambient weather loops authored as 0x3D sound records inside each /weat
// branch. Records are named by HHMM; the PS2 client's
// YmCombineWeather::FindPrevSound plays the latest record at or before the
// current Vana'diel time and uses the branch's "indo" subdirectory only for
// indoor areas.
namespace ZoneWeatherAudio
{
struct SoundRecord
{
    std::string directoryPath;
    int minuteOfDay = 0;
    unsigned int soundId = 0;
    bool indoor = false;
};

// Extracts weather sound records from a zone's parsed chunk list.
std::vector<SoundRecord> CollectSounds(const std::vector<ff11DatChunkDebug_t> &chunks);

// SeWave ID for the weather branch at `minuteOfDay`, or 0 when none applies.
unsigned int SelectSound(const std::vector<SoundRecord> &records, const char *weatherPath,
                         int minuteOfDay, bool indoor);

// Owns the XAudio2 voices for the current and fading weather loops. The
// manual audio browser and zone music use separate players.
class Controller
{
public:
    Controller();
    Controller(const Controller &) = delete;
    Controller &operator=(const Controller &) = delete;
    ~Controller();

    void SetRootPath(const char *ffxiRoot);
    // Scales every weather voice (tests use 0 to run silently).
    void SetVolumeScale(float scale) { volumeScale_ = scale; }
    // Cross-fades to `soundId` (0 = silence) over `fadeSeconds`. When playback
    // is not allowed every voice stops immediately; a later allowed update
    // restarts only the requested loop.
    void Update(unsigned int soundId, bool allowed, float deltaSeconds, float fadeSeconds);
    // Stops all voices (zone unload). Decoded clips stay cached.
    void Reset();
    // Releases voices, clips, and the audio engine.
    void Shutdown();

    unsigned int CurrentSoundId() const { return current_.soundId; }
    size_t ActiveVoiceCount() const;

private:
    struct Clip;
    struct Voice
    {
        unsigned int soundId = 0;
        IXAudio2SourceVoice *source = nullptr;
        float gain = 0.0f;
    };

    bool EnsureEngine();
    const Clip *GetClip(unsigned int soundId);
    IXAudio2SourceVoice *StartLoop(const Clip &clip);
    static void Destroy(Voice &voice);

    std::string root_;
    float volumeScale_ = 1.0f;
    IXAudio2 *engine_ = nullptr;
    IXAudio2MasteringVoice *master_ = nullptr;
    bool engineFailed_ = false;
    bool comInitialized_ = false;
    std::map<unsigned int, std::unique_ptr<Clip>> clips_;
    std::map<unsigned int, bool> failedClips_;
    Voice current_;
    std::vector<Voice> fading_;
};
}
