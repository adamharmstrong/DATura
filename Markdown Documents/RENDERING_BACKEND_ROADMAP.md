# Rendering backend roadmap: remaining work

Updated: 2026-10-16

## Purpose and scope

This document tracks the work remaining before DATura can actually render through
the alternatives listed in **Config → Rendering Backend**. It follows the shared
pipeline setup and presentation migration. It is an implementation roadmap, not a
claim that any additional backend is available.

The immediate requirement remains: **DirectX9 is the only implementation, remains
the default, and must retain its existing rendering behavior.** Changes should be
introduced in independently verifiable stages. Backend availability should change
only after a complete implementation passes the appropriate release checks.

Repository findings below were checked against the current source. Proposed API
names, implementation order, and backend strategies are design recommendations;
they have not been implemented or finalized. Platform SDK/toolchain requirements
must be verified against official documentation when each backend is started.

## 1. What is already complete

The following foundations exist and should be extended rather than recreated:

| Area | Current implementation |
| --- | --- |
| Lifecycle | `RendererBackend::Interface` and `Runtime` cover initialization, display changes, resize, preparation, resolve, present, shutdown, and status. |
| Resources | Shared textures, mesh buffers, shaders, render targets, and vertex layouts have D3D9 implementations. |
| Submission | Shared indexed/immediate drawing, clear, begin-scene, and end-scene operations exist. |
| Materials | Shared opaque/transparent material binding and a material cache exist. |
| Pipeline | Shared depth, alpha, blend, cull, fill, fog, clipping, bias, color-write and multisample settings exist. |
| Other state | Shared stencil, samplers, texture stages, lighting, transforms, viewport, queries, and snapshots exist. |
| Specialized passes | Shadow, home-point, and postprocessing shaders use shared shader resources. |
| Presentation resources | MSAA attachments, backbuffer capture, resolve copies, and postprocessing attachments use shared targets. |
| Windows overlays | An owned `OverlayContext` lease wraps the GDI presentation-surface DC. |
| Config | Backend entries exist; DirectX9 is the available implementation and default. |
| Validation | D3D9 GPU tests exercise resources, state mappings, shaders, presentation, overlays, and resize, with additional scene regression suites. |

See [SHARED_RENDERER_INTERFACE.md](SHARED_RENDERER_INTERFACE.md) for contracts and
the history of completed migrations.

### Important distinction

Calling a shared operation through `D3D9Graphics::Context(device)` is useful
encapsulation, but the caller is still tied to DirectX9. A second backend cannot
replace that context automatically. The next major task is to route those callers
through services supplied by the selected runtime.

Similarly, moving a shader into a shared `Shader` object does not translate its
source, register bindings, or fixed-function assumptions to another graphics API.

## 2. Concrete dependencies still present

These are starting points for the next migration, not an exhaustive list of every
occurrence. Line numbers are intentionally omitted because this code is changing.

| Location | Remaining dependency | Required direction |
| --- | --- | --- |
| `DATura/renderer_backend.cpp` | Runtime constructs `D3D9Device::Runtime` directly. | Introduce backend creation/registration when another implementation is ready. |
| `DATura/renderer_interface.h` | Windows types and `LegacyD3D9Device()` remain in the contract. | Separate platform presentation details and remove the legacy bridge after callers migrate. |
| `DATura/renderer_backend.h` | Exposes `D3D9Device()` to callers. | Replace caller usage with graphics/material/effect services. |
| `DATura/model_renderer.h` | Context contains both a native device and shared services; matrices/colors retain D3D types. | Make shared services sufficient for a complete model draw. |
| `DATura/model_renderer.cpp` | Direct adapter construction and native water-texture binding remain. | Use injected services and shared texture objects throughout. |
| `DATura/d3d_model_render_state.cpp` | Native texture parameters, unwrapping, and a native texture bind remain. | Separate common material decisions from D3D9 resource handling. |
| `DATura/custom_texture_assets.cpp` | Some helper results expose native textures and bind a native base texture. | Return shared textures with explicit ownership and dimensions. |
| `DATura/directional_shadow_map.inl` | Reads D3D9 caps; unwraps a depth surface to inspect its description. | Add semantic capability queries; use shared target descriptions. |
| `DATura/zone_environment_render_state.cpp` | Native water/environment texture bindings remain. | Carry shared texture handles through the entire pass. |
| `DATura/zone_weather_particles.cpp` | Native texture checks/binds and native material-helper arguments remain. | Use shared resource validity and typed material/texture-stage settings. |
| `DATura/home_point_effect.cpp`, `ffxi_nation_selection_renderer.cpp`, `main.cpp` | Some validity checks still unwrap native textures. | Check shared resource availability without assuming its native representation. |
| `DATura/ui_backbuffer.h` | Uses a shared overlay lease, but acquires it from a D3D9 device adapter. | Pass a presentation/overlay service into child-window compositing. |
| `DATura/stdafx.h` | Includes D3D9/compiler headers and link directives globally. | Restrict graphics SDK dependencies to backend-specific targets/files. |
| Rendering helpers across the application | Many entry points accept `IDirect3DDevice9*` and construct adapters locally. | Migrate callers incrementally to injected interfaces. |

