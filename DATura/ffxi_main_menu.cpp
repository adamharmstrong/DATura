#include "stdafx.h"
#include "ffxi_main_menu.h"
#include "win32_drawing.h"
#include "ffxi_title_ui_primitives.h"
#include "ffxi_title_assets.h"
#include "d3d_ui_renderer.h"
// Need the full definition of noesisTex_t when accessing texture fields
#include "noesis_rapi.h"

#include <algorithm>

namespace
{
const char* const kPageItems[][14] = {
    { "Status", "Equipment", "Magic", "Items", "Synthesis", "Abilities", "Party",
      "Trade", "Search", "Linkshell", "Friend List", "Region Info", "Map", "Mog House" },
    { "Missions", "Quests", "Key Items", "View House", "Bazaar", "Macros", "Config",
      "Help Desk", "Current Time", "Communication", "Shut Down", "Log Out" }
};
constexpr int kPageItemCounts[] = { 14, 12 };
}

namespace FFXIMainMenu
{
void DrawTextures(IDirect3DDevice9* device, const bool enableMipMapping,
                  noesisModel_t* uiModel, const State& state,
                  const int width, const int height, const bool showMogHouse)
{
    if (!device || !state.open || !uiModel) return;
    noesisTex_t* button = FFXITitleAssets::FindTitleTexture(uiModel, "buttonto");
    if (!button) button = FFXITitleAssets::FindTitleTexture(uiModel, "lrbutton");
    if (!button) return;
    const int rowHeight = std::max(29, height / 27);
    const int menuWidth = 190;
    const int left = width - menuWidth - std::max(24, width / 16);
    const int top = std::max(24, height / 7);
    const int titleHeight = rowHeight + 4;
    const int itemCount = kPageItemCounts[state.page] -
        (state.page == 0 && !showMogHouse ? 1 : 0);
    const int panelHeight = titleHeight + 4 + rowHeight * kPageItemCounts[0] + 10;
    // Prefer the dedicated menu background atlas (menu2fon) which contains
    // the horizontal blue stripe artwork. Fall back to the older ustatshd
    // atlas for installations that don't have the newer texture.
    // Ignore any installed atlas and draw a simple striped background so the
    // menu always uses horizontal blue / dark-blue bands rather than an
    // unintended icon sheet.
    const int stripeHeight = 2; // fixed 2-pixel stripes as requested
    for (int y = 0; y < panelHeight; y += stripeHeight)
    {
        const DWORD color = ((y / stripeHeight) & 1) == 0
            ? 0xFF2A4B7A : 0xFF16273B; // lighter and darker blue bands
        D3DUiRenderer::DrawSolidQuad(device, static_cast<float>(left),
            static_cast<float>(top + y), static_cast<float>(menuWidth),
            static_cast<float>((std::min)(stripeHeight, panelHeight - y)), color);
    }
    D3DUiRenderer::DrawSolidQuad(device, static_cast<float>(left), static_cast<float>(top),
        static_cast<float>(menuWidth), 2.0f, 0xFFE4E8F0);
    D3DUiRenderer::DrawSolidQuad(device, static_cast<float>(left),
        static_cast<float>(top + panelHeight - 2), static_cast<float>(menuWidth), 2.0f,
        0xFFE4E8F0);
    for (int i = 0; i < itemCount; ++i)
    {
        const int row = state.page == 0 && !showMogHouse && i >= 13 ? i - 1 : i;
        FFXITitleUiPrimitives::DrawButton(device, enableMipMapping, button,
            static_cast<float>(left + 10), static_cast<float>(top + titleHeight + 4 + row * rowHeight),
            static_cast<float>(menuWidth - 20), static_cast<float>(rowHeight - 2), i == state.selected);
    }
}

static int MenuWidth(HDC dc, int rowHeight)
{
    HFONT font = CreateFontA(-std::max(18, rowHeight - 8), 0, 0, 0, FW_NORMAL,
                             FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Arial");
    HGDIOBJ oldFont = SelectObject(dc, font);
    int textWidth = 0;
    for (int page = 0; page < 2; ++page)
    for (int i = 0; i < kPageItemCounts[page]; ++i)
    {
        SIZE size = {};
        GetTextExtentPoint32A(dc, kPageItems[page][i], lstrlenA(kPageItems[page][i]), &size);
        textWidth = std::max(textWidth, static_cast<int>(size.cx));
    }
    SelectObject(dc, oldFont);
    DeleteObject(font);
    return std::max(150, textWidth + 30);
}

Action HandleKey(State& state, const unsigned int key)
{
    if (key == VK_ESCAPE)
    {
        if (!state.open) { state.open = true; state.page = 0; state.selected = 0; return Action::Opened; }
        state.open = false;
        return Action::Closed;
    }
    if (!state.open) return Action::None;
    if (key == VK_LEFT || key == VK_RIGHT)
    {
        state.page = key == VK_RIGHT ? 1 : 0;
        state.selected = 0;
        return Action::None;
    }
    const int itemCount = kPageItemCounts[state.page];
    if (key == VK_UP) { state.selected = (state.selected + itemCount - 1) % itemCount; return Action::None; }
    if (key == VK_DOWN) { state.selected = (state.selected + 1) % itemCount; return Action::None; }
    if (key == VK_RETURN) return Action::Activated;
    return Action::None;
}

void Draw(HDC dc, const State& state, const int width, const int height, const bool showMogHouse)
{
    if (!dc || !state.open) return;

    const int rightMargin = std::max(24, width / 16);
    const int top = std::max(24, height / 7);
    const int rowHeight = std::max(29, height / 27);
    const int itemCount = kPageItemCounts[state.page] -
        (state.page == 0 && !showMogHouse ? 1 : 0);
    HFONT font = CreateFontA(-std::max(18, rowHeight - 8), 0, 0, 0, FW_NORMAL,
                             FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Arial");
    HGDIOBJ oldFont = SelectObject(dc, font);
    const int menuWidth = MenuWidth(dc, rowHeight);
    const int left = width - menuWidth - rightMargin;
    const int titleHeight = rowHeight + 4;
    const int firstItemTop = top + titleHeight + 4;
    // Both pages use the same footprint; this is intentional and matches the
    // stable page-to-page frame in the original UI.
    const int tabLeft = left + 10;
    const int tabWidth = (menuWidth - 20) / 2;
    for (int tabIndex = 0; tabIndex < 2; ++tabIndex)
    {
        RECT tab = { tabLeft + tabIndex * tabWidth, top + 4,
                     tabLeft + (tabIndex + 1) * tabWidth, top + titleHeight };
        if (tabIndex == state.page)
        {
            HBRUSH tabBrush = CreateSolidBrush(RGB(48, 79, 124));
            FillRect(dc, &tab, tabBrush);
            DeleteObject(tabBrush);
        }
        char label[8] = {};
        sprintf_s(label, "Page %d", tabIndex + 1);
        Win32Drawing::DrawShadowText(dc, font, label, tab,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE,
            tabIndex == state.page ? RGB(255, 226, 102) : RGB(190, 210, 240), 2);
    }

    for (int i = 0; i < itemCount; ++i)
    {
        const char* item = kPageItems[state.page][i];
        if (state.page == 0 && !showMogHouse && i >= 13)
            continue;
        RECT row = { left + 10, firstItemTop + i * rowHeight,
                     left + menuWidth - 10, firstItemTop + (i + 1) * rowHeight };
        Win32Drawing::DrawShadowText(dc, font, item, row,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE,
            i == state.selected ? RGB(255, 226, 102) : RGB(235, 240, 255), 2);
    }
    SelectObject(dc, oldFont);
    DeleteObject(font);
}

bool ClickTab(State& state, HDC dc, const int width, const int height, const POINT point)
{
    if (!state.open || !dc) return false;
    const int rowHeight = std::max(29, height / 27);
    const int menuWidth = MenuWidth(dc, rowHeight);
    const int rightMargin = std::max(24, width / 16);
    const int left = width - menuWidth - rightMargin;
    const int top = std::max(24, height / 7);
    const int titleHeight = rowHeight + 4;
    const int tabLeft = left + 10;
    const int tabWidth = (menuWidth - 20) / 2;
    if (point.y < top + 4 || point.y >= top + titleHeight ||
        point.x < tabLeft || point.x >= tabLeft + tabWidth * 2)
        return false;
    state.page = (point.x - tabLeft) / tabWidth;
    state.selected = 0;
    return true;
}
}
