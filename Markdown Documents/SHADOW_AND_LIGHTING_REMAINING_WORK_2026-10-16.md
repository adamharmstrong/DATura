# DATura shadow and lighting: remaining work

**Snapshot date:** October 16, 2026.  
**Scope:** The shadow and lighting work discussed in this chat, including its renderer dependencies. This is not a backlog for all DATura gameplay features.  
**Purpose:** Give the next developer enough context to continue without undoing the stability fixes, overstating what has been validated, or starting an unrelated graphics-backend rewrite.

This document records remaining work. It does not claim that the latest build eliminates every possible instance of flickering. The latest fixes address specific reproduced failures; live confirmation in the user's original scenes and a wider hardware test matrix remain outstanding.

## 1. Recommended order

| Priority | Work | Main reason |
|---|---|---|
| P0 | Reproduce any remaining artifacts at real window sizes and record deterministic captures | Avoid another fix based only on small synthetic fixtures |
| P0 | Measure CPU, GPU, and memory costs of the current shadow passes | No interactive performance budget has been established |
| P0 | Make resource failure and fallback behavior reliable | Late failures can currently leave a frame without either shadow implementation |
| P0 | Make cutout silhouettes consistent with the actual displayed textures | Replacement-texture alpha may differ from the texture used by shadow passes |
| P1 | Reconcile shadow settings and add useful diagnostics | Several controls still govern only the planar fallback |
| P1 | Separate sunlight, ambient, local lights, and emissive contributions | The current sunlight-shadow composite darkens all existing scene color |
| P1 | Verify point-light units and replace center-only influence selection | Existing lamp illumination can be too limited or miss large meshes |
| P1 | Improve light-volume bounds, bias controls, and caster continuity | A single fixed volume and hard selection boundaries still have limitations |
| P2 | Add stable cascaded sunlight shadows if measured coverage requires them | Improve near detail and distant coverage together |
| P2 | Add shadowed local lights after local lighting is correct | Match the visible behavior of actual lamps and torches |
| P2 | Define transparent, water, and special-effect shadow behavior | These need material-specific rules |
| P3 | Consider temporal filtering, advanced soft shadows, sun shafts, or ray tracing | Optional quality work after correctness and performance are established |
| Separate track | Continue the shared renderer interface and evaluate another backend | A graphics API change does not implement the lighting features above |

P0 means the work needed to understand and trust the current implementation. P1 improves the lighting model and addresses known limitations. P2 and P3 are proposed extensions, not features already promised for the current D3D9 pass.

## 2. What is already implemented

Do not mistake the following completed work for remaining tasks:

| Area | Current implementation |
|---|---|
| Directional shadow map | One 2048 × 2048 R32F color target containing light-space depth, with a depth/stencil attachment |
| Activation | Outdoor zones, Dynamic Shadows lighting, object shadows enabled, positive shadow distance, and sufficient device capabilities |
| Fallback | The planar path remains for unsupported conditions and indoor zones; late runtime failures still need work |
| Map anchor | The orbit focus, with a camera-position fallback for callers without an explicit focus |
| Map stability | Light axes built at the origin and translation snapped to shadow texels |
| Receiver selection | A private camera-depth pass selects the nearest opaque surface using linear view depth |
| Layered terrain | Unbiased receiver depth and an EQUAL comparison prevent hidden lower layers from taking ownership of the visible surface |
| Repeated composition | Stencil coverage in the private receiver surface limits shadow application to once per pixel |
| Filtering | Four bilinear comparison samples, implemented with sixteen point depth samples |
| Sloped surfaces | Receiver-plane depth correction at each filter tap |
| Resolution stability | UV derivatives are normalized before testing the conditioning of the receiver-plane calculation |
| Actor animation | Submitted poses are copied so shared NPC buffers can be animated again without changing an earlier actor's shadow pose |
| Offscreen NPC casters | Eligible NPCs can submit shadow-only geometry when their nameplates leave the camera view |
| NPC budget | Distance to orbit focus, stable entity-ID tie breaking, and a zone-cell visibility filter |
| Sun timing | Terrain, actors, and shadows share one fractional game-minute value per rendered frame |
| Time overrides | Explicit time overrides remain exact |
| Fog | Fog reduces the shadow-composite contribution |
| Device lifetime | Shadow resources have reset/shutdown release hooks and lazy recreation |

The renderer interface has also advanced since the initial shadow implementation. Resource, shader, render-target, material, and pipeline operations now have shared interfaces. Do not describe this work as merely a device-lifecycle wrapper. There is still only one production backend, and parts of the shadow code retain D3D9-specific types, capability queries, and adapter access.

### 2.1 Reproduced failures that have already been corrected

