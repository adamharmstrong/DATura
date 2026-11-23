# Collision and renderer handoff: remaining work

Recorded on October 16, 2026. Workspace: `C:\Users\nanob\source\repos\DATura`.

## 1. Current status and immediate next action

The collision implementation no longer depends on recognizing plant names or
foliage textures. Static zone collision is decoded from authored MZB records.
The remaining immediate work is to validate the current renderer implementation,
produce a fresh Release build from the latest files, and test that exact build
interactively with the collision changes.

**Important correction to the previous handoff:** the build started immediately
before the user interrupted the renderer-fix request completed successfully in
the background. The old abstract-renderer error is therefore a historical
failure, not a confirmed current blocker.

Evidence inspected while writing this document:

| Item | Observed state |
|---|---|
| `artifacts/renderer-rebuild.log` | Ends with successful code generation, a `DATura.vcxproj -> ...\DATura.exe` output line, and resource-copy output. No build failure is shown in that log. |
| `artifacts/grass-collision-app/DATura.exe` | Exists; size 3,712,000 bytes; modified `2026-10-16T13:34:44Z`. |
| `DATura/d3d9_graphics.h` | Modified `2026-10-16T13:43:23Z`, later than that executable. |
| Renderer adapter methods | The previously missing transform and state-query overrides are present in the current header. |
| Fresh build after the latest header modification | Not performed during this documentation request. |
| Interactive test of the resulting application | Still pending. |

Do not tell the user that no executable exists. Also do not present the existing
executable as verified against the latest renderer header. Rebuild first, retain
the log, and identify the exact executable that was tested.

## 2. User objective

The user wants to walk through foliage where the official client allows it,
without losing intentionally solid trunks, hedges, branches, terrain, or other
surfaces. They also want a working app build to test these changes themselves.

The user explicitly rejected an implementation limited to known plant names.
Do not restore lists of names, texture-name patterns, transparency checks, or
wind-animation checks as the source of static collision eligibility.

The work is complete when:

- [ ] The current application builds successfully with the collision changes.
- [ ] The renderer adapter passes relevant state/resource tests.
- [ ] The rebuilt application starts and renders a usable scene.
- [ ] Basic foliage and solid-surface movement is checked in that application.
- [ ] Door and elevator behavior remains usable in the rebuilt application.
- [ ] The user receives the path to the exact tested executable and any remaining limitations.

Exact cross-platform PS2 asset parity is a separate, stronger claim. See section
10 before describing this implementation as a complete PS2 collision emulator.

## 3. Collision work already implemented

### 3.1 Static geometry uses authored collision

`DATura/model_ff11_map_handlers.h` decodes the separate MZB collision grid.
The MMB rendering paths no longer append their visible triangles to the runtime
collision vector. This applies to all static objects, including unknown foliage,
opaque objects, cutout objects, and mixed meshes.

The previous generic visual-triangle collector and its collision-like-name
helper were removed. The earlier `ffxi_foliage_collision.h` classification header
was also removed, together with its project entries.

This means:

- Unknown foliage does not become solid simply because it renders.
- Known foliage is not automatically made non-solid if authored collision exists.
- Solid plant components remain solid according to their collision mesh.
- A visible static model with no authored collision does not acquire an inferred
  solid surface as a fallback.
- Render LOD, texture alpha, draw sidedness, and wind data do not determine static collision.

`collectCollisionUnreferenced` remains in `ff11Opts_t` only for existing caller
compatibility. Its comment says it no longer enables inferred helper/visual
collision. Production scene loading no longer sets that obsolete option.
Removing the field and updating every caller is optional cleanup, not necessary
to finish the user's request.

### 3.2 Authored collision decoder corrections

The source review uncovered additional bugs in the existing PC collision reader:

1. A zero collision-table pointer or grid pointer is treated as absent data.
2. Grid cell counts now use authored dimensions and bucket sizes:
   - Width: `header[0x0c] * header[0x0e] / 4`.
   - Height: `header[0x0d] * header[0x0f] / 4`.
