#include "stdafx.h"
#include "ffxi_sqle_motion.h"

#include "ffxi_file_io.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace FFXISqle
{
bool ReadMotionInfo(const char* ffxiRoot, const char* relativePath, MotionInfo* outInfo)
{
    if (outInfo)
        *outInfo = {};
    if (!ffxiRoot || !relativePath || !outInfo)
        return false;

    char fullPath[MAX_PATH] = {};
    sprintf_s(fullPath, "%s%s", ffxiRoot, relativePath);
    BYTE* buffer = nullptr;
    DWORD fileSize = 0;
    if (!FFXIFileIO::ReadWholeFile(fullPath, &buffer, &fileSize))
        return false;

    bool ok = false;
    if (fileSize >= 96 && memcmp(buffer, "SQLE", 4) == 0)
    {
        char header[192] = {};
        const int copyLength = std::min<int>(static_cast<int>(sizeof(header) - 1), static_cast<int>(fileSize));
        memcpy(header, buffer, copyLength);
        for (int i = 0; i < copyLength; ++i)
        {
            if (header[i] == '\0')
                header[i] = ' ';
        }

        const char* motionHeader = strstr(header, "MOTION:");
        char channelName[64] = {};
        float timeSeconds = 0.0f;
        int channelCount = 0;
        int frameCount = 0;
        float step = 0.0f;
        const int parsed = motionHeader ?
            sscanf_s(motionHeader, "MOTION: %63[^,], time=%f, size=%i, frames=%i, step=%f",
                     channelName, static_cast<unsigned>(_countof(channelName)),
                     &timeSeconds, &channelCount, &frameCount, &step) : 0;
        if (parsed >= 4 && channelCount > 0 && frameCount > 0)
        {
            outInfo->valid = true;
            outInfo->frameChannel = strstr(channelName, "FrameChannel") != nullptr;
            outInfo->pbChannel = strstr(channelName, "PBChannel") != nullptr;
            outInfo->timeSeconds = timeSeconds;
            outInfo->frameCount = frameCount;
            outInfo->channelCount = channelCount;
            ok = true;

            if (outInfo->frameChannel)
            {
                const size_t valueCount = static_cast<size_t>(channelCount) * static_cast<size_t>(frameCount);
                const size_t byteCount = valueCount * sizeof(float);
                const size_t controlByteCount = static_cast<size_t>(channelCount) * sizeof(unsigned int);
                if (valueCount == 0 || valueCount > 10000000 ||
                    byteCount > fileSize || controlByteCount > fileSize - byteCount)
                {
                    *outInfo = {};
                    ok = false;
                }
                else
                {
                    const size_t dataOffset = static_cast<size_t>(fileSize) - byteCount;
                    outInfo->frameValues.resize(valueCount);
                    memcpy(outInfo->frameValues.data(), buffer + dataOffset, byteCount);
                }
            }
            else if (outInfo->pbChannel)
            {
                const int sampleCount = frameCount - 1;
                size_t recordOffset = 0x74;
                const size_t valueCount = static_cast<size_t>(channelCount) * static_cast<size_t>(frameCount);
                if (sampleCount <= 0 || valueCount == 0 || valueCount > 10000000)
                {
                    *outInfo = {};
                    ok = false;
                }
                else
                {
                    outInfo->frameValues.assign(valueCount, 0.0f);
                    for (int channelIndex = 0; channelIndex < channelCount && ok; ++channelIndex)
                    {
                        if (recordOffset + 16 > fileSize)
                        {
                            ok = false;
                            break;
                        }
                        const unsigned int dataBytes = *(const unsigned int*)(buffer + recordOffset + 0);
                        const unsigned int bitsPerSample = *(const unsigned int*)(buffer + recordOffset + 4);
                        const float quantStep = *(const float*)(buffer + recordOffset + 8);
                        const float baseValue = *(const float*)(buffer + recordOffset + 12);
                        const size_t expectedBytes = bitsPerSample == 0 ? 1 :
                            ((static_cast<size_t>(sampleCount) * bitsPerSample + 7) / 8);
                        if (bitsPerSample > 16 ||
                            dataBytes != expectedBytes || recordOffset + 16 + dataBytes > fileSize ||
                            !std::isfinite(quantStep) || !std::isfinite(baseValue))
                        {
                            ok = false;
                            break;
                        }

                        const unsigned char* packed = buffer + recordOffset + 16;

                        // PB skeletal channels are signed-magnitude deltas from
                        // the preceding frame. This includes the root channels:
                        // their accumulated translation is the authored actor
                        // trajectory and must remain aligned with the cinematic
                        // camera track.
                        float accumulatedValue = baseValue;
                        outInfo->frameValues[static_cast<size_t>(channelIndex)] =
                            accumulatedValue;
                        for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
                        {
                            unsigned int code = 0;
                            if (bitsPerSample != 0)
                            {
                                // PBChannel packs codes most-significant-bit first. The
                                // high bit is a sign and the remaining bits are the
                                // magnitude of a delta from the previous frame.
                                // A zero-padded big-endian 24-bit window handles every
                                // observed width (2, 4, 8, and 16 bits) without reading
                                // across the end of a record.
                                const size_t bitOffset = static_cast<size_t>(sampleIndex) * bitsPerSample;
                                const size_t byteOffset = bitOffset >> 3;
                                const unsigned int bitInByte = static_cast<unsigned int>(bitOffset & 7);
                                unsigned int packedWord = static_cast<unsigned int>(packed[byteOffset]) << 16;
                                if (byteOffset + 1 < dataBytes)
                                    packedWord |= static_cast<unsigned int>(packed[byteOffset + 1]) << 8;
                                if (byteOffset + 2 < dataBytes)
                                    packedWord |= static_cast<unsigned int>(packed[byteOffset + 2]);
                                const unsigned int mask = (1u << bitsPerSample) - 1u;
                                code = (packedWord >> (24u - bitInByte - bitsPerSample)) & mask;
                            }
                            const unsigned int signBit = bitsPerSample == 0 ? 0u : 1u << (bitsPerSample - 1u);
                            const unsigned int magnitude = code & (signBit - 1u);
                            const float signedMagnitude = (code & signBit) ?
                                -static_cast<float>(magnitude) : static_cast<float>(magnitude);
                            accumulatedValue += signedMagnitude * quantStep;
                            outInfo->frameValues[static_cast<size_t>(sampleIndex + 1) *
                                                 static_cast<size_t>(channelCount) +
                                                 static_cast<size_t>(channelIndex)] = accumulatedValue;
                        }
                        recordOffset += 16 + dataBytes;
                    }
                    if (!ok)
                        *outInfo = {};
                }
            }
        }
    }

    delete[] buffer;
    return ok;
}

float FrameValue(const MotionInfo& motion, int frameIndex, int channelIndex)
{
    if (!motion.valid || motion.frameCount <= 0 ||
        channelIndex < 0 || channelIndex >= motion.channelCount)
    {
        return 0.0f;
    }
    frameIndex = std::max(0, std::min(frameIndex, motion.frameCount - 1));
    const size_t index = static_cast<size_t>(frameIndex) * static_cast<size_t>(motion.channelCount) +
                         static_cast<size_t>(channelIndex);
    return index < motion.frameValues.size() ? motion.frameValues[index] : 0.0f;
}

void NormalizeQuaternion(float quaternion[4])
{
    const float length = std::sqrt(quaternion[0] * quaternion[0] + quaternion[1] * quaternion[1] +
                                   quaternion[2] * quaternion[2] + quaternion[3] * quaternion[3]);
    if (length <= 0.00001f)
    {
        quaternion[0] = quaternion[1] = quaternion[2] = 0.0f;
        quaternion[3] = 1.0f;
        return;
    }
    quaternion[0] /= length;
    quaternion[1] /= length;
    quaternion[2] /= length;
    quaternion[3] /= length;
}

void InvertQuaternion(const float quaternion[4], float outQuaternion[4])
{
    outQuaternion[0] = -quaternion[0];
    outQuaternion[1] = -quaternion[1];
    outQuaternion[2] = -quaternion[2];
    outQuaternion[3] = quaternion[3];
}

void MultiplyQuaternions(const float a[4], const float b[4], float outQuaternion[4])
{
    outQuaternion[0] = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
    outQuaternion[1] = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
    outQuaternion[2] = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
    outQuaternion[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
    NormalizeQuaternion(outQuaternion);
}

void RotateVector(const float quaternion[4], const float vector[3], float outVector[3])
{
    const float x = quaternion[0], y = quaternion[1], z = quaternion[2], w = quaternion[3];
    const float tx = 2.0f * (y * vector[2] - z * vector[1]);
    const float ty = 2.0f * (z * vector[0] - x * vector[2]);
    const float tz = 2.0f * (x * vector[1] - y * vector[0]);
    outVector[0] = vector[0] + w * tx + (y * tz - z * ty);
    outVector[1] = vector[1] + w * ty + (z * tx - x * tz);
    outVector[2] = vector[2] + w * tz + (x * ty - y * tx);
}

bool ReadTransformGroup(const MotionInfo& motion, int frameIndex, int groupIndex,
                        float translation[3], float quaternion[4])
{
    const int base = groupIndex * 7;
    if (!motion.frameChannel || base + 6 >= motion.channelCount)
        return false;

    translation[0] = FrameValue(motion, frameIndex, base + 0);
    translation[1] = FrameValue(motion, frameIndex, base + 1);
    translation[2] = FrameValue(motion, frameIndex, base + 2);
    quaternion[0] = FrameValue(motion, frameIndex, base + 3);
    quaternion[1] = FrameValue(motion, frameIndex, base + 4);
    quaternion[2] = FrameValue(motion, frameIndex, base + 5);
    quaternion[3] = FrameValue(motion, frameIndex, base + 6);
    NormalizeQuaternion(quaternion);
    return true;
}

int FindNearestTransformGroup(const MotionInfo& motion, const float sourcePosition[3])
{
    const int groupCount = motion.channelCount / 7;
    int bestGroup = -1;
    float bestDistanceSquared = FLT_MAX;
    for (int groupIndex = 0; groupIndex < groupCount; ++groupIndex)
    {
        float translation[3] = {};
        float quaternion[4] = {};
        if (!ReadTransformGroup(motion, 0, groupIndex, translation, quaternion))
            continue;

        const float deltaX = sourcePosition[0] - translation[0];
        const float deltaY = sourcePosition[1] - translation[1];
        const float deltaZ = sourcePosition[2] - translation[2];
        const float distanceSquared = deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ;
        if (distanceSquared < bestDistanceSquared)
        {
            bestDistanceSquared = distanceSquared;
            bestGroup = groupIndex;
        }
    }
    return bestGroup;
}
}
