#pragma once

#include <d3d9.h>
#include <vector>

#include "noesis_rapi.h"
#include "d3d_model_render_state.h"
#include "zone_object_visibility.h"

struct noesisModel_t;
struct ff11GeneratorRecord_t;
struct ff11KeyframeRecord_t;

namespace ModelRenderer
{
enum class GeometryPass { All, Opaque, Transparent };

struct Context
{
    IDirect3DDevice9* device = nullptr;
    GeometryPass geometryPass = GeometryPass::All;
    int minuteOfDay = 0;
    int lightingQuality = 0;
    bool automaticLightDirection = true;
    float lightAzimuthDegrees = 0.0f;
    float lightElevationDegrees = 0.0f;
    bool useAuthoredLightColor = false;
    D3DCOLOR authoredMainLightColor = 0;
    D3DCOLOR authoredSecondaryLightColor = 0;
    D3DCOLOR authoredAmbientLightColor = 0;
    float authoredLightPower = 1.0f;
    bool useAuthoredFog = false;
    D3DCOLOR authoredFogColor = 0;
    float authoredFogNear = 0.0f;
    float authoredFogFar = 0.0f;
    bool useAuthoredLightDirection = false;
    float authoredMainLightDirection[3] = {};
    const std::vector<ff11GeneratorRecord_t>* authoredGenerators = nullptr;
    const std::vector<ff11KeyframeRecord_t>* authoredKeyframes = nullptr;
    bool mirrorAuthoredLightX = false;
    int vegetationAnimationMode = 0;
    bool rendersZoneObjects = false;
    bool dynamicActorShadows = false;
    bool dynamicObjectShadows = false;
    bool shadowGroundLikeObjects = false;
    bool shadowAlphaTestedObjects = false;
    float shadowMaxDistance = 80.0f;
    int shadowObjectLimit = 64;
    float shadowMinimumSize = 0.25f;
    float shadowMaximumSize = 40.0f;
    int shadowReceiverUpdateFrames = 4;
    float shadowMaximumLength = 30.0f;
    float shadowOpacity = 0.32f;
    bool shadowDebugVisualization = false;
    bool shadowPerformanceCounters = false;
    bool enableMipMapping = false;
    bool enableBumpMapping = false;
    float bumpMappingIntensity = 1.0f;
    bool indoorZone = false;
    bool waterRenderingEnabled = true;
    // Negative selects the runtime clock; tests can request an exact animation frame.
    double animationSeconds = -1.0;
    // Borrowed for the duration of a draw; the application owns these collections.
    ZoneObjectVisibility::RenderContext visibility;
    float cameraPosition[3] = {};
    bool (*findZoneShadowReceiverY)(float x, float y, float z, float* outY) = nullptr;
};

struct TextureScrollState
{
    bool enabled = false;
    float speedU = 0.0f;
    float speedV = 0.0f;

    void Reset(IDirect3DDevice9* device);
    void Update(IDirect3DDevice9* device, bool enabled, float speedU, float speedV);
};

void DrawActorPlanarShadow(const Context& context, noesisModel_t* model,
                           const D3DMATRIX& baseWorld);
void DrawZoneObjectPlanarShadows(const Context& context, noesisModel_t* model,
                                 const D3DMATRIX& baseWorld);
void PrepareFixedFunctionPass(const Context& context, const D3DMATRIX& baseWorld);
// Draw the requested geometry pass and restore the model-pass baseline.
// Split zone passes around actors so water blends with the completed opaque scene.
// Metadata and fixed-function state must be prepared.
void DrawGeometry(const Context& context, noesisModel_t* model,
                  const D3DMATRIX& baseWorld);
void DrawOpaqueBatch(const Context& context, D3DModelRenderState::MaterialBindingCache& cache,
                     TextureScrollState& textureAnimation, noesisModel_t* model,
                     const noesisModel_t::OpaqueBatch& batch);
void RestoreModelPassState(IDirect3DDevice9* device, const D3DMATRIX& baseWorld,
                           TextureScrollState& textureAnimation);
}
