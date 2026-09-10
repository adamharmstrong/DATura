#pragma once

#include <cmath>
#include <cstring>

namespace ZoneLod
{
struct Data
{
    bool enabled = false;
    unsigned char levelMask = 7; // runtime selection mask, NOT a native DAT flag
    float origin[3] = {};
    float highDistance = 0;
    float midDistance = 0;
    float drawDistance = 0;
};

inline int SuffixOffset(const char* name, int length = 16)
{
    while (length > 0 && (name[length - 1] == ' ' || name[length - 1] == 0)) --length;
    return length >= 2 && name[length - 2] == '_' &&
        (name[length - 1] == 'h' || name[length - 1] == 'm' || name[length - 1] == 'l')
        ? length - 1 : -1;
}

inline int ResolveLevel(int requested, const int available[3])
{
    static const int priority[3][3] = { {0, 1, 2}, {1, 0, 2}, {2, 1, 0} };
    for (int i : priority[requested]) if (available[i] >= 0) return available[i];
    return -1;
}

inline bool SameFamily(const char* first, const char* second)
{
    if (std::memcmp(first, second, 16) == 0) return true;
    const int suffix = SuffixOffset(first);
    return suffix >= 0 && suffix == SuffixOffset(second) &&
        std::memcmp(first, second, suffix) == 0 &&
        std::memcmp(first + suffix + 1, second + suffix + 1, 15 - suffix) == 0;
}

inline bool Visible(const Data& data, const float* viewer)
{
    if (!data.enabled) return true;
    float distance = 0;
    if (viewer)
    {
        for (int axis = 0; axis < 3; ++axis)
        {
            const float delta = viewer[axis] - data.origin[axis];
            distance += delta * delta;
        }
        distance = std::sqrt(distance);
    }
    if (!std::isfinite(distance)) return (data.levelMask & 1) != 0;
    if (std::isfinite(data.drawDistance) && data.drawDistance > 0 && distance > data.drawDistance)
        return false;
    const int level = distance < data.highDistance ? 0 : distance < data.midDistance ? 1 : 2;
    return (data.levelMask & (1 << level)) != 0;
}
}
