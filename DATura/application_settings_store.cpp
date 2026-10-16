#include "stdafx.h"
#include "application_settings_store.h"

#include <array>
#include <string>

namespace ApplicationSettingsStore
{
namespace
{
constexpr char kRegistryKey[] = "Software\\FFXIViewer";
constexpr char kEnableHdTexturesValue[] = "EnableHdTextures";
constexpr char kHdTextureFolderValue[] = "HdTextureFolder";
constexpr char kEnablePbrValue[] = "EnablePbr";
constexpr char kPbrTextureFolderValue[] = "PbrTextureFolder";
constexpr char kRenderingResolutionValue[] = "RenderingResolutionScale";
constexpr char kMapTextureCompressionValue[] = "MapTextureCompression";
constexpr char kWeatherEffectsValue[] = "WeatherEffects";
constexpr char kInvertBumpMappingValue[] = "InvertBumpMapping";
constexpr char kAntiAliasingValue[] = "AntiAliasing";
constexpr char kPostProcessAntiAliasingValue[] = "PostProcessAntiAliasing";
constexpr char kShadowPlayerValue[] = "ShadowPlayer";
constexpr char kShadowNpcsValue[] = "ShadowNpcs";
constexpr char kShadowObjectsValue[] = "ShadowObjects";
constexpr char kShadowGroundValue[] = "ShadowGroundLikeObjects";
constexpr char kShadowAlphaValue[] = "ShadowAlphaTestedObjects";
constexpr char kShadowDistanceValue[] = "ShadowMaxDistance";
constexpr char kShadowNpcLimitValue[] = "ShadowNpcLimit";
constexpr char kShadowObjectLimitValue[] = "ShadowObjectLimit";
constexpr char kShadowMinSizeValue[] = "ShadowMinimumSizePercent";
constexpr char kShadowMaxSizeValue[] = "ShadowMaximumSize";
constexpr char kShadowUpdateFramesValue[] = "ShadowReceiverUpdateFrames";
constexpr char kShadowReceiverQualityValue[] = "ShadowReceiverQuality";
constexpr char kShadowLengthValue[] = "ShadowMaximumLength";
constexpr char kShadowOpacityValue[] = "ShadowOpacityPercent";
constexpr char kShadowDebugValue[] = "ShadowDebugVisualization";
constexpr char kShadowCountersValue[] = "ShadowPerformanceCounters";
constexpr char kTitleBackgroundModeValue[] = "TitleBackgroundMode";
constexpr char kTitleBackgroundZoneValue[] = "TitleBackgroundZone";
constexpr char kTitleMusicModeValue[] = "TitleMusicMode";
constexpr char kTitleMusicValue[] = "TitleMusic";
constexpr char kShowTitleUiValue[] = "ShowTitleUi";

constexpr std::array<const char*, 14> kKeyBindingValues =
{
    "KeyForward", "KeyBackward", "KeyLeft", "KeyRight", "KeyJump",
    "KeyAutoRun", "KeyRunToggle", "KeyGameMode", "KeyZoneMap",
    "KeyUnstick", "KeyCycleWeather", "KeyCameraDebug", "KeyBumpMapping",
    "KeyMainMenu"
};

DWORD ReadDword(const char* valueName, const DWORD fallback)
{
    HKEY key = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, kRegistryKey, 0, KEY_READ, &key) != ERROR_SUCCESS)
        return fallback;
    DWORD value = fallback;
    DWORD type = 0;
    DWORD size = sizeof(value);
    if (RegQueryValueExA(key, valueName, nullptr, &type,
            reinterpret_cast<BYTE*>(&value), &size) != ERROR_SUCCESS ||
        type != REG_DWORD || size != sizeof(value))
        value = fallback;
    RegCloseKey(key);
    return value;
}

