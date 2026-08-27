#include "stdafx.h"
#include "zone_object_highlight_renderer.h"

#include "d3d_model_buffers.h"
#include "noesis_rapi.h"

namespace ZoneObjectHighlightRenderer
{
void Draw(IDirect3DDevice9 *device, noesisModel_t *model, const std::string& highlightedObject,
          const std::map<std::string, ZoneObjectTransform::DebugTransform>& overrides)
{
    if (!device || !model || highlightedObject.empty())
        return;

    D3DMATRIX baseWorld = {};
    device->GetTransform(D3DTS_WORLD, &baseWorld);
    device->SetFVF(FFXI_VERTEX_FVF);
    device->SetTexture(0, nullptr);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_ZENABLE, TRUE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
    device->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_XRGB(255, 128, 0));
    device->SetRenderState(D3DRS_DEPTHBIAS, 0xBA83126F);

    for (const noesisModel_t::Submesh& submesh : model->submeshes)
    {
        if (submesh.objectName != highlightedObject ||
            !D3DModelBuffers::HasDrawBuffers(model, submesh))
        {
            continue;
        }

        ZoneObjectTransform::ApplyWorldTransform(
            device, submesh.objectName, true, overrides, baseWorld);
        D3DModelBuffers::DrawSubmesh(device, model, submesh);
    }

    device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    device->SetRenderState(D3DRS_DEPTHBIAS, 0);
    device->SetTransform(D3DTS_WORLD, &baseWorld);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    device->SetStreamSource(0, nullptr, 0, 0);
    device->SetIndices(nullptr);
}
}
