// Included by model_renderer.cpp so existing renderer test targets exercise this pass.
namespace DirectionalShadowMap
{
constexpr UINT Resolution = 2048;
RendererBackend::RenderTarget* map = nullptr;
RendererBackend::RenderTarget* depth = nullptr;
RendererBackend::RenderTarget* receiverDepth = nullptr;
D3DSURFACE_DESC receiverDepthDescription = {};
RendererBackend::Shader* vertexShader = nullptr;
RendererBackend::Shader* depthShader = nullptr;
RendererBackend::Shader* receiverDepthShader = nullptr;
RendererBackend::Shader* receiverShader = nullptr;
bool active = false;
bool recording = false;
bool attempted = false;
struct Submission
{
    Context context; noesisModel_t* model; D3DMATRIX world;
    bool casterOnly = false;
    // NPC instances may share buffers that are animated again before this pass.
    std::vector<std::vector<FFXIVertex>> actorVertices;
};
std::vector<Submission> submissions;

const char Shader[] = R"hlsl(
row_major float4x4 Camera : register(c0);
row_major float4x4 Light : register(c4);
row_major float4x4 World : register(c8);
row_major float4x4 View : register(c12);
float4 PassParams : register(c16); // x: shadow depth pass
struct Input { float4 position:POSITION; float3 normal:NORMAL; float4 color:COLOR0; float2 uv:TEXCOORD0; };
struct Output { float4 position:POSITION; float4 light:TEXCOORD0; float2 uv:TEXCOORD1; float3 normal:TEXCOORD2; float alpha:TEXCOORD3; float distance:TEXCOORD4; };
Output vs(Input v) {
 Output o; o.light=mul(v.position,Light);
 // Match the scene's view/projection sequence instead of precombining WVP.
 o.position=PassParams.x>0 ? o.light : mul(mul(v.position,View),Camera);
 o.uv=v.uv; o.normal=mul(float4(v.normal,0),World).xyz;
 o.alpha=saturate(v.color.a*2); o.distance=mul(v.position,View).z; return o;
}
sampler2D Base:register(s0);
sampler2D Shadow:register(s1);
float4 Params:register(c0); // has texture, cutout threshold, opacity, inverse map size
float4 Direction:register(c1);
float4 Fog:register(c2); // near, inverse span, enabled
float4 ReceiverDepthParams:register(c3); // inverse camera far distance
void Cutout(Output i) { if(Params.x>0) clip(saturate(tex2D(Base,i.uv).a*Params.x)*i.alpha>Params.y ? 1 : -1); }
float4 depthPS(Output i):COLOR0 { Cutout(i); return i.light.z/i.light.w; }
float4 receiverDepthPS(Output i, out float depth:DEPTH):COLOR0 {
 Cutout(i); depth=saturate(i.distance*ReceiverDepthParams.x); return 0;
}
float CompareBilinear(float2 uv, float2 receiverUV, float z, float2 gradient) {
 float2 grid=uv/Params.w-.5;
 float2 base=(floor(grid)+.5)*Params.w;
 float2 blend=frac(grid);
 // Compare against the receiver plane at each sample's location. Using one
 // depth for every tap creates stripes on surfaces nearly parallel to the light.
 float a=z+dot(gradient,base-receiverUV)>tex2D(Shadow,base).r ? 1:0;
 float b=z+dot(gradient,base+float2(Params.w,0)-receiverUV)>tex2D(Shadow,base+float2(Params.w,0)).r ? 1:0;
 float c=z+dot(gradient,base+float2(0,Params.w)-receiverUV)>tex2D(Shadow,base+float2(0,Params.w)).r ? 1:0;
 float d=z+dot(gradient,base+Params.ww-receiverUV)>tex2D(Shadow,base+Params.ww).r ? 1:0;
 return lerp(lerp(a,b,blend.x),lerp(c,d,blend.x),blend.y);
}
float4 receiverPS(Output i, out float depth:DEPTH):COLOR0 {
 Cutout(i);
 depth=saturate(i.distance*ReceiverDepthParams.x);
 float3 p=i.light.xyz/i.light.w;
 float2 uv=p.xy*float2(.5,-.5)+.5;
 float3 dx=ddx(float3(uv,p.z)), dy=ddy(float3(uv,p.z));
 // Condition the UV derivatives before inversion. Their raw determinant shrinks
 // with viewport pixel size; an absolute cutoff disabled correction at 1080p+
 // and caused unobstructed surfaces to shadow themselves during camera motion.
 dx/=max(max(abs(dx.x),abs(dx.y)),1e-20);
 dy/=max(max(abs(dy.x),abs(dy.y)),1e-20);
 float determinant=dx.x*dy.y-dx.y*dy.x;
 float2 gradient=0;
 if(abs(determinant)>1e-5)
  gradient=float2(dx.z*dy.y-dy.z*dx.y,dx.x*dy.z-dy.x*dx.z)/determinant;
 // Outside the map is unshadowed; fade its edge instead of exposing a hard box.
 float edge=saturate(min(min(uv.x,uv.y),min(1-uv.x,1-uv.y))*20);
 float bias=.00035+.001*(1-abs(dot(normalize(i.normal),Direction.xyz)));
 // Smooth comparisons, rather than abruptly switching whole point-sampled taps.
 float occlusion=0;
 [unroll] for(int y=0;y<2;++y) [unroll] for(int x=0;x<2;++x)
  occlusion += CompareBilinear(uv+(float2(x,y)-.5)*1.5*Params.w,uv,p.z-bias,gradient);
 float valid=(p.z>0 && p.z<1)?1:0;
 float fog=Fog.z*saturate((i.distance-Fog.x)*Fog.y);
 float shade=1-Params.z*edge*valid*occlusion/4*(1-fog);
 return float4(shade,shade,shade,1);
}
)hlsl";