void WriteDword(const char* valueName, const DWORD value)
{
    HKEY key = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, kRegistryKey, 0, nullptr, 0,
            KEY_SET_VALUE, nullptr, &key, nullptr) == ERROR_SUCCESS)
    {
        RegSetValueExA(key, valueName, 0, REG_DWORD,
            reinterpret_cast<const BYTE*>(&value), sizeof(value));
        RegCloseKey(key);
    }
}

std::string ReadString(const char* valueName)
{
    HKEY key = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, kRegistryKey, 0, KEY_READ, &key) != ERROR_SUCCESS)
        return {};
    char value[4096] = {};
    DWORD type = 0;
    DWORD size = sizeof(value);
    const LONG result = RegQueryValueExA(key, valueName, nullptr, &type,
        reinterpret_cast<BYTE*>(value), &size);
    RegCloseKey(key);
    return result == ERROR_SUCCESS && type == REG_SZ ? std::string(value) : std::string();
}

void WriteString(const char* valueName, const std::string& value)
{
    HKEY key = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, kRegistryKey, 0, nullptr, 0,
            KEY_SET_VALUE, nullptr, &key, nullptr) == ERROR_SUCCESS)
    {
        RegSetValueExA(key, valueName, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(value.c_str()),
            static_cast<DWORD>(value.size() + 1));
        RegCloseKey(key);
    }
}

int* KeyBindingValue(ApplicationSettings::State& settings, const int index)
{
    switch (index)
    {
    case 0: return &settings.keyForward;
    case 1: return &settings.keyBackward;
    case 2: return &settings.keyLeft;
    case 3: return &settings.keyRight;
    case 4: return &settings.keyJump;
    case 5: return &settings.keyAutoRun;
    case 6: return &settings.keyRunToggle;
    case 7: return &settings.keyGameMode;
    case 8: return &settings.keyZoneMap;
    case 9: return &settings.keyUnstick;
    case 10: return &settings.keyCycleWeather;
    case 11: return &settings.keyCameraDebug;
    case 12: return &settings.keyBumpMapping;
    default: return &settings.keyMainMenu;
    }
}

const int* KeyBindingValue(const ApplicationSettings::State& settings, const int index)
{
    switch (index)
    {
    case 0: return &settings.keyForward;
    case 1: return &settings.keyBackward;
    case 2: return &settings.keyLeft;
    case 3: return &settings.keyRight;
    case 4: return &settings.keyJump;
    case 5: return &settings.keyAutoRun;
    case 6: return &settings.keyRunToggle;
    case 7: return &settings.keyGameMode;
    case 8: return &settings.keyZoneMap;
    case 9: return &settings.keyUnstick;
    case 10: return &settings.keyCycleWeather;
    case 11: return &settings.keyCameraDebug;
    case 12: return &settings.keyBumpMapping;
    default: return &settings.keyMainMenu;
    }
}
}

const char* RegistryKey()
{
    return kRegistryKey;
}

