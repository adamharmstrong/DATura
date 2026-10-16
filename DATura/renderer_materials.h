#pragma once

// Asset metadata is independent of the graphics API and remains borrowed.
struct noesisMaterial_t;
struct noesisTex_t;

namespace RendererBackend
{
struct MaterialBindingCache
{
    const noesisMaterial_t* material = nullptr;
    const noesisTex_t* texture = nullptr;
    bool bumpMappingEnabled = false;
    float bumpMappingIntensity = 1.0f;
    bool valid = false;

    void Invalidate() { *this = MaterialBindingCache{}; }
};

// The pass owns blend/depth state. Material binding preserves the original FFXI
// alpha, culling, texture replacement and shader-fallback rules within that pass.
// Invalidate the cache whenever another draw changes material state.
class Materials
{
public:
    virtual ~Materials() = default;
    virtual void ApplyOpaque(MaterialBindingCache&, const noesisMaterial_t*,
        const noesisTex_t*, bool bumpMapping = false, float bumpIntensity = 1.0f) = 0;
    virtual void ApplyTransparent(MaterialBindingCache&, const noesisMaterial_t*,
        const noesisTex_t*, bool bumpMapping = false, float bumpIntensity = 1.0f) = 0;
};
}
