#pragma once

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
    int resolutionIndex = 0;
    int environmentalAnimationMode = EnvironmentalAnimationSmooth;
    bool enableSounds = true;
    bool playSoundsInBackground = true;
    int maxSimultaneousSounds = -1;
    bool enableHardwareMouseCursor = true;
    bool enableMipMapping = true;
    bool enableBumpMapping = false;
    int lightingQuality = LightingDynamicShadows;
    int drawDistanceIndex = 4;
    bool enableTextureCompression = true;
    bool mirrorWorldZones = false;
    bool showCollisionGeometry = false;
    int renderingBackend = RenderingBackendDirectX9;
};

int ResolutionOptionCount();
const ResolutionOption& ResolutionOptionAt(int index);
int ClampResolutionIndex(int index);
int ClampWindowMode(int mode);

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
int RenderingBackendOptionCount();
int ClampRenderingBackend(int backend);
const char* RenderingBackendName(int backend);
bool RenderingBackendIsAvailable(int backend);
}
