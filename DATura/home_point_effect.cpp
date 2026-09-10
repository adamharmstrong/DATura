#include "stdafx.h"
#include "home_point_effect.h"
#include "d3d_math.h"
#include "ffxi_model_lifetime.h"
#include "ffxi_resource.h"
#include "home_point_audio.h"
#include "home_point_data.h"
#include "noesis_rapi.h"
#include "model_ff11.h"

namespace HomePoint
{
namespace
{
const char *Shader = R"(
row_major float4x4 World : register(c0);
row_major float4x4 VP : register(c4);
float4 Eye : register(c8);
struct V { float4 p:POSITION;float3 n:NORMAL;float4 color:COLOR0;float2 uv:TEXCOORD0; };
struct P {float4 p:POSITION;float4 color:COLOR0;float2 uv:TEXCOORD0;float3 normal:TEXCOORD1;float3 toEye:TEXCOORD2;};
P vs(V v){P o;float4 w=mul(v.p,World);o.p=mul(w,VP);o.normal=normalize(mul(float4(v.n,0),World).xyz);o.toEye=Eye.xyz-w.xyz;o.uv=v.uv;o.color=v.color;return o;}
sampler2D Base:register(s0);sampler2D Shine:register(s1);
float4 Tint:register(c0);float4 Params:register(c1);float4 Light:register(c2);float4 Specular:register(c3);
float4 Fog:register(c4);float4 FogColor:register(c5);
float4 ps(P p):COLOR0 {
 float4 t=tex2D(Base,p.uv+Params.xy);
 float alpha= t.a * lerp(1,1.875,Params.w);
 float3 color=2*t.rgb*p.color.rgb*Tint.rgb;
 float3 n=normalize(p.normal);float3 eye=normalize(p.toEye);
 float3 reflected=reflect(-eye,n);
 float2 uv=reflected.xy*.5+.5;
 float3 sheen=tex2D(Shine,uv+Params.xy).rgb;
 float highlight=pow(saturate(dot(n,normalize(eye-Light.xyz))),max(1,Light.w));
 color+=Params.z*Specular.rgb*(sheen*.3+highlight);
 float fog=saturate((length(p.toEye)-Fog.x)*Fog.y)*Fog.z;
 color=lerp(color,FogColor.rgb,fog);
 return float4(saturate(color),saturate(alpha*min(1,p.color.a*2)*Tint.a));
})";
float CurveValue(const Data &d, const std::string &name, float age, float fallback)
{
    const auto it = d.curves.find(name);
    return it == d.curves.end() ? fallback : it->second.At(age, fallback);
}
noesisTex_t *Texture(noesisModel_t *model, const std::string &name)
{
    if (!model->pMatData)
        return nullptr;
    for (int i = 0; i < model->pMatData->texCount; ++i)
    {
        auto *t = model->pMatData->textures[i];
        if (t && t->name &&
            (name == t->name || (name.size() <= 4 && std::string(t->name).find(name) != std::string::npos)))
            return t;
    }
    return nullptr;
}
} // namespace
struct Effect::Impl
{
    FFXIModelLifetime::OwnedModel asset;
    Data data;
    Audio audio;
    IDirect3DVertexShader9 *vs = nullptr;
    IDirect3DPixelShader9 *ps = nullptr;
    struct Mesh
    {
        std::vector<FFXIVertex> vertices;
        noesisTex_t *texture = nullptr;
    };
    std::map<std::string, Mesh> meshes;
    ~Impl()
    {
        if (vs)
            vs->Release();
        if (ps)
            ps->Release();
    }
    bool Shaders(IDirect3DDevice9 *device)
    {
        ID3DBlob *bytecode = nullptr;
        ID3DBlob *errors = nullptr;
        HRESULT hr = D3DCompile(Shader, strlen(Shader), "HomePoint", nullptr, nullptr, "vs", "vs_3_0", 0, 0,
                                &bytecode, &errors);
        if (errors)
        {
            OutputDebugStringA((const char *)errors->GetBufferPointer());
            errors->Release();
            errors = nullptr;
        }
        if (FAILED(hr))
            return false;
        hr = device->CreateVertexShader((const DWORD *)bytecode->GetBufferPointer(), &vs);
        bytecode->Release();
        bytecode = nullptr;
        if (FAILED(hr))
            return false;
        hr = D3DCompile(Shader, strlen(Shader), "HomePoint", nullptr, nullptr, "ps", "ps_3_0", 0, 0,
                        &bytecode, &errors);
        if (errors)
        {
            OutputDebugStringA((const char *)errors->GetBufferPointer());
            errors->Release();
        }
        if (FAILED(hr))
            return false;
        hr = device->CreatePixelShader((const DWORD *)bytecode->GetBufferPointer(), &ps);
        bytecode->Release();
        return SUCCEEDED(hr);
    }
};
Effect::Effect() : impl_(std::make_unique<Impl>())
{
}
Effect::~Effect() = default;
bool Effect::Load(IDirect3DDevice9 *device, const char *root, bool compression)
{
    if (!device || !root)
        return false;
    auto next = std::make_unique<Impl>();
    FFXIResource::ResolvedFile path;
    if (!FFXIResource::ResolveFileId(root, 1300 + ModelId, path))
        return false;
    BYTE *raw = nullptr;
    DWORD size = 0;
    if (!FFXIFileIO::ReadWholeFile(path.fullPath.c_str(), &raw, &size))
        return false;
    std::unique_ptr<BYTE[]> bytes(raw);
    std::vector<uint8_t> data(raw, raw + size);
    if (!Parse(data, next->data))
        return false;
    next->asset.Adopt(nullptr, new noeRAPI_t(device));
    auto *rapi = next->asset.ParserContext();
    rapi->SetTextureCompressionEnabled(compression);
    rapi->SetCurrentFilePath(path.fullPath.c_str());
    ff11Opts_t options = {};
    options.renderEffectMeshes = true;
    struct Restore
    {
        ff11Opts_t *previous;
        ~Restore()
        {
            gpFF11Opts = previous;
        }
    } restore{gpFF11Opts};
    gpFF11Opts = &options;
    int count = 0;
    auto *model = Model_FF11_LoadDAT(raw, (int)size, count, rapi);
    if (!model || !count)
        return false;
    next->asset.AttachModel(model);
    for (const auto &sub : model->submeshes)
    {
        const std::string prefix = "effect: ";
        if (sub.objectName.compare(0, prefix.size(), prefix))
            continue;
        std::string resource = sub.objectName.substr(prefix.size());
        while (!resource.empty() && resource.back() == ' ')
            resource.pop_back();
        auto &mesh = next->meshes[resource];
        auto *mat = model->pMatData ? model->pMatData->FindMaterial(sub.materialName.c_str()) : nullptr;
        if (mat && mat->texIdx >= 0 && mat->texIdx < model->pMatData->texCount)
            mesh.texture = model->pMatData->textures[mat->texIdx];
        for (auto index : sub.cpuIndices)
        {
            if (index >= sub.cpuVerts.size())
                return false;
            mesh.vertices.push_back(sub.cpuVerts[index]);
        }
    }
    if (!next->meshes.count("bind") || next->meshes["bind"].vertices.size() != 90 ||
        !next->meshes["bind"].texture || !next->Shaders(device))
        return false;
    const auto ambient = next->data.generators.find("snd0");
    if (ambient != next->data.generators.end())
    {
        const auto a = next->data.sounds.find(ambient->second.resource);
        const auto &events = next->data.schedules["bind"];
        for (const auto &event : events)
            if (event.sound)
            {
                const auto b = next->data.sounds.find(event.resource);
                if (a != next->data.sounds.end() && b != next->data.sounds.end())
                    next->audio.Load(root, a->second, b->second);
                break;
            }
    }
    impl_ = std::move(next);
    return true;
}
void Effect::Draw(IDirect3DDevice9 *device, const D3DMATRIX &world, const float *eye, double seconds,
                  double activatedAt, bool mipMapping)
{
    if (!device || !impl_->asset)
        return;
    auto particles = Evaluate(impl_->data, seconds, activatedAt);
    IDirect3DStateBlock9 *saved = nullptr;
    if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &saved)))
        return;
    saved->Capture();
    D3DMATRIX view, proj;
    device->GetTransform(D3DTS_VIEW, &view);
    device->GetTransform(D3DTS_PROJECTION, &proj);
    auto vp = D3DMath::Multiply(view, proj);
    device->SetVertexShader(impl_->vs);
    device->SetPixelShader(impl_->ps);
    device->SetFVF(FFXI_VERTEX_FVF);
    device->SetVertexShaderConstantF(4, &vp._11, 4);
    float eye4[4] = {eye[0], eye[1], eye[2], 1};
    device->SetVertexShaderConstantF(8, eye4, 1);
    DWORD fogEnabled = 0, fogNear = 0, fogFar = 0, fogColor = 0;
    device->GetRenderState(D3DRS_FOGENABLE, &fogEnabled);
    device->GetRenderState(D3DRS_FOGSTART, &fogNear);
    device->GetRenderState(D3DRS_FOGEND, &fogFar);
    device->GetRenderState(D3DRS_FOGCOLOR, &fogColor);
    float nearValue, farValue;
    memcpy(&nearValue, &fogNear, 4);
    memcpy(&farValue, &fogFar, 4);
    float fog[4] = {nearValue, farValue > nearValue ? 1 / (farValue - nearValue) : 0,
                    (float)(fogEnabled != 0), 0};
    float fogRgb[4] = {((fogColor >> 16) & 255) / 255.f, ((fogColor >> 8) & 255) / 255.f,
                       (fogColor & 255) / 255.f, 1};
    device->SetPixelShaderConstantF(4, fog, 1);
    device->SetPixelShaderConstantF(5, fogRgb, 1);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_ZENABLE, TRUE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    for (int sampler = 0; sampler < 2; ++sampler)
    {
        device->SetSamplerState(sampler, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
        device->SetSamplerState(sampler, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
        device->SetSamplerState(sampler, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        device->SetSamplerState(sampler, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        device->SetSamplerState(sampler, D3DSAMP_MIPFILTER, mipMapping ? D3DTEXF_LINEAR : D3DTEXF_NONE);
    }
    // Sort alpha layers back to front using their authored spawn positions.
    std::stable_sort(particles.begin(), particles.end(), [&](const Particle &a, const Particle &b) {
        auto distance = [&](const Particle &p) {
            float d = 0;
            for (int i = 0; i < 3; ++i)
            {
                float x = world.m[3][i] + p.generator->position[i] - eye[i];
                d += x * x;
            }
            return d;
        };
        return distance(a) > distance(b);
    });
    for (const auto &particle : particles)
    {
        const auto &g = *particle.generator;
        const auto it = impl_->meshes.find(g.resource);
        if (it == impl_->meshes.end())
            continue;
        const auto &mesh = it->second;
        if (!mesh.texture || !mesh.texture->pD3DTex || mesh.vertices.empty())
            continue;
        const float age = particle.age, phase = std::clamp(age / g.life, 0.f, 1.f);
        Vec position = g.position, rotation = g.rotation, scale = g.scale;
        for (int a = 0; a < 3; ++a)
        {
            position[a] += (g.velocity[a] + g.variance[a] * Random(particle.serial, a)) * age;
            rotation[a] += (float)std::fmod((double)g.rotationSpeed[a] * age, 6.283185307179586);
            scale[a] = std::max(0.f, scale[a] + g.scaleSpeed[a] * age +
                                         g.scaleAcceleration[a] * age * (age + 1) * .5f);
            if (!g.scaleCurves[a].empty())
                scale[a] = std::max(0.f, CurveValue(impl_->data, g.scaleCurves[a], phase, scale[a]));
        }
        // Setup slot velocity plus update slot angular acceleration (tama).
        if (g.billboard)
            for (int a = 0; a < 3; ++a)
                rotation[a] += g.acceleration[a] * age;
        auto local = D3DMath::BuildIdentity();
        for (int a = 0; a < 3; ++a)
            local.m[a][a] = scale[a];
        local = D3DMath::Multiply(local, D3DMath::BuildRotationX(rotation[0]));
        local = D3DMath::Multiply(local, D3DMath::BuildRotationY(rotation[1]));
        local = D3DMath::Multiply(local, D3DMath::BuildRotationZ(rotation[2]));
        local._41 = position[0];
        local._42 = position[1];
        local._43 = position[2];
        D3DMATRIX transform = D3DMath::Multiply(local, world);
        if (g.billboard)
        {
            for (int a = 0; a < 3; ++a)
                for (int b = 0; b < 3; ++b)
                    transform.m[a][b] = view.m[b][a] * scale[a];
        }
        device->SetVertexShaderConstantF(0, &transform._11, 4);
        auto color = g.color;
        if (g.alphaAnimated)
            color[3] = CurveValue(impl_->data, g.alphaCurve, phase, 0) * 2;
        if (!g.redCurve.empty())
            color[0] = CurveValue(impl_->data, g.redCurve, phase, color[0] * .5f) * 2;
        if (!g.greenCurve.empty())
            color[1] = CurveValue(impl_->data, g.greenCurve, phase, color[1] * .5f) * 2;
        float params[4] = {(float)std::fmod(age * g.uv[0], 1.f), (float)std::fmod(age * g.uv[1], 1.f),
                           g.specularTexture.empty() ? 0.f : 1.f,
                           mesh.texture->texType == NOESISTEX_DXT3 ? 1.f : 0.f};
        float light[4] = {g.specularDirection[0], g.specularDirection[1], g.specularDirection[2],
                          g.specularPower};
        device->SetPixelShaderConstantF(0, color.data(), 1);
        device->SetPixelShaderConstantF(1, params, 1);
        device->SetPixelShaderConstantF(2, light, 1);
        device->SetPixelShaderConstantF(3, g.specularColor.data(), 1);
        device->SetTexture(0, mesh.texture->pD3DTex);
        auto *shine = Texture(impl_->asset.Model(), g.specularTexture.empty() ? "nami" : g.specularTexture);
        device->SetTexture(1, shine ? shine->pD3DTex : mesh.texture->pD3DTex);
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        device->SetRenderState(D3DRS_DESTBLEND, g.blend == 0x44 ? D3DBLEND_INVSRCALPHA : D3DBLEND_ONE);
        const FFXIVertex *vertices = mesh.vertices.data();
        size_t count = mesh.vertices.size();
        if (g.type == 0x0e && count >= 6)
        {
            size_t frames = count / 6;
            size_t frame = (g.spriteFrame + (g.spriteAnimated ? (int)age : 0)) % frames;
            vertices += frame * 6;
            count = 6;
        }
        device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, (UINT)(count / 3), vertices, sizeof(FFXIVertex));
    }
    saved->Apply();
    saved->Release();
}
void Effect::UpdateSound(const std::vector<Instance> &i, const float *l, const float *r, bool allowed,
                         int max)
{
    impl_->audio.Update(i, l, r, allowed, max);
}
void Effect::ActivateSound(const Instance &i, const float *l, const float *r)
{
    impl_->audio.Activate(i, l, r);
}
void Effect::StopSound()
{
    impl_->audio.Stop();
}
size_t Effect::LayerCount() const
{
    return impl_->meshes.size();
}
size_t Effect::TriangleCount() const
{
    size_t n = 0;
    for (const auto &[name, m] : impl_->meshes)
        n += m.vertices.size() / 3;
    return n;
}
size_t Effect::ActiveParticleCount(double seconds, double activatedAt) const
{
    return Evaluate(impl_->data, seconds, activatedAt).size();
}
bool Effect::HasSounds() const
{
    return impl_->audio.Ready();
}
size_t Effect::SoundVoiceCount() const
{
    return impl_->audio.VoiceCount();
}
} // namespace HomePoint
