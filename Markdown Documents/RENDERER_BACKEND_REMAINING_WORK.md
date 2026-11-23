# Renderer backend abstraction: remaining work

## Purpose

This document describes the work still required before DATura can support a rendering
backend other than Direct3D 9. It is based on the current working tree as of
2026-10-16, not on the original first-pass wrapper.

Direct3D 9 remains the only usable backend today. The shared renderer layer is already
substantial: device lifecycle, resources, shaders, render targets, pipeline state,
vertex layouts, samplers, stencil, fixed-function texture stages, lighting,
transforms, viewport state, frame submission, post-processing, and GDI overlay access
all have backend-facing interfaces. The remaining work is primarily eliminating the
native-device compatibility path from production rendering code, defining portable
shader and pass behavior, tightening ownership and error contracts, and proving the
interface with a second implementation.

This is a migration plan, not a proposal to rewrite the renderer in one step. Each
milestone should leave the D3D9 build working and should preserve the current visual
output unless a task explicitly says otherwise.

## Current architecture

The important pieces already present are:

- `renderer_interface.h` defines backend lifecycle and presentation operations.
- `renderer_resources.h` defines shared GPU resources, state descriptions, transforms,
  render targets, submission operations, and state queries.
- `renderer_materials.h` defines shared material binding behavior.
- `renderer_backend.h/.cpp` owns the selected implementation behind
  `RendererBackend::Interface`.
- `d3d9_device.h/.cpp` implements initialization, reset, presentation, anti-aliasing,
  post-processing, and device-loss recovery.
- `d3d9_graphics.h` implements the shared graphics resource and command interfaces.
- Model textures and mesh buffers have begun moving from COM pointers to shared
  `Texture` and `MeshBuffer` objects.
- `RendererBackendTests` exercises lifecycle, resources, shaders, render targets,
  state mapping, frame operations, overlays, post-processing, and resize behavior.

The architecture is currently transitional:

```text
Application and scene code
    |-- shared Graphics/Materials calls
    |-- temporary IDirect3DDevice9 calls and D3D9 helper entry points
    v
RendererBackend::Runtime
    v
RendererBackend::Interface
    v
D3D9Device::Runtime
    v
D3D9Graphics::Context and native D3D9 objects
```

The target architecture is:

```text
Application and scene code
    |-- backend-neutral frame/pass descriptions
    |-- backend-neutral resources, materials, and draw commands
    v
RendererBackend::Runtime
    v
RendererBackend::Interface selected by a backend factory
    |-- D3D9Backend
    |-- future D3D11/Vulkan/Metal/etc. implementation
```

No production scene, asset, UI, effect, or model module should include D3D headers or
receive a native D3D device in the target architecture.

## What “backend abstracted” means

The abstraction is complete enough to add another backend when all of the following
are true:

- A backend can be selected through a factory without changing application or scene
  code.
- A backend reports whether it is compiled, supported on the current platform, and
  usable on the current machine.
- Application rendering never calls `IDirect3DDevice9` or converts shared resources
  back to native D3D9 resources.
- Shared headers used by scene and asset code do not include `<d3d9.h>`, `<d3dx9.h>`,
  or other API-specific graphics headers.
- Native types are confined to the D3D9 implementation and D3D9-specific tests.
- Shader selection does not require portable code to know HLSL profiles or D3D
  constant-register numbers.
- Resource ownership and device-reset behavior are explicit for every resource type.
- Frame and render-pass ordering are represented above the backend implementation.
- A second backend, or at minimum a strict validation backend, can initialize and
  execute representative scenes through the same public contracts.
- Visual regression tests compare stable reference scenes and identify unsupported
  capability paths deliberately rather than silently falling back.

## Guiding constraints

### Preserve FFXI behavior

The original game relies heavily on fixed-function-era behavior: texture combiners,
vertex colors, alpha tests, fog, lighting, winding, stage transforms, and precise draw
ordering. A modern backend may implement these using shaders, but the shared contract
must preserve their semantics.

The PS2 client is useful as a behavioral and frame-organization reference. Its
separation of scene update, actor drawing, sorted effects, post-processing, UI, and
frame swap supports the pass-oriented direction described below. Its GS and VU packet
formats must not become part of the shared API.

### Keep the D3D9 implementation authoritative during migration

Until a second backend produces matching output, D3D9 is the compatibility oracle.
Migration work should route an existing operation through the shared interface first,
verify the same state and pixels, and only then remove the native path.

### Avoid an API-shaped abstraction

The shared interface should describe DATura’s rendering needs, not reproduce every
D3D9 method. When a new requirement appears, define the behavior and ownership DATura
needs, then map it to D3D9. Do not expose a native enum merely because a caller already
uses one.

## Priority 0: protect the current baseline

Before widening the abstraction, preserve a reproducible baseline.

- [ ] Commit or otherwise isolate the current renderer work from unrelated working-tree
  changes. The working tree currently contains many renderer and non-renderer edits,
  making regressions difficult to attribute.
- [ ] Record the exact build configuration used for renderer validation: architecture,
  configuration, SDK, compiler toolset, and runtime asset path.
