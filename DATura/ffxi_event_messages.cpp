#include "stdafx.h"
#include "ffxi_event_messages.h"
#include "ffxi_resource.h"
#include <algorithm>
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

std::string DecodeText(const std::vector<std::uint8_t>& raw)
{
    std::string text;
    for (std::size_t p = 0; p < raw.size(); ++p)
    {
        const std::uint8_t c = raw[p];
        if (c == 0) break;
        if (c == 0x07) text.push_back('\n');
        else if (c < 0x20 || c == 0x7f)
        {
            // Event text contains substitution/color controls. The offline
            // viewer has no runtime substitution state yet, so omit the
            // control and its immediate parameter instead of showing raw tags.
            if (c != 0x07 && p + 1 < raw.size()) ++p;
        }
        else text.push_back(static_cast<char>(c));
    }
    while (!text.empty() && (text.back() == '\r' || text.back() == '\n' || text.back() == ' '))
        text.pop_back();
    return text;
}

bool SplitChoiceText(const Entry& entry, std::string& prompt, std::vector<std::string>& options)
{
    prompt.clear();
    options.clear();
    const auto& raw = entry.raw;
    const auto marker = std::find(raw.begin(), raw.end(), static_cast<std::uint8_t>(0x0b));
    if (marker == raw.end())
        return false;

    std::vector<std::uint8_t> promptBytes(raw.begin(), marker);
    prompt = DecodeText(promptBytes);

    std::size_t start = static_cast<std::size_t>((marker - raw.begin()) + 1);
    while (start < raw.size())
    {
        if (raw[start] == 0 || raw[start] == 0x7f)
            break;
        std::size_t end = start;
        while (end < raw.size() && raw[end] != 0 && raw[end] != 0x07 && raw[end] != 0x7f)
            ++end;
        std::vector<std::uint8_t> optionBytes(raw.begin() + start, raw.begin() + end);
        std::string option = DecodeText(optionBytes);
        if (!option.empty())
            options.push_back(std::move(option));
        if (end >= raw.size() || raw[end] == 0 || raw[end] == 0x7f)
            break;
        start = end + 1;
    }
    return !options.empty();
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
        entry.raw.assign(bytes.begin() + 4 + offsets[i], bytes.begin() + 4 + offsets[i + 1]);
        entry.text = DecodeText(entry.raw);
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
