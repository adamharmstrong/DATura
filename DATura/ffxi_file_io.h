#pragma once

#include <windows.h>

#include <cstddef>

// Small file-reading primitive shared by DAT loaders. The caller owns the
// returned buffer and must release it with delete[].
namespace FFXIFileIO
{
    enum class ReadResult
    {
        Success,
        InvalidArguments,
        OpenFailed,
        EmptyOrUnreadable,
        ReadFailed,
    };

    bool ReadWholeFile(const char* path, BYTE** outBuffer, DWORD* outSize,
                       ReadResult* outResult = nullptr);
    // Reads text verbatim and appends a null terminator. Empty files are valid.
    // The caller owns the returned buffer and must release it with delete[].
    bool ReadWholeTextFile(const char* path, char** outText, DWORD* outSize = nullptr,
                           ReadResult* outResult = nullptr);
    bool WriteTextFile(const char* path, const char* text);

    void GetExecutableDirectory(char* outDir, std::size_t outDirSize);
    void MakeSafeFileStem(const char* name, char* outStem, std::size_t outStemSize);
    void ReplaceExtension(const char* path, const char* extension, char* outPath, std::size_t outPathSize);

    bool BrowseForDatFile(HWND owner, const char* initialDirectory,
                          char* outPath, std::size_t outPathSize, const char* title);
    bool BrowseForDatSetFile(HWND owner, const char* initialDirectory,
                             char* outPath, std::size_t outPathSize);
}
