#pragma once

// Generator-owned water is an explicitly linked world resource. Keep this
// policy separate from weather shells and the opt-in unreferenced mesh view.
#include "model_ff11.h"
#include "zone_water_data.h"

#include <cmath>
#include <memory>
#include <string>

namespace FF11Water
{
inline std::string TrimmedName(const char *name, size_t length)
{
    size_t used = 0;
    while (used < length && name[used]) ++used;
    while (used > 0 && name[used - 1] == ' ') --used;
    std::string result(name, used);
    for (char &c : result)
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    return result;
}

inline size_t EffectRootLength(const std::string &path)
{
    size_t start = 0;
    while (start < path.size())
    {
        const size_t end = path.find('/', start);
        const size_t length = (end == std::string::npos ? path.size() : end) - start;
        if (length == 4 && _strnicmp(path.c_str() + start, "effe", 4) == 0)
            return start + length;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return 0;
}

// Resources may live beside their generator or in a parent effect directory.
// Never resolve into a sibling effect, another weather group, or outside effe.
inline size_t ResourceScopeRank(const char *generatorDirectory, const char *resourceDirectory)
{
    const std::string owner = generatorDirectory ? generatorDirectory : "";
    const std::string resource = resourceDirectory ? resourceDirectory : "";
    const size_t rootLength = EffectRootLength(owner);
    if (!rootLength || resource.size() < rootLength || resource.size() > owner.size() ||
        _strnicmp(owner.c_str(), resource.c_str(), resource.size()) != 0 ||
        (resource.size() != owner.size() && owner[resource.size()] != '/'))
        return 0;
    return resource.size();
}

inline bool IsSupportedGenerator(const ff11GeneratorRecord_t &generator)
{
    // 0x0B is the standard element's MMB resource subtype. Only persistent,
    // automatically started world placements are supported in this first pass.
    if (!EffectRootLength(generator.directoryPath) ||
        Model_FF11_IsWeatherDirectory(generator.directoryPath) ||
        !generator.hasStandardParticleSetup || !generator.linkedResource[0] ||
        generator.linkedDataType != 0x0B || generator.attachFlags != 0 ||
        !(generator.generatorFlags & 0x10) ||
        generator.particleLifetimeFrames != 0 || !generator.hasSpawnPosition ||
        generator.hasAnimatedScale || generator.hasLinearVelocity ||
        generator.hasLinearAcceleration || generator.hasRotationVelocity)
        return false;
    for (int axis = 0; axis < 3; ++axis)
        if (!std::isfinite(generator.spawnPosition[axis]) ||
            (generator.hasRotation && !std::isfinite(generator.rotation[axis])) ||
            (generator.hasScale && !std::isfinite(generator.scale[axis])))
            return false;
    return true;
}

inline RichMat43 PlacementTransform(const ff11GeneratorRecord_t &generator)
{
    // Same raw FFXI frame and XYZ radians as an MZB placement. Conversion into
    // DATura's view is already applied by the scene; do not reflect axes here.
    RichMat43 transform = generator.hasRotation ?
        RichAngles(generator.rotation, true).ToMat43_XYZ() : RichMat43();
    for (int axis = 0; axis < 3; ++axis)
        transform[axis] *= generator.hasScale ? generator.scale[axis] : 1.0f;
    // Zero scale is authored: Valkurm's sea uses (6, 0, 6).
    transform[3] = RichVec3(generator.spawnPosition[0], generator.spawnPosition[1], generator.spawnPosition[2]);
    return transform;
}

inline bool BackwardWinding(const ff11GeneratorRecord_t &generator)
{
    return generator.hasScale && generator.scale[0] * generator.scale[1] * generator.scale[2] < 0.0f;
}

inline std::string ObjectIdentity(const ff11GeneratorRecord_t &generator,
                                  const char *resource, const char *objectName)
{
    char identity[512];
    const std::string object = TrimmedName(objectName, 16);
    sprintf_s(identity, "water: %s/@%s/%s#%08X/%s", generator.directoryPath,
              resource, generator.name, generator.sourceDataOffset, object.c_str());
    return identity;
}

inline bool IsSeaSurfaceBatch(const char *objectName, const char *materialName)
{
    const std::string object = TrimmedName(objectName, 16);
    return TrimmedName(materialName, 16) == "effect  umi0" &&
        (object == "umi0" || object == "umif" || object == "ukro");
}

inline bool IsSurfaceBatch(const char *objectName, const char *materialName)
{
    const std::string object = TrimmedName(objectName, 16);
    const std::string material = TrimmedName(materialName, 16);
    // Confirmed in installed East Ronfaure: generator-linked river surfaces.
    // Deliberately do not treat every transparent effect or a name containing
    // "water" as a surface; fountains, spray and fire use the same envelope.
    if (object == "umi0" || object == "umif" || object == "ukro")
        return IsSeaSurfaceBatch(objectName, materialName);
    if (material == "effect  kaw1") return true;
    if (material == "sea     sea01" && (object == "allsea" || object == "lowsea"))
        return true;
    return material.empty() && (object == "mizu" || object == "funmiz");
}

inline const ff11KeyframeRecord_t *FindColorCurve(const ff11GeneratorRecord_t &generator,
                                                const char *name)
{
    if (!name || !name[0]) return nullptr;
    const ff11KeyframeRecord_t *selected = nullptr;
    size_t bestRank = 0;
    bool ambiguous = false;
    for (const auto &curve : gFF11LastKeyframeRecords)
    {
        if (TrimmedName(curve.name, sizeof(curve.name)) != TrimmedName(name, 4)) continue;
        const size_t rank = ResourceScopeRank(generator.directoryPath, curve.directoryPath);
        if (rank > bestRank)
        {
            selected = &curve;
            bestRank = rank;
            ambiguous = false;
        }
        else if (rank && rank == bestRank) ambiguous = true;
    }
    return ambiguous ? nullptr : selected;
}

inline std::shared_ptr<ZoneWater::Surface> BuildSurface(const ff11GeneratorRecord_t &generator)
{
    auto surface = std::make_shared<ZoneWater::Surface>();
    surface->directory = generator.directoryPath;
    surface->resource = generator.linkedResource;
    surface->generator = generator.name;
    surface->sourceOffset = generator.sourceDataOffset;
    // A collapsed axis has no determinant sign. After a later scene mirror,
    // culling cannot safely infer its facing from the authored scale product.
    // Preserve these valid planar sheets by rendering both faces.
    if (generator.hasScale)
        for (int axis = 0; axis < 3; ++axis)
            surface->twoSided |= generator.scale[axis] == 0.0f;
    if (generator.hasColor)
    {
        surface->colorScale[0] = ((generator.colorBgra >> 16) & 0xff) / 128.0f;
        surface->colorScale[1] = ((generator.colorBgra >> 8) & 0xff) / 128.0f;
        surface->colorScale[2] = (generator.colorBgra & 0xff) / 128.0f;
        surface->colorScale[3] = ((generator.colorBgra >> 24) & 0xff) / 64.0f;
    }
    const char *names[] = { generator.redKeyframe, generator.greenKeyframe,
                            generator.blueKeyframe, generator.alphaKeyframe };
    for (int channel = 0; channel < 4; ++channel)
    {
        const auto *curve = FindColorCurve(generator, names[channel]);
        if (!curve) continue;
        for (int index = 0; index < curve->pairCount; ++index)
        {
            const float time = curve->times[index];
            const float value = curve->values[index];
            auto &keys = surface->colorCurves[channel].keys;
            if (!std::isfinite(time) || !std::isfinite(value) || time < 0.0f || time > 1.0f ||
                (!keys.empty() && time < keys.back().first))
                break;
            keys.emplace_back(time, value);
            if (time == 1.0f) break; // remaining DAT chunk bytes are alignment padding
        }
    }
    if (generator.hasUvScrollU && std::isfinite(generator.uvScrollU))
        surface->uvVelocity[0] = generator.uvScrollU * 60.0f;
    if (generator.hasUvScrollV && std::isfinite(generator.uvScrollV))
        surface->uvVelocity[1] = generator.uvScrollV * 60.0f;
    if (generator.hasBlendMode) surface->blendMode = generator.blendMode;
    if (generator.hasCullDistance && std::isfinite(generator.cullDistance) && generator.cullDistance > 0.0f)
        surface->cullDistance = generator.cullDistance;
    return surface;
}
}
