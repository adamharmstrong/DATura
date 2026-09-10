#include "stdafx.h"
#include "game_ui_config.h"
#include "ffxi_stat_system.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace
{
static const char kConfigFileName[] = "DATura.game-ui.config";

static void CopyText(char *dst, size_t dstSize, const char *src)
{
    if (!dst || dstSize == 0)
        return;
    strcpy_s(dst, dstSize, src ? src : "");
}

static bool FileExists(const char *path)
{
    const DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

static bool FindConfigPath(char *outPath, size_t outPathSize)
{
    char modulePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, modulePath, MAX_PATH) > 0)
    {
        char *slash = strrchr(modulePath, '\\');
        if (slash)
        {
            slash[1] = 0;
            strcat_s(modulePath, kConfigFileName);
            if (FileExists(modulePath))
            {
                CopyText(outPath, outPathSize, modulePath);
                return true;
            }
        }
    }

    char currentPath[MAX_PATH] = {};
    if (GetCurrentDirectoryA(MAX_PATH, currentPath) > 0)
    {
        char candidate[MAX_PATH] = {};
        sprintf_s(candidate, "%s\\%s", currentPath, kConfigFileName);
        if (FileExists(candidate))
        {
            CopyText(outPath, outPathSize, candidate);
            return true;
        }

        sprintf_s(candidate, "%s\\DATura\\%s", currentPath, kConfigFileName);
        if (FileExists(candidate))
        {
            CopyText(outPath, outPathSize, candidate);
            return true;
        }
    }
    return false;
}

static void ReadString(const char *path, const char *section, const char *key,
                       const char *fallback, char *out, size_t outSize)
{
    char defaultText[MAX_PATH] = {};
    CopyText(defaultText, sizeof(defaultText), fallback);
    GetPrivateProfileStringA(section, key, defaultText, out, (DWORD)outSize, path);
}

static int ReadInt(const char *path, const char *section, const char *key, int fallback)
{
    char defaultText[32] = {};
    char value[32] = {};
    sprintf_s(defaultText, "%d", fallback);
    ReadString(path, section, key, defaultText, value, sizeof(value));
    return atoi(value);
}

static int ReadJob(const char* path, const char* key, int fallback, bool allowNone)
{
    char value[16] = {};
    ReadString(path, "Player.Nameplate", key,
        FFXIStats::JobName(static_cast<FFXIStats::Job>(fallback)), value, sizeof(value));
    for (int job = allowNone ? 0 : 1; job < FFXIStats::kJob_Count; ++job)
        if (_stricmp(value, FFXIStats::JobName(static_cast<FFXIStats::Job>(job))) == 0) return job;
    return fallback;
}

static float ReadFloat(const char *path, const char *section, const char *key, float fallback)
{
    char defaultText[32] = {};
    char value[32] = {};
    sprintf_s(defaultText, "%.6g", fallback);
    ReadString(path, section, key, defaultText, value, sizeof(value));
    return (float)atof(value);
}

static unsigned long ReadUnsigned(const char *path, const char *section, const char *key,
                                  unsigned long fallback)
{
    char defaultText[32] = {};
    char value[32] = {};
    sprintf_s(defaultText, "0x%08lX", fallback);
    ReadString(path, section, key, defaultText, value, sizeof(value));
    char *end = nullptr;
    const unsigned long parsed = strtoul(value, &end, 0);
    return (end && end != value) ? parsed : fallback;
}

static COLORREF ReadRgb(const char *path, const char *section, const char *key, COLORREF fallback)
{
    char defaultText[32] = {};
    char value[64] = {};
    sprintf_s(defaultText, "%u,%u,%u", GetRValue(fallback), GetGValue(fallback), GetBValue(fallback));
    ReadString(path, section, key, defaultText, value, sizeof(value));
    int r = 0, g = 0, b = 0;
    if (sscanf_s(value, "%d,%d,%d", &r, &g, &b) != 3)
        return fallback;
    return RGB(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255));
}
}

