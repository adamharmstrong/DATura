#include "stdafx.h"
#include "bgw_player.h"

#include <mmsystem.h>
#include <stdint.h>
#include <ctype.h>
#include <algorithm>
#include <vector>

#pragma comment(lib, "winmm.lib")

static int16_t Clamp16(int sample)
{
    if (sample > 32767) return 32767;
    if (sample < -32768) return -32768;
    return (int16_t)sample;
}

static uint32_t ReadU32LE(const std::vector<unsigned char>& data, size_t ofs)
{
    return (uint32_t)data[ofs] |
           ((uint32_t)data[ofs + 1] << 8) |
           ((uint32_t)data[ofs + 2] << 16) |
           ((uint32_t)data[ofs + 3] << 24);
}

static int32_t ReadS32LE(const std::vector<unsigned char>& data, size_t ofs)
{
    return (int32_t)ReadU32LE(data, ofs);
}

static bool ReadWholeFile(const char* path, std::vector<unsigned char>& data)
{
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(hFile, &size) || size.QuadPart <= 0 || size.QuadPart > 0x7fffffff)
    {
        CloseHandle(hFile);
        return false;
    }

    data.resize((size_t)size.QuadPart);
    DWORD bytesRead = 0;
    const bool ok = ReadFile(hFile, data.data(), (DWORD)data.size(), &bytesRead, nullptr) &&
                    bytesRead == data.size();
    CloseHandle(hFile);
    return ok;
}

static void AppendU16LE(std::vector<unsigned char>& out, uint16_t v)
{
    out.push_back((unsigned char)(v & 0xff));
    out.push_back((unsigned char)((v >> 8) & 0xff));
}

static void AppendU32LE(std::vector<unsigned char>& out, uint32_t v)
{
    out.push_back((unsigned char)(v & 0xff));
    out.push_back((unsigned char)((v >> 8) & 0xff));
    out.push_back((unsigned char)((v >> 16) & 0xff));
    out.push_back((unsigned char)((v >> 24) & 0xff));
}

static bool WriteWholeFile(const char* path, const std::vector<unsigned char>& data)
{
    HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    DWORD written = 0;
    const bool ok = WriteFile(hFile, data.data(), (DWORD)data.size(), &written, nullptr) &&
                    written == data.size();
    CloseHandle(hFile);
    return ok;
}

static void DecodePSADPCMFrame(const unsigned char* frame, int frameSize, int& hist1, int& hist2,
                               std::vector<int16_t>& samples)
{
    static const int coefs[5][2] =
    {
        {   0,   0 },
        {  60,   0 },
        { 115, -52 },
        {  98, -55 },
        { 122, -60 },
    };

    int coefIndex = (frame[0] >> 4) & 0x0f;
    int shift = frame[0] & 0x0f;
    if (coefIndex > 4) coefIndex = 0;
    if (shift > 12) shift = 9;

    const int sampleCount = (frameSize - 1) * 2;
    for (int i = 0; i < sampleCount; ++i)
    {
        const unsigned char nibbles = frame[1 + i / 2];
        int nibble = (i & 1) ? ((nibbles >> 4) & 0x0f) : (nibbles & 0x0f);
        int sample = (int16_t)((nibble << 12) & 0xf000);
        sample >>= shift;
        sample += (coefs[coefIndex][0] * hist1 + coefs[coefIndex][1] * hist2) >> 6;
        sample = Clamp16(sample);

        samples.push_back((int16_t)sample);
        hist2 = hist1;
        hist1 = sample;
    }
}

struct ParsedAudioHeader
{
    FFXIAudioInfo info;
    uint32_t declaredSize = 0;
    uint32_t dataOffset = 0;
};