1. **Repeated darkening on coplanar surfaces.** Two receivers could apply a 50% shadow twice, producing approximately 75% darkness.
2. **Hidden stacked layers taking receiver ownership.** A biased depth replay allowed a lower detail layer to darken the visible upper surface. The failure depended on layer order and camera position.
3. **Grazing-angle self-shadow stripes.** Neighboring shadow samples used one receiver depth instead of the depth of the receiver plane at each sample location.
4. **Resolution-dependent slope correction.** A raw derivative determinant crossed an absolute cutoff at large viewports. An unobstructed plane stayed lit at 128² but developed 409,600 falsely shadowed interior pixels per tested frame at 1920². The corrected fixture stays lit across the tested camera positions.
5. **Whole-minute sun movement.** Automatic direction advanced in discrete game-minute steps, roughly every 2.4 real seconds under the Vana'diel clock. The automatic direction now receives fractional minutes.
6. **Camera-hidden NPC casters disappearing.** Nameplate culling could remove an NPC from the shadow map. Shadow-only submission now preserves eligible offscreen casters without adding them to the camera color or receiver passes.
7. **Sun-motion texel crawl scaled by world position.** The light basis rotated about the world origin, so with the continuously moving automatic sun the shadow-map texel grid slid under casters at (distance from origin) × (sun angular rate): ~6–7 texels/s in Bastok Markets (~280 units out) with a stationary camera. Light now pivots on a persistent anchor that follows the focus in whole-texel steps (`directional_shadow_map.inl`). Regression: `MaterialRenderingTests --shadow-bastok-temporal` (worst popped pixels per 60 Hz frame 33 → 2 at 640²).
8. **Light camera on the wrong side of the world.** Production light directions point toward the light (Y-down scene: a sun above has negative Y, as the planar fallback assumes), but the map placed its eye at `-direction`, underneath the world. The ground therefore shadowed actors and walls standing on it, and actors cast nothing. The eye now sits along `+direction`; synthetic fixtures that had encoded the reversed convention were negated. Regression: `MaterialRenderingTests --shadow-sun-side` (roof over ground at 10:00: ground behind roof 255 → 128 and roof top stays 255; before the fix the ground stayed 255 and the roof top went to 128).
9. **Fixed-function sun lit the side facing away from the sun, and actors faceted under self-shadowing.** `ApplyDynamicLighting` passed the toward-the-light vector as D3D's travel direction, so actors and upward-facing ground were lit from the anti-sun side (character crown normals measure mean Y = -0.87, i.e. outward). It now negates for D3D (fill light swapped to match). Separately, the 2048 map (~0.08 units per texel) cannot resolve face-scale detail, so actor receivers self-shadowed per triangle. Actor receivers now offset their shadow lookup two texels along the normal and fade occlusion by the smooth vertex N·L. Regressions: `PlayerAnimationTests --actor-shading` (unoccluded face view darkened by the map: 2,855/15,126 px at 10:00/15:00 before, 455/122 after) and a D3D light-direction check in `--shadow-sun-side`. With sunlit ground the Bastok temporal sun-motion check measures 109 worst / 1,976 total popped pixels for the old origin pivot versus 12 / 136 now; its limit is 20.
10. **Actor lighting creased along triangle edges from per-vertex saturation.** Actor vertices are white, and Bastok's authored model light reaches ambient + sun ≈ 1.9 (10:00: main `c4c2ad` × 1.63, ambient `a7a8a6`), so D3D clamps each lit vertex to 1.0 and interpolation bends exactly at triangle edges. Actors (FFXI texture shader, not PBR) now light at half intensity through a 0.5 material and the shader doubles it with the same cap applied per pixel (`LightParams` c3); zone geometry keeps the per-vertex path. At night the automatic main light now uses the source above the horizon, matching the shadows, instead of lighting undersides. Regressions: `TestActorPerPixelLightClamp` (midpoint 102 per-vertex vs 128 per-pixel) and a night light-direction check in `--shadow-sun-side`. Split normals were measured (172 of 937 positions differ by >20°) but welding them made little visible difference, so they were left authored.

These regressions must remain in the test suite. In particular, do not restore receiver depth offsets as a general flicker remedy; they were the cause of the hidden-layer ownership failure.

### 2.2 Current evidence and its limits

The last recorded full material/shadow run associated with this work reported **2,687 assertions and zero failures**. Its log is in `artifacts/shadow-temporal-agent/full.log`.

That result includes synthetic material tests, layered ground, software and hardware vertex processing, supported 4× MSAA cases, high-resolution grazing surfaces, temporal edge cases, shadow-only actor submission, and an installed Bastok Markets DAT comparison.

Important limitations:

