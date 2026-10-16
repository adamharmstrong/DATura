#pragma once

#include "renderer_resources.h"
#include "renderer_interface.h"
#include <d3d9.h>
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")
#include <cstring>
#include <limits>
#include <new>

// Inline adapter also serves parser/test targets which do not link the app runtime.
namespace D3D9Graphics
{
class OverlayContext final : public RendererBackend::OverlayContext
{
public:
    OverlayContext(IDirect3DSurface9* surface, HDC dc, unsigned width, unsigned height)
        : surface_(surface), dc_(dc), width_(width), height_(height) {}
    ~OverlayContext() override { surface_->ReleaseDC(dc_); surface_->Release(); }
    OverlayContext(const OverlayContext&) = delete;
    OverlayContext& operator=(const OverlayContext&) = delete;
    HDC DeviceContext() const override { return dc_; }
    unsigned Width() const override { return width_; }
    unsigned Height() const override { return height_; }
private:
    IDirect3DSurface9* surface_;
    HDC dc_;
    unsigned width_, height_;
};
inline RendererBackend::OverlayContext* AcquireOverlay(IDirect3DDevice9* device)
{
    if (!device) return nullptr;
    IDirect3DSurface9* surface = nullptr;
    if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &surface))) return nullptr;
    D3DSURFACE_DESC description{};
    HDC dc = nullptr;
    if (FAILED(surface->GetDesc(&description)) || FAILED(surface->GetDC(&dc)))
    { surface->Release(); return nullptr; }
    auto* result = new (std::nothrow) OverlayContext(surface, dc, description.Width, description.Height);
    if (!result) { surface->ReleaseDC(dc); surface->Release(); }
    return result;
}

class VertexLayout final : public RendererBackend::VertexLayout
{
public:
    explicit VertexLayout(IDirect3DVertexDeclaration9* value) : value(value) {}
    ~VertexLayout() override { value->Release(); }
    VertexLayout(const VertexLayout&) = delete;
    VertexLayout& operator=(const VertexLayout&) = delete;
    IDirect3DVertexDeclaration9* value;
};
class Shader final : public RendererBackend::Shader
{
public:
    explicit Shader(IDirect3DVertexShader9* value) : vertex(value) {}
    explicit Shader(IDirect3DPixelShader9* value) : pixel(value) {}
    ~Shader() override { if (vertex) vertex->Release(); if (pixel) pixel->Release(); }
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    RendererBackend::ShaderStage Stage() const override
    { return vertex ? RendererBackend::ShaderStage::Vertex : RendererBackend::ShaderStage::Pixel; }
    IDirect3DVertexShader9* vertex = nullptr;
    IDirect3DPixelShader9* pixel = nullptr;
};

class Texture final : public RendererBackend::Texture
{
public:
    // Takes ownership of one COM reference.
    explicit Texture(IDirect3DTexture9* value) : value_(value) {}
    ~Texture() override { if (value_) value_->Release(); }
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    IDirect3DTexture9* Native() const { return value_; }
private:
    IDirect3DTexture9* value_;
};

// Explicit bridge for the remaining fixed-function material and UI code.
class RenderTarget final : public RendererBackend::RenderTarget
{
public:
    RenderTarget(IDirect3DSurface9* surface, IDirect3DTexture9* texture = nullptr)
        : surface(surface), texture(texture) {}
    ~RenderTarget() override { if (mapped_) UnmapRead(); if (surface) surface->Release(); if (texture) texture->Release(); }
    bool MapRead(RendererBackend::TargetReadMapping& out) override
    {
        if (mapped_) return false;
        D3DLOCKED_RECT lock;
        if (FAILED(surface->LockRect(&lock,nullptr,D3DLOCK_READONLY))) return false;
        mapped_=true; out={lock.pBits,size_t(lock.Pitch)}; return true;
    }
    bool UnmapRead() override
    { if (!mapped_) return false; mapped_=false; return SUCCEEDED(surface->UnlockRect()); }
    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;
    RendererBackend::RenderTargetDescription Description() const override
    {
        D3DSURFACE_DESC d = {}; surface->GetDesc(&d);
        return {d.Width, d.Height, d.Format == D3DFMT_D24S8 ? RendererBackend::TargetFormat::Depth24Stencil8 :
            d.Format == D3DFMT_R32F ? RendererBackend::TargetFormat::R32Float :
            d.Format == D3DFMT_X8R8G8B8 ? RendererBackend::TargetFormat::Bgrx8 : RendererBackend::TargetFormat::Bgra8,
            uint32_t(d.MultiSampleType), d.MultiSampleQuality, texture != nullptr};
    }
    IDirect3DSurface9* surface;
    IDirect3DTexture9* texture;
private:
    bool mapped_=false;
};
inline IDirect3DSurface9* NativeSurface(RendererBackend::RenderTarget* target)
{
    auto* native = dynamic_cast<RenderTarget*>(target);
    return native ? native->surface : nullptr;
}
class PipelineSnapshot final : public RendererBackend::PipelineSnapshot
{
public:
    explicit PipelineSnapshot(IDirect3DStateBlock9* state) : state_(state) {}
    ~PipelineSnapshot() override { state_->Release(); }
    bool Restore() override { return SUCCEEDED(state_->Apply()); }
    PipelineSnapshot(const PipelineSnapshot&) = delete;
    PipelineSnapshot& operator=(const PipelineSnapshot&) = delete;
private:
    IDirect3DStateBlock9* state_;
};
inline IDirect3DTexture9* NativeTexture(RendererBackend::Texture* texture)
{
    if (auto* target = dynamic_cast<RenderTarget*>(texture)) return target->texture;
    auto* native = dynamic_cast<Texture*>(texture);
    return native ? native->Native() : nullptr;
}

class MeshBuffer final : public RendererBackend::MeshBuffer
{
public:
    MeshBuffer(IDirect3DVertexBuffer9* value, size_t bytes) : vertex_(value), bytes_(bytes) {}
    MeshBuffer(IDirect3DIndexBuffer9* value, size_t bytes) : index_(value), bytes_(bytes) {}
    ~MeshBuffer() override
    {
        if (mapped_) Unmap();
        if (vertex_) vertex_->Release();
        if (index_) index_->Release();
    }
    MeshBuffer(const MeshBuffer&) = delete;
    MeshBuffer& operator=(const MeshBuffer&) = delete;
    bool Map(size_t offset, size_t size, void** data,
             RendererBackend::MapAccess access = RendererBackend::MapAccess::Write) override
    {
        if (!data) return false;
        *data = nullptr;
        if (mapped_ || offset >= bytes_ || size > bytes_ - offset) return false;
        const DWORD flags = access == RendererBackend::MapAccess::Read ? D3DLOCK_READONLY : 0;
        const HRESULT result = vertex_ ? vertex_->Lock((UINT)offset, (UINT)size, data, flags) :
            index_ ? index_->Lock((UINT)offset, (UINT)size, data, flags) : E_FAIL;
        mapped_ = SUCCEEDED(result);
        return mapped_;
    }
    bool Unmap() override
    {
        if (!mapped_) return false;
        mapped_ = false;
        return SUCCEEDED(vertex_ ? vertex_->Unlock() : index_->Unlock());
    }
    IDirect3DVertexBuffer9* Vertex() const { return vertex_; }
    IDirect3DIndexBuffer9* Index() const { return index_; }
    size_t Size() const { return bytes_; }
private:
    IDirect3DVertexBuffer9* vertex_ = nullptr;
    IDirect3DIndexBuffer9* index_ = nullptr;
    size_t bytes_ = 0;
    bool mapped_ = false;
};