static bool ParseAudioHeader(const std::vector<unsigned char>& source, uint64_t actualFileSize,
                             ParsedAudioHeader& header)
{
    if (source.size() < 0x30)
        return false;

    const bool music = memcmp(source.data(), "BGMStream\0\0\0", 12) == 0;
    const bool soundEffect = memcmp(source.data(), "SeWave\0\0", 8) == 0;
    if (!music && !soundEffect)
        return false;

    const size_t codecOffset = 0x0c;
    const size_t sizeOffset = music ? 0x10 : 0x08;
    const size_t idOffset = music ? 0x14 : 0x10;
    const size_t sampleOffset = music ? 0x18 : 0x14;
    const size_t loopOffset = music ? 0x1c : 0x18;
    const size_t rateLowOffset = music ? 0x20 : 0x1c;
    const size_t rateHighOffset = music ? 0x24 : 0x20;
    const size_t dataOffset = music ? 0x28 : 0x24;
    const size_t channelsOffset = music ? 0x2e : 0x2a;
    const size_t blockSizeOffset = music ? 0x2f : 0x2b;

    const int32_t codecValue = ReadS32LE(source, codecOffset);
    header = {};
    header.info.kind = music ? FFXIAudioKind::Music : FFXIAudioKind::SoundEffect;
    if (codecValue == (int32_t)FFXIAudioCodec::ADPCM ||
        codecValue == (int32_t)FFXIAudioCodec::PCM ||
        codecValue == (int32_t)FFXIAudioCodec::ATRAC3)
    {
        header.info.codec = (FFXIAudioCodec)codecValue;
    }
    header.declaredSize = ReadU32LE(source, sizeOffset);
    header.info.id = ReadS32LE(source, idOffset);
    header.info.sampleBlocks = ReadU32LE(source, sampleOffset);
    header.info.loopStart = ReadS32LE(source, loopOffset);
    header.info.sampleRate = (ReadU32LE(source, rateLowOffset) +
                              ReadU32LE(source, rateHighOffset)) & 0x7fffffff;
    header.dataOffset = ReadU32LE(source, dataOffset);
    header.info.channels = source[channelsOffset];
    header.info.blockSize = source[blockSizeOffset];

    if (header.declaredSize != actualFileSize || header.info.sampleRate == 0 ||
        header.info.channels == 0 || header.info.channels > 2 ||
        (header.info.codec == FFXIAudioCodec::ADPCM && header.info.blockSize == 0) ||
        header.dataOffset < 0x30 ||
        header.dataOffset >= actualFileSize)
    {
        return false;
    }

    if (header.info.codec == FFXIAudioCodec::ADPCM)
    {
        header.info.durationSeconds =
            (double)header.info.sampleBlocks * header.info.blockSize / header.info.sampleRate;
    }
    else
    {
        header.info.durationSeconds =
            (double)header.info.sampleBlocks / header.info.sampleRate;
    }
    return true;
}

