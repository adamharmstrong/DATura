#include "stdafx.h"
#include "ffxi_event_messages.h"
#include "ffxi_resource.h"
#include <fstream>

namespace FFXIEventMessages
{
namespace
{
std::uint32_t U32(const std::vector<std::uint8_t>& b, std::size_t p)
{
    if (p + 4 > b.size()) return 0;
    return static_cast<std::uint32_t>(b[p]) | (static_cast<std::uint32_t>(b[p + 1]) << 8) |
        (static_cast<std::uint32_t>(b[p + 2]) << 16) | (static_cast<std::uint32_t>(b[p + 3]) << 24);
}
}

bool Parse(const std::vector<std::uint8_t>& source, std::vector<Entry>& out)
{
    out.clear();
    if (source.size() < 8) return false;
    std::vector<std::uint8_t> bytes = source;
    if (bytes[3] == 0x10)
        for (std::size_t i = 4; i < bytes.size(); ++i) bytes[i] ^= 0x80;
    const std::uint32_t first = U32(bytes, 4);
    if (first < 8 || first > bytes.size() || (first & 3u) != 0) return false;
    const std::size_t count = first / 4;
    if (count == 0 || 4 + count * 4 > bytes.size()) return false;
    std::vector<std::uint32_t> offsets;
    offsets.reserve(count + 1);
    for (std::size_t i = 0; i < count; ++i)
    {
        const std::uint32_t offset = U32(bytes, 4 + i * 4);
        if (offset < first || offset > bytes.size() - 4) return false;
        offsets.push_back(offset);
    }
    offsets.push_back(static_cast<std::uint32_t>(bytes.size() - 4));
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        if (offsets[i] > offsets[i + 1] || 4ull + offsets[i + 1] > bytes.size()) return false;
        Entry entry;
        for (std::size_t p = 4 + offsets[i]; p < 4 + offsets[i + 1]; ++p)
        {
            const std::uint8_t c = bytes[p];
            if (c == 0) break;
            if (c == 0x07) entry.text.push_back('\n');
            else if (c < 0x20 || c == 0x7f)
            {
                // Event text contains substitution/color controls. The
                // offline viewer has no runtime substitution state, so omit
                // the control and its parameter instead of displaying it.
                if (c != 0x07 && p + 1 < 4 + offsets[i + 1]) ++p;
            }
            else entry.text.push_back(static_cast<char>(c));
        }
        out.push_back(std::move(entry));
    }
    return !out.empty();
}

bool Load(const char* path, std::vector<Entry>& out)
{
    std::vector<std::uint8_t> bytes;
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    bytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return Parse(bytes, out);
}
}
