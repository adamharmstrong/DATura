#pragma once

#include "noesis_rapi.h"

namespace D3DModelBuffers
{
    void UploadVertices(noesisModel_t::Submesh& submesh);
    void UploadIndices(noesisModel_t::Submesh& submesh);
    void BuildStaticOpaqueBatches(noesisModel_t *model, IDirect3DDevice9 *device);
    bool HasDrawBuffers(const noesisModel_t *model, const noesisModel_t::Submesh& submesh);
    void DrawSubmesh(IDirect3DDevice9 *device, const noesisModel_t *model,
                     const noesisModel_t::Submesh& submesh);
    void DrawOpaqueBatch(IDirect3DDevice9 *device, const noesisModel_t *model,
                         const noesisModel_t::OpaqueBatch& batch);
}