static bool DecodeAudioToWAV(const char* sourcePath, const char* wavPath)
{
    std::vector<unsigned char> source;
    if (!ReadWholeFile(sourcePath, source))
        return false;

    ParsedAudioHeader header;
    if (!ParseAudioHeader(source, source.size(), header))
        return false;

    const int channels = (int)header.info.channels;
    const int blockSize = (int)header.info.blockSize;
    const uint32_t sampleRate = header.info.sampleRate;
    std::vector<int16_t> pcm;

    if (header.info.codec == FFXIAudioCodec::ADPCM)
    {
        const int frameSize = (blockSize / 2) + 1;
        const int samplesPerFrame = (frameSize - 1) * 2;
        const uint64_t targetSamples = (uint64_t)header.info.sampleBlocks * blockSize;
        const uint64_t totalSamples = targetSamples * channels;
        if (frameSize <= 1 || samplesPerFrame <= 0 || targetSamples == 0 ||
            totalSamples > (UINT32_MAX - 36ull) / sizeof(int16_t))
            return false;

        const uint64_t requiredBytes =
            (uint64_t)header.info.sampleBlocks * frameSize * channels;
        if (requiredBytes > source.size() - header.dataOffset)
            return false;
        pcm.reserve((size_t)totalSamples);

        int hist1[2] = {};
        int hist2[2] = {};
        size_t dataOfs = header.dataOffset;
        while (dataOfs + (size_t)frameSize * channels <= source.size() &&
               pcm.size() < (size_t)targetSamples * (size_t)channels)
        {
            std::vector<int16_t> channelSamples[2];
            for (int ch = 0; ch < channels; ++ch)
            {
                DecodePSADPCMFrame(&source[dataOfs + (size_t)frameSize * ch], frameSize,
                                   hist1[ch], hist2[ch], channelSamples[ch]);
            }

            for (int s = 0;
                 s < samplesPerFrame &&
                 pcm.size() < (size_t)targetSamples * (size_t)channels;
                 ++s)
            {
                for (int ch = 0; ch < channels; ++ch)
                    pcm.push_back(channelSamples[ch][s]);
            }

            dataOfs += (size_t)frameSize * channels;
        }
    }
    else if (header.info.codec == FFXIAudioCodec::PCM)
    {
        const uint64_t requestedBytes =
            (uint64_t)header.info.sampleBlocks * channels * sizeof(int16_t);
        const size_t availableBytes = source.size() - header.dataOffset;
        if (requestedBytes > availableBytes || requestedBytes > UINT32_MAX - 36ull ||
            requestedBytes < (size_t)channels * sizeof(int16_t))
            return false;
        const size_t pcmBytes = (size_t)requestedBytes;
        pcm.resize(pcmBytes / sizeof(int16_t));
        memcpy(pcm.data(), source.data() + header.dataOffset, pcm.size() * sizeof(int16_t));
    }
    else
    {
        return false;
    }

    if (pcm.empty())
        return false;

    std::vector<unsigned char> wav;
    const uint32_t dataBytes = (uint32_t)(pcm.size() * sizeof(int16_t));
    const uint16_t outBlockAlign = (uint16_t)(channels * sizeof(int16_t));
    const uint32_t byteRate = sampleRate * outBlockAlign;

    wav.insert(wav.end(), { 'R','I','F','F' });
    AppendU32LE(wav, 36 + dataBytes);
    wav.insert(wav.end(), { 'W','A','V','E','f','m','t',' ' });
    AppendU32LE(wav, 16);
    AppendU16LE(wav, 1);
    AppendU16LE(wav, (uint16_t)channels);
    AppendU32LE(wav, sampleRate);
    AppendU32LE(wav, byteRate);
    AppendU16LE(wav, outBlockAlign);
    AppendU16LE(wav, 16);
    wav.insert(wav.end(), { 'd','a','t','a' });
    AppendU32LE(wav, dataBytes);
    const unsigned char* pcmBytes = (const unsigned char*)pcm.data();
    wav.insert(wav.end(), pcmBytes, pcmBytes + dataBytes);

    return WriteWholeFile(wavPath, wav);
}

static bool FindBGWPath(const char* ffxiRootPath, int musicId, char* outPath, DWORD outPathSize)
{
    static const char* kSoundRoots[] =
    {
        "sound", "sound2", "sound3", "sound4", "sound5", "sound6", "sound9"
    };

    for (const char* soundRoot : kSoundRoots)
    {
        sprintf_s(outPath, outPathSize, "%s%s\\win\\music\\data\\music%03d.bgw",
                  ffxiRootPath, soundRoot, musicId);
        if (GetFileAttributesA(outPath) != INVALID_FILE_ATTRIBUTES)
            return true;
    }

    return false;
}

static uint64_t HashSourcePath(const char* path)
{
    uint64_t hash = 1469598103934665603ull;
    for (const unsigned char* p = (const unsigned char*)path; p && *p; ++p)
    {
        hash ^= (uint64_t)tolower(*p);
        hash *= 1099511628211ull;
    }

    WIN32_FILE_ATTRIBUTE_DATA attributes = {};
    if (GetFileAttributesExA(path, GetFileExInfoStandard, &attributes))
    {
        const uint32_t values[] =
        {
            attributes.nFileSizeLow, attributes.nFileSizeHigh,
            attributes.ftLastWriteTime.dwLowDateTime,
            attributes.ftLastWriteTime.dwHighDateTime,
        };
        for (uint32_t value : values)
        {
            hash ^= value;
            hash *= 1099511628211ull;
        }
    }
    return hash;
}

