#include "stdafx.h"
#include "zone_model_transform.h"

#include "d3d_model_buffers.h"

#include <algorithm>

namespace ZoneModelTransform
{
void MirrorOnX(noesisModel_t* model, IDirect3DDevice9* device)
{
    if (!model)
        return;

    const bool rebuildSharedBuffers = !model->staticBufferGroups.empty();
    for (noesisModel_t::Submesh& submesh : model->submeshes)
    {
        for (FFXIVertex& vertex : submesh.cpuVerts)
        {
            vertex.pos[0] = -vertex.pos[0];
            vertex.nrm[0] = -vertex.nrm[0];
        }
        for (FFXIVertex& vertex : submesh.cpuBindVerts)
        {
            vertex.pos[0] = -vertex.pos[0];
            vertex.nrm[0] = -vertex.nrm[0];
        }
        for (size_t index = 0; index + 2 < submesh.cpuIndices.size(); index += 3)
            std::swap(submesh.cpuIndices[index + 1], submesh.cpuIndices[index + 2]);

        if (!rebuildSharedBuffers)
        {
            D3DModelBuffers::UploadVertices(submesh);
            D3DModelBuffers::UploadIndices(submesh);
        }
    }
    model->UpdateSubmeshBounds();
    model->renderMetadataPrepared = false;
    if (rebuildSharedBuffers)
    {
        model->ReleaseD3DBuffers();
        model->BuildD3DBuffers(device);
    }
}
}
