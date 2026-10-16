#pragma once

#include "ffxi_coordinate_frame.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <algorithm>

// PS2 KO_Rid / KO_RectData: resource 0x36, 64-byte records.
namespace ZoneTriggerVolume
{
using Point = FFXICoordinateFrame::Vector;
struct Volume
{
    Point center, size;
    float yaw;
    std::uint32_t id, target, zone;
};
inline std::uint32_t Word(const unsigned char* p)
{
    std::uint32_t value; std::memcpy(&value, p, 4); return value;
}
inline float Float(const unsigned char* p)
{
    float value; std::memcpy(&value, p, 4); return value;
}
inline std::uint32_t Token(const char* text)
{
    std::uint32_t value = 0;
    for (int i = 0; i < 4 && text[i]; ++i)
        value |= static_cast<std::uint32_t>(static_cast<unsigned char>(text[i])) << (i * 8);
    return value;
}
inline bool Decode(const unsigned char* bytes, std::size_t length, std::vector<Volume>& output,
                   bool includeZeroTarget = false)
{
    output.clear();
    for (std::size_t offset = 0; offset + 16 <= length;)
    {
        const auto info = Word(bytes + offset + 4);
        const std::size_t chunkSize = (info >> 3) & 0x7ffff0;
        if (chunkSize < 16 || chunkSize > length - offset) { output.clear(); return false; }
        if ((info & 0x7f) == 0x36)
        {
            const auto* data = bytes + offset + 16;
            const auto dataSize = chunkSize - 16;
            if (dataSize < 32 || Word(data) != Token("RID")) { output.clear(); return false; }
            const std::size_t table = Word(data + 16);
            if (table > dataSize || dataSize - table < 16) { output.clear(); return false; }
            const std::size_t count = Word(data + table);
            if (count > (dataSize - table - 16) / 64) { output.clear(); return false; }
            for (std::size_t i = 0; i < count; ++i)
            {
                const auto* p = data + table + 16 + i * 64;
                auto id = Word(p + 36);
                // KO_RectData::RotClamp folds all four ID bytes to lowercase.
                for (int shift = 0; shift < 32; shift += 8)
                {
                    const auto c = (id >> shift) & 255;
                    if (c >= 'A' && c <= 'Z') id += 32u << shift;
                }
                if ((id & 255) != 'z' || (!includeZeroTarget && Word(p + 40) == 0)) continue;
                Volume v = {{Float(p), Float(p+4), Float(p+8)},
                    {std::fabs(Float(p+24)), std::fabs(Float(p+28)), std::fabs(Float(p+32))},
                    Float(p+16), id, Word(p+40), Word(p+44)};
                bool valid = std::isfinite(v.yaw);
                for (int axis = 0; axis < 3; ++axis)
                    valid = valid && std::isfinite(v.center[axis]) &&
                        std::isfinite(v.size[axis]) && v.size[axis] > 0;
                if (!valid) { output.clear(); return false; }
                output.push_back(v);
            }
        }
        offset += chunkSize;
    }
    return true;
}
inline Point Local(const Volume& v, const Point& p)
{
    const float x = p[0] - v.center[0], z = p[2] - v.center[2];
    const float c = std::cos(v.yaw), s = std::sin(v.yaw);
    return {(c*x - s*z)/v.size[0], (p[1]-v.center[1])/v.size[1],
            (s*x + c*z)/v.size[2]};
}
inline bool Contains(const Volume& v, const Point& p)
{
    const auto q = Local(v, p);
    return std::fabs(q[0]) < 0.5f && std::fabs(q[1]) < 0.5f && std::fabs(q[2]) < 0.5f;
}
inline bool Hit(const Volume& v, const Point& start, const Point& end)
{
    // Strict local unit-box bounds from KO_RectData::HitCheck. Use a continuous
    // segment test rather than PS2's discrete interpolation loop to avoid
    // skipping narrow volumes during long desktop frames.
    const auto a = Local(v, start), b = Local(v, end);
    float enter = 0, leave = 1;
    for (int axis = 0; axis < 3; ++axis)
    {
        const float d = b[axis] - a[axis];
        if (d == 0) { if (std::fabs(a[axis]) >= 0.5f) return false; continue; }
        float lo = (-0.5f-a[axis])/d, hi = (0.5f-a[axis])/d;
        if (lo > hi) std::swap(lo, hi);
        enter = (std::max)(enter, lo); leave = (std::min)(leave, hi);
        if (enter >= leave) return false;
    }
    return true;
}
}