void GameUiConfig_SetDefaults(GameUiConfig &config)
{
    ZeroMemory(&config, sizeof(config));
    config.chatLogTimeoutSeconds = 15;
    config.chatLogWidthPercent = 50;
    config.chatLogHeightPercent = 25;
    config.playerNameplate.enabled = true;
    config.playerNameplate.linkshellColor = RGB(160, 128, 224);
    config.playerNameplate.subtitleMode = PlayerSubtitleMode::Linkshell;
    config.playerNameplate.mainJob = FFXIStats::kJob_WAR;
    config.playerNameplate.mainLevel = 1;
    config.playerNameplate.subLevel = 1;
    GameUiTitleConfig &t = config.title;
    CopyText(t.logoDat, sizeof(t.logoDat), "ROM6/0/96.DAT");
    CopyText(t.atlasDat, sizeof(t.atlasDat), "ROM/119/50.DAT");
    CopyText(t.uiDat, sizeof(t.uiDat), "ROM/119/51.DAT");
    CopyText(t.logoTexture, sizeof(t.logoTexture), "ffxi");
    CopyText(t.atlasTexture, sizeof(t.atlasTexture), "titlwin");
    t.logoSourceX = 36.0f; t.logoSourceY = 310.0f;
    t.logoSourceWidth = 935.0f; t.logoSourceHeight = 520.0f;
    t.logoWidthRatio = 0.58f; t.logoMinimumWidth = 760;
    t.logoXRatio = 1.0f / 9.0f; t.logoYRatio = 1.0f / 3.0f; t.logoYOffsetByHeight = -0.60f;
    t.logoAlphaScale = 1.5f; t.logoMaximumOpacity = 0.90f;
    t.menuXRatio = 0.345f; t.menuYRatio = 0.57f;
    t.menuWidthRatio = 1.0f / 7.0f; t.menuMinimumWidth = 230;
    t.menuHeightRatio = 1.0f / 30.0f; t.menuMinimumHeight = 28; t.menuGap = 6;
    CopyText(t.menuFont, sizeof(t.menuFont), "Arial");
    t.menuFontMinimumHeight = 18; t.menuFontInset = 8;
    t.menuTextColor = RGB(255, 255, 255); t.menuHoverTextColor = RGB(255, 226, 102); t.menuTextShadow = 2;
    t.expansionWidthRatio = 3.0f / 16.0f; t.expansionMinimumWidth = 260;
    t.expansionHeightRatio = 1.0f / 20.0f; t.expansionMinimumHeight = 38;
    t.expansionXMarginRatio = 1.0f / 16.0f; t.expansionYRatio = 1.0f / 14.0f;
    t.expansionGapRatio = 1.0f / 105.0f; t.expansionMinimumGap = 7;
    t.securedIconWidthRatio = 1.0f / 19.0f; t.securedIconMinimumWidth = 78;
    t.securedIconXRatio = 1.0f / 16.0f; t.securedIconBottomRatio = 1.0f / 6.0f;
    t.securedIconBottomMinimum = 145;
    t.statusHeightRatio = 1.0f / 36.0f; t.statusMinimumHeight = 26;
    t.statusColor = RGB(42, 38, 74); CopyText(t.statusFont, sizeof(t.statusFont), "Consolas");
    t.statusFontMinimumHeight = 14; t.statusFontHeightRatio = 1.0f / 48.0f;
    t.statusTextColor = RGB(255, 255, 255);

    GameUiNationConfig &n = config.nation;
    CopyText(n.bastokTexture, sizeof(n.bastokTexture), "bcre");
    CopyText(n.windurstTexture, sizeof(n.windurstTexture), "wcre");
    CopyText(n.sandoriaTexture, sizeof(n.sandoriaTexture), "scre");
    n.marginXRatio = 1.0f / 18.0f; n.marginXMinimum = 56;
    n.cardGapRatio = 1.0f / 70.0f; n.cardGapMinimum = 18;
    n.cardHeightRatio = 1.0f / 3.0f; n.cardHeightMinimum = 230;
    n.cardYRatio = 1.0f / 7.0f; n.cardYMinimum = 118;
    n.cardColor = 0xA0202230; n.selectedCardColor = 0xB02E2A24;
    n.cardBorderColor = RGB(112, 116, 128); n.selectedCardBorderColor = RGB(80, 96, 210);
    n.cardBorderWidth = 2;
    n.iconSizeRatio = 1.0f / 6.0f; n.iconMinimumSize = 96; n.iconMaximumSize = 160;
    n.iconTopMinimum = 26; n.iconTopRatio = 1.0f / 38.0f;
    n.titleTopRatio = 1.0f / 25.0f; n.titleTopMinimum = 24;
    n.titleHeightRatio = 1.0f / 9.0f; n.titleHeightMinimum = 82;
    CopyText(n.titleFont, sizeof(n.titleFont), "Georgia");
    n.titleFontHeightRatio = 1.0f / 17.0f; n.titleFontMinimumHeight = 32; n.titleColor = RGB(255, 255, 255);
    CopyText(n.nameFont, sizeof(n.nameFont), "Georgia");
    n.nameFontHeightRatio = 1.0f / 42.0f; n.nameFontMinimumHeight = 18; n.nameColor = RGB(255, 255, 255);
    n.nameTopRatio = 1.0f / 5.0f; n.nameTopMinimum = 132;
    n.nameHeightRatio = 1.0f / 20.0f; n.nameHeightMinimum = 50;
    CopyText(n.subtitleFont, sizeof(n.subtitleFont), "Arial");
    n.subtitleFontHeightRatio = 1.0f / 55.0f; n.subtitleFontMinimumHeight = 15;
    n.subtitleHeightRatio = 1.0f / 28.0f; n.subtitleHeightMinimum = 34;
    n.descriptionMarginXRatio = 1.0f / 12.0f; n.descriptionMarginXMinimum = 80;
    n.descriptionBottomRatio = 1.0f / 12.0f; n.descriptionBottomMinimum = 62;
    n.descriptionHeightRatio = 1.0f / 4.0f; n.descriptionHeightMinimum = 170;
    n.descriptionFillColor = RGB(20, 22, 34); n.descriptionBorderColor = RGB(120, 126, 155);
    n.descriptionTextColor = RGB(235, 238, 245); CopyText(n.descriptionFont, sizeof(n.descriptionFont), "Arial");
    n.descriptionFontHeightRatio = 1.0f / 48.0f; n.descriptionFontMinimumHeight = 16;
    n.statusHeightRatio = 1.0f / 30.0f; n.statusMinimumHeight = 28;
    n.statusColor = RGB(42, 42, 92); n.statusTextColor = RGB(255, 255, 255);
    CopyText(n.statusFont, sizeof(n.statusFont), "Consolas");
    n.statusFontHeightRatio = 1.0f / 60.0f; n.statusFontMinimumHeight = 13;
}