- The assertion count is a historical result, not a permanent property of the test executable. Concurrent work can add cases or change the count.
- Software and hardware vertex-processing tests still use the HAL on the same installed GPU. They are not a vendor or driver compatibility matrix.
- The installed Bastok comparison is a small static render. A changed-pixel count establishes that the pass affects the scene, not that every affected pixel is correct.
- The 1920² regression checks an unobstructed grazing plane. It does not establish full-resolution correctness for every animated character, texture replacement, MSAA mode, or real-zone shadow edge.
- Native test readback and capture writing distort timing. Their total runtime is not a gameplay performance measurement.
- The latest application built successfully. Live visual confirmation in the original reported camera views is still required.

## 3. Invariants to preserve

Any subsequent implementation must preserve these behaviors:

- Select the actual nearest visible receiver, independently of submesh submission order.
- Apply a shadow contribution only once to a covered sample/pixel.
- Preserve the original scene depth and stencil contents.
- Use the same receiver depth expression in the private prepass and composite.
- Keep the receiver depth comparison free of the old constant and slope offsets.
- Match attachment dimensions and multisampling when switching render targets.
- Keep camera-hidden shadow-only submissions out of camera color and receiver depth.
- Keep per-instance actor poses stable when models share animated buffers.
- Keep explicit hidden objects hidden.
- Maintain coherent scene/native-DAT coordinates, world mirroring, and light transforms.
- Use one frame light time for terrain, actors, and directional shadows.
- Retain exact manual time overrides for deterministic captures.
- Preserve the existing FFXI alpha rules, including expanded vertex alpha and DXT3 behavior.
- Restore graphics state and attachments after success and after failure.
- Release backend-owned resources before backend shutdown.

## 4. P0: establish a reproducible visual baseline

### R01 — Capture the actual remaining flicker

**Status:** Outstanding live validation. Several causes have been fixed, but any residual symptom needs to be classified before further tuning.

**Work:**

- Record the zone, player coordinates, camera target, yaw, pitch, distance, internal render resolution, display resolution, AA mode, lighting mode, shadow settings, and texture-replacement settings.
- Identify the executable actually launched. An old process or the normal output directory may differ from the linked artifact build.
- Capture separate sequences for a stationary camera, camera rotation, camera translation, player movement, and moving sun.
- Freeze time for camera tests. Freeze the camera for sun-motion tests.
- Repeat at the Bastok locations from the earlier screenshots: layered pavement, low walls, home-point areas, NPCs, market structures, trees, and lamps.
- Record the same sequence with shadows disabled while keeping the lighting tier otherwise constant. Texture shimmer, base-geometry depth fighting, vegetation alpha aliasing, and lighting changes can resemble shadow flicker.
- Save raw frames and settings with the capture. Video alone can hide small changes through compression.
- If the failure appears only in gameplay, create a fixture with the actual zone geometry or a reduced extraction rather than another unrelated plane.

**Acceptance:** A developer can reproduce the same artifact from recorded settings and distinguish a shadow-specific failure from ordinary scene aliasing. A proposed fix has a failing-before and passing-after case.

### R02 — Establish performance and memory budgets

**Status:** Unmeasured in interactive play.

The current path renders light depth, camera receiver depth, and a receiver composite. Actor pose snapshots also have CPU and allocation costs.

**Instrument:**

| Measurement | Why it matters |
|---|---|
| CPU caster selection and submission time | Separate visibility work from actual drawing |
| Actor snapshot bytes, allocations, and copy time | Shared-buffer correctness currently requires per-submission copies |
| Light-depth GPU time | Quantify caster and shadow-map cost |
| Receiver-depth GPU time | Measure the additional nearest-surface pass |
| Composite GPU time | Quantify filtering, coverage, and screen-resolution cost |
| Draw calls and triangles per pass | Locate batching and culling opportunities |
| Selected, rejected, and submitted casters | Reveal wasted budget and geometry work |
| Shadow allocation/reallocation counts | Detect resize/reset churn |
| Frame-time p50, p95, and p99 | Expose intermittent stalls hidden by average FPS |

Nominal attachment memory is about **32 MiB** for 2048² R32F plus 2048² D24S8, before the private receiver surface and driver overhead. Receiver depth adds roughly four bytes per pixel times the sample count. Measure actual allocations rather than treating this estimate as a complete GPU-memory report.

Benchmark with Dynamic Shadows lighting held constant and compare shadow flags off versus selected shadow flags on. Turning all lighting off changes additional work and is not a clean shadow-only baseline.

**Acceptance:** Define target hardware, target frame time, an allowed shadow budget, and representative worst-case scenes. Then optimize against measurements. Do not promise a particular FPS or claim that a larger map is inexpensive before this work.

## 5. P0: close correctness and failure-path gaps

### R03 — Preflight resources and make fallback explicit

**Known implementation gap:** The active map path suppresses planar shadows before all later receiver resources and attachment operations have succeeded. Receiver-depth allocation, state capture, or binding can fail afterward. `DrawDirectionalShadowFrame()` returns a result, but its application call does not currently provide a complete late-failure fallback strategy.

**Work:**

