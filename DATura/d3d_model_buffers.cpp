#include "stdafx.h"
#include "d3d_model_buffers.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <functional>
#include <map>
#include <vector>

namespace D3DModelBuffers
{
namespace
{
struct OpaqueBatchBuildKey
{
    int bufferGroupIndex;
    int cellX;
    int cellY;
    int cellZ;
    noesisMaterial_t *pMaterial;
    bool animatedWater;

    bool operator<(const OpaqueBatchBuildKey &other) const
    {
        if (bufferGroupIndex != other.bufferGroupIndex) return bufferGroupIndex < other.bufferGroupIndex;
        if (cellX != other.cellX) return cellX < other.cellX;
        if (cellY != other.cellY) return cellY < other.cellY;
        if (cellZ != other.cellZ) return cellZ < other.cellZ;
        if (pMaterial != other.pMaterial)
            return std::less<noesisMaterial_t *>()(pMaterial, other.pMaterial);
        return animatedWater < other.animatedWater;
    }
};
}

void UploadVertices(noesisModel_t::Submesh& submesh)
{
    if (!submesh.pVB || submesh.cpuVerts.empty())
        return;

    void* bufferData = nullptr;
    const UINT bufferSize = (UINT)(submesh.cpuVerts.size() * sizeof(FFXIVertex));
    if (SUCCEEDED(submesh.pVB->Lock(0, 0, &bufferData, 0)))
    {
        std::memcpy(bufferData, submesh.cpuVerts.data(), bufferSize);
        submesh.pVB->Unlock();
    }
}

void UploadIndices(noesisModel_t::Submesh& submesh)
{
    if (!submesh.pIB || submesh.cpuIndices.empty())
        return;

    void* bufferData = nullptr;
    const UINT bufferSize = (UINT)(submesh.cpuIndices.size() * sizeof(DWORD));
    if (SUCCEEDED(submesh.pIB->Lock(0, 0, &bufferData, 0)))
    {
        std::memcpy(bufferData, submesh.cpuIndices.data(), bufferSize);
        submesh.pIB->Unlock();
    }
}

bool HasDrawBuffers(const noesisModel_t *model, const noesisModel_t::Submesh& submesh)
{
    if (!model || submesh.triCount <= 0)
        return false;
    if (submesh.pVB && submesh.pIB)
        return true;
    return submesh.staticBufferGroupIndex >= 0 &&
        submesh.staticBufferGroupIndex < (int)model->staticBufferGroups.size() &&
        model->staticBufferGroups[(size_t)submesh.staticBufferGroupIndex].pVB &&
        model->staticBufferGroups[(size_t)submesh.staticBufferGroupIndex].pIB;
}

void DrawSubmesh(IDirect3DDevice9 *device, const noesisModel_t *model,
                 const noesisModel_t::Submesh& submesh)
{
    if (!device || !HasDrawBuffers(model, submesh))
        return;

    IDirect3DVertexBuffer9 *vertexBuffer = submesh.pVB;
    IDirect3DIndexBuffer9 *indexBuffer = submesh.pIB;
    int baseVertex = 0;
    int startIndex = 0;
    if (submesh.staticBufferGroupIndex >= 0)
    {
        const noesisModel_t::StaticBufferGroup &group =
            model->staticBufferGroups[(size_t)submesh.staticBufferGroupIndex];
        vertexBuffer = group.pVB;
        indexBuffer = group.pIB;
        baseVertex = submesh.staticVertexOffset;
        startIndex = submesh.staticStartIndex;
    }
    device->SetStreamSource(0, vertexBuffer, 0, (UINT)sizeof(FFXIVertex));
    device->SetIndices(indexBuffer);
    device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, baseVertex, 0,
        (UINT)submesh.vertCount, (UINT)startIndex, (UINT)submesh.triCount);
}

void DrawOpaqueBatch(IDirect3DDevice9 *device, const noesisModel_t *model,
                     const noesisModel_t::OpaqueBatch& batch)
{
    if (!device || !model || batch.bufferGroupIndex < 0 ||
        batch.bufferGroupIndex >= (int)model->staticBufferGroups.size() || batch.triCount <= 0)
        return;
    const noesisModel_t::StaticBufferGroup &group =
        model->staticBufferGroups[(size_t)batch.bufferGroupIndex];
    if (!group.pVB || !group.pOpaqueBatchIB)
        return;
    device->SetStreamSource(0, group.pVB, 0, (UINT)sizeof(FFXIVertex));
    device->SetIndices(group.pOpaqueBatchIB);
    device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0,
        (UINT)batch.minVertexIndex, (UINT)batch.vertexCount,
        (UINT)batch.startIndex, (UINT)batch.triCount);
}

