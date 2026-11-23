#include "renderer_backend.h"
#include "d3d9_device.h"
#include <cstdio>
#include <type_traits>
#include <array>
#include <cstring>

static bool TestResources(IDirect3DDevice9* device)
{
    D3D9Graphics::Context implementation(device);
    RendererBackend::Graphics& graphics = implementation;
    {
        using namespace RendererBackend;
        std::unique_ptr<PipelineSnapshot> saved(graphics.CapturePipelineState());
        if (!saved) return false;
        const float matrix[16]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
        const Transform kinds[]={Transform::World,Transform::View,Transform::Projection};
        const D3DTRANSFORMSTATETYPE nativeKinds[]={D3DTS_WORLD,D3DTS_VIEW,D3DTS_PROJECTION};
        for(size_t i=0;i<3;++i) {
            if(!graphics.SetTransform(kinds[i],matrix)) return false;
            D3DMATRIX native={}; device->GetTransform(nativeKinds[i],&native);
            if(std::memcmp(matrix,&native,sizeof(native))) return false;
            native._41=-37.25f; native._23=.125f;
            if(FAILED(device->SetTransform(nativeKinds[i],&native))) return false;
            float queried[16]={};
            if(!graphics.QueryTransform(kinds[i],queried) || std::memcmp(queried,&native,sizeof(native))) return false;
        }
        if(!graphics.SetTextureTransform(3,matrix)) return false;
        float queried[16]={};
        if(!graphics.QueryTextureTransform(3,queried) || std::memcmp(matrix,queried,sizeof(matrix))) return false;
        Viewport viewport;
        if(!graphics.QueryViewport(viewport)) return false;
        Viewport croppedViewport={1,2,16,12,.125f,.875f};
        if(!graphics.SetViewport(croppedViewport)) return false;
        D3DVIEWPORT9 nativeViewport={}; device->GetViewport(&nativeViewport);
        if(nativeViewport.X!=1 || nativeViewport.Y!=2 || nativeViewport.Width!=16 ||
            nativeViewport.Height!=12 || nativeViewport.MinZ!=.125f || nativeViewport.MaxZ!=.875f) return false;
        Viewport actual;
        if(!graphics.QueryViewport(actual) || actual.width!=16 || actual.minDepth!=.125f) return false;
        float start=12.5f,end=80.25f; DWORD bits;
        std::memcpy(&bits,&start,4); device->SetRenderState(D3DRS_FOGSTART,bits);
        std::memcpy(&bits,&end,4); device->SetRenderState(D3DRS_FOGEND,bits);
        device->SetRenderState(D3DRS_FOGENABLE,TRUE); device->SetRenderState(D3DRS_FOGCOLOR,0xff123456);
        FogState fog;
        if(!graphics.QueryFogState(fog) || !fog.enabled || fog.color!=0xff123456 || fog.start!=start || fog.end!=end) return false;
        D3D9Graphics::Context idle;
        Viewport untouched=croppedViewport;
        if(idle.QueryViewport(untouched) || untouched.width!=croppedViewport.width ||
            idle.QueryTransform(Transform::World,queried) || std::memcmp(matrix,queried,sizeof(matrix)) ||
            graphics.QueryTextureTransform(8,queried) || graphics.QueryTransform(Transform(99),queried)) return false;
        SamplerState badSampler; badSampler.srgb=true;
        if(graphics.QuerySamplerState(16,badSampler) || badSampler.srgb!=true) return false;
        if(!graphics.SetViewport(viewport) || !saved->Restore()) return false;
        std::puts("Shared matrix/viewport/fog queries, native readback and failure-output preservation passed.");
    }
    {
        using namespace RendererBackend;
        std::unique_ptr<PipelineSnapshot> restore(graphics.CapturePipelineState());
        if (!restore) return false;
        DWORD oldMask=0, oldAmbient=0, oldOperation=0;
        device->GetRenderState(D3DRS_STENCILMASK,&oldMask);
        device->GetRenderState(D3DRS_AMBIENT,&oldAmbient);
        device->GetTextureStageState(1,D3DTSS_COLOROP,&oldOperation);
        StencilState stencil;
        stencil.enabled=true; stencil.twoSided=true; stencil.reference=3;
        stencil.readMask=0x80; stencil.writeMask=0x40;
        stencil.compare=Compare::Equal; stencil.fail=StencilOperation::Keep;
        stencil.depthFail=StencilOperation::Replace; stencil.pass=StencilOperation::Invert;
        stencil.backCompare=Compare::Greater; stencil.backFail=StencilOperation::Zero;
        stencil.backDepthFail=StencilOperation::IncrementSaturate; stencil.backPass=StencilOperation::Decrement;
        if (!graphics.ApplyStencilState(stencil)) return false;
        StencilState queriedStencil;
        if (!graphics.QueryStencilState(queriedStencil) || queriedStencil.reference!=3 ||
            queriedStencil.backPass!=StencilOperation::Decrement || queriedStencil.pass!=StencilOperation::Invert) return false;
        const D3DRENDERSTATETYPE fields[]={D3DRS_STENCILENABLE,D3DRS_TWOSIDEDSTENCILMODE,
            D3DRS_STENCILREF,D3DRS_STENCILMASK,D3DRS_STENCILWRITEMASK,D3DRS_STENCILFUNC,
            D3DRS_STENCILFAIL,D3DRS_STENCILZFAIL,D3DRS_STENCILPASS,D3DRS_CCW_STENCILFUNC,
            D3DRS_CCW_STENCILFAIL,D3DRS_CCW_STENCILZFAIL,D3DRS_CCW_STENCILPASS};
        const DWORD expected[]={TRUE,TRUE,3,0x80,0x40,D3DCMP_EQUAL,D3DSTENCILOP_KEEP,
            D3DSTENCILOP_REPLACE,D3DSTENCILOP_INVERT,D3DCMP_GREATER,D3DSTENCILOP_ZERO,D3DSTENCILOP_INCRSAT,D3DSTENCILOP_DECR};
        for(size_t i=0;i<sizeof(fields)/sizeof(fields[0]);++i) {
            DWORD v=0; device->GetRenderState(fields[i],&v); if(v!=expected[i]) return false;
        }
        TextureStageState stage;
        stage.colorOperation=TextureOperation::Modulate2X; stage.alphaOperation=TextureOperation::Modulate4X;
        stage.colorArg1=TextureArgument::Texture; stage.colorArg2=TextureArgument::Diffuse;
        stage.alphaArg1=TextureArgument::Current; stage.alphaArg2=TextureArgument::Factor;
        stage.result=TextureArgument::Current; stage.coordinateIndex=1; stage.transform=TextureTransform::Count2;
        if(!graphics.ApplyTextureStageState(1,stage)) return false;
        TextureStageState queriedStage;
        if(!graphics.QueryTextureStageState(1,queriedStage) || queriedStage.alphaOperation!=TextureOperation::Modulate4X ||
            queriedStage.alphaArg2!=TextureArgument::Factor || queriedStage.transform!=TextureTransform::Count2) return false;
        const D3DTEXTURESTAGESTATETYPE textureFields[]={D3DTSS_COLOROP,D3DTSS_ALPHAOP,D3DTSS_COLORARG1,
            D3DTSS_COLORARG2,D3DTSS_ALPHAARG1,D3DTSS_ALPHAARG2,D3DTSS_RESULTARG,D3DTSS_TEXCOORDINDEX,D3DTSS_TEXTURETRANSFORMFLAGS};
        const DWORD textureExpected[]={D3DTOP_MODULATE2X,D3DTOP_MODULATE4X,D3DTA_TEXTURE,D3DTA_DIFFUSE,
            D3DTA_CURRENT,D3DTA_TFACTOR,D3DTA_CURRENT,1,D3DTTFF_COUNT2};
        for(size_t i=0;i<9;++i) { DWORD v=0; device->GetTextureStageState(1,textureFields[i],&v); if(v!=textureExpected[i]) return false; }
        float transform[16]={1,0,0,0,0,1,0,0,.125f,.25f,1,0,0,0,0,1};
        D3DMATRIX actual = {};
        if(!graphics.SetTextureTransform(1,transform)) return false;
        device->GetTransform(D3DTS_TEXTURE1,&actual);
        if(std::memcmp(transform,&actual,sizeof(actual))) return false;
        LightingState lighting;
        lighting.enabled=true; lighting.vertexColor=true; lighting.normalizeNormals=true;
        lighting.specular=false; lighting.ambient=0xff123456;
        lighting.diffuseSource=MaterialSource::DiffuseVertex; lighting.ambientSource=MaterialSource::Material;
        if(!graphics.ApplyLightingState(lighting)) return false;
        LightingState queriedLighting;
        if(!graphics.QueryLightingState(queriedLighting) || queriedLighting.ambient!=0xff123456 ||
            queriedLighting.diffuseSource!=MaterialSource::DiffuseVertex || queriedLighting.enabled!=true) return false;
        DWORD v=0; device->GetRenderState(D3DRS_AMBIENT,&v); if(v!=0xff123456) return false;
        device->GetRenderState(D3DRS_DIFFUSEMATERIALSOURCE,&v); if(v!=D3DMCS_COLOR1) return false;
        for(auto type : {LightType::Directional,LightType::Point,LightType::Spot}) {
            Light light; light.type=type; light.diffuse={.25f,.5f,.75f,1};
            light.position={1,2,3}; light.direction={0,-1,0}; light.range=100;
            light.attenuation0=1; light.attenuation1=.125f; light.falloff=1; light.theta=.25f; light.phi=.5f;
            if(!graphics.SetLight(0,light) || !graphics.EnableLight(0,true)) return false;
            D3DLIGHT9 native={}; BOOL enabled=FALSE;
            device->GetLight(0,&native); device->GetLightEnable(0,&enabled);
            const D3DLIGHTTYPE expectedType=type==LightType::Directional?D3DLIGHT_DIRECTIONAL:type==LightType::Point?D3DLIGHT_POINT:D3DLIGHT_SPOT;
            if(native.Type!=expectedType || native.Diffuse.g!=.5f || native.Position.z!=3 ||
                native.Direction.y!=-1 || native.Attenuation1!=.125f || native.Phi!=.5f || !enabled) return false;
        }
        if(!graphics.EnableLight(0,false)) return false;
        stage.colorOperation=TextureOperation(999);
        stencil.pass=StencilOperation(999);
        if(graphics.ApplyTextureStageState(1,stage) || graphics.ApplyTextureStageState(8,{}) ||
            graphics.ApplyStencilState(stencil)) return false;
        if(!restore->Restore()) return false;
        device->GetRenderState(D3DRS_STENCILMASK,&v); if(v!=oldMask) return false;
        device->GetRenderState(D3DRS_AMBIENT,&v); if(v!=oldAmbient) return false;
        device->GetTextureStageState(1,D3DTSS_COLOROP,&v); if(v!=oldOperation) return false;
        std::puts("Shared stencil, texture-stage, transform and lighting readback/restoration passed.");
    }
    {
        using namespace RendererBackend;
        std::unique_ptr<PipelineSnapshot> restore(graphics.CapturePipelineState());
        if (!restore) return false;
        const VertexElement elements[] = {
            {0,VertexFormat::Float4,VertexSemantic::Position},
            {16,VertexFormat::Float2,VertexSemantic::TexCoord}
        };
        std::unique_ptr<VertexLayout> layout(graphics.CreateVertexLayout(elements,2));
        if (!layout || !graphics.BindVertexLayout(layout.get())) return false;
        IDirect3DVertexDeclaration9* native = nullptr;
        if (FAILED(device->GetVertexDeclaration(&native)) || !native) return false;
        D3DVERTEXELEMENT9 actual[3] = {}; UINT count = 3;
        const HRESULT hr = native->GetDeclaration(actual,&count);
        native->Release();
        if (FAILED(hr) || count != 3 || actual[0].Type != D3DDECLTYPE_FLOAT4 ||
            actual[0].Usage != D3DDECLUSAGE_POSITION || actual[1].Offset != 16 ||
            actual[1].Type != D3DDECLTYPE_FLOAT2 || actual[1].Usage != D3DDECLUSAGE_TEXCOORD ||
            actual[2].Stream != 0xff) return false;
        const BuiltinVertexLayout presets[] = {BuiltinVertexLayout::Model,
            BuiltinVertexLayout::PositionColor,BuiltinVertexLayout::ScreenColorTexture};
        const DWORD expected[] = {D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_DIFFUSE|D3DFVF_TEX1,
            D3DFVF_XYZ|D3DFVF_DIFFUSE,D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1};
        for (size_t i = 0; i < 3; ++i)
        {
            DWORD fvf = 0;
            if (!graphics.BindBuiltinVertexLayout(presets[i]) || FAILED(device->GetFVF(&fvf)) || fvf != expected[i]) return false;
        }
        VertexElement invalid[] = {elements[0],elements[1]}; invalid[1].offset = 4;
        if (graphics.CreateVertexLayout(nullptr,1) || graphics.CreateVertexLayout(invalid,2)) return false;
        invalid[1] = elements[1]; invalid[1].semantic = VertexSemantic::Position;
        if (graphics.CreateVertexLayout(invalid,2) || !graphics.BindVertexLayout(nullptr)) return false;
        SamplerState sampler;
        sampler.minFilter = TextureFilter::Linear; sampler.magFilter = TextureFilter::Point;
        sampler.mipFilter = TextureFilter::None; sampler.addressU = TextureAddress::Clamp;
        sampler.addressV = TextureAddress::Wrap; sampler.srgb = false;
        DWORD untouched = 0; device->GetSamplerState(0,D3DSAMP_ADDRESSU,&untouched);
        if (!graphics.ApplySamplerState(1,sampler)) return false;
        SamplerState queriedSampler;
        if(!graphics.QuerySamplerState(1,queriedSampler) || queriedSampler.minFilter!=TextureFilter::Linear ||
            queriedSampler.magFilter!=TextureFilter::Point || queriedSampler.addressU!=TextureAddress::Clamp) return false;
        const D3DSAMPLERSTATETYPE fields[] = {D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER,D3DSAMP_MIPFILTER,D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV,D3DSAMP_SRGBTEXTURE};
        const DWORD values[] = {D3DTEXF_LINEAR,D3DTEXF_POINT,D3DTEXF_NONE,D3DTADDRESS_CLAMP,D3DTADDRESS_WRAP,FALSE};
        for (size_t i = 0; i < 6; ++i) { DWORD v=0; device->GetSamplerState(1,fields[i],&v); if(v!=values[i]) return false; }
        if (!graphics.SetSamplerField(1,&SamplerState::mipFilter,TextureFilter::Linear)) return false;
        DWORD v = 0; device->GetSamplerState(1,D3DSAMP_ADDRESSU,&v); if(v!=D3DTADDRESS_CLAMP) return false;
        device->GetSamplerState(0,D3DSAMP_ADDRESSU,&v); if(v!=untouched) return false;
        sampler.minFilter = TextureFilter::None;
        if (graphics.ApplySamplerState(1,sampler) || graphics.ApplySamplerState(16,{})) return false;
        if (!restore->Restore()) return false;
        std::puts("Shared vertex declarations, fixed-function layouts, sampler mappings and isolation passed.");
    }
    using RendererBackend::TargetFormat;
    using RendererBackend::RenderTarget;
    std::unique_ptr<RenderTarget> originalTarget(graphics.CaptureRenderTarget());
    std::unique_ptr<RenderTarget> originalDepth(graphics.CaptureRenderTarget(true));
    std::unique_ptr<RendererBackend::PipelineSnapshot> saved(graphics.CapturePipelineState());
    std::unique_ptr<RenderTarget> color(graphics.CreateRenderTarget({32,32,TargetFormat::Bgra8,0,0,true}));
    std::unique_ptr<RenderTarget> copy(graphics.CreateRenderTarget({32,32,TargetFormat::Bgra8,0,0,true}));
    std::unique_ptr<RenderTarget> depth(graphics.CreateRenderTarget({32,32,TargetFormat::Depth24Stencil8}));
    if (!originalTarget || !saved || !color || !copy || !depth ||
        color->Description().width != 32 || !color->Description().sampleable ||
        depth->Description().format != TargetFormat::Depth24Stencil8) return false;
    if (graphics.CreateRenderTarget({0,32}) ||
        graphics.CreateRenderTarget({32,32,TargetFormat::Depth24Stencil8,0,0,true})) return false;
    if (!graphics.BindRenderTarget(nullptr,true) || !graphics.BindRenderTarget(color.get()) ||
        !graphics.BindRenderTarget(depth.get(),true)) return false;
    if (FAILED(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff123456,1,0))) return false;
    if (!graphics.BindRenderTarget(nullptr,true) || !graphics.BindRenderTarget(originalTarget.get()) ||
        !graphics.CopyRenderTarget(color.get(),copy.get())) return false;
    IDirect3DSurface9* pixelsSurface = nullptr;
    if (FAILED(device->CreateOffscreenPlainSurface(32,32,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&pixelsSurface,nullptr))) return false;
    HRESULT copyResult = device->GetRenderTargetData(D3D9Graphics::NativeSurface(copy.get()),pixelsSurface);
    D3DLOCKED_RECT pixelLock = {};
    if (SUCCEEDED(copyResult)) copyResult = pixelsSurface->LockRect(&pixelLock,nullptr,D3DLOCK_READONLY);
    bool copied = false;
    if (SUCCEEDED(copyResult)) { copied = *static_cast<const DWORD*>(pixelLock.pBits) == 0xff123456; pixelsSurface->UnlockRect(); }
    pixelsSurface->Release();
    if (!copied || !graphics.BindTexture(0,color.get()) || !graphics.BindTexture(0,nullptr) ||
        graphics.BindTexture(0,depth.get())) return false;
    DWORD originalCull = 0, originalZ = 0;
    device->GetRenderState(D3DRS_CULLMODE,&originalCull);
    device->GetRenderState(D3DRS_ZWRITEENABLE,&originalZ);
    RendererBackend::PipelineState pipeline;
    pipeline.cull = RendererBackend::Cull::None;
    pipeline.depthWrite = false;
    pipeline.sourceBlend = RendererBackend::BlendFactor::SourceAlpha;
    pipeline.depthCompare = RendererBackend::Compare::Greater;
    pipeline.fill = RendererBackend::Fill::Wireframe;
    pipeline.blendOperation = RendererBackend::BlendOperation::ReverseSubtract;
    pipeline.alphaCompare = RendererBackend::Compare::GreaterEqual;
    pipeline.alphaReference = 73;
    pipeline.clipping = false;
    pipeline.dither = true;
    pipeline.rangeFog = true;
    pipeline.clipPlaneMask = 1;
    pipeline.multisampleMask = 0x55;
    pipeline.fogColor = 0xff123456;
    pipeline.fogStart = 12.5f;
    pipeline.fogEnd = 1024.0f;
    pipeline.fogTableMode = RendererBackend::FogMode::Linear;
    pipeline.fogVertexMode = RendererBackend::FogMode::ExponentialSquared;
    if (!graphics.ApplyPipelineState(pipeline)) return false;
    RendererBackend::PipelineState queriedPipeline;
    if(!graphics.QueryPipelineState(queriedPipeline) || queriedPipeline.depthWrite!=false ||
        queriedPipeline.depthCompare!=RendererBackend::Compare::Greater ||
        queriedPipeline.sourceBlend!=RendererBackend::BlendFactor::SourceAlpha) return false;
    if (queriedPipeline.fill != pipeline.fill || queriedPipeline.blendOperation != pipeline.blendOperation ||
        queriedPipeline.alphaCompare != pipeline.alphaCompare || queriedPipeline.alphaReference != pipeline.alphaReference ||
        queriedPipeline.clipping != pipeline.clipping || queriedPipeline.dither != pipeline.dither ||
        queriedPipeline.rangeFog != pipeline.rangeFog || queriedPipeline.clipPlaneMask != pipeline.clipPlaneMask ||
        queriedPipeline.multisampleMask != pipeline.multisampleMask || queriedPipeline.fogColor != pipeline.fogColor ||
        queriedPipeline.fogStart != pipeline.fogStart || queriedPipeline.fogEnd != pipeline.fogEnd ||
        queriedPipeline.fogTableMode != pipeline.fogTableMode || queriedPipeline.fogVertexMode != pipeline.fogVertexMode) return false;
    const std::pair<D3DRENDERSTATETYPE,DWORD> expected[] = {
        {D3DRS_FILLMODE,D3DFILL_WIREFRAME}, {D3DRS_BLENDOP,D3DBLENDOP_REVSUBTRACT},
        {D3DRS_ALPHAFUNC,D3DCMP_GREATEREQUAL}, {D3DRS_ALPHAREF,73}, {D3DRS_CLIPPING,FALSE},
        {D3DRS_DITHERENABLE,TRUE}, {D3DRS_RANGEFOGENABLE,TRUE}, {D3DRS_CLIPPLANEENABLE,1},
        {D3DRS_MULTISAMPLEMASK,0x55}, {D3DRS_FOGCOLOR,0xff123456},
        {D3DRS_FOGSTART,0x41480000}, {D3DRS_FOGEND,0x44800000},
        {D3DRS_FOGTABLEMODE,D3DFOG_LINEAR}, {D3DRS_FOGVERTEXMODE,D3DFOG_EXP2}};
    for (const auto& state : expected)
    {
        DWORD actual = 0;
        if (FAILED(device->GetRenderState(state.first,&actual)) || actual != state.second) return false;
    }
    DWORD cull = 0, z = 0, compare = 0, source = 0;
    device->GetRenderState(D3DRS_CULLMODE,&cull); device->GetRenderState(D3DRS_ZWRITEENABLE,&z);
    device->GetRenderState(D3DRS_ZFUNC,&compare); device->GetRenderState(D3DRS_SRCBLEND,&source);
    if (cull != D3DCULL_NONE || z != FALSE || compare != D3DCMP_GREATER || source != D3DBLEND_SRCALPHA) return false;
    if (!graphics.SetPipelineField(&RendererBackend::PipelineState::depthWrite,true)) return false;
    device->GetRenderState(D3DRS_CULLMODE,&cull);
    if (cull != D3DCULL_NONE || !graphics.BindRenderTarget(originalDepth.get(),true) || !saved->Restore()) return false;
    device->GetRenderState(D3DRS_CULLMODE,&cull); device->GetRenderState(D3DRS_ZWRITEENABLE,&z);
    if (cull != originalCull || z != originalZ) return false;
    std::puts("Shared target creation, binding, copy/readback, and pipeline state restoration passed.");
    using RendererBackend::ShaderStage;
    const char pixelSource[] = "float4 main() : COLOR0 { return float4(1,0,0,1); }";
    const char vertexSource[] = "float4 main(float4 p : POSITION) : POSITION { return p; }";
    std::unique_ptr<RendererBackend::Shader> pixel(graphics.CreateShader(
        {ShaderStage::Pixel, pixelSource, sizeof(pixelSource)-1, "test pixel", "main", "ps_2_0"}));
    std::unique_ptr<RendererBackend::Shader> vertex(graphics.CreateShader(
        {ShaderStage::Vertex, vertexSource, sizeof(vertexSource)-1, "test vertex", "main", "vs_2_0"}));
    if (!pixel || !vertex || pixel->Stage() != ShaderStage::Pixel || vertex->Stage() != ShaderStage::Vertex)
        return false;
    if (!graphics.BindShader(ShaderStage::Pixel, pixel.get()) ||
        !graphics.BindShader(ShaderStage::Vertex, vertex.get()) ||
        graphics.BindShader(ShaderStage::Vertex, pixel.get()) ||
        graphics.BindShader(ShaderStage::Pixel, vertex.get())) return false;
    IDirect3DPixelShader9* boundPixel = nullptr;
    IDirect3DVertexShader9* boundVertex = nullptr;
    device->GetPixelShader(&boundPixel);
    device->GetVertexShader(&boundVertex);
    const bool bindingsMatch = boundPixel == dynamic_cast<D3D9Graphics::Shader*>(pixel.get())->pixel &&
        boundVertex == dynamic_cast<D3D9Graphics::Shader*>(vertex.get())->vertex;
    if (boundPixel) boundPixel->Release();
    if (boundVertex) boundVertex->Release();
    if (!bindingsMatch) return false;
    const float constants[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    for (auto stage : {ShaderStage::Vertex, ShaderStage::Pixel})
    {
        if (!graphics.SetShaderConstants(stage, 2, constants, 2)) return false;
        float actual[8] = {};
        const HRESULT result = stage == ShaderStage::Vertex ? device->GetVertexShaderConstantF(2, actual, 2) :
            device->GetPixelShaderConstantF(2, actual, 2);
        if (FAILED(result) || std::memcmp(actual, constants, sizeof(actual))) return false;
        if (!graphics.BindShader(stage, nullptr)) return false;
    }
    device->GetPixelShader(&boundPixel);
    device->GetVertexShader(&boundVertex);
    const bool unbound = !boundPixel && !boundVertex;
    if (boundPixel) boundPixel->Release();
    if (boundVertex) boundVertex->Release();
    if (!unbound) return false;
    std::unique_ptr<RendererBackend::Shader> invalid(graphics.CreateShader(
        {ShaderStage::Pixel, "invalid", 7, "intentional compile failure", "main", "ps_2_0"}));
    if (invalid || graphics.CreateShader({}) || graphics.SetShaderConstants(ShaderStage::Pixel, 0, nullptr, 1))
        return false;
    std::puts("Shared vertex/pixel shader creation, binding, constants, unbinding and failure checks passed.");
    std::array<unsigned char, 256> pixels;
    for (size_t i = 0; i < pixels.size(); ++i) pixels[i] = static_cast<unsigned char>(i);
    for (auto format : {RendererBackend::TextureFormat::Bgra8, RendererBackend::TextureFormat::BC1,
                        RendererBackend::TextureFormat::BC2, RendererBackend::TextureFormat::BC3})
    {
        RendererBackend::TextureUpload upload;
        upload.width = 8; upload.height = 8; upload.format = format;
        const bool rgba = format == RendererBackend::TextureFormat::Bgra8;
        const size_t rowBytes = rgba ? 32 : format == RendererBackend::TextureFormat::BC1 ? 16 : 32;
        const size_t rows = rgba ? 8 : 2;
        upload.data = pixels.data(); upload.dataSize = rowBytes * rows;
        upload.generateMips = false;
        std::unique_ptr<RendererBackend::Texture> texture(graphics.CreateTexture(upload));
        if (!texture) return false;
        auto* native = D3D9Graphics::NativeTexture(texture.get());
        if (!graphics.BindTexture(0, texture.get())) return false;
        IDirect3DBaseTexture9* bound = nullptr;
        device->GetTexture(0, &bound);
        const bool correctBinding = bound == native;
        if (bound) bound->Release();
        if (!correctBinding || !graphics.BindTexture(0, nullptr)) return false;
        D3DLOCKED_RECT lock = {};
        if (!native || native->GetLevelCount() != 1 || FAILED(native->LockRect(0, &lock, nullptr, D3DLOCK_READONLY))) return false;
        bool same = true;
        for (size_t row = 0; row < rows; ++row)
            same &= std::memcmp(static_cast<unsigned char*>(lock.pBits) + row * lock.Pitch,
                                pixels.data() + row * rowBytes, rowBytes) == 0;
        native->UnlockRect(0);
        if (!same) return false;
        --upload.dataSize;
        std::unique_ptr<RendererBackend::Texture> truncated(graphics.CreateTexture(upload));
        if (truncated) return false;
    }
    for (auto kind : {RendererBackend::BufferKind::Vertex, RendererBackend::BufferKind::Index32})
    {
        std::unique_ptr<RendererBackend::MeshBuffer> buffer(graphics.CreateMeshBuffer(kind, 128));
        if (!buffer) return false;
        void* data = nullptr;
        if (buffer->Map(127, 2, &data) || buffer->Unmap()) return false;
        if (!buffer->Map(16, 32, &data) || !data) return false;
        std::memcpy(data, pixels.data(), 32);
        void* second = nullptr;
        if (buffer->Map(0, 0, &second) || !buffer->Unmap()) return false;
        if (!buffer->Map(0, 0, &data) || !buffer->Unmap()) return false;
    }
    if (graphics.DrawIndexed({}) || graphics.DrawImmediate(RendererBackend::Primitive::TriangleList, 1, nullptr, 36)) return false;
    std::puts("BGRA/BC1/BC2/BC3 uploads, truncation rejection, and buffer map checks passed.");
    return true;
}