bool Compile(IDirect3DDevice9* device, const char* entry, const char* profile, bool vertex)
{
    auto*& shader = vertex ? vertexShader : strcmp(entry, "depthPS") == 0 ? depthShader :
        strcmp(entry, "receiverDepthPS") == 0 ? receiverDepthShader : receiverShader;
    shader = D3D9Graphics::Context(device).CreateShader({
        vertex ? RendererBackend::ShaderStage::Vertex : RendererBackend::ShaderStage::Pixel,
        Shader, sizeof(Shader)-1, "DirectionalShadow", entry, profile});
    return shader != nullptr;
}

bool Ensure(IDirect3DDevice9* device)
{
    if (attempted) return map && depth && vertexShader && depthShader && receiverDepthShader && receiverShader;
    attempted = true;
    D3DCAPS9 caps = {}; device->GetDeviceCaps(&caps);
    if (caps.VertexShaderVersion < D3DVS_VERSION(3,0) || caps.PixelShaderVersion < D3DPS_VERSION(3,0) ||
        !(caps.StencilCaps & D3DSTENCILCAPS_INVERT)) return false;
    const bool ok = (map = D3D9Graphics::Context(device).CreateRenderTarget(
        {Resolution, Resolution, RendererBackend::TargetFormat::R32Float, 0, 0, true})) &&
        (depth = D3D9Graphics::Context(device).CreateRenderTarget(
        {Resolution, Resolution, RendererBackend::TargetFormat::Depth24Stencil8})) &&
        Compile(device,"vs","vs_3_0",true) && Compile(device,"depthPS","ps_3_0",false) &&
        Compile(device,"receiverDepthPS","ps_3_0",false) &&
        Compile(device,"receiverPS","ps_3_0",false);
    if (!ok) OutputDebugStringA("Directional shadow maps unavailable; retaining planar shadows.\n");
    return ok;
}

bool EnsureReceiverDepth(IDirect3DDevice9* device, const D3DSURFACE_DESC& description)
{
    if (receiverDepth && receiverDepthDescription.Width == description.Width &&
        receiverDepthDescription.Height == description.Height &&
        receiverDepthDescription.MultiSampleType == description.MultiSampleType &&
        receiverDepthDescription.MultiSampleQuality == description.MultiSampleQuality) return true;
    if (receiverDepth) { delete receiverDepth; receiverDepth = nullptr; }
    receiverDepth = D3D9Graphics::Context(device).CreateRenderTarget(
        {description.Width, description.Height, RendererBackend::TargetFormat::Depth24Stencil8,
         uint32_t(description.MultiSampleType), description.MultiSampleQuality});
    if (!receiverDepth) return false;
    receiverDepthDescription=description;
    return true;
}