void Load(ApplicationSettings::State& settings)
{
    settings.enableHdTextures = ReadDword(kEnableHdTexturesValue, 0) != 0;
    settings.hdTextureFolder = ReadString(kHdTextureFolderValue);
    settings.enablePbr = ReadDword(kEnablePbrValue, 0) != 0;
    settings.pbrTextureFolder = ReadString(kPbrTextureFolderValue);

    settings.renderingResolutionScaleIndex =
        ApplicationSettings::ClampRenderingResolutionScaleIndex(static_cast<int>(
            ReadDword(kRenderingResolutionValue, 2)));
    settings.enableMapTextureCompression = ReadDword(kMapTextureCompressionValue, 1) != 0;
    settings.enableWeatherEffects = ReadDword(kWeatherEffectsValue, 1) != 0;
    settings.showZoneTriggerVolumes = ReadDword("ShowZoneTriggerVolumes", 0) != 0;
    settings.showZoneArrivalVolumes = ReadDword("ShowZoneArrivalVolumes", 0) != 0;
    settings.showZoneLineMarkers = ReadDword("ShowZoneLineMarkers", 1) != 0;
    settings.invertBumpMapping = ReadDword(kInvertBumpMappingValue, 0) != 0;
    settings.antiAliasingIndex = ApplicationSettings::ClampAntiAliasingIndex(
        static_cast<int>(ReadDword(kAntiAliasingValue, 0)));
    settings.postProcessAntiAliasingMode =
        ApplicationSettings::ClampPostProcessAntiAliasingMode(static_cast<int>(
            ReadDword(kPostProcessAntiAliasingValue, 0)));
    settings.shadowPlayer = ReadDword(kShadowPlayerValue, 1) != 0;
    settings.shadowNpcs = ReadDword(kShadowNpcsValue, 1) != 0;
    settings.shadowObjects = ReadDword(kShadowObjectsValue, 1) != 0;
    settings.shadowGroundLikeObjects = ReadDword(kShadowGroundValue, 0) != 0;
    settings.shadowAlphaTestedObjects = ReadDword(kShadowAlphaValue, 0) != 0;
    settings.shadowMaxDistance = static_cast<int>(ReadDword(kShadowDistanceValue, 80));
    settings.shadowNpcLimit = static_cast<int>(ReadDword(kShadowNpcLimitValue, 16));
    settings.shadowObjectLimit = static_cast<int>(ReadDword(kShadowObjectLimitValue, 64));
    settings.shadowMinimumSizePercent = static_cast<int>(ReadDword(kShadowMinSizeValue, 25));
    settings.shadowMaximumSize = static_cast<int>(ReadDword(kShadowMaxSizeValue, 40));
    settings.shadowReceiverUpdateFrames = static_cast<int>(ReadDword(kShadowUpdateFramesValue, 4));
    settings.shadowReceiverQuality = static_cast<int>(ReadDword(kShadowReceiverQualityValue, 1));
    settings.shadowMaximumLength = static_cast<int>(ReadDword(kShadowLengthValue, 30));
    settings.shadowOpacityPercent = static_cast<int>(ReadDword(kShadowOpacityValue, 32));
    settings.shadowDebugVisualization = ReadDword(kShadowDebugValue, 0) != 0;
    settings.shadowPerformanceCounters = ReadDword(kShadowCountersValue, 0) != 0;

    settings.titleBackgroundMode = ApplicationSettings::ClampTitleBackgroundMode(
        static_cast<int>(ReadDword(kTitleBackgroundModeValue,
            ApplicationSettings::TitleBackgroundRandom)));
    settings.titleBackgroundZoneIndex = ApplicationSettings::ClampTitleBackgroundZoneIndex(
        static_cast<int>(ReadDword(kTitleBackgroundZoneValue, 8)));
    settings.titleMusicMode = ApplicationSettings::ClampTitleMusicMode(
        static_cast<int>(ReadDword(kTitleMusicModeValue,
            ApplicationSettings::TitleMusicRandom)));
    settings.titleMusicIndex = ApplicationSettings::ClampTitleMusicIndex(
        static_cast<int>(ReadDword(kTitleMusicValue, 0)));
    settings.showTitleUi = ReadDword(kShowTitleUiValue, 1) != 0;

    for (int index = 0; index < static_cast<int>(kKeyBindingValues.size()); ++index)
    {
        int* binding = KeyBindingValue(settings, index);
        *binding = static_cast<int>(ReadDword(kKeyBindingValues[index],
            static_cast<DWORD>(*binding)));
    }
}

void SaveCustomTextures(const ApplicationSettings::State& settings)
{
    WriteDword(kEnableHdTexturesValue, settings.enableHdTextures ? 1u : 0u);
    WriteString(kHdTextureFolderValue, settings.hdTextureFolder);
    WriteDword(kEnablePbrValue, settings.enablePbr ? 1u : 0u);
    WriteString(kPbrTextureFolderValue, settings.pbrTextureFolder);
}