bool GameUiConfig_Load(GameUiConfig &config)
{
    GameUiConfig_SetDefaults(config);
    if (!FindConfigPath(config.loadedPath, sizeof(config.loadedPath)))
        return false;
    const char *p = config.loadedPath;
    config.chatLogWidthPercent = std::clamp(ReadInt(p, "ChatLog", "WidthPercent", 50), 20, 100);
    config.chatLogHeightPercent = std::clamp(ReadInt(p, "ChatLog", "HeightPercent", 25), 10, 75);
    config.chatLogTimeoutSeconds = std::clamp(ReadInt(p, "ChatLog", "TimeoutSeconds", 15), 1, 300);
    auto& player = config.playerNameplate;
    player.enabled = ReadInt(p, "Player.Nameplate", "Enabled", 1) != 0;
    ReadString(p, "Player.Nameplate", "Name", "", player.name, sizeof(player.name));
    ReadString(p, "Player.Nameplate", "Icon", "none", player.icon, sizeof(player.icon));
    player.linkshellColor = ReadRgb(p, "Player.Nameplate", "LinkshellColor", player.linkshellColor);
    player.jobMaster = ReadInt(p, "Player.Nameplate", "JobMaster", 0) != 0;
    char subtitle[32] = {};
    ReadString(p, "Player.Nameplate", "Subtitle", "linkshell", subtitle, sizeof(subtitle));
    player.subtitleMode = _stricmp(subtitle, "jobs") == 0 ? PlayerSubtitleMode::Jobs :
        (_stricmp(subtitle, "linkshell") == 0 ? PlayerSubtitleMode::Linkshell : PlayerSubtitleMode::None);
    ReadString(p, "Player.Nameplate", "LinkshellName", "", player.linkshellName, sizeof(player.linkshellName));
    player.mainJob = ReadJob(p, "MainJob", FFXIStats::kJob_WAR, false);
    player.subJob = ReadJob(p, "SubJob", FFXIStats::kJob_None, true);
    player.mainLevel = std::clamp(ReadInt(p, "Player.Nameplate", "MainLevel", 1), 1, 99);
    player.subLevel = std::clamp(ReadInt(p, "Player.Nameplate", "SubLevel", 1), 1, 99);
    GameUiTitleConfig &t = config.title;
    ReadString(p, "Title.Assets", "LogoDat", t.logoDat, t.logoDat, sizeof(t.logoDat));
    ReadString(p, "Title.Assets", "AtlasDat", t.atlasDat, t.atlasDat, sizeof(t.atlasDat));
    ReadString(p, "Title.Assets", "UiDat", t.uiDat, t.uiDat, sizeof(t.uiDat));
    ReadString(p, "Title.Assets", "LogoTexture", t.logoTexture, t.logoTexture, sizeof(t.logoTexture));
    ReadString(p, "Title.Assets", "AtlasTexture", t.atlasTexture, t.atlasTexture, sizeof(t.atlasTexture));
    t.logoSourceX = ReadFloat(p, "Title.Logo", "SourceX", t.logoSourceX);
    t.logoSourceY = ReadFloat(p, "Title.Logo", "SourceY", t.logoSourceY);
    t.logoSourceWidth = ReadFloat(p, "Title.Logo", "SourceWidth", t.logoSourceWidth);
    t.logoSourceHeight = ReadFloat(p, "Title.Logo", "SourceHeight", t.logoSourceHeight);
    t.logoWidthRatio = ReadFloat(p, "Title.Logo", "WidthRatio", t.logoWidthRatio);
    t.logoMinimumWidth = ReadInt(p, "Title.Logo", "MinimumWidth", t.logoMinimumWidth);
    t.logoXRatio = ReadFloat(p, "Title.Logo", "XRatio", t.logoXRatio);
    t.logoYRatio = ReadFloat(p, "Title.Logo", "YRatio", t.logoYRatio);
    t.logoYOffsetByHeight = ReadFloat(p, "Title.Logo", "YOffsetByLogoHeight", t.logoYOffsetByHeight);
    t.logoAlphaScale = ReadFloat(p, "Title.Logo", "AlphaScale", t.logoAlphaScale);
    t.logoMaximumOpacity = std::clamp(ReadFloat(p, "Title.Logo", "MaximumOpacity", t.logoMaximumOpacity), 0.0f, 1.0f);
    t.menuXRatio = ReadFloat(p, "Title.Menu", "XRatio", t.menuXRatio);
    t.menuYRatio = ReadFloat(p, "Title.Menu", "YRatio", t.menuYRatio);
    t.menuWidthRatio = ReadFloat(p, "Title.Menu", "WidthRatio", t.menuWidthRatio);
    t.menuMinimumWidth = ReadInt(p, "Title.Menu", "MinimumWidth", t.menuMinimumWidth);
    t.menuHeightRatio = ReadFloat(p, "Title.Menu", "HeightRatio", t.menuHeightRatio);
    t.menuMinimumHeight = ReadInt(p, "Title.Menu", "MinimumHeight", t.menuMinimumHeight);
    t.menuGap = ReadInt(p, "Title.Menu", "Gap", t.menuGap);
    ReadString(p, "Title.Menu", "Font", t.menuFont, t.menuFont, sizeof(t.menuFont));
    t.menuFontMinimumHeight = ReadInt(p, "Title.Menu", "FontMinimumHeight", t.menuFontMinimumHeight);
    t.menuFontInset = ReadInt(p, "Title.Menu", "FontInset", t.menuFontInset);
    t.menuTextColor = ReadRgb(p, "Title.Menu", "TextColor", t.menuTextColor);
    t.menuHoverTextColor = ReadRgb(p, "Title.Menu", "HoverTextColor", t.menuHoverTextColor);
    t.menuTextShadow = ReadInt(p, "Title.Menu", "TextShadow", t.menuTextShadow);
    t.expansionWidthRatio = ReadFloat(p, "Title.Expansions", "WidthRatio", t.expansionWidthRatio);
    t.expansionMinimumWidth = ReadInt(p, "Title.Expansions", "MinimumWidth", t.expansionMinimumWidth);
    t.expansionHeightRatio = ReadFloat(p, "Title.Expansions", "HeightRatio", t.expansionHeightRatio);
    t.expansionMinimumHeight = ReadInt(p, "Title.Expansions", "MinimumHeight", t.expansionMinimumHeight);
    t.expansionXMarginRatio = ReadFloat(p, "Title.Expansions", "RightMarginRatio", t.expansionXMarginRatio);
    t.expansionYRatio = ReadFloat(p, "Title.Expansions", "YRatio", t.expansionYRatio);
    t.expansionGapRatio = ReadFloat(p, "Title.Expansions", "GapRatio", t.expansionGapRatio);
    t.expansionMinimumGap = ReadInt(p, "Title.Expansions", "MinimumGap", t.expansionMinimumGap);
    t.securedIconWidthRatio = ReadFloat(p, "Title.Icons", "SecuredWidthRatio", t.securedIconWidthRatio);
    t.securedIconMinimumWidth = ReadInt(p, "Title.Icons", "SecuredMinimumWidth", t.securedIconMinimumWidth);
    t.securedIconXRatio = ReadFloat(p, "Title.Icons", "SecuredXRatio", t.securedIconXRatio);
    t.securedIconBottomRatio = ReadFloat(p, "Title.Icons", "SecuredBottomRatio", t.securedIconBottomRatio);
    t.securedIconBottomMinimum = ReadInt(p, "Title.Icons", "SecuredBottomMinimum", t.securedIconBottomMinimum);
    t.statusHeightRatio = ReadFloat(p, "Title.Status", "HeightRatio", t.statusHeightRatio);
    t.statusMinimumHeight = ReadInt(p, "Title.Status", "MinimumHeight", t.statusMinimumHeight);
    t.statusColor = ReadRgb(p, "Title.Status", "Color", t.statusColor);
    ReadString(p, "Title.Status", "Font", t.statusFont, t.statusFont, sizeof(t.statusFont));
    t.statusFontMinimumHeight = ReadInt(p, "Title.Status", "FontMinimumHeight", t.statusFontMinimumHeight);
    t.statusFontHeightRatio = ReadFloat(p, "Title.Status", "FontHeightRatio", t.statusFontHeightRatio);
    t.statusTextColor = ReadRgb(p, "Title.Status", "TextColor", t.statusTextColor);

    GameUiNationConfig &n = config.nation;
    ReadString(p, "Nation.Icons", "BastokTexture", n.bastokTexture, n.bastokTexture, sizeof(n.bastokTexture));
    ReadString(p, "Nation.Icons", "WindurstTexture", n.windurstTexture, n.windurstTexture, sizeof(n.windurstTexture));
    ReadString(p, "Nation.Icons", "SandoriaTexture", n.sandoriaTexture, n.sandoriaTexture, sizeof(n.sandoriaTexture));
    n.iconSizeRatio = ReadFloat(p, "Nation.Icons", "SizeRatio", n.iconSizeRatio);
    n.iconMinimumSize = ReadInt(p, "Nation.Icons", "MinimumSize", n.iconMinimumSize);
    n.iconMaximumSize = ReadInt(p, "Nation.Icons", "MaximumSize", n.iconMaximumSize);
    n.iconTopMinimum = ReadInt(p, "Nation.Icons", "TopMinimum", n.iconTopMinimum);
    n.iconTopRatio = ReadFloat(p, "Nation.Icons", "TopRatio", n.iconTopRatio);
    n.marginXRatio = ReadFloat(p, "Nation.Cards", "MarginXRatio", n.marginXRatio);
    n.marginXMinimum = ReadInt(p, "Nation.Cards", "MarginXMinimum", n.marginXMinimum);
    n.cardGapRatio = ReadFloat(p, "Nation.Cards", "GapRatio", n.cardGapRatio);
    n.cardGapMinimum = ReadInt(p, "Nation.Cards", "GapMinimum", n.cardGapMinimum);
    n.cardHeightRatio = ReadFloat(p, "Nation.Cards", "HeightRatio", n.cardHeightRatio);
    n.cardHeightMinimum = ReadInt(p, "Nation.Cards", "HeightMinimum", n.cardHeightMinimum);
    n.cardYRatio = ReadFloat(p, "Nation.Cards", "YRatio", n.cardYRatio);
    n.cardYMinimum = ReadInt(p, "Nation.Cards", "YMinimum", n.cardYMinimum);
    n.cardColor = ReadUnsigned(p, "Nation.Cards", "ArgbColor", n.cardColor);
    n.selectedCardColor = ReadUnsigned(p, "Nation.Cards", "SelectedArgbColor", n.selectedCardColor);
    n.cardBorderColor = ReadRgb(p, "Nation.Cards", "BorderColor", n.cardBorderColor);
    n.selectedCardBorderColor = ReadRgb(p, "Nation.Cards", "SelectedBorderColor", n.selectedCardBorderColor);
    n.cardBorderWidth = ReadInt(p, "Nation.Cards", "BorderWidth", n.cardBorderWidth);
    n.titleTopRatio = ReadFloat(p, "Nation.Text", "TitleTopRatio", n.titleTopRatio);
    n.titleTopMinimum = ReadInt(p, "Nation.Text", "TitleTopMinimum", n.titleTopMinimum);
    n.titleHeightRatio = ReadFloat(p, "Nation.Text", "TitleHeightRatio", n.titleHeightRatio);
    n.titleHeightMinimum = ReadInt(p, "Nation.Text", "TitleHeightMinimum", n.titleHeightMinimum);
    ReadString(p, "Nation.Text", "TitleFont", n.titleFont, n.titleFont, sizeof(n.titleFont));
    n.titleFontHeightRatio = ReadFloat(p, "Nation.Text", "TitleFontHeightRatio", n.titleFontHeightRatio);
    n.titleFontMinimumHeight = ReadInt(p, "Nation.Text", "TitleFontMinimumHeight", n.titleFontMinimumHeight);
    n.titleColor = ReadRgb(p, "Nation.Text", "TitleColor", n.titleColor);
    ReadString(p, "Nation.Text", "NameFont", n.nameFont, n.nameFont, sizeof(n.nameFont));
    n.nameFontHeightRatio = ReadFloat(p, "Nation.Text", "NameFontHeightRatio", n.nameFontHeightRatio);
    n.nameFontMinimumHeight = ReadInt(p, "Nation.Text", "NameFontMinimumHeight", n.nameFontMinimumHeight);
    n.nameColor = ReadRgb(p, "Nation.Text", "NameColor", n.nameColor);
    n.nameTopRatio = ReadFloat(p, "Nation.Text", "NameTopRatio", n.nameTopRatio);
    n.nameTopMinimum = ReadInt(p, "Nation.Text", "NameTopMinimum", n.nameTopMinimum);
    n.nameHeightRatio = ReadFloat(p, "Nation.Text", "NameHeightRatio", n.nameHeightRatio);
    n.nameHeightMinimum = ReadInt(p, "Nation.Text", "NameHeightMinimum", n.nameHeightMinimum);
    ReadString(p, "Nation.Text", "SubtitleFont", n.subtitleFont, n.subtitleFont, sizeof(n.subtitleFont));
    n.subtitleFontHeightRatio = ReadFloat(p, "Nation.Text", "SubtitleFontHeightRatio", n.subtitleFontHeightRatio);
    n.subtitleFontMinimumHeight = ReadInt(p, "Nation.Text", "SubtitleFontMinimumHeight", n.subtitleFontMinimumHeight);
    n.subtitleHeightRatio = ReadFloat(p, "Nation.Text", "SubtitleHeightRatio", n.subtitleHeightRatio);
    n.subtitleHeightMinimum = ReadInt(p, "Nation.Text", "SubtitleHeightMinimum", n.subtitleHeightMinimum);
    n.descriptionMarginXRatio = ReadFloat(p, "Nation.Description", "MarginXRatio", n.descriptionMarginXRatio);
    n.descriptionMarginXMinimum = ReadInt(p, "Nation.Description", "MarginXMinimum", n.descriptionMarginXMinimum);
    n.descriptionBottomRatio = ReadFloat(p, "Nation.Description", "BottomRatio", n.descriptionBottomRatio);
    n.descriptionBottomMinimum = ReadInt(p, "Nation.Description", "BottomMinimum", n.descriptionBottomMinimum);
    n.descriptionHeightRatio = ReadFloat(p, "Nation.Description", "HeightRatio", n.descriptionHeightRatio);
    n.descriptionHeightMinimum = ReadInt(p, "Nation.Description", "HeightMinimum", n.descriptionHeightMinimum);
    n.descriptionFillColor = ReadRgb(p, "Nation.Description", "FillColor", n.descriptionFillColor);
    n.descriptionBorderColor = ReadRgb(p, "Nation.Description", "BorderColor", n.descriptionBorderColor);
    n.descriptionTextColor = ReadRgb(p, "Nation.Description", "TextColor", n.descriptionTextColor);
    ReadString(p, "Nation.Description", "Font", n.descriptionFont, n.descriptionFont, sizeof(n.descriptionFont));
    n.descriptionFontHeightRatio = ReadFloat(p, "Nation.Description", "FontHeightRatio", n.descriptionFontHeightRatio);
    n.descriptionFontMinimumHeight = ReadInt(p, "Nation.Description", "FontMinimumHeight", n.descriptionFontMinimumHeight);
    n.statusHeightRatio = ReadFloat(p, "Nation.Status", "HeightRatio", n.statusHeightRatio);
    n.statusMinimumHeight = ReadInt(p, "Nation.Status", "MinimumHeight", n.statusMinimumHeight);
    n.statusColor = ReadRgb(p, "Nation.Status", "Color", n.statusColor);
    n.statusTextColor = ReadRgb(p, "Nation.Status", "TextColor", n.statusTextColor);
    ReadString(p, "Nation.Status", "Font", n.statusFont, n.statusFont, sizeof(n.statusFont));
    n.statusFontHeightRatio = ReadFloat(p, "Nation.Status", "FontHeightRatio", n.statusFontHeightRatio);
    n.statusFontMinimumHeight = ReadInt(p, "Nation.Status", "FontMinimumHeight", n.statusFontMinimumHeight);
    config.loaded = true;
    return true;
}

