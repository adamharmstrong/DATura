#pragma once

#include <d3d9.h>

#include <map>
#include <string>

namespace ZoneObjectTransform
{
struct DebugTransform
{
    float trans[3];
    float rot[3];
    float scale[3];
};

D3DMATRIX BuildWorldMatrix(const DebugTransform& transform);
void ApplyWorldTransform(IDirect3DDevice9 *device, const std::string& objectName,
                         bool allowOverrides,
                         const std::map<std::string, DebugTransform>& overrides,
                         const D3DMATRIX& baseWorld);
}
