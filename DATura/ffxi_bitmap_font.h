#pragma once

#include "noesis_rapi.h"
#include "ffxi_dat_resolver.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

#pragma comment(lib, "Msimg32.lib")

// English UI ROM/119/51.DAT, texture "font    moji    ". Only the verified ASCII
// cells are mapped here; other scripts continue through the Unicode GDI path.
namespace FFXIBitmapFont
{
inline std::string rootPath;
inline bool attempted = false;
inline std::vector<unsigned char> rgba;
inline std::array<int, 95> left{}, advance{};

inline void SetRootPath(const char* root)
{
    const std::string next = root ? root : "";
    if (next == rootPath) return;
    rootPath = next;
    attempted = false;
    rgba.clear();
}

inline bool Load()
{
    if (attempted) return !rgba.empty();
    attempted = true;
    std::string path = rootPath;
    if (path.empty()) return false;
    if (path.back() != '/' && path.back() != '\\') path += '\\';
    path += "ROM\\119\\51.DAT";
    std::vector<unsigned char> bytes;
    if (!FFXIDatResolver::ReadBytes(path.c_str(),bytes,64*1024*1024)) return false;
    for (size_t offset = 0; offset + 16 <= bytes.size();)
    {
        unsigned int info = 0;
        memcpy(&info, bytes.data() + offset + 4, 4);
        const size_t length = (info >> 3) & 0x7ffff0;
        if (length < 16 || length > bytes.size() - offset) break;
        const unsigned char* chunk = bytes.data() + offset;
        if ((info & 127) == 32 && length >= 85 && chunk[16] == 0xa1 &&
            memcmp(chunk + 17, "font    moji    ", 16) == 0)
        {
            int width = 0, height = 0;
            memcpy(&width, chunk + 37, 4);
            memcpy(&height, chunk + 41, 4);
            if (width != 1024 || height != 2048 ||
                memcmp(chunk + 73, "3TXD", 4) != 0 || length < 85 + 1024 * 2048)
                break;
            noeRAPI_t api(nullptr);
            unsigned char* decoded = api.Noesis_ConvertDXT(width, height,
                const_cast<unsigned char*>(chunk + 85), NOESISTEX_DXT3);
            if (!decoded) break;
            // ASCII occupies the first two rows. Keep CPU pixels across device resets.
            rgba.assign(decoded, decoded + 1024 * 32 * 4);
            for (int glyph = 0; glyph < 95; ++glyph)
            {
                int first = 16, last = -1;
                for (int y = 0; y < 16; ++y)
                    for (int x = 0; x < 16; ++x)
                    {
                        const size_t p = ((glyph / 64 * 16 + y) * 1024 + glyph % 64 * 16 + x) * 4;
                        if (rgba[p + 3] && std::max({rgba[p], rgba[p + 1], rgba[p + 2]}) > 32)
                        { first = std::min(first, x); last = std::max(last, x); }
                    }
                left[glyph] = last < 0 ? 0 : first;
                advance[glyph] = last < 0 ? 6 : last - first + 2;
            }
            OutputDebugStringA("FFXI bitmap font loaded from English ROM/119/51.DAT (font moji).\n");
            return true;
        }
        offset += length;
    }
    OutputDebugStringA("FFXI bitmap font unavailable; using system text fallback.\n");
    return false;
}

inline bool Supports(const std::wstring& text)
{
    return Load() && std::all_of(text.begin(), text.end(), [](wchar_t c) {
        return (c >= 32 && c <= 126) || c == '\n' || c == '\r';
    });
}

inline int Advance(wchar_t c, int height)
{
    return (advance[c - 32] * height + 8) / 16;
}

inline SIZE Measure(const std::wstring& text, int height)
{
    SIZE size = {0, height};
    int x = 0;
    for (wchar_t c : text)
    {
        if (c == '\r') continue;
        if (c == '\n') { size.cx = std::max<LONG>(size.cx, x); x = 0; size.cy += height; }
        else x += Advance(c, height);
    }
    size.cx = std::max<LONG>(size.cx, x);
    return size;
}

// Blend glyph coverage into a DC. Bilinear sampling stays inside each glyph cell;
// fractional UI font sizes must not duplicate individual rows/columns. Caller owns
// text colour, shadows, and (for nameplates) outline composition.
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
    DWORD* pixels = static_cast<DWORD*>(bits);
    int penX = 0, penY = 0;
    for (wchar_t c : text)
    {
        if (c == '\r') continue;
        if (c == '\n') { penX = 0; penY += height; continue; }
        const int glyph = c - 32, step = Advance(c, height);
        for (int dy = 0; dy < height; ++dy)
            for (int dx = 0; dx < step && penX + dx < size.cx; ++dx)
            {
                const float sx = left[glyph] + (dx + 0.5f) * 16 / height - 0.5f;
                const float sy = (dy + 0.5f) * 16 / height - 0.5f;
                const int ix = static_cast<int>(std::floor(sx)), iy = static_cast<int>(std::floor(sy));
                const float fx = sx - ix, fy = sy - iy;
                const auto coverage = [&](int px, int py) -> float {
                    if (px < 0 || px >= 16 || py < 0 || py >= 16) return 0;
                    const size_t p = ((glyph / 64 * 16 + py) * 1024 + glyph % 64 * 16 + px) * 4;
                    return std::min(255u, unsigned(rgba[p + 3]) * 2) *
                        std::max({rgba[p], rgba[p + 1], rgba[p + 2]}) / 255.0f;
                };
                const unsigned int a = static_cast<unsigned>((1 - fy) *
                    ((1 - fx) * coverage(ix, iy) + fx * coverage(ix + 1, iy)) + fy *
                    ((1 - fx) * coverage(ix, iy + 1) + fx * coverage(ix + 1, iy + 1)) + 0.5f);
                pixels[(penY + dy) * size.cx + penX + dx] = (a << 24) |
                    ((GetRValue(color) * a / 255) << 16) | ((GetGValue(color) * a / 255) << 8) |
                    (GetBValue(color) * a / 255);
            }
        penX += step;
    }
    HGDIOBJ old = SelectObject(source, bitmap);
    const BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    AlphaBlend(dc, x, y, size.cx, size.cy, source, 0, 0, size.cx, size.cy, blend);
    SelectObject(source, old);
    DeleteDC(source);
    DeleteObject(bitmap);
}
}
