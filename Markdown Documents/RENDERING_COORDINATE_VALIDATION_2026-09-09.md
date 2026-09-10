# Rendering and coordinate validation

Phase 2 of the reference implementation plan is implemented. It preserves DATura's
existing rendering frame and adds regression coverage and a zone inspection tool.
The implementations are original; upstream research informed the questions and
test cases rather than supplying copied renderer code.

## Coordinate contract

`DATura/ffxi_coordinate_frame.h` defines native DAT to scene conversion and its
inverse. DATura keeps native Y (positive downward) and Z. Normal viewing reflects
X, including normals and vegetation displacement; triangle winding reverses once.
The existing **Mirror World Zones** setting exposes native coordinates, so call
sites pass `!mirrorWorldZones` when asking to apply the reflection. Model and
collision installation, catalog NPCs, and native visibility query points now share
this contract. Native LOD and visibility metadata remain unchanged.

The `(x, -y, -z)` convention in XI-Test-Client belongs to that renderer. Applying
it again to DATura's placed geometry would change the scene incorrectly. Player
camera-relative yaw is also distinct from catalog NPC heading and was preserved.

`CoordinateFrameTests` checks basis vectors, inverse transforms, winding/normals,
animation bind vertices, vegetation, heading wrap, nonuniform scale with nonzero
XYZ rotations, and zero water scale. The installed Bastok Markets fixture loads
14 room DATs, compares 4,353 mesh samples and 16 room/collision placements, and
checks identical grounding results for all 112 catalog NPCs across both frames.
111 have a floor within the existing query range; the remaining NPC has the same
no-floor result in both orientations.

## Material-state correction

Actual GPU tests exposed a textured-to-untextured state leak in both opaque and
transparent material binding. The previous sampling shader remained active and
produced black or invisible geometry. Both paths now clear it when there is no
D3D texture. `MaterialRenderingTests` passes all 157 assertions after reproducing
four failures before the correction. See `tests/MATERIAL_RENDERING.md`.

## Zone Geometry Diagnostics

Open **Resources > Zone Geometry Diagnostics...** after loading a zone. The window
captures the loaded zone and rooms, the current renderer LOD query, hidden objects,
runtime transforms, and current collision geometry. Analysis runs on an owned
snapshot in a background worker, so changing zones or closing the window cannot
leave dangling scene pointers. Reopen the window to capture a changed scene.

Set cell size and maximum surface slope; optionally set an increasing Y interval
to inspect a floor. Smaller Y means higher altitude. Click **Analyze**, then save
an HTML report or CSV. The HTML report works offline and offers layer toggles,
pan/zoom, fit-to-map, and per-cell coordinate inspection. CSV contains occupied
cells only, with scene-space bounds and independent render/collision/water flags.
Exported files contain occupancy data, not game textures or triangle assets.

Every layer uses the same grid. The engine rasterizes full projected triangles,
including conservative edge/corner contacts, instead of sampling vertices. It
clips triangles against the height interval before projection, handles either
winding, rejects malformed or degenerate geometry, and bounds work/allocation.
An oversized grid increases cell size and reports the actual resolution. Exceeding
the triangle or cell-test budget reports a failure instead of a partial success.

The report lists material categories and source omissions. Intentionally
untextured geometry is distinct from unresolved named references. Camera frustum
and PVS are deliberately excluded from this whole-zone snapshot. Sky, weather
particles and actors are excluded; vegetation is static. Water is an independent
layer and does not count as structural render coverage.

This is a diagnostic projection, not a walkability metric or a renderer fidelity
score. Ceilings, canopies and stacked floors overlap, and runtime collision can
include visual-derived room/terrain triangles as well as native collision. A
render/collision match is therefore not independent proof of retail correctness.
Hidden objects, current LOD/draw distance, and edited geometry can intentionally
differ from collision. Reanalysis at a finer cell size or a narrower Y interval
helps examine an apparent discrepancy.

## Verification

Build each test `.vcxproj` with `/p:Configuration=Release /p:Platform=x64` using the
same MSBuild as DATura, then run from the repository root:

```powershell
tests/bin/zone-coverage/Release/ZoneCoverageTests.exe
tests/bin/coordinate-frame/Release/CoordinateFrameTests.exe 'C:/Program Files (x86)/PlayOnline/SquareEnix/FINAL FANTASY XI'
tests/bin/material-rendering/Release/MaterialRenderingTests.exe artifacts/material-validation
tests/bin/zone-diagnostics/Release/ZoneDiagnosticsTests.exe 'C:/Program Files (x86)/PlayOnline/SquareEnix/FINAL FANTASY XI' artifacts/zone-diagnostics
```

- Coverage: 80/80 analytic checks including alternate tessellations, reflection,
  thin triangles, height clipping, malformed inputs, empty grids and budgets.
- Diagnostics: 20 checks covering snapshot lifetime, hidden/LOD/environment
  selection, runtime transforms, separate water, missing geometry, escaped source
  names, CSV consistency, invalid export rejection and installed zone reports.
- Installed Bastok snapshot: 526,512 triangles, no missing CPU geometry or invalid
  indices; full-height grid 166 x 205 at cell size 4. Height slice [-4, 4] also
  completes within budget. Generated HTML was inspected in the browser, including
  layer toggling, zoom, and cell coordinate feedback.
- Material/depth/fog: 157 assertions, zero failures after the shader correction.
- Existing water suite: installed East Ronfaure, Valkurm and Bastok Markets pass;
  actor composition matches its reference exactly.
- DATura Debug/x64 and Release/x64 builds pass with the new menu and dialog.

GPU tests need desktop device access; SDK discovery also needed the normal
unsandboxed Visual Studio environment on the development machine. No retail files
were modified. Native dialog layout and save-dialog interaction were not visually
automated; the compiled UI uses the tested snapshot, analysis and export code.

The next planned phase is the common DAT replacement resolver. Broader installed
zone comparisons and finite-lifetime foam/spray remain follow-up validation work.
