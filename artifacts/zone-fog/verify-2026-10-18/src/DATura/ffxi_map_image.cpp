#include "stdafx.h"
#include "ffxi_map_image.h"
#include "ffxi_file_io.h"
#include "ffxi_resource.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{
template<typename T>
bool ReadAt(const std::vector<unsigned char>& data, const size_t offset, T& value)
{
    if (offset > data.size() || sizeof(T) > data.size() - offset)
        return false;
    memcpy(&value, data.data() + offset, sizeof(T));
    return true;
}

std::string MapCatalogPath()
{
    char directory[MAX_PATH] = {};
    FFXIFileIO::GetExecutableDirectory(directory, sizeof(directory));
    return std::string(directory) + "map_dats.json";
}

bool ReadFile(const char* path, std::vector<unsigned char>& data)
{
    BYTE* bytes = nullptr;
    DWORD byteCount = 0;
    if (!FFXIFileIO::ReadWholeFile(path, &bytes, &byteCount))
        return false;
    data.assign(bytes, bytes + byteCount);
    delete[] bytes;
    return true;
}

bool FindFirstMapFileId(const int zoneId, int& mapNumber, int& fileId)
{
    std::vector<unsigned char> bytes;
    const std::string path = MapCatalogPath();
    if (!ReadFile(path.c_str(), bytes))
        return false;
    const std::string json(bytes.begin(), bytes.end());
    const std::string zoneKey = "\"" + std::to_string(zoneId) + "\"";
    size_t position = json.find(zoneKey);
    if (position == std::string::npos || (position = json.find('{', position)) == std::string::npos)
        return false;
    const size_t end = json.find('}', position);
    if (end == std::string::npos)
        return false;

    int bestMap = INT_MAX;
    int bestFile = -1;
    while (position < end)
    {
        const size_t keyStart = json.find('"', position + 1);
        if (keyStart == std::string::npos || keyStart >= end)
            break;
        const size_t keyEnd = json.find('"', keyStart + 1);
        const size_t colon = json.find(':', keyEnd);
        if (keyEnd == std::string::npos || colon == std::string::npos || colon >= end)
            break;
        const int candidateMap = atoi(json.substr(keyStart + 1, keyEnd - keyStart - 1).c_str());
        const int candidateFile = atoi(json.c_str() + colon + 1);
        if (candidateFile > 0 && candidateMap < bestMap)
        {
            bestMap = candidateMap;
            bestFile = candidateFile;
        }
        position = colon + 1;
    }
    if (bestFile < 0)
        return false;
    mapNumber = bestMap;
    fileId = bestFile;
    return true;
}

void Decode565(const uint16_t color, unsigned char output[4])
{
    output[2] = static_cast<unsigned char>(((color >> 11) & 31) * 255 / 31);
    output[1] = static_cast<unsigned char>(((color >> 5) & 63) * 255 / 63);
    output[0] = static_cast<unsigned char>((color & 31) * 255 / 31);
    output[3] = 255;
}

bool DecodeDxt(const std::vector<unsigned char>& data, const size_t offset,
               const int width, const int height, const int type,
               std::vector<unsigned char>& pixels)
{
    const int blockSize = type == 1 ? 8 : 16;
    const int blockColumns = (width + 3) / 4;
    const int blockRows = (height + 3) / 4;
    if (offset + static_cast<size_t>(blockColumns) * blockRows * blockSize > data.size())
        return false;
    pixels.assign(static_cast<size_t>(width) * height * 4, 255);
    size_t source = offset;
    for (int by = 0; by < blockRows; ++by)
    {
        for (int bx = 0; bx < blockColumns; ++bx, source += blockSize)
        {
            uint64_t alphaBits = 0;
            if (type != 1) memcpy(&alphaBits, data.data() + source, 8);
            const size_t colorOffset = source + (type == 1 ? 0 : 8);
            uint16_t c0 = 0, c1 = 0;
            uint32_t indices = 0;
            memcpy(&c0, data.data() + colorOffset, 2);
            memcpy(&c1, data.data() + colorOffset + 2, 2);
            memcpy(&indices, data.data() + colorOffset + 4, 4);
            unsigned char colors[4][4] = {};
            Decode565(c0, colors[0]); Decode565(c1, colors[1]);
            for (int channel = 0; channel < 3; ++channel)
            {
                colors[2][channel] = static_cast<unsigned char>((2 * colors[0][channel] + colors[1][channel] + 1) / 3);
                colors[3][channel] = static_cast<unsigned char>((colors[0][channel] + 2 * colors[1][channel] + 1) / 3);
            }
            colors[2][3] = colors[3][3] = 255;
            if (type == 1 && c0 <= c1)
            {
                for (int channel = 0; channel < 3; ++channel)
                    colors[2][channel] = static_cast<unsigned char>((colors[0][channel] + colors[1][channel]) / 2);
                memset(colors[3], 0, 4);
            }
            for (int py = 0; py < 4; ++py)
            {
                for (int px = 0; px < 4; ++px)
                {
                    const int x = bx * 4 + px, y = by * 4 + py;
                    const int texel = py * 4 + px;
                    if (x >= width || y >= height) continue;
                    const unsigned int index = (indices >> (texel * 2)) & 3;
                    unsigned char* destination = pixels.data() + (static_cast<size_t>(y) * width + x) * 4;
                    memcpy(destination, colors[index], 4);
                    if (type == 3)
                    {
                        const int alpha = static_cast<int>((alphaBits >> (texel * 4)) & 15);
                        destination[3] = static_cast<unsigned char>(alpha >= 8 ? 255 : alpha << 5);
                    }
                }
            }
        }
    }
    return true;
}

