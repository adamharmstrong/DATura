# Shared renderer interface

`RendererBackend::Interface` defines the Windows device lifecycle: initialization,
display changes, resize, frame preparation, resolve, presentation, shutdown, and
device status. Shared configuration and result types live in `renderer_interface.h`
without including Direct3D headers.

`RendererBackend::Runtime` owns an implementation through `unique_ptr<Interface>`.
The sole implementation is `D3D9Device::Runtime`. Unsupported selections are still
rejected before initialization; existing settings and menu availability are unchanged.
Reset callbacks retain their existing ordering and lifetime across shutdown and
reinitialization. D3D9 retains native device creation, reset and Present; frame
operations and AA target management use shared graphics interfaces with the same
ordering, formats, sampling and fallback behavior.

`renderer_resources.h` defines API-independent texture uploads (BGRA8, BC1/2/3),
texture binding, owned texture and mesh-buffer interfaces, mapping, indexed mesh
draws, and immediate draws. `GetGraphics()` exposes this service through the runtime.
`D3D9Graphics::Context` is the only production implementation. Managed resources,
linear mip generation with the existing fallback, triangle offsets, and submission
order are preserved. Decoding and optional DXT decompression remain on the CPU.

Model asset structures hold shared-interface resource pointers rather than COM
pointers. Their existing owners explicitly delete them when releasing assets;
callers must not duplicate ownership. Static buffer groups retain their shared
offsets and separate batch index buffer. Animation and vegetation use Map/Unmap.
Nameplate and custom-texture caches also own shared textures. Renderer-internal
render targets and postprocessing resources use shared owners backed by D3D9.

The loader accepts a borrowed Graphics service and models expose BuildBuffers;
legacy device-taking entry points adapt to the same service for existing callers.
The D3D9 adapter is inline so standalone parser/rendering test projects use the
same implementation without linking the application runtime.

`renderer_materials.h` now defines the shared `Materials` binding service and
`MaterialBindingCache`, without graphics API headers. ModelRenderer accepts a
borrowed service in its draw context; existing callers use the D3D9 adapter when
none is supplied. Opaque batches, individual opaque meshes, and transparent
meshes all bind through this interface. The adapter retains the existing FFXI
alpha/culling rules, replacement textures, bump/PBR selection, state ordering,
and cache invalidation. Asset metadata is borrowed; the service does not own it.
Pass-level blending, depth, lighting, and special water state remain separate.

`Graphics` also creates owned `Shader` objects, binds vertex/pixel stages, and
uploads float4 constants. Null binding restores fixed-function processing. D3D9
owns the native shader references and rejects incompatible stages; compile and
creation failure return null. Texture/bump, UI, PBR, directional shadow,
home-point and postprocessing shader paths now use these
operations, with unchanged shader source, profiles, constants, and fallbacks.
Shader source/profile and constant register mappings belong to each backend's
shader library: the interface does not make HLSL portable to Metal or Vulkan.
Like textures, shader objects must be destroyed before backend shutdown.

`RenderTarget` now owns color/depth attachments, with descriptions for BGRA/BGRX,
R32 float and depth24/stencil8, optional multisampling, and sampleable color
targets. Graphics creates targets, captures owned references to current attachments,
binds them, and copies/resolves full surfaces without filtering. Shadow targets
and postprocessing textures use these owners. Shadow attachment changes and
SMAA intermediate target binding use the shared service. Default-pool targets and
captured attachment references must be released before reset and recreated after.

`PipelineState` provides optional depth, culling, blend-factor, enable/disable,
color-write and depth-bias fields. Unspecified fields retain their previous values;
the D3D9 adapter maps shared enums to native constants. `PipelineSnapshot` captures
the current pipeline immediately and restores it explicitly. Snapshots retain
resource references and must also be released before reset. Render-target
attachments remain separate from snapshots, preserving the original restoration
order. Shadow, home-point and postprocessing passes use shared snapshots and
shared state updates at their existing call locations.

