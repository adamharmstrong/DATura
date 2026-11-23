#pragma once

#include <stddef.h>
#include <stdint.h>

enum class FFXIAudioKind
{
    Unknown,
    Music,
    SoundEffect,
};

enum class FFXIAudioCodec
{
    ADPCM = 0,
    PCM = 1,
    ATRAC3 = 3,
    Unknown = -1,
};

struct FFXIAudioInfo
{
    FFXIAudioKind kind = FFXIAudioKind::Unknown;
    FFXIAudioCodec codec = FFXIAudioCodec::Unknown;
    int id = 0;
    uint32_t sampleBlocks = 0;
    int32_t loopStart = -1;
    uint32_t sampleRate = 0;
    uint8_t channels = 0;
    uint8_t blockSize = 0;
    double durationSeconds = 0.0;
};

// Reads the common BGMStream/SeWave header without decoding the payload.
bool FFXIAudio_ReadInfo(const char* path, FFXIAudioInfo* outInfo);

// Decodes a BGMStream or SeWave file to a temporary WAV and starts playback.
// ADPCM and PCM payloads are supported. ATRAC3 metadata is recognized but is
// not decoded by the native preview player.
bool FFXIAudio_PlayFile(const char* path, bool loop);

// Splits potentially expensive decoding from the fast playback call so UI
// callers can prepare files on a worker thread and start them on the UI thread.
bool FFXIAudio_PrepareFile(const char* path, char* outWavPath, size_t outWavPathSize);
bool FFXIAudio_PlayPreparedFile(const char* wavPath, bool loop);

const char* FFXIAudio_CodecName(FFXIAudioCodec codec);

void BGM_Stop();
bool BGM_PlayZoneMusic(const char* ffxiRootPath, int musicId);