Native API calls **inside the D3D9 implementation** are expected. The goal is to
remove those dependencies from shared scene logic, not to eliminate native calls
from the backend that implements them.

## 3. Recommended next stage: finish caller independence

### 3.1 Inject renderer services consistently

- [ ] Define which object supplies graphics, materials, effects, capabilities,
  and presentation services to each render pass.
- [ ] Make `ModelRenderer::Context::graphics` and its material service the normal
  route, not optional alternatives to a native device.
- [ ] Migrate scene setup, model drawing, water, sky, weather, collision overlays,
  object highlighting, nameplates, home-point effects, and UI helpers in small groups.
- [ ] Remove direct `D3D9Graphics::Context(...)` construction from migrated groups.
- [ ] Update callers in the application and standalone test targets together.
- [ ] Keep any temporary legacy overload narrowly scoped and clearly marked.
- [ ] Remove each legacy overload once its callers are gone.

Preserve state-update order, pass order, alpha rules, and material-cache invalidation.
This stage should not introduce a new lighting model or change scene composition.

**Completion criteria:** the migrated scene entry points can execute with an
injected `Graphics`/`Materials` implementation without requiring a D3D9 pointer.
A test double can detect accidental use of the legacy device bridge.

### 3.2 Remove native texture and surface escape paths

- [ ] Change native texture-returning helpers to shared texture references or
  small descriptors containing shared texture, dimensions, and ownership rules.
- [ ] Replace remaining direct texture binds with `Graphics::BindTexture`.
- [ ] Replace `NativeTexture(...)` availability checks with shared checks.
- [ ] Replace native shadow depth-surface descriptions with `RenderTarget::Description()`.
- [ ] Replace native texture-operation arguments with shared enums at helper boundaries.
- [ ] Ensure caches never mix resources created by different backend instances.
- [ ] Confine native unwrapping helpers to the D3D9 implementation and D3D9-specific tests.

**Completion criteria:** shared scene/material/effect code neither receives nor
extracts native textures or surfaces. Existing replacement textures, water,
weather, cutouts, and shadow receiver behavior still pass regression tests.

### 3.3 Neutralize CPU-side types without changing the math

- [ ] Inventory public use of D3D matrix, vector, color, rectangle, and enum types.
- [ ] Introduce or reuse small neutral math/value types where necessary.
- [ ] Preserve the current row-major storage and row-vector multiplication convention.
- [ ] Preserve packed-color channel order and alpha interpretation explicitly.
- [ ] Keep coordinate conversion at well-defined backend boundaries.
- [ ] Test non-symmetric matrices so accidental transposition cannot pass unnoticed.
- [ ] Verify picking, camera projection, billboards, UV animation, shadows, and UI scaling.

Do not rewrite all math for stylistic consistency. Replace types only where they
prevent shared code from compiling or make the contract ambiguous.

## 4. Complete and document backend-independent contracts

### 4.1 Capabilities and availability

The current availability helper identifies the implemented backend. Future
availability needs more detail than one Boolean.

- [ ] Distinguish compiled-in support, OS support, runtime/driver availability,
  successful device creation, and optional feature availability.
- [ ] Define semantic capabilities such as shadow support, usable target formats,
  texture compression formats, sample counts, texture limits, and overlay strategy.