bool GameUiConfig_SaveChatLog(const GameUiConfig &config)
{
    char path[MAX_PATH] = {};
    if (config.loadedPath[0]) CopyText(path, sizeof(path), config.loadedPath);
    else
    {
        if (!GetModuleFileNameA(nullptr, path, MAX_PATH)) return false;
        char* slash = strrchr(path, '\\');
        if (!slash) return false;
        slash[1] = 0;
        strcat_s(path, kConfigFileName);
    }
    const bool widthSaved = WritePrivateProfileStringA("ChatLog", "WidthPercent",
        std::to_string(std::clamp(config.chatLogWidthPercent, 20, 100)).c_str(), path) != FALSE;
    const bool heightSaved = WritePrivateProfileStringA("ChatLog", "HeightPercent",
        std::to_string(std::clamp(config.chatLogHeightPercent, 10, 75)).c_str(), path) != FALSE;
    return WritePrivateProfileStringA("ChatLog", "TimeoutSeconds",
        std::to_string(std::clamp(config.chatLogTimeoutSeconds, 1, 300)).c_str(), path) != FALSE && widthSaved && heightSaved;
}

bool GameUiConfig_SavePlayerNameplate(const GameUiConfig &config)
{
    char path[MAX_PATH] = {};
    if (config.loadedPath[0]) CopyText(path, sizeof(path), config.loadedPath);
    else
    {
        if (!GetModuleFileNameA(nullptr, path, MAX_PATH)) return false;
        char* slash = strrchr(path, '\\');
        if (!slash) return false;
        slash[1] = 0;
        strcat_s(path, kConfigFileName);
    }
    // Write only the controls changed here; preserve the rest of the UI config.
    const bool iconSaved = WritePrivateProfileStringA("Player.Nameplate", "Icon",
        config.playerNameplate.icon, path) != FALSE;
    const bool starsSaved = WritePrivateProfileStringA("Player.Nameplate", "JobMaster",
        config.playerNameplate.jobMaster ? "1" : "0", path) != FALSE;
    const auto& player = config.playerNameplate;
    bool saved = iconSaved && starsSaved;
    const auto write = [&](const char* key, const char* value) {
        if (!WritePrivateProfileStringA("Player.Nameplate", key, value, path)) saved = false;
    };
    write("Subtitle", player.subtitleMode == PlayerSubtitleMode::Jobs ? "jobs" :
        (player.subtitleMode == PlayerSubtitleMode::Linkshell ? "linkshell" : "none"));
    write("LinkshellName", player.linkshellName);
    write("MainJob", FFXIStats::JobName(static_cast<FFXIStats::Job>(player.mainJob)));
    write("SubJob", FFXIStats::JobName(static_cast<FFXIStats::Job>(player.subJob)));
    write("MainLevel", std::to_string(player.mainLevel).c_str());
    write("SubLevel", std::to_string(player.subLevel).c_str());
    return saved;
}

