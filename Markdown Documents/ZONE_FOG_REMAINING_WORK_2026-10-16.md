# Zone fog: remaining work and verification handoff

Date: October 16, 2026 (America/Denver).

## 1. Scope and status

This document covers the remaining work from the zone-fog investigation, the
texture-shader build problem, and the subsequent rendering verification. It is
not a backlog for every unfinished feature in DATura.

**The basic fog change is implemented.** DATura already applied authored fog to
terrain. World NPCs and the player now opt into the active zone's fog as well.
Character preview passes retain their separate atmosphere. Invalid fog ranges
are rejected. The texture-shader handle mismatch is corrected in the workspace,
and a full Release/x64 application build succeeded.

**The work is not yet a complete retail-fidelity certification.** The remaining
tasks are to resolve the known build-tool and shadow issues, strengthen the
reproducibility of the evidence, and verify behavior omitted by the current
static scene and synthetic actor tests.

The findings below distinguish three categories:

| Category | Meaning |
| --- | --- |
| Confirmed issue | A recorded failure or directly observed limitation exists. |
| Verification gap | Behavior has not been adequately exercised; a defect is not established. |
| Optional extension | Additional functionality beyond ordinary authored distance fog. |

The shared workspace contained substantial unrelated changes and continued
changing during the previous verification. Test results are snapshots of their
respective runs. Rebuild before treating any result as a statement about the
current checkout.

### 1.1 Verification run, later on 2026-10-16

A follow-up run built from a hashed source snapshot (details, logs and binaries in
`artifacts/zone-fog/verify-2026-10-16/`, see its `README.md`):