class Context final : public RendererBackend::Graphics
{
public:
    // Borrows the device for the duration of a call, never retains a COM reference.
    explicit Context(IDirect3DDevice9* device = nullptr) : device_(device) {}
    void SetDevice(IDirect3DDevice9* device) { device_ = device; }
    bool BeginScene() override { return device_ && SUCCEEDED(device_->BeginScene()); }
    bool EndScene() override { return device_ && SUCCEEDED(device_->EndScene()); }
    bool Clear(const RendererBackend::ClearState& c) override
    {
        const DWORD flags=(c.color?D3DCLEAR_TARGET:0)|(c.depth?D3DCLEAR_ZBUFFER:0)|(c.stencil?D3DCLEAR_STENCIL:0);
        return device_ && flags && SUCCEEDED(device_->Clear(0,nullptr,flags,c.colorValue,c.depthValue,c.stencilValue));
    }
    RendererBackend::RenderTarget* CaptureBackBuffer() override
    {
        if (!device_) return nullptr;
        IDirect3DSurface9* surface=nullptr;
        if (FAILED(device_->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&surface))) return nullptr;
        auto* result=new(std::nothrow) RenderTarget(surface);
        if (!result) surface->Release();
        return result;
    }
    bool SetTransform(RendererBackend::Transform kind, const float* values) override
    {
        if (!device_ || !values || size_t(kind)>2) return false;
        const D3DTRANSFORMSTATETYPE types[]={D3DTS_WORLD,D3DTS_VIEW,D3DTS_PROJECTION};
        D3DMATRIX matrix; std::memcpy(&matrix,values,sizeof(matrix));
        return SUCCEEDED(device_->SetTransform(types[size_t(kind)],&matrix));
    }
    bool QueryTransform(RendererBackend::Transform kind, float* values) override
    {
        if (!device_ || !values || size_t(kind)>2) return false;
        const D3DTRANSFORMSTATETYPE types[]={D3DTS_WORLD,D3DTS_VIEW,D3DTS_PROJECTION};
        D3DMATRIX matrix;
        if (FAILED(device_->GetTransform(types[size_t(kind)],&matrix))) return false;
        std::memcpy(values,&matrix,sizeof(matrix)); return true;
    }
    bool QueryTextureTransform(uint32_t stage, float* values) override
    {
        if (!device_ || !values || stage>=8) return false;
        D3DMATRIX matrix;
        if (FAILED(device_->GetTransform(D3DTRANSFORMSTATETYPE(D3DTS_TEXTURE0+stage),&matrix))) return false;
        std::memcpy(values,&matrix,sizeof(matrix)); return true;
    }
    bool SetViewport(const RendererBackend::Viewport& v) override
    {
        if (!device_) return false;
        const D3DVIEWPORT9 native={v.x,v.y,v.width,v.height,v.minDepth,v.maxDepth};
        return SUCCEEDED(device_->SetViewport(&native));
    }
    bool QueryViewport(RendererBackend::Viewport& out) override
    {
        if (!device_) return false;
        D3DVIEWPORT9 v;
        if (FAILED(device_->GetViewport(&v))) return false;
        out={v.X,v.Y,v.Width,v.Height,v.MinZ,v.MaxZ}; return true;
    }
    bool QueryFogState(RendererBackend::FogState& out) override
    {
        if (!device_) return false;
        DWORD enabled,color,start,end;
        if (FAILED(device_->GetRenderState(D3DRS_FOGENABLE,&enabled)) ||
            FAILED(device_->GetRenderState(D3DRS_FOGCOLOR,&color)) ||
            FAILED(device_->GetRenderState(D3DRS_FOGSTART,&start)) ||
            FAILED(device_->GetRenderState(D3DRS_FOGEND,&end))) return false;
        RendererBackend::FogState value; value.enabled=enabled!=FALSE; value.color=color;
        std::memcpy(&value.start,&start,4); std::memcpy(&value.end,&end,4);
        out=value; return true;
    }
    bool QueryStencilState(RendererBackend::StencilState& out) override
    {
        if (!device_) return false;
        static const DWORD comparisons[]={D3DCMP_NEVER,D3DCMP_LESS,D3DCMP_EQUAL,D3DCMP_LESSEQUAL,D3DCMP_GREATER,D3DCMP_NOTEQUAL,D3DCMP_GREATEREQUAL,D3DCMP_ALWAYS};
        static const DWORD operations[]={D3DSTENCILOP_KEEP,D3DSTENCILOP_ZERO,D3DSTENCILOP_REPLACE,D3DSTENCILOP_INCRSAT,D3DSTENCILOP_DECRSAT,D3DSTENCILOP_INVERT,D3DSTENCILOP_INCR,D3DSTENCILOP_DECR};
        RendererBackend::StencilState value;
        DWORD native=0;
        if (FAILED(device_->GetRenderState(D3DRS_STENCILENABLE,&native))) return false;
        value.enabled=native!=FALSE;
        if (FAILED(device_->GetRenderState(D3DRS_TWOSIDEDSTENCILMODE,&native))) return false;
        value.twoSided=native!=FALSE;
        if (FAILED(device_->GetRenderState(D3DRS_STENCILREF,&native))) return false;
        value.reference=native;
        if (FAILED(device_->GetRenderState(D3DRS_STENCILMASK,&native))) return false;
        value.readMask=native;
        if (FAILED(device_->GetRenderState(D3DRS_STENCILWRITEMASK,&native))) return false;
        value.writeMask=native;
        if (FAILED(device_->GetRenderState(D3DRS_STENCILFUNC,&native))) return false;
        { size_t i=0; for (; i<sizeof(comparisons)/sizeof(comparisons[0]) && comparisons[i]!=native; ++i) {}
          if (i==sizeof(comparisons)/sizeof(comparisons[0])) return false;
          value.compare=RendererBackend::Compare(i); }
        if (FAILED(device_->GetRenderState(D3DRS_CCW_STENCILFUNC,&native))) return false;
        { size_t i=0; for (; i<sizeof(comparisons)/sizeof(comparisons[0]) && comparisons[i]!=native; ++i) {}
          if (i==sizeof(comparisons)/sizeof(comparisons[0])) return false;
          value.backCompare=RendererBackend::Compare(i); }
        if (FAILED(device_->GetRenderState(D3DRS_STENCILFAIL,&native))) return false;
        { size_t i=0; for (; i<sizeof(operations)/sizeof(operations[0]) && operations[i]!=native; ++i) {}
          if (i==sizeof(operations)/sizeof(operations[0])) return false;
          value.fail=RendererBackend::StencilOperation(i); }
        if (FAILED(device_->GetRenderState(D3DRS_STENCILZFAIL,&native))) return false;
        { size_t i=0; for (; i<sizeof(operations)/sizeof(operations[0]) && operations[i]!=native; ++i) {}
          if (i==sizeof(operations)/sizeof(operations[0])) return false;
          value.depthFail=RendererBackend::StencilOperation(i); }
        if (FAILED(device_->GetRenderState(D3DRS_STENCILPASS,&native))) return false;
        { size_t i=0; for (; i<sizeof(operations)/sizeof(operations[0]) && operations[i]!=native; ++i) {}
          if (i==sizeof(operations)/sizeof(operations[0])) return false;
          value.pass=RendererBackend::StencilOperation(i); }
        if (FAILED(device_->GetRenderState(D3DRS_CCW_STENCILFAIL,&native))) return false;
        { size_t i=0; for (; i<sizeof(operations)/sizeof(operations[0]) && operations[i]!=native; ++i) {}
          if (i==sizeof(operations)/sizeof(operations[0])) return false;
          value.backFail=RendererBackend::StencilOperation(i); }
        if (FAILED(device_->GetRenderState(D3DRS_CCW_STENCILZFAIL,&native))) return false;
        { size_t i=0; for (; i<sizeof(operations)/sizeof(operations[0]) && operations[i]!=native; ++i) {}
          if (i==sizeof(operations)/sizeof(operations[0])) return false;
          value.backDepthFail=RendererBackend::StencilOperation(i); }
        if (FAILED(device_->GetRenderState(D3DRS_CCW_STENCILPASS,&native))) return false;
        { size_t i=0; for (; i<sizeof(operations)/sizeof(operations[0]) && operations[i]!=native; ++i) {}
          if (i==sizeof(operations)/sizeof(operations[0])) return false;
          value.backPass=RendererBackend::StencilOperation(i); }
        out=value; return true;
    }
    bool QueryTextureStageState(uint32_t stage, RendererBackend::TextureStageState& out) override
    {
        if (!device_ || stage>=8) return false;
        static const DWORD operations[]={D3DTOP_DISABLE,D3DTOP_SELECTARG1,D3DTOP_SELECTARG2,D3DTOP_MODULATE,D3DTOP_MODULATE2X,D3DTOP_MODULATE4X,D3DTOP_ADD};
        static const DWORD arguments[]={D3DTA_DIFFUSE,D3DTA_CURRENT,D3DTA_TEXTURE,D3DTA_TFACTOR,D3DTA_SPECULAR,D3DTA_TEMP,D3DTA_CONSTANT};
        static const DWORD transforms[]={D3DTTFF_DISABLE,D3DTTFF_COUNT1,D3DTTFF_COUNT2,D3DTTFF_COUNT3,D3DTTFF_COUNT4};
        RendererBackend::TextureStageState value;
        DWORD native=0;
        if (FAILED(device_->GetTextureStageState(stage,D3DTSS_COLOROP,&native))) return false;
        { size_t i=0; for (; i<sizeof(operations)/sizeof(operations[0]) && operations[i]!=native; ++i) {}
          if (i==sizeof(operations)/sizeof(operations[0])) return false;
          value.colorOperation=RendererBackend::TextureOperation(i); }
        if (FAILED(device_->GetTextureStageState(stage,D3DTSS_ALPHAOP,&native))) return false;
        { size_t i=0; for (; i<sizeof(operations)/sizeof(operations[0]) && operations[i]!=native; ++i) {}
          if (i==sizeof(operations)/sizeof(operations[0])) return false;
          value.alphaOperation=RendererBackend::TextureOperation(i); }
        if (FAILED(device_->GetTextureStageState(stage,D3DTSS_COLORARG1,&native))) return false;
        { size_t i=0; for (; i<sizeof(arguments)/sizeof(arguments[0]) && arguments[i]!=native; ++i) {}
          if (i==sizeof(arguments)/sizeof(arguments[0])) return false;
          value.colorArg1=RendererBackend::TextureArgument(i); }
        if (FAILED(device_->GetTextureStageState(stage,D3DTSS_COLORARG2,&native))) return false;
        { size_t i=0; for (; i<sizeof(arguments)/sizeof(arguments[0]) && arguments[i]!=native; ++i) {}
          if (i==sizeof(arguments)/sizeof(arguments[0])) return false;
          value.colorArg2=RendererBackend::TextureArgument(i); }
        if (FAILED(device_->GetTextureStageState(stage,D3DTSS_ALPHAARG1,&native))) return false;
        { size_t i=0; for (; i<sizeof(arguments)/sizeof(arguments[0]) && arguments[i]!=native; ++i) {}
          if (i==sizeof(arguments)/sizeof(arguments[0])) return false;
          value.alphaArg1=RendererBackend::TextureArgument(i); }
        if (FAILED(device_->GetTextureStageState(stage,D3DTSS_ALPHAARG2,&native))) return false;
        { size_t i=0; for (; i<sizeof(arguments)/sizeof(arguments[0]) && arguments[i]!=native; ++i) {}
          if (i==sizeof(arguments)/sizeof(arguments[0])) return false;
          value.alphaArg2=RendererBackend::TextureArgument(i); }
        if (FAILED(device_->GetTextureStageState(stage,D3DTSS_RESULTARG,&native))) return false;
        { size_t i=0; for (; i<sizeof(arguments)/sizeof(arguments[0]) && arguments[i]!=native; ++i) {}
          if (i==sizeof(arguments)/sizeof(arguments[0])) return false;
          value.result=RendererBackend::TextureArgument(i); }
        if (FAILED(device_->GetTextureStageState(stage,D3DTSS_TEXCOORDINDEX,&native))) return false;
        value.coordinateIndex=native;
        if (FAILED(device_->GetTextureStageState(stage,D3DTSS_TEXTURETRANSFORMFLAGS,&native))) return false;
        { size_t i=0; for (; i<sizeof(transforms)/sizeof(transforms[0]) && transforms[i]!=native; ++i) {}
          if (i==sizeof(transforms)/sizeof(transforms[0])) return false;
          value.transform=RendererBackend::TextureTransform(i); }
        out=value; return true;
    }
    bool QueryLightingState(RendererBackend::LightingState& out) override
    {
        if (!device_) return false;
        static const DWORD sources[]={D3DMCS_MATERIAL,D3DMCS_COLOR1,D3DMCS_COLOR2};
        RendererBackend::LightingState value;
        DWORD native=0;
        if (FAILED(device_->GetRenderState(D3DRS_LIGHTING,&native))) return false;
        value.enabled=native!=FALSE;
        if (FAILED(device_->GetRenderState(D3DRS_COLORVERTEX,&native))) return false;
        value.vertexColor=native!=FALSE;
        if (FAILED(device_->GetRenderState(D3DRS_NORMALIZENORMALS,&native))) return false;
        value.normalizeNormals=native!=FALSE;
        if (FAILED(device_->GetRenderState(D3DRS_SPECULARENABLE,&native))) return false;
        value.specular=native!=FALSE;
        if (FAILED(device_->GetRenderState(D3DRS_LOCALVIEWER,&native))) return false;
        value.localViewer=native!=FALSE;
        if (FAILED(device_->GetRenderState(D3DRS_AMBIENT,&native))) return false;
        value.ambient=native;
        if (FAILED(device_->GetRenderState(D3DRS_DIFFUSEMATERIALSOURCE,&native))) return false;
        { size_t i=0; for (; i<sizeof(sources)/sizeof(sources[0]) && sources[i]!=native; ++i) {}
          if (i==sizeof(sources)/sizeof(sources[0])) return false;
          value.diffuseSource=RendererBackend::MaterialSource(i); }
        if (FAILED(device_->GetRenderState(D3DRS_AMBIENTMATERIALSOURCE,&native))) return false;
        { size_t i=0; for (; i<sizeof(sources)/sizeof(sources[0]) && sources[i]!=native; ++i) {}
          if (i==sizeof(sources)/sizeof(sources[0])) return false;
          value.ambientSource=RendererBackend::MaterialSource(i); }
        if (FAILED(device_->GetRenderState(D3DRS_SPECULARMATERIALSOURCE,&native))) return false;
        { size_t i=0; for (; i<sizeof(sources)/sizeof(sources[0]) && sources[i]!=native; ++i) {}
          if (i==sizeof(sources)/sizeof(sources[0])) return false;
          value.specularSource=RendererBackend::MaterialSource(i); }
        if (FAILED(device_->GetRenderState(D3DRS_EMISSIVEMATERIALSOURCE,&native))) return false;
        { size_t i=0; for (; i<sizeof(sources)/sizeof(sources[0]) && sources[i]!=native; ++i) {}
          if (i==sizeof(sources)/sizeof(sources[0])) return false;
          value.emissiveSource=RendererBackend::MaterialSource(i); }
        out=value; return true;
    }
    bool QueryPipelineState(RendererBackend::PipelineState& out) override
    {
        if (!device_) return false;
        static const DWORD comparisons[] = {D3DCMP_NEVER,D3DCMP_LESS,D3DCMP_EQUAL,D3DCMP_LESSEQUAL,D3DCMP_GREATER,D3DCMP_NOTEQUAL,D3DCMP_GREATEREQUAL,D3DCMP_ALWAYS};
        static const DWORD culls[] = {D3DCULL_NONE,D3DCULL_CW,D3DCULL_CCW};
        static const DWORD blends[] = {D3DBLEND_ZERO,D3DBLEND_ONE,D3DBLEND_SRCCOLOR,D3DBLEND_INVSRCCOLOR,D3DBLEND_SRCALPHA,D3DBLEND_INVSRCALPHA,D3DBLEND_DESTALPHA,D3DBLEND_INVDESTALPHA,D3DBLEND_DESTCOLOR,D3DBLEND_INVDESTCOLOR};
        RendererBackend::PipelineState value;
        DWORD native=0;
        if (FAILED(device_->GetRenderState(D3DRS_ZENABLE,&native))) return false;
        value.depthTest=native!=FALSE;
        if (FAILED(device_->GetRenderState(D3DRS_ZWRITEENABLE,&native))) return false;
        value.depthWrite=native!=FALSE;
        if (FAILED(device_->GetRenderState(D3DRS_ALPHABLENDENABLE,&native))) return false;
        value.blend=native!=FALSE;
        if (FAILED(device_->GetRenderState(D3DRS_ALPHATESTENABLE,&native))) return false;
        value.alphaTest=native!=FALSE;
        if (FAILED(device_->GetRenderState(D3DRS_STENCILENABLE,&native))) return false;
        value.stencil=native!=FALSE;
        if (FAILED(device_->GetRenderState(D3DRS_SCISSORTESTENABLE,&native))) return false;
        value.scissor=native!=FALSE;
        if (FAILED(device_->GetRenderState(D3DRS_FOGENABLE,&native))) return false;
        value.fog=native!=FALSE;
        if (FAILED(device_->GetRenderState(D3DRS_SRGBWRITEENABLE,&native))) return false;
        value.srgbWrite=native!=FALSE;
        if (FAILED(device_->GetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,&native))) return false;
        value.separateAlphaBlend=native!=FALSE;
        if (FAILED(device_->GetRenderState(D3DRS_MULTISAMPLEANTIALIAS,&native))) return false;
        value.multisample=native!=FALSE;
        if (FAILED(device_->GetRenderState(D3DRS_ZFUNC,&native))) return false;
        { size_t i=0; for (; i<sizeof(comparisons)/sizeof(comparisons[0]) && comparisons[i]!=native; ++i) {}
          if (i==sizeof(comparisons)/sizeof(comparisons[0])) return false;
          value.depthCompare=RendererBackend::Compare(i); }
        if (FAILED(device_->GetRenderState(D3DRS_CULLMODE,&native))) return false;
        { size_t i=0; for (; i<sizeof(culls)/sizeof(culls[0]) && culls[i]!=native; ++i) {}
          if (i==sizeof(culls)/sizeof(culls[0])) return false;
          value.cull=RendererBackend::Cull(i); }
        if (FAILED(device_->GetRenderState(D3DRS_SRCBLEND,&native))) return false;
        { size_t i=0; for (; i<sizeof(blends)/sizeof(blends[0]) && blends[i]!=native; ++i) {}
          if (i==sizeof(blends)/sizeof(blends[0])) return false;
          value.sourceBlend=RendererBackend::BlendFactor(i); }
        if (FAILED(device_->GetRenderState(D3DRS_DESTBLEND,&native))) return false;
        { size_t i=0; for (; i<sizeof(blends)/sizeof(blends[0]) && blends[i]!=native; ++i) {}
          if (i==sizeof(blends)/sizeof(blends[0])) return false;
          value.destinationBlend=RendererBackend::BlendFactor(i); }
        if (FAILED(device_->GetRenderState(D3DRS_DEPTHBIAS,&native))) return false;
        { float decoded; std::memcpy(&decoded,&native,4); value.depthBias=decoded; }
        if (FAILED(device_->GetRenderState(D3DRS_SLOPESCALEDEPTHBIAS,&native))) return false;
        { float decoded; std::memcpy(&decoded,&native,4); value.slopeDepthBias=decoded; }
        if (FAILED(device_->GetRenderState(D3DRS_COLORWRITEENABLE,&native))) return false;
        value.colorWriteMask=native;
        static const DWORD fills[]={D3DFILL_POINT,D3DFILL_WIREFRAME,D3DFILL_SOLID};
        static const DWORD blendOps[]={D3DBLENDOP_ADD,D3DBLENDOP_SUBTRACT,D3DBLENDOP_REVSUBTRACT,D3DBLENDOP_MIN,D3DBLENDOP_MAX};
        static const DWORD fogModes[]={D3DFOG_NONE,D3DFOG_EXP,D3DFOG_EXP2,D3DFOG_LINEAR};
        if (FAILED(device_->GetRenderState(D3DRS_CLIPPING,&native))) return false;
        value.clipping=native!=FALSE;
        if (FAILED(device_->GetRenderState(D3DRS_DITHERENABLE,&native))) return false;
        value.dither=native!=FALSE;
        if (FAILED(device_->GetRenderState(D3DRS_RANGEFOGENABLE,&native))) return false;
        value.rangeFog=native!=FALSE;
        if (FAILED(device_->GetRenderState(D3DRS_ALPHAREF,&native))) return false;
        value.alphaReference=native;
        if (FAILED(device_->GetRenderState(D3DRS_CLIPPLANEENABLE,&native))) return false;
        value.clipPlaneMask=native;
        if (FAILED(device_->GetRenderState(D3DRS_MULTISAMPLEMASK,&native))) return false;
        value.multisampleMask=native;
        if (FAILED(device_->GetRenderState(D3DRS_FOGCOLOR,&native))) return false;
        value.fogColor=native;
        if (FAILED(device_->GetRenderState(D3DRS_FOGSTART,&native))) return false;
        value.fogStart=RendererBackend::FloatFromBits(native);
        if (FAILED(device_->GetRenderState(D3DRS_FOGEND,&native))) return false;
        value.fogEnd=RendererBackend::FloatFromBits(native);
        if (FAILED(device_->GetRenderState(D3DRS_ALPHAFUNC,&native))) return false;
        { size_t i=0; for(; i<sizeof(comparisons)/sizeof(comparisons[0]) && comparisons[i]!=native; ++i) {}
          if(i==sizeof(comparisons)/sizeof(comparisons[0])) return false;
          value.alphaCompare=RendererBackend::Compare(i); }
        if (FAILED(device_->GetRenderState(D3DRS_FILLMODE,&native))) return false;
        { size_t i=0; for(; i<sizeof(fills)/sizeof(fills[0]) && fills[i]!=native; ++i) {}
          if(i==sizeof(fills)/sizeof(fills[0])) return false;
          value.fill=RendererBackend::Fill(i); }
        if (FAILED(device_->GetRenderState(D3DRS_BLENDOP,&native))) return false;
        { size_t i=0; for(; i<sizeof(blendOps)/sizeof(blendOps[0]) && blendOps[i]!=native; ++i) {}
          if(i==sizeof(blendOps)/sizeof(blendOps[0])) return false;
          value.blendOperation=RendererBackend::BlendOperation(i); }
        if (FAILED(device_->GetRenderState(D3DRS_FOGTABLEMODE,&native))) return false;
        { size_t i=0; for(; i<sizeof(fogModes)/sizeof(fogModes[0]) && fogModes[i]!=native; ++i) {}
          if(i==sizeof(fogModes)/sizeof(fogModes[0])) return false;
          value.fogTableMode=RendererBackend::FogMode(i); }
        if (FAILED(device_->GetRenderState(D3DRS_FOGVERTEXMODE,&native))) return false;
        { size_t i=0; for(; i<sizeof(fogModes)/sizeof(fogModes[0]) && fogModes[i]!=native; ++i) {}
          if(i==sizeof(fogModes)/sizeof(fogModes[0])) return false;
          value.fogVertexMode=RendererBackend::FogMode(i); }
        out=value; return true;
    }
    bool QuerySamplerState(uint32_t stage, RendererBackend::SamplerState& out) override
    {
        if (!device_ || stage>=16) return false;
        static const DWORD filters[] = {D3DTEXF_NONE,D3DTEXF_POINT,D3DTEXF_LINEAR,D3DTEXF_ANISOTROPIC};
        static const DWORD addresses[] = {D3DTADDRESS_WRAP,D3DTADDRESS_MIRROR,D3DTADDRESS_CLAMP,D3DTADDRESS_BORDER,D3DTADDRESS_MIRRORONCE};
        RendererBackend::SamplerState value;
        DWORD native=0;
        if (FAILED(device_->GetSamplerState(stage,D3DSAMP_MINFILTER,&native))) return false;
        { size_t i=0; for (; i<sizeof(filters)/sizeof(filters[0]) && filters[i]!=native; ++i) {}
          if (i==sizeof(filters)/sizeof(filters[0])) return false;
          value.minFilter=RendererBackend::TextureFilter(i); }
        if (FAILED(device_->GetSamplerState(stage,D3DSAMP_MAGFILTER,&native))) return false;
        { size_t i=0; for (; i<sizeof(filters)/sizeof(filters[0]) && filters[i]!=native; ++i) {}
          if (i==sizeof(filters)/sizeof(filters[0])) return false;
          value.magFilter=RendererBackend::TextureFilter(i); }
        if (FAILED(device_->GetSamplerState(stage,D3DSAMP_MIPFILTER,&native))) return false;
        { size_t i=0; for (; i<sizeof(filters)/sizeof(filters[0]) && filters[i]!=native; ++i) {}
          if (i==sizeof(filters)/sizeof(filters[0])) return false;
          value.mipFilter=RendererBackend::TextureFilter(i); }
        if (FAILED(device_->GetSamplerState(stage,D3DSAMP_ADDRESSU,&native))) return false;
        { size_t i=0; for (; i<sizeof(addresses)/sizeof(addresses[0]) && addresses[i]!=native; ++i) {}
          if (i==sizeof(addresses)/sizeof(addresses[0])) return false;
          value.addressU=RendererBackend::TextureAddress(i); }
        if (FAILED(device_->GetSamplerState(stage,D3DSAMP_ADDRESSV,&native))) return false;
        { size_t i=0; for (; i<sizeof(addresses)/sizeof(addresses[0]) && addresses[i]!=native; ++i) {}
          if (i==sizeof(addresses)/sizeof(addresses[0])) return false;
          value.addressV=RendererBackend::TextureAddress(i); }
        if (FAILED(device_->GetSamplerState(stage,D3DSAMP_ADDRESSW,&native))) return false;
        { size_t i=0; for (; i<sizeof(addresses)/sizeof(addresses[0]) && addresses[i]!=native; ++i) {}
          if (i==sizeof(addresses)/sizeof(addresses[0])) return false;
          value.addressW=RendererBackend::TextureAddress(i); }
        if (FAILED(device_->GetSamplerState(stage,D3DSAMP_SRGBTEXTURE,&native))) return false;
        value.srgb=native!=FALSE;
        if (FAILED(device_->GetSamplerState(stage,D3DSAMP_MAXANISOTROPY,&native))) return false;
        value.maxAnisotropy=native;
        if (FAILED(device_->GetSamplerState(stage,D3DSAMP_MAXMIPLEVEL,&native))) return false;
        value.maxMipLevel=native;
        if (FAILED(device_->GetSamplerState(stage,D3DSAMP_BORDERCOLOR,&native))) return false;
        value.borderColor=native;
        if (FAILED(device_->GetSamplerState(stage,D3DSAMP_MIPMAPLODBIAS,&native))) return false;
        { float decoded; std::memcpy(&decoded,&native,4); value.mipLodBias=decoded; }
        out=value; return true;
    }
    // Compatibility for the existing device-taking material helper.
    bool SetLegacyTextureAlphaOperation(uint32_t stage, D3DTEXTUREOP native)
    {
        using RendererBackend::TextureOperation;
        TextureOperation op;
        switch (native) {
        case D3DTOP_DISABLE: op=TextureOperation::Disable; break;
        case D3DTOP_SELECTARG1: op=TextureOperation::SelectArg1; break;
        case D3DTOP_SELECTARG2: op=TextureOperation::SelectArg2; break;
        case D3DTOP_MODULATE: op=TextureOperation::Modulate; break;
        case D3DTOP_MODULATE2X: op=TextureOperation::Modulate2X; break;
        case D3DTOP_MODULATE4X: op=TextureOperation::Modulate4X; break;
        case D3DTOP_ADD: op=TextureOperation::Add; break;
        default: return false;
        }
        return SetTextureStageField(stage,&RendererBackend::TextureStageState::alphaOperation,op);
    }
    bool SetTextureTransform(uint32_t stage, const float* values) override
    {
        if (!device_ || stage >= 8 || !values) return false;
        D3DMATRIX matrix; std::memcpy(&matrix,values,sizeof(matrix));
        return SUCCEEDED(device_->SetTransform(D3DTRANSFORMSTATETYPE(D3DTS_TEXTURE0+stage),&matrix));
    }
    bool SetTextureFactor(uint32_t value) override
    { return device_ && SUCCEEDED(device_->SetRenderState(D3DRS_TEXTUREFACTOR,value)); }
    bool SetLight(uint32_t index, const RendererBackend::Light& light) override
    {
        if (!device_) return false;
        D3DLIGHT9 native = {};
        switch(light.type) {
        case RendererBackend::LightType::Point: native.Type=D3DLIGHT_POINT; break;
        case RendererBackend::LightType::Spot: native.Type=D3DLIGHT_SPOT; break;
        case RendererBackend::LightType::Directional: native.Type=D3DLIGHT_DIRECTIONAL; break;
        default: return false;
        }
        native.Diffuse={light.diffuse.r,light.diffuse.g,light.diffuse.b,light.diffuse.a};
        native.Specular={light.specular.r,light.specular.g,light.specular.b,light.specular.a};
        native.Ambient={light.ambient.r,light.ambient.g,light.ambient.b,light.ambient.a};
        native.Position={light.position.x,light.position.y,light.position.z};
        native.Direction={light.direction.x,light.direction.y,light.direction.z};
        native.Range=light.range; native.Falloff=light.falloff;
        native.Attenuation0=light.attenuation0; native.Attenuation1=light.attenuation1; native.Attenuation2=light.attenuation2;
        native.Theta=light.theta; native.Phi=light.phi;
        return SUCCEEDED(device_->SetLight(index,&native));
    }
    bool EnableLight(uint32_t index, bool enabled) override
    { return device_ && SUCCEEDED(device_->LightEnable(index,enabled ? TRUE : FALSE)); }
    bool ApplyStencilState(const RendererBackend::StencilState& s) override
    {
        if (!device_) return false;
        static const DWORD comparisons[]={D3DCMP_NEVER,D3DCMP_LESS,D3DCMP_EQUAL,D3DCMP_LESSEQUAL,D3DCMP_GREATER,D3DCMP_NOTEQUAL,D3DCMP_GREATEREQUAL,D3DCMP_ALWAYS};
        static const DWORD operations[]={D3DSTENCILOP_KEEP,D3DSTENCILOP_ZERO,D3DSTENCILOP_REPLACE,D3DSTENCILOP_INCRSAT,D3DSTENCILOP_DECRSAT,D3DSTENCILOP_INVERT,D3DSTENCILOP_INCR,D3DSTENCILOP_DECR};
        if (s.compare && size_t(*s.compare) >= sizeof(comparisons)/sizeof(comparisons[0])) return false;
        if (s.backCompare && size_t(*s.backCompare) >= sizeof(comparisons)/sizeof(comparisons[0])) return false;
        if (s.fail && size_t(*s.fail) >= sizeof(operations)/sizeof(operations[0])) return false;
        if (s.depthFail && size_t(*s.depthFail) >= sizeof(operations)/sizeof(operations[0])) return false;
        if (s.pass && size_t(*s.pass) >= sizeof(operations)/sizeof(operations[0])) return false;
        if (s.backFail && size_t(*s.backFail) >= sizeof(operations)/sizeof(operations[0])) return false;
        if (s.backDepthFail && size_t(*s.backDepthFail) >= sizeof(operations)/sizeof(operations[0])) return false;
        if (s.backPass && size_t(*s.backPass) >= sizeof(operations)/sizeof(operations[0])) return false;
        bool ok=true;
        if (s.enabled && FAILED(device_->SetRenderState(D3DRS_STENCILENABLE,DWORD(*s.enabled)))) ok=false;
        if (s.twoSided && FAILED(device_->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE,DWORD(*s.twoSided)))) ok=false;
        if (s.reference && FAILED(device_->SetRenderState(D3DRS_STENCILREF,DWORD(*s.reference)))) ok=false;
        if (s.readMask && FAILED(device_->SetRenderState(D3DRS_STENCILMASK,DWORD(*s.readMask)))) ok=false;
        if (s.writeMask && FAILED(device_->SetRenderState(D3DRS_STENCILWRITEMASK,DWORD(*s.writeMask)))) ok=false;
        if (s.compare && FAILED(device_->SetRenderState(D3DRS_STENCILFUNC,comparisons[size_t(*s.compare)]))) ok=false;
        if (s.backCompare && FAILED(device_->SetRenderState(D3DRS_CCW_STENCILFUNC,comparisons[size_t(*s.backCompare)]))) ok=false;
        if (s.fail && FAILED(device_->SetRenderState(D3DRS_STENCILFAIL,operations[size_t(*s.fail)]))) ok=false;
        if (s.depthFail && FAILED(device_->SetRenderState(D3DRS_STENCILZFAIL,operations[size_t(*s.depthFail)]))) ok=false;
        if (s.pass && FAILED(device_->SetRenderState(D3DRS_STENCILPASS,operations[size_t(*s.pass)]))) ok=false;
        if (s.backFail && FAILED(device_->SetRenderState(D3DRS_CCW_STENCILFAIL,operations[size_t(*s.backFail)]))) ok=false;
        if (s.backDepthFail && FAILED(device_->SetRenderState(D3DRS_CCW_STENCILZFAIL,operations[size_t(*s.backDepthFail)]))) ok=false;
        if (s.backPass && FAILED(device_->SetRenderState(D3DRS_CCW_STENCILPASS,operations[size_t(*s.backPass)]))) ok=false;
        return ok;
    }
    bool ApplyTextureStageState(uint32_t stage, const RendererBackend::TextureStageState& s) override
    {
        if (!device_ || stage >= 8) return false;
        static const DWORD operations[]={D3DTOP_DISABLE,D3DTOP_SELECTARG1,D3DTOP_SELECTARG2,D3DTOP_MODULATE,D3DTOP_MODULATE2X,D3DTOP_MODULATE4X,D3DTOP_ADD};
        static const DWORD arguments[]={D3DTA_DIFFUSE,D3DTA_CURRENT,D3DTA_TEXTURE,D3DTA_TFACTOR,D3DTA_SPECULAR,D3DTA_TEMP,D3DTA_CONSTANT};
        static const DWORD transforms[]={D3DTTFF_DISABLE,D3DTTFF_COUNT1,D3DTTFF_COUNT2,D3DTTFF_COUNT3,D3DTTFF_COUNT4};
        if (s.colorOperation && size_t(*s.colorOperation) >= sizeof(operations)/sizeof(operations[0])) return false;
        if (s.alphaOperation && size_t(*s.alphaOperation) >= sizeof(operations)/sizeof(operations[0])) return false;
        if (s.colorArg1 && size_t(*s.colorArg1) >= sizeof(arguments)/sizeof(arguments[0])) return false;
        if (s.colorArg2 && size_t(*s.colorArg2) >= sizeof(arguments)/sizeof(arguments[0])) return false;
        if (s.alphaArg1 && size_t(*s.alphaArg1) >= sizeof(arguments)/sizeof(arguments[0])) return false;
        if (s.alphaArg2 && size_t(*s.alphaArg2) >= sizeof(arguments)/sizeof(arguments[0])) return false;
        if (s.result && size_t(*s.result) >= sizeof(arguments)/sizeof(arguments[0])) return false;
        if (s.transform && size_t(*s.transform) >= sizeof(transforms)/sizeof(transforms[0])) return false;
        if ((s.coordinateIndex && *s.coordinateIndex >= 8) || (s.result && *s.result != RendererBackend::TextureArgument::Current && *s.result != RendererBackend::TextureArgument::Temporary)) return false;
        bool ok=true;
        if (s.colorOperation && FAILED(device_->SetTextureStageState(stage,D3DTSS_COLOROP,operations[size_t(*s.colorOperation)]))) ok=false;
        if (s.alphaOperation && FAILED(device_->SetTextureStageState(stage,D3DTSS_ALPHAOP,operations[size_t(*s.alphaOperation)]))) ok=false;
        if (s.colorArg1 && FAILED(device_->SetTextureStageState(stage,D3DTSS_COLORARG1,arguments[size_t(*s.colorArg1)]))) ok=false;
        if (s.colorArg2 && FAILED(device_->SetTextureStageState(stage,D3DTSS_COLORARG2,arguments[size_t(*s.colorArg2)]))) ok=false;
        if (s.alphaArg1 && FAILED(device_->SetTextureStageState(stage,D3DTSS_ALPHAARG1,arguments[size_t(*s.alphaArg1)]))) ok=false;
        if (s.alphaArg2 && FAILED(device_->SetTextureStageState(stage,D3DTSS_ALPHAARG2,arguments[size_t(*s.alphaArg2)]))) ok=false;
        if (s.result && FAILED(device_->SetTextureStageState(stage,D3DTSS_RESULTARG,arguments[size_t(*s.result)]))) ok=false;
        if (s.coordinateIndex && FAILED(device_->SetTextureStageState(stage,D3DTSS_TEXCOORDINDEX,DWORD(*s.coordinateIndex)))) ok=false;
        if (s.transform && FAILED(device_->SetTextureStageState(stage,D3DTSS_TEXTURETRANSFORMFLAGS,transforms[size_t(*s.transform)]))) ok=false;
        return ok;
    }
    bool ApplyLightingState(const RendererBackend::LightingState& s) override
    {
        if (!device_) return false;
        static const DWORD sources[]={D3DMCS_MATERIAL,D3DMCS_COLOR1,D3DMCS_COLOR2};
        if (s.diffuseSource && size_t(*s.diffuseSource) >= sizeof(sources)/sizeof(sources[0])) return false;
        if (s.ambientSource && size_t(*s.ambientSource) >= sizeof(sources)/sizeof(sources[0])) return false;
        if (s.specularSource && size_t(*s.specularSource) >= sizeof(sources)/sizeof(sources[0])) return false;
        if (s.emissiveSource && size_t(*s.emissiveSource) >= sizeof(sources)/sizeof(sources[0])) return false;
        bool ok=true;
        if (s.enabled && FAILED(device_->SetRenderState(D3DRS_LIGHTING,DWORD(*s.enabled)))) ok=false;
        if (s.vertexColor && FAILED(device_->SetRenderState(D3DRS_COLORVERTEX,DWORD(*s.vertexColor)))) ok=false;
        if (s.normalizeNormals && FAILED(device_->SetRenderState(D3DRS_NORMALIZENORMALS,DWORD(*s.normalizeNormals)))) ok=false;
        if (s.specular && FAILED(device_->SetRenderState(D3DRS_SPECULARENABLE,DWORD(*s.specular)))) ok=false;
        if (s.localViewer && FAILED(device_->SetRenderState(D3DRS_LOCALVIEWER,DWORD(*s.localViewer)))) ok=false;
        if (s.ambient && FAILED(device_->SetRenderState(D3DRS_AMBIENT,DWORD(*s.ambient)))) ok=false;
        if (s.diffuseSource && FAILED(device_->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE,sources[size_t(*s.diffuseSource)]))) ok=false;
        if (s.ambientSource && FAILED(device_->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE,sources[size_t(*s.ambientSource)]))) ok=false;
        if (s.specularSource && FAILED(device_->SetRenderState(D3DRS_SPECULARMATERIALSOURCE,sources[size_t(*s.specularSource)]))) ok=false;
        if (s.emissiveSource && FAILED(device_->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE,sources[size_t(*s.emissiveSource)]))) ok=false;
        return ok;
    }
    RendererBackend::VertexLayout* CreateVertexLayout(const RendererBackend::VertexElement* elements, size_t count) override
    {
        if (!device_ || !elements || !count || count > 16) return nullptr;
        const BYTE types[] = {D3DDECLTYPE_FLOAT1,D3DDECLTYPE_FLOAT2,D3DDECLTYPE_FLOAT3,D3DDECLTYPE_FLOAT4,D3DDECLTYPE_D3DCOLOR};
        const BYTE usages[] = {D3DDECLUSAGE_POSITION,D3DDECLUSAGE_NORMAL,D3DDECLUSAGE_COLOR,D3DDECLUSAGE_TEXCOORD};
        const uint32_t sizes[] = {4,8,12,16,4};
        D3DVERTEXELEMENT9 declaration[17] = {};
        uint32_t end = 0;
        for (size_t i = 0; i < count; ++i)
        {
            const auto& e = elements[i];
            if (size_t(e.format) >= 5 || size_t(e.semantic) >= 4 || e.semanticIndex > 15 ||
                e.offset < end || e.offset % 4 || e.offset > 65535 - sizes[size_t(e.format)]) return nullptr;
            for (size_t j = 0; j < i; ++j)
                if (elements[j].semantic == e.semantic && elements[j].semanticIndex == e.semanticIndex) return nullptr;
            declaration[i] = {0, WORD(e.offset), types[size_t(e.format)], D3DDECLMETHOD_DEFAULT,
                usages[size_t(e.semantic)], BYTE(e.semanticIndex)};
            end = e.offset + sizes[size_t(e.format)];
        }
        declaration[count] = D3DDECL_END();
        IDirect3DVertexDeclaration9* native = nullptr;
        if (FAILED(device_->CreateVertexDeclaration(declaration, &native))) return nullptr;
        auto* layout = new (std::nothrow) VertexLayout(native);
        if (!layout) native->Release();
        return layout;
    }
    bool BindVertexLayout(RendererBackend::VertexLayout* layout) override
    {
        auto* native = dynamic_cast<VertexLayout*>(layout);
        return device_ && (!layout || native) && SUCCEEDED(device_->SetVertexDeclaration(native ? native->value : nullptr));
    }
    bool BindBuiltinVertexLayout(RendererBackend::BuiltinVertexLayout layout) override
    {
        if (!device_) return false;
        DWORD fvf;
        switch (layout) {
        case RendererBackend::BuiltinVertexLayout::Model: fvf = D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_DIFFUSE|D3DFVF_TEX1; break;
        case RendererBackend::BuiltinVertexLayout::PositionColor: fvf = D3DFVF_XYZ|D3DFVF_DIFFUSE; break;
        case RendererBackend::BuiltinVertexLayout::ScreenColorTexture: fvf = D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1; break;
        default: return false;
        }
        return SUCCEEDED(device_->SetFVF(fvf));
    }
    bool ApplySamplerState(uint32_t stage, const RendererBackend::SamplerState& s) override
    {
        using RendererBackend::TextureFilter;
        if (!device_ || stage >= 16) return false;
        const auto validFilter = [](const auto& f, bool mip) {
            return !f || (size_t(*f) < 4 && (mip ? *f != TextureFilter::Anisotropic : *f != TextureFilter::None));
        };
        const auto validAddress = [](const auto& a) { return !a || size_t(*a) < 5; };
        if (!validFilter(s.minFilter,false) || !validFilter(s.magFilter,false) || !validFilter(s.mipFilter,true) ||
            !validAddress(s.addressU) || !validAddress(s.addressV) || !validAddress(s.addressW) ||
            (s.maxAnisotropy && !*s.maxAnisotropy)) return false;
        static const DWORD filters[] = {D3DTEXF_NONE,D3DTEXF_POINT,D3DTEXF_LINEAR,D3DTEXF_ANISOTROPIC};
        static const DWORD addresses[] = {D3DTADDRESS_WRAP,D3DTADDRESS_MIRROR,D3DTADDRESS_CLAMP,D3DTADDRESS_BORDER,D3DTADDRESS_MIRRORONCE};
        bool ok = true;
        const auto set = [&](D3DSAMPLERSTATETYPE field, DWORD value) { if (FAILED(device_->SetSamplerState(stage,field,value))) ok = false; };
        const auto filter = [&](D3DSAMPLERSTATETYPE field, const auto& value) { if (value) set(field,filters[size_t(*value)]); };
        const auto address = [&](D3DSAMPLERSTATETYPE field, const auto& value) { if (value) set(field,addresses[size_t(*value)]); };
        filter(D3DSAMP_MINFILTER,s.minFilter); filter(D3DSAMP_MAGFILTER,s.magFilter); filter(D3DSAMP_MIPFILTER,s.mipFilter);
        address(D3DSAMP_ADDRESSU,s.addressU); address(D3DSAMP_ADDRESSV,s.addressV); address(D3DSAMP_ADDRESSW,s.addressW);
        if (s.srgb) set(D3DSAMP_SRGBTEXTURE,*s.srgb ? TRUE : FALSE);
        if (s.maxAnisotropy) set(D3DSAMP_MAXANISOTROPY,*s.maxAnisotropy);
        if (s.maxMipLevel) set(D3DSAMP_MAXMIPLEVEL,*s.maxMipLevel);
        if (s.borderColor) set(D3DSAMP_BORDERCOLOR,*s.borderColor);
        if (s.mipLodBias) { DWORD bits; std::memcpy(&bits,&*s.mipLodBias,sizeof(bits)); set(D3DSAMP_MIPMAPLODBIAS,bits); }
        return ok;
    }
    RendererBackend::RenderTarget* CreateRenderTarget(const RendererBackend::RenderTargetDescription& d) override
    {
        using RendererBackend::TargetFormat;
        if (!device_ || !d.width || !d.height || d.samples > 16 || d.samples == 1 ||
            (d.sampleable && (d.samples || d.format == TargetFormat::Depth24Stencil8))) return nullptr;
        D3DFORMAT format;
        switch (d.format) {
        case TargetFormat::Bgra8: format = D3DFMT_A8R8G8B8; break;
        case TargetFormat::Bgrx8: format = D3DFMT_X8R8G8B8; break;
        case TargetFormat::R32Float: format = D3DFMT_R32F; break;
        case TargetFormat::Depth24Stencil8: format = D3DFMT_D24S8; break;
        default: return nullptr;
        }
        IDirect3DSurface9* surface = nullptr;
        IDirect3DTexture9* texture = nullptr;
        HRESULT result;
        if (d.sampleable)
        {
            result = device_->CreateTexture(d.width, d.height, 1, D3DUSAGE_RENDERTARGET,
                format, D3DPOOL_DEFAULT, &texture, nullptr);
            if (SUCCEEDED(result)) result = texture->GetSurfaceLevel(0, &surface);
        }
        else if (d.format == TargetFormat::Depth24Stencil8)
            result = device_->CreateDepthStencilSurface(d.width, d.height, format,
                D3DMULTISAMPLE_TYPE(d.samples), d.quality, TRUE, &surface, nullptr);
        else result = device_->CreateRenderTarget(d.width, d.height, format,
            D3DMULTISAMPLE_TYPE(d.samples), d.quality, FALSE, &surface, nullptr);
        auto* owned = SUCCEEDED(result) ? new (std::nothrow) RenderTarget(surface, texture) : nullptr;
        if (!owned) { if (surface) surface->Release(); if (texture) texture->Release(); }
        return owned;
    }
    RendererBackend::RenderTarget* CaptureRenderTarget(bool depth = false) override
    {
        if (!device_) return nullptr;
        IDirect3DSurface9* surface = nullptr;
        if (FAILED(depth ? device_->GetDepthStencilSurface(&surface) : device_->GetRenderTarget(0, &surface))) return nullptr;
        auto* owned = new (std::nothrow) RenderTarget(surface);
        if (!owned) surface->Release();
        return owned;
    }
    bool BindRenderTarget(RendererBackend::RenderTarget* target, bool depth = false) override
    {
        auto* surface = NativeSurface(target);
        if (!device_ || (target && !surface) || (!depth && !target)) return false;
        return SUCCEEDED(depth ? device_->SetDepthStencilSurface(surface) : device_->SetRenderTarget(0, surface));
    }
    bool CopyRenderTarget(RendererBackend::RenderTarget* source, RendererBackend::RenderTarget* destination) override
    {
        auto* from = NativeSurface(source); auto* to = NativeSurface(destination);
        return device_ && from && to && SUCCEEDED(device_->StretchRect(from, nullptr, to, nullptr, D3DTEXF_NONE));
    }
    RendererBackend::PipelineSnapshot* CapturePipelineState() override
    {
        if (!device_) return nullptr;
        IDirect3DStateBlock9* state = nullptr;
        if (FAILED(device_->CreateStateBlock(D3DSBT_ALL, &state))) return nullptr;
        if (FAILED(state->Capture())) { state->Release(); return nullptr; }
        auto* owned = new (std::nothrow) PipelineSnapshot(state);
        if (!owned) state->Release();
        return owned;
    }
    bool ApplyPipelineState(const RendererBackend::PipelineState& p) override
    {
        if (!device_) return false;
        bool ok = true;
        const auto set = [&](D3DRENDERSTATETYPE state, DWORD value) { if (FAILED(device_->SetRenderState(state, value))) ok = false; };
        const auto boolean = [&](D3DRENDERSTATETYPE state, const std::optional<bool>& v) { if (v) set(state, *v ? TRUE : FALSE); };
        boolean(D3DRS_ZENABLE,p.depthTest); boolean(D3DRS_ZWRITEENABLE,p.depthWrite);
        boolean(D3DRS_ALPHABLENDENABLE,p.blend); boolean(D3DRS_ALPHATESTENABLE,p.alphaTest);
        boolean(D3DRS_STENCILENABLE,p.stencil); boolean(D3DRS_SCISSORTESTENABLE,p.scissor);
        boolean(D3DRS_FOGENABLE,p.fog); boolean(D3DRS_SRGBWRITEENABLE,p.srgbWrite);
        boolean(D3DRS_SEPARATEALPHABLENDENABLE,p.separateAlphaBlend); boolean(D3DRS_MULTISAMPLEANTIALIAS,p.multisample);
        // Explicit tables keep shared enum values independent of D3D9 constants.
        static const DWORD comparisons[] = {D3DCMP_NEVER,D3DCMP_LESS,D3DCMP_EQUAL,D3DCMP_LESSEQUAL,D3DCMP_GREATER,D3DCMP_NOTEQUAL,D3DCMP_GREATEREQUAL,D3DCMP_ALWAYS};
        static const DWORD culls[] = {D3DCULL_NONE,D3DCULL_CW,D3DCULL_CCW};
        static const DWORD blends[] = {D3DBLEND_ZERO,D3DBLEND_ONE,D3DBLEND_SRCCOLOR,D3DBLEND_INVSRCCOLOR,D3DBLEND_SRCALPHA,D3DBLEND_INVSRCALPHA,D3DBLEND_DESTALPHA,D3DBLEND_INVDESTALPHA,D3DBLEND_DESTCOLOR,D3DBLEND_INVDESTCOLOR};
        const auto mapped = [&](D3DRENDERSTATETYPE state, auto value, const auto& table) {
            if (value) { const auto i = size_t(*value); if (i >= sizeof(table)/sizeof(table[0])) ok = false; else set(state,table[i]); }
        };
        mapped(D3DRS_ZFUNC,p.depthCompare,comparisons); mapped(D3DRS_CULLMODE,p.cull,culls);
        mapped(D3DRS_SRCBLEND,p.sourceBlend,blends); mapped(D3DRS_DESTBLEND,p.destinationBlend,blends);
        if (p.colorWriteMask) { if (*p.colorWriteMask > 15) ok = false; else set(D3DRS_COLORWRITEENABLE,*p.colorWriteMask); }
        const auto floating = [&](D3DRENDERSTATETYPE state, const std::optional<float>& v) { if (v) { DWORD bits; std::memcpy(&bits,&*v,sizeof(bits)); set(state,bits); } };
        floating(D3DRS_DEPTHBIAS,p.depthBias); floating(D3DRS_SLOPESCALEDEPTHBIAS,p.slopeDepthBias);
        static const DWORD fills[]={D3DFILL_POINT,D3DFILL_WIREFRAME,D3DFILL_SOLID};
        static const DWORD blendOps[]={D3DBLENDOP_ADD,D3DBLENDOP_SUBTRACT,D3DBLENDOP_REVSUBTRACT,D3DBLENDOP_MIN,D3DBLENDOP_MAX};
        static const DWORD fogModes[]={D3DFOG_NONE,D3DFOG_EXP,D3DFOG_EXP2,D3DFOG_LINEAR};
        boolean(D3DRS_CLIPPING,p.clipping);
        boolean(D3DRS_DITHERENABLE,p.dither);
        boolean(D3DRS_RANGEFOGENABLE,p.rangeFog);
        if(p.alphaReference) set(D3DRS_ALPHAREF,*p.alphaReference);
        if(p.clipPlaneMask) set(D3DRS_CLIPPLANEENABLE,*p.clipPlaneMask);
        if(p.multisampleMask) set(D3DRS_MULTISAMPLEMASK,*p.multisampleMask);
        if(p.fogColor) set(D3DRS_FOGCOLOR,*p.fogColor);
        floating(D3DRS_FOGSTART,p.fogStart);
        floating(D3DRS_FOGEND,p.fogEnd);
        mapped(D3DRS_ALPHAFUNC,p.alphaCompare,comparisons);
        mapped(D3DRS_FILLMODE,p.fill,fills);
        mapped(D3DRS_BLENDOP,p.blendOperation,blendOps);
        mapped(D3DRS_FOGTABLEMODE,p.fogTableMode,fogModes);
        mapped(D3DRS_FOGVERTEXMODE,p.fogVertexMode,fogModes);
        return ok;
    }
    RendererBackend::Shader* CreateShader(const RendererBackend::ShaderSource& source) override
    {
        using RendererBackend::ShaderStage;
        if (!device_ || !source.source || !source.length || !source.entryPoint || !source.profile ||
            (source.stage != ShaderStage::Vertex && source.stage != ShaderStage::Pixel)) return nullptr;
        ID3DBlob* code = nullptr;
        ID3DBlob* errors = nullptr;
        const HRESULT compiled = D3DCompile(source.source, source.length, source.name,
            nullptr, nullptr, source.entryPoint, source.profile, 0, 0, &code, &errors);
        if (FAILED(compiled) && errors)
            OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
        if (errors) errors->Release();
        if (FAILED(compiled)) { if (code) code->Release(); return nullptr; }
        Shader* result = nullptr;
        if (source.stage == ShaderStage::Vertex)
        {
            IDirect3DVertexShader9* native = nullptr;
            if (SUCCEEDED(device_->CreateVertexShader(static_cast<const DWORD*>(code->GetBufferPointer()), &native)))
            {
                result = new (std::nothrow) Shader(native);
                if (!result) native->Release();
            }
        }
        else
        {
            IDirect3DPixelShader9* native = nullptr;
            if (SUCCEEDED(device_->CreatePixelShader(static_cast<const DWORD*>(code->GetBufferPointer()), &native)))
            {
                result = new (std::nothrow) Shader(native);
                if (!result) native->Release();
            }
        }
        code->Release();
        return result;
    }
    bool BindShader(RendererBackend::ShaderStage stage, RendererBackend::Shader* shader) override
    {
        using RendererBackend::ShaderStage;
        auto* native = dynamic_cast<Shader*>(shader);
        if (!device_ || (shader && (!native || shader->Stage() != stage))) return false;
        if (stage == ShaderStage::Vertex)
            return SUCCEEDED(device_->SetVertexShader(native ? native->vertex : nullptr));
        if (stage == ShaderStage::Pixel)
            return SUCCEEDED(device_->SetPixelShader(native ? native->pixel : nullptr));
        return false;
    }
    bool SetShaderConstants(RendererBackend::ShaderStage stage, uint32_t first,
        const float* values, uint32_t count) override
    {
        if (!device_ || !values || !count) return false;
        if (stage == RendererBackend::ShaderStage::Vertex)
            return SUCCEEDED(device_->SetVertexShaderConstantF(first, values, count));
        if (stage == RendererBackend::ShaderStage::Pixel)
            return SUCCEEDED(device_->SetPixelShaderConstantF(first, values, count));
        return false;
    }
    bool BindTexture(uint32_t stage, RendererBackend::Texture* texture) override
    {
        auto* native = NativeTexture(texture);
        if (!device_ || (texture && !native)) return false;
        return SUCCEEDED(device_->SetTexture(stage, native));
    }
    RendererBackend::Texture* CreateTexture(const RendererBackend::TextureUpload& u) override
    {
        using RendererBackend::TextureFormat;
        if (!device_ || !u.data || u.width <= 0 || u.height <= 0) return nullptr;
        D3DFORMAT format;
        size_t rowBytes, rows;
        switch (u.format)
        {
        case TextureFormat::Bgra8: format = D3DFMT_A8R8G8B8; rowBytes = size_t(u.width) * 4; rows = u.height; break;
        case TextureFormat::BC1: format = D3DFMT_DXT1; rowBytes = ((size_t(u.width)+3)/4)*8; rows = (size_t(u.height)+3)/4; break;
        case TextureFormat::BC2: format = D3DFMT_DXT3; rowBytes = ((size_t(u.width)+3)/4)*16; rows = (size_t(u.height)+3)/4; break;
        case TextureFormat::BC3: format = D3DFMT_DXT5; rowBytes = ((size_t(u.width)+3)/4)*16; rows = (size_t(u.height)+3)/4; break;
        default: return nullptr;
        }
        if (rowBytes > u.dataSize / rows) return nullptr;
        IDirect3DTexture9* texture = nullptr;
        bool autoMips = u.generateMips;
        HRESULT result = device_->CreateTexture(u.width, u.height, autoMips ? 0 : 1,
            autoMips ? D3DUSAGE_AUTOGENMIPMAP : 0, format, D3DPOOL_MANAGED, &texture, nullptr);
        if (FAILED(result) && autoMips && u.allowMipFallback)
        {
            autoMips = false;
            result = device_->CreateTexture(u.width, u.height, 1, 0, format, D3DPOOL_MANAGED, &texture, nullptr);
        }
        if (FAILED(result)) return nullptr;
        D3DLOCKED_RECT lock = {};
        if (FAILED(texture->LockRect(0, &lock, nullptr, 0))) { texture->Release(); return nullptr; }
        if (lock.Pitch < 0 || size_t(lock.Pitch) < rowBytes)
        { texture->UnlockRect(0); texture->Release(); return nullptr; }
        for (size_t row = 0; row < rows; ++row)
            std::memcpy(static_cast<unsigned char*>(lock.pBits) + row * lock.Pitch,
                        static_cast<const unsigned char*>(u.data) + row * rowBytes, rowBytes);
        texture->UnlockRect(0);
        if (autoMips && texture->GetLevelCount() > 1)
        {
            texture->SetAutoGenFilterType(D3DTEXF_LINEAR);
            texture->GenerateMipSubLevels();
        }
        auto* resource = new (std::nothrow) Texture(texture);
        if (!resource) texture->Release();
        return resource;
    }
    RendererBackend::MeshBuffer* CreateMeshBuffer(RendererBackend::BufferKind kind, size_t bytes) override
    {
        if (!device_ || !bytes || bytes > (std::numeric_limits<UINT>::max)()) return nullptr;
        if (kind == RendererBackend::BufferKind::Vertex)
        {
            IDirect3DVertexBuffer9* buffer = nullptr;
            // Existing FFXI vertex layout: position, normal, diffuse, one UV set.
            if (FAILED(device_->CreateVertexBuffer((UINT)bytes, D3DUSAGE_WRITEONLY,
                D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1,
                D3DPOOL_MANAGED, &buffer, nullptr))) return nullptr;
            auto* resource = new (std::nothrow) MeshBuffer(buffer, bytes);
            if (!resource) buffer->Release();
            return resource;
        }
        if (kind != RendererBackend::BufferKind::Index32 || bytes % 4) return nullptr;
        IDirect3DIndexBuffer9* buffer = nullptr;
        if (FAILED(device_->CreateIndexBuffer((UINT)bytes, D3DUSAGE_WRITEONLY,
            D3DFMT_INDEX32, D3DPOOL_MANAGED, &buffer, nullptr))) return nullptr;
        auto* resource = new (std::nothrow) MeshBuffer(buffer, bytes);
        if (!resource) buffer->Release();
        return resource;
    }
    bool DrawIndexed(const RendererBackend::MeshDraw& d) override
    {
        auto* vertices = dynamic_cast<MeshBuffer*>(d.vertices);
        auto* indices = dynamic_cast<MeshBuffer*>(d.indices);
        if (!device_ || !vertices || !indices || !vertices->Vertex() || !indices->Index() || !d.stride) return false;
        device_->SetStreamSource(0, vertices->Vertex(), 0, d.stride);
        device_->SetIndices(indices->Index());
        return SUCCEEDED(device_->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, d.baseVertex,
            d.minVertex, d.vertexCount, d.startIndex, d.primitiveCount));
    }
    bool DrawImmediate(RendererBackend::Primitive primitive, uint32_t count,
                       const void* vertices, uint32_t stride) override
    {
        if (!device_ || !vertices || !stride) return false;
        D3DPRIMITIVETYPE type;
        switch (primitive)
        {
        case RendererBackend::Primitive::TriangleList: type = D3DPT_TRIANGLELIST; break;
        case RendererBackend::Primitive::TriangleStrip: type = D3DPT_TRIANGLESTRIP; break;
        case RendererBackend::Primitive::TriangleFan: type = D3DPT_TRIANGLEFAN; break;
        case RendererBackend::Primitive::LineList: type = D3DPT_LINELIST; break;
        default: return false;
        }
        return SUCCEEDED(device_->DrawPrimitiveUP(type, count, vertices, stride));
    }
    bool DrawIndexedImmediate(uint32_t vertexCount, uint32_t primitiveCount,
        const void* indices32, const void* vertices, uint32_t stride) override
    {
        if (!device_ || !indices32 || !vertices || !stride) return false;
        return SUCCEEDED(device_->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0,
            vertexCount, primitiveCount, indices32, D3DFMT_INDEX32, vertices, stride));
    }
private:
    IDirect3DDevice9* device_;
};
}