The remaining pipeline setup now uses shared fields for fill mode, blend
operation, alpha comparison/reference, clipping and clip-plane masks, dithering,
multisample masks, and fog color/range/modes. Queries return these fields too.
All production render-state writes and state-block capture/restore operations
are inside the D3D9 adapter; callers preserve their individual update order.

`Graphics::Clear`, `BeginScene`, and `EndScene` handle frame submission boundaries.
`CaptureBackBuffer` returns an owned presentation-target reference. MSAA color/depth
targets, resolve copies, postprocessing passes and their fallback copies now use
shared targets and submission. `RenderTarget::MapRead` is an optional direct read
mapping (not an implicit GPU staging copy); it can fail for non-lockable targets.
Successful mappings expose pixels and row pitch until `UnmapRead` or destruction.
The existing uniform-output postprocessing fallback uses this mapping unchanged.

The Windows lifecycle interface exposes an owned `OverlayContext` lease for GDI
UI compositing, with an HDC and presentation dimensions. Destroy it before further
GPU operations, presentation, reset or shutdown. The D3D9 implementation owns the
surface reference and matching GetDC/ReleaseDC pair. Title/debug overlays use the
runtime entry point; legacy device-taking child-window painters use the same
adapter and shared lease. This Windows-specific bridge does not make GDI portable
to future non-Windows backends. Some legacy resource/material entry points still
accept a D3D9 device; this stage does not remove that compatibility bridge.

`VertexLayout` owns programmable single-stream vertex declarations described by
byte offsets, float/packed BGRA formats, and position/normal/color/UV semantics.
Postprocessing uses this resource with its original float4-position/float2-UV
declaration. `BuiltinVertexLayout` preserves the existing fixed-function model,
position/color, and screen/color/UV formats exactly. Every production layout bind
now passes through Graphics; only the D3D9 adapter uses native FVF/declaration calls.
Layouts are caller-owned and must be destroyed before backend shutdown. The
initial declaration interface accepts up to 16 elements in non-overlapping offset
order, without duplicate semantic/index pairs; instanced/multiple streams are not
part of this stage.

`SamplerState` exposes optional min/mag/mip filtering, U/V/W addressing, sRGB,
anisotropy, mip limits/bias and border color. Unspecified fields and other sampler
slots remain unchanged. All production sampler writes now use this interface,
preserving original call order, wrap/clamp, point/linear filtering, mip toggles,
and sRGB settings. The initial interface addresses pixel sampler slots 0–15;
vertex-shader sampler slots remain out of scope. Pipeline snapshots restore
sampler and vertex-declaration state as part of the existing D3D9 state capture.

`StencilState` exposes optional enable/two-sided settings, reference/read/write
masks, comparisons and fail/depth-fail/pass operations for both winding directions.
The D3D9 adapter maps the primary face to its standard stencil states and the
back face to its counter-clockwise states. Existing shadow stencil operations
and call order are unchanged.

`TextureStageState` covers the fixed-function operations and arguments used by
DATura, result routing, UV indices and transform component counts. Shared methods
also set row-major texture matrices and the packed texture-factor color. Existing
water/scrolling, opaque/cutout/translucent, UI and untextured stage updates now
use these methods. The old device-taking material helper retains its native
alpha-operation parameter through an explicit compatibility adapter; supported
operations are disable, select-argument, modulate (1x/2x/4x), and add. Generated
coordinates, projected transforms and other unused combiner modes are not yet
included in this initial interface.

`LightingState` covers enable flags, vertex-color/normal/specular settings, ambient
color and material color sources. `Light` describes point, directional and spot
lights with colors, position/direction, attenuation, range and cone parameters;
setting and enabling lights are separate operations. Sun/fill/point-light formulas
and authored colors remain unchanged. Production writes use shared interfaces.

