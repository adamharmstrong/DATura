#pragma once

#include <string>

namespace ApplicationSettings
{
enum WindowMode
{
    Windowed = 0,
    Borderless = 1,
    Fullscreen = 2
};

enum EnvironmentalAnimationMode
{
    EnvironmentalAnimationOff = 0,
    EnvironmentalAnimationSimple = 1,
    EnvironmentalAnimationSmooth = 2
};

enum ColorTheme
{
    DarkTheme = 0,
    LightTheme = 1
};

enum LightingQuality
{
    LightingOff = 0,
    LightingSimplified = 1,
    LightingDynamicShadows = 2
};

enum LightDirectionMode
{
    LightDirectionAutomatic = 0,
    LightDirectionManual = 1
};

enum SceneClockMode
{
    SceneClockVanadiel = 0,
    SceneClockLocalSystem = 1
};

enum RenderingBackend
{
    RenderingBackendDirectX8 = 0,
    RenderingBackendDirectX9 = 1,
    RenderingBackendDirectX11 = 2,
    RenderingBackendDirectX12 = 3,
    RenderingBackendOpenGL = 4,
    RenderingBackendVulkan = 5,
    RenderingBackendMetal = 6
};

enum DoorInteractionMode
{
    DoorClassic = 0,
    DoorPhysics = 1
};

enum MouseCursorStyle
{
    MouseCursorWindows = 0,
    MouseCursorFfxiAnimated = 1,
    MouseCursorFfxiStatic = 2,
    MouseCursorFfxiInteraction = 3
};

enum MovementStyle
{
    MovementRetail = 0,
    MovementDATura = 1,
    MovementClickToMove = 2
};

enum TitleBackgroundMode
{
    TitleBackgroundRandom = 0,
    TitleBackgroundCycle = 1,
    TitleBackgroundFixed = 2
};

enum TitleMusicMode
{
    TitleMusicRandom = 0,
    TitleMusicCycle = 1,
    TitleMusicFixed = 2
};

struct TitleBackgroundOption
{
    const char* name;
    const char* modelDat;
};

struct TitleMusicOption
{
    int musicId;
    const char* name;
};

struct ResolutionOption
{
    int width;
    int height;
    const char* label;
};

struct State
{
    int doorInteractionMode = DoorClassic;
    int windowMode = Windowed;
    // Keep the historical application default at 1280x720. The resolution
    // catalog is ordered from smaller compatibility modes upward, so this is
    // not the first catalog entry.
    int resolutionIndex = 6;
    int renderingResolutionScaleIndex = 2;
    int antiAliasingIndex = 0;
    int postProcessAntiAliasingMode = 0;
    int environmentalAnimationMode = EnvironmentalAnimationSmooth;
    bool enableSounds = true;
    bool playSoundsInBackground = true;
    int maxSimultaneousSounds = -1;
    bool enableHardwareMouseCursor = true;
    int mouseCursorStyle = MouseCursorWindows;
    bool enableMipMapping = true;
    bool enableBumpMapping = true;
    bool invertBumpMapping = false;
    int bumpMappingIntensityPercent = 100;
    bool enableHdTextures = false;
    std::string hdTextureFolder;
    bool enablePbr = false;
    std::string pbrTextureFolder;
    int lightingQuality = LightingDynamicShadows;
    int lightDirectionMode = LightDirectionAutomatic;
    int lightAzimuthDegrees = 0;
    int lightElevationDegrees = 45;
    int lightAzimuthOffsetDegrees = 0;
    int lightElevationOffsetDegrees = 0;
    int sceneClockMode = SceneClockVanadiel;
    bool shadowPlayer = true;
    bool shadowNpcs = true;
    bool shadowObjects = true;
    bool shadowGroundLikeObjects = false;
    bool shadowAlphaTestedObjects = false;
    int shadowMaxDistance = 80;
    int shadowNpcLimit = 16;
    int shadowObjectLimit = 64;
    int shadowMinimumSizePercent = 25;
    int shadowMaximumSize = 40;
    int shadowReceiverUpdateFrames = 4;
    int shadowReceiverQuality = 1;
    int shadowMaximumLength = 30;
    int shadowOpacityPercent = 32;
    bool shadowDebugVisualization = false;
    bool shadowPerformanceCounters = false;
    int drawDistanceIndex = 4;
    bool enableTextureCompression = true;
    bool enableMapTextureCompression = true;
    bool enableWeatherEffects = true;
    bool mirrorWorldZones = false;
    bool showCollisionGeometry = false;
    int movementStyle = MovementDATura;
    int renderingBackend = RenderingBackendDirectX9;
    int titleBackgroundMode = TitleBackgroundRandom;
    int titleBackgroundZoneIndex = 8; // Konschtat Highlands
    bool showTitleUi = true;
    int titleMusicMode = TitleMusicRandom;
    int titleMusicIndex = 0;
    // Virtual-key codes used by gameplay input. These remain staged with the
    // rest of the Config dialog and are committed by Apply/Okay.
    int keyForward = 'W';
    int keyBackward = 'S';
    int keyLeft = 'A';
    int keyRight = 'D';
    int keyJump = 0x20;
    int keyAutoRun = 'R';
    int keyRunToggle = 0x10;
    int keyGameMode = 'F';
    int keyZoneMap = 'M';
    int keyUnstick = 'G';
    int keyCycleWeather = 'V';
    int keyCameraDebug = 'T';
    int keyBumpMapping = 'P';
    int keyMainMenu = 0xBD; // VK_OEM_MINUS
};

int ResolutionOptionCount();
const ResolutionOption& ResolutionOptionAt(int index);
int ClampResolutionIndex(int index);
int RenderingResolutionScaleOptionCount();
int RenderingResolutionScalePercent(int index);
const char* RenderingResolutionScaleLabel(int index);
int ClampRenderingResolutionScaleIndex(int index);
int AntiAliasingOptionCount();
int AntiAliasingSamples(int index);
const char* AntiAliasingLabel(int index);
int ClampAntiAliasingIndex(int index);
int PostProcessAntiAliasingOptionCount();
const char* PostProcessAntiAliasingLabel(int mode);
int ClampPostProcessAntiAliasingMode(int mode);
int ClampWindowMode(int mode);
int ClampMovementStyle(int style);
const char* MovementStyleName(int style);

int MaxSoundOptionCount();
int MaxSoundOptionValue(int index);
const char* MaxSoundOptionLabel(int index);
int MaxSoundOptionIndex(int value);

int DrawDistanceOptionCount();
float DrawDistanceOptionValue(int index);
const char* DrawDistanceOptionLabel(int index);
bool DrawDistanceOptionIsUnlimited(int index);
int ClampDrawDistanceIndex(int index);

int ClampEnvironmentalAnimationMode(int mode);
const char* EnvironmentalAnimationModeName(int mode);
int ClampLightingQuality(int quality);
const char* LightingQualityName(int quality);
int ClampLightDirectionMode(int mode);
int ClampLightAzimuth(int degrees);
int ClampLightElevation(int degrees);
int ClampBumpMappingIntensity(int percent);
int ClampSceneClockMode(int mode);
int RenderingBackendOptionCount();
int ClampRenderingBackend(int backend);
const char* RenderingBackendName(int backend);
bool RenderingBackendIsAvailable(int backend);
int ClampTitleBackgroundMode(int mode);
const char* TitleBackgroundModeName(int mode);
int TitleBackgroundOptionCount();
const TitleBackgroundOption& TitleBackgroundOptionAt(int index);
int ClampTitleBackgroundZoneIndex(int index);
int ClampTitleMusicMode(int mode);
const char* TitleMusicModeName(int mode);
int TitleMusicOptionCount();
const TitleMusicOption& TitleMusicOptionAt(int index);
int ClampTitleMusicIndex(int index);
}
