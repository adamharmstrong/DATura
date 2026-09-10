#include "stdafx.h"
#include "ffxi_title_screen_renderer.h"

#include "d3d9_device.h"
#include "d3d_ui_renderer.h"
#include "ffxi_title_assets.h"
#include "ffxi_title_ui_primitives.h"
#include "noesis_rapi.h"
#include "win32_drawing.h"

#include <algorithm>

namespace FFXITitleScreenRenderer
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

    const GameUiTitleConfig& ui = context.config;
    noesisTex_t* logoTextTexture = FFXITitleAssets::FindTitleTexture(
        context.logoModel, ui.logoTexture);
    noesisTex_t* titleAtlasTexture = FFXITitleAssets::FindTitleTexture(
        context.logoMarkModel, ui.atlasTexture);
    noesisTex_t* logoGraphicTexture = FFXITitleAssets::FindTitleTexture(
        context.logoMarkModel, "xilogo");
    if (titleAtlasTexture && titleAtlasTexture->pD3DTex)
    {
        const float sourceX = ui.logoSourceX;
        const float sourceY = ui.logoSourceY;
        const float sourceWidth = ui.logoSourceWidth;
        const float sourceHeight = ui.logoSourceHeight;
        const float logoWidth = (float)std::max(
            ui.logoMinimumWidth, (int)((float)width * ui.logoWidthRatio));
        const float logoHeight = logoWidth * (sourceHeight / sourceWidth);
        const float logoX = (float)width * ui.logoXRatio;
        const float logoY = (float)height * ui.logoYRatio +
            logoHeight * ui.logoYOffsetByHeight;
        D3DUiRenderer::DrawTexturedQuadUV(
            context.device, context.enableMipMapping, titleAtlasTexture->pD3DTex,
            logoX, logoY, logoWidth, logoHeight,
            sourceX / (float)titleAtlasTexture->w,
            sourceY / (float)titleAtlasTexture->h,
            (sourceX + sourceWidth) / (float)titleAtlasTexture->w,
            (sourceY + sourceHeight) / (float)titleAtlasTexture->h,
            0xFFFFFFFF, D3DUiRenderer::UsesFfxiDxt3Alpha(titleAtlasTexture),
            ui.logoAlphaScale, ui.logoMaximumOpacity);
    }
    else if (logoGraphicTexture && logoGraphicTexture->pD3DTex)
    {
        const float graphicWidth = (float)std::max(650, (width * 55) / 100);
        const float graphicHeight = graphicWidth *
            ((float)logoGraphicTexture->h / (float)logoGraphicTexture->w);
        const float graphicX = (float)(width / 7) - graphicWidth * 0.02f;
        const float graphicY = (float)(height / 3) - graphicHeight * 0.70f;
        D3DUiRenderer::DrawTexturedQuad(
            context.device, context.enableMipMapping,
            logoGraphicTexture, graphicX, graphicY, graphicWidth, graphicHeight,
            0xFFFFFFFF);
    }

    if (!titleAtlasTexture && logoTextTexture && logoTextTexture->pD3DTex)
    {
        const float logoWidth = (float)std::max(560, (width * 48) / 100);
        const float logoHeight = logoWidth *
            ((float)logoTextTexture->h / (float)logoTextTexture->w);
        const float logoX = (float)(width / 7);
        const float logoY = (float)(height / 3) - logoHeight * 0.05f;
        D3DUiRenderer::DrawTexturedQuad(
            context.device, context.enableMipMapping,
            logoTextTexture, logoX, logoY, logoWidth, logoHeight, 0xFFFFFFFF);
    }

    const float plaqueWidth = (float)std::max(
        ui.expansionMinimumWidth, (int)((float)width * ui.expansionWidthRatio));
    const float plaqueHeight = (float)std::max(
        ui.expansionMinimumHeight, (int)((float)height * ui.expansionHeightRatio));
    const float plaqueGap = (float)std::max(
        ui.expansionMinimumGap, (int)((float)height * ui.expansionGapRatio));
    const float plaqueX = (float)width - plaqueWidth -
        (float)width * ui.expansionXMarginRatio;
    const float plaqueY = (float)height * ui.expansionYRatio;
    noesisTex_t* expansion1Texture = FFXITitleAssets::FindTitleTexture(
        context.logoMarkModel, "ex1us");
    noesisTex_t* expansion2Texture = FFXITitleAssets::FindTitleTexture(
        context.logoMarkModel, "ex2us");
    noesisTex_t* expansion5Texture = FFXITitleAssets::FindTitleTexture(
        context.logoMarkModel, "ex5us");
    FFXITitleUiPrimitives::DrawExpansionAtlas(
        context.device, context.enableMipMapping, expansion1Texture, 5, 0,
        plaqueX, plaqueY, plaqueWidth, plaqueHeight, plaqueGap);
    FFXITitleUiPrimitives::DrawExpansionAtlas(
        context.device, context.enableMipMapping, expansion2Texture, 5, 5,
        plaqueX, plaqueY, plaqueWidth, plaqueHeight, plaqueGap);
    if (expansion5Texture)
    {
        D3DUiRenderer::DrawTextureRegion(
            context.device, context.enableMipMapping, expansion5Texture,
            plaqueX, plaqueY + (plaqueHeight + plaqueGap) * 10.0f,
            plaqueWidth, plaqueHeight, 0.0f, 0.0f,
            (float)expansion5Texture->w, (float)expansion5Texture->h, 0xFFFFFFFF);
    }

    noesisTex_t* securedTexture = FFXITitleAssets::FindTitleTexture(
        context.logoMarkModel, "otp");
    if (!securedTexture)
        securedTexture = FFXITitleAssets::FindTitleTexture(context.titleUiModel, "otp");
    if (securedTexture)
    {
        const float iconWidth = (float)std::max(
            ui.securedIconMinimumWidth, (int)((float)width * ui.securedIconWidthRatio));
        const float iconHeight = iconWidth *
            ((float)securedTexture->h / (float)securedTexture->w);
        D3DUiRenderer::DrawTextureRegion(
            context.device, context.enableMipMapping, securedTexture,
            (float)width * ui.securedIconXRatio,
            (float)height - std::max(
                ui.securedIconBottomMinimum,
                (int)((float)height * ui.securedIconBottomRatio)),
            iconWidth, iconHeight, 0.0f, 0.0f,
            (float)securedTexture->w, (float)securedTexture->h, 0xFFFFFFFF);
    }

    noesisTex_t* buttonTexture = FFXITitleAssets::FindTitleTexture(
        context.titleUiModel, "buttonto");
    if (!buttonTexture)
        buttonTexture = FFXITitleAssets::FindTitleTexture(context.titleUiModel, "lrbutton");
    if (buttonTexture && buttonTexture->pD3DTex)
    {
        const GameUiTitleMenuMetrics menuMetrics =
            GameUiConfig_GetTitleMenuMetrics(ui, width, height);
        const int buttonX = menuMetrics.x;
        int buttonY = menuMetrics.y;
        const int buttonWidth = menuMetrics.width;
        const int buttonHeight = menuMetrics.height;
        const int selectedButton = GameUiConfig_GetTitleMenuButtonIndex(
            ui, width, height, D3D9Device::MapClientPointToViewport(
                context.window, width, height, context.mouseClient));
        for (int index = 0; index < 5; ++index)
        {
            FFXITitleUiPrimitives::DrawButton(
                context.device, context.enableMipMapping, buttonTexture,
                (float)buttonX, (float)buttonY,
                (float)buttonWidth, (float)buttonHeight, index == selectedButton);
            buttonY += buttonHeight + ui.menuGap;
        }
    }

    if (titleAtlasTexture && titleAtlasTexture->pD3DTex)
    {
        const GameUiTitleMenuMetrics menuMetrics =
            GameUiConfig_GetTitleMenuMetrics(ui, width, height);
        const int buttonX = menuMetrics.x;
        int buttonY = menuMetrics.y;
        const int buttonWidth = menuMetrics.width;
        const int buttonHeight = menuMetrics.height;
        buttonY += (buttonHeight + ui.menuGap) * 5;

        const float sourceX = 670.0f;
        const float sourceY = 20.0f;
        const float sourceWidth = 295.0f;
        const float sourceHeight = 32.0f;
        const float copyrightWidth = (float)std::max(235, (buttonWidth * 11) / 10);
        const float copyrightHeight = copyrightWidth * (sourceHeight / sourceWidth);
        const float copyrightX = (float)buttonX +
            ((float)buttonWidth - copyrightWidth) * 0.5f;
        const float copyrightY = (float)buttonY +
            (float)std::max(12, height / 80);
        D3DUiRenderer::DrawTextureRegion(
            context.device, context.enableMipMapping, titleAtlasTexture,
            copyrightX, copyrightY, copyrightWidth, copyrightHeight,
            sourceX, sourceY, sourceWidth, sourceHeight, 0xFFFFFFFF);
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

    const bool ownsDc = context.overlayDc == nullptr;
    HDC hdc = ownsDc ? GetDC(context.window) : context.overlayDc;
    if (!hdc)
        return;
    const int savedDc = ownsDc ?
        D3D9Device::BeginGdiViewportMapping(hdc, context.window, width, height) :
        SaveDC(hdc);
    const GameUiTitleConfig& ui = context.config;

    const bool haveRealLogo =
        (FFXITitleAssets::FindTitleTexture(context.logoModel, ui.logoTexture) != nullptr) ||
        (FFXITitleAssets::FindTitleTexture(context.logoMarkModel, ui.atlasTexture) != nullptr);
    const bool haveRealButton =
        (FFXITitleAssets::FindTitleTexture(context.titleUiModel, "buttonto") != nullptr) ||
        (FFXITitleAssets::FindTitleTexture(context.titleUiModel, "lrbutton") != nullptr);
    const bool haveRealCopyright =
        FFXITitleAssets::FindTitleTexture(context.logoMarkModel, ui.atlasTexture) != nullptr;
    const bool haveRealExpansions =
        (FFXITitleAssets::FindTitleTexture(context.logoMarkModel, "ex1us") != nullptr) &&
        (FFXITitleAssets::FindTitleTexture(context.logoMarkModel, "ex2us") != nullptr) &&
        (FFXITitleAssets::FindTitleTexture(context.logoMarkModel, "ex5us") != nullptr);

    const int logoTop = height / 3;
    RECT logoBounds =
    {
        width / 12, logoTop, (width * 7) / 10,
        logoTop + std::max(76, height / 7)
    };
    if (!haveRealLogo)
    {
        HFONT logoFont = CreateFontA(
            -std::max(64, height / 8), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_ROMAN, "Times New Roman");
        Win32Drawing::DrawShadowText(
            hdc, logoFont, "FINAL FANTASY XI", logoBounds,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(248, 248, 248), 3);
        DeleteObject(logoFont);

        RECT onlineBounds =
        {
            logoBounds.left, logoBounds.bottom - 10, logoBounds.right,
            logoBounds.bottom + std::max(28, height / 32)
        };
        HFONT onlineFont = CreateFontA(
            -std::max(18, height / 36), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_ROMAN, "Georgia");
        Win32Drawing::DrawShadowText(
            hdc, onlineFont, "O N L I N E", onlineBounds,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(255, 255, 255), 2);
        DeleteObject(onlineFont);
    }

    const GameUiTitleMenuMetrics menuMetrics =
        GameUiConfig_GetTitleMenuMetrics(ui, width, height);
    const int buttonX = menuMetrics.x;
    int buttonY = menuMetrics.y;
    const int buttonWidth = menuMetrics.width;
    const int buttonHeight = menuMetrics.height;
    const int selectedButton = GameUiConfig_GetTitleMenuButtonIndex(
        ui, width, height, D3D9Device::MapClientPointToViewport(
            context.window, width, height, context.mouseClient));
    const char* buttons[] =
    {
        "Select Character", "Create Character", "Delete Character", "Config", "Back"
    };
    const char* helpText[] =
    {
        "Select a character to log on with.",
        "Create a new character.",
        "Delete a character.",
        "Change the game's options.",
        "Return to the previous screen.",
    };
    HFONT buttonFont = CreateFontA(
        -std::max(ui.menuFontMinimumHeight, buttonHeight - ui.menuFontInset),
        0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, ui.menuFont);
    for (int index = 0; index < 5; ++index)
    {
        RECT buttonBounds =
        {
            buttonX, buttonY, buttonX + buttonWidth, buttonY + buttonHeight
        };
        if (haveRealButton)
        {
            const COLORREF textColor = index == selectedButton
                ? ui.menuHoverTextColor
                : ui.menuTextColor;
            Win32Drawing::DrawShadowText(
                hdc, buttonFont, buttons[index], buttonBounds,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE, textColor, ui.menuTextShadow);
        }
        else
        {
            Win32Drawing::DrawTitleMenuButton(
                hdc, buttonBounds, buttons[index], index == selectedButton);
        }
        buttonY += buttonHeight + ui.menuGap;
    }
    DeleteObject(buttonFont);

    if (!haveRealCopyright)
    {
        RECT copyrightBounds =
        {
            buttonX - 20, buttonY + 12, buttonX + buttonWidth + 20, buttonY + 40
        };
        HFONT copyrightFont = CreateFontA(
            -std::max(18, height / 34), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS, "Arial");
        Win32Drawing::DrawShadowText(
            hdc, copyrightFont, "(C) SQUARE ENIX", copyrightBounds,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(255, 255, 255), 2);
        DeleteObject(copyrightFont);
    }

    if (!haveRealExpansions)
    {
        const int plaqueWidth = std::max(
            ui.expansionMinimumWidth, (int)((float)width * ui.expansionWidthRatio));
        const int plaqueHeight = std::max(
            ui.expansionMinimumHeight, (int)((float)height * ui.expansionHeightRatio));
        const int plaqueX = width - plaqueWidth -
            (int)((float)width * ui.expansionXMarginRatio);
        int plaqueY = (int)((float)height * ui.expansionYRatio);
        struct Plaque
        {
            const char* label;
            COLORREF color;
        };
        const Plaque plaques[] =
        {
            { "Rise of the Zilart", RGB(84, 56, 26) },
            { "Chains of Promathia", RGB(73, 58, 84) },
            { "Treasures of Aht Urhgan", RGB(110, 78, 28) },
            { "Wings of the Goddess", RGB(41, 61, 92) },
            { "A Crystalline Prophecy", RGB(22, 87, 180) },
            { "A Moogle Kupo d'Etat", RGB(181, 26, 42) },
            { "A Shantotto Ascension", RGB(98, 54, 31) },
            { "Vision of Abyssea", RGB(0, 124, 179) },
            { "Scars of Abyssea", RGB(98, 64, 142) },
            { "Heroes of Abyssea", RGB(142, 36, 46) },
            { "Seekers of Adoulin", RGB(72, 132, 52) },
        };
        for (int index = 0; index < (int)(sizeof(plaques) / sizeof(plaques[0])); ++index)
        {
            RECT plaqueBounds =
            {
                plaqueX, plaqueY, plaqueX + plaqueWidth, plaqueY + plaqueHeight
            };
            Win32Drawing::DrawExpansionPlaque(
                hdc, plaqueBounds, plaques[index].label, plaques[index].color);
            plaqueY += plaqueHeight + std::max(
                ui.expansionMinimumGap,
                (int)((float)height * ui.expansionGapRatio));
        }
    }

    RECT statusBounds =
    {
        0, height - std::max(
            ui.statusMinimumHeight, (int)((float)height * ui.statusHeightRatio)),
        width, height
    };
    HBRUSH statusBrush = CreateSolidBrush(ui.statusColor);
    FillRect(hdc, &statusBounds, statusBrush);
    DeleteObject(statusBrush);

    HFONT statusFont = CreateFontA(
        -std::max(
            ui.statusFontMinimumHeight,
            (int)((float)height * ui.statusFontHeightRatio)),
        0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_MODERN, ui.statusFont);
    const char* statusText = selectedButton >= 0 ? helpText[selectedButton] : helpText[0];
    Win32Drawing::DrawShadowText(
        hdc, statusFont, statusText, statusBounds,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE, ui.statusTextColor, 1);
    DeleteObject(statusFont);

    if (savedDc)
        RestoreDC(hdc, savedDc);
    if (ownsDc)
        ReleaseDC(context.window, hdc);
}
}
