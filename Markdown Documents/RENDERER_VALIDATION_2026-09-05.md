# Renderer validation — September 5, 2026

## PS2 conformance additions — 2026-09-20

Add the following source-derived validation cases:

- zone name beginning with `_` enables hard alpha;
- PS2 zone alpha reference converts to normalized threshold `0.375`;
- PS2 character alpha-tested groups use approximately `0.375`, not the later-PC `69/255` value;
- ordinary and transparent groups remain separate through submission;
- transparent groups use their own test/blend state and are drawn after ordinary and shadow-designated zone geometry;
- placed-part LOD exercises both boundaries, clipping, and missing-high fallback;
- grid-chip LOD changes at squared grid distance `5`;
- character and background fog environments can differ;
- weather interpolation covers fog, ambient/light values, focus, clip range, and sky parameters;
- character environment-map and specular passes remain distinct; and
- framebuffer-derived background light maps do not collapse into bump mapping or ordinary alpha overlays.

Tested the current working tree after Slice 11c. No application source changed
during this validation. The repeatable harness is
`tools/test_renderer_runtime.ps1`.

## Automated checks

- Debug and Release x64 application builds: passed with warnings treated as errors.
- Existing Debug and Release logic executables: 219/219 checks passed in each.
  These checks do not validate rendered pixels.

## Runtime checks

The harness starts its own hidden process and targets only that process's window.
It sends the existing menu commands for Lion (NPC 10000), Jeuno Mog House (3000),
and West Ronfaure (3100), waits between transitions, checks responsiveness,
resizes to 1000 × 740, returns to title, and closes normally.
It leaves the saved installation path and FFXI assets unchanged.

- Debug: completed this sequence and exited with code zero.
- Release: completed the same sequence and exited with code zero.
- `git diff --check`: passed.

## Visual limitation

The inspected Debug actor and Release outdoor captures made with `PrintWindow`
were black even though the API reported
success. Hidden-window capture does not provide usable evidence of this D3D9
scene. Responsiveness and successful command delivery do not prove that assets
rendered correctly or that pixels match the pre-refactor output.

Consequently this run does **not** sign off visual parity, texture/alpha handling,
transparency ordering, shadows, lighting tiers, object overrides, mirroring,
weather, or fullscreen/device-reset recovery. Window resizing was exercised,
but correct pixels after the resize remain unverified. Slice 11's visual gate
remains open.

The generated captures and menu inventories were removed during cleanup because
the black captures were not usable visual baselines. Rerunning the harness
recreates `artifacts/renderer-smoke-Debug` or `artifacts/renderer-smoke-Release`;
these output folders are ignored by Git. The harness and this validation record
are retained.