- Preflight the attachments and shaders required for the entire frame before committing to map shadows where practical.
- Define behavior for unsupported capability, shader failure, allocation failure, capture failure, bind failure, and device loss as separate cases.
- Ensure partial initialization releases partial resources.
- Define whether a failure is permanent until reset, recoverable next frame, or retried with a lower quality level.
- Avoid an allocation retry storm while also avoiding an unexplained permanent disable after a transient failure.
- Make the selected path and failure reason available in diagnostics.
- If fallback requires an earlier scene stage, make that scheduling dependency explicit instead of attempting a late projection over completed geometry.
- Restore attachments and state on every early return.

**Tests:** Inject failures into creation, capture, and bind operations; exercise device reset and recreation; verify both state restoration and a deliberate visible outcome.

**Acceptance:** A failed shadow frame does not silently lose both shadow methods, leak resources, corrupt later passes, or get stuck without an explanation.

### R04 — Use the same cutout silhouette as the color pass

**Known implementation mismatch to investigate:** Shadow mesh binding samples the resolved original mesh texture, while ordinary material binding can resolve a replacement texture through `CustomTextureAssets`. A replacement with different alpha can produce a different visible silhouette. The visible impact needs a dedicated fixture.

**Work:**

- Resolve the effective texture, alpha expansion, vertex alpha, and cutoff consistently for color, light depth, and receiver depth.
- Define whether every replacement is allowed to change silhouette or whether a material policy constrains it.
- Test a mixed opaque/transparent pattern, not only an entirely transparent texture.
- Add shadow-specific DXT3 cutout coverage. General material-alpha tests alone do not validate shadow silhouettes.
- Test two-sided foliage, fences, thin props, mirrored worlds, and animated cutout meshes.
- Check sampler and mip behavior on alpha-tested geometry at distance.

**Acceptance:** A visible hole remains a hole in the shadow, and an opaque part remains an occluder, across original and replacement textures. No solid rectangle should appear because a shadow pass used a different alpha source.

### R05 — Make settings match the active shadow implementation

**Known inconsistencies:**

- The UI accepts shadow distance values up to 9999, while the directional map clamps its half-width to 16–160 scene units.
- NPC selection derives a radius from the raw setting, which can disagree with the effective map coverage.
- Zero-distance behavior differs between parts of the legacy and map paths.
- Several object-size, receiver-update, ground-like-object, alpha-object, and projection-length controls apply to planar shadows but not to directional maps.
- Existing performance/debug flags do not provide a complete directional-map diagnostic view.

**Work:**

- Define distance as an actual coverage parameter and use the effective value consistently in selection, rendering, UI, and diagnostics.
- Decide what zero means and use one documented meaning.
- Clearly identify fallback-only controls; disable or explain controls that do not affect the active path.
- Add quality presets only after defining their actual map resolution, distance, filtering, and budget behavior.
- Expose the active path, effective map bounds, resource sizes, selected casters, and receiver ownership.
- Add views for raw light depth, receiver depth, coverage, shadow visibility, and map boundaries.
- Keep renderer implementation details in diagnostics rather than making ordinary users manage low-level bias constants.

**Acceptance:** A user can tell which implementation is active, and every enabled control has the documented effect.

## 6. P1: improve the lighting model

### R06 — Separate direct light from ambient, local lights, and emission

**Known limitation:** The current composite multiplies existing scene color by shadow visibility. It therefore dims ambient illumination, point-light illumination, and any emissive-looking contribution along with sunlight. Fog attenuation reduces one artifact but does not make the lighting decomposition correct.

**Required direction:** Compute independently controllable contributions, conceptually:

```text
surface color = ambient contribution
              + sunlight contribution × sunlight visibility
              + sum(local-light contribution × local-light visibility)
              + emissive contribution
```

This is a decomposition requirement, not a requirement to replace FFXI's artistic lighting with a particular physically based model.

**Work:**

- Preserve authored environment colors, secondary/fill lighting, vertex colors, and the intended appearance of classic materials.
- Define how sun/moon source selection and shadow strength work around dawn and dusk.
- Apply fog at a consistent stage after the relevant lighting contributions.
- Keep emissive surfaces visible without requiring them to illuminate neighbors unless an actual local light exists.
- Integrate optional PBR materials with actual scene light and view inputs. The current PBR experiment includes fixed light/view assumptions that need review before claiming consistent lighting.
- Verify gamma/sRGB behavior before tuning intensity values by appearance.

**Acceptance:** A lamp-lit area remains locally lit inside a sunlight shadow; ambient illumination does not disappear with direct sunlight; emissive surfaces remain emissive; classic-material captures retain their intended character.

### R07 — Verify authored point-light units and influence

Bastok Markets has **31 identified authored point-light generators**, `pl01`–`pl31`, in `ROM/1/35.DAT` under `t_ba/mode/pl`. The exported coordinates and authored values are in `artifacts/zone-diagnostics/bastok-markets-point-lights.csv`.