Shared queries now read pipeline, sampler, stencil, texture-stage and lighting
descriptors from the device rather than a cached state mirror. They populate all
fields on success and leave the caller's output unchanged on failure, including
unsupported native enum values. Fog queries expose the enable flag, packed color,
and start/end distances as floats. All existing production render-state reads
now use these queries; resource/capability queries remain separate.

World/view/projection and texture transforms use 16 row-major floats with the
existing row-vector convention, without transposition or coordinate conversion.
Viewport get/set uses a shared descriptor. Existing transform/viewport calls in
models, actors, effects, shadows, sky, picking overlays and UI sizing now use the
shared service. D3D matrix types still occur in CPU math and higher-level APIs;
the shared transform interface itself does not depend on them. Queries and setters
do not bind shaders, draw, or change unrelated state.

Other pipeline settings, including fog setup, retain native calls in some paths.
Swap-chain/MSAA presentation plumbing and some surface operations retain native
bridges; this is not yet a fully portable renderer. Draw submission consumes
configured state. Windows handles
and HRESULT remain in the lifecycle contract; LegacyD3D9Device and NativeTexture
are explicit temporary bridges. A second backend still needs the remaining
pipeline-state migration before it can render the whole scene.

Validation: build DATura Release/x64, then build and run
`tests/RendererBackendTests.vcxproj`. The test covers uninitialized calls,
unsupported backend rejection, repeated shutdown, GPU texture upload/binding, and
buffer mapping bounds. GPU checks require a D3D9 HAL device and use a hidden window.
MaterialRenderingTests, WaterRenderingTests, and VegetationTests cover pixel
results, depth/fog/shadows, synthetic water composition, and animated buffer offsets.
Installed-DAT and interactive device-loss recovery checks remain separate.

Validated on 2026-10-16:
- DATura Release/x64 build succeeded.
- Resource upload/binding and lifecycle tests passed on a hidden D3D9 HAL device.
- Material/depth/fog/shadow suite: 1,716 assertions, zero failures.
- Synthetic water composition and vegetation tests passed.
- Player animation suite passed using installed assets for eight player race
  variants and sampled NPCs, including GPU buffer checks.
- Nameplate GPU checks passed using `--render-only` (colors, icons, cache isolation,
  render state, and occlusion). The default nameplate suite stops at its separate
  Config settings precheck; that failure was not changed or suppressed in default
  test execution. The focused mode excludes Config and quest-catalog prechecks.

Material/shader interface validation (2026-10-16):
- DATura Release/x64 build succeeded.
- Shared vertex/pixel shader creation, stage-mismatch rejection, native binding,
  constant readback, unbinding, and compile-failure checks passed, alongside the
  existing resource/lifecycle checks.
- Material tests now bind through `RendererBackend::Materials`, covering alpha,
  depth, fog, bump, PBR shader compilation and layered shadows. UI shader pixel
  checks cover DXT3 alpha expansion and the maximum-opacity clamp.
  Final run: 1,776 assertions, zero failures.
- Synthetic water composition matched its independent reference exactly (zero
  differing pixels); installed-DAT water checks were not run in this pass.
- Focused nameplate GPU checks passed with installed assets (`--render-only`),
  excluding the previously documented Config/quest prechecks.

Specialized shader migration validation (2026-10-16):
- Shadow, home-point, FXAA and SMAA shader creation, binding, constants, unbinding
  and destruction use the shared shader interface. D3D9 remains the sole backend.
- Shader source definitions were compared with their pre-migration versions and
  are unchanged, as are shader profiles, entry points and constant registers.
- DATura Release/x64 build succeeded; material/shadow tests passed all 1,776
  assertions, including layered shadows, hardware vertex processing and MSAA.
- HomePointTests passed with installed assets (zero failures), including idle and
  activation rendering, state restoration, device reset and failed reload behavior.
