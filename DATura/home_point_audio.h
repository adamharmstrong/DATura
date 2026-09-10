#pragma once
#include "bgw_player.h"
#include "ffxi_file_io.h"
#include "home_point_effect.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <string>
#include <vector>
#include <xaudio2.h>
#pragma comment(lib, "xaudio2.lib")

namespace HomePoint
{
class Audio
{
    struct Clip
    {
        std::vector<BYTE> pcm;
        WAVEFORMATEX format = {};
        UINT32 loopBegin = 0;
    };
    struct Voice
    {
        IXAudio2SourceVoice *source = nullptr;
    };
    IXAudio2 *engine_ = nullptr;
    IXAudio2MasteringVoice *master_ = nullptr;
    Clip ambient_, activation_;
    std::map<uint32_t, Voice> voices_;
    IXAudio2SourceVoice *activationVoice_ = nullptr;
    bool allowed_ = false;
    UINT32 channels_ = 2;
    bool comInitialized_ = false;
    Instance activeInstance_;
    int limit_ = 8;
    static bool LoadClip(const char *root, unsigned int id, Clip &clip)
    {
        const std::string prefix = std::string(root) + "/";
        char relative[80];
        sprintf_s(relative, "/win/se/se%03u/se%06u.spw", id / 1000, id);
        std::string path;
        for (const char *folder : {"sound", "sound2", "sound3", "sound4", "sound5", "sound6", "sound9"})
        {
            path = prefix + folder + relative;
            if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES)
                break;
        }
        FFXIAudioInfo info;
        char wav[MAX_PATH] = {};
        if (!FFXIAudio_ReadInfo(path.c_str(), &info) ||
            !FFXIAudio_PrepareFile(path.c_str(), wav, sizeof(wav)))
            return false;
        BYTE *raw = nullptr;
        DWORD count = 0;
        if (!FFXIFileIO::ReadWholeFile(wav, &raw, &count))
            return false;
        std::unique_ptr<BYTE[]> bytes(raw);
        if (count < 44 || memcmp(raw, "RIFF", 4) || memcmp(raw + 8, "WAVE", 4))
            return false;
        for (size_t p = 12; p + 8 <= count;)
        {
            DWORD n;
            memcpy(&n, raw + p + 4, 4);
            if (n > count - p - 8)
                return false;
            if (!memcmp(raw + p, "fmt ", 4) && n >= 16)
                memcpy(&clip.format, raw + p + 8, 16);
            if (!memcmp(raw + p, "data", 4))
                clip.pcm.assign(raw + p + 8, raw + p + 8 + n);
            p += 8 + n + (n & 1);
        }
        if (clip.format.wFormatTag != WAVE_FORMAT_PCM || clip.format.wBitsPerSample != 16 ||
            clip.pcm.empty() || !clip.format.nBlockAlign)
            return false;
        const uint64_t frame =
            info.loopStart < 0
                ? 0
                : uint64_t(info.loopStart) * (info.codec == FFXIAudioCodec::ADPCM ? info.blockSize : 1);
        const size_t frames = clip.pcm.size() / clip.format.nBlockAlign;
        clip.loopBegin = frame < frames ? (UINT32)frame : 0;
        return true;
    }
    IXAudio2SourceVoice *Start(const Clip &clip, bool loop)
    {
        IXAudio2SourceVoice *voice = nullptr;
        if (!engine_ || clip.pcm.empty() || FAILED(engine_->CreateSourceVoice(&voice, &clip.format)))
            return nullptr;
        XAUDIO2_BUFFER b = {};
        b.Flags = XAUDIO2_END_OF_STREAM;
        b.AudioBytes = (UINT32)clip.pcm.size();
        b.pAudioData = clip.pcm.data();
        if (loop)
        {
            b.LoopBegin = clip.loopBegin;
            b.LoopLength = (UINT32)(clip.pcm.size() / clip.format.nBlockAlign) - b.LoopBegin;
            b.LoopCount = XAUDIO2_LOOP_INFINITE;
        }
        voice->SetVolume(0);
        if (FAILED(voice->SubmitSourceBuffer(&b)) || FAILED(voice->Start()))
        {
            voice->DestroyVoice();
            return nullptr;
        }
        return voice;
    }
    void Spatial(IXAudio2SourceVoice *voice, const Clip &clip, const Instance &instance,
                 const float *listener, const float *right)
    {
        if (!voice)
            return;
        float delta[3];
        float distance = 0;
        for (int a = 0; a < 3; ++a)
        {
            delta[a] = instance.position[a] - listener[a];
            distance += delta[a] * delta[a];
        }
        distance = sqrtf(distance);
        const float gain = std::clamp(1.f - distance / 25.f, 0.f, 1.f);
        voice->SetVolume(gain * gain * .45f);
        float pan = 0;
        for (int a = 0; a < 3; ++a)
            pan += delta[a] * right[a];
        pan = std::clamp(pan / std::max(distance, 1.f), -1.f, 1.f);
        if (channels_ == 2)
        {
            std::vector<float> matrix(clip.format.nChannels * 2);
            for (int c = 0; c < clip.format.nChannels; ++c)
            {
                matrix[c * 2] = sqrtf((1 - pan) * .5f) / clip.format.nChannels;
                matrix[c * 2 + 1] = sqrtf((1 + pan) * .5f) / clip.format.nChannels;
            }
            voice->SetOutputMatrix(master_, clip.format.nChannels, 2, matrix.data());
        }
    }

