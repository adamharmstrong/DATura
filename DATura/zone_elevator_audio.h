#pragma once

#include "bgw_player.h"
#include "ffxi_coordinate_frame.h"
#include "ffxi_file_io.h"
#include "zone_elevator.h"

#include <memory>
#include <string>
#include <vector>
#include <xaudio2.h>

#pragma comment(lib, "xaudio2.lib")

namespace ZoneElevator
{
class Audio
{
    struct Clip
    {
        std::vector<BYTE> pcm;
        WAVEFORMATEX format = {};
    };

    IXAudio2* engine_ = nullptr;
    IXAudio2MasteringVoice* master_ = nullptr;
    IXAudio2SourceVoice* voice_ = nullptr;
    Clip upward_;
    Clip downward_;
    bool comInitialized_ = false;
    int lastLeg_ = -1;

    static constexpr float kFullVolumeDistance = 6.0f;
    static constexpr float kAudibleDistance = 30.0f;

    static float DistanceGain(const float distance)
    {
        const float fade = std::clamp(
            (kAudibleDistance - distance) /
                (kAudibleDistance - kFullVolumeDistance),
            0.0f, 1.0f);
        return fade * fade * 0.65f;
    }

    static bool LoadClip(const char* root, const unsigned int id, Clip& clip)
    {
        char relative[80] = {};
        sprintf_s(relative, "\\win\\se\\se%03u\\se%06u.spw", id / 1000, id);
        std::string path;
        for (const char* folder : { "sound", "sound2", "sound3", "sound4", "sound5", "sound6", "sound9" })
        {
            path = std::string(root) + "\\" + folder + relative;
            if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES)
                break;
        }

        char wav[MAX_PATH] = {};
        if (!FFXIAudio_PrepareFile(path.c_str(), wav, sizeof(wav)))
            return false;

        BYTE* raw = nullptr;
        DWORD count = 0;
        if (!FFXIFileIO::ReadWholeFile(wav, &raw, &count))
            return false;
        std::unique_ptr<BYTE[]> bytes(raw);
        if (count < 44 || memcmp(raw, "RIFF", 4) || memcmp(raw + 8, "WAVE", 4))
            return false;

        for (size_t p = 12; p + 8 <= count;)
        {
            DWORD size = 0;
            memcpy(&size, raw + p + 4, sizeof(size));
            if (size > count - p - 8)
                return false;
            if (!memcmp(raw + p, "fmt ", 4) && size >= 16)
                memcpy(&clip.format, raw + p + 8, 16);
            else if (!memcmp(raw + p, "data", 4))
                clip.pcm.assign(raw + p + 8, raw + p + 8 + size);
            p += 8 + size + (size & 1);
        }
        return clip.format.wFormatTag == WAVE_FORMAT_PCM &&
               clip.format.wBitsPerSample == 16 && !clip.pcm.empty();
    }

    void Play(const Clip& clip, const float gain)
    {
        if (!engine_ || clip.pcm.empty())
            return;
        if (voice_)
        {
            voice_->DestroyVoice();
            voice_ = nullptr;
        }
        if (FAILED(engine_->CreateSourceVoice(&voice_, &clip.format)))
            return;
        XAUDIO2_BUFFER buffer = {};
        buffer.Flags = XAUDIO2_END_OF_STREAM;
        buffer.AudioBytes = static_cast<UINT32>(clip.pcm.size());
        buffer.pAudioData = clip.pcm.data();
        voice_->SetVolume(gain);
        if (FAILED(voice_->SubmitSourceBuffer(&buffer)) || FAILED(voice_->Start()))
        {
            voice_->DestroyVoice();
            voice_ = nullptr;
        }
    }

public:
    ~Audio()
    {
        Shutdown();
    }

    bool Load(const char* root)
    {
        Shutdown();
        if (!root || !root[0] || !LoadClip(root, 8082, upward_) || !LoadClip(root, 8083, downward_))
            return false;
        const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        comInitialized_ = SUCCEEDED(com);
        if (FAILED(com) && com != RPC_E_CHANGED_MODE)
            return false;
        if (FAILED(XAudio2Create(&engine_, 0, XAUDIO2_DEFAULT_PROCESSOR)) ||
            FAILED(engine_->CreateMasteringVoice(&master_)))
        {
            Shutdown();
            return false;
        }
        return true;
    }

    void Reset()
    {
        lastLeg_ = -1;
        if (voice_)
        {
            voice_->DestroyVoice();
            voice_ = nullptr;
        }
    }

    void Shutdown()
    {
        Reset();
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
        upward_ = {};
        downward_ = {};
    }

    void Update(const State& state, const float scenePosition[3], const bool mirrorX,
                const bool allowed)
    {
        if (!allowed)
        {
            Reset();
            return;
        }
        if (!master_)
            return;

        const auto native = FFXICoordinateFrame::SceneToNativeDat(
            { scenePosition[0], scenePosition[1], scenePosition[2] }, mirrorX);
        int nearest = 0;
        float nearestDistanceSquared = FLT_MAX;
        for (int i = 0; i < 2; ++i)
        {
            const float dx = native[0] - kPlatformX;
            const float dy = native[1] - state.platforms[i].lastY;
            const float dz = native[2] - state.platforms[i].z;
            const float distanceSquared = dx * dx + dy * dy + dz * dz;
            if (distanceSquared < nearestDistanceSquared)
            {
                nearestDistanceSquared = distanceSquared;
                nearest = i;
            }
        }

        const float gain = DistanceGain(std::sqrt(nearestDistanceSquared));
        if (gain <= 0.0f)
        {
            if (voice_)
            {
                voice_->DestroyVoice();
                voice_ = nullptr;
            }
            return;
        }
        if (voice_)
            voice_->SetVolume(gain);

        const int leg = static_cast<int>(state.elapsed / kLegPeriod);
        const float legTime = state.elapsed - static_cast<float>(leg) * kLegPeriod;
        if (leg == lastLeg_ || legTime >= kTravelTime)
            return;
        lastLeg_ = leg;

        const bool startsAtTop = (leg % 2 == 0)
            ? state.platforms[nearest].startsAtTop
            : !state.platforms[nearest].startsAtTop;
        Play(startsAtTop ? downward_ : upward_, gain);
    }
};
} // namespace ZoneElevator
