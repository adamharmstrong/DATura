#include "application_settings.h"

#include <algorithm>
#include <cstddef>

namespace
{
const ApplicationSettings::ResolutionOption kResolutionOptions[] =
{
    { 640, 480,    "640 x 480" },
    { 800, 600,    "800 x 600" },
    { 1024, 768,   "1024 x 768" },
    { 1152, 864,   "1152 x 864" },
    { 1280, 800,   "1280 x 800" },
    { 1280, 1024,  "1280 x 1024" },
    { 1280, 720,   "1280 x 720" },
    { 1360, 768,   "1360 x 768" },
    { 1366, 768,  "1366 x 768" },
    { 1440, 900,   "1440 x 900" },
    { 1440, 1080,  "1440 x 1080" },
    { 1600, 1200,  "1600 x 1200" },
    { 1600, 900,  "1600 x 900" },
    { 1680, 1050, "1680 x 1050" },
    { 1920, 1200, "1920 x 1200" },
    { 1920, 1080, "1920 x 1080" },
    { 2560, 1080, "2560 x 1080" },
    { 2560, 1440, "2560 x 1440" },
    { 2560, 1600, "2560 x 1600" },
    { 3440, 1440, "3440 x 1440" },
    { 3840, 1080, "3840 x 1080" },
    { 3840, 1600, "3840 x 1600" },
    { 3840, 2160, "3840 x 2160" },
    { 5120, 1440, "5120 x 1440" },
    { 5120, 2160, "5120 x 2160" },
};

const int kMaxSoundOptions[] = { 8, 16, 20, 32, 64, 128, -1 };
const char* kMaxSoundOptionLabels[] =
{
    "8", "16", "20", "32", "64", "128", "Unlimited"
};

// Zero is a sentinel for the bounds-derived Unlimited mode; it is never passed
// directly to the projection matrix.
const float kDrawDistanceOptions[] = { 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 0.0f };
const char* kDrawDistanceOptionLabels[] =
{
    "500", "1000", "2000", "4000", "8000", "Unlimited"
};

const char* kRenderingBackendNames[] =
{
    "DirectX 8",
    "DirectX 9",
    "DirectX 11",
    "DirectX 12",
    "OpenGL",
    "Vulkan",
    "Metal (macOS)"
};

const int kRenderingResolutionScales[] = { 50, 75, 100, 150, 200 };
const char* kRenderingResolutionScaleLabels[] =
{
    "50% (Undersampling)", "75% (Undersampling)", "100% (Native)",
    "150% (Oversampling)", "200% (Oversampling)"
};
const int kAntiAliasingSamples[] = { 0, 2, 4, 8 };
const char* kAntiAliasingLabels[] = { "Off", "2x MSAA", "4x MSAA", "8x MSAA" };
const char* kPostProcessAntiAliasingLabels[] = { "Off", "FXAA", "SMAA" };

// The 19 retail PS2 title-demo locations plus DATura's existing Konschtat
// Highlands presentation. Keep this catalog in display/cycle order so the
// Config dialog and title loader always agree.
const ApplicationSettings::TitleBackgroundOption kTitleBackgroundOptions[] =
{
    { "La Theine Plateau", "ROM/0/115.DAT" },
    { "Valkurm Dunes", "ROM/0/102.DAT" },
    { "North Gustaberg", "ROM/0/123.DAT" },
    { "Jugner Forest", "ROM/0/114.DAT" },
    { "Pashhow Marshlands", "ROM/0/125.DAT" },
    { "Fort Ghelsba", "ROM/1/7.DAT" },
    { "Beaucedine Glacier", "ROM/0/72.DAT" },
    { "Qufim Island", "ROM/0/58.DAT" },
    { "Konschtat Highlands", "ROM/0/90.DAT" },
    { "Ru'Aun Gardens", "ROM2/12/107.DAT" },
    { "The Sanctuary of Zi'Tah", "ROM2/0/2.DAT" },
    { "Yuhtunga Jungle", "ROM2/0/4.DAT" },
    { "Al'Taieu", "ROM3/0/32.DAT" },
    { "Carpenters' Landing", "ROM3/0/1.DAT" },
    { "Bibiki Bay", "ROM3/0/3.DAT" },
    { "Riverne - Site #B01", "ROM3/0/28.DAT" },
    { "Aht Urhgan Whitegate", "ROM4/0/3.DAT" },
    { "Wajaom Woodlands", "ROM4/1/99.DAT" },
    { "Mount Zhayolm", "ROM4/0/14.DAT" },
    { "Aydeewa Subterrane", "ROM4/0/21.DAT" },
};

const ApplicationSettings::TitleMusicOption kTitleMusicOptions[] =
{
    { 108, "Vana'diel March" },
    { 242, "Unity" },
    { 179, "Vana'diel March #4" },
    { 140, "Vana'diel March #5" },
    { 58, "A New Direction" },
};

template <typename T, std::size_t Size>
constexpr int ArrayCount(const T (&)[Size])
{
    return static_cast<int>(Size);
}
}

