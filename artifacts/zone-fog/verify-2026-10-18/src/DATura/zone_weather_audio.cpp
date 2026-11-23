#include "stdafx.h"
#include "zone_weather_audio.h"

#include "bgw_player.h"
#include "ffxi_file_io.h"
#include "noesis_rapi.h"
#include "model_ff11.h"
#include "zone_environment_identity.h"

#include <algorithm>
#include <cstring>
#include <xaudio2.h>

#pragma comment(lib, "xaudio2.lib")

namespace ZoneWeatherAudio
{
namespace
{
// The SeSep wrapper's volume fields are not decoded yet; this keeps weather
// beneath zone music, similar to the other ambient loops.
constexpr float kWeatherLoopGain = 0.55f;

bool IsTimeName(const char *name)
{
    for (int index = 0; index < 4; ++index)
        if (name[index] < '0' || name[index] > '9')
            return false;
    return true;
}

bool EndsWithIndoor(const char *path)
{
    const size_t length = strlen(path);
    return length >= 5 && _stricmp(path + length - 5, "/indo") == 0;
}
}

struct Controller::Clip
{
    std::vector<BYTE> pcm;
    WAVEFORMATEX format = {};
    UINT32 loopBegin = 0;
};

std::vector<SoundRecord> CollectSounds(const std::vector<ff11DatChunkDebug_t> &chunks)
{
    std::vector<SoundRecord> records;
    for (const ff11DatChunkDebug_t &chunk : chunks)
    {
        if (chunk.type != CFFXIDat::skChunkType_SoundPointer || !chunk.hasSoundPointer ||
            !Model_FF11_IsWeatherDirectory(chunk.directoryPath) || !IsTimeName(chunk.name))
            continue;
        const int hour = (chunk.name[0] - '0') * 10 + (chunk.name[1] - '0');
        const int minute = (chunk.name[2] - '0') * 10 + (chunk.name[3] - '0');
        SoundRecord record;
        record.directoryPath = chunk.directoryPath;
        record.minuteOfDay = (hour % 24) * 60 + minute % 60;
        record.soundId = chunk.soundId;
        record.indoor = EndsWithIndoor(chunk.directoryPath);
        records.push_back(record);
    }
    return records;
}

unsigned int SelectSound(const std::vector<SoundRecord> &records, const char *weatherPath,
                         const int minuteOfDay, const bool indoor)
{
    if (!weatherPath || !weatherPath[0])
        return 0;
    bool hasIndoorSet = false;
    for (const SoundRecord &record : records)
        if (record.indoor && ZoneEnvironmentIdentity::EnvironmentWeatherRootsMatch(
                record.directoryPath.c_str(), weatherPath))
            hasIndoorSet = true;

    // FindPrevSound: the latest record activated at or before now; if none
    // has activated yet today, the last eligible record in file order.
    const SoundRecord *latest = nullptr;
    const SoundRecord *lastEligible = nullptr;
    for (const SoundRecord &record : records)
    {
        if (!ZoneEnvironmentIdentity::EnvironmentWeatherRootsMatch(
                record.directoryPath.c_str(), weatherPath))
            continue;
        if (hasIndoorSet && record.indoor != indoor)
            continue;
        lastEligible = &record;
        if (record.minuteOfDay <= minuteOfDay &&
            (!latest || record.minuteOfDay >= latest->minuteOfDay))
            latest = &record;
    }
    const SoundRecord *selected = latest ? latest : lastEligible;
    return selected ? selected->soundId : 0;
}

Controller::Controller() = default;

Controller::~Controller()
{
    Shutdown();
}

void Controller::SetRootPath(const char *ffxiRoot)
{
    const std::string root = ffxiRoot ? ffxiRoot : "";
    if (root == root_)
        return;
    Reset();
    clips_.clear();
    failedClips_.clear();
    root_ = root;
}

bool Controller::EnsureEngine()
{
    if (master_)
        return true;
    if (engineFailed_)
        return false;
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    comInitialized_ = SUCCEEDED(com);
    if ((FAILED(com) && com != RPC_E_CHANGED_MODE) ||
        FAILED(XAudio2Create(&engine_, 0, XAUDIO2_DEFAULT_PROCESSOR)) ||
        FAILED(engine_->CreateMasteringVoice(&master_)))
    {
        Shutdown();
        engineFailed_ = true;
        return false;
    }
    return true;
}

const Controller::Clip *Controller::GetClip(const unsigned int soundId)
{
    const auto cached = clips_.find(soundId);
    if (cached != clips_.end())
        return cached->second.get();
    if (failedClips_.count(soundId) || root_.empty())
        return nullptr;
    failedClips_[soundId] = true;

    char relative[80] = {};
    sprintf_s(relative, "\\win\\se\\se%03u\\se%06u.spw", soundId / 1000, soundId);
    std::string path;
    for (const char *folder : { "sound", "sound2", "sound3", "sound4", "sound5", "sound6", "sound9" })
    {
        const std::string candidate = root_ + "\\" + folder + relative;
        if (GetFileAttributesA(candidate.c_str()) != INVALID_FILE_ATTRIBUTES)
        {
            path = candidate;
            break;
        }
    }
    FFXIAudioInfo info;
    char wav[MAX_PATH] = {};
    if (path.empty() || !FFXIAudio_ReadInfo(path.c_str(), &info) ||
        !FFXIAudio_PrepareFile(path.c_str(), wav, sizeof(wav)))
        return nullptr;
    BYTE *raw = nullptr;
    DWORD count = 0;
    if (!FFXIFileIO::ReadWholeFile(wav, &raw, &count))
        return nullptr;
    std::unique_ptr<BYTE[]> bytes(raw);
    if (count < 44 || memcmp(raw, "RIFF", 4) || memcmp(raw + 8, "WAVE", 4))
        return nullptr;

    auto clip = std::make_unique<Clip>();
    for (size_t offset = 12; offset + 8 <= count;)
    {
        DWORD size = 0;
        memcpy(&size, raw + offset + 4, sizeof(size));
        if (size > count - offset - 8)
            return nullptr;
        if (!memcmp(raw + offset, "fmt ", 4) && size >= 16)
            memcpy(&clip->format, raw + offset + 8, 16);
        else if (!memcmp(raw + offset, "data", 4))
            clip->pcm.assign(raw + offset + 8, raw + offset + 8 + size);
        offset += 8 + size + (size & 1);
    }
    if (clip->format.wFormatTag != WAVE_FORMAT_PCM || clip->format.wBitsPerSample != 16 ||
        clip->pcm.empty() || !clip->format.nBlockAlign)
        return nullptr;
    const unsigned long long loopFrame = info.loopStart < 0 ? 0ULL :
        static_cast<unsigned long long>(info.loopStart) *
            (info.codec == FFXIAudioCodec::ADPCM ? info.blockSize : 1);
    const size_t frames = clip->pcm.size() / clip->format.nBlockAlign;
    clip->loopBegin = loopFrame < frames ? static_cast<UINT32>(loopFrame) : 0;

    failedClips_.erase(soundId);
    const Clip *result = clip.get();
    clips_[soundId] = std::move(clip);
    return result;
}

IXAudio2SourceVoice *Controller::StartLoop(const Clip &clip)
{
    IXAudio2SourceVoice *voice = nullptr;
    if (!engine_ || FAILED(engine_->CreateSourceVoice(&voice, &clip.format)))
        return nullptr;
    XAUDIO2_BUFFER buffer = {};
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    buffer.AudioBytes = static_cast<UINT32>(clip.pcm.size());
    buffer.pAudioData = clip.pcm.data();
    buffer.LoopBegin = clip.loopBegin;
    buffer.LoopLength = static_cast<UINT32>(clip.pcm.size() / clip.format.nBlockAlign) - clip.loopBegin;
    buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
    voice->SetVolume(0.0f);
    if (FAILED(voice->SubmitSourceBuffer(&buffer)) || FAILED(voice->Start()))
    {
        voice->DestroyVoice();
        return nullptr;
    }
    return voice;
}

void Controller::Destroy(Voice &voice)
{
    if (voice.source)
        voice.source->DestroyVoice();
    voice = {};
}

void Controller::Update(const unsigned int soundId, const bool allowed, const float deltaSeconds,
                        const float fadeSeconds)
{
    if (!allowed)
    {
        Reset();
        return;
    }
    if (soundId != current_.soundId)
    {
        // A returning loop resumes from its fading voice instead of stacking.
        const auto resumed = std::find_if(fading_.begin(), fading_.end(),
            [soundId](const Voice &voice) { return voice.soundId == soundId; });
        Voice previous = current_;
        current_ = {};
        if (resumed != fading_.end())
        {
            current_ = *resumed;
            fading_.erase(resumed);
        }
        if (previous.source)
            fading_.push_back(previous);
        if (!current_.source && soundId)
        {
            current_.soundId = soundId;
            if (EnsureEngine())
                if (const Clip *clip = GetClip(soundId))
                    current_.source = StartLoop(*clip);
        }
    }

    const float step = fadeSeconds > 0.0f ?
        std::clamp(deltaSeconds, 0.0f, 1.0f) / fadeSeconds : 1.0f;
    if (current_.source)
    {
        current_.gain = std::min(1.0f, current_.gain + step);
        current_.source->SetVolume(current_.gain * kWeatherLoopGain * volumeScale_);
    }
    for (auto voice = fading_.begin(); voice != fading_.end();)
    {
        voice->gain = std::max(0.0f, voice->gain - step);
        if (voice->gain <= 0.0f || !voice->source)
        {
            Destroy(*voice);
            voice = fading_.erase(voice);
            continue;
        }
        voice->source->SetVolume(voice->gain * kWeatherLoopGain * volumeScale_);
        ++voice;
    }
}

void Controller::Reset()
{
    Destroy(current_);
    for (Voice &voice : fading_)
        Destroy(voice);
    fading_.clear();
}

void Controller::Shutdown()
{
    Reset();
    clips_.clear();
    failedClips_.clear();
    if (master_)
    {
        master_->DestroyVoice();
        master_ = nullptr;
    }
    if (engine_)
    {
        engine_->Release();
        engine_ = nullptr;
    }
    if (comInitialized_)
    {
        CoUninitialize();
        comInitialized_ = false;
    }
}

size_t Controller::ActiveVoiceCount() const
{
    return (current_.source ? 1u : 0u) + std::count_if(fading_.begin(), fading_.end(),
        [](const Voice &voice) { return voice.source != nullptr; });
}
}