std::string GameUiConfig_PlayerSubtitle(const GameUiPlayerNameplateConfig &player)
{
    if (player.subtitleMode == PlayerSubtitleMode::Linkshell) return player.linkshellName;
    if (player.subtitleMode != PlayerSubtitleMode::Jobs ||
        player.mainJob <= FFXIStats::kJob_None || player.mainJob >= FFXIStats::kJob_Count) return {};
    std::string result = FFXIStats::JobName(static_cast<FFXIStats::Job>(player.mainJob));
    result += " " + std::to_string(std::clamp(player.mainLevel, 1, 99));
    if (player.subJob > FFXIStats::kJob_None && player.subJob < FFXIStats::kJob_Count)
    {
        result += " / ";
        result += FFXIStats::JobName(static_cast<FFXIStats::Job>(player.subJob));
        result += " " + std::to_string(std::clamp(player.subLevel, 1, 99));
    }
    return result;
}

GameUiTitleMenuMetrics GameUiConfig_GetTitleMenuMetrics(const GameUiTitleConfig &config,
                                                        int viewportWidth, int viewportHeight)
{
    GameUiTitleMenuMetrics metrics = {};
    metrics.x = (int)((float)viewportWidth * config.menuXRatio);
    metrics.y = (int)((float)viewportHeight * config.menuYRatio);
    metrics.width = std::max(config.menuMinimumWidth,
                             (int)((float)viewportWidth * config.menuWidthRatio));
    metrics.height = std::max(config.menuMinimumHeight,
                              (int)((float)viewportHeight * config.menuHeightRatio));
    return metrics;
}

