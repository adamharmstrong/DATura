#include "stdafx.h"
#include "ffxi_file_io.h"

#include <cctype>
#include <commdlg.h>
#include <cstring>

namespace FFXIFileIO
{
bool ReadWholeFile(const char* path, BYTE** outBuffer, DWORD* outSize, ReadResult* outResult)
{
    if (outResult)
        *outResult = ReadResult::InvalidArguments;
    if (!path || !outBuffer || !outSize)
        return false;

    *outBuffer = nullptr;
    *outSize = 0;

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ,
                              nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        if (outResult)
            *outResult = ReadResult::OpenFailed;
        return false;
    }

    const DWORD fileSize = GetFileSize(file, nullptr);
    if (fileSize == 0 || fileSize == INVALID_FILE_SIZE)
    {
        CloseHandle(file);
        if (outResult)
            *outResult = ReadResult::EmptyOrUnreadable;
        return false;
    }

    BYTE* buffer = new BYTE[fileSize];
    DWORD bytesRead = 0;
    const BOOL readOk = ReadFile(file, buffer, fileSize, &bytesRead, nullptr);
    CloseHandle(file);

    if (!readOk || bytesRead != fileSize)
    {
        delete[] buffer;
        if (outResult)
            *outResult = ReadResult::ReadFailed;
        return false;
    }

    *outBuffer = buffer;
    *outSize = fileSize;
    if (outResult)
        *outResult = ReadResult::Success;
    return true;
}

bool ReadWholeTextFile(const char* path, char** outText, DWORD* outSize, ReadResult* outResult)
{
    if (outResult)
        *outResult = ReadResult::InvalidArguments;
    if (!path || !outText)
        return false;

    *outText = nullptr;
    if (outSize)
        *outSize = 0;

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ,
                              nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        if (outResult)
            *outResult = ReadResult::OpenFailed;
        return false;
    }

    const DWORD fileSize = GetFileSize(file, nullptr);
    if (fileSize == INVALID_FILE_SIZE)
    {
        CloseHandle(file);
        if (outResult)
            *outResult = ReadResult::EmptyOrUnreadable;
        return false;
    }

    char* text = new char[static_cast<std::size_t>(fileSize) + 1];
    DWORD bytesRead = 0;
    const BOOL readOk = ReadFile(file, text, fileSize, &bytesRead, nullptr);
    CloseHandle(file);
    text[fileSize] = '\0';

    if (!readOk || bytesRead != fileSize)
    {
        delete[] text;
        if (outResult)
            *outResult = ReadResult::ReadFailed;
        return false;
    }

    *outText = text;
    if (outSize)
        *outSize = fileSize;
    if (outResult)
        *outResult = ReadResult::Success;
    return true;
}

bool WriteTextFile(const char* path, const char* text)
{
    HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    const DWORD textLength = static_cast<DWORD>(std::strlen(text));
    DWORD written = 0;
    const BOOL writeOk = WriteFile(file, text, textLength, &written, nullptr);
    CloseHandle(file);
    return writeOk && written == textLength;
}

void GetExecutableDirectory(char* outDir, std::size_t outDirSize)
{
    if (!outDir || outDirSize == 0)
        return;

    outDir[0] = '\0';
    char executablePath[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, executablePath, sizeof(executablePath));
    strcpy_s(outDir, outDirSize, executablePath);
    char* lastSlash = strrchr(outDir, '\\');
    if (!lastSlash)
        lastSlash = strrchr(outDir, '/');
    if (lastSlash)
        lastSlash[1] = '\0';
}

void MakeSafeFileStem(const char* name, char* outStem, std::size_t outStemSize)
{
    if (!outStem || outStemSize == 0)
        return;

    const char* source = (name && name[0]) ? name : "Adventurer";
    std::size_t out = 0;
    for (int i = 0; source[i] && out < outStemSize - 1; ++i)
    {
        const unsigned char c = static_cast<unsigned char>(source[i]);
        if (std::isalnum(c) || c == '-' || c == '_')
        {
            outStem[out++] = static_cast<char>(c);
        }
        else if ((c == ' ' || c == '.') && out > 0 && outStem[out - 1] != '_')
        {
            outStem[out++] = '_';
        }
    }

    while (out > 0 && outStem[out - 1] == '_')
        --out;
    if (out == 0)
        strcpy_s(outStem, outStemSize, "Adventurer");
    else
        outStem[out] = '\0';
}

void ReplaceExtension(const char* path, const char* extension, char* outPath, std::size_t outPathSize)
{
    strcpy_s(outPath, outPathSize, path ? path : "");
    char* lastSlash = strrchr(outPath, '\\');
    char* alternateSlash = strrchr(outPath, '/');
    if (!lastSlash || (alternateSlash && alternateSlash > lastSlash))
        lastSlash = alternateSlash;
    char* dot = strrchr(outPath, '.');
    if (!dot || (lastSlash && dot < lastSlash))
        dot = outPath + strlen(outPath);
    *dot = '\0';
    strcat_s(outPath, outPathSize, extension);
}

bool BrowseForDatFile(const HWND owner, const char* initialDirectory,
                      char* outPath, const std::size_t outPathSize, const char* title)
{
    if (!outPath || outPathSize == 0 || outPathSize > MAXDWORD)
        return false;

    OPENFILENAMEA dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = "DAT Files (*.dat)\0*.dat\0All Files (*.*)\0*.*\0";
    dialog.lpstrFile = outPath;
    dialog.nMaxFile = static_cast<DWORD>(outPathSize);
    dialog.lpstrTitle = title;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    dialog.lpstrDefExt = "dat";
    dialog.lpstrInitialDir = (initialDirectory && initialDirectory[0]) ? initialDirectory : nullptr;
    outPath[0] = '\0';
    return GetOpenFileNameA(&dialog) != FALSE;
}

bool BrowseForDatSetFile(const HWND owner, const char* initialDirectory,
                         char* outPath, const std::size_t outPathSize)
{
    if (!outPath || outPathSize == 0 || outPathSize > MAXDWORD)
        return false;

    OPENFILENAMEA dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = "FFXI DAT Set Files (*.ff11datset;*.txt)\0*.ff11datset;*.txt\0All Files (*.*)\0*.*\0";
    dialog.lpstrFile = outPath;
    dialog.nMaxFile = static_cast<DWORD>(outPathSize);
    dialog.lpstrTitle = "Open FFXI DAT Set";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    dialog.lpstrInitialDir = (initialDirectory && initialDirectory[0]) ? initialDirectory : nullptr;
    outPath[0] = '\0';
    return GetOpenFileNameA(&dialog) != FALSE;
}
}