bool Decode(const std::vector<unsigned char>& data, int& width, int& height,
            std::vector<unsigned char>& pixels)
{
    uint16_t bitCount = 0;
    uint32_t type = 0;
    if (!ReadAt(data, 0x45, width) || !ReadAt(data, 0x49, height) ||
        !ReadAt(data, 0x4f, bitCount) || !ReadAt(data, 0x69, type) ||
        width <= 0 || height <= 0 || width > 4096 || height > 4096)
        return false;
    if (type >= 0x44585431 && type <= 0x44585435)
        return DecodeDxt(data, 0x75, width, height, static_cast<int>(type - 0x44585430), pixels);
    if (type != 0x0a)
        return false;

    const size_t pixelCount = static_cast<size_t>(width) * height;
    pixels.assign(pixelCount * 4, 255);
    size_t source = 0x6d;
    std::vector<unsigned char> palette;
    if (bitCount == 8)
    {
        if (source + 1024 + pixelCount > data.size()) return false;
        palette.assign(data.begin() + source, data.begin() + source + 1024);
        source += 1024;
    }
    const size_t bytesPerPixel = bitCount / 8;
    if ((bitCount != 8 && bitCount != 16 && bitCount != 24 && bitCount != 32) ||
        source + pixelCount * bytesPerPixel > data.size())
        return false;
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const size_t sourcePixel = static_cast<size_t>(y) * width + x;
            unsigned char* destination = pixels.data() +
                (static_cast<size_t>(height - 1 - y) * width + x) * 4;
            if (bitCount == 8)
                memcpy(destination, palette.data() + data[source + sourcePixel] * 4, 4);
            else if (bitCount == 16)
            {
                uint16_t color = 0; memcpy(&color, data.data() + source + sourcePixel * 2, 2);
                Decode565(color, destination);
            }
            else
            {
                memcpy(destination, data.data() + source + sourcePixel * bytesPerPixel, bytesPerPixel);
                destination[3] = 255;
            }
        }
    }
    return true;
}
}

namespace FFXIMapImage
{
void Release(Image& image)
{
    if (image.bitmap) DeleteObject(image.bitmap);
    image = {};
}

bool Load(const char* ffxiRoot, const int zoneId, Image& image)
{
    Release(image);
    int mapNumber = -1, fileId = -1;
    if (!FindFirstMapFileId(zoneId, mapNumber, fileId))
        return false;
    FFXIResource::ResolvedFile resolved;
    if (!FFXIResource::ResolveFileId(ffxiRoot, fileId, resolved))
        return false;
    std::vector<unsigned char> data, pixels;
    int width = 0, height = 0;
    if (!ReadFile(resolved.sourcePath.c_str(), data) || !Decode(data, width, height, pixels))
        return false;
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HDC screen = GetDC(nullptr);
    image.bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!image.bitmap || !bits)
    {
        Release(image);
        return false;
    }
    memcpy(bits, pixels.data(), pixels.size());
    image.width = width;
    image.height = height;
    image.mapNumber = mapNumber;
    image.sourcePath = resolved.sourcePath;
    return true;
}
}