3. Bucket lists use their declared pair count, stored in the low 14 bits of the
   bucket header. They are not read until a zero terminator is encountered.
4. Mesh triangle counts are read as unsigned 16-bit values.

Installed data contains 20-, 40-, and 80-unit bucket sizes. The old fixed
multiplier of ten missed collision in nonstandard grids, including Port Bastok
and Port Jeuno. The old zero-terminated-list assumption could consume data
outside a bucket's declared entries.

### 3.3 Moving solids have explicit registration

`Model_FF11_CollectMovingObjectCollision` is declared in `model_ff11.h` and
implemented in `model_ff11.cpp`.

Its callers are:

- Door registration in `DATura/zone_door_interaction.cpp`.
- The existing two Metalworks elevator bindings in `DATura/main.cpp`.

The helper collects only a registered object's highest-detail available LOD.
It converts scene-space model vertices back to native coordinates, undoing both
position reflection and winding reversal where necessary. Repeated registration
does not append duplicate geometry while the object's collision range is set.

This is deliberately separate from static foliage handling. The existing door
and elevator discovery/binding behavior has been preserved; it has not been
reconstructed as an exact PS2 dynamic-object collision implementation.

## 4. Source and evidence locations

| File or directory | Purpose |
|---|---|
| `DATura/model_ff11_map_handlers.h` | Authored collision grid decoding and MMB rendering paths. |
| `DATura/model_ff11.cpp` | Removed unreferenced visual collision pass; explicit moving-solid collector. |
| `DATura/model_ff11.h` | Collision option documentation and moving-solid helper declaration. |
| `DATura/scene_model_loader.cpp` | Runtime parser options. |
| `DATura/zone_door_interaction.cpp` | Explicit moving door registration. |
| `DATura/main.cpp` | Existing Metalworks platform bindings now explicitly register moving collision. |
| `tests/vegetation_tests.cpp` | Synthetic collision fixtures, installed foliage checks, full-zone audit, and lift checks. |
| `tests/coordinate_frame_tests.cpp` | Authored room collision provenance and NPC grounding across coordinate orientations. |
| `tools/audit_authored_collision.py` | Separate raw-MZB count/index audit. |
| `tools/inspect_zone_collision.py` | Shared chunk reading/decryption used by the independent audit. |
| `Markdown Documents/GRASS_COLLISION.md` | Technical findings; its final build-status paragraph is stale and needs updating. |
| `artifacts/authored-collision-audit.csv` | Runtime audit of installed zone entries. |
| `artifacts/authored-collision-independent.txt` | Independent raw-record comparison result. |
| `artifacts/authored-room-results.txt` | Room-loading test output. |
| `artifacts/renderer-rebuild.log` | Successful background build log from the interrupted turn. |

PS2 reference source:

`C:\Users\nanob\source\repos\FFXI-PS2\src\ohno\ko_chara.cpp`

Relevant functions are `KO_MapLocation::CheckChips` and `CheckParts`. They select
collision groups using placement flags and submit selected polygons for
collision testing. `include/KO_Vum.h` documents the separate triangle,
transparent, and collision groups.

A separate PC-format reference is available locally at:

`tmp/xi-test-client-review/renderer/ffxi/mzb.cpp`

It describes authored grid dimensions and counted bucket lists. The independent
audit checks actual installed records rather than treating this reference alone
as proof of correctness.

## 5. Renderer status: inspect before changing more code

The earlier application build failed because `D3D9Graphics::Context` was abstract
while the shared `RendererBackend::Graphics` interface required additional
methods. Those failures came from constructing the context in application code.

Current `DATura/d3d9_graphics.h` now contains the relevant overrides, including:

- `SetTransform`, `QueryTransform`, and `QueryTextureTransform`.
- `SetViewport` and `QueryViewport`.
- `QueryFogState`.
- `QueryPipelineState` and `QuerySamplerState`.
- `QueryStencilState` and `QueryTextureStageState`.
- `QueryLightingState`.