**Current behavior:** `ApplyAuthoredPointLights()` selects up to six fixed-function lights. Range is calculated as authored range times range ratio divided by 16. Selection is based on distance from a batch or mesh center.

**Work:**

- Verify the range, power, ratio, color, position, and attenuation units against decoded data and a known reference. The `/16` conversion is suspicious enough to investigate, but it must not be removed merely because it produces a small radius.
- Match generator positions to actual lamp meshes and distinguish the light emitter, fixture geometry, glow sprite, and emissive material.
- Verify scene/native coordinates and mirroring for lights and geometry together.
- Audit generator activation, time schedules, environment selection, and keyframes. Do not assume every identified light is active at every time.
- Select lights by influence-volume intersection with geometry bounds, rather than center distance alone.
- Make selection deterministic and resistant to switching at the six-light boundary.
- Introduce per-pixel local lighting so a lamp can illuminate the interior of a large triangle or mesh.
- Preserve a bounded fallback for low-capability devices.

**Acceptance:** A lamp near the edge of a large mesh illuminates that edge even when the mesh center is outside its range. Splitting or rebatching the same geometry does not substantially change the illumination. The chosen unit conversion has evidence and regression coverage.

### R08 — Make map coverage and comparison bias explicit

**Current limits:** One orthographic map; fixed 2048 resolution; half-width clamped to 16–160; light-eye offset 200; light-depth span 400. The map has an edge fade. These are implementation constants, not a fitted scene volume.

**Work:**

- Visualize and record the effective receiver and caster volumes.
- Include upstream occluders that can shadow visible receivers, rather than fitting only what the camera sees.
- Test elevated structures, deep terrain, large cliffs, long low-angle shadows, zoomed cameras, and boundary crossings.
- Audit depth-span clipping before attributing a missing shadow to bias.
- Express comparison bias in documented scene-space terms or derive it from the actual map scale.
- The current normalized bias constants correspond to approximately 0.14–0.54 units along a 400-unit light-depth span; evaluate contact loss and floating shadows at that scale.
- Preserve receiver-plane correction and its resolution-independent conditioning.
- Keep bias used for light comparison separate from the removed receiver depth offsets.

At the default half-width of 80, a 2048 map covers about 0.0781 scene units per texel. Increasing half-width to 160 doubles that footprint. A larger coverage setting necessarily reduces spatial detail unless map strategy changes.

**Acceptance:** Coverage limits are predictable; occluders do not disappear because of an unexplained fixed depth interval; bias prevents self-shadowing without visibly detaching contact shadows.

### R09 — Improve caster continuity while moving

Camera rotation no longer redistributes the NPC budget, but selection can still change when the player/focus moves or actors cross budget and distance boundaries.

**Work:**

- Measure whether nearest-N selection produces visible popping during movement.
- Add hysteresis, stable retention, or a controlled transition if the measurements justify it.
- Test multiple NPCs with nearly equal distances and dense crowds around the budget limit.
- Ensure hidden-room actors never consume the usable budget.
- Audit zone LOD transitions: caster geometry still uses LOD visibility rules and can change shape as thresholds are crossed.
- Verify caster and receiver geometry use compatible vegetation deformation and animated poses.
- Test movable props, doors, lifts, and runtime object overrides.
- Keep actor pose copies correct before optimizing their memory use.

**Acceptance:** Turning the camera does not remove a still-relevant caster, and ordinary movement across a selection or LOD threshold does not produce an unexplained large brightness change.

## 7. P2: extend coverage and local shadows

### R10 — Add stable cascades if one map cannot meet the measured requirements

This is a proposed extension, not an implemented feature.

- Choose split distances from actual camera and gameplay requirements.
- Keep cascade scale and orientation stable; retain texel snapping.
- Include upstream caster depth bounds for each cascade.
- Blend transitions without double-darkening receivers.
- Preserve stacked-layer receiver selection and all alpha rules.
- Define per-cascade resolution, update rate, bias, and allocation budgets.
- Test movement, zoom, aspect-ratio changes, cascades crossing geometry, and dawn/dusk.

**Dependency:** R02 and R08. Add cascades because a measured coverage/detail tradeoff requires them, not as an unmeasured response to every artifact.

**Acceptance:** Near shadows improve without creating visible cascade seams, unstable splits, excessive memory growth, or lost distant occluders.

### R11 — Add shadowed lamps and torches

Point lights currently illuminate without local occlusion. Sunlight shadows do not make a wall block a lamp.

**Work:**