void Submit(const Context& context, noesisModel_t* model, const D3DMATRIX& world,
            bool casterOnly=false)
{
    if (!recording || context.geometryPass == GeometryPass::Transparent) return;
    if (context.rendersZoneObjects && submissions.empty())
        active = context.lightingQuality == 2 && context.dynamicObjectShadows && context.shadowMaxDistance > 0 &&
            !context.indoorZone && Ensure(context.device);
    if (active)
    {
        Submission item={context,model,world,casterOnly,{}};
        if (!context.rendersZoneObjects)
            for (const auto& mesh:model->submeshes) item.actorVertices.push_back(mesh.cpuVerts);
        submissions.push_back(std::move(item));
    }
}

void DrawMeshes(const Submission& item, const D3DMATRIX& camera, const D3DMATRIX& light,
                const float direction[4], bool cast, bool receiverDepthOnly=false)
{
    if (item.casterOnly && !cast) return;
    auto* device = item.context.device;
    auto visibility = item.context.visibility;
    // Offscreen geometry can cast onto visible terrain. Keep explicit hiding and LOD.
    if (cast) { visibility.frustum = nullptr; visibility.visibleMapObjects = nullptr; }
    for (size_t index=0;index<item.model->submeshes.size();++index)
    {
        const auto& mesh=item.model->submeshes[index];
        if (mesh.environmentObject || mesh.animatedWater || mesh.softBlend ||
            !D3DModelBuffers::HasDrawBuffers(item.model,mesh) ||
            !ZoneObjectVisibility::PassesRenderVisibility(item.model,mesh,
                item.context.rendersZoneObjects,visibility)) continue;
        if (cast && !item.context.rendersZoneObjects && !item.context.dynamicActorShadows) continue;
        D3DMATRIX world = item.world;
        if (item.context.rendersZoneObjects && visibility.overrides && !visibility.overrides->empty())
        {
            ZoneObjectTransform::ApplyWorldTransform(device,mesh.objectName,true,*visibility.overrides,item.world);
            D3D9Graphics::Context(device).QueryTransform(RendererBackend::Transform::World, &world._11);
        }
        const auto lightMatrix = D3DMath::Multiply(world,light);
        if (mesh.hasBounds && !receiverDepthOnly)
        {
            unsigned outside = 0x3f;
            for (int corner=0;corner<8;++corner)
            {
                const float x=(corner&1)?mesh.boundsMax[0]:mesh.boundsMin[0];
                const float y=(corner&2)?mesh.boundsMax[1]:mesh.boundsMin[1];
                const float z=(corner&4)?mesh.boundsMax[2]:mesh.boundsMin[2];
                const float lx=x*lightMatrix._11+y*lightMatrix._21+z*lightMatrix._31+lightMatrix._41;
                const float ly=x*lightMatrix._12+y*lightMatrix._22+z*lightMatrix._32+lightMatrix._42;
                const float lz=x*lightMatrix._13+y*lightMatrix._23+z*lightMatrix._33+lightMatrix._43;
                outside &= (lx < -1 ? 1u:0u) | (lx > 1 ? 2u:0u) | (ly < -1 ? 4u:0u) |
                    (ly > 1 ? 8u:0u) | (lz < 0 ? 16u:0u) | (lz > 1 ? 32u:0u);
            }
            if (outside) continue;
        }
        D3D9Graphics::Context(device).SetShaderConstants(RendererBackend::ShaderStage::Vertex, 0,&camera._11,4);
        D3D9Graphics::Context(device).SetShaderConstants(RendererBackend::ShaderStage::Vertex, 4,&lightMatrix._11,4);
        D3D9Graphics::Context(device).SetShaderConstants(RendererBackend::ShaderStage::Vertex, 8,&world._11,4);
        D3DMATRIX view = {}; D3D9Graphics::Context(device).QueryTransform(RendererBackend::Transform::View, &view._11);
        const auto viewMatrix=D3DMath::Multiply(world,view);
        D3D9Graphics::Context(device).SetShaderConstants(RendererBackend::ShaderStage::Vertex, 12,&viewMatrix._11,4);
        const float pass[4]={cast ? 1.0f:0.0f,0,0,0};
        D3D9Graphics::Context(device).SetShaderConstants(RendererBackend::ShaderStage::Vertex, 16,pass,1);
        const auto* material = mesh.pResolvedMaterial;
        const bool cutout = material && material->alphaTest > 0;
        const float params[4] = {cutout && mesh.pResolvedTexture ?
            (mesh.pResolvedTexture->texType == NOESISTEX_DXT3 ? 2.0f : 1.0f) : 0.0f,
            cutout ? floorf(material->alphaTest*255)/255 : 0.0f,
            std::clamp(item.context.shadowOpacity,0.0f,1.0f),1.0f/Resolution};
        D3D9Graphics::Context(device).SetShaderConstants(RendererBackend::ShaderStage::Pixel, 0,params,1);
        D3D9Graphics::Context(device).SetShaderConstants(RendererBackend::ShaderStage::Pixel, 1,direction,1);
        const bool fogEnabled=item.context.useAuthoredFog &&
            std::isfinite(item.context.authoredFogNear) &&
            std::isfinite(item.context.authoredFogFar) && item.context.authoredFogFar > 0.0f &&
            item.context.authoredFogFar > item.context.authoredFogNear;
        const float fog[4]={item.context.authoredFogNear,
            fogEnabled ? 1/(item.context.authoredFogFar-item.context.authoredFogNear) : 0.0f,
            fogEnabled ? 1.0f : 0.0f,0};
        D3D9Graphics::Context(device).SetShaderConstants(RendererBackend::ShaderStage::Pixel, 2,fog,1);
        // Linear receiver depth resolves small layer separations even far from
        // the camera. Both receiver passes use exactly the same expression.
        const float farDistance=camera._34 != 0 && camera._33 != 1 ?
            camera._43/(1-camera._33) : 1000.0f;
        const float receiverDepthParams[4]={1/std::max(farDistance,1.0f),0,0,0};
        D3D9Graphics::Context(device).SetShaderConstants(RendererBackend::ShaderStage::Pixel, 3,receiverDepthParams,1);
        D3D9Graphics::Context(device).BindTexture(0,mesh.pResolvedTexture ? mesh.pResolvedTexture->gpuTexture : nullptr);
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::cull, material && (material->flags & NMATFLAG_TWOSIDED) ? RendererBackend::Cull::None : RendererBackend::Cull::Clockwise);
        if (index<item.actorVertices.size() && !item.actorVertices[index].empty() && !mesh.cpuIndices.empty())
            D3D9Graphics::Context(device).DrawIndexedImmediate(
                static_cast<UINT>(item.actorVertices[index].size()),static_cast<UINT>(mesh.triCount),
                mesh.cpuIndices.data(),item.actorVertices[index].data(),sizeof(FFXIVertex));
        else D3DModelBuffers::DrawSubmesh(device,item.model,mesh);
    }
}

}

