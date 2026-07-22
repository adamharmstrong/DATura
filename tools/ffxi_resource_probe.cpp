#include "../DATura/ffxi_resource.h"

#include <windows.h>
#include <cstdlib>
#include <iostream>
#include <string>

static std::string Utf8(const std::wstring& text)
{
    if (text.empty())
        return std::string();
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0, nullptr, nullptr);
    std::string result((size_t)size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), result.data(), size, nullptr, nullptr);
    return result;
}

int main(int argc, char** argv)
{
    if (argc != 2 && argc != 4)
    {
        std::cerr << "Usage: ffxi_resource_probe <DAT path>\n"
                     "   or: ffxi_resource_probe --id <FFXI root> <file id>\n";
        return 2;
    }

    std::string path;
    if (argc == 4)
    {
        if (std::string(argv[1]) != "--id")
            return 2;
        FFXIResource::ResolvedFile resolved;
        if (!FFXIResource::ResolveFileId(argv[2], std::atoi(argv[3]), resolved))
        {
            std::cerr << "File ID could not be resolved.\n";
            return 1;
        }
        path = resolved.fullPath;
        std::cout << "Resolved\t" << resolved.fileId << "\t" << resolved.relativePath << "\n";
    }
    else
    {
        path = argv[1];
    }

    FFXIResource::ParseResult parsed;
    if (!FFXIResource::ParseFile(path.c_str(), parsed))
    {
        std::cerr << Utf8(parsed.warning) << "\n";
        return 1;
    }

    std::cout << "Format\t" << Utf8(parsed.format) << "\n";
    std::cout << "Rows\t" << parsed.rows.size() << "\n";
    const size_t displayCount = parsed.rows.size() < 10 ? parsed.rows.size() : 10;
    for (size_t i = 0; i < displayCount; ++i)
    {
        const FFXIResource::Row& row = parsed.rows[i];
        std::cout << Utf8(row.kind) << "\t" << Utf8(row.id) << "\t"
                  << Utf8(row.text) << "\t" << Utf8(row.details) << "\n";
    }
    return 0;
}