- Complete R06 and R07 first.
- Select a bounded set of shadowed local lights independently from the total number of active emitters.
- Evaluate cubemap shadow depth or another explicitly chosen local-light technique for the retained backend.
- Define resolution, range, near plane, update scheduling, and cache invalidation per light.
- Keep light selection stable as the player moves.
- Handle moving actors and props inside otherwise static light volumes.
- Validate point-light seams, wall leakage, thin geometry, and lights placed close to surfaces.
- Test mixed sun and lamp lighting, including a lamp inside a sunlight shadow.
- Retain a lower-cost unshadowed local-light mode.

**Acceptance:** Walls and props block lamp light, characters cast local shadows, and movement does not cause nearby lamps to blink between shadowed and unshadowed states unexpectedly.

### R12 — Define water, transparency, and special-effect behavior

Soft transparency, water, and environment shells are currently excluded from the ordinary shadow passes. Treating every transparent effect as an opaque caster would introduce new errors.

- Keep hard cutouts distinct from soft transparency.
- Decide which water surfaces receive sunlight shadows and how those shadows affect reflected, transmitted, and authored surface color.
- Decide whether any translucent material casts an approximate shadow; document that approximation.
- Preserve water/actor depth ordering and home-point crystal/aura composition.
- Decide whether world items should join shared shadow submission or intentionally remain on a separate path.
- Test underwater objects, waterfalls, fences, foliage, crystal effects, smoke, and additive glows.

**Acceptance:** Material-specific behavior is deliberate. Water or effects do not become solid blockers, and shadow changes do not regress established transparency ordering.

## 8. Optional later work and renderer organization

### R13 — Evaluate temporal filtering and advanced effects only after the baseline

Potential work includes temporal shadow filtering, variable penumbrae, contact refinement, sun shafts, and eventually ray-traced shadows.

For temporal filtering, first define history validity, motion information, camera cuts, disocclusion, animation, light movement, and reset behavior. A history buffer must not hide invalid shadows, smear actors, or retain shadows from removed geometry.

For soft shadows, define a light size and a measured filter cost. A larger blur is not a complete soft-shadow model.

For sun shafts and ray tracing, treat implementation, capability, performance, and fallback requirements as separate design work. These features are not provided automatically by a different graphics API or abstraction library.

### R14 — Continue backend separation without restarting the asset loader

The existing shared interfaces are useful groundwork. Remaining backend work should be based on a fresh source audit because another renderer effort is changing these interfaces concurrently.

- Remove remaining native capability/attachment-description queries from shared shadow orchestration where appropriate.
- Make capability and format support explicit in the shared interface.
- Define ownership, error reporting, and state restoration consistently.
- Provide backend-specific shader libraries and consistent constant/transform conventions.
- Add backend conformance tests for depth range, winding, mirroring, alpha, sampling, MSAA, and state restoration.
- Decide separately whether to implement another backend directly or through a library.
- Keep D3D9 compatibility only to the extent explicitly required; an added library does not automatically implement that fallback.

There is no need to replace the FFXI DAT decoder or gameplay code merely to add another graphics backend. Likewise, adopting wgpu or another library is not necessary to finish the current shadow fixes.

### R15 — Make the shadow implementation easier to maintain

- Consider moving the included `.inl` implementation into a dedicated module once its interfaces are stable.
- Separate resource ownership, light-volume math, submission, visibility policy, and pass execution.
- Avoid global resource state that assumes only one device/context unless that restriction is explicit and enforced.
- Keep borrowed model/visibility pointers valid for the complete frame.
- Add structured pass results and counters rather than relying only on a Boolean and debug output.
- Reuse buffers and pose-storage capacity after profiling demonstrates the benefit.
- Coordinate changes to interface headers and rebuild every dependent translation unit.

This refactoring should preserve the tested rendering behavior. It is not a reason to rewrite the pass and lose its regressions.

## 9. Validation matrix still to complete

| Dimension | Required cases beyond the current baseline |
|---|---|
| Real zones | Original Bastok views; another city; uneven outdoor terrain; dense foliage; indoor fallback |
| Resolution | 1280×720, 1920×1080, 2560×1440, 3840×2160, non-square aspect ratios, and different internal render scales |
| High-resolution content | Actual shadow edges, animated actors, detail layers, and mixed alpha rather than only an unoccluded grazing plane |
| Camera | Static, slow orbit, fast orbit, translation, zoom, pitch near the ground, camera cuts, and transitions across visibility cells |
| Time | Frozen time, smoothly advancing time, dawn, dusk, source changes, midnight wrap, local-system clock, and explicit overrides |
| Geometry | Nearly coplanar layers, separated layers, reversed draw order, differently triangulated surfaces, thin walls, roofs, and large bounds |
| Materials | Original textures, HD replacements, PBR enabled/disabled, DXT3 cutouts, mixed alpha masks, two-sided foliage, and untextured geometry |
| Animation | Shared NPC assets, offscreen casters, moving actors, doors/lifts, vegetation, and runtime transforms |
| Budgets | Zero and small NPC limits, dense crowds, equal-distance actors, boundary crossings, and hidden-room candidates |
| Shadows | Player only, NPC only, object only, combined, unsupported-map fallback, and indoor fallback |
| AA | None, supported 2×/4×/8× MSAA, full-resolution MSAA, and postprocessing combinations |
| Lifetime | Resize, internal-resolution change, fullscreen changes, reset, device loss, resource failure, zone reload, and shutdown |
| Hardware | Representative Intel, AMD, and NVIDIA devices/drivers, including the lowest capability level retained by the project |
| Performance | Shadow-only cost, crowded scenes, wide draw distance, high resolution, and long sessions |