The successful background build indicates the renderer had a compiling
implementation at that snapshot. The header subsequently changed again.

Remaining renderer work:

- [ ] Reproduce the current state with a fresh build; do not assume the historical abstract-class error still exists.
- [ ] If compilation fails, use the new log's first actionable error rather than the earlier output.
- [ ] Compare required virtual signatures in `renderer_resources.h` with the adapter's exact overrides.
- [ ] Verify queries leave their output untouched on failure, as required by the interface contract.
- [ ] Verify native state conversions, enum mapping, and matrix conventions through existing tests.
- [ ] Avoid adding placeholder success returns or silently ignoring unsupported states to get a build through.
- [ ] Preserve unrelated renderer work already present in the workspace.

No claim is made here that all of these behaviors are faulty. These are the
checks still required to validate the current implementation.

## 6. Priority 1: rebuild the latest application

### 6.1 Build environment

The installed tools used successfully in this work are:

- Visual Studio Community 18.
- MSBuild at `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe`.
- Developer setup at `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat`.
- Release / x64 configuration.

An earlier build failed with `LNK1101: incorrect MSPDB140.DLL version`. That no
longer reproduces: on 2026-10-16, Release and Debug x64 builds with debug
information linked and produced `DATura.pdb`, even outside VsDevCmd (logs in
`artifacts/lnk1101-check/`). If it returns, run
`Get-Process mspdbsrv | Select Id,Path` and end any `mspdbsrv` that is not from
the toolset in use before rebuilding.

### 6.2 Reproducible application command

Run from the repository root in PowerShell:

```powershell
Set-Location 'C:\Users\nanob\source\repos\DATura'

cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && msbuild DATura\DATura.vcxproj /p:Configuration=Release /p:Platform=x64 /p:LinkIncremental=false /p:OutDir=C:\Users\nanob\source\repos\DATura\artifacts\grass-collision-app\ /p:IntDir=C:\Users\nanob\source\repos\DATura\artifacts\grass-collision-obj\ /v:minimal /nologo' > artifacts/renderer-rebuild-latest.log 2>&1

$buildExit = $LASTEXITCODE
Get-Content artifacts/renderer-rebuild-latest.log -Tail 60
if ($buildExit -ne 0) { throw "Application build failed: $buildExit" }
```

These are the build settings used for the successful prior application build,
except that it also passed `/p:GenerateDebugInformation=false`, which is no
longer needed.
The output executable is:

`C:\Users\nanob\source\repos\DATura\artifacts\grass-collision-app\DATura.exe`

Acceptance criteria:

- [ ] Build exits successfully and produces a new executable.
- [ ] The log contains the final application output line and no errors.
- [ ] The executable timestamp is later than the relevant current source files.
- [ ] If source files change while building, rebuild the affected changes before treating the output as current.
- [ ] Required data files are present beside the executable.

The existing output directory includes `map_dats.json`, `npc_placements.csv`,
`npc_roles.csv`, `npc_starter_quests.csv`, and `DATura.game-ui.config`. Confirm
that the project still copies the current required resources; do not replace
the user's settings unnecessarily.

## 7. Priority 2: validate the renderer and current collision binaries

Use a Visual Studio x64 developer terminal for the following test builds. Give
each project a separate intermediate directory if overriding its defaults.
Do not share one intermediate directory across test projects.

### 7.1 Renderer backend tests

Project: `tests/RendererBackendTests.vcxproj`.

Its checked-in default Release output is under `tests/bin/renderer-backend/`.
The test covers renderer lifecycle, unsupported backends, resource behavior,
postprocessing paths, state restoration, presentation, overlays, and resize.

```powershell
msbuild tests\RendererBackendTests.vcxproj /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo
& tests/bin/renderer-backend/RendererBackendTests.exe
```

- [ ] Build against the latest renderer headers.
- [ ] Run on a system with a working D3D9 HAL device.
- [ ] Retain output and confirm successful exit.
- [ ] Investigate any resource or state-restoration failures before launching the app for user testing.

### 7.2 Material/rendering checks