- [ ] Add a single documented command or script that builds DATura and all renderer
  validation targets in dependency order.
- [ ] Add a single documented command or script that runs the renderer test matrix and
  writes a concise pass/fail summary.
- [ ] Store representative reference captures for at least one zone, one character,
  transparent geometry, water, weather, nameplates, home-point effects, UI, shadows,
  and each anti-aliasing mode.
- [ ] Record tolerances for pixel comparisons. Exact equality is appropriate for some
  synthetic D3D9 fixtures but not necessarily across vendors or future APIs.
- [ ] Separate tests requiring installed game assets from synthetic tests so the
  portable renderer contract can be validated in CI without an FFXI installation.
- [ ] Resolve or explicitly quarantine the existing default nameplate suite’s Config
  and quest-catalog precheck failure. A permanent focused-test exception can hide an
  unrelated regression.
- [ ] Run the installed-DAT water checks that are repeatedly listed as not run in the
  current renderer notes.
- [ ] Add an interactive or automated device-loss/reset exercise, since the current
  notes identify it as separate from the normal test run.

Acceptance criteria:

- A clean renderer baseline can be rebuilt and rerun without reconstructing commands
  from conversation history.
- Failures identify whether they come from shared behavior, the D3D9 adapter, installed
  assets, or an unrelated application subsystem.

## Priority 1: remove platform and D3D9 types from the public lifecycle contract

`RendererBackend::Interface` is backend-polymorphic, but it is not yet platform- or
API-neutral. It includes Windows types and exposes a D3D9 escape hatch.

### Replace HRESULT results

`ResolveFrame()` and `Present()` return `HRESULT`. That leaks COM/D3D conventions into
every future implementation.

- [ ] Define a shared result type, for example `OperationResult`, with at least:
  `Success`, `NotInitialized`, `DeviceLost`, `OutOfDate`, `Unsupported`, and `Failed`.
- [ ] Decide whether failures carry a backend diagnostic code or message. If retained,
  diagnostics must be optional metadata and not influence portable control flow.
- [ ] Change `ResolveFrame()` and `Present()` to return the shared result.
- [ ] Update application error handling to branch on shared outcomes.
- [ ] Keep native `HRESULT` translation entirely inside `D3D9Device::Runtime`.
- [ ] Update tests to assert shared results rather than `SUCCEEDED`/`FAILED`.

### Replace HWND/HDC in the core interface

The current interface accepts `HWND` and returns an `OverlayContext` exposing `HDC`.
That prevents the lifecycle header from being used by a non-Windows backend.

- [ ] Introduce a platform-surface descriptor or opaque native-window handle with an
  explicit platform tag.
- [ ] Decide whether window ownership remains entirely outside the renderer. It should;
  the backend should borrow a presentation target and never destroy the application
  window.
- [ ] Move the GDI overlay lease behind a Windows-only extension interface, such as
  `WindowsOverlayProvider`.
- [ ] Make overlay support queryable through capabilities.
- [ ] Keep title/debug GDI paths operational on D3D9 while preparing a renderer-native
  UI replacement.
- [ ] Remove `<windows.h>` from core renderer headers when no shared type requires it.

### Remove the native-device escape hatch

- [ ] Replace every call to `Runtime::D3D9Device()`/`LegacyD3D9Device()` with a shared
  service or a deliberately D3D9-owned subsystem.
- [ ] Delete `Interface::LegacyD3D9Device()` after the last production caller moves.
- [ ] Delete `Runtime::D3D9Device()` after tests and application code no longer need it.
- [ ] Add a source check that rejects `LegacyD3D9Device`, `GraphicsDevice()`, and
  `NativeTexture` outside D3D9 implementation/test allowlists.

Acceptance criteria:

- `renderer_interface.h` contains no D3D9 declarations, `HRESULT`, `HWND`, or `HDC`.
- A platform/backend implementation can include the lifecycle contract without Windows
  or DirectX headers.

## Priority 2: make backend creation and selection real

`RendererBackend::Runtime` currently constructs `D3D9Device::Runtime` unconditionally.
The setting can reject unavailable choices, but it cannot instantiate another backend.

### Backend identity and descriptors

- [ ] Replace integer backend identifiers with a scoped `BackendId` enum or stable
  string identifier.
- [ ] Define `BackendDescriptor` with stable ID, display name, build availability,
  platform support, runtime support, and an unavailable reason.
- [ ] Distinguish these states:
  - not compiled into this build;
  - incompatible operating system;
  - required runtime/driver missing;
  - adapter or feature level unsupported;
  - compiled and selectable;
  - experimental but selectable.
- [ ] Generate the configuration dropdown from descriptors instead of separate name,
  count, clamp, and availability functions.
- [ ] Preserve unknown persisted backend IDs safely so newer settings can be opened by
  older builds without accidental remapping.

### Factory and fallback policy

- [ ] Add a backend factory that creates an `Interface` for a `BackendId`.
- [ ] Move D3D9 construction out of `Runtime`’s constructor and into the factory.
- [ ] Define what happens when the selected backend cannot initialize:
  - report and remain stopped;
  - explicitly offer D3D9 fallback;
  - automatically fall back only when the user enabled that policy.