- [ ] Replace the shadow pass's D3D9 shader-version/stencil-cap checks with those
  semantic capabilities; keep D3D9-specific checks inside its provider.
- [ ] Query support for the actual format/sample/depth combination being requested.
- [ ] Record the reason a backend or feature is unavailable for diagnostics and UI.
- [ ] Specify existing fallbacks explicitly: planar shadows, reduced MSAA, shader
  fallbacks, and unprocessed scene output where applicable.

**Completion criteria:** scene code asks whether a rendering feature is supported,
not whether a particular D3D9 capability bit or shader-model value is present.

### 4.2 State queries and snapshots

- [ ] Define exactly which state a snapshot captures; attachments currently have
  separate capture/restoration and must not silently become part of a different contract.
- [ ] Preserve partial-update semantics: omitted fields retain their values.
- [ ] Preserve query failure semantics: caller outputs remain unchanged on failure.
- [ ] Define how non-D3D9 implementations maintain a logical state mirror.
- [ ] Make every shared mutation update that mirror consistently.
- [ ] Define snapshot ownership, lifetime, and behavior across reset/recreation.
- [ ] Test nested saves, helper-induced state changes, and restoration after failed passes.
- [ ] Decide whether invalid multi-field updates are atomic; document and test the
  chosen behavior rather than allowing backends to disagree silently.

A modern backend need not query hardware for every state read or implement a
native state block. It must reproduce the observable shared contract.

### 4.3 Resource lifetime, uploads, and submission

- [ ] Document ownership for every create/capture method and borrowed reference.
- [ ] Define invalidation across resize, full device recreation, and backend change.
- [ ] Retain CPU data or reliable reload paths for resources that must be rebuilt.
- [ ] Define mapping lifetime, repeated-map rejection, offset bounds, and failure behavior.
- [ ] Specify how immediate draws retain/copy caller memory before returning.
- [ ] Add upload staging, transient buffers, deferred destruction, and synchronization
  inside backends that need them; do not expose their details to scene callers.
- [ ] Reject incompatible resources from another backend/device instance predictably.
- [ ] Test buffer reuse and destruction while submitted GPU work is still pending.

The existing D3D9 managed-resource behavior cannot simply be assumed for every
backend. Reproduce its application-visible lifetime guarantees deliberately.

### 4.4 Render targets, readback, and resolves

- [ ] Document supported attachment combinations, format interpretation, sampling,
  copy constraints, and whether a copy also performs an MSAA resolve.
- [ ] Decide whether explicit copy and resolve operations are needed for clarity.
- [ ] Define how depth attachments and sampleable color targets are restored after passes.
- [ ] Keep direct `MapRead` optional; do not require every GPU target to be CPU-lockable.
- [ ] Add explicit staging readback if screenshots, tests, or effects require portable reads.
- [ ] Preserve the existing D3D9 uniform-output fallback until its replacement has
  equivalent evidence; do not copy a synchronous backbuffer read into all backends
  without considering its cost and intended failure-detection role.
- [ ] Test failed target creation, unsupported sample counts, failed resolves, and
  resize while captures/snapshots have held references.

### 4.5 Error reporting

- [ ] Introduce backend-neutral result categories where `HRESULT` currently crosses
  the lifecycle boundary, if cross-platform lifecycle work is undertaken.
- [ ] Retain native error details in diagnostic records for troubleshooting.
- [ ] Distinguish unsupported features from allocation failure, invalid use,
  device loss, and recoverable presentation changes.
- [ ] Audit ignored return values and define where rendering should skip a pass,
  retry, use an existing fallback, or stop safely.
- [ ] Avoid logging every frame for the same persistent failure.

## 5. Separate effects from D3D9 shader representation

### 5.1 Introduce an effect/program selection layer

- [ ] Inventory all shader sources, profiles, entry points, constants, textures,
  vertex layouts, and expected render-target formats.
- [ ] Give common effects semantic identities instead of having scene code request
  a specific D3D9 shader profile or register layout.
- [ ] Cover UI, bump/PBR materials, directional shadow depth and receivers,
  home-point effects, FXAA, and SMAA.
- [ ] Define shared parameter data for each effect and translate it to backend bindings.
- [ ] Preserve current shader source and constants for the D3D9 implementation.
- [ ] Choose and document a source-generation/translation or per-backend shader strategy.
- [ ] Make shader compilation errors identify the backend, effect, and variant.
- [ ] Cache compiled variants with correct invalidation after source/settings changes.

