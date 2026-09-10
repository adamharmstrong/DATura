#pragma once

#include <d3d9.h>

#include "noesis_rapi.h"
#include "d3d_model_render_state.h"
#include "zone_object_visibility.h"

struct noesisModel_t;

namespace ModelRenderer
{
enum class GeometryPass { All, Opaque, Transparent };

struct Context
{
    IDirect3DDevice9* device = nullptr;
    GeometryPass geometryPass = GeometryPass::All;
    int minuteOfDay = 0;
    int lightingQuality = 0;
    int vegetationAnimationMode = 0;
    bool rendersZoneObjects = false;
    bool dynamicActorShadows = false;
    bool enableMipMapping = false;
    bool indoorZone = false;
    bool waterRenderingEnabled = true;
    // Negative selects the runtime clock; tests can request an exact animation frame.
    double animationSeconds = -1.0;
    // Borrowed for the duration of a draw; the application owns these collections.
    ZoneObjectVisibility::RenderContext visibility;
    float cameraPosition[3] = {};
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