static bool GetCachedWAVPath(const char* sourcePath, char* outPath, DWORD outPathSize)
{
    char tempPath[MAX_PATH] = {};
    if (!GetTempPathA(sizeof(tempPath), tempPath))
        return false;

    char cacheDir[MAX_PATH] = {};
    sprintf_s(cacheDir, "%sDATuraAudio", tempPath);
    CreateDirectoryA(cacheDir, nullptr);
    const uint64_t hash = HashSourcePath(sourcePath);
    sprintf_s(outPath, outPathSize, "%s\\%08X%08X.wav", cacheDir,
              (uint32_t)(hash >> 32), (uint32_t)hash);
    return true;
}

bool FFXIAudio_ReadInfo(const char* path, FFXIAudioInfo* outInfo)
{
    if (!path || !path[0] || !outInfo)
        return false;

    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    LARGE_INTEGER size = {};
    std::vector<unsigned char> headerBytes(0x30);
    DWORD bytesRead = 0;
    const bool read = GetFileSizeEx(hFile, &size) && size.QuadPart >= 0x30 &&
                      ReadFile(hFile, headerBytes.data(), (DWORD)headerBytes.size(),
                               &bytesRead, nullptr) && bytesRead == headerBytes.size();
    CloseHandle(hFile);
    if (!read)
        return false;

    ParsedAudioHeader header;
    if (!ParseAudioHeader(headerBytes, (uint64_t)size.QuadPart, header))
        return false;
    *outInfo = header.info;
    return true;
}

const char* FFXIAudio_CodecName(FFXIAudioCodec codec)
{
    switch (codec)
    {
    case FFXIAudioCodec::ADPCM: return "ADPCM";
    case FFXIAudioCodec::PCM: return "PCM";
    case FFXIAudioCodec::ATRAC3: return "ATRAC3";
    default: return "Unknown";
    }
}

void BGM_Stop()
{
    PlaySoundA(nullptr, nullptr, 0);
}

bool FFXIAudio_PlayFile(const char* path, bool loop)
{
    char wavPath[MAX_PATH] = {};
    if (!FFXIAudio_PrepareFile(path, wavPath, sizeof(wavPath)))
        return false;
    return FFXIAudio_PlayPreparedFile(wavPath, loop);
}

bool FFXIAudio_PrepareFile(const char* path, char* outWavPath, size_t outWavPathSize)
{
    if (!path || !path[0] || !outWavPath || outWavPathSize == 0 ||
        outWavPathSize > MAXDWORD)
        return false;

    if (!GetCachedWAVPath(path, outWavPath, (DWORD)outWavPathSize))
        return false;

    if (GetFileAttributesA(outWavPath) == INVALID_FILE_ATTRIBUTES)
    {
        char temporaryPath[MAX_PATH] = {};
        sprintf_s(temporaryPath, "%s.%08X.%08X.tmp", outWavPath,
                  GetCurrentProcessId(), GetCurrentThreadId());
        if (!DecodeAudioToWAV(path, temporaryPath))
        {
            DeleteFileA(temporaryPath);
            return false;
        }
        if (!MoveFileExA(temporaryPath, outWavPath,
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            DeleteFileA(temporaryPath);
            if (GetFileAttributesA(outWavPath) == INVALID_FILE_ATTRIBUTES)
                return false;
        }
    }
    return true;
}

bool FFXIAudio_PlayPreparedFile(const char* wavPath, bool loop)
{
    BGM_Stop();
    if (!wavPath || !wavPath[0])
        return false;

    DWORD flags = SND_FILENAME | SND_ASYNC | SND_NODEFAULT;
    if (loop)
        flags |= SND_LOOP;
    return PlaySoundA(wavPath, nullptr, flags) != FALSE;
}

bool BGM_PlayZoneMusic(const char* ffxiRootPath, int musicId)
{
    BGM_Stop();

    if (!ffxiRootPath || !ffxiRootPath[0] || musicId <= 0)
        return false;

    char bgwPath[MAX_PATH] = {};
    if (!FindBGWPath(ffxiRootPath, musicId, bgwPath, sizeof(bgwPath)))
        return false;

    return FFXIAudio_PlayFile(bgwPath, true);
}