int GameUiConfig_GetTitleMenuButtonIndex(const GameUiTitleConfig &config,
                                         int viewportWidth, int viewportHeight, POINT point)
{
    GameUiTitleMenuMetrics metrics = GameUiConfig_GetTitleMenuMetrics(
        config, viewportWidth, viewportHeight);
    for (int index = 0; index < 5; ++index)
    {
        if (point.x >= metrics.x && point.x < metrics.x + metrics.width &&
            point.y >= metrics.y && point.y < metrics.y + metrics.height)
        {
            return index;
        }
        metrics.y += metrics.height + config.menuGap;
    }
    return -1;
}

RECT GameUiConfig_GetNationCardRect(const GameUiNationConfig &config,
                                    int viewportWidth, int viewportHeight, int index)
{
    const int marginX = std::max(config.marginXMinimum,
                                 (int)((float)viewportWidth * config.marginXRatio));
    const int cardGap = std::max(config.cardGapMinimum,
                                 (int)((float)viewportWidth * config.cardGapRatio));
    const int cardWidth = std::max(80, (viewportWidth - marginX * 2 - cardGap * 2) / 3);
    const int cardHeight = std::max(config.cardHeightMinimum,
                                    (int)((float)viewportHeight * config.cardHeightRatio));
    const int cardY = std::max(config.cardYMinimum,
                               (int)((float)viewportHeight * config.cardYRatio));
    const int cardX = marginX + (cardWidth + cardGap) * index;
    return { cardX, cardY, cardX + cardWidth, cardY + cardHeight };
}

int GameUiConfig_GetNationCardIndex(const GameUiNationConfig &config,
                                    int viewportWidth, int viewportHeight,
                                    int cardCount, POINT point)
{
    for (int index = 0; index < cardCount; ++index)
    {
        const RECT card = GameUiConfig_GetNationCardRect(
            config, viewportWidth, viewportHeight, index);
        if (PtInRect(&card, point))
            return index;
    }
    return -1;
}