- [ ] Do not silently overwrite the stored selection with D3D9. The UI should distinguish
  “selected preference” from “active backend.”
- [ ] Expose the active backend and fallback reason in diagnostics/configuration UI.
- [ ] Recreate the implementation when the selected backend changes, with resources
  released before the old backend is destroyed.
- [ ] Ensure repeated initialization and shutdown remain idempotent for every backend.

Acceptance criteria:

- Adding a stub backend requires a new factory registration and implementation, not
  changes throughout `main.cpp`.
- The configuration UI accurately explains why a backend is unavailable.
- Requested and active backend values cannot be confused.

## Priority 3: define capabilities and limits

The current code assumes D3D9-era limits and features. A second backend needs a formal
capability contract.

- [ ] Define a `Capabilities` structure returned after adapter/backend discovery.
- [ ] Include texture format support for BGRA8, BGRX8, BC1, BC2, BC3, R32 float, and
  depth/stencil formats currently used.
- [ ] Include maximum texture dimensions, render-target dimensions, sampler count,
  texture-stage count, light count, and shader constant capacity.
- [ ] Include supported sample counts per render-target/depth format.
- [ ] Include support flags for render-target sampling, depth sampling if needed,
  direct target read mapping, GDI/native overlay access, fixed-function emulation,
  clip planes, two-sided stencil, sRGB sampling, anisotropy, and timestamp/debug tools.
- [ ] Define required baseline capabilities separately from optional quality features.
- [ ] Make anti-aliasing choices query capabilities instead of assuming a numeric sample
  count is usable.
- [ ] Make texture upload select a supported format or a documented CPU conversion.
- [ ] Make shader/material selection choose an explicit fallback when a capability is
  absent.
- [ ] Expose capability information in a diagnostics dump for bug reports.

Acceptance criteria:

- Unsupported features fail predictably or choose a tested fallback.
- Scene code does not ask the backend implementation directly about native caps.

## Priority 4: convert application frame orchestration to shared APIs

`main.cpp` remains one of the largest native dependency concentrations. It still owns
substantial render sequencing and reaches the D3D9 device through `GraphicsDevice()`.

### Introduce an application render context

- [ ] Replace `GraphicsDevice()` with an application-owned `RenderContext` containing
  borrowed references to `Graphics`, `Materials`, capabilities, frame dimensions, and
  immutable per-frame camera/environment data.
- [ ] Pass this context to scene, character, effect, debug, and UI renderers.
- [ ] Separate simulation/update data from rendering services so update code cannot
  acquire a GPU device accidentally.
- [ ] Keep backend-owned resources out of global application state where practical.

### Make frame phases explicit

Define and enforce an order similar to:

1. prepare/acquire frame;
2. update per-frame render data;
3. begin scene;
4. sky and background;
5. opaque world geometry;
6. characters and world objects;
7. transparent and sorted effects;
8. overlays, nameplates, and UI;
9. end scene;
10. resolve/post-process;
11. platform overlay, if supported;
12. present.

- [ ] Extract this ordering from the monolithic render section in `main.cpp`.
- [ ] Give each pass an explicit state setup and restoration contract.
- [ ] Avoid relying on state left behind by the previous pass.
- [ ] Move clear, begin/end scene, resolve, overlay, and present calls through the
  runtime/shared interface exclusively.
- [ ] Move viewport/client-size mapping behind shared frame dimensions and viewport
  helpers.
- [ ] Replace native backbuffer capture in screenshot code with shared capture/readback.
- [ ] Define behavior when frame preparation reports lost/out-of-date/not-ready.

Acceptance criteria:

- `main.cpp` does not call a native graphics device.
- Frame order is readable as a sequence of named passes.
- Every pass can be invoked against a validation backend with deterministic inputs.

## Priority 5: migrate renderer call sites

The following production areas still contain native graphics types or D3D-shaped entry
points. Migrate them in small groups, preserving pixel tests after each group.

### Model rendering and model buffers

High-priority files:

- `model_renderer.cpp/.h`
- `d3d_model_buffers.cpp/.h`
- `d3d_model_render_state.cpp/.h`
- `noesis_rapi.cpp/.h`
- `model_ff11_texture_handler.h`
- `zone_model_render_metadata.cpp/.h`
- `zone_model_transform.cpp/.h`

Tasks:

- [ ] Make `ModelRenderer::Context` require `Graphics` and `Materials`; remove its native
  device member and fallback adapter construction.
- [ ] Rename D3D-prefixed modules once their interfaces are neutral, for example
  `model_buffers` and `model_render_state`.
- [ ] Convert buffer build/draw functions to accept `Graphics&` and shared resources.
- [ ] Replace `D3DCOLOR` inputs with a shared packed-color type or `uint32_t` with a
  documented channel order.
- [ ] Replace `D3DTEXTUREOP` compatibility parameters with `TextureOperation`.
- [ ] Remove `IDirect3DTexture9*` from material and custom-texture paths.
- [ ] Ensure material cache invalidation is explicit when passes modify textures,
  shaders, samplers, or fixed-function emulation state.
- [ ] Replace D3D matrix structures in public rendering signatures with the shared
  row-major matrix representation or a project math type.