static_assert(std::is_base_of_v<RendererBackend::Interface, D3D9Device::Runtime>);
static_assert(!std::is_abstract_v<D3D9Device::Runtime>);

static bool TestPostProcessing(HWND window)
{
    for (int samples : {0, 4})
    for (int mode : {0, 1, 2})
    {
        D3D9Device::Runtime renderer;
        RendererBackend::DisplayConfiguration display;
        display.width = display.renderWidth = 64;
        display.height = display.renderHeight = 64;
        display.postProcessAntiAliasingMode = mode;
        display.antiAliasingSamples = samples;
        if (renderer.Initialize(window, display) != RendererBackend::InitializeResult::Success) return false;
        for (int width : {64, 96})
        {
            if (width != 64 && !renderer.Resize(width, 64)) return false;
            if (renderer.PrepareFrame() != RendererBackend::FrameStatus::Ready) return false;
            auto* device = renderer.Device();
            IDirect3DSurface9* target = nullptr;
            if (FAILED(device->GetRenderTarget(0, &target))) return false;
            D3DSURFACE_DESC desc = {};
            target->GetDesc(&desc);
            target->Release();
            // Broad solid regions check sampling/orientation without depending on
            // driver-specific edge filtering. Resolve must not produce a blank frame.
            if (!renderer.GetGraphics().Clear({true,false,false,0xffff0000,0,1}) ||
                !renderer.GetGraphics().BeginScene() || !renderer.GetGraphics().EndScene()) return false;
            D3DRECT right = {LONG(desc.Width/2), 0, LONG(desc.Width), LONG(desc.Height)};
            device->Clear(1, &right, D3DCLEAR_TARGET, 0xff0000ff, 1, 0);
            device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
            device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
            if (FAILED(renderer.ResolveFrame())) return false;
            DWORD cull = 0, blend = 0;
            device->GetRenderState(D3DRS_CULLMODE, &cull);
            device->GetRenderState(D3DRS_ALPHABLENDENABLE, &blend);
            if (cull != D3DCULL_CW || blend != TRUE) return false;
            if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &target))) return false;
            target->GetDesc(&desc);
            IDirect3DSurface9* readback = nullptr;
            HRESULT result = device->CreateOffscreenPlainSurface(desc.Width, desc.Height,
                desc.Format, D3DPOOL_SYSTEMMEM, &readback, nullptr);
            if (SUCCEEDED(result)) result = device->GetRenderTargetData(target, readback);
            target->Release();
            if (FAILED(result)) { if (readback) readback->Release(); return false; }
            D3DLOCKED_RECT lock = {};
            result = readback->LockRect(&lock, nullptr, D3DLOCK_READONLY);
            bool correct = false;
            if (SUCCEEDED(result))
            {
                const auto* row = reinterpret_cast<const DWORD*>(
                    static_cast<const char*>(lock.pBits) + (desc.Height/2)*lock.Pitch);
                correct = (row[desc.Width/4] & 0xffffff) == 0xff0000 &&
                    (row[desc.Width*3/4] & 0xffffff) == 0x0000ff;
                readback->UnlockRect();
            }
            readback->Release();
            if (!correct) return false;
            {
                std::unique_ptr<RendererBackend::OverlayContext> overlay(renderer.AcquireOverlay());
                if (!overlay || overlay->Width() != unsigned(width) || overlay->Height() != 64 ||
                    SetPixel(overlay->DeviceContext(), 0, 0, RGB(0,255,0)) == CLR_INVALID) return false;
            }
            {
                std::unique_ptr<RendererBackend::RenderTarget> backbuffer(renderer.GetGraphics().CaptureBackBuffer());
                RendererBackend::TargetReadMapping mapping;
                if (!backbuffer || !backbuffer->MapRead(mapping)) return false;
                const bool overlayVisible = (*static_cast<const DWORD*>(mapping.pixels) & 0xffffff) == 0x00ff00;
                RendererBackend::TargetReadMapping duplicate;
                if (backbuffer->MapRead(duplicate) || !backbuffer->UnmapRead() || !overlayVisible) return false;
                if (backbuffer->UnmapRead()) return false;
            }
            if (FAILED(renderer.Present())) return false;
        }
    }
    std::puts("No-AA/MSAA/FXAA/SMAA clear, scene, output, GDI overlay/read mapping, presentation, state restoration and resize checks passed.");
    return true;
}