- Focused shadow mode: **854 assertions, 0 failures**. The five 1920² grazing-ground
  failures are fixed (darkest=255, 0 false pixels in frames 0-4), by the shadow
  slope-correction fix (#4 in `SHADOW_AND_LIGHTING_REMAINING_WORK_2026-10-16.md`).
- Focused fog/material mode: **230 assertions, 0 failures**. Installed-zone fog:
  **277, 0 failures**. Full material suite: **2,661, 0 failures** (before and after
  the fix below). SkyRenderingTests and WeatherRenderingTests: 0 failures.
- New `tests/ZoneFogTransitionTests.vcxproj` (`zone_fog_transition_tests.cpp`):
  **60 assertions, 0 failures**, stable across repeated runs. It found and now
  guards the weather-blend color bug below.
- **Fixed: red/blue swap during weather transitions.** `BlendWeatherData` in
  `DATura/zone_environment_state.cpp` decoded already-converted D3D XRGB colors
  with the 0x2F packed-color helper, so fog, light, clear and sky-ring colors had
  red and blue swapped for the 3-second blend. Because the final frame was cached,
  the swap also stayed until the next game-minute tick. Gray mist colors hid it.
  It now uses a separate `LerpD3DColor`.
- **Hardened:** `Update` with empty records now also cancels any in-flight
  weather blend and marks the cache dirty. DATura always calls `Invalidate` on
  zone load, so this was not reachable in practice. Without it, an empty-records
  gap mid-blend would carry the previous zone's fog for up to 3 seconds.
- Debug PDB breakpoints in fog code bound to the source lines and were hit (section 6).

Still pending: live NPC/player captures in the running application, camera
viewpoints, wider zone coverage, and the remaining P2/P3 items.

## 2. Work already completed

These items should be preserved rather than implemented again:

- Audited every nonempty model path in `DATura/Zone_DATs_Table.csv`: 295 zone
  files, no missing files, and 9,519 environment records.
- Identified enabled terrain fog in 292 of those files. IDs 229, 278, and 279
  had no environment records in the scanned model files.
- Saved the per-zone inventory in `tools/zone_fog_inventory.csv` and added
  `tools/audit_zone_fog.py` to regenerate detailed evidence.
- Connected world NPC/player model passes to the existing active-zone fog.
- Kept preview rendering separate from world fog.
- Added finite-value and range checks to the fog setup paths and shadow fog
  attenuation.
- Verified the texture shader uses the renderer's shader wrapper consistently.
  The shader-handle correction was already present when follow-up work began.
- Built the full application to `artifacts/zone-fog/app/DATura.exe` using the
  a local no-debug-symbol workaround (no longer needed; see section 6).
- Ran the focused material/depth/fog mode: **226 assertions, zero failures**.
- Ran the installed-zone fog mode: **277 assertions, zero failures**.
- Inspected day/night comparisons for six representative zones.

The 503 passing focused assertions combine synthetic GPU checks and real-zone
render checks. They are not 503 zones, screenshots, or gameplay scenarios.

## 3. Priority order

| Priority | Task | Category | Completion evidence |
| --- | --- | --- | --- |
| P1 | Establish a fresh, reproducible build/test snapshot | Done 2026-10-16 (section 1.1) | Build manifest, fresh logs, exact source state recorded |
| P1 | Reproduce and resolve five grazing-ground shadow failures | Resolved 2026-10-16 (shadow fix #4) | Focused shadow and full-suite runs pass |
| P1 | Repair the local debug-symbol toolchain | Resolved 2026-10-16; breakpoints confirmed | Normal symbol-producing build links and can be debugged |
| P1 | Verify live NPC/player integration | Verification gap (pending desktop access) | Real actor captures at multiple fog distances and after pass changes |
| P2 | Verify weather/time and zone transitions | State-level tests done, one bug fixed; in-app evidence pending | Deterministic transition checks and gameplay evidence |
| P2 | Improve camera coverage and scene fidelity | Confirmed capture limitations | Clear near/middle/far views and production-path captures |
| P2 | Audit interiors, event environments, and remaining zone types | Verification gap | Expanded inventory and representative capture matrix |
| P2 | Verify fog across effects, water, shadows, and alternate materials | Verification gap | Cross-pass comparisons without discontinuities |
| P2 | Strengthen the automated fog oracle and inventory tooling | Verification gap | Targeted regressions and reproducible reports |
| P3 | Establish retail-reference comparisons | Verification gap | Matched reference scenes with documented differences |
| P3 | Investigate additional ground-mist/volumetric effects | Optional extension | Evidence-led scope and separate implementation plan |

## 4. P1: establish a reproducible baseline

### 4.1 Record exactly what was built

The previous workspace was changing during verification, including shadow code
and tests. A material suite passed 2,504 assertions earlier; a later expanded
suite reported 2,657 assertions and five failures. The counts differ because
the suite changed. Do not interpret that difference as a measured fog regression.

Remaining actions:

- [x] Record the branch, commit, and relevant uncommitted changes before a run
  (`verify-2026-10-16/source-manifest.txt`).
- [x] Identify which changes belong to fog, shader migration, and shadow work
  (at snapshot time only shadow files were uncommitted under `DATura/` and `tests/`).
- [x] Build the application and tests from the same recorded source state.
- [~] Save the actual compiler/toolset version, Windows SDK, GPU/driver,
  processing mode, build configuration, and capture resolution. Recorded in the
  run README, except which physical GPU `D3DADAPTER_DEFAULT` used.
- [ ] Record the retail DAT installation/version where available. Include input
  hashes or another reproducible fingerprint for the six tested model files.
- [x] Write results into a new run directory rather than silently mixing old
  images with newly generated logs.
- [x] Preserve native executable exit codes. A surrounding PowerShell command
  succeeding does not prove the executable succeeded.
- [ ] Regenerate contact sheets from that run's images and retain the generation
  procedure in the repository.

Acceptance: another developer can identify the source and inputs behind every
reported result and reproduce the run without relying on this chat.

### 4.2 Keep a focused fog gate and a full regression gate

The focused gate should exercise texture/bump shader execution, alpha handling,
depth, authored fog, world actor fog, preview isolation, and sky exclusion.
The full gate must also include the independent shadow tests. A passing focused
gate must not conceal a failing full gate.

Acceptance: reports show both outcomes separately, including any known failures.

## 5. P1: investigate the recorded shadow failures

Evidence: `artifacts/zone-fog/regression.log` ends with five failures named:

```text
unoccluded grazing ground remains bright at full resolution during camera orbit
```

The recorded 1920-resolution cases, frames 0 through 4, each reported
`darkest=191` and `false shadow pixels=409600`. This establishes a failing test
observation. It does not establish a root cause or prove the failure remains
in the latest workspace.

Relevant starting points:

- `tests/material_rendering_tests.cpp`: `TestHighResolutionShadowSlope`,
  `TestTemporalShadowEdges`, and the `--shadow-temporal` mode.
- `DATura/directional_shadow_map.inl`: shadow rendering/compositing and fog
  attenuation.
- `DATura/model_renderer.cpp`: pass setup and ordering.

**Status 2026-10-16: resolved.** The shadow workstream fixed the cause
(resolution-dependent slope correction, fix #4 in
`SHADOW_AND_LIGHTING_REMAINING_WORK_2026-10-16.md`). Rerun from the snapshot:
`--shadow-temporal` 854 assertions, 0 failures; 128² and 1920² frames 0-4 all
darkest=255 with 0 false shadow pixels; full suite 2,661, 0 failures. The items
below remain with the shadow workstream; the fog-specific one (shadow darkening
on fully fogged receivers) is still unverified in real scenes.

Remaining actions:

- [x] Rerun the focused temporal/slope cases before changing shadow code.
- [ ] Compare 128- and 1920-resolution outputs with the same scene parameters.
- [ ] Confirm the viewport, render target, projection, depth precision, and
  camera-space conventions used by the test match its analytic expectation.
- [ ] Determine whether darkening originates in shadow visibility, bias,
  receiver selection, filtering, compositing, or an invalid test assumption.
- [ ] Check behavior with fog disabled, partially active, and fully active.
- [ ] Fix the identified cause without weakening the expectation merely to
  obtain a passing result.
- [ ] Retain tests for genuine occluders, stacked ground, both draw orders,
  hardware vertex processing, and available MSAA configurations.
- [ ] Rerun the complete suite and preserve before/after evidence.

Acceptance: unoccluded grazing ground stays bright, genuine shadows remain,
and fully fogged surfaces do not acquire inappropriate shadow darkening.

## 6. P1: repair the local debug-symbol build

The shader compilation/type error is cleared. A separate local linker error
was encountered:

```text
LNK1101: incorrect MSPDB140.DLL version; recheck installation of this product
```

Verification succeeded by importing
`artifacts/sun-shadow-no-pdb.targets`, which disables compiler/linker debug
information. That is a workaround, not a repair of Visual Studio.

**Status 2026-10-16: resolved; the error no longer reproduces.** With VS 18
MSBuild (v145, MSVC 14.51.36231) from plain PowerShell and no no-PDB import,
Release x64 and Debug x64 both linked with 0 errors and produced `DATura.pdb`
(logs: `artifacts/lnk1101-check/build-fresh-release-pdb.log` and
`build-fresh-debug.log`). Earlier runs in the same folder with VS 2022/v143 and
with a foreign `mspdbsrv` running also linked. Every installed MSVC bin
directory has matching `link.exe`/`mspdb140.dll` versions, and no MSVC
directory is on PATH. No log of the original failure survives, so its exact
cause was not recovered; the likeliest cause is the earlier build shell's
environment, not the installation. Nothing was changed to fix it.

If it returns: keep the full log, run `Get-Process mspdbsrv | Select Id,Path`,
end any `mspdbsrv` that is not from the toolset in use
(`Stop-Process -Name mspdbsrv`), check that PATH has no other VS bin directories
ahead of VS 18, and rebuild.

Remaining actions:

- [x] Reproduce a normal symbol-producing build and record the complete error
  (did not reproduce; builds succeed).
- [x] Check the selected compiler/linker, matching PDB tools, environment search
  paths, and any stale PDB server processes before deciding on a repair.
- [x] Repair or align the toolchain based on evidence from those checks (no
  repair needed).
- [x] Verify a normal Release build with symbols and a Debug build.
- [x] Confirm symbols load and source breakpoints work. No cdb/WinDbg is
  installed, so a small Win32 Debug API + DbgHelp harness loaded the Debug PDB of
  `ZoneFogTransitionTests.exe`, bound `zone_environment_state.cpp:143` (weather
  blend fog color) and `:360` (time-of-day fog color) to their exact lines, and
  hit both (1,247 and 2,703 hits) with source-line stack frames
  (`artifacts/zone-fog/verify-2026-10-16/debugger-breakpoints.log`). The Visual
  Studio IDE debugger UI was not driven.
- [x] Document any retained workaround in a stable repository location rather
  than requiring an unrelated shadow artifact to exist (no workaround retained).

Acceptance: ordinary builds link without the no-PDB override, and debugging
works. This task is local toolchain maintenance; it should not change fog output.

## 7. P1: verify actual world characters

The production calls now opt NPCs and the world player into zone fog. The
automated actor checks render synthetic geometry. Installed-zone captures do
not place live NPCs or a real player model in the scene.

Remaining actions:

- [ ] Capture actual player and NPC assets in at least a dense-mist field and
  a forest, using the normal application rendering sequence.
- [ ] Place actors near the fog start, within the interval, and beyond its end.
- [ ] Compare actor and adjacent terrain appearance at similar eye distances.
- [ ] Include textured skin, equipment, opaque surfaces, and alpha-tested hair
  or clothing where the assets provide them.
- [ ] Check actor shadows together with actor fog.
- [ ] Verify NPC nameplates and interaction highlights retain their intended
  readability and depth behavior.
- [ ] Switch to character creation, the roster, and companion previews, then
  return to the world. Verify fog is neither retained nor lost incorrectly.
- [ ] Exercise a fog-bearing zone followed by a standalone model and the reverse.

Acceptance: characters fade consistently with the world, previews keep their
intended appearance, and no previous pass's fog leaks into the next scene.

### 7.1 Resolve the actor fog-source fidelity question

The current world actor context receives the active terrain fog color and
distances. Parsed environment records also contain a `modelLight` configuration.
That alone does not establish which source retail uses for every actor pass.

- [ ] Compare model and terrain fog fields across a useful sample of DATs.
- [ ] Find cases where the values differ materially.
- [ ] Consult existing client/rendering research and, where needed, matched
  reference behavior to establish the intended source.
- [ ] Change the actor policy only if evidence supports a different choice.
- [ ] Add a regression using differing configurations if the policy changes.

Acceptance: the selected actor fog source has documented evidence. This is an
open fidelity question, not a confirmed defect in the current shared-fog policy.

## 8. P2: verify transitions rather than isolated snapshots

The installed-zone test deliberately filters records to one weather directory
and creates fresh environment state for each capture. It therefore avoids the
weather-transition path. It tests midnight and noon, not a continuous day.

Relevant implementation: `DATura/zone_environment_state.cpp`, including
`BlendWeatherData`, weather-group selection, time interpolation, and cache
invalidation.

**Status 2026-10-16:** `tests/ZoneFogTransitionTests.vcxproj` covers the
state-level items below with synthetic records (60 assertions, 0 failures). It
found the red/blue weather-blend swap described in section 1.1, now fixed.
In-app visual evidence of transitions is still pending.

- [x] Test fair-to-mist and mist-to-fair transitions at start, intermediate
  points, and completion.
- [x] Test transitions involving an authored `fogFar == 0` off sentinel in
  both directions. Preserve discrete off behavior rather than interpolating
  into a spurious short fog range (fog switches once at the weather midpoint).
- [x] Verify rapid weather changes and changes during an existing transition
  (a mid-blend change continues from the visible fog).
- [x] Test sunrise, sunset, midnight wrap, and intervals between authored times.
- [~] Check fog color, distances, sky, lighting, and actors update coherently.
  Fog, clear and sky-ring colors follow one blend factor in state; actor and
  on-screen coherence are not yet captured.
- [x] Verify cached state invalidates on zone reload and environment changes.
- [x] Check transitions between zones with dense fog, light haze, and no valid
  active environment; previous-zone fog must not persist (state level).
- [x] Make transition tests deterministic. Rather than injecting a clock (which
  would change production signatures shared with the weather work), each check
  derives its expectation from the blend factor `Update` reports
  (`Data::weatherTransition`), so pass/fail does not depend on sleep timing.
  The run takes about 15 seconds because it waits on the real 3-second blends.

Acceptance: finite, bounded fog values and coherent appearance throughout
transitions, with no stale state or unintended fog flashes.

## 9. P2: improve the scene captures

### 9.1 Improve viewpoints

The current cases use fixed cameras near known arrivals. Some views are
dominated by nearby terrain: La Theine faces a rock wall, and Valkurm includes
a large close rock face. Those views demonstrate local fog application but
are weak evidence for distant scenery, horizon blending, or draw-distance
behavior. Earlier camera attempts were replaced during verification.

- [ ] Select at least one open near/middle/far composition for every test zone.
- [ ] Confirm cameras are above walkable ground and outside solid geometry.
- [ ] Add a second forest view showing layered foliage at different distances.
- [ ] Include a coastline/water view and a view across a terrain silhouette.
- [ ] Save camera position, direction, projection, and relevant world mirroring
  settings with each capture.
- [ ] Use consistent viewpoints between disabled, fair-weather, and mist images.
- [ ] Increase review resolution beyond 640x640 for edges, foliage, and actors.

Acceptance: each zone has at least one image where fog progression can be
judged across several depths, not only against an adjacent wall.

### 9.2 Match the production rendering sequence

The current installed-zone helper directly renders real DAT geometry, textures,
authored lighting, and the sky dome. It does not invoke the complete gameplay
scene. It omits generated cloud/weather particles and live actor placement.

- [ ] Add captures through the normal application frame path or extend the
  harness to reproduce its meaningful pass ordering.
- [ ] Include clouds, weather particles, opaque terrain, actors, effects,
  transparent surfaces, and UI in the proper order.
- [ ] Match room visibility, LOD selection, draw distance, lighting settings,
  and projection to the application configuration under review.
- [ ] Verify no texture shader, fog mode, or fog enable state leaks between passes.

Acceptance: the integrated application looks consistent with the isolated fog
checks, and differences caused by omitted scene components are explained.

## 10. P2: expand coverage beyond the six outdoor zones

The inventory covers 295 model files, while visual testing covers six zones,
two weather directories, and two times per zone. A record inventory is not
equivalent to visual or gameplay coverage.

- [ ] Group the inventory by meaningful environment characteristics: outdoor
  fields, forests, towns, caves, dungeons, expansion zones, and special areas.
- [ ] Choose representative dense-fog, long-distance-haze, and off-sentinel cases.
- [ ] Add separately loaded room resources to the audit where applicable.
- [ ] Verify indoor/outdoor transitions and event-specific environments.
- [ ] Determine whether environment selection follows the correct room/event
  state before modifying fog strength to compensate for a selection problem.
- [ ] Investigate IDs 229, 278, and 279 before assigning fallback fog. The scan
  found no records in their model files; it did not establish missing content.
- [ ] Distinguish weather variants stored in a DAT from weather that naturally
  occurs in that zone. Document selection/occurrence separately.

Acceptance: a coverage table records which resources and scenarios were
audited, rendered, inspected, reference-compared, or remain untested.

## 11. P2: verify interactions with other rendering paths

These are verification tasks unless a reproducible defect is found.

| Path | Remaining checks |
| --- | --- |
| Water | River/sea transparency, fogged background seen through water, and shoreline blending |
| Foliage and cutouts | Leaf/hair holes remain transparent; fog changes RGB without filling alpha gaps |
| Soft transparency | No ordering-dependent fog mismatch or double fog application |
| Home points and portals | Shader fog agrees with nearby geometry and uses the intended active state |
| Weather particles | Intended fog participation, depth behavior, and state restoration |
| Sky and cloud shells | Sky exclusion remains correct; no horizon seam or accidental landscape fog on shells |
| HD/PBR materials | Optional shaders retain intended fog behavior and match the base-material path |
| Directional shadows | Shadow attenuation agrees with fog and does not darken fully fogged receivers |
| Nameplates/UI | Intended readability and depth behavior remain intact after world passes |
| Device/resource lifecycle | Scene changes and resource recreation do not retain shaders or fog from old state |

The home-point shader explicitly computes fog from eye distance, while other
paths may use fixed-function fog. Verify distance conventions at oblique angles
and after world transforms before asserting those paths are equivalent.

Acceptance: adjacent objects rendered by different paths have no unexplained
fog boundary, brightness jump, alpha defect, or stale-state artifact.

## 12. P2: strengthen tests and the audit tool

### 12.1 Strengthen the image oracle

The installed-zone test compares summed RGB distance from the authored fog
color. It requires more than 1,000 materially changed pixels and at least 95%
of those changes to move toward that color. This detects an absent or grossly
incorrect fog contribution. It does not prove correct depth progression,
retail-equivalent density, correct active weather, or good camera placement.

- [ ] Add depth-aware expectations or known near/middle/far sample regions.
- [ ] Independently calculate expected RGB for synthetic textured and untextured
  surfaces under partial fog, preserving alpha expectations.
- [ ] Cover a nonzero fog start, a zero start, and different valid spans.
- [ ] Verify both software and hardware vertex-processing paths where supported.
- [ ] Add negative tests showing the oracle rejects disabled fog, wrong colors,
  incorrect intervals, and incorrect pass-state restoration.
- [ ] Keep full-size images and metrics alongside any resized contact sheets.
- [ ] Consider camera-motion checks for fog stability at edges and large triangles.

Acceptance: tests reject representative incorrect implementations while
remaining deterministic and tolerant only of justified GPU rounding differences.

### 12.2 Harden inventory generation

The current scanner reads terrain fog from sufficiently large 0x2F payloads,
tracks directory nesting, and reports valid enabled ranges. Initial synthetic
checks were run ad hoc rather than added as a maintained test suite.

- [ ] Convert the enabled, off-sentinel, and malformed-chunk checks into tests.
- [ ] Add nonfinite values, reversed/equal ranges, short environment payloads,
  directory nesting, missing DATs, and trailing/truncated data cases.
- [ ] Define whether malformed input should stop the whole audit or produce a
  per-zone error and continue; expose the decision in the report.
- [ ] Add model fog fields if needed for the actor-source investigation.
- [ ] Record installation provenance and explain the packed color format.
- [ ] Clearly label record minima/maxima versus time-specific interpolated
  runtime values. The summary's minimum far distance is not necessarily the
  noon or midnight value shown in a capture.
- [ ] Refresh the saved inventory only from an identified input installation.

Acceptance: reports are reproducible, malformed or incomplete data cannot be
silently mistaken for complete coverage, and summary values are interpretable.

## 13. P3: retail reference comparison and optional effects

### 13.1 Establish matched reference scenes

- [ ] Obtain suitable reference captures with identified zone, weather, time,
  camera position/orientation, draw distance, and relevant graphics settings.
- [ ] Match those conditions in DATura before comparing fog color or strength.
- [ ] Separate fog differences from exposure, lighting, missing clouds, LOD,
  geometry, and particle differences.
- [ ] Record discrepancies and confidence instead of claiming parity from
  a generally similar atmosphere.

Acceptance: each fidelity claim is supported by a matched reference, with
remaining differences documented.

### 13.2 Treat additional mist effects as separate work

The implemented change concerns ordinary authored distance fog. It does not
add drifting ground mist, localized fog volumes, volumetric light scattering,
or a general particle-effect playback system.

- [ ] Determine whether a reported missing effect is distance fog, an authored
  mesh/particle effect, or another rendering technique.
- [ ] Identify its source resources and playback rules before implementation.
- [ ] Prepare a separate scoped plan if such effects are required.

Acceptance: extra effects are evidence-led and do not replace correct authored
distance fog with arbitrary per-zone visual presets.

## 14. Reproduction commands

Run these from the repository root. Paths below reflect this machine; replace
the installation/tool paths on another system.

### 14.1 Audit the installed zone files

```powershell
python tools/audit_zone_fog.py `
  'C:/Program Files (x86)/PlayOnline/SquareEnix/FINAL FANTASY XI' `
  --output artifacts/zone-fog/new-audit
```

### 14.2 Build the test executable

```powershell
$fogMsbuild = 'C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe'
& $fogMsbuild tests/MaterialRenderingTests.vcxproj `
  /p:Configuration=Release /p:Platform=x64 `
  /p:OutDir=C:/Users/nanob/source/repos/DATura/artifacts/zone-fog/tests/ `
  /p:IntDir=C:/Users/nanob/source/repos/DATura/artifacts/zone-fog/obj/ `
  /v:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw 'Fog test build failed' }
```

The recorded local PDB mismatch no longer reproduces (section 6), so this is
normally unnecessary. If it returns, the prior verification added this argument
to the build invocation:

```text
/p:ForceImportAfterCppTargets=C:/Users/nanob/source/repos/DATura/artifacts/sun-shadow-no-pdb.targets
```

That artifact contained:

```xml
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemDefinitionGroup>
    <Link><GenerateDebugInformation>false</GenerateDebugInformation></Link>
    <ClCompile><DebugInformationFormat>None</DebugInformationFormat></ClCompile>
  </ItemDefinitionGroup>
</Project>
```

Do not describe a workaround build as a successful test of symbol generation.

### 14.3 Run focused fog and installed-zone checks

```powershell
$fogTests = './artifacts/zone-fog/tests/MaterialRenderingTests.exe'
& $fogTests --materials-only artifacts/zone-fog/new-run/materials
if ($LASTEXITCODE -ne 0) { throw 'Material/fog checks failed' }

& $fogTests --zone-fog `
  'C:/Program Files (x86)/PlayOnline/SquareEnix/FINAL FANTASY XI' `
  artifacts/zone-fog/new-run/zones
if ($LASTEXITCODE -ne 0) { throw 'Installed-zone fog checks failed' }
```

### 14.4 Reproduce the shadow and full-suite results separately

```powershell
& $fogTests --shadow-temporal artifacts/zone-fog/new-run/shadow-temporal
if ($LASTEXITCODE -ne 0) { Write-Warning 'Focused shadow checks failed' }

& $fogTests artifacts/zone-fog/new-run/full-suite
if ($LASTEXITCODE -ne 0) { throw 'Full material/rendering suite failed' }
```

The known counts are historical observations, not fixed expected totals for
future versions of the suite. Investigate failures by assertion and scenario.

### 14.5 Weather/time transition tests

These need no device or DAT installation:

```powershell
& $fogMsbuild tests/ZoneFogTransitionTests.vcxproj `
  /p:Configuration=Release /p:Platform=x64 `
  /p:OutDir=C:/Users/nanob/source/repos/DATura/artifacts/zone-fog/transition/ `
  /p:IntDir=C:/Users/nanob/source/repos/DATura/artifacts/zone-fog/obj-transition/ `
  /v:minimal /nologo
& ./artifacts/zone-fog/transition/ZoneFogTransitionTests.exe
if ($LASTEXITCODE -ne 0) { throw 'Fog transition checks failed' }
```

## 15. Evidence and code map

| File or directory | Purpose |
| --- | --- |
| `Markdown Documents/ZONE_FOG.md` | Existing implementation and verification notes |
| `tools/audit_zone_fog.py` | Regenerates the authored terrain-fog inventory |
| `tools/zone_fog_inventory.csv` | Saved per-zone inventory |
| `DATura/main.cpp` | World/preview pass policy and actor fog opt-in |
| `DATura/model_renderer.cpp` | Shared model-pass fog setup |
| `DATura/zone_environment_fog.cpp` | Dedicated environment fog setup |
| `DATura/zone_environment_state.cpp` | Authored time/weather selection and interpolation |
| `DATura/model_ff11.cpp` | Environment record parsing |
| `DATura/d3d_bump_mapping.cpp` | Texture/bump pixel shader and wrapper ownership |
| `DATura/directional_shadow_map.inl` | Shadow implementation and fog attenuation |
| `tests/material_rendering_tests.cpp` | Material, fog, depth, and shadow fixtures/modes |
| `tests/installed_zone_fog_cases.h` | Six real-zone capture cases and pixel checks |
| `tests/zone_fog_transition_tests.cpp` | Weather/time/zone transition checks of `ZoneEnvironmentState` |
| `artifacts/zone-fog/verify-2026-10-16/` | Snapshot source, manifest, binaries, and logs of the follow-up run |
| `artifacts/zone-fog/materials.log` | Recorded 226-assertion focused pass |
| `artifacts/zone-fog/captures.log` | Recorded 277-assertion installed-zone pass |
| `artifacts/zone-fog/regression.log` | Recorded full run with five shadow failures |
| `artifacts/zone-fog/captures/metrics.csv` | Per-zone/time/weather fog metrics |
| `artifacts/zone-fog/captures/day-comparison.jpg` | Reviewed daytime contact sheet |
| `artifacts/zone-fog/captures/night-comparison.jpg` | Reviewed nighttime contact sheet |

Generated artifacts may be replaced or absent in another checkout. Preserve
needed evidence before rerunning into the same directory.

## 16. Completion checklist

For the ordinary authored-fog feature to be considered fully integrated:

- [x] A recorded source state produces a successful application and test build.
- [x] Focused fog/material checks pass on that same source state.
- [ ] Real player/NPC behavior is verified through the normal scene path.
- [ ] Preview isolation and zone-change state reset are verified in the app.
- [x] Weather/time transitions and off-sentinel behavior have deterministic tests.
- [ ] Representative open-distance, forest, water, and interior scenes are reviewed.
- [ ] Capture metadata and generation steps are reproducible.
- [x] The full regression suite is green, or remaining failures are explicitly
  tracked with current evidence and ownership; fog success is reported separately.
- [x] The local PDB issue is repaired or a clearly labeled workaround is retained.
- [ ] Documentation states the limits of zone coverage and retail parity.

Retail-reference parity and additional ground-mist effects have their own
acceptance criteria above. They should not be silently added to the completion
claim for the existing distance-fog implementation.