void SaveExtendedGraphics(const ApplicationSettings::State& settings)
{
    WriteDword(kRenderingResolutionValue, static_cast<DWORD>(
        ApplicationSettings::ClampRenderingResolutionScaleIndex(
            settings.renderingResolutionScaleIndex)));
    WriteDword(kMapTextureCompressionValue, settings.enableMapTextureCompression ? 1u : 0u);
    WriteDword(kWeatherEffectsValue, settings.enableWeatherEffects ? 1u : 0u);
    WriteDword("ShowZoneTriggerVolumes", settings.showZoneTriggerVolumes ? 1u : 0u);
    WriteDword("ShowZoneArrivalVolumes", settings.showZoneArrivalVolumes ? 1u : 0u);
    WriteDword("ShowZoneLineMarkers", settings.showZoneLineMarkers ? 1u : 0u);
    WriteDword(kInvertBumpMappingValue, settings.invertBumpMapping ? 1u : 0u);
    WriteDword(kAntiAliasingValue, static_cast<DWORD>(
        ApplicationSettings::ClampAntiAliasingIndex(settings.antiAliasingIndex)));
    WriteDword(kPostProcessAntiAliasingValue, static_cast<DWORD>(
        ApplicationSettings::ClampPostProcessAntiAliasingMode(
            settings.postProcessAntiAliasingMode)));
    WriteDword(kShadowPlayerValue, settings.shadowPlayer);
    WriteDword(kShadowNpcsValue, settings.shadowNpcs);
    WriteDword(kShadowObjectsValue, settings.shadowObjects);
    WriteDword(kShadowGroundValue, settings.shadowGroundLikeObjects);
    WriteDword(kShadowAlphaValue, settings.shadowAlphaTestedObjects);
    WriteDword(kShadowDistanceValue, settings.shadowMaxDistance);
    WriteDword(kShadowNpcLimitValue, settings.shadowNpcLimit);
    WriteDword(kShadowObjectLimitValue, settings.shadowObjectLimit);
    WriteDword(kShadowMinSizeValue, settings.shadowMinimumSizePercent);
    WriteDword(kShadowMaxSizeValue, settings.shadowMaximumSize);
    WriteDword(kShadowUpdateFramesValue, settings.shadowReceiverUpdateFrames);
    WriteDword(kShadowReceiverQualityValue, settings.shadowReceiverQuality);
    WriteDword(kShadowLengthValue, settings.shadowMaximumLength);
    WriteDword(kShadowOpacityValue, settings.shadowOpacityPercent);
    WriteDword(kShadowDebugValue, settings.shadowDebugVisualization);
    WriteDword(kShadowCountersValue, settings.shadowPerformanceCounters);
}

void SaveKeyBindings(const ApplicationSettings::State& settings)
{
    for (int index = 0; index < static_cast<int>(kKeyBindingValues.size()); ++index)
        WriteDword(kKeyBindingValues[index], static_cast<DWORD>(
            *KeyBindingValue(settings, index)));
}

void SaveTitleBackground(const ApplicationSettings::State& settings)
{
    WriteDword(kTitleBackgroundModeValue, static_cast<DWORD>(
        ApplicationSettings::ClampTitleBackgroundMode(settings.titleBackgroundMode)));
    WriteDword(kTitleBackgroundZoneValue, static_cast<DWORD>(
        ApplicationSettings::ClampTitleBackgroundZoneIndex(settings.titleBackgroundZoneIndex)));
    WriteDword(kTitleMusicModeValue, static_cast<DWORD>(
        ApplicationSettings::ClampTitleMusicMode(settings.titleMusicMode)));
    WriteDword(kTitleMusicValue, static_cast<DWORD>(
        ApplicationSettings::ClampTitleMusicIndex(settings.titleMusicIndex)));
    WriteDword(kShowTitleUiValue, settings.showTitleUi ? 1u : 0u);
}

void SaveAll(const ApplicationSettings::State& settings)
{
    SaveCustomTextures(settings);
    SaveExtendedGraphics(settings);
    SaveKeyBindings(settings);
    SaveTitleBackground(settings);
}
}
