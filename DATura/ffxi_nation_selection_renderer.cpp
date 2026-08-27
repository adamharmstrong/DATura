#include "stdafx.h"
#include "ffxi_nation_selection_renderer.h"

#include "d3d9_device.h"
#include "d3d_ui_renderer.h"
#include "ffxi_nation_selection.h"
#include "ffxi_title_assets.h"
#include "noesis_rapi.h"
#include "win32_drawing.h"

#include <algorithm>

namespace FFXINationSelectionRenderer
{
void DrawTextures(const Context& context)
{
    if (!context.window)
        return;

    int width = 0;
    int height = 0;
    D3D9Device::GetViewportOrClientSize(context.device, context.window, &width, &height);
    if (width <= 0 || height <= 0)
        return;

    const GameUiNationConfig& ui = context.config;
    const char* crestTextures[] =
    {
        ui.bastokTexture, ui.windurstTexture, ui.sandoriaTexture
    };
    for (int index = 0; index < FFXINationSelection::Count(); ++index)
    {
        RECT card = GameUiConfig_GetNationCardRect(ui, width, height, index);
        const bool selected = index == context.selectedIndex;
        D3DUiRenderer::DrawSolidQuad(
            context.device, (float)card.left, (float)card.top,
            (float)(card.right - card.left), (float)(card.bottom - card.top),
            selected ? ui.selectedCardColor : ui.cardColor);

        noesisTex_t* crest = FFXITitleAssets::FindTitleTexture(
            context.titleUiModel, crestTextures[index]);
        if (!crest || !crest->pD3DTex)
            continue;

        const int crestAvailable = std::max<int>(32, card.right - card.left - 48);
        const int configuredSize = std::max(
            ui.iconMinimumSize, (int)((float)height * ui.iconSizeRatio));
        const float crestSize = (float)std::min(
            crestAvailable, std::min(ui.iconMaximumSize, configuredSize));
        const float crestX = (float)(card.left + card.right) * 0.5f - crestSize * 0.5f;
        const float crestY = (float)card.top +
            (float)std::max(ui.iconTopMinimum, (int)((float)height * ui.iconTopRatio));
        D3DUiRenderer::DrawTextureRegion(
            context.device, context.enableMipMapping, crest, crestX, crestY, crestSize, crestSize,
            0.0f, 0.0f, (float)crest->w, (float)crest->h, 0xFFFFFFFF);
    }
}

void DrawOverlay(const Context& context)
{
    if (!context.window)
        return;

    int width = 0;
    int height = 0;
    D3D9Device::GetViewportOrClientSize(context.device, context.window, &width, &height);
    if (width <= 0 || height <= 0)
        return;

    HDC hdc = GetDC(context.window);
    if (!hdc)
        return;
    const int savedDc = D3D9Device::BeginGdiViewportMapping(
        hdc, context.window, width, height);

    const GameUiNationConfig& ui = context.config;
    HFONT titleFont = CreateFontA(
        -std::max(ui.titleFontMinimumHeight, (int)((float)height * ui.titleFontHeightRatio)),
        0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_ROMAN, ui.titleFont);
    const int titleTop = std::max(ui.titleTopMinimum, (int)((float)height * ui.titleTopRatio));
    RECT titleBounds =
    {
        0, titleTop, width,
        titleTop + std::max(ui.titleHeightMinimum, (int)((float)height * ui.titleHeightRatio))
    };
    Win32Drawing::DrawShadowText(
        hdc, titleFont, "Choose Your Starting Nation", titleBounds,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE, ui.titleColor, 2);
    DeleteObject(titleFont);

    HFONT nameFont = CreateFontA(
        -std::max(ui.nameFontMinimumHeight, (int)((float)height * ui.nameFontHeightRatio)),
        0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_ROMAN, ui.nameFont);
    HFONT subtitleFont = CreateFontA(
        -std::max(ui.subtitleFontMinimumHeight, (int)((float)height * ui.subtitleFontHeightRatio)),
        0, 0, 0, FW_NORMAL, TRUE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, ui.subtitleFont);
    HFONT bodyFont = CreateFontA(
        -std::max(ui.descriptionFontMinimumHeight, (int)((float)height * ui.descriptionFontHeightRatio)),
        0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, ui.descriptionFont);

    for (int index = 0; index < FFXINationSelection::Count(); ++index)
    {
        const FFXINationSelection::Info& nation = FFXINationSelection::InfoForIndex(index);
        RECT card = GameUiConfig_GetNationCardRect(ui, width, height, index);
        const bool selected = index == context.selectedIndex;
        HBRUSH cardBrush = (HBRUSH)GetStockObject(HOLLOW_BRUSH);
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, cardBrush);
        HPEN pen = CreatePen(
            PS_SOLID, selected ? std::max(1, ui.cardBorderWidth) : 1,
            selected ? ui.selectedCardBorderColor : ui.cardBorderColor);
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);
        RoundRect(hdc, card.left, card.top, card.right, card.bottom, 8, 8);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(pen);