**Completion criteria:** scene code requests an effect and supplies meaningful
parameters without selecting `vs_3_0`, `ps_3_0`, or raw constant-register numbers.

### 5.2 Implement fixed-function compatibility

Shared texture-stage and lighting interfaces currently describe behavior that
D3D9 can execute directly. Other implementations need an intentional equivalent.

- [ ] Inventory the combinations actually used, including null-shader bindings.
- [ ] Implement model, position/color, and screen/color/UV vertex processing.
- [ ] Reproduce texture arguments, stage operations, stage disable behavior, and UV transforms.
- [ ] Reproduce alpha test/reference, fog, lighting, material sources, and vertex colors.
- [ ] Preserve the interaction of fixed-function settings with custom shader passes.
- [ ] Generate/cache only required shader variants to control compilation cost.
- [ ] Specify handling of unsupported combinations explicitly.
- [ ] Compare cutout edges, translucent ordering, water, sky, and untextured overlays.

Binding a null shader must retain its existing semantic meaning. It cannot simply
mean “no usable program bound” on another implementation.

### 5.3 Coordinate and rasterization compatibility

- [ ] Define the shared projection/depth convention, winding, viewport orientation,
  texture origin, screen-space pixel centers, and color-space expectations.
- [ ] Apply required backend conversions in one documented place.
- [ ] Verify fullscreen quad UVs and offsets independently from world geometry.
- [ ] Compare depth bias and shadow receiver behavior using real layered geometry.
- [ ] Use exact comparisons for deterministic flat regions and documented tolerances
  for filtering, antialiasing, and other hardware-dependent edges.

## 6. Presentation and UI portability

### 6.1 Finish service routing on Windows

- [ ] Pass the runtime's presentation/overlay service into child-window UI composition.
- [ ] Remove D3D9 acquisition from `ui_backbuffer.h` callers.
- [ ] Ensure all overlay leases end before drawing, resolving, presenting, or resetting.
- [ ] Preserve the order: scene → resolve/postprocess → UI overlays → present.
- [ ] Verify title, roster, nation/job selection, maps, chat, choices, debug panels,
  menus, and developer console at different render/window sizes.

### 6.2 Choose a portable overlay strategy

The current GDI lease is explicitly a Windows bridge, not a universal graphics API.

- [ ] Evaluate rendering GDI output to a CPU bitmap and uploading/compositing it on Windows.
- [ ] Evaluate moving relevant text, images, and primitives into shared GPU UI drawing.
- [ ] Preserve fonts, transparency, clipping, ordering, and coordinate mapping.
- [ ] Test resize, DPI changes, focus changes, and repeated UI open/close cycles.
- [ ] Choose a non-GDI implementation for any future macOS UI path.

### 6.3 Presentation lifecycle

- [ ] Define restart-required backend changes as the initial behavior unless there
  is a concrete requirement for switching live.
- [ ] Separate window creation/event handling from graphics surface/swapchain ownership.
- [ ] Define minimized/zero-size handling, resize, fullscreen transitions, and recovery.
- [ ] Define present synchronization and user-facing options only after backend behavior is known.
- [ ] Preserve native D3D9 device-loss/reset behavior while other implementations are added.
- [ ] Test repeated shutdown/reinitialize, failed startup, and recovery without stale resources.

## 7. Implement actual alternative backends

The existing settings enumerate DirectX8, DirectX9, DirectX11, DirectX12, OpenGL,
Vulkan, and Metal. DirectX12 is included here because it is already in the source,
even though it was not in the original example list.

Every new backend needs resources, submission, state, effects, presentation, UI,
diagnostics, packaging, and tests. Showing a triangle is an early milestone, not
completion of DATura support.

### 7.1 Suggested order

1. Finish D3D9 caller independence and lock down shared contracts.
2. Implement one programmable Windows backend end to end; DirectX11 is a practical
   project option because it separates renderer work from a simultaneous OS port.
3. Use that implementation to validate fixed-function emulation and effect contracts.
4. Add OpenGL or Vulkan according to the desired platform coverage and available test hardware.
5. Pursue DirectX8 as a separate compatibility/authenticity effort.
6. Add Metal alongside the required macOS platform work.
7. Implement DirectX12 if its benefits justify maintaining another explicit backend.