The table is a validation backlog. It must not be presented as a list of cases that already pass.

### 9.1 Test-harness work required before expanding the matrix

The current harness cannot yet substantiate every row of the matrix above.

- Extend device creation to accept independent width and height. Existing resolution cases create square backbuffers, so 1920² must not be reported as a 1920×1080 test.
- Correct the MSAA readback resolve allocation to use the source attachment's actual width and height. It currently allocates the original 128×128 fixture size even when the source is larger. The passing large-viewport case uses no MSAA, while the passing MSAA cases use 128×128.
- Retain native-resolution readback and analysis after that correction. Downsampling first can hide the artifact being measured.
- Add meaningful expected lit and occluded regions to installed-zone captures. A nonzero or minimum changed-pixel count is only a smoke test that a pass ran.
- Add application-level caster-selection tests. The shadow-only API regression proves that submission can cast without color output; it does not prove the complete nameplate-culling, zone-cell, animation, and NPC-budget integration.
- Add a rendered whole-day or focused transition sequence. The four clock assertions establish override and fractional-direction behavior, not visual behavior at every sun/moon transition.
- Save a capture manifest with source/build identity, GPU and driver, settings, dimensions, sample count, and the exact test command.

**Acceptance:** A test result names the actual dimensions, processing mode, and scenario it exercised. New large-window or MSAA claims require the corresponding harness support and captured evidence.

### 9.2 Better temporal measurements

For deterministic sequences, record more than a single center pixel:

- Lit interiors must not acquire broad false shadows.
- Real shadow interiors must remain present when their casters remain valid.
- Compare edge brightness changes separately from interior changes.
- Align world-space samples or account for motion before classifying screen-pixel changes as flicker.
- Distinguish expected shadow movement from discontinuous jumps.
- Track how many pixels change, the magnitude of the change, and whether the change follows material boundaries or triangle edges.
- Capture at the native viewport size before producing smaller previews.

Thresholds should come from a stated fixture and intended behavior. Do not use a loose global image-difference tolerance that makes the original failure pass.

## 10. Build and test handoff

### 10.1 Build consistency

The renderer interface has been edited concurrently with this work. Incremental outputs built against different interface versions produced misleading failures and virtual-table size warnings. A clean rebuild restored passing behavior.

Before interpreting a new rendering failure:

1. Confirm which executable is running.
2. Confirm the source and build outputs belong to the same interface version.
3. Finish relevant source changes before building dependent targets.
4. Use a clean rebuild if interface layouts or signatures changed during compilation.
5. Treat ABI warnings as a build correctness problem; do not tune shadow math around them.

The local toolchain also encountered an MSPDB140.DLL version mismatch. Validation used the installed v143 tools with an artifact-only import disabling debug information. Production project settings were not changed to work around that machine-specific linker issue. As of 2026-10-16 the mismatch no longer reproduces: Release and Debug x64 builds with symbols link and produce `DATura.pdb` (logs in `artifacts/lnk1101-check/`), so the no-PDB import below is optional. If LNK1101 returns, run `Get-Process mspdbsrv | Select Id,Path` and end any `mspdbsrv` not from the toolset in use before rebuilding.

### 10.2 Repeatable commands for this workspace

Run from PowerShell. These paths describe the current local workspace and installed toolchain; verify them on another machine. A rebuild is appropriate after interface changes, not necessary for every small edit.

```powershell
$repoRoot = 'C:\Users\nanob\source\repos\DATura'
$msbuildPath = 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe'
$testOutput = Join-Path $repoRoot 'artifacts\shadow-temporal-agent\'
$testObjects = Join-Path $repoRoot 'artifacts\shadow-temporal-agent-obj\'
$buildImport = Join-Path $repoRoot 'artifacts\sun-shadow-no-pdb.targets'
$ffxiRoot = 'C:\Program Files (x86)\PlayOnline\SquareEnix\FINAL FANTASY XI'

Set-Location -LiteralPath $repoRoot
& $msbuildPath 'tests/MaterialRenderingTests.vcxproj' /t:Rebuild /m `
    /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 `
    "/p:OutDir=$testOutput" "/p:IntDir=$testObjects" `
    "/p:ForceImportAfterCppTargets=$buildImport" /v:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw 'Material rendering test build failed.' }