  public:
    ~Audio()
    {
        Stop();
        if (master_)
            master_->DestroyVoice();
        if (engine_)
            engine_->Release();
        if (comInitialized_)
            CoUninitialize();
    }
    bool Load(const char *root, unsigned int ambient, unsigned int activation)
    {
        if (!LoadClip(root, ambient, ambient_) || !LoadClip(root, activation, activation_))
            return false;
        const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        comInitialized_ = SUCCEEDED(com);
        if (FAILED(com) && com != RPC_E_CHANGED_MODE)
            return false;
        if (FAILED(XAudio2Create(&engine_, 0, XAUDIO2_DEFAULT_PROCESSOR)))
            return false;
        if (FAILED(engine_->CreateMasteringVoice(&master_)))
            return false;
        XAUDIO2_VOICE_DETAILS details;
        master_->GetVoiceDetails(&details);
        channels_ = details.InputChannels;
        return true;
    }
    bool Ready() const
    {
        return master_ != nullptr;
    }
    size_t VoiceCount() const
    {
        return std::count_if(voices_.begin(), voices_.end(),
                             [](const auto &item) { return item.second.source != nullptr; }) +
               (activationVoice_ ? 1 : 0);
    }
    void Stop()
    {
        for (auto &[id, v] : voices_)
            if (v.source)
                v.source->DestroyVoice();
        voices_.clear();
        if (activationVoice_)
        {
            activationVoice_->DestroyVoice();
            activationVoice_ = nullptr;
        }
        allowed_ = false;
    }
    void Update(const std::vector<Instance> &instances, const float *listener, const float *right,
                bool allowed, int maxSounds)
    {
        if (!allowed || maxSounds == 0 || !Ready())
        {
            Stop();
            return;
        }
        allowed_ = true;
        limit_ = maxSounds < 0 ? 8 : std::min(8, maxSounds);
        if (activationVoice_)
        {
            XAUDIO2_VOICE_STATE state;
            activationVoice_->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
            if (!state.BuffersQueued)
            {
                activationVoice_->DestroyVoice();
                activationVoice_ = nullptr;
            }
            else
                Spatial(activationVoice_, activation_, activeInstance_, listener, right);
        }
        std::vector<std::pair<float, const Instance *>> nearest;
        for (const auto &i : instances)
        {
            float d = 0;
            for (int a = 0; a < 3; ++a)
                d += (i.position[a] - listener[a]) * (i.position[a] - listener[a]);
            if (d < 625)
                nearest.push_back({d, &i});
        }
        std::sort(nearest.begin(), nearest.end(),
                  [](const auto &a, const auto &b) { return a.first < b.first; });
        const int limit = std::max(0, limit_ - (activationVoice_ ? 1 : 0));
        if (nearest.size() > (size_t)limit)
            nearest.resize(limit);
        for (auto it = voices_.begin(); it != voices_.end();)
        {
            if (std::none_of(nearest.begin(), nearest.end(),
                             [&](const auto &p) { return p.second->id == it->first; }))
            {
                if (it->second.source)
                    it->second.source->DestroyVoice();
                it = voices_.erase(it);
            }
            else
                ++it;
        }
        for (const auto &[d, i] : nearest)
        {
            auto &v = voices_[i->id];
            if (!v.source)
                v.source = Start(ambient_, true);
            Spatial(v.source, ambient_, *i, listener, right);
        }
    }
    void Activate(const Instance &instance, const float *listener, const float *right)
    {
        if (!allowed_)
            return;
        if (activationVoice_)
            activationVoice_->DestroyVoice();
        activationVoice_ = nullptr;
        while (voices_.size() >= (size_t)limit_ && !voices_.empty())
        {
            auto it = std::prev(voices_.end());
            if (it->second.source)
                it->second.source->DestroyVoice();
            voices_.erase(it);
        }
        activeInstance_ = instance;
        activationVoice_ = Start(activation_, false);
        Spatial(activationVoice_, activation_, instance, listener, right);
    }
};
} // namespace HomePoint