This is a proposed sequence, not a commitment or estimate. DirectX8 may be
prioritized earlier if authentic-client comparison becomes the main objective.

### 7.2 DirectX11

- [ ] Implement device/context, presentation targets, resize, and recovery.
- [ ] Implement textures, buffers, layouts, samplers, state objects, and resource views.
- [ ] Implement effect shaders and fixed-function compatibility variants.
- [ ] Translate shared constants and texture stages to shader/resource bindings.
- [ ] Handle attachment/texture binding hazards and resolve behavior explicitly.
- [ ] Implement logical queries/snapshots and dynamic/immediate upload paths.
- [ ] Composite Windows overlays using the chosen supported path.
- [ ] Pass complete scene and lifecycle checks before enabling its Config entry.

### 7.3 OpenGL

- [ ] Choose the minimum supported version/profile and platform context strategy.
- [ ] Implement context creation, function loading, buffer swap, and capability discovery.
- [ ] Implement shared resources, framebuffer attachments, state, and shader programs.
- [ ] Provide effect and fixed-function compatibility programs for the chosen profile.
- [ ] Verify projection depth, winding, texture origin, screen-space alignment, and pixel packing.
- [ ] Validate compressed textures and supply tested decoding/upload fallbacks where needed.
- [ ] Test context lifetime, thread ownership, resize, and presentation on supported platforms.
- [ ] Record vendor/driver differences and actionable startup failures.

### 7.4 Vulkan

- [ ] Define the supported API/feature baseline and validation configuration.
- [ ] Implement instance/device selection, queues, presentation surface, and swapchain.
- [ ] Implement command recording, submission, fences/semaphores, and frames in flight.
- [ ] Implement allocation, staging uploads, descriptors, pipelines, and shader modules.
- [ ] Translate logical state changes into pipeline/binding choices without excessive churn.
- [ ] Implement image layout transitions and attachment/resolve synchronization.
- [ ] Defer resource destruction until submitted work has completed.
- [ ] Handle swapchain recreation, minimized windows, and device failure.
- [ ] Validate correctness before tuning batching and pipeline caches.

### 7.5 Metal and macOS

- [ ] Define the supported macOS/hardware/toolchain baseline.
- [ ] Implement device, command queues/buffers, drawable presentation, textures,
  buffers, samplers, pipeline/depth-stencil state, and shader libraries.
- [ ] Implement effects, fixed-function compatibility, uploads, resolves, and readback.
- [ ] Integrate a macOS window/surface and event lifecycle.
- [ ] Replace or adapt Windows UI/GDI dependencies needed to reach playable rendering.
- [ ] Audit asset paths/discovery, dialogs, input, cursor behavior, audio, and build scripts.
- [ ] Define how users provide locally installed assets without depending on Windows installation discovery.
- [ ] Add macOS builds and real-device validation.

A Metal renderer alone does not make the existing Windows application a macOS
application. Track platform-porting tasks separately so renderer progress is not
mistaken for completed macOS support.

### 7.6 DirectX8 and the authenticity objective

- [ ] Define what is being compared with the official client: materials, alpha,
  lighting, fog, textures, depth behavior, and presentation are separate concerns.
- [ ] Capture controlled reference scenes using matching assets, camera, time,
  weather, settings, and resolution where possible.
- [ ] Determine the usable API/toolchain/runtime and executable architecture strategy.
- [ ] Inventory which current DATura effects fit the supported feature set.
- [ ] Define faithful fallbacks or explicit feature exclusions for effects that do not.
- [ ] Implement resources, state, submission, presentation, and recovery.
- [ ] Keep enhancement settings distinct from an authenticity preset if one is added.
- [ ] Compare the result with both the current D3D9 baseline and official-client references.

Using the same API is not sufficient to guarantee the same image. The rendering
algorithms, state choices, content interpretation, and settings must also match.
Do not silently alter DirectX9's current appearance to accommodate DirectX8 limits.

### 7.7 DirectX12