- [ ] Preserve static opaque batching, vegetation updates, animated buffer mapping,
  transparent ordering, water behavior, and authored alpha rules.

Acceptance criteria:

- Model parsing and CPU animation remain independent of the backend.
- All model GPU creation, update, and drawing use shared interfaces.
- Existing material, vegetation, animation, sky, sign, and water tests retain their
  expected output.

### Custom textures, bump mapping, and materials

High-priority files:

- `custom_texture_assets.cpp/.h`
- `d3d_bump_mapping.cpp/.h`
- `d3d_model_render_state.cpp/.h`

Tasks:

- [ ] Store `RendererBackend::Texture*`, not `IDirect3DTexture9*`, in replacement
  texture results and caches.
- [ ] Store a borrowed `Graphics*` or renderer-owned service, not a global native device.
- [ ] Move PBR and bump shader creation into a backend shader library.
- [ ] Replace `NativeTexture` conversions with `Graphics::BindTexture`.
- [ ] Define PBR/bump material inputs independently of shader registers.
- [ ] Make fallback to fixed-function-compatible rendering explicit and tested.
- [ ] Destroy cached shaders and textures before backend shutdown/reset as required.

Acceptance criteria:

- `NativeTexture()` has no production caller.
- Custom textures and PBR/bump paths operate entirely on shared resources.

### UI, title screens, and nameplates

High-priority files:

- `d3d_ui_renderer.cpp/.h`
- `ffxi_title_ui_primitives.cpp/.h`
- `ffxi_title_screen_renderer.cpp/.h`
- `ffxi_nation_selection_renderer.cpp/.h`
- `ffxi_main_menu.cpp/.h`
- `npc_nameplate_renderer.cpp/.h`
- `npc_chat_window.h`
- `ui_backbuffer.h`

Tasks:

- [ ] Rename `D3DUiRenderer` after it no longer accepts a native device.
- [ ] Accept `Graphics&`, frame dimensions, and shared textures in all quad/text APIs.
- [ ] Define a shared screen-space vertex format/layout once and reuse it.
- [ ] Move FFXI UI alpha-expansion shader lookup behind the shader library.
- [ ] Replace child-window/GDI drawing where practical with renderer-native UI draws.
- [ ] Keep the Windows overlay extension only for UI that genuinely requires GDI.
- [ ] Move nameplate texture caches to renderer lifecycle ownership and make cache
  invalidation explicit on backend recreation.
- [ ] Replace viewport/client coordinate helpers that require a native D3D device.
- [ ] Add UI clipping/scissor support to the shared interface if existing native paths
  require it.

Acceptance criteria:

- UI renderers can draw to any backend supporting the shared screen-space contract.
- Nameplates, title UI, chat UI, and nation selection pass synthetic pixel tests without
  native resource access.

### Environment, sky, weather, and effects

High-priority files:

- `zone_environment_render_state.cpp/.h`
- `zone_environment_fog.cpp/.h`
- `zone_sky_dome.cpp/.h`
- `zone_weather_particles.cpp/.h`
- `home_point_effect.cpp/.h`
- `zone_object_highlight_renderer.cpp/.h`
- `zone_collision_geometry.cpp/.h`
- `world_item_renderer.cpp/.h`
- `zone_object_transform.cpp/.h`

Tasks:

- [ ] Pass the shared render context to every environment/effect renderer.
- [ ] Replace direct native state calls with complete shared state descriptors.
- [ ] Replace immediate D3D draw calls with shared draw submission.
- [ ] Move shader creation and constants behind semantic shader programs.
- [ ] Replace D3D matrix/color/vector types in public headers.
- [ ] Define explicit pass ownership for fog, blending, depth writes, culling, stencil,
  clip planes, and texture stages.
- [ ] Confirm transparent weather particles and highlighted geometry do not depend on
  inherited state.
- [ ] Verify resource recreation for home-point and other default-pool-like effects.

Acceptance criteria:

- These modules include no Direct3D headers and accept no native graphics pointers.
- Existing home-point, weather, sky, water, shadow, highlight, and collision-overlay
  tests pass through shared services.

### Asset loaders and animation entry points

High-priority files:

- `scene_model_loader.cpp/.h`
- `player_model_loader.cpp/.h`
- `creation_model_loader.cpp/.h`
- `title_scene_assets.cpp/.h`
- `ffxi_title_assets.cpp/.h`
- `ffxi_sqle_model_animation.cpp/.h`
- `ffxi_model_lifetime.cpp`

Tasks:

- [ ] Make GPU-aware loader functions accept `Graphics&` or a dedicated resource
  uploader.
- [ ] Separate CPU parsing/decompression from GPU upload so parsed assets can survive a
  backend recreation and be uploaded again.
- [ ] Remove legacy device-taking overloads after all callers migrate.
- [ ] Make animation buffer updates use shared mapping only.
- [ ] Define whether loaders return CPU assets, GPU assets, or a composed object; avoid
  ambiguous mixed ownership.
- [ ] Ensure failed uploads leave CPU assets valid and release partial GPU resources.

Acceptance criteria:

- Loading/parsing can be tested without constructing D3D9.
- GPU upload can be rerun against a new backend without reparsing DAT files.

## Priority 6: replace D3D matrix and color types in cross-module APIs

Even where commands use `Graphics`, many signatures and structures still use D3D math
types. CPU math is not itself a backend operation, but D3D types in public headers keep
the dependency graph tied to DirectX.

- [ ] Inventory every `D3DMATRIX`, `D3DXMATRIX`, `D3DVECTOR`, `D3DXVECTOR*`, `D3DCOLOR`,
  and `D3DCOLORVALUE` occurrence outside D3D9 implementation files.
- [ ] Adopt or define project math types for matrix, vector, quaternion, plane, and
  packed color values.
- [ ] Document row-vector/row-major conventions and clip-space expectations.
- [ ] Add conversion helpers only at backend boundaries.
- [ ] Keep coordinate-system conversion out of arbitrary scene modules.
- [ ] Replace D3D color-construction macros with shared color utilities that define
  clamping and channel packing.
- [ ] Update picking, frustum, door interaction, NPC placement, coordinate frames, and
  diagnostics modules that include D3D types only for CPU math.
- [ ] Add math tests for projection, inverse transforms, frustum extraction, screen
  mapping, and handedness.

Acceptance criteria:

- Non-D3D renderer and gameplay headers compile without DirectX headers.
- Matrices produce identical D3D9 results after boundary conversion.

## Priority 7: create a semantic shader system

The shared API can create shaders, but portable callers still supply source text,
entry points, profiles, and numeric constant registers. That abstracts ownership, not
shader portability.

### Shader program identity

- [ ] Define semantic programs, for example:
  - fixed-function model emulation;
  - textured FFXI model with authored-alpha variants;
  - UI texture/alpha expansion;
  - bump mapping;
  - PBR replacement material;
  - home-point effect;
  - directional shadow;
  - FXAA;
  - SMAA edge, blend, and neighborhood passes.
- [ ] Define variant keys rather than duplicating loosely related source strings.
- [ ] Make each backend own its shader source/binary and profile selection.
- [ ] Keep HLSL and `vs_2_0`/`ps_2_0` names inside the D3D9 backend.

### Semantic parameters and bindings

- [ ] Replace public numeric constant registers with named parameter blocks.
- [ ] Define vertex inputs semantically and validate them against bound layouts.
- [ ] Define texture/sampler slots semantically for each program.
- [ ] Decide whether fixed-function behavior is implemented as native D3D9 state,
  generated shaders, or a shared emulation shader on modern backends.
- [ ] Define shader compilation error reporting with program, variant, and backend
  information.
- [ ] Add a shader cache whose ownership and invalidation follow backend lifetime.
- [ ] Add offline shader compilation where appropriate for future backends.

Acceptance criteria:

- Portable rendering code never supplies HLSL, a D3D profile, or a register number.
- Shader variants and required parameters are testable independently of scene code.

## Priority 8: formalize resource ownership and recreation

The current shared resources are mostly raw owning pointers deleted by callers. This is
workable but fragile during backend changes, partial failures, and device reset.

- [ ] Replace raw owning `Texture*`, `MeshBuffer*`, `Shader*`, `VertexLayout*`,
  `RenderTarget*`, and `PipelineSnapshot*` with explicit unique-owner aliases or RAII
  wrappers.
- [ ] Distinguish owning handles from borrowed references in signatures.
- [ ] Prevent cross-backend binding. Resources should carry backend/device identity and
  reject use by another implementation deterministically.
- [ ] Define resource validity after device loss, resize, and backend recreation.
- [ ] Categorize resources as persistent/recreatable, transient frame resources, or
  external/imported resources.
- [ ] Replace global release/recreate callbacks with a registry or explicit owner graph
  if callbacks become difficult to order.
- [ ] Preserve CPU upload data or reconstruction recipes where automatic recreation is
  required.
- [ ] Ensure partially created render-target groups unwind safely.
- [ ] Ensure mapped resources cannot survive reset, submission, or destruction.
- [ ] Add debug names to all resources.
- [ ] Add optional leak/lifetime diagnostics in debug builds.
- [ ] Define thread-affinity rules for creation, mapping, destruction, and drawing.

Acceptance criteria:

- Backend shutdown cannot leave live resources silently referencing a destroyed device.
- Device reset and backend switching have deterministic release/recreation order.
- Ownership is obvious from every public signature.

## Priority 9: finish pipeline and pass abstraction gaps

The shared state layer is broad, but it should be audited against every remaining
native call rather than assumed complete.

- [ ] Inventory every remaining `SetRenderState`, `GetRenderState`, `SetSamplerState`,
  `SetTextureStageState`, `SetTransform`, `SetViewport`, `SetLight`, `SetTexture`,
  `SetStreamSource`, `SetIndices`, `SetFVF`, and shader call outside the adapter.
- [ ] Add a shared operation only when the behavior is genuinely needed.
- [ ] Add scissor rectangles if any UI/debug path depends on native scissoring.
- [ ] Add explicit clip-plane descriptions if clipping remains in scene code.
- [ ] Define depth-range and viewport-origin conventions for APIs with different clip
  spaces.
