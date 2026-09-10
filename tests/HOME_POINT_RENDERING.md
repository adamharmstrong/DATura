# Home Point rendering

Home Points (appearance `0x33`) use the installed client's logical DAT 1351,
normally `ROM/3/25.DAT`. DATura loads its five effect meshes and two sprite
resources, textures, generator parameters, animation curves, and idle/activation
schedules. It renders the transparent layers after opaque actors and zone
transparency, with depth testing and restored D3D state.

The crystal rotates with its authored animation; UV scrolling, aura emissions,
ground rings, and activation particles use the DAT's 60 Hz timing. Selecting and
interacting with a Home Point within six world units triggers its activation
effect, with a one-second cooldown. This does not implement teleport menus or
change the player's saved home point.

The DAT's sound references resolve to `se009013.spw` (looping ambience) and
`se016023.spw` (activation). Independent XAudio2 voices provide distance falloff
and stereo positioning while preserving background music. Sound settings,
background playback, zone unloading, and a bounded voice count are respected.

## Validation

Build `tests/HomePointTests.vcxproj` in Release/x64, then run:

```powershell
& tests/bin/home-point/Release/HomePointTests.exe 'C:\Program Files (x86)\PlayOnline\SquareEnix\FINAL FANTASY XI'
```

The test requires a local FFXI installation and a D3D9 device. It checks resource
counts (seven layers, 302 triangles), schedule parsing, malformed-input rejection,
long-session emission, idle and activation pixel changes, render-state restoration,
audio decoding and voice limits, muting/distance cutoff, device reset, and failed
reload preservation. Captures are written under `tests/bin/home-point/captures`.

Validated on 2026-09-09: zero failures, both 48 kHz sounds decoded, all seven
layers loaded, and rendering survived device reset. The idle capture was visually
inspected for crystal, aura, and rings.

## Fidelity boundary

This implementation uses the retail assets and decoded schedules. The specular
shader reconstructs the observed texture/reflection inputs; its exact arithmetic
and some particle generator operations have not been verified against a running
official client. Live side-by-side parity remains unverified. Offline evidence
is in `artifacts/home-point-static-inspection/findings.md`.