int main()
{
    RendererBackend::Runtime renderer;
    const auto idle = [&]() {
        return !renderer.IsInitialized() && !renderer.IsDeviceLost() &&
            renderer.D3D9Device() == nullptr &&
            renderer.PrepareFrame() == RendererBackend::FrameStatus::Uninitialized &&
            FAILED(renderer.ResolveFrame()) && FAILED(renderer.Present());
    };
    if (!idle()) return 1;
    // Unsupported choices must never initialize the sole D3D9 implementation.
    for (int backend = 0; backend < ApplicationSettings::RenderingBackendOptionCount(); ++backend)
    {
        if (ApplicationSettings::RenderingBackendIsAvailable(backend)) continue;
        if (renderer.Initialize(nullptr, backend, {}) !=
            RendererBackend::InitializeResult::UnsupportedBackend || !idle()) return 2;
    }
    renderer.Shutdown();
    renderer.Shutdown();
    if (!idle()) return 3;
    if (renderer.GetGraphics().CreateMeshBuffer(RendererBackend::BufferKind::Vertex, 128)) return 4;
    HWND window = CreateWindowExA(0, "STATIC", "Renderer resource tests", WS_POPUP,
        0, 0, 64, 64, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    IDirect3DDevice9* device = nullptr;
    D3DPRESENT_PARAMETERS parameters = {};
    parameters.Windowed = TRUE; parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    parameters.BackBufferWidth = 64; parameters.BackBufferHeight = 64;
    if (window && d3d) d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &parameters, &device);
    const bool resourcesPassed = device && TestResources(device);
    if (device) device->Release();
    if (d3d) d3d->Release();
    const bool postProcessingPassed = window && TestPostProcessing(window);
    if (window) DestroyWindow(window);
    if (!postProcessingPassed) { std::puts("Postprocessing checks failed."); return 6; }
    if (!resourcesPassed) { std::puts("Resource checks failed (or D3D9 HAL unavailable)."); return 5; }
    std::puts("Renderer lifecycle and unsupported-backend checks passed.");
    return 0;
}