void BeginDirectionalShadowFrame()
{
    DirectionalShadowMap::submissions.clear();
    DirectionalShadowMap::active = false;
    DirectionalShadowMap::recording = true;
}

bool IsDirectionalShadowFrameActive()
{
    return DirectionalShadowMap::active;
}

void SubmitDirectionalShadowCaster(const Context& context, noesisModel_t* model,
                                   const D3DMATRIX& world)
{
    if (model && context.device && context.dynamicActorShadows)
        DirectionalShadowMap::Submit(context,model,world,true);
}

void ReleaseDirectionalShadowResources()
{
    using namespace DirectionalShadowMap;
    if (map) { delete map; map = nullptr; }
    if (depth) { delete depth; depth = nullptr; }
    if (receiverDepth) { delete receiverDepth; receiverDepth = nullptr; }
    if (vertexShader) { delete vertexShader; vertexShader = nullptr; }
    if (depthShader) { delete depthShader; depthShader = nullptr; }
    if (receiverDepthShader) { delete receiverDepthShader; receiverDepthShader = nullptr; }
    if (receiverShader) { delete receiverShader; receiverShader = nullptr; }
    submissions.clear(); active = recording = attempted = false;
}

bool DrawDirectionalShadowFrame()
{
    using namespace DirectionalShadowMap;
    recording = false;
    if (!active || submissions.empty()) { active = false; submissions.clear(); return false; }
    const Context context = submissions.front().context;
    auto* device = context.device;
    float direction[4] = {};
    if (context.automaticLightDirection && context.useAuthoredLightDirection)
        memcpy(direction,context.authoredMainLightDirection,3*sizeof(float));
    else D3DModelRenderState::CalculateDirectionalLight(context.minuteOfDay,
        context.automaticLightDirection,context.lightAzimuthDegrees,context.lightElevationDegrees,direction);
    const float length = sqrtf(direction[0]*direction[0]+direction[1]*direction[1]+direction[2]*direction[2]);
    if (!std::isfinite(length) || length < .001f) { submissions.clear(); active = false; return false; }
    for (int i=0;i<3;++i) direction[i] /= length;
    if (context.automaticLightDirection && direction[1]>0)
        for (int i=0;i<3;++i) direction[i] = -direction[i];
    const float extent = std::clamp(context.shadowMaxDistance,16.0f,160.0f);
    const float* center = context.shadowCenterValid ? context.shadowCenter : context.cameraPosition;
    // Build the axes at the origin; subtracting two large world positions loses
    // precision and subtly changes the light's orientation when the camera moves.
    auto view = D3DMath::BuildLookAtLH(-direction[0]*200,-direction[1]*200,
        -direction[2]*200,0,0,0);
    view._41-=center[0]*view._11+center[1]*view._21+center[2]*view._31;
    view._42-=center[0]*view._12+center[1]*view._22+center[2]*view._32;
    view._43-=center[0]*view._13+center[1]*view._23+center[2]*view._33;
    // Snap the light-space translation to texels to reduce camera-motion shimmer.
    const float texel = 2*extent/Resolution;
    view._41 = floorf(view._41/texel)*texel;
    view._42 = floorf(view._42/texel)*texel;
    auto projection = D3DMath::BuildIdentity();
    projection._11 = projection._22 = 1/extent; projection._33 = 1/400.0f;
    const auto light = D3DMath::Multiply(view,projection);
    RendererBackend::PipelineSnapshot* state = nullptr;
    RendererBackend::RenderTarget *target = nullptr, *oldDepth = nullptr;
    RendererBackend::Viewport viewport = {};
    D3DMATRIX cameraView = {}, cameraProjection = {};
    D3D9Graphics::Context(device).QueryTransform(RendererBackend::Transform::View, &cameraView._11); D3D9Graphics::Context(device).QueryTransform(RendererBackend::Transform::Projection, &cameraProjection._11);
    D3D9Graphics::Context(device).QueryViewport(viewport);
    if (!(state = D3D9Graphics::Context(device).CapturePipelineState()) ||
        !(target = D3D9Graphics::Context(device).CaptureRenderTarget()) ||
        !(oldDepth = D3D9Graphics::Context(device).CaptureRenderTarget(true)))
    {
        delete state; delete target; delete oldDepth;
        submissions.clear(); active=false; return false;
    }
    D3D9Graphics::Context(device).BindTexture(1, nullptr);
    // Unbind the backbuffer's multisampled depth surface before attaching a non-MSAA target.
    D3D9Graphics::Context(device).BindRenderTarget(nullptr, true);
    bool ok = D3D9Graphics::Context(device).BindRenderTarget(map) && D3D9Graphics::Context(device).BindRenderTarget(depth, true);
    if (ok)
    {
        RendererBackend::Viewport shadowViewport = {0,0,Resolution,Resolution,0,1};
        D3D9Graphics::Context(device).SetViewport(shadowViewport);
        D3D9Graphics::Context(device).Clear({true,true,false,0xffffffff,0,1});
        D3D9Graphics::Context(device).BindBuiltinVertexLayout(RendererBackend::BuiltinVertexLayout::Model); D3D9Graphics::Context(device).BindShader(RendererBackend::ShaderStage::Vertex, vertexShader); D3D9Graphics::Context(device).BindShader(RendererBackend::ShaderStage::Pixel, depthShader);
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthTest, true); D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthWrite, true);
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthCompare, RendererBackend::Compare::LessEqual);
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::blend, false); D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::alphaTest, false);
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::fog, false); D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::scissor, false);
        D3D9Graphics::Context(device).SetStencilField(&RendererBackend::StencilState::enabled, false); D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::fill, RendererBackend::Fill::Solid);
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::colorWriteMask, uint32_t(15));
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::srgbWrite, false);
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthBias, RendererBackend::FloatFromBits(0)); D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::slopeDepthBias, RendererBackend::FloatFromBits(0));
        for(const auto& item:submissions) DrawMeshes(item,light,light,direction,true);
    }
    D3D9Graphics::Context(device).BindRenderTarget(nullptr, true); D3D9Graphics::Context(device).BindRenderTarget(target);
    D3D9Graphics::Context(device).BindRenderTarget(oldDepth, true); D3D9Graphics::Context(device).SetViewport(viewport);
    D3DSURFACE_DESC sceneDepthDescription = {};
    D3D9Graphics::NativeSurface(oldDepth)->GetDesc(&sceneDepthDescription);
    if (ok) ok=EnsureReceiverDepth(device,sceneDepthDescription) &&
        D3D9Graphics::Context(device).BindRenderTarget(receiverDepth, true);
    if (ok)
    {
        D3D9Graphics::Context(device).BindBuiltinVertexLayout(RendererBackend::BuiltinVertexLayout::Model); D3D9Graphics::Context(device).BindShader(RendererBackend::ShaderStage::Vertex, vertexShader);
        // Select the actual frontmost opaque surface with unbiased linear depth.
        // Replaying against biased scene depth lets hidden detail layers claim
        // stencil coverage and apply their shadow to the visible surface above.
        D3D9Graphics::Context(device).Clear({false,true,true,0,0,1});
        D3D9Graphics::Context(device).BindShader(RendererBackend::ShaderStage::Pixel, receiverDepthShader);
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::colorWriteMask, uint32_t(0));
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthWrite, true);
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthCompare, RendererBackend::Compare::Less);
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthBias, RendererBackend::FloatFromBits(0)); D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::slopeDepthBias, RendererBackend::FloatFromBits(0));
        D3D9Graphics::Context(device).SetStencilField(&RendererBackend::StencilState::enabled, false);
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::blend, false);
        for(const auto& item:submissions) DrawMeshes(item,cameraProjection,light,direction,false,true);
        D3D9Graphics::Context(device).BindTexture(1,map); D3D9Graphics::Context(device).BindShader(RendererBackend::ShaderStage::Pixel, receiverShader);
        D3D9Graphics::Context(device).SetSamplerField(1, &RendererBackend::SamplerState::minFilter, RendererBackend::TextureFilter::Point);
        D3D9Graphics::Context(device).SetSamplerField(1, &RendererBackend::SamplerState::magFilter, RendererBackend::TextureFilter::Point);
        D3D9Graphics::Context(device).SetSamplerField(1, &RendererBackend::SamplerState::mipFilter, RendererBackend::TextureFilter::None);
        D3D9Graphics::Context(device).SetSamplerField(1, &RendererBackend::SamplerState::addressU, RendererBackend::TextureAddress::Clamp);
        D3D9Graphics::Context(device).SetSamplerField(1, &RendererBackend::SamplerState::addressV, RendererBackend::TextureAddress::Clamp);
        D3D9Graphics::Context(device).SetSamplerField(1, &RendererBackend::SamplerState::srgb, false);
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::srgbWrite, false);
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthTest, true);
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthWrite, false); D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::depthCompare, RendererBackend::Compare::Equal);
        D3D9Graphics::Context(device).SetStencilField(&RendererBackend::StencilState::twoSided, false);
        D3D9Graphics::Context(device).SetStencilField(&RendererBackend::StencilState::enabled, true);
        D3D9Graphics::Context(device).SetStencilField(&RendererBackend::StencilState::readMask, uint32_t(0x80)); D3D9Graphics::Context(device).SetStencilField(&RendererBackend::StencilState::writeMask, uint32_t(0x80));
        D3D9Graphics::Context(device).SetStencilField(&RendererBackend::StencilState::reference, uint32_t(0));
        D3D9Graphics::Context(device).SetStencilField(&RendererBackend::StencilState::compare, RendererBackend::Compare::Equal);
        D3D9Graphics::Context(device).SetStencilField(&RendererBackend::StencilState::fail, RendererBackend::StencilOperation::Keep);
        D3D9Graphics::Context(device).SetStencilField(&RendererBackend::StencilState::depthFail, RendererBackend::StencilOperation::Keep);
        D3D9Graphics::Context(device).SetStencilField(&RendererBackend::StencilState::pass, RendererBackend::StencilOperation::Invert);
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::blend, true);
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::blendOperation, RendererBackend::BlendOperation::Add);
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::sourceBlend, RendererBackend::BlendFactor::Zero);
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::destinationBlend, RendererBackend::BlendFactor::SourceColor);
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::separateAlphaBlend, false);
        D3D9Graphics::Context(device).SetPipelineField(&RendererBackend::PipelineState::colorWriteMask, uint32_t(7));
        for(const auto& item:submissions) DrawMeshes(item,cameraProjection,light,direction,false);
    }
    D3D9Graphics::Context(device).BindRenderTarget(oldDepth, true);
    state->Restore(); D3D9Graphics::Context(device).SetViewport(viewport);
    delete state; delete target; delete oldDepth;
    submissions.clear(); active = false;
    return ok;
}