        const int nameTop = card.top + std::max(
            ui.nameTopMinimum, (int)((float)height * ui.nameTopRatio));
        RECT nameBounds =
        {
            card.left + 16, nameTop, card.right - 16,
            nameTop + std::max(ui.nameHeightMinimum, (int)((float)height * ui.nameHeightRatio))
        };
        Win32Drawing::DrawShadowText(
            hdc, nameFont, nation.name, nameBounds,
            DT_CENTER | DT_WORDBREAK, ui.nameColor, 1);

        RECT subtitleBounds =
        {
            card.left + 18, nameBounds.bottom + 2, card.right - 18,
            nameBounds.bottom + std::max(
                ui.subtitleHeightMinimum, (int)((float)height * ui.subtitleHeightRatio))
        };
        Win32Drawing::DrawShadowText(
            hdc, subtitleFont, nation.subtitle, subtitleBounds,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE, nation.accent, 1);
    }

    const int descriptionMargin = std::max(
        ui.descriptionMarginXMinimum, (int)((float)width * ui.descriptionMarginXRatio));
    const int descriptionBottom = std::max(
        ui.descriptionBottomMinimum, (int)((float)height * ui.descriptionBottomRatio));
    const int descriptionHeight = std::max(
        ui.descriptionHeightMinimum, (int)((float)height * ui.descriptionHeightRatio));
    RECT descriptionBounds =
    {
        descriptionMargin, height - descriptionHeight,
        width - descriptionMargin, height - descriptionBottom
    };
    HBRUSH descriptionBrush = CreateSolidBrush(ui.descriptionFillColor);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, descriptionBrush);
    HPEN descriptionPen = CreatePen(PS_SOLID, 1, ui.descriptionBorderColor);
    HPEN oldPen = (HPEN)SelectObject(hdc, descriptionPen);
    RoundRect(hdc, descriptionBounds.left, descriptionBounds.top,
              descriptionBounds.right, descriptionBounds.bottom, 8, 8);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(descriptionPen);
    DeleteObject(descriptionBrush);

    RECT bodyBounds =
    {
        descriptionBounds.left + 24, descriptionBounds.top + 18,
        descriptionBounds.right - 24, descriptionBounds.bottom - 18
    };
    Win32Drawing::DrawShadowText(
        hdc, bodyFont,
        FFXINationSelection::InfoForIndex(context.selectedIndex).description, bodyBounds,
        DT_CENTER | DT_VCENTER | DT_WORDBREAK, ui.descriptionTextColor, 1);

    HFONT statusFont = CreateFontA(
        -std::max(ui.statusFontMinimumHeight, (int)((float)height * ui.statusFontHeightRatio)),
        0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_MODERN, ui.statusFont);
    RECT statusBounds =
    {
        0, height - std::max(ui.statusMinimumHeight, (int)((float)height * ui.statusHeightRatio)),
        width, height
    };
    HBRUSH statusBrush = CreateSolidBrush(ui.statusColor);
    FillRect(hdc, &statusBounds, statusBrush);
    DeleteObject(statusBrush);
    Win32Drawing::DrawShadowText(
        hdc, statusFont,
        "Click a nation to select it. Press Enter to confirm, or Backspace to return.",
        statusBounds, DT_CENTER | DT_VCENTER | DT_SINGLELINE, ui.statusTextColor, 1);
    DeleteObject(statusFont);

    DeleteObject(bodyFont);
    DeleteObject(subtitleFont);
    DeleteObject(nameFont);
    if (savedDc)
        RestoreDC(hdc, savedDc);
    ReleaseDC(context.window, hdc);
}
}
