#pragma once

#include "ffxi_nameplate_font.h"
#include <string_view>

namespace FFXIPlayerIcons
{
struct Definition
{
    const char* id;
    const char* label;
    int x, y;
    bool tintable;
    int secondX = -1, secondY = 0;
    int stars = 0;
};

// 32px source rectangles verified against fontshp's ustatshd quads in
// ROM/119/51.DAT. Render at 16px, as authored by their logical quads.
inline constexpr Definition Catalog[] = {
    {"linkshell", "Linkshell", 0, 64, true},
    {"mentor", "Mentor", 64, 96, false},
    {"gm", "Game Master", 96, 96, false},
    {"away", "Away", 96, 64, false},
    {"seeking-party", "Seeking party", 64, 64, false},
    {"disconnecting", "Connection problems", 0, 96, false},
    {"auto-party", "Auto-party", 224, 64, false},
    {"new-adventurer", "New adventurer", 224, 0, false},
    {"bazaar", "Bazaar", 32, 96, false},
    {"ballista-sandoria", "Ballista: San d'Oria", 128, 0, false},
    {"ballista-bastok", "Ballista: Bastok", 160, 0, false},
    {"ballista-windurst", "Ballista: Windurst", 192, 0, false},
    {"ballista-griffon", "Ballista: Griffons", 160, 32, false},
    {"ballista-wyvern", "Ballista: Wyverns", 192, 32, false},
    {"ballista-red", "Ballista: red", 0, 128, false},
    {"ballista-blue", "Ballista: blue", 32, 128, false},
    {"level-sync", "Level sync", 224, 96, false},
    {"trial", "Trial adventurer", 160, 64, false},
    // The current retail fontshp maps all three GM grades to the same HD quad.
    {"sgm", "Senior Game Master", 96, 96, false},
    {"lgm", "Lead Game Master", 96, 96, false},
    {"developer", "Developer / Producer", 192, 96, false},
    {"official-staff", "Official staff (PlayOnline)", 224, 32, false},
    {"tribune", "Tribune correspondent (historical)", 64, 128, false},
    {"campaign", "Campaign", 128, 96, false},
    {"campaign-seeking", "Campaign + seeking party", 128, 96, false, 64, 64},
    {"campaign-auto-party", "Campaign + auto-party", 128, 96, false, 224, 64},
    {"gladiator", "Gladiator (Gla)", 128, 96, false},
    {"gladiator-cg", "Champion Gladiator (CG)", 128, 96, false, -1, 0, 1},
    {"gladiator-hcg", "High Champion Gladiator (HCG)", 128, 96, false, -1, 0, 2},
    {"monipulator", "Monipulator (Mon)", 128, 96, false, 160, 96},
    {"monipulator-nm", "Monipulator (NM)", 128, 96, false, 160, 96, 1},
    {"monipulator-hnm", "Monipulator (HNM)", 128, 96, false, 160, 96, 2},
    // Explicit previews of chat badges, not automatic overhead mentor grades.
    {"mentor-bronze", "Assist chat: Bronze Mentor", 192, 128, false},
    {"mentor-silver", "Assist chat: Silver Mentor", 224, 128, false},
    {"mentor-gold", "Assist chat: Gold Mentor", 0, 160, false},
};

inline const Definition* Find(std::string_view id)
{
    if (id == "ls") id = "linkshell";
    if (id == "m") id = "mentor";
    if (id == "busy") id = "auto-party"; // Migrate the old, incorrect label.
    for (const auto& icon : Catalog)
        if (id == icon.id) return &icon;
    return nullptr; // Unknown/none does not manufacture a placeholder badge.
}

inline const Definition* Available(std::string_view id)
{
    const auto* icon = Find(id);
    return icon && FFXINameplateFont::Load() && FFXINameplateFont::iconPixels.size() == 256 * 256 * 4
        ? icon : nullptr;
}

inline int Width(const Definition& icon) { return icon.secondX >= 0 ? 34 : 16; }

inline void CompositeSprite(std::vector<DWORD>& target, int width, int height,
                      int left, int top, int sourceX, int sourceY, int drawSize, DWORD tint)
{
    const auto& source = FFXINameplateFont::iconPixels;
    if (source.size() != 256 * 256 * 4 || width <= 0 || height <= 0 ||
        target.size() < size_t(width) * size_t(height)) return;
    if (drawSize <= 0 || 32 % drawSize || sourceX < 0 || sourceY < 0 ||
        sourceX + 32 > 256 || sourceY + 32 > 256) return;
    const int samples = 32 / drawSize;
    for (int y = 0; y < drawSize; ++y)
        for (int x = 0; x < drawSize; ++x)
        {
            if (left + x < 0 || left + x >= width || top + y < 0 || top + y >= height) continue;
            // Average in premultiplied space when reducing the 32px source;
            // transparent texels must not darken the coloured edges.
            unsigned alpha = 0, r = 0, g = 0, b = 0;
            for (int sy = 0; sy < samples; ++sy)
                for (int sx = 0; sx < samples; ++sx)
                {
                    const size_t p = ((sourceY + samples * y + sy) * 256 + sourceX + samples * x + sx) * 4;
                    const unsigned a = std::min(255u, unsigned(source[p + 3]) * 2);
                    // Noesis_ConvertDXT returns D3D A8R8G8B8 (BGRA bytes).
                    alpha += a; r += source[p + 2] * a; g += source[p + 1] * a; b += source[p] * a;
                }
            if (!alpha) continue;
            const unsigned a = (alpha + samples * samples / 2) / (samples * samples);
            r = r / alpha * ((tint >> 16) & 255) / 255;
            g = g / alpha * ((tint >> 8) & 255) / 255;
            b = b / alpha * (tint & 255) / 255;
            // Source-over in straight alpha, including partially covered rank stars.
            auto& destination = target[(top + y) * width + left + x];
            const unsigned background = ((destination >> 24) & 255) * (255 - a) / 255;
            const unsigned outAlpha = a + background;
            r = (r * a + ((destination >> 16) & 255) * background) / outAlpha;
            g = (g * a + ((destination >> 8) & 255) * background) / outAlpha;
            b = (b * a + (destination & 255) * background) / outAlpha;
            destination = D3DCOLOR_ARGB(outAlpha, r, g, b);
        }
}

inline void Composite(std::vector<DWORD>& target, int width, int height,
                      int left, int top, const Definition& icon, DWORD tint)
{
    CompositeSprite(target, width, height, left, top, icon.x, icon.y, 16,
        icon.tintable ? tint : 0xFFFFFFFF);
    if (icon.secondX >= 0)
        CompositeSprite(target, width, height, left + 18, top,
            icon.secondX, icon.secondY, 16, 0xFFFFFFFF);
    for (int i = 0; i < icon.stars; ++i)
        CompositeSprite(target, width, height, left + 14 - (icon.stars - i) * 6,
            top + 9, 192, 64, 8, 0xFFFFFFFF);
}

inline void CompositeJobMaster(std::vector<DWORD>& target, int width, int height, int left, int top)
{
    for (int i = 0; i < 3; ++i)
        CompositeSprite(target, width, height, left + i * 9, top, 192, 64, 8, 0xFFFFFFFF);
}
}