void BuildStaticOpaqueBatches(noesisModel_t *model, IDirect3DDevice9 *device)
{
    if (!model || !device || model->staticBufferGroups.empty())
        return;

    for (noesisModel_t::StaticBufferGroup &group : model->staticBufferGroups)
    {
        if (group.pOpaqueBatchIB)
        {
            group.pOpaqueBatchIB->Release();
            group.pOpaqueBatchIB = nullptr;
        }
    }
    model->opaqueBatches.clear();

    static const float kBatchCellSize = 192.0f;
    std::map<OpaqueBatchBuildKey, std::vector<size_t> > batchMembers;
    for (size_t submeshIndex : model->opaqueSubmeshOrder)
    {
        const noesisModel_t::Submesh &submesh = model->submeshes[submeshIndex];
        if (submesh.environmentObject || submesh.staticBufferGroupIndex < 0)
            continue;
        OpaqueBatchBuildKey key = {};
        key.bufferGroupIndex = submesh.staticBufferGroupIndex;
        key.cellX = (int)floorf(submesh.boundsCenter[0] / kBatchCellSize);
        key.cellY = (int)floorf(submesh.boundsCenter[1] / kBatchCellSize);
        key.cellZ = (int)floorf(submesh.boundsCenter[2] / kBatchCellSize);
        key.pMaterial = submesh.pResolvedMaterial;
        key.animatedWater = submesh.animatedWater;
        batchMembers[key].push_back(submeshIndex);
    }

    std::vector<std::vector<DWORD> > groupIndices(model->staticBufferGroups.size());
    for (const auto &entry : batchMembers)
    {
        const OpaqueBatchBuildKey &key = entry.first;
        if (key.bufferGroupIndex < 0 ||
            key.bufferGroupIndex >= (int)model->staticBufferGroups.size())
        {
            continue;
        }

        std::vector<DWORD> &indices = groupIndices[(size_t)key.bufferGroupIndex];
        noesisModel_t::OpaqueBatch batch;
        batch.pMaterial = key.pMaterial;
        batch.bufferGroupIndex = key.bufferGroupIndex;
        batch.startIndex = (int)indices.size();
        batch.minVertexIndex = INT_MAX;
        batch.animatedWater = key.animatedWater;
        if (batch.pMaterial && model->pMatData && batch.pMaterial->texIdx >= 0 &&
            batch.pMaterial->texIdx < model->pMatData->texCount)
        {
            batch.pTexture = model->pMatData->textures[batch.pMaterial->texIdx];
        }

        int maxVertexIndex = 0;
        for (size_t submeshIndex : entry.second)
        {
            const noesisModel_t::Submesh &submesh = model->submeshes[submeshIndex];
            for (DWORD localIndex : submesh.cpuIndices)
                indices.push_back((DWORD)submesh.staticVertexOffset + localIndex);
            batch.triCount += submesh.triCount;
            if (submesh.staticVertexOffset < batch.minVertexIndex)
                batch.minVertexIndex = submesh.staticVertexOffset;
            const int submeshMaxVertex = submesh.staticVertexOffset + submesh.vertCount;
            if (submeshMaxVertex > maxVertexIndex)
                maxVertexIndex = submeshMaxVertex;

            if (submesh.hasBounds && !batch.hasBounds)
            {
                std::memcpy(batch.boundsMin, submesh.boundsMin, sizeof(batch.boundsMin));
                std::memcpy(batch.boundsMax, submesh.boundsMax, sizeof(batch.boundsMax));
                batch.hasBounds = true;
            }
            else if (submesh.hasBounds)
            {
                for (int axis = 0; axis < 3; ++axis)
                {
                    if (submesh.boundsMin[axis] < batch.boundsMin[axis])
                        batch.boundsMin[axis] = submesh.boundsMin[axis];
                    if (submesh.boundsMax[axis] > batch.boundsMax[axis])
                        batch.boundsMax[axis] = submesh.boundsMax[axis];
                }
            }
        }
        if (batch.minVertexIndex == INT_MAX)
            batch.minVertexIndex = 0;
        batch.vertexCount = std::max(0, maxVertexIndex - batch.minVertexIndex);
        model->opaqueBatches.push_back(batch);
    }

    bool buildOk = true;
    for (size_t groupIndex = 0; groupIndex < groupIndices.size(); ++groupIndex)
    {
        std::vector<DWORD> &indices = groupIndices[groupIndex];
        if (indices.empty())
            continue;
        noesisModel_t::StaticBufferGroup &group = model->staticBufferGroups[groupIndex];
        const UINT byteCount = (UINT)(indices.size() * sizeof(DWORD));
        if (FAILED(device->CreateIndexBuffer(byteCount, D3DUSAGE_WRITEONLY,
            D3DFMT_INDEX32, D3DPOOL_MANAGED, &group.pOpaqueBatchIB, nullptr)))
        {
            buildOk = false;
            break;
        }
        void *data = nullptr;
        if (FAILED(group.pOpaqueBatchIB->Lock(0, 0, &data, 0)))
        {
            buildOk = false;
            break;
        }
        std::memcpy(data, indices.data(), byteCount);
        group.pOpaqueBatchIB->Unlock();
    }

    if (!buildOk)
    {
        for (noesisModel_t::StaticBufferGroup &group : model->staticBufferGroups)
        {
            if (group.pOpaqueBatchIB)
            {
                group.pOpaqueBatchIB->Release();
                group.pOpaqueBatchIB = nullptr;
            }
        }
        model->opaqueBatches.clear();
    }
}
}
