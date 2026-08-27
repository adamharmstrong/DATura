#include "stdafx.h"
#include "zone_object_transform.h"

#include "d3d_math.h"

namespace ZoneObjectTransform
{
D3DMATRIX BuildWorldMatrix(const DebugTransform& transform)
{
    return D3DMath::BuildScaleRotateTranslate(transform.scale, transform.rot, transform.trans);
}

void ApplyWorldTransform(IDirect3DDevice9 *device, const std::string& objectName,
                         const bool allowOverrides,
                         const std::map<std::string, DebugTransform>& overrides,
                         const D3DMATRIX& baseWorld)
{
    if (!device)
        return;
    if (allowOverrides && !objectName.empty())
    {
        const std::map<std::string, DebugTransform>::const_iterator overrideIt =
            overrides.find(objectName);
        if (overrideIt != overrides.end())
        {
            const D3DMATRIX world = BuildWorldMatrix(overrideIt->second);
            device->SetTransform(D3DTS_WORLD, &world);
            return;
        }
    }
    device->SetTransform(D3DTS_WORLD, &baseWorld);
}
}
