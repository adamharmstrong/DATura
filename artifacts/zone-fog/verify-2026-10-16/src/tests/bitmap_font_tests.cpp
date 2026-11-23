#include "stdafx.h"
#include "ffxi_bitmap_font.h"
#include "ffxi_nameplate_font.h"
#include "win32_drawing.h"
#include <fstream>
#include <iterator>

int main(int argc, char** argv)
{
    if (argc != 3) { puts("usage: BitmapFontTests <FFXI root> <preview.bmp>"); return 2; }
    FFXIBitmapFont::SetRootPath(argv[1]);
    if (!FFXIBitmapFont::Load() || !FFXIBitmapFont::Supports(L"Abc 0123 !?")) return 1;
    if (FFXIBitmapFont::Supports(L"\u65e5")) return 1;
    if (FFXIBitmapFont::Measure(L"WWW", 16).cx <= FFXIBitmapFont::Measure(L"iii", 16).cx) return 1;
    if (FFXIBitmapFont::Measure(L"A\nB", 16).cy != 32) return 1;
    if (!FFXINameplateFont::Load() || !FFXINameplateFont::Supports(L"Jeanvirgaud")) return 1;
    if (FFXINameplateFont::Supports(L"\u65e5") || FFXINameplateFont::Supports(L"A\nB")) return 1;
    // These authored UVs identify the large nameplate letters, not moji or damage digits.
    const auto& a = FFXINameplateFont::glyphs[L'A' - 32];
    const auto& g = FFXINameplateFont::glyphs[L'g' - 32];
    if (a.x != 211 || a.y != 131 || a.width != 19 || a.height != 23 || g.height != 27) return 1;
    if (FFXINameplateFont::Measure(L"WWW", 16).cx <= FFXINameplateFont::Measure(L"iii", 16).cx) return 1;
    // Reject damaged layout records instead of sampling arbitrary texture regions.
    std::ifstream input(std::string(argv[1]) + "/ROM/119/51.DAT", std::ios::binary);
    std::vector<unsigned char> dat{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    bool checkedShape = false;
    bool checkedEnglishUi = false;
    for (size_t o = 0; o + 32 <= dat.size();)
    {
        unsigned info = 0;
        memcpy(&info, dat.data() + o + 4, 4);
        const size_t length = (info >> 3) & 0x7ffff0;
        if (length < 16 || length > dat.size() - o) return 1;
        if ((info & 127) == 32 && length >= 85 + 1024 * 2048 &&
            memcmp(dat.data() + o + 17, "font    moji    ", 16) == 0)
        {
            noeRAPI_t api(nullptr);
            const auto* english = api.Noesis_ConvertDXT(1024, 2048, dat.data() + o + 85, NOESISTEX_DXT3);
            if (!english || memcmp(english, FFXIBitmapFont::rgba.data(), 1024 * 32 * 4)) return 1;
            checkedEnglishUi = true;
        }
        if ((info & 127) == 49 && memcmp(dat.data() + o + 16, "font    fontshp ", 16) == 0)
        {
            std::array<FFXINameplateFont::Glyph, 95> parsed;
            if (!FFXINameplateFont::ReadShapes(dat.data() + o, length, parsed) ||
                FFXINameplateFont::ReadShapes(dat.data() + o, 34, parsed)) return 1;
            std::vector<unsigned char> damaged(dat.begin() + o, dat.begin() + o + length);
            const size_t first = 33 + size_t(damaged[32]) * 16 + 2 + 46;
            damaged[first + 33] = damaged[first + 34] = 255;
            if (FFXINameplateFont::ReadShapes(damaged.data(), damaged.size(), parsed)) return 1;
            checkedShape = true;
            break;
        }
        o += length;
    }
    if (!checkedShape || !checkedEnglishUi) return 1;
    HDC dc = CreateCompatibleDC(nullptr);
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = 640;
    info.bmiHeader.biHeight = -280;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap || !bits) return 1;
    auto old = SelectObject(dc, bitmap);
    std::fill_n(static_cast<DWORD*>(bits), 640 * 280, 0x00101830);
    FFXIBitmapFont::Draw(dc, L"FINAL FANTASY XI - Bastok Markets", 12, 8, 24, RGB(255, 255, 255));
    FFXINameplateFont::Draw(dc, L"> Jeanvirgaud <", 12, 44, 16, RGB(164, 255, 164));
    FFXINameplateFont::Draw(dc, L"Home Point - Auction House", 12, 66, 13, RGB(210, 220, 230));
    FFXINameplateFont::Draw(dc, L"ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz 0123456789", 12, 90, 16, RGB(255, 255, 255));
    const char* labels[] = {"Select Character", "Create Character", "Delete Character", "Config", "Back"};
    for (int column = 0; column < 2; ++column)
    {
        HFONT font = CreateFontA(column ? -24 : -18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, "Arial");
        for (int row = 0; row < 5; ++row)
        {
            RECT bounds = {column * 320, 130 + row * 28, (column + 1) * 320, 158 + row * 28};
            Win32Drawing::DrawShadowText(dc, font, labels[row], bounds,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(255, 255, 255), 1);
        }
        DeleteObject(font);
    }
    GdiFlush();
    const auto* pixels = static_cast<const DWORD*>(bits);
    int changed = 0;
    for (int i = 0; i < 640 * 280; ++i) changed += (pixels[i] & 0xffffff) != 0x101830;
    if (changed < 500 || pixels[639] != 0x00101830) return 1;
    BITMAPFILEHEADER header = {};
    header.bfType = 0x4d42;
    header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
    header.bfSize = header.bfOffBits + 640 * 280 * 4;
    FILE* file = nullptr;
    if (fopen_s(&file, argv[2], "wb") != 0 || !file) return 1;
    fwrite(&header, sizeof(header), 1, file);
    fwrite(&info.bmiHeader, sizeof(BITMAPINFOHEADER), 1, file);
    fwrite(bits, 4, 640 * 280, file);
    fclose(file);
    SelectObject(dc, old);
    DeleteObject(bitmap);
    DeleteDC(dc);
    FFXIBitmapFont::SetRootPath("missing-font-test-root");
    if (FFXIBitmapFont::Load() || FFXINameplateFont::Load()) return 1;
    FFXIBitmapFont::SetRootPath(argv[1]);
    if (!FFXIBitmapFont::Load() || !FFXINameplateFont::Load()) return 1;
    puts("PASS: separate moji/nameplate atlases, authored glyph UVs, malformed layout rejection, ASCII coverage, spacing, transparency, root reload.");
    return 0;
}
