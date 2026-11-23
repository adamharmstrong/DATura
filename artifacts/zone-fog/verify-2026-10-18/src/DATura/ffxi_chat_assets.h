#pragma once

#include "ffxi_dat_resolver.h"
#include <windows.h>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

// Retail ROM/119/51.DAT stores logwindo as a menu control which selects group
// 6 of the windowps shape set. The window surface itself is a generated
// "newtex" quad; its four authored vertex colours live in that shape record.
namespace FFXIChatAssets
{
inline std::string rootPath;
inline bool attempted = false;
inline bool available = false;
inline COLORREF topColour = RGB(16, 24, 61);
inline COLORREF bottomColour = RGB(8, 12, 31);

inline void SetRootPath(const char* root)
{
    const std::string next = root ? root : "";
    if (next == rootPath) return;
    rootPath = next;
    attempted = false;
    available = false;
}

inline unsigned int ReadU32(const unsigned char* data)
{
    unsigned int value = 0;
    memcpy(&value, data, sizeof(value));
    return value;
}

inline unsigned short ReadU16(const unsigned char* data)
{
    unsigned short value = 0;
    memcpy(&value, data, sizeof(value));
    return value;
}

inline bool Load()
{
    if (attempted) return available;
    attempted = true;
    if (rootPath.empty()) return false;
    std::string path = rootPath;
    if (path.back() != '/' && path.back() != '\\') path += '\\';
    path += "ROM\\119\\51.DAT";
    std::vector<unsigned char> bytes;
    if (!FFXIDatResolver::ReadBytes(path.c_str(), bytes, 64 * 1024 * 1024) || bytes.size() < 48)
        return false;

    for (size_t offset = 32; offset + 48 <= bytes.size();)
    {
        const unsigned int info = ReadU32(bytes.data() + offset + 4);
        const size_t length = (info >> 3) & 0x7ffff0;
        if (length < 16 || length > bytes.size() - offset) break;
        if ((info & 127) == 0x31 && length >= 64 &&
            memcmp(bytes.data() + offset + 16, "menu    windowps", 16) == 0)
        {
            size_t cursor = offset + 33;
            const unsigned int textureRefs = bytes[offset + 32];
            if (cursor + textureRefs * 16 + 2 > offset + length) break;
            cursor += textureRefs * 16;
            const unsigned int groupCount = ReadU16(bytes.data() + cursor);
            cursor += 2;
            for (unsigned int group = 0; group < groupCount; ++group)
            {
                if (cursor >= offset + length) break;
                const unsigned int imageCount = bytes[cursor++];
                for (unsigned int image = 0; image < imageCount; ++image)
                {
                    constexpr size_t imageRefSize = 61;
                    if (cursor + imageRefSize > offset + length) return false;
                    if (group == 6 && image == 0 &&
                        memcmp(bytes.data() + cursor + 45, "menu    newtex  ", 16) == 0)
                    {
                        const unsigned char* colours = bytes.data() + cursor + 25;
                        // Shape components use 0..127 modulation. Preserve the
                        // retail top/bottom intensity ratio over FFXI's blue log tint.
                        const int top = (colours[0] + colours[4]) / 2;
                        const int bottom = (colours[8] + colours[12]) / 2;
                        const auto tint = [](int intensity) -> COLORREF {
                            return RGB(intensity * 18 / 127,
                                intensity * 28 / 127, intensity * 72 / 127);
                        };
                        topColour = tint(top);
                        bottomColour = tint(bottom);
                        available = true;
                        OutputDebugStringA("FFXI chat window shape loaded: menu/windowps group 6.\n");
                        return true;
                    }
                    cursor += imageRefSize;
                }
            }
            break;
        }
        offset += length;
    }
    OutputDebugStringA("FFXI chat window shape unavailable; using fallback frame.\n");
    return false;
}

inline bool PaintWindow(HDC dc, const RECT& rect)
{
    if (!Load() || rect.right <= rect.left || rect.bottom <= rect.top) return false;
    const int saved = SaveDC(dc);
    if (!saved)
        return false;
    // GDI centers thick pen strokes on their endpoints. Keep the generated
    // scan lines inside the window even when this is painted directly into
    // the D3D back buffer, whose DC is not naturally clipped to the child.
    IntersectClipRect(dc, rect.left, rect.top, rect.right, rect.bottom);
    TRIVERTEX vertices[2] = {};
    vertices[0].x = rect.left; vertices[0].y = rect.top;
    vertices[0].Red = GetRValue(topColour) << 8;
    vertices[0].Green = GetGValue(topColour) << 8;
    vertices[0].Blue = GetBValue(topColour) << 8;
    vertices[0].Alpha = 0xffff;
    vertices[1].x = rect.right; vertices[1].y = rect.bottom;
    vertices[1].Red = GetRValue(bottomColour) << 8;
    vertices[1].Green = GetGValue(bottomColour) << 8;
    vertices[1].Blue = GetBValue(bottomColour) << 8;
    vertices[1].Alpha = 0xffff;
    GRADIENT_RECT gradient{0, 1};
    if (!GradientFill(dc, vertices, 2, &gradient, 1, GRADIENT_FILL_RECT_V))
    {
        RestoreDC(dc, saved);
        return false;
    }

    // newtex is the retail client's generated scan-lined window surface. Its
    // vertex colours come from windowps; reproduce its one-pixel horizontal
    // modulation over the complete window instead of restarting it per row.
    constexpr int retailPatternScale = 2;
    HPEN scanline = CreatePen(PS_SOLID, retailPatternScale, RGB(
        (GetRValue(bottomColour) * 3) / 4,
        (GetGValue(bottomColour) * 3) / 4,
        (GetBValue(bottomColour) * 3) / 4));
    HGDIOBJ oldPen = SelectObject(dc, scanline);
    for (int y = rect.top + retailPatternScale; y < rect.bottom;
        y += retailPatternScale * 2)
    {
        MoveToEx(dc, rect.left, y, nullptr);
        LineTo(dc, rect.right - 1, y);
    }
    SelectObject(dc, oldPen);
    DeleteObject(scanline);
    RestoreDC(dc, saved);
    return true;
}
}