Project: `tests/MaterialRenderingTests.vcxproj`.

Review its current command-line modes and output directory before running. It
includes material, alpha, depth ordering, fog, and shadow tests. A
`--materials-only` mode exists for material/depth/fog checks without the full
shadow suite.

- [ ] Run the relevant material checks after the renderer adapter is confirmed current.
- [ ] Inspect any generated captures that are needed to explain a failure.
- [ ] Expand testing only if a renderer change or observed failure warrants it.

### 7.3 Collision checks already passed

These results were obtained during the collision work, before the most recent
renderer-header edits. They are evidence for the collision implementation, not
proof that every current test executable matches the latest workspace.

| Check | Recorded result |
|---|---|
| Synthetic geometry/collision fixtures | Passed across list/strip topology, static/wind layouts, arbitrary names, draw flags, and absent/present authored collision. |
| Grid/list boundary fixtures | Passed; valid-looking data outside the declared grid or pair count is ignored. |
| Explicit moving-solid helper | Passed idempotence and reflected-position/winding checks. |
| Full installed-zone runtime audit | 298 entries; 1,023,541 static placements; 97,282,749 authored triangle references; zero failures. |
| Independent raw-MZB audit | Agreed for all 298 entries; validated declared counts and vertex-index bounds. |
| Empty zone entries | 278 and 279 contained no scene/collision data; not silently counted as populated scenes. |
| Door tests | 101 checks passed, including 30 Bastok Markets leaves and 15 opened paired doorways. |
| Room loading | Existing city companion-room fixture set passed. |
| Zone-transition movement | All 28 existing cases passed, including both coordinate orientations. |
| Coordinate/grounding checks | Passed; 111 of 112 Bastok Markets catalog NPCs found floors. |
| Metalworks moving platforms | Both actual bindings registered collision in native and reflected scenes. |

The 97-million figure counts repeated grid references. It is not the number of
unique authored triangles or unique obstacles.

### 7.4 Useful regression commands

Installed game root used for the recorded tests:

`C:\Program Files (x86)\PlayOnline\SquareEnix\FINAL FANTASY XI`

The following existing artifact paths may contain older binaries. Rebuild the
corresponding projects before using them to validate a newer source change.

```powershell
$gameRoot = 'C:\Program Files (x86)\PlayOnline\SquareEnix\FINAL FANTASY XI'

& artifacts/grass-collision-tests/VegetationTests.exe `
    "$gameRoot/ROM/0/90.DAT" `
    "$gameRoot/ROM/0/115.DAT" `
    --static "$gameRoot/ROM/0/120.DAT" `
    --foliage "$gameRoot/ROM/1/43.DAT" `
    --foliage "$gameRoot/ROM3/0/23.DAT" `
    --lifts "$gameRoot/ROM/1/37.DAT"

& artifacts/authored-door-tests/DoorInteractionTests.exe "$gameRoot/ROM/1/35.DAT"
& artifacts/authored-room-tests/ZoneRoomTests.exe $gameRoot
& artifacts/authored-transition-tests/ZoneTransitionCollisionTests.exe $gameRoot
& artifacts/authored-coordinate-tests/CoordinateFrameTests.exe $gameRoot
```

Project mappings:

- `VegetationTests.vcxproj` -> foliage, synthetic, audit, and lift checks.
- `DoorInteractionTests.vcxproj` -> door tests.
- `ZoneRoomTests.vcxproj` -> companion-room tests.
- `ZoneTransitionCollisionTests.vcxproj` -> movement/transition checks.
- `CoordinateFrameTests.vcxproj` -> coordinate and NPC grounding checks.

If collision decoding changes again, rerun the full raw-data comparison:

```powershell
& artifacts/grass-collision-tests/VegetationTests.exe --audit-all $gameRoot `
    > artifacts/authored-collision-audit.csv
if ($LASTEXITCODE -ne 0) { throw 'Runtime zone audit failed' }