- RendererBackendTests passed shader/resource/lifecycle checks plus new FXAA/SMAA
  solid-region pixel readbacks, render-state restoration and resize recovery.
  These checks do not claim driver-independent antialiased edge pixel identity.

Render-target/pipeline interface validation (2026-10-16):
- Shared target creation, descriptions, color/depth attachment binding, sampleable
  color binding, invalid descriptors, and copied-pixel readback passed.
- Shared pipeline enum mapping, individual-field preservation, and snapshot
  restoration passed on the native D3D9 device.
- Renderer shader/resource/lifecycle and FXAA/SMAA output/state/resize checks passed.
- Material/depth/fog/shadow suite: 2,657 assertions, zero failures.
- Installed-asset HomePointTests: zero failures, including state restoration and reset.

Vertex-layout/sampler interface validation (2026-10-16):
- DATura Release/x64 build succeeded; D3D9 remains the sole implementation.
- GPU declaration readback matches the original postprocessing offsets, types,
  and semantics; all three built-in formats match their original FVF values.
- Invalid layout rejection, sampler enum mapping, stage/field isolation, and
  invalid sampler rejection passed alongside target/pipeline/shader tests.
- FXAA/SMAA output, state restoration and resize checks passed.
- Material/depth/fog/shadow suite: 2,661 assertions, zero failures.
- Installed-asset HomePointTests and focused nameplate GPU checks passed.
  Nameplates used `--render-only`, retaining the earlier Config precheck exclusion.
- Synthetic water output matched its reference exactly; installed-DAT water
  checks were not run in this pass.

Stencil/texture-stage/lighting interface validation (2026-10-16):
- DATura Release/x64 build succeeded, with D3D9 as the sole implementation.
- Native readback verified both stencil faces, masks/reference, texture-stage
  operations/arguments, UV transform matrices, lighting state, and directional,
  point and spot descriptors. Invalid enums/stages and snapshot restoration passed.
- Existing shared resource, sampler, layout, shader and FXAA/SMAA checks passed.
- Material/depth/fog/shadow suite: 2,661 assertions, zero failures.
- HomePointTests with installed assets passed with zero failures. Material and
  home-point test targets required clean rebuilds after invalid-object linker
  errors; rebuilt executables were used for these results.
- Synthetic water matched its reference exactly; focused nameplate GPU checks
  passed using `--render-only`. Installed-DAT water and the separate nameplate
  Config/quest prechecks were not run in this pass.

State-query/transform interface validation (2026-10-16):
- DATura Release/x64 build succeeded, with D3D9 as the sole implementation.
- Non-symmetric world/view/projection and texture matrices matched native readback
  exactly; native matrix changes were visible through shared queries.
- Viewport/fog readback, pipeline/sampler/stencil/texture-stage/lighting queries,
  invalid-query rejection and unchanged output on failure passed GPU checks.
- Existing resource/state/layout/shader and FXAA/SMAA resize checks passed.
- Material/depth/fog/shadow suite: 2,661 assertions, zero failures.
- Installed-asset home-point and focused nameplate GPU checks passed.
- Synthetic water output matched its reference exactly. Nameplate Config/quest
  prechecks and installed-DAT water checks were not run in this pass.

Pipeline setup/presentation migration validation (2026-10-16):
- DATura Release/x64 build succeeded; DirectX9 remains the sole implementation.
- Native readback verified the additional pipeline fields, including exact fog
  float bits, and shared queries matched them.
- No-AA, MSAA, FXAA and SMAA combinations passed clear/scene, output pixel,
  state restoration, GDI overlay pixel/read mapping, Present and resize checks.
- Existing shared resource, shader, layout, sampler, transform and query checks passed.
- Material/depth/fog/shadow suite: 2,661 assertions, zero failures.
- Installed-asset home-point and focused nameplate GPU checks passed; synthetic
  water matched its reference exactly. Nameplate Config/quest prechecks and
  installed-DAT water checks were not run in this pass.
