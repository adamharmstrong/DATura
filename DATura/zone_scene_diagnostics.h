#pragma once

#include "zone_coverage.h"
#include "zone_object_transform.h"
#include <array>
#include <iosfwd>
#include <string>
#include <vector>

struct noesisModel_t;
namespace ZoneCollision { class Mesh; }

namespace ZoneSceneDiagnostics
{
struct Snapshot
{
    std::string source;
    bool mirrorX = false;
    std::array<float, 3> nativeViewer = {};
    std::vector<ZoneCoverage::Triangle> triangles;
    size_t includedMeshes = 0, environmentMeshes = 0, hiddenMeshes = 0, lodMeshes = 0;
    size_t missingCpuMeshes = 0, invalidIndexTriangles = 0, trailingIndices = 0;
    size_t opaqueMeshes = 0, cutoutMeshes = 0, transparentMeshes = 0, waterMeshes = 0;
    size_t unresolvedMaterials = 0, unresolvedTextures = 0, transformedMeshes = 0;
    size_t untexturedMeshes = 0;
};

// Call on the scene thread. The returned snapshot owns all data and can safely
// outlive a zone reload. Model vertices and collision are already scene-space.
Snapshot Capture(const noesisModel_t& model, const ZoneCollision::Mesh& collision,
    const std::vector<std::string>& hidden,
    const std::map<std::string, ZoneObjectTransform::DebugTransform>& overrides,
    const std::string& source, bool mirrorX, const std::array<float, 3>& nativeViewer);

std::string Summary(const Snapshot& snapshot, const ZoneCoverage::Options& options,
                    const ZoneCoverage::Result& result);
void WriteHtml(std::ostream& out, const Snapshot& snapshot,
               const ZoneCoverage::Options& options, const ZoneCoverage::Result& result);
void WriteCsv(std::ostream& out, const ZoneCoverage::Result& result);
}
