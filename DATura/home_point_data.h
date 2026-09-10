#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace HomePoint
{
using Vec = std::array<float, 3>;
struct Curve
{
    std::vector<std::pair<float, float>> keys;
    float At(float t, float fallback = 1) const
    {
        if (keys.empty())
            return fallback;
        if (t <= keys.front().first)
            return keys.front().second;
        for (size_t i = 1; i < keys.size(); ++i)
            if (t <= keys[i].first)
            {
                const auto [a, x] = keys[i - 1];
                const auto [b, y] = keys[i];
                return b > a ? x + (y - x) * (t - a) / (b - a) : y;
            }
        return keys.back().second;
    }
};
struct Generator
{
    std::string name, resource, alphaCurve, redCurve, greenCurve;
    std::array<std::string, 3> scaleCurves;
    Vec position = {}, rotation = {}, rotationSpeed = {}, scale = {1, 1, 1};
    Vec velocity = {}, variance = {}, scaleSpeed = {}, scaleAcceleration = {}, acceleration = {};
    std::array<float, 4> color = {1, 1, 1, 1};
    std::array<float, 2> uv = {};
    Vec specularDirection = {0, -1, 0};
    std::array<float, 4> specularColor = {1, 1, 1, 1};
    float specularPower = 10;
    std::string specularTexture;
    int period = 1, life = 120, spriteFrame = 0;
    unsigned int flags = 0, blend = 0x48, type = 0;
    bool alphaAnimated = false, billboard = false, spriteAnimated = false;
};
struct Event
{
    std::string resource;
    int frame = 0, duration = 0;
    bool sound = false;
};
struct Data
{
    std::map<std::string, Generator> generators;
    std::map<std::string, Curve> curves;
    std::map<std::string, std::vector<Event>> schedules;
    std::map<std::string, unsigned int> sounds;
};
inline std::string Tag(const uint8_t *p, size_t n = 4)
{
    std::string s(reinterpret_cast<const char *>(p), n);
    while (!s.empty() && (s.back() == ' ' || s.back() == 0))
        s.pop_back();
    return s;
}
template <class T> inline T Read(const uint8_t *p)
{
    T v;
    std::memcpy(&v, p, sizeof(v));
    return v;
}
inline Vec ReadVec(const uint8_t *p)
{
    return {Read<float>(p), Read<float>(p + 4), Read<float>(p + 8)};
}
inline bool Parse(const std::vector<uint8_t> &bytes, Data &out)
{
    Data result;
    for (size_t pos = 0; pos < bytes.size();)
    {
        if (bytes.size() - pos < 16)
            return false;
        const auto *h = bytes.data() + pos;
        const unsigned int info = Read<uint32_t>(h + 4);
        const size_t length = (info >> 3) & 0x7ffff0;
        const int type = info & 127;
        if (length < 16 || length > bytes.size() - pos)
            return false;
        const auto *p = h + 16;
        const size_t n = length - 16;
        const auto name = Tag(h);
        if (type == 5)
        {
            if (n < 128)
                return false;
            Generator g;
            g.name = name;
            g.period = Read<uint16_t>(p + 0x66) + 1;
            size_t offsets[5];
            for (int i = 0; i < 4; ++i)
            {
                auto v = Read<uint32_t>(p + 0x70 + i * 4);
                if (v < 0x90 || v > n + 16)
                    return false;
                offsets[i] = v - 16;
            }
            offsets[4] = n;
            Vec slots[64] = {};
            for (int stream = 0; stream < 4; ++stream)
            {
                if (offsets[stream] > offsets[stream + 1])
                    return false;
                for (size_t c = offsets[stream]; c < offsets[stream + 1];)
                {
                    if (offsets[stream + 1] - c < 4)
                        return false;
                    auto config = Read<uint32_t>(p + c);
                    int op = config & 255, slot = (config >> 13) & 63;
                    size_t size = std::max(1u, (config >> 8) & 31) * 4;
                    if (size > offsets[stream + 1] - c)
                        return false;
                    const auto *q = p + c;
                    if (stream == 1)
                    {
                        if (op == 1 && size >= 36)
                        {
                            g.flags = Read<uint32_t>(q + 4);
                            g.resource = Tag(q + 12);
                            g.position = ReadVec(q + 20);
                            g.type = q[33];
                            g.life = std::max(1, (int)Read<uint16_t>(q + 34));
                            g.billboard = (g.flags & 1) != 0;
                        }
                        if (op == 9 && size >= 16)
                            g.rotation = ReadVec(q + 4);
                        if (op == 15 && size >= 16)
                            g.scale = ReadVec(q + 4);
                        if ((op == 2 || op == 11 || op == 18) && size >= 16)
                            slots[slot] = ReadVec(q + 4);
                        if (op == 3 && size >= 16)
                            g.variance = ReadVec(q + 4);
                        if (op == 22 && size >= 8)
                        {
                            g.color = {q[6] / 128.f, q[5] / 128.f, q[4] / 128.f, q[7] / 128.f};
                        }
                        if (op == 30 && size >= 8)
                            g.blend = Read<uint16_t>(q + 4);
                        if (op == 29 && size >= 8)
                            g.spriteFrame = Read<uint16_t>(q + 4);
                        if (op == 45 && size >= 12)
                            g.alphaCurve = Tag(q + 8);
                        if (op == 0x55 && size >= 40)
                        {
                            g.specularDirection = ReadVec(q + 4);
                            g.specularTexture = Tag(q + 16);
                            g.specularPower = std::max(1.f, Read<float>(q + 24));
                            g.specularColor = {q[34] / 128.f, q[33] / 128.f, q[32] / 128.f, q[35] / 128.f};
                        }
                        if (op >= 0x27 && op <= 0x29 && size >= 12)
                            g.scaleCurves[op - 0x27] = Tag(q + 8);
                        if (op == 0x2a && size >= 12)
                            g.redCurve = Tag(q + 8);
                        if (op == 0x2b && size >= 12)
                            g.greenCurve = Tag(q + 8);
                    }
                    else if (stream == 2)
                    {
                        if (op == 2)
                            g.velocity = slots[slot];
                        if (op == 5)
                            g.rotationSpeed = slots[slot];
                        if (op == 8)
                            g.scaleSpeed = slots[slot];
                        if (op == 9 && size >= 16)
                            g.scaleAcceleration = ReadVec(q + 4);
                        if (op == 0x26 && size >= 16)
                            g.acceleration = ReadVec(q + 4);
                        if (op == 0x27 && size >= 8)
                            g.uv[0] = Read<float>(q + 4);
                        if (op == 0x28 && size >= 8)
                            g.uv[1] = Read<float>(q + 4);
                        if (op == 0x1b)
                            g.alphaAnimated = true;
                        if (op == 0x0d)
                            g.spriteAnimated = true;
                    }
                    c += size;
                }
            }
            for (auto v : {g.position, g.rotation, g.rotationSpeed, g.scale, g.velocity, g.variance,
                           g.scaleSpeed, g.acceleration})
                for (float x : v)
                    if (!std::isfinite(x) || std::abs(x) > 100000)
                        return false;
            for (float x : g.uv)
                if (!std::isfinite(x) || std::abs(x) > 1000)
                    return false;
            result.generators[name] = g;
        }
        else if (type == 0x19)
        {
            Curve curve;
            for (size_t c = 0; c + 8 <= n; c += 8)
            {
                float t = Read<float>(p + c), v = Read<float>(p + c + 4);
                if (!std::isfinite(t) || !std::isfinite(v))
                    return false;
                if (!curve.keys.empty() && t < curve.keys.back().first)
                    break; // DAT alignment padding
                if (t < 0 || t > 1)
                    return false;
                curve.keys.emplace_back(t, v);
            }
            result.curves[name] = std::move(curve);
        }
        else if (type == 7)
        {
            if (n < 0x20)
                return false;
            auto begin = Read<uint32_t>(p + 0x14), end = Read<uint32_t>(p + 0x18);
            if (begin < 16 || end < 16 || begin > end || end - 16 > n)
                return false;
            for (size_t c = begin - 16; c < end - 16;)
            {
                if (end - 16 - c < 4)
                    return false;
                const int op = p[c];
                size_t size = std::max(1, (int)p[c + 1]) * 4;
                if (size > end - 16 - c)
                    return false;
                if ((op == 2 || op == 10) && size >= 16)
                    result.schedules[name].push_back(
                        {Tag(p + c + 8), Read<uint16_t>(p + c + 4), Read<uint16_t>(p + c + 6), op == 10});
                c += size;
            }
        }
        else if (type == 0x3d && n >= 12 && std::memcmp(p, "SeSep", 5) == 0)
            result.sounds[name] = Read<uint32_t>(p + 8);
        pos += length;
    }
    if (!result.generators.count("bnd0") || !result.schedules.count("aper") ||
        !result.schedules.count("bind"))
        return false;
    out = std::move(result);
    return true;
}
struct Particle
{
    const Generator *generator = nullptr;
    float age = 0;
    int serial = 0;
    bool activation = false;
};
inline std::vector<Particle> Evaluate(const Data &data, double seconds, double activatedAt)
{
    std::vector<Particle> result;
    if (!std::isfinite(seconds) || !std::isfinite(activatedAt))
        return result;
    for (const char *schedule : {"aper", "bind"})
    {
        const bool activation = schedule[0] == 'b';
        const double frame = (activation ? seconds - activatedAt : seconds) * 60;
        if (frame < 0)
            continue;
        const auto events = data.schedules.find(schedule);
        if (events == data.schedules.end())
            continue;
        for (const auto &e : events->second)
        {
            auto it = data.generators.find(e.resource);
            if (e.sound || it == data.generators.end() || it->second.type == 0x3d)
                continue;
            const auto &g = it->second;
            const double elapsed = frame - e.frame;
            if (elapsed < 0)
                continue;
            if (!activation && !g.billboard)
            {
                result.push_back({&g, (float)elapsed, 0, false});
                continue;
            }
            const double last = activation ? std::min(elapsed, (double)std::max(0, e.duration - 1)) : elapsed;
            const double newest = std::floor(last / g.period);
            for (int previous = 0; previous < 256 && previous <= newest; ++previous)
            {
                const double emitted = (newest - previous) * g.period;
                const double age = elapsed - emitted;
                if (age >= g.life)
                    break;
                if (age >= 0)
                    result.push_back(
                        {&g, (float)age, (int)std::fmod(newest - previous, 1000000.0), activation});
            }
        }
    }
    return result;
}
inline float Random(int serial, int axis)
{
    uint32_t x = uint32_t(serial) * 747796405u + uint32_t(axis) * 2891336453u + 277803737u;
    x = (x ^ (x >> 16)) * 2246822519u;
    x ^= x >> 13;
    return (x & 65535) / 32767.5f - 1.f;
}
} // namespace HomePoint