- [ ] Define front-face convention independently of D3D winding constants.
- [ ] Define color-space behavior for textures, render targets, blending, screenshots,
  and presentation.
- [ ] Define texture-origin/UV convention and handle backend flips at one boundary.
- [ ] Define render-target load/store intent for modern APIs, even if D3D9 ignores it.
- [ ] Decide whether `PipelineSnapshot` remains part of the long-term contract. Modern
  APIs generally favor explicit immutable pipeline/pass state; snapshots may remain a
  migration aid rather than the final design.
- [ ] Replace field-by-field state mutation in hot paths with reusable state/pass
  descriptions where it improves correctness and modern-backend mapping.
- [ ] Define barriers/transitions implicitly through pass/resource usage or explicitly
  enough for Vulkan/D3D12-style implementations.

Acceptance criteria:

- No production state mutation bypasses `Graphics`.
- Passes state all behavior they depend on.
- The contract has defined coordinate, color-space, and synchronization semantics.

## Priority 10: split commands from resources and consider command recording

`Graphics` currently combines resource creation, state changes, queries, frame
operations, and draw submission. That is sufficient for D3D9 but may constrain modern
backends.

- [ ] Decide whether to split the interface into:
  - `RenderDevice` for capabilities and resource creation;
  - `CommandContext` for state, targets, clears, and draws;
  - `Swapchain` or presentation surface for acquire/resolve/present;
  - `ResourceUploader` for staging and uploads.
- [ ] Keep the initial split narrow; do not build a general-purpose engine API DATura
  does not need.
- [ ] Define command-context lifetime and whether calls execute immediately or record.
- [ ] Define per-frame transient allocation for immediate vertices and small uploads.
- [ ] Replace frequent `DrawPrimitiveUP`-style paths with a shared dynamic vertex/index
  stream suitable for modern APIs.
- [ ] Define how state queries work if a future backend records commands. Prefer
  application-owned explicit state over querying the driver in production paths.
- [ ] Retain query methods for validation/debugging only if they are expensive or
  unnatural on modern APIs.

Acceptance criteria:

- The interface can map cleanly to both immediate D3D9 and command-buffer-based APIs.
- Immediate UI/effect draws do not require backend-specific upload behavior.

## Priority 11: screenshots, readback, overlays, and platform UI

- [ ] Create a backend-neutral screenshot/readback service with explicit pixel format,
  row pitch, dimensions, and synchronization behavior.
- [ ] Decide whether readback is synchronous for now or returns a pending operation.
- [ ] Use the service for screenshots and render tests instead of native surfaces.
- [ ] Make backbuffer readback behavior explicit when post-processing or MSAA is active.
- [ ] Replace GDI-composited UI with renderer-native draws where feasible.
- [ ] Keep Windows-only overlay support as an optional extension, not a required backend
  method.
- [ ] Define behavior for backends that cannot expose a native DC.
- [ ] Verify overlay ordering relative to resolve and present.

Acceptance criteria:

- Automated captures work without D3D9 surface APIs.
- A backend without GDI support can still render the complete required UI, or reports a
  clearly scoped optional-feature limitation.

## Priority 12: test architecture for multiple backends

### Contract tests

- [ ] Convert `RendererBackendTests` into a reusable contract suite that accepts a
  backend factory.
- [ ] Run the same lifecycle, resource, state, shader, render-target, resize, and
  presentation tests for every implementation.
- [ ] Keep native readback assertions in D3D9 adapter tests, separate from public
  contract assertions.
- [ ] Add negative tests for binding resources from another backend/device.
- [ ] Add allocation-failure and partial-initialization cleanup tests.
- [ ] Add repeated backend switch tests.
- [ ] Add lost-device/out-of-date swapchain tests through injectable failures where
  hardware reproduction is unreliable.
- [ ] Add capability fallback tests.

### Validation backend

A CPU rasterizer is not required immediately. A strict validation backend would still
provide value.

- [ ] Implement a headless validation backend that records resources, state, passes,
  and draw commands without using D3D9.
- [ ] Validate resource ownership, mapping bounds, pass ordering, required bindings,
  primitive counts, and state completeness.
- [ ] Use it for unit tests that currently create hidden D3D9 devices only to inspect
  command behavior.
- [ ] Make unsupported functionality fail loudly with useful diagnostics.

### Visual tests

- [ ] Define backend-independent scene fixtures and camera settings.
- [ ] Capture final resolved images through the shared readback service.
- [ ] Compare structural metrics in addition to pixels: non-background bounds, alpha
  coverage, depth ordering probes, and known sample points.
- [ ] Keep backend-specific references only where rasterization differences make a
  shared tolerance impractical.
- [ ] Include transitions, resize, fullscreen/windowed changes, and AA combinations.

Acceptance criteria:

- A backend author can run one contract suite and know which required behaviors are
  missing.
- Tests no longer rely on native D3D inspection except when testing the D3D9 adapter.

## Priority 13: implement a second backend proof

Do not wait for every optional feature before testing the design with another
implementation. The second backend is what reveals accidental D3D9 assumptions.

Recommended progression:

