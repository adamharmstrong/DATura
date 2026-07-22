#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace FFXIResource
{
    struct ResolvedFile
    {
        int fileId = -1;
        int hive = 0;
        std::uint16_t packedLocation = 0;
        std::string relativePath;
        std::string fullPath;
    };

    struct Row
    {
        std::wstring kind;
        std::wstring id;
        std::wstring text;
        std::wstring details;
    };

    enum class FileKind
    {
        Unknown,
        Dialog,
        MobList,
        Dmsg,
        XiString,
        SimpleString,
        ItemData,
        Audio,
        EmbeddedImages,
    };

    struct ParseResult
    {
        FileKind kind = FileKind::Unknown;
        std::wstring format;
        std::vector<Row> rows;
        std::wstring warning;
    };

    // Resolves a numerical FFXI file ID through every installed VTABLE/FTABLE
    // pair. The table lengths, rather than a hard-coded maximum ID, determine
    // the valid range.
    bool ResolveFileId(const char* ffxiRoot, int fileId, ResolvedFile& result);

    // Parses the supported POLUtils-derived resource formats. The parser is
    // deliberately read-only and rejects malformed offsets and lengths.
    bool ParseFile(const char* path, ParseResult& result);

    // Exposed for callers that already have bytes in memory.
    bool ParseBytes(const std::vector<std::uint8_t>& bytes, ParseResult& result);

    // Shift-JIS/CP932 decoding with the FFXI element, Auto-Translator, and
    // embedded resource-reference extensions preserved as readable tokens.
    std::wstring DecodeText(const std::uint8_t* bytes, std::size_t size);
}
