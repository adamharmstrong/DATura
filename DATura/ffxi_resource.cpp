#include "stdafx.h"
#include "ffxi_resource.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

// Resource container behavior in this file is based on and modified from
// Windower/POLUtils (copyright 2004-2014 Tim Van Holder, Nevin Stepan, and the
// Windower Team), licensed under Apache-2.0. This DATura port adds explicit
// bounds checks, unified detection, and native presentation data. See
// THIRD_PARTY_NOTICES.md and LICENSE-POLUTILS-APACHE-2.0.txt.

namespace
{
    using ByteVector = std::vector<std::uint8_t>;

    bool HasRange(const ByteVector& bytes, const std::size_t offset, const std::size_t length)
    {
        return offset <= bytes.size() && length <= bytes.size() - offset;
    }

    std::uint16_t ReadU16(const ByteVector& bytes, const std::size_t offset)
    {
        return static_cast<std::uint16_t>(bytes[offset]) |
               static_cast<std::uint16_t>(bytes[offset + 1] << 8);
    }

    std::int16_t ReadS16(const ByteVector& bytes, const std::size_t offset)
    {
        return static_cast<std::int16_t>(ReadU16(bytes, offset));
    }

    std::uint32_t ReadU32(const ByteVector& bytes, const std::size_t offset)
    {
        return static_cast<std::uint32_t>(bytes[offset]) |
               (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
               (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
               (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
    }

    std::int32_t ReadS32(const ByteVector& bytes, const std::size_t offset)
    {
        return static_cast<std::int32_t>(ReadU32(bytes, offset));
    }

    std::uint64_t ReadU64(const ByteVector& bytes, const std::size_t offset)
    {
        return static_cast<std::uint64_t>(ReadU32(bytes, offset)) |
               (static_cast<std::uint64_t>(ReadU32(bytes, offset + 4)) << 32);
    }

    std::wstring HexValue(const std::uint64_t value, const int width)
    {
        std::wostringstream stream;
        stream << L"0x" << std::uppercase << std::hex << std::setw(width)
               << std::setfill(L'0') << value;
        return stream.str();
    }

    std::string EnsureTrailingSlash(std::string path)
    {
        if (!path.empty() && path.back() != '\\' && path.back() != '/')
            path.push_back('\\');
        return path;
    }

    bool FileExists(const std::string& path)
    {
        const DWORD attributes = GetFileAttributesA(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES &&
               (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    bool ReadFileBytes(const char* path, ByteVector& bytes)
    {
        bytes.clear();
        if (!path || !path[0])
            return false;

        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
            return false;

        const std::streamoff length = file.tellg();
        if (length <= 0 || static_cast<std::uint64_t>(length) > 512ull * 1024ull * 1024ull)
            return false;

        bytes.resize(static_cast<std::size_t>(length));
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(bytes.data()), length);
        return file.good() || file.gcount() == length;
    }

    bool ReadByteAt(const std::string& path, const std::uint64_t offset, std::uint8_t& value)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            return false;
        file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        char byte = 0;
        file.read(&byte, 1);
        if (!file)
            return false;
        value = static_cast<std::uint8_t>(byte);
        return true;
    }

    bool ReadU16At(const std::string& path, const std::uint64_t offset, std::uint16_t& value)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            return false;
        file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        std::uint8_t raw[2] = {};
        file.read(reinterpret_cast<char*>(raw), 2);
        if (!file)
            return false;
        value = static_cast<std::uint16_t>(raw[0]) |
                static_cast<std::uint16_t>(raw[1] << 8);
        return true;
    }

    std::wstring DecodeCp932(const std::uint8_t* bytes, const std::size_t size)
    {
        if (!bytes || size == 0)
            return std::wstring();

        const int byteCount = size > static_cast<std::size_t>(std::numeric_limits<int>::max())
            ? std::numeric_limits<int>::max()
            : static_cast<int>(size);
        const int charCount = MultiByteToWideChar(932, 0,
            reinterpret_cast<const char*>(bytes), byteCount, nullptr, 0);
        if (charCount > 0)
        {
            std::wstring result(static_cast<std::size_t>(charCount), L'\0');
            MultiByteToWideChar(932, 0, reinterpret_cast<const char*>(bytes), byteCount,
                                result.data(), charCount);
            return result;
        }

        std::wstring result;
        result.reserve(size);
        for (std::size_t i = 0; i < size; ++i)
        {
            if (bytes[i] >= 0x20 && bytes[i] < 0x7f)
                result.push_back(static_cast<wchar_t>(bytes[i]));
            else
            {
                std::wostringstream escaped;
                escaped << L"<" << std::uppercase << std::hex << std::setw(2)
                        << std::setfill(L'0') << static_cast<unsigned int>(bytes[i]) << L">";
                result += escaped.str();
            }
        }
        return result;
    }

    void AppendDecodedRun(std::wstring& output, const ByteVector& run)
    {
        if (!run.empty())
            output += DecodeCp932(run.data(), run.size());
    }

    std::wstring DecodeDialogPayload(const ByteVector& bytes)
    {
        std::wstring output;
        ByteVector run;
        const auto flush = [&]()
        {
            AppendDecodedRun(output, run);
            run.clear();
        };
        const auto parameterToken = [&](const wchar_t* label, const unsigned int parameter)
        {
            flush();
            std::wostringstream token;
            token << L"<" << label << L" " << parameter << L">";
            output += token.str();
        };

        for (std::size_t i = 0; i < bytes.size(); ++i)
        {
            const std::uint8_t code = bytes[i];
            if (code == 0)
                break;
            if (code == 0x07)
            {
                flush();
                output += L"\r\n";
            }
            else if (code == 0x08)
            {
                flush(); output += L"<Player Name>";
            }
            else if (code == 0x09)
            {
                flush(); output += L"<Speaker Name>";
            }
            else if (code == 0x0a && i + 1 < bytes.size())
            {
                parameterToken(L"Numeric Parameter", bytes[++i]);
            }
            else if (code == 0x0b)
            {
                flush(); output += L"<Selection Dialog>";
            }
            else if (code == 0x0c && i + 1 < bytes.size())
            {
                parameterToken(L"Multiple Choice Parameter", bytes[++i]);
            }
            else if (code == 0x19 && i + 1 < bytes.size())
            {
                parameterToken(L"Item Parameter", bytes[++i]);
            }
            else if (code == 0x1a && i + 1 < bytes.size())
            {
                parameterToken(L"Key Item Parameter", bytes[++i]);
            }
            else if (code == 0x1c && i + 1 < bytes.size())
            {
                parameterToken(L"Player/Chocobo Parameter", bytes[++i]);
            }
            else if (code == 0x1e && i + 1 < bytes.size())
            {
                parameterToken(L"Color", bytes[++i]);
            }
            else if (code == 0x7f && i + 1 < bytes.size())
            {
                flush();
                const std::uint8_t type = bytes[++i];
                if (type == 0x31 && i + 1 < bytes.size())
                {
                    const std::uint8_t seconds = bytes[++i];
                    output += seconds ? L"<Prompt Delay " + std::to_wstring(seconds) + L"s>" : L"<Prompt>";
                }
                else if (type == 0x85)
                {
                    output += L"<Player Gender Choice>";
                }
                else if ((type == 0x8d || type == 0x8e || type == 0x92 || type == 0xb1) && i + 1 < bytes.size())
                {
                    const std::uint8_t parameter = bytes[++i];
                    const wchar_t* label = type == 0x8d ? L"Weather Event" :
                                           type == 0x8e ? L"Weather Type" :
                                           type == 0x92 ? L"Singular/Plural" : L"Title";
                    output += L"<" + std::wstring(label) + L" Parameter " +
                              std::to_wstring(parameter) + L">";
                }
                else if (i + 1 < bytes.size())
                {
                    const std::uint8_t parameter = bytes[++i];
                    output += L"<Unknown Parameter " + HexValue(type, 2) + L" " +
                              std::to_wstring(parameter) + L">";
                }
                else
                {
                    output += L"<Unknown Marker " + HexValue(type, 2) + L">";
                }
            }
            else if (code < 0x20 || code == 0x7f)
            {
                flush();
                output += L"<Control " + HexValue(code, 2) + L">";
            }
            else
            {
                run.push_back(code);
            }
        }
        flush();
        return output;
    }

    bool StartsWith(const ByteVector& bytes, const std::size_t offset, const char* marker, const std::size_t length)
    {
        return HasRange(bytes, offset, length) && std::memcmp(bytes.data() + offset, marker, length) == 0;
    }

    bool ParseDialog(const ByteVector& bytes, FFXIResource::ParseResult& result)
    {
        if (bytes.size() < 8)
            return false;
        if (ReadU32(bytes, 0) != 0x10000000u + static_cast<std::uint32_t>(bytes.size() - 4))
            return false;

        const std::uint32_t firstText = ReadU32(bytes, 4) ^ 0x80808080u;
        if (firstText < 8 || firstText > bytes.size() - 4 || (firstText & 3u) != 0)
            return false;
        const std::uint32_t entryCount = firstText / 4;
        if (entryCount == 0 || !HasRange(bytes, 4, static_cast<std::size_t>(entryCount) * 4))
            return false;

        std::vector<std::uint32_t> offsets;
        offsets.reserve(static_cast<std::size_t>(entryCount) + 1);
        for (std::uint32_t i = 0; i < entryCount; ++i)
        {
            const std::uint32_t offset = ReadU32(bytes, 4 + static_cast<std::size_t>(i) * 4) ^ 0x80808080u;
            if (offset < entryCount * 4 || offset > bytes.size() - 4)
                return false;
            offsets.push_back(offset);
        }
        offsets.push_back(static_cast<std::uint32_t>(bytes.size() - 4));
        std::sort(offsets.begin(), offsets.end());

        result = {};
        result.kind = FFXIResource::FileKind::Dialog;
        result.format = L"FFXI dialog table";
        result.rows.reserve(entryCount);
        for (std::uint32_t i = 0; i < entryCount; ++i)
        {
            if (offsets[i] > offsets[i + 1] || !HasRange(bytes, 4 + offsets[i], offsets[i + 1] - offsets[i]))
                return false;
            ByteVector payload(bytes.begin() + 4 + offsets[i], bytes.begin() + 4 + offsets[i + 1]);
            for (std::uint8_t& byte : payload)
                byte ^= 0x80;
            FFXIResource::Row row;
            row.kind = L"Dialog";
            row.id = std::to_wstring(i);
            row.text = DecodeDialogPayload(payload);
            row.details = L"Offset " + HexValue(offsets[i], 8);
            result.rows.push_back(std::move(row));
        }
        return true;
    }

    bool ParseMobList(const ByteVector& bytes, FFXIResource::ParseResult& result)
    {
        if (bytes.size() < 0x20 || (bytes.size() % 0x20) != 0)
            return false;

        const std::size_t entryCount = bytes.size() / 0x20;
        const std::wstring firstName = FFXIResource::DecodeText(bytes.data(), 0x1c);
        if (ReadU32(bytes, 0x1c) != 0 || _wcsicmp(firstName.c_str(), L"none") != 0)
            return false;

        result = {};
        result.kind = FFXIResource::FileKind::MobList;
        result.format = L"FFXI NPC/mob list (32-byte records)";
        result.rows.reserve(entryCount > 0 ? entryCount - 1 : 0);
        int expectedZone = -1;
        for (std::size_t i = 1; i < entryCount; ++i)
        {
            const std::size_t offset = i * 0x20;
            const std::uint32_t id = ReadU32(bytes, offset + 0x1c);
            const std::uint32_t family = id & 0xfff00000u;
            if (id != 0 && family != 0x01000000u && family != 0x01100000u && family != 0x01300000u)
                return false;
            const int zone = static_cast<int>((id >> 12) & 0xffu);
            if (id != 0 && expectedZone < 0)
                expectedZone = zone;
            else if (id != 0 && expectedZone != zone)
                return false;

            FFXIResource::Row row;
            row.kind = L"NPC/Mob";
            row.id = HexValue(id, 8);
            row.text = FFXIResource::DecodeText(bytes.data() + offset, 0x1c);
            row.details = L"Zone " + std::to_wstring(zone) + L", entity " +
                          std::to_wstring(id & 0xfffu);
            result.rows.push_back(std::move(row));
        }
        return true;
    }

    bool ParseDmsgStringBlock(const ByteVector& block, const std::size_t outerIndex,
                              std::vector<FFXIResource::Row>& rows)
    {
        if (block.size() < 4)
            return false;
        std::int32_t count = ReadS32(block, 0);
        std::uint32_t mask = 0;
        if (count < 0 || count > 100)
        {
            count = ~count;
            mask = 0xffffffffu;
        }
        if (count < 0 || count > 100 || !HasRange(block, 4, static_cast<std::size_t>(count) * 8))
            return false;

        for (std::int32_t i = 0; i < count; ++i)
        {
            const std::size_t pair = 4 + static_cast<std::size_t>(i) * 8;
            const std::uint32_t stringOffset = ReadU32(block, pair) ^ mask;
            const std::uint32_t flags = ReadU32(block, pair + 4) ^ mask;
            const std::size_t textOffset = 28ull + stringOffset;
            if ((flags != 0 && flags != 1) || !HasRange(block, textOffset, 4))
                return false;

            ByteVector textBytes;
            std::size_t cursor = textOffset;
            bool terminated = false;
            while (HasRange(block, cursor, 4))
            {
                std::uint8_t four[4] = {};
                for (int j = 0; j < 4; ++j)
                    four[j] = block[cursor + j] ^ static_cast<std::uint8_t>(mask);
                textBytes.insert(textBytes.end(), four, four + 4);
                cursor += 4;
                if (four[3] == 0)
                {
                    terminated = true;
                    break;
                }
            }
            if (!terminated)
                return false;

            FFXIResource::Row row;
            row.kind = L"String";
            row.id = count == 1 ? std::to_wstring(outerIndex) :
                     std::to_wstring(outerIndex) + L"." + std::to_wstring(i);
            row.text = FFXIResource::DecodeText(textBytes.data(), textBytes.size());
            row.details = L"Flags " + HexValue(flags, 8);
            rows.push_back(std::move(row));
        }
        return true;
    }

    bool HasDmsgMagic(const ByteVector& bytes)
    {
        static const std::uint8_t magic[8] = { 'd', '_', 'm', 's', 'g', 0, 0, 0 };
        return bytes.size() >= 8 && std::memcmp(bytes.data(), magic, 8) == 0;
    }

    bool ParseDmsg(const ByteVector& bytes, FFXIResource::ParseResult& result)
    {
        if (bytes.size() < 0x38 || !HasDmsgMagic(bytes))
            return false;

        result.warning = L"A d_msg signature was found, but its header or records did not match a supported variant.";

        // Original 0x38-header variant with 36-byte entry descriptors.
        if (HasRange(bytes, 0, 0x38) && ReadU16(bytes, 8) == 1 && ReadU32(bytes, 10) == 0 &&
            ReadU16(bytes, 14) == 2 && ReadU32(bytes, 16) == 3)
        {
            const std::uint32_t entryCount = ReadU32(bytes, 20);
            const std::uint32_t fileSize = ReadU32(bytes, 28);
            const std::uint32_t headerSize = ReadU32(bytes, 32);
            const std::uint32_t entryBytes = ReadU32(bytes, 36);
            const std::uint32_t dataBytes = ReadU32(bytes, 40);
            if (ReadU32(bytes, 24) != 1 || fileSize != bytes.size() || headerSize != 0x38 ||
                entryBytes != entryCount * 36ull ||
                static_cast<std::uint64_t>(headerSize) + entryBytes + dataBytes != bytes.size())
                return false;

            result = {};
            result.kind = FFXIResource::FileKind::Dmsg;
            result.format = L"d_msg string table v1";
            result.rows.reserve(entryCount);
            for (std::uint32_t i = 0; i < entryCount; ++i)
            {
                const std::size_t descriptor = 0x38ull + static_cast<std::size_t>(i) * 36;
                const std::uint32_t offset = ReadU32(bytes, descriptor);
                const std::int16_t length = ReadS16(bytes, descriptor + 8);
                if (length < 0 || static_cast<std::uint64_t>(offset) + length > dataBytes)
                    return false;
                const std::size_t textOffset = 0x38ull + entryBytes + offset;
                FFXIResource::Row row;
                row.kind = L"String";
                row.id = std::to_wstring(i);
                row.text = FFXIResource::DecodeText(bytes.data() + textOffset, static_cast<std::size_t>(length));
                row.details = L"Length " + std::to_wstring(length);
                result.rows.push_back(std::move(row));
            }
            return true;
        }

        if (bytes.size() < 0x40 || ReadU32(bytes, 12) != 3 || ReadU32(bytes, 16) != 3 ||
            ReadU32(bytes, 20) != bytes.size() || ReadU32(bytes, 24) != 0x40)
            return false;

        const std::uint32_t dataBytes = ReadU32(bytes, 36);
        const std::uint32_t entryCount = ReadU32(bytes, 40);
        if (ReadU32(bytes, 44) != 1 || ReadU64(bytes, 48) != 0 || ReadU64(bytes, 56) != 0)
            return false;

        // Offset-table variant.
        if (ReadU16(bytes, 8) == 1 && ReadU16(bytes, 10) == 1 && ReadU32(bytes, 32) == 0)
        {
            const std::uint32_t entryBytes = ReadU32(bytes, 28);
            if (ReadU32(bytes, 32) != 0 || entryBytes != entryCount * 8ull ||
                0x40ull + entryBytes + dataBytes != bytes.size())
                return false;
            result = {};
            result.kind = FFXIResource::FileKind::Dmsg;
            result.format = L"d_msg string table v2";
            for (std::uint32_t i = 0; i < entryCount; ++i)
            {
                const std::size_t descriptor = 0x40ull + static_cast<std::size_t>(i) * 8;
                const std::int32_t offset = static_cast<std::int32_t>(~ReadU32(bytes, descriptor));
                const std::int32_t length = static_cast<std::int32_t>(~ReadU32(bytes, descriptor + 4));
                if (offset < 0 || length < 0 || static_cast<std::uint64_t>(offset) + length > dataBytes)
                    return false;
                const std::size_t blockStart = 0x40ull + entryBytes + static_cast<std::uint32_t>(offset);
                ByteVector block(bytes.begin() + blockStart, bytes.begin() + blockStart + length);
                if (!ParseDmsgStringBlock(block, i, result.rows))
                    return false;
            }
            return true;
        }

        // Fixed-record variant.
        if ((ReadU16(bytes, 8) == 0 || ReadU16(bytes, 8) == 1) &&
            (ReadU16(bytes, 10) == 0 || ReadU16(bytes, 10) == 1) && ReadU32(bytes, 28) == 0)
        {
            const std::int32_t bytesPerEntry = ReadS32(bytes, 32);
            if (bytesPerEntry <= 0 || dataBytes % static_cast<std::uint32_t>(bytesPerEntry) != 0 ||
                static_cast<std::uint64_t>(entryCount) * bytesPerEntry != dataBytes ||
                0x40ull + dataBytes != bytes.size())
                return false;
            result = {};
            result.kind = FFXIResource::FileKind::Dmsg;
            result.format = L"d_msg string table v3";
            for (std::uint32_t i = 0; i < entryCount; ++i)
            {
                const std::size_t blockStart = 0x40ull + static_cast<std::size_t>(i) * bytesPerEntry;
                ByteVector block(bytes.begin() + blockStart, bytes.begin() + blockStart + bytesPerEntry);
                if (!ParseDmsgStringBlock(block, i, result.rows))
                {
                    result.warning = L"The d_msg header was recognized, but fixed record " +
                                     std::to_wstring(i) + L" was malformed.";
                    return false;
                }
            }
            return true;
        }
        return false;
    }

    bool ParseXiString(const ByteVector& bytes, FFXIResource::ParseResult& result)
    {
        static const std::uint8_t marker[10] = { 'X', 'I', 'S', 'T', 'R', 'I', 'N', 'G', 0, 0 };
        if (bytes.size() < 0x38 || std::memcmp(bytes.data(), marker, 10) != 0 || ReadU16(bytes, 10) != 2)
            return false;
        for (std::size_t i = 12; i < 32; ++i)
            if (bytes[i] != 0)
                return false;

        const std::uint32_t fileSize = ReadU32(bytes, 32);
        const std::uint32_t entryCount = ReadU32(bytes, 36);
        const std::uint32_t entryBytes = ReadU32(bytes, 40);
        const std::uint32_t dataBytes = ReadU32(bytes, 44);
        if (fileSize != bytes.size() || entryBytes != entryCount * 12ull ||
            0x38ull + entryBytes + dataBytes != bytes.size())
            return false;

        result = {};
        result.kind = FFXIResource::FileKind::XiString;
        result.format = L"XISTRING table";
        result.rows.reserve(entryCount);
        for (std::uint32_t i = 0; i < entryCount; ++i)
        {
            const std::size_t descriptor = 0x38ull + static_cast<std::size_t>(i) * 12;
            const std::uint32_t offset = ReadU32(bytes, descriptor);
            const std::int16_t length = ReadS16(bytes, descriptor + 4);
            if (length < 0 || static_cast<std::uint64_t>(offset) + length > dataBytes)
                return false;
            FFXIResource::Row row;
            row.kind = L"String";
            row.id = std::to_wstring(i);
            row.text = FFXIResource::DecodeText(bytes.data() + 0x38 + entryBytes + offset,
                                                static_cast<std::size_t>(length));
            row.details = L"Flags " + HexValue(ReadU16(bytes, descriptor + 6), 4);
            result.rows.push_back(std::move(row));
        }
        return true;
    }

    bool ParseSimpleString(const ByteVector& bytes, FFXIResource::ParseResult& result)
    {
        if (bytes.empty() || (bytes.size() % 0x40) != 0)
            return false;
        const std::size_t entryCount = bytes.size() / 0x40;
        for (std::size_t i = 0; i < entryCount; ++i)
            if (bytes[i * 0x40 + 0x3f] != 0xff)
                return false;

        result = {};
        result.kind = FFXIResource::FileKind::SimpleString;
        result.format = L"Simple 64-byte string table";
        result.rows.reserve(entryCount);
        for (std::size_t i = 0; i < entryCount; ++i)
        {
            const std::size_t offset = i * 0x40;
            ByteVector payload(bytes.begin() + offset + 4, bytes.begin() + offset + 0x3f);
            FFXIResource::Row row;
            row.kind = L"String";
            row.id = HexValue(ReadU32(bytes, offset), 8);
            row.text = DecodeDialogPayload(payload);
            result.rows.push_back(std::move(row));
        }
        return true;
    }

    enum class ItemLayout
    {
        Unknown, Armor, Currency, Item, Puppet, Usable, Weapon, Slip, Instinct, Monipulator
    };

    ItemLayout DeduceItemLayout(const std::uint32_t id)
    {
        if (id == 0xffff) return ItemLayout::Currency;
        if (id < 0x1000) return ItemLayout::Item;
        if (id < 0x2000) return ItemLayout::Usable;
        if (id < 0x2200) return ItemLayout::Puppet;
        if (id < 0x2800) return ItemLayout::Item;
        if (id < 0x4000) return ItemLayout::Armor;
        if (id < 0x5a00) return ItemLayout::Weapon;
        if (id < 0x7000) return ItemLayout::Armor;
        if (id < 0x7400) return ItemLayout::Slip;
        if (id < 0x7800) return ItemLayout::Instinct;
        if (id < 0xf200) return ItemLayout::Monipulator;
        return ItemLayout::Unknown;
    }

    const wchar_t* ItemTypeName(const std::uint16_t type)
    {
        static const wchar_t* names[] =
        {
            L"Nothing", L"Item", L"Quest item", L"Fish", L"Weapon", L"Armor", L"Linkshell",
            L"Usable item", L"Crystal", L"Currency", L"Furnishing", L"Plant", L"Flowerpot",
            L"Puppet item", L"Mannequin", L"Book", L"Racing form", L"Betting slip",
            L"Soul plate", L"Reflector", L"Type 20", L"Lottery ticket", L"Maze Tabula M",
            L"Maze Tabula R", L"Maze voucher", L"Maze rune", L"Type 26", L"Storage slip"
        };
        return type < sizeof(names) / sizeof(names[0]) ? names[type] : L"Unknown";
    }

    bool ParseItemData(const ByteVector& bytes, FFXIResource::ParseResult& result)
    {
        if (bytes.size() < 0xc000 || (bytes.size() % 0xc00) != 0)
            return false;

        result = {};
        result.kind = FFXIResource::FileKind::ItemData;
        result.format = L"FFXI item resource records";
        const std::size_t recordCount = bytes.size() / 0xc00;
        bool sawPhysicalRecord = false;
        for (std::size_t recordIndex = 0; recordIndex < recordCount; ++recordIndex)
        {
            ByteVector record(0xc00);
            bool allZero = true;
            for (std::size_t i = 0; i < record.size(); ++i)
            {
                const std::uint8_t encrypted = bytes[recordIndex * 0xc00 + i];
                if (encrypted != 0)
                    allZero = false;
                record[i] = static_cast<std::uint8_t>((encrypted >> 5) | (encrypted << 3));
            }
            if (allZero)
                continue;

            const std::uint32_t id = ReadU32(record, 0);
            const ItemLayout layout = DeduceItemLayout(id);
            if (layout == ItemLayout::Unknown)
                return false;

            const std::int32_t graphicSize = ReadS32(record, 0x280);
            if (graphicSize <= 0 || graphicSize > 0x97c ||
                (record[0x284] != 0x91 && record[0x284] != 0xa1 && record[0x284] != 0xb1))
                return false;
            sawPhysicalRecord = true;

            const std::uint16_t flags = ReadU16(record, 4);
            const std::uint16_t stackSize = ReadU16(record, 6);
            const std::uint16_t semanticType = ReadU16(record, 8);
            const std::uint16_t resourceId = ReadU16(record, 10);
            const std::uint16_t validTargets = ReadU16(record, 12);
            std::size_t stringBase = 0;
            std::wostringstream details;
            details << ItemTypeName(semanticType) << L", stack " << stackSize
                    << L", flags " << HexValue(flags, 4)
                    << L", resource " << HexValue(resourceId, 4)
                    << L", targets " << HexValue(validTargets, 4);

            if (layout == ItemLayout::Armor)
            {
                stringBase = 0x2c;
                details << L", level " << ReadU16(record, 0x0e)
                        << L", slots " << HexValue(ReadU16(record, 0x10), 4)
                        << L", jobs " << HexValue(ReadU32(record, 0x14), 8)
                        << L", item level " << static_cast<unsigned int>(record[0x26]);
            }
            else if (layout == ItemLayout::Weapon)
            {
                stringBase = 0x38;
                details << L", level " << ReadU16(record, 0x0e)
                        << L", damage " << ReadU16(record, 0x1c)
                        << L", delay " << ReadS16(record, 0x1e)
                        << L", skill " << static_cast<unsigned int>(record[0x22])
                        << L", item level " << static_cast<unsigned int>(record[0x32]);
            }
            else if (layout == ItemLayout::Puppet || layout == ItemLayout::Item)
                stringBase = 0x18;
            else if (layout == ItemLayout::Usable)
                stringBase = 0x1c;
            else if (layout == ItemLayout::Currency)
                stringBase = 0x10;
            else if (layout == ItemLayout::Slip)
                stringBase = 0x54;
            else if (layout == ItemLayout::Instinct)
                stringBase = 0x28;
            else if (layout == ItemLayout::Monipulator)
                stringBase = 0x70;

            if (!HasRange(record, stringBase, 4))
                return false;
            const std::uint32_t stringCount = ReadU32(record, stringBase);
            if (stringCount > 9 || !HasRange(record, stringBase + 4, stringCount * 8ull))
                return false;

            std::vector<std::wstring> strings(stringCount);
            for (std::uint32_t i = 0; i < stringCount; ++i)
            {
                const std::size_t pair = stringBase + 4 + static_cast<std::size_t>(i) * 8;
                const std::uint32_t relativeOffset = ReadU32(record, pair);
                const std::uint32_t stringFlags = ReadU32(record, pair + 4);
                if (stringFlags != 0 && stringFlags != 1)
                    return false;
                if (stringFlags == 1)
                    continue;
                const std::size_t block = stringBase + relativeOffset;
                if (block + 28 > 0x280 || ReadU32(record, block) != 1)
                    return false;
                for (std::size_t zero = 1; zero <= 6; ++zero)
                    if (ReadU32(record, block + zero * 4) != 0)
                        return false;
                std::size_t end = block + 28;
                while (end < 0x280 && record[end] != 0)
                    ++end;
                strings[i] = FFXIResource::DecodeText(record.data() + block + 28, end - (block + 28));
            }

            FFXIResource::Row row;
            row.kind = L"Item";
            row.id = HexValue(id, 4) + L" (" + std::to_wstring(id) + L")";
            row.text = strings.empty() ? L"" : strings[0];
            if (stringCount == 2) row.details = strings[1];
            else if (stringCount == 5) row.details = strings[4];
            else if (stringCount == 6) row.details = strings[5];
            else if (stringCount == 9) row.details = strings[8];
            if (!row.details.empty())
                row.details += L" | ";
            row.details += details.str();
            result.rows.push_back(std::move(row));
        }
        return sawPhysicalRecord && !result.rows.empty();
    }

    bool ParseAudio(const ByteVector& bytes, FFXIResource::ParseResult& result)
    {
        const bool bgm = bytes.size() >= 0x30 && StartsWith(bytes, 0, "BGMStream\0\0\0", 12);
        const bool soundEffect = bytes.size() >= 0x30 && StartsWith(bytes, 0, "SeWave\0\0", 8);
        if (!bgm && !soundEffect)
            return false;

        const std::size_t base = bgm ? 12 : 8;
        const std::int32_t size = bgm ? ReadS32(bytes, base + 4) : ReadS32(bytes, base);
        const std::int32_t sampleFormat = bgm ? ReadS32(bytes, base) : ReadS32(bytes, base + 4);
        const std::size_t fields = base + 8;
        const std::int32_t id = ReadS32(bytes, fields);
        const std::int32_t sampleBlocks = ReadS32(bytes, fields + 4);
        const std::int32_t loopStart = ReadS32(bytes, fields + 8);
        const std::int32_t sampleRateHigh = ReadS32(bytes, fields + 12);
        const std::int32_t sampleRateLow = ReadS32(bytes, fields + 16);
        const std::uint8_t channels = bytes[fields + 26];
        const std::uint8_t blockSize = bytes[fields + 27];
        const wchar_t* formatName = sampleFormat == 0 ? L"ADPCM" : sampleFormat == 1 ? L"PCM" :
                                    sampleFormat == 3 ? L"ATRAC3" : L"Unknown";

        result = {};
        result.kind = FFXIResource::FileKind::Audio;
        result.format = bgm ? L"BGMStream audio" : L"SeWave sound effect";
        const auto add = [&](const wchar_t* field, const std::wstring& value, const std::wstring& detail = L"")
        {
            FFXIResource::Row row;
            row.kind = L"Audio Header";
            row.id = field;
            row.text = value;
            row.details = detail;
            result.rows.push_back(std::move(row));
        };
        add(L"Format", formatName, std::to_wstring(sampleFormat));
        add(L"Size", std::to_wstring(size));
        add(L"ID", std::to_wstring(id));
        add(L"Sample blocks", std::to_wstring(sampleBlocks));
        add(L"Loop start", std::to_wstring(loopStart), loopStart < 0 ? L"Not looped" : L"");
        const std::int64_t sampleRate = static_cast<std::int64_t>(sampleRateHigh) + sampleRateLow;
        add(L"Sample rate", std::to_wstring(sampleRate));
        add(L"Channels", std::to_wstring(channels));
        add(L"Block size", std::to_wstring(blockSize));
        if (sampleRate > 0 && sampleBlocks >= 0)
        {
            const double duration = sampleFormat == 0
                ? static_cast<double>(sampleBlocks) * blockSize / sampleRate
                : static_cast<double>(sampleBlocks) / sampleRate;
            std::wostringstream durationText;
            durationText << std::fixed << std::setprecision(3) << duration << L" seconds";
            add(L"Duration", durationText.str());
        }
        if (sampleFormat == 0 && blockSize != 0)
        {
            const std::uint64_t compressed = (1ull + blockSize / 2ull) * channels;
            const std::uint64_t decoded = static_cast<std::uint64_t>(blockSize) * channels * 2ull;
            add(L"ADPCM bytes/block", std::to_wstring(compressed),
                L"Decodes to " + std::to_wstring(decoded) + L" PCM bytes");
        }
        return true;
    }

    bool IsGraphicString(const ByteVector& bytes, const std::size_t offset)
    {
        bool hasCharacter = false;
        for (std::size_t i = 0; i < 8; ++i)
        {
            const std::uint8_t value = bytes[offset + i];
            if (value == 0 || value == ' ')
                continue;
            if (value < 0x20 || value >= 0x7f)
                return false;
            hasCharacter = true;
        }
        return hasCharacter;
    }

    bool ScanImages(const ByteVector& bytes, FFXIResource::ParseResult& result)
    {
        std::vector<FFXIResource::Row> rows;
        for (std::size_t offset = 0; offset + 57 <= bytes.size() && rows.size() < 4096; ++offset)
        {
            const std::uint8_t flag = bytes[offset];
            if (flag != 0x91 && flag != 0xa1 && flag != 0xb1)
                continue;
            if (!IsGraphicString(bytes, offset + 1) || !IsGraphicString(bytes, offset + 9) ||
                ReadU32(bytes, offset + 17) != 40)
                continue;
            const std::int32_t width = ReadS32(bytes, offset + 21);
            const std::int32_t height = ReadS32(bytes, offset + 25);
            const std::uint16_t planes = ReadU16(bytes, offset + 29);
            const std::uint16_t bits = ReadU16(bytes, offset + 31);
            if (width <= 0 || height <= 0 || width > 16384 || height > 16384 || planes != 1)
                continue;
            if (flag != 0xa1 && bits != 8 && bits != 16 && bits != 24 && bits != 32)
                continue;

            FFXIResource::Row row;
            row.kind = L"Embedded Image";
            row.id = HexValue(offset, 8);
            row.text = FFXIResource::DecodeText(bytes.data() + offset + 1, 8) + L" / " +
                       FFXIResource::DecodeText(bytes.data() + offset + 9, 8);
            std::wostringstream details;
            details << width << L"x" << height << L", " << bits << L" bpp, flag " << HexValue(flag, 2)
                    << L", compression " << HexValue(ReadU32(bytes, offset + 33), 8)
                    << L", header image size " << ReadU32(bytes, offset + 37) << L" bytes";
            row.details = details.str();
            rows.push_back(std::move(row));
            offset += 56;
        }
        if (rows.empty())
            return false;
        result = {};
        result.kind = FFXIResource::FileKind::EmbeddedImages;
        result.format = L"Embedded FFXI graphics";
        result.rows = std::move(rows);
        return true;
    }
}

namespace FFXIResource
{
    bool ResolveFileId(const char* ffxiRoot, const int fileId, ResolvedFile& result)
    {
        result = {};
        if (!ffxiRoot || !ffxiRoot[0] || fileId < 0)
            return false;

        const std::string root = EnsureTrailingSlash(ffxiRoot);
        for (int tableIndex = 1; tableIndex < 20; ++tableIndex)
        {
            const std::string suffix = tableIndex == 1 ? std::string() : std::to_string(tableIndex);
            const std::string tableDirectory = tableIndex == 1 ? root : root + "ROM" + suffix + "\\";
            const std::string vtable = tableDirectory + "VTABLE" + suffix + ".DAT";
            const std::string ftable = tableDirectory + "FTABLE" + suffix + ".DAT";
            if (!FileExists(vtable) || !FileExists(ftable))
                continue;

            std::uint8_t hive = 0;
            if (!ReadByteAt(vtable, static_cast<std::uint64_t>(fileId), hive) || hive != tableIndex)
                continue;
            std::uint16_t packed = 0;
            if (!ReadU16At(ftable, static_cast<std::uint64_t>(fileId) * 2ull, packed))
                continue;

            const int directory = packed / 0x80;
            const int file = packed % 0x80;
            const std::string rom = hive == 1 ? "ROM" : "ROM" + std::to_string(hive);
            const std::string relative = rom + "/" + std::to_string(directory) + "/" +
                                         std::to_string(file) + ".DAT";
            std::string full = root + relative;
            std::replace(full.begin(), full.end(), '/', '\\');
            if (!FileExists(full))
                continue;

            result.fileId = fileId;
            result.hive = hive;
            result.packedLocation = packed;
            result.relativePath = relative;
            result.fullPath = full;
            return true;
        }
        return false;
    }

    std::wstring DecodeText(const std::uint8_t* bytes, const std::size_t size)
    {
        if (!bytes || size == 0)
            return std::wstring();

        static const wchar_t* elements[] =
        {
            L"Fire", L"Ice", L"Air", L"Earth", L"Thunder", L"Water", L"Light", L"Dark"
        };
        std::wstring output;
        ByteVector run;
        const auto flush = [&]()
        {
            AppendDecodedRun(output, run);
            run.clear();
        };

        for (std::size_t i = 0; i < size; ++i)
        {
            const std::uint8_t value = bytes[i];
            if (value == 0)
                break;
            if (value == 0xef && i + 1 < size && bytes[i + 1] >= 0x1f && bytes[i + 1] <= 0x26)
            {
                flush();
                output += L"<Element: " + std::wstring(elements[bytes[++i] - 0x1f]) + L">";
            }
            else if (value == 0xef && i + 1 < size && (bytes[i + 1] == 0x27 || bytes[i + 1] == 0x28))
            {
                flush();
                output += bytes[++i] == 0x27 ? L"<AutoTrans Start>" : L"<AutoTrans End>";
            }
            else if (value == 0xfd && i + 5 < size && bytes[i + 5] == 0xfd)
            {
                flush();
                const std::uint32_t resourceId = (static_cast<std::uint32_t>(bytes[i + 1]) << 24) |
                                                 (static_cast<std::uint32_t>(bytes[i + 2]) << 16) |
                                                 (static_cast<std::uint32_t>(bytes[i + 3]) << 8) |
                                                  static_cast<std::uint32_t>(bytes[i + 4]);
                output += L"<Resource " + HexValue(resourceId, 8) + L">";
                i += 5;
            }
            else
            {
                run.push_back(value);
            }
        }
        flush();
        return output;
    }

    bool ParseBytes(const ByteVector& bytes, ParseResult& result)
    {
        result = {};
        if (bytes.empty())
            return false;

        if (ParseAudio(bytes, result) || ParseDialog(bytes, result) || ParseMobList(bytes, result) ||
            ParseDmsg(bytes, result) || ParseXiString(bytes, result) || ParseItemData(bytes, result) ||
            ParseSimpleString(bytes, result) || ScanImages(bytes, result))
        {
            if (result.kind == FileKind::Dialog || result.kind == FileKind::MobList ||
                result.kind == FileKind::Dmsg || result.kind == FileKind::XiString ||
                result.kind == FileKind::SimpleString || result.kind == FileKind::ItemData)
            {
                result.warning = L"Text uses Windows CP932 plus known FFXI extensions; rare private glyphs may be shown as fallback characters.";
            }
            return true;
        }
        if (result.warning.empty())
            result.warning = L"No supported POLUtils-derived resource structure was detected.";
        return false;
    }

    bool ParseFile(const char* path, ParseResult& result)
    {
        ByteVector bytes;
        if (!ReadFileBytes(path, bytes))
        {
            result = {};
            result.warning = L"The file could not be read or exceeds the 512 MiB resource-inspection limit.";
            return false;
        }
        return ParseBytes(bytes, result);
    }
}