namespace ApplicationSettings
{
int ClampMovementStyle(const int style)
{
    return style >= MovementRetail && style <= MovementClickToMove ? style : MovementDATura;
}

const char* MovementStyleName(const int style)
{
    switch (ClampMovementStyle(style))
    {
    case MovementRetail: return "Controller (retail)";
    case MovementClickToMove: return "Click to Move";
    default: return "WoW style";
    }
}

int ResolutionOptionCount()
{
    return ArrayCount(kResolutionOptions);
}

const ResolutionOption& ResolutionOptionAt(const int index)
{
    return kResolutionOptions[ClampResolutionIndex(index)];
}

int ClampResolutionIndex(const int index)
{
    return index >= 0 && index < ResolutionOptionCount() ? index : 0;
}

int ClampWindowMode(const int mode)
{
    return mode >= Windowed && mode <= Fullscreen ? mode : Windowed;
}

int MaxSoundOptionCount()
{
    return ArrayCount(kMaxSoundOptions);
}

int MaxSoundOptionValue(const int index)
{
    const int safeIndex = index >= 0 && index < MaxSoundOptionCount()
        ? index
        : MaxSoundOptionCount() - 1;
    return kMaxSoundOptions[safeIndex];
}

const char* MaxSoundOptionLabel(const int index)
{
    const int safeIndex = index >= 0 && index < MaxSoundOptionCount()
        ? index
        : MaxSoundOptionCount() - 1;
    return kMaxSoundOptionLabels[safeIndex];
}

int MaxSoundOptionIndex(const int value)
{
    for (int i = 0; i < MaxSoundOptionCount(); ++i)
    {
        if (kMaxSoundOptions[i] == value)
            return i;
    }
    return MaxSoundOptionCount() - 1;
}

int DrawDistanceOptionCount()
{
    return ArrayCount(kDrawDistanceOptions);
}

float DrawDistanceOptionValue(const int index)
{
    return kDrawDistanceOptions[ClampDrawDistanceIndex(index)];
}

const char* DrawDistanceOptionLabel(const int index)
{
    return kDrawDistanceOptionLabels[ClampDrawDistanceIndex(index)];
}

bool DrawDistanceOptionIsUnlimited(const int index)
{
    return ClampDrawDistanceIndex(index) == DrawDistanceOptionCount() - 1;
}

int ClampDrawDistanceIndex(const int index)
{
    return index >= 0 && index < DrawDistanceOptionCount() ? index : 4;
}

int ClampEnvironmentalAnimationMode(const int mode)
{
    if (mode < EnvironmentalAnimationOff)
        return EnvironmentalAnimationOff;
    if (mode > EnvironmentalAnimationSmooth)
        return EnvironmentalAnimationSmooth;
    return mode;
}

const char* EnvironmentalAnimationModeName(const int mode)
{
    switch (ClampEnvironmentalAnimationMode(mode))
    {
    case EnvironmentalAnimationOff:
        return "Off";
    case EnvironmentalAnimationSimple:
        return "Simple";
    case EnvironmentalAnimationSmooth:
    default:
        return "Smooth";
    }
}

int ClampLightingQuality(const int quality)
{
    if (quality < LightingOff)
        return LightingOff;
    if (quality > LightingDynamicShadows)
        return LightingDynamicShadows;
    return quality;
}

const char* LightingQualityName(const int quality)
{
    switch (ClampLightingQuality(quality))
    {
    case LightingOff: return "Off";
    case LightingSimplified: return "Simplified";
    case LightingDynamicShadows:
    default: return "Dynamic Shadows";
    }
}

int ClampLightDirectionMode(const int mode)
{
    return mode == LightDirectionManual ? LightDirectionManual : LightDirectionAutomatic;
}

int ClampLightAzimuth(const int degrees)
{
    return std::max(-180, std::min(180, degrees));
}

int ClampLightElevation(const int degrees)
{
    return std::max(-80, std::min(89, degrees));
}

int ClampBumpMappingIntensity(const int percent)
{
    return std::max(0, std::min(300, percent));
}

int ClampSceneClockMode(const int mode)
{
    return mode == SceneClockLocalSystem ? SceneClockLocalSystem : SceneClockVanadiel;
}

int RenderingBackendOptionCount()
{
    return ArrayCount(kRenderingBackendNames);
}

int ClampRenderingBackend(const int backend)
{
    return backend >= 0 && backend < RenderingBackendOptionCount()
        ? backend
        : RenderingBackendDirectX9;
}

const char* RenderingBackendName(const int backend)
{
    return kRenderingBackendNames[ClampRenderingBackend(backend)];
}

bool RenderingBackendIsAvailable(const int backend)
{
    return backend == RenderingBackendDirectX9;
}

int RenderingResolutionScaleOptionCount()
{
    return ArrayCount(kRenderingResolutionScales);
}

int RenderingResolutionScalePercent(const int index)
{
    return kRenderingResolutionScales[ClampRenderingResolutionScaleIndex(index)];
}

const char* RenderingResolutionScaleLabel(const int index)
{
    return kRenderingResolutionScaleLabels[ClampRenderingResolutionScaleIndex(index)];
}

int ClampRenderingResolutionScaleIndex(const int index)
{
    return index >= 0 && index < RenderingResolutionScaleOptionCount() ? index : 2;
}

int AntiAliasingOptionCount()
{
    return ArrayCount(kAntiAliasingSamples);
}

int AntiAliasingSamples(const int index)
{
    return kAntiAliasingSamples[ClampAntiAliasingIndex(index)];
}

const char* AntiAliasingLabel(const int index)
{
    return kAntiAliasingLabels[ClampAntiAliasingIndex(index)];
}

int ClampAntiAliasingIndex(const int index)
{
    return index >= 0 && index < AntiAliasingOptionCount() ? index : 0;
}

int PostProcessAntiAliasingOptionCount()
{
    return ArrayCount(kPostProcessAntiAliasingLabels);
}

const char* PostProcessAntiAliasingLabel(const int mode)
{
    return kPostProcessAntiAliasingLabels[ClampPostProcessAntiAliasingMode(mode)];
}

int ClampPostProcessAntiAliasingMode(const int mode)
{
    return mode >= 0 && mode < PostProcessAntiAliasingOptionCount() ? mode : 0;
}

int ClampTitleBackgroundMode(const int mode)
{
    return mode >= TitleBackgroundRandom && mode <= TitleBackgroundFixed
        ? mode : TitleBackgroundRandom;
}

const char* TitleBackgroundModeName(const int mode)
{
    switch (ClampTitleBackgroundMode(mode))
    {
    case TitleBackgroundCycle: return "Cycle";
    case TitleBackgroundFixed: return "Specific Zone";
    default: return "Random";
    }
}

int TitleBackgroundOptionCount()
{
    return ArrayCount(kTitleBackgroundOptions);
}

int ClampTitleBackgroundZoneIndex(const int index)
{
    return index >= 0 && index < TitleBackgroundOptionCount() ? index : 8;
}

const TitleBackgroundOption& TitleBackgroundOptionAt(const int index)
{
    return kTitleBackgroundOptions[ClampTitleBackgroundZoneIndex(index)];
}

int ClampTitleMusicMode(const int mode)
{
    return mode >= TitleMusicRandom && mode <= TitleMusicFixed ? mode : TitleMusicRandom;
}

const char* TitleMusicModeName(const int mode)
{
    switch (ClampTitleMusicMode(mode))
    {
    case TitleMusicCycle: return "Cycle";
    case TitleMusicFixed: return "Specific Track";
    default: return "Random";
    }
}

int TitleMusicOptionCount()
{
    return ArrayCount(kTitleMusicOptions);
}

int ClampTitleMusicIndex(const int index)
{
    return index >= 0 && index < TitleMusicOptionCount() ? index : 0;
}

const TitleMusicOption& TitleMusicOptionAt(const int index)
{
    return kTitleMusicOptions[ClampTitleMusicIndex(index)];
}
}