- [ ] Confirm the product reason for maintaining this additional backend.
- [ ] Implement device/queue/swapchain, command allocators/lists, fences, and recovery.
- [ ] Implement descriptors, root signatures, pipelines, memory/upload management,
  resource transitions, resolves, and deferred destruction.
- [ ] Reuse common effect semantics while supplying backend-specific bindings.
- [ ] Validate all scene/UI behavior and profile submission overhead and memory use.

## 8. Backend selection, settings, and recovery

- [ ] Add a backend factory rather than always constructing the D3D9 implementation.
- [ ] Preserve persisted backend identifiers; do not change enum numbering casually.
- [ ] Centralize backend metadata: stable ID, display name, platform support,
  compiled availability, runtime status, and failure reason.
- [ ] Keep unavailable dropdown entries visible but disabled, with useful explanation.
- [ ] Keep DirectX9 selected by default until a separate product decision changes that.
- [ ] Show a restart requirement before applying a supported backend change.
- [ ] Define safe recovery when a saved backend cannot initialize on another machine.
- [ ] Report any fallback honestly; show the backend actually running.
- [ ] Make AA/shadow/other feature availability follow the active backend's capabilities.
- [ ] Test invalid settings, removed implementations, failed initialization, and repeated changes.

Live backend switching is optional later work. If implemented, it must release
every resource and cache tied to the old device, rebuild from CPU data, preserve
application state, and roll back cleanly after failure. Restart-based switching
avoids making that complexity a prerequisite for the first alternative backend.

## 9. Build boundaries and packaging

- [ ] Move shared renderer headers into a dependency boundary that compiles without D3D headers.
- [ ] Restrict D3D9 headers, native casts, compiler linkage, and adapter definitions
  to D3D9 implementation targets and intentionally native validation tests.
- [ ] Reduce reliance on the global Windows/D3D precompiled header in shared code.
- [ ] Preserve standalone parser/test builds currently served by the inline adapter.
- [ ] Add build-time backend selection without requiring every SDK on every platform.
- [ ] Package shader assets/libraries and runtime dependencies consistently.
- [ ] Verify required third-party dependency licenses and distribution conditions.
- [ ] Add clean-machine startup checks and actionable missing-runtime messages.
- [ ] Add a compile check proving shared renderer/scene headers do not need D3D9.

## 10. Validation and regression plan

### 10.1 Existing evidence

The latest completed migration recorded:

- Release/x64 application build success.
- Shared resource/state/shader/layout/query checks passing on D3D9.
- No-AA, MSAA, FXAA, and SMAA combinations passing frame, output, state restoration,
  GDI overlay/read mapping, presentation, and resize checks.
- Material/depth/fog/shadow suite: **2,661 assertions, zero failures**.
- Installed-asset home-point and focused nameplate GPU checks passing.
- Synthetic water output matching its independent reference exactly.

These are prior execution results, not tests rerun for this documentation change.
The nameplate run used `--render-only`; separate Config/quest prechecks were not
included. Installed-DAT water checks were also not run in that latest pass.
Hidden-window GPU tests are not evidence that every interactive device-loss or
fullscreen scenario has been covered.

### 10.2 Make tests reusable across implementations

- [ ] Separate shared conformance tests from native D3D9 inspection tests.
- [ ] Parameterize applicable scene tests by backend instead of constructing D3D9 directly.
- [ ] Retain native mapping tests as D3D9-specific tests; do not weaken them to fit another API.
- [ ] Test unsupported resource formats, allocation failures, bad descriptors,
  incompatible resources, and unchanged query outputs after failures.
- [ ] Validate frame order, restoration, and ownership with a recording test implementation.
- [ ] Run existing material, home-point, water, sky, weather, vegetation, player
  animation, nameplate, coordinate, and bitmap-font checks where relevant.
- [ ] Record unsupported/skipped cases distinctly from passing tests.

### 10.3 Visual scene matrix