1. Headless validation backend for contract and call-sequence correctness.
2. A minimal real backend that clears and presents.
3. Screen-space textured UI.
4. Static opaque model rendering.
5. Alpha-tested and transparent models.
6. Camera, fog, lighting, and fixed-function material emulation.
7. Dynamic characters and vegetation.
8. Effects, shadows, water, weather, and post-processing.
9. Full application parity.

For the real backend:

- [ ] Select the target API based on supported platforms and maintenance cost.
- [ ] Implement adapter/device/swapchain discovery and capability reporting.
- [ ] Implement shared resource creation and destruction.
- [ ] Implement state/pass translation and fixed-function emulation shaders.
- [ ] Implement shader-library variants and semantic parameters.
- [ ] Implement upload and dynamic-buffer paths.
- [ ] Implement render targets, depth/stencil, resolve, and readback.
- [ ] Implement presentation, resize, fullscreen/borderless behavior, and recovery.
- [ ] Run the full contract suite at each stage.
- [ ] Compare representative captures against D3D9 and document intentional
  differences.

Acceptance criteria:

- The second backend renders at least one complete representative scene through normal
  application code without invoking D3D9 compatibility bridges.
- Any unsupported feature is represented by capability/fallback logic rather than a
  hidden native dependency.

## Priority 14: remove migration scaffolding

After a second backend reaches required parity:

- [ ] Delete all legacy device-taking overloads.
- [ ] Delete `LegacyD3D9Device()` and `Runtime::D3D9Device()`.
- [ ] Delete production uses of `D3D9Graphics::Context` constructed ad hoc around a
  native device.
- [ ] Delete production uses of `D3D9Graphics::NativeTexture`.
- [ ] Rename neutral modules that still carry `D3D` prefixes.
- [ ] Move D3D9-only implementation files into a clear backend directory/namespace.
- [ ] Ensure project files compile only the selected platform backends.
- [ ] Add a lint/source-boundary check that forbids Direct3D includes and identifiers
  outside D3D9 backend and D3D9 adapter tests.
- [ ] Remove stale comments describing temporary bridges.
- [ ] Consolidate the historical implementation notes in
  `SHARED_RENDERER_INTERFACE.md` into durable architecture documentation.
- [ ] Update contributor documentation with the process for adding resources, state,
  shader variants, passes, capabilities, and backend tests.

Acceptance criteria:

- Direct3D symbols occur only in the D3D9 implementation, D3D9-specific tests, and
  explicitly named platform integration code.
- The portable renderer boundary is enforceable mechanically.

## Suggested implementation milestones

### Milestone A: eliminate the central escape hatch

- Introduce shared lifecycle result and platform surface types.
- Add backend descriptors, capabilities, and factory creation.
- Introduce the application render context.
- Convert `main.cpp` frame orchestration.
- Remove direct `GraphicsDevice()` use from `main.cpp`.

Exit condition: application frame control is backend-neutral even though child
renderers may still use compatibility overloads.

### Milestone B: make model rendering portable

- Convert model buffers, render state, materials, custom textures, and loaders.
- Remove native texture conversions.
- Replace public D3D math/color types in model APIs.
- Preserve material, water, vegetation, animation, and shadow tests.

Exit condition: a validation backend can observe a complete model draw without D3D9.

### Milestone C: migrate environment, effects, and UI

- Convert sky, weather, fog, highlights, collision overlays, home points, world items,
  title UI, nameplates, chat, and main menu.
- Add any missing shared scissor/clip/readback behavior.
- Move shader selection to semantic program IDs.

Exit condition: all production scene and UI rendering is backend-neutral.

### Milestone D: harden ownership and testing

- Adopt explicit resource RAII handles.
- Formalize reset/recreation and backend switching.
- Parameterize contract tests.
- Add the headless validation backend.
- Add source-boundary checks.

Exit condition: accidental D3D9 dependencies are caught automatically.

### Milestone E: second real backend

- Implement clear/present, resources, passes, shaders, and full scene parity in stages.
- Run shared contract and visual suites.
- Remove compatibility scaffolding after parity.

Exit condition: the player can select and use a non-D3D9 backend for normal gameplay.

## File-oriented migration inventory

This list is intended as a working checklist. It groups files by why they still matter;
some files belong to more than one migration track.

### Backend core

- `renderer_interface.h`: remove Windows/D3D result types and native-device bridge.
- `renderer_resources.h`: finish semantics, ownership, capabilities, pass behavior, and
  modern-API considerations.
- `renderer_materials.h`: move toward semantic material inputs and explicit ownership.
- `renderer_backend.h/.cpp`: backend factory, requested/active identity, switching, and
  fallback policy.
- `d3d9_device.h/.cpp`: remain the D3D9 lifecycle implementation; translate shared
  results/capabilities and isolate Windows presentation details.
- `d3d9_graphics.h`: remain the D3D9 resource/command adapter; eventually split if its
  size impedes maintenance.

### Highest native-dependency concentrations

