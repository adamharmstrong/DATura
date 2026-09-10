#pragma once

#include <windows.h>
#include <string>

struct GameUiTitleConfig
{
    char logoDat[MAX_PATH];
    char atlasDat[MAX_PATH];
    char uiDat[MAX_PATH];
    char logoTexture[32];
    char atlasTexture[32];
    float logoSourceX;
    float logoSourceY;
    float logoSourceWidth;
    float logoSourceHeight;
    float logoWidthRatio;
    int logoMinimumWidth;
    float logoXRatio;
    float logoYRatio;
    float logoYOffsetByHeight;
    float logoAlphaScale;
    float logoMaximumOpacity;
    float menuXRatio;
    float menuYRatio;
    float menuWidthRatio;
    int menuMinimumWidth;
    float menuHeightRatio;
    int menuMinimumHeight;
    int menuGap;
    char menuFont[64];
    int menuFontMinimumHeight;
    int menuFontInset;
    COLORREF menuTextColor;
    COLORREF menuHoverTextColor;
    int menuTextShadow;
    float expansionWidthRatio;
    int expansionMinimumWidth;
    float expansionHeightRatio;
    int expansionMinimumHeight;
    float expansionXMarginRatio;
    float expansionYRatio;
    float expansionGapRatio;
    int expansionMinimumGap;
    float securedIconWidthRatio;
    int securedIconMinimumWidth;
    float securedIconXRatio;
    float securedIconBottomRatio;
    int securedIconBottomMinimum;
    float statusHeightRatio;
    int statusMinimumHeight;
    COLORREF statusColor;
    char statusFont[64];
    int statusFontMinimumHeight;
    float statusFontHeightRatio;
    COLORREF statusTextColor;
};

struct GameUiNationConfig
{
    char bastokTexture[32];
    char windurstTexture[32];
    char sandoriaTexture[32];
    float marginXRatio;
    int marginXMinimum;
    float cardGapRatio;
    int cardGapMinimum;
    float cardHeightRatio;
    int cardHeightMinimum;
    float cardYRatio;
    int cardYMinimum;
    DWORD cardColor;
    DWORD selectedCardColor;
    COLORREF cardBorderColor;
    COLORREF selectedCardBorderColor;
    int cardBorderWidth;
    float iconSizeRatio;
    int iconMinimumSize;
    int iconMaximumSize;
    int iconTopMinimum;
    float iconTopRatio;
    float titleTopRatio;
    int titleTopMinimum;
    float titleHeightRatio;
    int titleHeightMinimum;
    char titleFont[64];
    float titleFontHeightRatio;
    int titleFontMinimumHeight;
    COLORREF titleColor;
    char nameFont[64];
    float nameFontHeightRatio;
    int nameFontMinimumHeight;
    COLORREF nameColor;
    float nameTopRatio;
    int nameTopMinimum;
    float nameHeightRatio;
    int nameHeightMinimum;
    char subtitleFont[64];
    float subtitleFontHeightRatio;
    int subtitleFontMinimumHeight;
    float subtitleHeightRatio;
    int subtitleHeightMinimum;
    float descriptionMarginXRatio;
    int descriptionMarginXMinimum;
    float descriptionBottomRatio;
    int descriptionBottomMinimum;
    float descriptionHeightRatio;
    int descriptionHeightMinimum;
    COLORREF descriptionFillColor;
    COLORREF descriptionBorderColor;
    COLORREF descriptionTextColor;
    char descriptionFont[64];
    float descriptionFontHeightRatio;
    int descriptionFontMinimumHeight;
    float statusHeightRatio;
    int statusMinimumHeight;
    COLORREF statusColor;
    COLORREF statusTextColor;
    char statusFont[64];
    float statusFontHeightRatio;
    int statusFontMinimumHeight;
};

enum class PlayerSubtitleMode { None, Linkshell, Jobs };

struct GameUiPlayerNameplateConfig
{
    bool enabled;
    char name[64];
    char icon[32];
    COLORREF linkshellColor;
    bool jobMaster;
    PlayerSubtitleMode subtitleMode;
    char linkshellName[128];
    int mainJob, mainLevel, subJob, subLevel;
};

struct GameUiConfig
{
    bool loaded;
    char loadedPath[MAX_PATH];
    GameUiTitleConfig title;
    GameUiNationConfig nation;
    GameUiPlayerNameplateConfig playerNameplate;
    int chatLogTimeoutSeconds;
    int chatLogWidthPercent;
    int chatLogHeightPercent;
};

struct GameUiTitleMenuMetrics
{
    int x;
    int y;
    int width;
    int height;
};

void GameUiConfig_SetDefaults(GameUiConfig &config);
bool GameUiConfig_Load(GameUiConfig &config);
bool GameUiConfig_SavePlayerNameplate(const GameUiConfig &config);
bool GameUiConfig_SaveChatLog(const GameUiConfig &config);
// Formats per-player presentation data; levels are independent, never inferred.
std::string GameUiConfig_PlayerSubtitle(const GameUiPlayerNameplateConfig &player);
GameUiTitleMenuMetrics GameUiConfig_GetTitleMenuMetrics(const GameUiTitleConfig &config,
                                                        int viewportWidth, int viewportHeight);
int GameUiConfig_GetTitleMenuButtonIndex(const GameUiTitleConfig &config,
                                         int viewportWidth, int viewportHeight, POINT point);
RECT GameUiConfig_GetNationCardRect(const GameUiNationConfig &config,
                                    int viewportWidth, int viewportHeight, int index);
int GameUiConfig_GetNationCardIndex(const GameUiNationConfig &config,
                                    int viewportWidth, int viewportHeight,
                                    int cardCount, POINT point);
