#include "stdafx.h"
#include "player_model_loader.h"

#include "character_dat_table.h"
#include "ffxi_parser_diagnostics.h"
#include "noesis_rapi.h"
#include "model_ff11.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace
{
float ComputeGroundOffset(
    const noesisModel_t* model, const float footContactAdjustment) noexcept
{
    if (!model)
        return 0.0f;

    bool haveVertex = false;
    float minimumY = 0.0f;
    for (const noesisModel_t::Submesh& submesh : model->submeshes)
    {
        for (const FFXIVertex& vertex : submesh.cpuVerts)
        {
            if (!haveVertex || vertex.pos[1] < minimumY)
            {
                minimumY = vertex.pos[1];
                haveVertex = true;
            }
        }
    }
    return haveVertex ? -minimumY + footContactAdjustment : 0.0f;
}

float ComputeCameraTargetLocalY(const noesisModel_t* model) noexcept
{
    if (!model)
        return -2.0f;

    bool haveVertex = false;
    float topY = 0.0f;
    float bottomY = 0.0f;
    for (const noesisModel_t::Submesh& submesh : model->submeshes)
    {
        // Composite character DATs can retain component-local bind vertices;
        // prefer the current posed vertices for the assembled silhouette.
        const std::vector<FFXIVertex>& vertices =
            !submesh.cpuVerts.empty() ? submesh.cpuVerts : submesh.cpuBindVerts;
        for (const FFXIVertex& vertex : vertices)
        {
            if (!haveVertex)
            {
                topY = bottomY = vertex.pos[1];
                haveVertex = true;
            }
            else
            {
                topY = std::min(topY, vertex.pos[1]);
                bottomY = std::max(bottomY, vertex.pos[1]);
            }
        }
    }

    if (!haveVertex)
        return -2.0f;

    const float height = bottomY - topY;
    return topY + std::clamp(height * 0.085f, 0.35f, 1.25f);
}
}

namespace PlayerModelLoader
{
bool Result::Succeeded() const noexcept
{
    return error == Error::None && static_cast<bool>(asset);
}

Result Load(IDirect3DDevice9* device, const Request& request)
{
    Result result;
    if (!device || !request.ffxiRoot || !request.ffxiRoot[0] ||
        request.customization.raceIndex < 0 ||
        request.customization.raceIndex >= kFFXICharRaceCount)
    {
        result.error = Error::InvalidRequest;
        return result;
    }

    const FFXICharRace& race = kFFXICharRaces[request.customization.raceIndex];
    if (race.count < 8)
    {
        result.error = Error::IncompleteRaceEntry;
        return result;
    }

    char datSet[4096] = {};
    if (!FFXIDatSet::BuildPlayer(
            request.ffxiRoot, request.customization, datSet, sizeof(datSet)))
    {
        result.error = Error::InvalidRequest;
        return result;
    }

    result.asset.Adopt(nullptr, new noeRAPI_t(device));
    result.asset.ParserContext()->SetTextureCompressionEnabled(
        request.enableTextureCompression);
    result.asset.ParserContext()->SetCurrentFilePath(
        "DATura generated player.ff11datset");

    noesisModel_t* model = nullptr;
    {
        // Player DAT parsing reuses global FF11 diagnostic vectors. Preserve
        // the active zone's environment, sky, cloud, and fog records.
        FFXIParserDiagnostics::ScopedSnapshot parserDiagnostics;
        model = Model_FF11_LoadDATSet(
            reinterpret_cast<BYTE*>(datSet), static_cast<int>(std::strlen(datSet)),
            result.modelCount, result.asset.ParserContext());
    }
    if (!model || result.modelCount == 0)
    {
        result.asset.AttachModel(model);
        result.asset.Reset();
        result.error = Error::NoDisplayableGeometry;
        result.modelCount = 0;
        return result;
    }

    result.asset.AttachModel(model);
    result.groundOffset =
        ComputeGroundOffset(model, request.footContactAdjustment);
    result.cameraTargetLocalY = ComputeCameraTargetLocalY(model);
    return result;
}
}