python tools/audit_authored_collision.py $gameRoot artifacts/authored-collision-audit.csv `
    > artifacts/authored-collision-independent.txt
if ($LASTEXITCODE -ne 0) { throw 'Independent MZB comparison failed' }
```

Keep both outputs. A provenance check of already-exported triangles alone could
miss a decoder that silently omitted authored records; the independent count
comparison addresses that gap. It still does not compare against PS2 assets.

## 8. Priority 3: launch and smoke-test the exact rebuilt app

No interactive test of the newly rebuilt app was completed in this conversation.
Automated parser and physics tests do not establish that the application UI,
renderer state transitions, settings, and selected executable work together.

- [ ] Launch the rebuilt executable with its output directory as the working directory.
- [ ] Confirm the configured FFXI installation is found.
- [ ] Confirm the title scene is visible and the UI remains responsive.
- [ ] Load a zone and verify terrain, foliage, NPCs, and ordinary UI render.
- [ ] Resize the window and verify rendering recovers.
- [ ] Return to the title screen and load another zone.
- [ ] Close normally and check the exit code or crash output.

`tools/test_renderer_runtime.ps1` is an existing automated smoke script. It loads
scenes, captures images, resizes, returns to the title, and checks shutdown.

**Important path mismatch:** that script currently launches
`x64/<Configuration>/DATura.exe`, whereas the commands above build into
`artifacts/grass-collision-app/`. Do not run the script unchanged and assume it
tested the new artifact. Either add an explicit executable-path option, build to
the script's expected location deliberately, or use an equivalent test that
records the actual artifact path. Preserve unrelated binaries and configuration.
The script itself says its captured images still require visual inspection.

## 9. Priority 4: interactive collision test matrix

Use real player movement, not only the free camera. Confirm which executable and
which coordinate/mirroring setting are active before reporting a result.

| Zone | What to exercise | Expected observation |
|---|---|---|
| Konschtat Highlands (`ROM/0/90.DAT`) | Grass cards and broad flower sheets; nearby terrain and rock structures. | Visible cards do not create extra walls or artificial floors; authored solid surfaces still support/block movement. |
| La Theine Plateau (`ROM/0/115.DAT`) | Grass, trees, foliage around uneven ground, and mixed visual meshes. | Movement follows authored collision rather than render-triangle shape. |
| West Ronfaure (`ROM/0/120.DAT`) | Small shrubs, tree foliage, trunks, and slopes. | Unknown-name coverage is automatic; intentionally solid authored trunks remain solid. |
| Lufaise Meadows (`ROM3/0/23.DAT`) | Additional plant families and mixed tree/plant meshes. | No dependency on the earlier naming list or wind layout. |
| Selbina (`ROM/1/43.DAT`) | Foliage near structures and companion rooms. | Plant cards do not add collision; town geometry and rooms remain usable. |
| Port Bastok (`ROM/1/33.DAT`) | General floor/wall movement and room entrances. | Nonstandard grid dimensions no longer omit authored collision. |
| Port Jeuno (`ROM/1/42.DAT`) | General floor/wall movement and room entrances. | The 80-unit bucket layout produces usable authored collision. |
| Bastok Markets (`ROM/1/35.DAT`) | Both sides of several doors, including upstairs thresholds. | Closed doors block, opened doors clear, and surrounding static walls/floors do not move. |
| Metalworks (`ROM/1/37.DAT`) | Both elevator platforms during travel and boarding. | The player stays supported and can enter/leave the platforms without duplicate static surfaces trapping them. |

For foliage findings:

- [ ] Check both movement through the foliage and standing near its base.
- [ ] Check that removed artificial flower/card floors do not cause unexpected snapping.
- [ ] Check mixed objects containing both plant decoration and solid components.
- [ ] Check nearby floors, ceilings, walls, slopes, and small ledges.
- [ ] Repeat representative scenes with the alternate coordinate setting if that setting is supported for user testing.
- [ ] Distinguish camera occlusion, click-to-move selection, and physical player blocking when reporting a symptom.