At the time of this inventory, the largest concentrations outside the core D3D9
implementation are `model_renderer.cpp`, `main.cpp`, `d3d_model_render_state.cpp`,
`zone_environment_render_state.cpp`, `npc_nameplate_renderer.cpp`,
`zone_weather_particles.cpp`, `home_point_effect.cpp`, and `d3d_ui_renderer.cpp`.
These should receive focused migration passes rather than scattered substitutions.

### Model and assets

- `model_renderer.cpp/.h`
- `d3d_model_buffers.cpp/.h`
- `d3d_model_render_state.cpp/.h`
- `d3d_bump_mapping.cpp/.h`
- `custom_texture_assets.cpp/.h`
- `noesis_rapi.cpp/.h`
- `scene_model_loader.cpp/.h`
- `player_model_loader.cpp/.h`
- `creation_model_loader.cpp/.h`
- `ffxi_sqle_model_animation.cpp/.h`
- `ffxi_model_lifetime.cpp`
- `model_ff11_texture_handler.h`

### World and effects

- `zone_environment_render_state.cpp/.h`
- `zone_environment_fog.cpp/.h`
- `zone_sky_dome.cpp/.h`
- `zone_weather_particles.cpp/.h`
- `zone_collision_geometry.cpp/.h`
- `zone_object_highlight_renderer.cpp/.h`
- `zone_object_transform.cpp/.h`
- `world_item_renderer.cpp/.h`
- `home_point_effect.cpp/.h`
- `npc_render_geometry.cpp/.h`

### UI and presentation-adjacent code

- `d3d_ui_renderer.cpp/.h`
- `ffxi_title_ui_primitives.cpp/.h`
- `ffxi_title_screen_renderer.cpp/.h`
- `ffxi_nation_selection_renderer.cpp/.h`
- `ffxi_main_menu.cpp/.h`
- `npc_nameplate_renderer.cpp/.h`
- `npc_chat_window.h`
- `ui_backbuffer.h`

### CPU math users that should not depend on D3D headers

- `ffxi_coordinate_frame.h`
- `zone_render_frustum.cpp/.h`
- `zone_object_picker.cpp/.h`
- `zone_door_interaction.cpp/.h`
- `npc_placement.cpp/.h`
- `zone_room_loader.cpp`
- `zone_scene_diagnostics.cpp`
- related query and coordinate-conversion code

These may not issue GPU commands, but their D3D math dependencies prevent a clean
portable build boundary.

## Risks and decisions to make explicitly

### Fixed-function emulation

Modern backends do not provide D3D9 fixed-function texture stages and lighting. Decide
whether the long-term shared API retains fixed-function descriptions that each modern
backend translates to shaders, or whether scene/material code selects semantic shader
programs directly. A pragmatic approach is to retain the descriptions during migration
and compile them into a small, finite shader-variant library.

### State snapshots

D3D9 state blocks make `PipelineSnapshot` convenient. They do not map naturally to
modern explicit APIs. Treat snapshots as a compatibility tool. New pass code should
prefer explicit state setup and locally known restoration.

### Resource reset behavior

D3D9 has managed/default pool behavior that future backends will not share. The common
contract should describe logical resource persistence and recreation, not D3D pool
classes.

### Shader language

Passing arbitrary source and profiles through the shared API leaves portability to the
caller. Semantic shader IDs and per-backend implementations avoid requiring one shader
language to work everywhere.

### Coordinate and depth conventions

D3D, Vulkan, OpenGL, and Metal differ in clip-space depth, framebuffer orientation,
texture origin, and presentation behavior. Document one DATura convention and perform
conversion in projection generation, shader libraries, or backend setup exactly once.

### Threading

The present code appears designed around immediate single-threaded D3D9 use. Do not
promise multithreaded creation or command recording accidentally. State explicit
single-thread rules now and widen them only when a backend needs it.

### Scope control

The goal is a renderer suitable for DATura, not a universal graphics API. New shared
features should be justified by a real DATura render path or a concrete second-backend
requirement.

## Definition of done

The backend abstraction project is complete when:

- [ ] DirectX 9 is one registered backend rather than a constructor hardcoded into the
  runtime.
- [ ] At least one additional backend can be selected and used for normal rendering.
- [ ] Production application, scene, UI, model, loader, and effect code contains no
  D3D9 device/resource types or native conversions.
- [ ] Core renderer headers contain no Windows or DirectX types.
- [ ] Backend capability discovery and failure reasons drive the configuration UI.
- [ ] Requested and active backend settings are represented separately.
- [ ] Shader programs and parameters are semantic and backend-owned.
- [ ] Resource ownership, reset, resize, switching, and shutdown are deterministic.
- [ ] Frame passes have explicit ordering and state contracts.
- [ ] Shared contract tests run against every backend.
- [ ] Representative visual tests pass within documented tolerances.
- [ ] A source-boundary check prevents new native graphics dependencies from leaking
  into portable modules.
- [ ] Legacy D3D9 bridges and overloads have been removed.
- [ ] Architecture and contributor documentation describe the final system rather than
  the migration state.

## Recommended next task

Start with Milestone A: introduce backend descriptors/factory and an application
`RenderContext`, then remove `GraphicsDevice()` calls from the frame loop in
`main.cpp`. That establishes the ownership and call direction needed by every later
migration and prevents new rendering features from deepening the native-device path.
