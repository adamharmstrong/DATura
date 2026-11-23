#pragma once

#include "ffxi_bitmap_font.h"

// Nameplates use the Latin-only "font font", not the kanji/chat "font moji".
// Read the matching fontshp UVs and logical quad sizes from the same English DAT.
namespace FFXINameplateFont
{
struct Glyph
{
    int x = 0, y = 0, width = 0, height = 0;
    int drawWidth = 0, top = 0, bottom = 0;
};
inline std::string loadedRoot;
inline bool attempted = false;
inline std::vector<unsigned char> pixels;
inline std::vector<unsigned char> iconPixels;
inline std::array<Glyph, 95> glyphs;

inline unsigned short U16(const unsigned char* p)
{
    return unsigned(p[0]) | (unsigned(p[1]) << 8);
}

inline bool ReadShapes(const unsigned char* chunk, size_t size, std::array<Glyph, 95>& result)
{
    if (size < 35 || memcmp(chunk + 16, "font    fontshp ", 16) != 0) return false;
    const size_t tableEnd = 33 + size_t(chunk[32]) * 16;
    if (tableEnd + 2 > size || U16(chunk + tableEnd) < 95) return false;
    // Verified retail single-quad ASCII layout: the blank space has no texture
    // name; subsequent entries have a four-byte prefix, material name, and quad.
    const size_t firstMaterial = tableEnd + 2 + 46;
    if (firstMaterial + 94 * 62 > size) return false;
    result = {};
    result[0].drawWidth = 4;
    for (size_t i = 1; i < result.size(); ++i)
    {
        const auto* material = chunk + firstMaterial + (i - 1) * 62;
        if (memcmp(material, "font    font    ", 16) != 0 || material[16] != 1) return false;
        const auto* quad = material + 17;
        Glyph g;
        g.drawWidth = U16(quad + 4);
        g.top = U16(quad + 2);
        g.bottom = U16(quad + 10);
        g.width = U16(quad + 16);
        g.height = U16(quad + 18);
        g.x = U16(quad + 20);
        g.y = U16(quad + 22);
        if (g.drawWidth <= 0 || g.drawWidth > 32 || g.top > g.bottom || g.bottom > 16 ||
            g.width <= 0 || g.height <= 0 || g.x + g.width > 256 || g.y + g.height > 256)
            return false;
        result[i] = g;
    }
    return true;
}

inline bool Load()
{
    if (loadedRoot != FFXIBitmapFont::rootPath)
    {
        loadedRoot = FFXIBitmapFont::rootPath;
        attempted = false;
        pixels.clear();
        iconPixels.clear();
    }
    if (attempted) return !pixels.empty();
    attempted = true;
    if (loadedRoot.empty()) return false;
    std::string path = loadedRoot;
    if (path.back() != '/' && path.back() != '\\') path += '\\';
    path += "ROM\\119\\51.DAT";
    std::vector<unsigned char> bytes;
    if (!FFXIDatResolver::ReadBytes(path.c_str(),bytes,64*1024*1024)) return false;
    std::vector<unsigned char> decodedPixels;
    std::vector<unsigned char> decodedIcons;
    std::array<Glyph, 95> shapes = {};
    bool haveShapes = false;
    for (size_t offset = 0; offset + 16 <= bytes.size();)
    {
        unsigned int info = 0;
        memcpy(&info, bytes.data() + offset + 4, 4);
        const size_t size = (info >> 3) & 0x7ffff0;
        if (size < 16 || size > bytes.size() - offset) return false;
        const auto* chunk = bytes.data() + offset;
        if ((info & 127) == 32 && size >= 85 + 256 * 256 && chunk[16] == 0xa1 &&
            (memcmp(chunk + 17, "font    font    ", 16) == 0 ||
             memcmp(chunk + 17, "menu    ustatshd", 16) == 0))
        {
            int width = 0, height = 0;
            memcpy(&width, chunk + 37, 4);
            memcpy(&height, chunk + 41, 4);
            if (width != 256 || height != 256 || memcmp(chunk + 73, "3TXD", 4) != 0) return false;
            noeRAPI_t api(nullptr);
            auto* decoded = api.Noesis_ConvertDXT(width, height,
                const_cast<unsigned char*>(chunk + 85), NOESISTEX_DXT3);
            if (!decoded) return false;
            auto& destination = memcmp(chunk + 17, "font    font    ", 16) == 0 ? decodedPixels : decodedIcons;
            destination.assign(decoded, decoded + 256 * 256 * 4);
        }
        if ((info & 127) == 49 && size >= 32 && memcmp(chunk + 16, "font    fontshp ", 16) == 0)
            haveShapes = ReadShapes(chunk, size, shapes);
        offset += size;
    }
    if (decodedPixels.empty() || !haveShapes) return false;
    pixels.swap(decodedPixels);
    iconPixels.swap(decodedIcons);
    glyphs = shapes;
    OutputDebugStringA("FFXI nameplate font: ROM/119/51.DAT, font font + fontshp.\n");
    return true;
}

inline bool Supports(const std::wstring& text)
{
    return Load() && std::all_of(text.begin(), text.end(), [](wchar_t c) { return c >= 32 && c <= 126; });
}

inline int Advance(const Glyph& glyph, int height)
{
    return ((glyph.drawWidth + 1) * height + 6) / 13;
}

inline SIZE Measure(const std::wstring& text, int height)
{
    SIZE result = {};
    if (height <= 0 || !Supports(text)) return result;
    result.cy = height;
    for (wchar_t c : text) result.cx += Advance(glyphs[c - 32], height);
    return result;
}

inline void Draw(HDC dc, const std::wstring& text, int x, int y, int height, COLORREF color)
{
    const SIZE size = Measure(text, height);
    if (size.cx <= 0 || size.cy <= 0) return;
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = size.cx;
    info.bmiHeader.biHeight = -size.cy;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    HDC source = CreateCompatibleDC(dc);
    if (!bitmap || !bits || !source)
    { if (bitmap) DeleteObject(bitmap); if (source) DeleteDC(source); return; }
    memset(bits, 0, size.cx * size.cy * 4);
    int pen = 0;
    for (wchar_t c : text)
    {
        const auto& g = glyphs[c - 32];
        const int width = (g.drawWidth * height + 6) / 13;
        const int top = g.top * height / 13, bottom = g.bottom * height / 13;
        if (c != L' ')
            for (int dy = top; dy < bottom && dy < size.cy; ++dy)
                for (int dx = 0; dx < width && pen + dx < size.cx; ++dx)
                {
                    const int sx = g.x + dx * g.width / width;
                    const int sy = g.y + (dy - top) * g.height / (bottom - top);
                    const size_t p = (sy * 256 + sx) * 4;
                    const unsigned a = std::min(255u, unsigned(pixels[p + 3]) * 2) *
                        std::max({pixels[p], pixels[p + 1], pixels[p + 2]}) / 255;
                    static_cast<DWORD*>(bits)[dy * size.cx + pen + dx] = (a << 24) |
                        ((GetRValue(color) * a / 255) << 16) | ((GetGValue(color) * a / 255) << 8) |
                        (GetBValue(color) * a / 255);
                }
        pen += Advance(g, height);
    }
    const auto old = SelectObject(source, bitmap);
    const BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    AlphaBlend(dc, x, y, size.cx, size.cy, source, 0, 0, size.cx, size.cy, blend);
    SelectObject(source, old);
    DeleteDC(source);
    DeleteObject(bitmap);
}
}
