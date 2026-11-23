# Water rendering regression

Build `WaterRenderingTests.vcxproj` for Debug/x64 or Release/x64 with the same Visual Studio toolchain as DATura:

```powershell
& 'C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe' tests/WaterRenderingTests.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
& tests/bin/water-rendering/Debug/WaterRenderingTests.exe 'C:/Program Files (x86)/PlayOnline/SquareEnix/FINAL FANTASY XI'
```

Run from the repository root. Omit the installation path to run only the synthetic and parser-policy tests. An optional second argument selects the capture directory; the default is `tests/bin/water-rendering/captures`. Retail assets remain in the local installation and are never modified or bundled with the test.

The test creates a hidden D3D9 window and reads back actual rendered pixels. It needs a desktop graphics device with D24S8 support. In the Codex sandbox used during development, running the GPU executable required escalation for desktop-device access; the executable did not need visible interaction.

## Checks

- Visible water on/off, authored opacity, time-of-day interpolation, signed U/V flow and deterministic repeated frames.
- Opaque geometry in front of water remains unchanged by the blend.
- Separate zone-opaque, actor and zone-transparent passes match a combined-model reference. The previous all-zone-before-actor sequence must demonstrate the regression.
- Water draws after submerged transparent terrain even when a wide sheet's centroid sorts farther away; authored overlapping water layers retain their contributions.
- Shader, texture, UV transform, blend and buffer bindings return to the pass baseline.
- Buffer recreation and later DAT parsing preserve water metadata and rendered output; animation leaves CPU geometry unchanged.
- Effect resource/curve scope, persistent-generator admission, authored SRT and singular mirrored planes.
- Installed East Ronfaure, Valkurm Dunes and Bastok Markets exercise actual river, sea, canal and untextured basin assets, including the object inspector identity mapping.

Captures deliberately isolate model rendering without sky, weather or UI. They establish placement, material and composition behavior; they are not a claim of pixel-identical retail rendering or universal zone coverage.

## Validation record

Validated on 2026-09-09 against the local retail installation and desktop D3D9 device:

| Fixture | Water placements | Water submeshes | Pixels changed by enabling water |
| --- | ---: | ---: | ---: |
| East Ronfaure, `ROM/0/121.DAT` | 44 | 44 | 39,807 |
| Valkurm Dunes, `ROM/0/102.DAT` | 11 | 22 | 168,439 |
| Bastok Markets, `ROM/1/35.DAT` | 4 | 5 | 120,495 |

The complete Debug suite passed with zero failures. Actor composition and the wide-water/submerged-overlay fixture each matched their reference exactly. Synthetic flow changed 65,732 pixels; all 13,974 opaque foreground pixels remained correctly occluded. Installed river/sea frames changed with authored UV time, while the untextured fountain changed with its day/night curves. Final Valkurm captures show neither the earlier rectangular terrain overlays nor the opacity band introduced by the discarded stencil experiment.

Both DATura Debug/x64 and Release/x64 builds passed. Existing `ZoneRoomTests --gpu` passed all installed room, collision, material and LOD checks. `SignRenderingTests` retained its depth regression result: 29,190 pixels differing with the old near plane, zero with the corrected near plane.

The exploratory renderer/material experiments are archived locally under `tmp/water-research`; the permanent test suite contains the final implementation checks and standard off/on/animated/day-night captures.