Do not assume every plant should be passable. If an object still blocks, locate
its authored collision and investigate that record before changing anything.
Do not remove a collision surface merely because its appearance is foliage.

For any failure, record the zone, position, movement direction, settings,
expected result, actual result, and collision-mesh evidence. A short reproducible
case is more useful than a broad assertion that a foliage family is wrong.

## 10. Remaining PS2 parity questions and limits

What is supported now:

- The PS2 source selects authored geometry/groups for collision.
- DATura's static collision no longer uses plant naming or appearance rules.
- The PC authored collision decoder agrees with an independent reader across
  the installed audit set.

What is not established:

- Bit-for-bit equivalence between PC zone collision assets and the corresponding PS2 release's assets.
- Complete reproduction of the PS2 movement solver, contact response, slope/step handling, or all collision-query modes.
- Exact PS2 parity for dynamic doors, elevators, actor collision, or event-controlled solids.
- Exhaustive semantic interpretation of every packed collision attribute.

For a stronger PS2-parity claim, obtain matched PS2 zone assets and identify the
client version they belong to. Compare authored geometry, placements, selection
flags, and representative movement queries for the same locations. Separate
asset differences from decoder bugs and from movement-solver differences.

This additional research is not necessary to give the user a working test build,
but it is necessary before claiming that every collision behavior exactly
matches the PS2 client. Do not substitute more naming conventions for missing
cross-platform evidence.

## 11. Documentation and delivery still needed

- [ ] Update the final paragraph of `Markdown Documents/GRASS_COLLISION.md`; it currently says the full build is blocked by an abstract context, which is no longer an accurate statement of the latest observed build log.
- [ ] Record the fresh rebuild result and the exact executable path.
- [ ] Add renderer test and runtime smoke results only after they are actually run.
- [ ] Keep automated collision results separate from interactive observations.
- [ ] Tell the user whether the supplied build was actually launched and tested.
- [ ] Link the executable once it is current and usable, with its adjacent runtime resources in place.
- [ ] Mention any remaining concrete failures or PS2 comparison limits plainly.

Do not report renderer fixes as authored in this conversation unless their actual
changes were made and verified here. The current adapter already contained the
missing methods when the renderer-fix turn inspected it.

## 12. Workspace precautions and optional cleanup

The working tree already contains extensive unrelated renderer, UI, gameplay,
and test changes. Some are modified tracked files; others are untracked source
files that participate in the current project.

- Do not reset, stash, overwrite, or discard those changes to obtain a clean build.
- Keep new edits focused on reproducible build/runtime failures.
- Do not delete the current output directory while another process may be using its executable.
- Preserve the user configuration unless a specific setting is shown to cause a failure.
- Use distinct build intermediates for the app and each test project.
- Do not assume a test binary is current just because it exists.

Optional follow-ups after the working build is delivered:

1. Remove the obsolete `collectCollisionUnreferenced` compatibility field and its remaining test assignments in a separate cleanup.
2. Consider renaming visual-collision debug fields to make their moving-solid-only role clearer, updating all room/door/elevator consumers together.
3. Make the renderer smoke script accept an explicit executable path to prevent stale-binary testing.
4. Investigate whether repeated grid collision references should be deduplicated for performance, while preserving transforms, attributes, and moving-object bindings. The audit's large reference count is not itself proof of a performance defect.
5. Expand documented PS2 parity research only when matched assets or direct runtime evidence are available.

## 13. Suggested order for the next work session

1. Inspect current source timestamps and preserve unrelated working-tree edits.
2. Build the latest Release/x64 application and inspect the new log.
3. Resolve only errors that still reproduce; the historical abstract-context failure may already be resolved.
4. Run the renderer backend tests and appropriate material checks.
5. Rebuild/rerun relevant collision tests if the decoder or moving-solid code changes.
6. Launch the exact output artifact and perform the runtime smoke checks.
7. Walk through the foliage/solid-surface matrix, including doors and elevators.
8. Correct stale build-status documentation.
9. Deliver the current executable path with a concise statement of what was tested and what remains unverified.
