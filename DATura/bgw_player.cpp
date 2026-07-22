#include "stdafx.h"
#include "bgw_player.h"

#include <mmsystem.h>
#include <stdint.h>
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

static bool DecodeBGWToWAV(const char* bgwPath, const char* wavPath)
{
    std::vector<unsigned char> bgw;
    if (!ReadWholeFile(bgwPath, bgw) || bgw.size() < 0x30)
        return false;

    if (memcmp(bgw.data(), "BGMStream\0\0\0", 12) != 0)
        return false;

    const uint32_t codec = ReadU32LE(bgw, 0x0c);
    const uint32_t fileSize = ReadU32LE(bgw, 0x10);
    const uint32_t blockSize = ReadU32LE(bgw, 0x18);
    const uint32_t sampleRate = (ReadU32LE(bgw, 0x20) + ReadU32LE(bgw, 0x24)) & 0x7fffffff;
    const uint32_t startOffset = ReadU32LE(bgw, 0x28);
    const int channels = (int)bgw[0x2e];
    const int blockAlign = (int)bgw[0x2f];

    if (codec != 0 || fileSize != bgw.size() || sampleRate == 0 ||
        channels <= 0 || channels > 2 || blockAlign <= 0 || startOffset >= bgw.size())
    {
        return false;
    }

    const int frameSize = (blockAlign / 2) + 1;
    const int samplesPerFrame = (frameSize - 1) * 2;
    const uint32_t targetSamples = blockSize * blockAlign;
    if (frameSize <= 1 || samplesPerFrame <= 0 || targetSamples == 0)
        return false;

    std::vector<int16_t> pcm;
    pcm.reserve((size_t)targetSamples * (size_t)channels);

    int hist1[2] = {};
    int hist2[2] = {};
    size_t dataOfs = startOffset;
    while (dataOfs + (size_t)frameSize * channels <= bgw.size() &&
           pcm.size() < (size_t)targetSamples * (size_t)channels)
    {
        std::vector<int16_t> channelSamples[2];
        for (int ch = 0; ch < channels; ++ch)
        {
            DecodePSADPCMFrame(&bgw[dataOfs + (size_t)frameSize * ch], frameSize,
                               hist1[ch], hist2[ch], channelSamples[ch]);
        }

        for (int s = 0; s < samplesPerFrame && pcm.size() < (size_t)targetSamples * (size_t)channels; ++s)
        {
            for (int ch = 0; ch < channels; ++ch)
            {
                pcm.push_back(channelSamples[ch][s]);
            }
        }

        dataOfs += (size_t)frameSize * channels;
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

static bool GetCachedWAVPath(int musicId, char* outPath, DWORD outPathSize)
{
    char tempPath[MAX_PATH] = {};
    if (!GetTempPathA(sizeof(tempPath), tempPath))
        return false;

    char cacheDir[MAX_PATH] = {};
    sprintf_s(cacheDir, "%sDATuraMusic", tempPath);
    CreateDirectoryA(cacheDir, nullptr);
    sprintf_s(outPath, outPathSize, "%s\\music%03d.wav", cacheDir, musicId);
    return true;
}

void BGM_Stop()
{
    PlaySoundA(nullptr, nullptr, 0);
}

bool BGM_PlayZoneMusic(const char* ffxiRootPath, int musicId)
{
    BGM_Stop();

    if (!ffxiRootPath || !ffxiRootPath[0] || musicId <= 0)
        return false;

    char bgwPath[MAX_PATH] = {};
    if (!FindBGWPath(ffxiRootPath, musicId, bgwPath, sizeof(bgwPath)))
        return false;

    char wavPath[MAX_PATH] = {};
    if (!GetCachedWAVPath(musicId, wavPath, sizeof(wavPath)))
        return false;

    if (GetFileAttributesA(wavPath) == INVALID_FILE_ATTRIBUTES)
    {
        if (!DecodeBGWToWAV(bgwPath, wavPath))
            return false;
    }

    return PlaySoundA(wavPath, nullptr, SND_FILENAME | SND_ASYNC | SND_LOOP | SND_NODEFAULT) != FALSE;
}