$testExecutable = Join-Path $testOutput 'MaterialRenderingTests.exe'
$fullCaptures = Join-Path $testOutput 'full-captures'
& $testExecutable $fullCaptures $ffxiRoot
if ($LASTEXITCODE -ne 0) { throw 'Material/shadow validation failed.' }

$temporalCaptures = Join-Path $testOutput 'temporal-captures'
& $testExecutable --shadow-temporal $temporalCaptures
if ($LASTEXITCODE -ne 0) { throw 'Temporal shadow validation failed.' }
```

Also build the application after integration changes. The tested artifact output used in this chat is `artifacts/sun-shadow-app/DATura.exe`; it is distinct from a normal `x64/Release` executable and from any already-running process.

### 10.3 Useful evidence already available

| Artifact | Purpose |
|---|---|
| `artifacts/shadow-temporal-agent/full.log` | Last recorded full test run associated with the latest fixes |
| `artifacts/shadow-temporal-agent/baseline.log` | High-resolution failure before the derivative-conditioning correction |
| `artifacts/shadow-temporal-agent/fixed.log` | Focused result after that correction |
| `artifacts/shadow-temporal-agent/baseline-captures/1920/` | Native high-resolution failing capture |
| `artifacts/shadow-temporal-agent/fixed-captures/1920/` | Matching corrected capture |
| `artifacts/sun-shadow-tests/layer-before-native.log` | Older stacked-layer regression evidence |
| `artifacts/sun-shadow-tests/layer-after/` | Layered-ground and genuine-shadow captures |
| `artifacts/zone-diagnostics/bastok-markets-point-lights.csv` | Identified point emitters, coordinates, ranges, powers, and DAT offsets |

These are local generated artifacts. Preserve important evidence deliberately if it is needed outside this workspace; do not assume every artifact is tracked or available in a fresh checkout.

## 11. Source map for the next developer

Paths below are relative to the repository root.

| File | Responsibility and likely remaining work |
|---|---|
| `DATura/directional_shadow_map.inl` | Resource setup, submission snapshots, light-volume math, shader source, receiver selection, filtering, composition, and failure paths |
| `DATura/model_renderer.cpp` | Point-light selection, fixed-function lighting preparation, geometry passes, planar fallback, and material/pass interaction |
| `DATura/model_renderer.h` | Draw context, fractional light time, shadow parameters, and shadow-only submission API |
| `DATura/main.cpp` | Shared frame light time, shadow scheduling, NPC eligibility/budgets, camera culling, world-item ordering, reset callbacks |
| `DATura/zone_environment_state.cpp` | Integer environment clock, precise light clock, time overrides, environment interpolation |
| `DATura/d3d_model_render_state.cpp` | Automatic directional lighting, material alpha behavior, replacement texture resolution |
| `DATura/custom_texture_assets.cpp` | HD/PBR texture replacement and material shader assumptions |
| `DATura/zone_object_visibility.cpp` / `DATura/zone_lod.h` | Zone visibility and LOD decisions shared by scene geometry and casters |
| `DATura/model_ff11.cpp` / `DATura/model_ff11_map_handlers.h` | Authored generators, light records, and geometry/material decoding |
| `DATura/renderer_resources.h` / `DATura/renderer_materials.h` | Shared graphics/material contracts |
| `DATura/d3d9_graphics.h` / `DATura/renderer_backend.*` | Current adapter and runtime implementation |
| `DATura/application_settings.h` / `DATura/config_dialog.cpp` | Setting semantics, supported ranges, active-path controls, diagnostics UI |
| `tests/material_rendering_tests.cpp` | Native image readback, shadow regressions, temporal cases, installed-zone comparison |
| `Markdown Documents/DIRECTIONAL_SHADOW_MAPS.md` | Current implementation notes and history of the major fixes |
| `Markdown Documents/SHARED_RENDERER_INTERFACE.md` | Current backend separation status; re-read before planning migration |

## 12. Suggested next deliverable

The next bounded deliverable should be a **validated shadow baseline**, not another unmeasured graphics feature:

- [ ] Confirm the latest executable in the user's original views.
- [ ] Capture a deterministic real-zone camera/time sequence at the actual internal resolution.
- [ ] Reproduce any remaining artifact with a failing test or saved native capture.
- [ ] Add directional pass counters and collect CPU/GPU/memory measurements.
- [ ] Fix late-failure/fallback handling and effective-setting inconsistencies.
- [ ] Validate mixed-alpha original/replacement silhouettes.
- [ ] Retain the layered-ground, full-resolution, temporal, and offscreen-caster regressions.
- [ ] Record the supported hardware/quality configuration and observed limitations.

After that baseline, the most valuable lighting extension is to separate sunlight from ambient and local lighting, verify the authored lamp data, and implement bounded per-pixel local lights. Shadowed lamps can then be added on a sound foundation.