| Scene or feature | Required comparisons |
| --- | --- |
| Static zone geometry | Placement, winding, UVs, mipmapping, depth, and material colors. |
| Cutout vegetation | Alpha thresholds, mip edges, animation, depth writes, and sorting. |
| Actors and NPCs | Animation, textures, material layers, lighting, equipment, and shadows. |
| Water | UV scrolling, compositing, fog, reflection-related effects, and occlusion. |
| Sky and weather | Layer order, authored colors, time changes, particles, and transparency. |
| Shadows | Planar fallback, directional depth/receiver passes, stencil, bias, and layered geometry. |
| Home points | Idle/activation effects, opacity, textures, and state restoration. |
| UI and nameplates | Fonts, icons, tint, clipping, scaling, ordering, and depth/occlusion. |
| Postprocessing | Flat-region correctness, edges, orientation, fallback behavior, and overlay ordering. |
| Debug rendering | Collision mesh, highlights, wireframe, maps, and console/UI overlays. |

Use fixed cameras, known asset identifiers, stable time/weather inputs, documented
settings, and repeatable captures. Record tolerances and explain them rather than
accepting broad differences to make a new backend pass.

### 10.4 Lifecycle and hardware matrix

- [ ] Startup/shutdown and repeated initialization.
- [ ] Unsupported backend, missing runtime, and failed device creation.
- [ ] Resize, minimize/restore, display-mode changes, and supported fullscreen modes.
- [ ] DPI and render-resolution changes with GDI/shared UI alignment.
- [ ] Alt-tab/focus changes and backend-specific device/surface recovery.
- [ ] Feature toggles, AA changes, postprocess changes, and resource recreation.
- [ ] Multiple GPU vendors and integrated/discrete hardware where available.
- [ ] Long sessions with scene changes, asset reloads, and repeated effect activation.
- [ ] Memory/resource leak checks and validation/debug-layer checks.

### 10.5 Performance acceptance

- [ ] Record the current D3D9 CPU/GPU frame-time and memory baseline in representative scenes.
- [ ] Measure shader compilation stalls, pipeline/state churn, uploads, and readback stalls.
- [ ] Track transient-buffer growth, descriptor/resource counts, and deferred destruction.
- [ ] Set measured acceptance criteria per backend and supported hardware tier.
- [ ] Optimize after correctness; preserve a regression baseline throughout optimization.

## 11. Suggested deliverable-sized milestones

| Milestone | Deliverable | Exit condition |
| --- | --- | --- |
| A: Shared caller routing | Scene/effect helpers receive shared services. | Migrated paths run without a native-device argument; D3D9 regressions pass. |
| B: Resource/capability cleanup | Native texture/surface bridges removed from shared paths; semantic caps added. | Shared scene code no longer unwraps resources or reads D3D9 capability structs. |
| C: Contract/build boundary | Neutral types, documented state/lifetime rules, isolated SDK dependencies. | Shared headers compile without D3D9; failure/lifetime tests pass. |
| D: Effects/UI boundary | Effect library and portable overlay composition strategy. | Scene code no longer selects native shader profiles; complete UI can be composited. |
| E: First new backend foundation | Device, targets, resources, basic draws, presentation. | Conformance and simple pixel tests pass; backend remains experimental/disabled for normal use. |
| F: Full scene parity | Materials, fixed-function compatibility, specialized passes, UI. | Scene and lifecycle matrix passes with documented limits. |
| G: Config release | Availability probing, persistence, restart/recovery, packaging. | Supported users can select the backend and recover from startup failure. |
| H: Additional backends | Repeat E–G using established contracts. | Each backend independently satisfies its release criteria. |

The best immediate follow-up is **Milestone A**, starting with the model/material
path and its texture helpers, followed by the shadow capability/surface queries.
That removes concrete blockers without changing the rendering algorithms or
prematurely enabling another Config option.

## 12. Definition of done for an enabled backend

A backend is ready for normal selection only when all of the following are true:

- [ ] It implements the shared services needed by DATura's complete rendering path.
- [ ] Shared scene code does not accidentally call D3D9 while it is active.
- [ ] It renders the supported materials, effects, UI, and presentation modes correctly.
- [ ] Unsupported features have explicit, tested behavior and honest UI reporting.
- [ ] Its resources survive supported lifecycle changes or recreate correctly.
- [ ] Settings select the implementation that actually runs.
- [ ] Startup failure leaves the user a documented recovery path.
- [ ] Tests, reference captures, driver/platform coverage, and known limitations are recorded.
- [ ] Packaging includes the required runtime/shader pieces.
- [ ] DirectX9 still passes its regression suite.

Future conveniences such as live switching, additional advanced rendering
features, or a broad renderer rewrite are not required to reach this definition.
