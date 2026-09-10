#include "stdafx.h"
#include "ffxi_event_table.h"
#include "ffxi_resource.h"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace FFXIEventTable
{
namespace
{
std::uint32_t U32(const std::vector<std::uint8_t>& b, std::size_t p)
{
    if (p + 4 > b.size()) return 0;
    return static_cast<std::uint32_t>(b[p]) | (static_cast<std::uint32_t>(b[p + 1]) << 8) |
        (static_cast<std::uint32_t>(b[p + 2]) << 16) | (static_cast<std::uint32_t>(b[p + 3]) << 24);
}

std::uint16_t U16(const std::vector<std::uint8_t>& b, std::size_t p)
{
    if (p + 2 > b.size()) return 0;
    return static_cast<std::uint16_t>(b[p] | (b[p + 1] << 8));
}
}

bool Parse(const std::vector<std::uint8_t>& bytes, std::vector<EntityScripts>& out)
{
    out.clear();
    if (bytes.size() < 8) return false;
    // DAT resources carry a four-byte resource-size prefix before the event
    // header. Accept both resolved forms for compatibility with replacements.
    std::size_t base = 0;
    std::uint32_t count = U32(bytes, base);
    if (count == 0 || count > 10000 || base + 4ull + 4ull * count > bytes.size())
    {
        base = 4;
        count = U32(bytes, base);
    }
    if (count == 0 || count > 10000 || base + 4ull + 4ull * count > bytes.size()) return false;
    std::size_t at = base + 4ull + 4ull * count;
    for (std::uint32_t i = 0; i < count; ++i)
    {
        const std::uint32_t length = U32(bytes, base + 4ull + 4ull * i);
        if (length < 4 || at + length > bytes.size()) return false;
        const std::uint32_t entity = U32(bytes, at);
        const std::uint32_t capacity = U32(bytes, at + 4);
        const std::size_t offsetsAt = at + 8;
        const std::size_t idsAt = offsetsAt + capacity * 2ull;
        const std::size_t immediateCountAt = idsAt + capacity * 2ull;
        if (capacity == 0 || capacity > 4000 || immediateCountAt + 4 > at + length)
        { at += length; continue; }
        const std::uint32_t immediateCount = U32(bytes, immediateCountAt);
        const std::size_t codeSizeAt = immediateCountAt + 4ull + immediateCount * 4ull;
        if (codeSizeAt + 4 > at + length) { at += length; continue; }
        const std::uint32_t codeSize = U32(bytes, codeSizeAt);
        const std::size_t codeAt = codeSizeAt + 4;
        if (codeAt + codeSize > at + length) { at += length; continue; }
        EntityScripts scripts;
        scripts.entityId = entity;
        for (std::uint32_t e = 0; e < capacity; ++e)
        {
            const std::uint16_t start = U16(bytes, offsetsAt + e * 2ull);
            const std::uint16_t stop = e + 1 == capacity ? static_cast<std::uint16_t>(codeSize) : U16(bytes, offsetsAt + (e + 1) * 2ull);
            if (start > stop || codeAt + stop > at + length) { scripts.events.clear(); break; }
            Event event;
            event.id = U16(bytes, idsAt + e * 2ull);
            event.references.reserve(immediateCount);
            for (std::uint32_t r = 0; r < immediateCount; ++r)
                event.references.push_back(U32(bytes, immediateCountAt + 4ull + r * 4ull));
            event.code.assign(bytes.begin() + codeAt + start, bytes.begin() + codeAt + stop);
            scripts.events.push_back(std::move(event));
        }
        if (!scripts.events.empty()) out.push_back(std::move(scripts));
        at += length;
    }
    return !out.empty();
}

bool Load(const char* path, std::vector<EntityScripts>& out)
{
    std::vector<std::uint8_t> bytes;
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    bytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return Parse(bytes, out);
}

std::vector<std::uint16_t> MessageIds(const Event& event)
{
    std::vector<std::uint16_t> ids;
    // Event VM message operations are one-byte opcodes. The following two
    // bytes contain either the message index or an immediate-table reference.
    // Event offsets are byte offsets, so an event may begin on an odd byte
    // boundary. Scan both alignments rather than assuming word alignment.
    for (std::size_t p = 0; p + 3 < event.code.size(); ++p)
    {
        const std::uint8_t opcode = event.code[p];
        if (opcode != 0x1d && opcode != 0x2b && opcode != 0x48) continue;
        const std::uint16_t operand = static_cast<std::uint16_t>(event.code[p + 1] | (event.code[p + 2] << 8));
        const std::uint32_t id = (operand & 0x8000u) != 0 &&
            (operand & 0x7fffu) < event.references.size()
            ? event.references[operand & 0x7fffu]
            : operand;
        if (std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
    }
    return ids;
}
}
