#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <cstring>

namespace RendererBackend
{
inline float FloatFromBits(uint32_t bits)
{ float value; std::memcpy(&value,&bits,sizeof(value)); return value; }
struct ClearState
{
    bool color=true, depth=true, stencil=false;
    uint32_t colorValue=0, stencilValue=0;
    float depthValue=1;
};
struct TargetReadMapping { const void* pixels=nullptr; size_t rowPitch=0; };
enum class TextureFormat { Bgra8, BC1, BC2, BC3 };
enum class BufferKind { Vertex, Index32 };
enum class MapAccess { Write, Read };
enum class Primitive { TriangleList, TriangleStrip, TriangleFan, LineList };
enum class ShaderStage { Vertex, Pixel };
enum class Transform { World, View, Projection };
struct Viewport
{
    uint32_t x=0, y=0, width=0, height=0;
    float minDepth=0, maxDepth=1;
};
struct FogState
{
    bool enabled=false;
    uint32_t color=0;
    float start=0, end=0;
};

enum class VertexFormat { Float1, Float2, Float3, Float4, ColorBgra8 };
enum class VertexSemantic { Position, Normal, Color, TexCoord };
struct VertexElement
{
    uint32_t offset = 0;
    VertexFormat format = VertexFormat::Float3;
    VertexSemantic semantic = VertexSemantic::Position;
    uint32_t semanticIndex = 0;
};
// Single-stream programmable layouts. Stride belongs to draw submission; offsets
// and semantics belong to the layout. Caller owns the resulting resource.
class VertexLayout
{
public:
    virtual ~VertexLayout() = default;
};
// Keep fixed-function processing for the application's existing vertex formats.
enum class BuiltinVertexLayout { Model, PositionColor, ScreenColorTexture };
enum class TextureFilter { None, Point, Linear, Anisotropic };
enum class TextureAddress { Wrap, Mirror, Clamp, Border, MirrorOnce };
struct SamplerState
{
    std::optional<TextureFilter> minFilter, magFilter, mipFilter;
    std::optional<TextureAddress> addressU, addressV, addressW;
    std::optional<bool> srgb;
    std::optional<uint32_t> maxAnisotropy, maxMipLevel, borderColor; // packed ARGB
    std::optional<float> mipLodBias;
};

// Source/profile are supplied by the backend's shader library. They are not a
// promise that one shader language can be compiled by every future backend.
struct ShaderSource
{
    ShaderStage stage = ShaderStage::Pixel;
    const char* source = nullptr;
    size_t length = 0;
    const char* name = nullptr;
    const char* entryPoint = "main";
    const char* profile = nullptr;
};

// Caller-owned, like textures and mesh buffers. Destroy before backend shutdown.
class Shader
{
public:
    virtual ~Shader() = default;
    virtual ShaderStage Stage() const = 0;
};

struct TextureUpload
{
    int width = 0, height = 0;
    TextureFormat format = TextureFormat::Bgra8;
    const void* data = nullptr;
    size_t dataSize = 0;
    bool generateMips = true;
    bool allowMipFallback = true;
};

// Owned by the caller. Destruction releases the backend resource. These objects
// must be destroyed before the renderer shuts down; references are borrowed.
class Texture
{
public:
    virtual ~Texture() = default;
};

enum class TargetFormat { Bgra8, Bgrx8, R32Float, Depth24Stencil8 };
struct RenderTargetDescription
{
    uint32_t width = 0, height = 0;
    TargetFormat format = TargetFormat::Bgra8;
    uint32_t samples = 0, quality = 0;
    bool sampleable = false;
};
// Default-pool resources: release before device reset, recreate afterwards.
// Only sampleable color targets can be bound as textures.
class RenderTarget : public Texture
{
public:
    virtual RenderTargetDescription Description() const = 0;
    // Opportunistic native read mapping; not every target is CPU-readable.
    virtual bool MapRead(TargetReadMapping&) = 0;
    virtual bool UnmapRead() = 0;
};
enum class Compare { Never, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always };
enum class Cull { None, Clockwise, CounterClockwise };
enum class Fill { Point, Wireframe, Solid };
enum class BlendOperation { Add, Subtract, ReverseSubtract, Minimum, Maximum };
enum class FogMode { None, Exponential, ExponentialSquared, Linear };
enum class BlendFactor { Zero, One, SourceColor, InverseSourceColor, SourceAlpha,
    InverseSourceAlpha, DestinationAlpha, InverseDestinationAlpha, DestinationColor, InverseDestinationColor };
// Unspecified fields preserve current state, including legacy fixed-function state.
struct PipelineState
{
    std::optional<bool> depthTest, depthWrite, blend, alphaTest, stencil, scissor, fog,
        srgbWrite, separateAlphaBlend, multisample, clipping, dither, rangeFog;
    std::optional<Compare> depthCompare;
    std::optional<Cull> cull;
    std::optional<BlendFactor> sourceBlend, destinationBlend;
    std::optional<uint32_t> colorWriteMask; // RGBA bits 0..3
    std::optional<float> depthBias, slopeDepthBias;
    std::optional<Compare> alphaCompare;
    std::optional<Fill> fill;
    std::optional<BlendOperation> blendOperation;
    std::optional<FogMode> fogTableMode, fogVertexMode;
    std::optional<uint32_t> alphaReference, clipPlaneMask, multisampleMask, fogColor;
    std::optional<float> fogStart, fogEnd;
};
// Captures pipeline state immediately. Render targets are deliberately separate;
// callers must restore target attachments explicitly before restoring this state.
// Snapshots retain resource references and must be destroyed before device reset.
class PipelineSnapshot
{
public:
    virtual ~PipelineSnapshot() = default;
    virtual bool Restore() = 0;
};

enum class StencilOperation { Keep, Zero, Replace, IncrementSaturate, DecrementSaturate, Invert, Increment, Decrement };
struct StencilState
{
    std::optional<bool> enabled, twoSided;
    std::optional<uint32_t> reference, readMask, writeMask;
    std::optional<Compare> compare, backCompare;
    std::optional<StencilOperation> fail, depthFail, pass, backFail, backDepthFail, backPass;
};
enum class TextureOperation { Disable, SelectArg1, SelectArg2, Modulate, Modulate2X, Modulate4X, Add };
enum class TextureArgument { Diffuse, Current, Texture, Factor, Specular, Temporary, Constant };
enum class TextureTransform { Disabled, Count1, Count2, Count3, Count4 };
struct TextureStageState
{
    std::optional<TextureOperation> colorOperation, alphaOperation;
    std::optional<TextureArgument> colorArg1, colorArg2, alphaArg1, alphaArg2, result;
    std::optional<uint32_t> coordinateIndex;
    std::optional<TextureTransform> transform;
};
enum class MaterialSource { Material, DiffuseVertex, SpecularVertex };
struct LightingState
{
    std::optional<bool> enabled, vertexColor, normalizeNormals, specular, localViewer;
    std::optional<uint32_t> ambient; // packed ARGB
    std::optional<MaterialSource> diffuseSource, ambientSource, specularSource, emissiveSource;
};
struct LightColor { float r=0, g=0, b=0, a=0; };
struct LightVector { float x=0, y=0, z=0; };
enum class LightType { Point, Spot, Directional };
struct Light
{
    LightType type = LightType::Point;
    LightColor diffuse, specular, ambient;
    LightVector position, direction;
    float range=0, falloff=0, attenuation0=0, attenuation1=0, attenuation2=0, theta=0, phi=0;
};

class MeshBuffer
{
public:
    virtual ~MeshBuffer() = default;
    // A zero size maps the remaining bytes. Only one outstanding map is allowed.
    virtual bool Map(size_t offset, size_t size, void** data, MapAccess access = MapAccess::Write) = 0;
    virtual bool Unmap() = 0;
};

struct MeshDraw
{
    MeshBuffer* vertices = nullptr;
    MeshBuffer* indices = nullptr;
    uint32_t stride = 0;
    int32_t baseVertex = 0;
    uint32_t minVertex = 0, vertexCount = 0, startIndex = 0, primitiveCount = 0;
};

// Submission consumes the currently configured material/layout state.
class Graphics
{
public:
    virtual ~Graphics() = default;
    virtual bool BeginScene() = 0;
    virtual bool EndScene() = 0;
    virtual bool Clear(const ClearState&) = 0;
    virtual RenderTarget* CaptureBackBuffer() = 0;
    // Matrices contain 16 row-major floats, unchanged from the existing row-vector
    // math convention. Queries leave outputs untouched on failure.
    virtual bool SetTransform(Transform, const float* matrix16) = 0;
    virtual bool QueryTransform(Transform, float* matrix16) = 0;
    virtual bool QueryTextureTransform(uint32_t stage, float* matrix16) = 0;
    virtual bool SetViewport(const Viewport&) = 0;
    virtual bool QueryViewport(Viewport&) = 0;
    virtual bool QueryFogState(FogState&) = 0;
    virtual bool QueryPipelineState(PipelineState&) = 0;
    virtual bool QuerySamplerState(uint32_t stage, SamplerState&) = 0;
    virtual bool QueryStencilState(StencilState&) = 0;
    virtual bool QueryTextureStageState(uint32_t stage, TextureStageState&) = 0;
    virtual bool QueryLightingState(LightingState&) = 0;
    virtual bool ApplyStencilState(const StencilState&) = 0;
    virtual bool ApplyTextureStageState(uint32_t stage, const TextureStageState&) = 0;
    virtual bool SetTextureTransform(uint32_t stage, const float* rowMajorMatrix16) = 0;
    virtual bool SetTextureFactor(uint32_t argb) = 0;
    virtual bool ApplyLightingState(const LightingState&) = 0;
    virtual bool SetLight(uint32_t index, const Light&) = 0;
    virtual bool EnableLight(uint32_t index, bool enabled) = 0;
    template<class T> bool SetStencilField(std::optional<T> StencilState::* field, T value)
    { StencilState s; s.*field=value; return ApplyStencilState(s); }
    template<class T> bool SetTextureStageField(uint32_t stage, std::optional<T> TextureStageState::* field, T value)
    { TextureStageState s; s.*field=value; return ApplyTextureStageState(stage,s); }
    template<class T> bool SetLightingField(std::optional<T> LightingState::* field, T value)
    { LightingState s; s.*field=value; return ApplyLightingState(s); }
    virtual VertexLayout* CreateVertexLayout(const VertexElement* elements, size_t count) = 0;
    virtual bool BindVertexLayout(VertexLayout* layout) = 0;
    virtual bool BindBuiltinVertexLayout(BuiltinVertexLayout layout) = 0;
    // Unspecified sampler fields preserve current settings. Stage is a pixel
    // sampler slot; vertex shader samplers are outside this initial interface.
    virtual bool ApplySamplerState(uint32_t stage, const SamplerState&) = 0;
    template<class T> bool SetSamplerField(uint32_t stage, std::optional<T> SamplerState::* field, T value)
    {
        SamplerState state;
        state.*field = value;
        return ApplySamplerState(stage, state);
    }
    virtual RenderTarget* CreateRenderTarget(const RenderTargetDescription&) = 0;
    // Acquires an owned reference to the current color/depth attachment.
    virtual RenderTarget* CaptureRenderTarget(bool depth = false) = 0;
    virtual bool BindRenderTarget(RenderTarget* target, bool depth = false) = 0;
    virtual bool CopyRenderTarget(RenderTarget* source, RenderTarget* destination) = 0;
    virtual PipelineSnapshot* CapturePipelineState() = 0;
    virtual bool ApplyPipelineState(const PipelineState&) = 0;
    template<class T> bool SetPipelineField(std::optional<T> PipelineState::* field, T value)
    {
        PipelineState state;
        state.*field = value;
        return ApplyPipelineState(state);
    }
    virtual Shader* CreateShader(const ShaderSource& source) = 0;
    // Null restores fixed-function processing for this stage.
    virtual bool BindShader(ShaderStage stage, Shader* shader) = 0;
    // Constants are contiguous float4 vectors; register mapping belongs to the
    // backend shader library. Does not change the shader or other pipeline state.
    virtual bool SetShaderConstants(ShaderStage stage, uint32_t first,
        const float* values, uint32_t vectorCount) = 0;
    virtual Texture* CreateTexture(const TextureUpload& upload) = 0;
    // Null unbinds the texture. Sampler/filter state is preserved.
    virtual bool BindTexture(uint32_t stage, Texture* texture) = 0;
    // Vertex storage currently uses the FFXI position/normal/color/UV layout;
    // index storage contains unsigned 32-bit values.
    virtual MeshBuffer* CreateMeshBuffer(BufferKind kind, size_t bytes) = 0;
    virtual bool DrawIndexed(const MeshDraw& draw) = 0;
    virtual bool DrawImmediate(Primitive primitive, uint32_t count,
                               const void* vertices, uint32_t stride) = 0;
    virtual bool DrawIndexedImmediate(uint32_t vertexCount, uint32_t primitiveCount,
        const void* indices32, const void* vertices, uint32_t stride) = 0;
};
}
