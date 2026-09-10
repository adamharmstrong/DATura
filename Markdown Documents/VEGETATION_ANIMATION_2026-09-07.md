# DAT-authored vegetation animation

DATura now retains the wind displacement in 48-byte MapGeo/MMB vertices and
updates visible vegetation before drawing. The CPU base vertices remain immutable;
the D3D9 upload computes `base + weight * displacement`. This works with both
individual and packed shared vertex buffers and preserves the fixed-function
lighting, texture coordinates, materials, and opaque batches.

## DAT fields and timing

Offsets are relative to the decrypted type `0x2E` payload. Byte `+0x04` bit 0
selects triangle-strip topology; bit 1 selects vertex blending. These controls
are independent. The existing early-prototype 36-byte layout fallback remains.

The blended vertex layout is base position at `+0x00`, displacement at `+0x0C`,
normal at `+0x18`, BGRA at `+0x24`, and UV at `+0x28` (48 bytes total).
The displacement is not an absolute second position. Placement rotation and
scale apply to it, while translation does not. DATura's X-axis zone mirror also
mirrors displacement. Culling bounds cover both endpoints. Root vertices with
zero displacement remain fixed; meshes without nonzero vectors allocate no
wind array and remain static.

- **Off:** base geometry, weight zero.
- **Simple:** linear 0 → 1 → 0 cycle over four seconds.
- **Smooth:** cosine easing over the same four-second cycle.

The four-second linear cycle follows Xim's `EnvironmentManager.kt` WindFactor.
Smooth easing is DATura behavior. Neither is a claim that retail timing was
decoded from these DATs. Xim's `ZoneMeshSection.kt`, `ZoneDrawer.kt`, and
`gl/XimShader.kt` corroborate the fields and displacement equation; source was
checked in the [published archive](https://github.com/Masin-M/xim-docker/blob/main/src.zip).

## Validation

`tests/VegetationTests.vcxproj` exercises all four topology/blend combinations,
triangulation, mixed static/moving vertices, invalid vectors, placement rotation
and scale, X mirroring, full-sway bounds, and mode timing. A hidden D3D9 device
checks exact uploaded positions for individual and shared buffers, untouched
neighbors and roots, repeated updates without drift, restoration when switched
off, and buffer recreation.

Installed-DAT parser checks passed:

| Zone | DAT | Submeshes with nonzero wind | Grass submeshes |
|---|---|---:|---:|
| Konschtat Highlands | `ROM/0/90.DAT` | 12,294 | 1,833 |
| La Theine Plateau | `ROM/0/115.DAT` | 16,114 | 3,220 |
| West Ronfaure | `ROM/0/120.DAT` | 0 | 0 with wind |

These counts include loaded LOD variants, rather than simultaneous visible draws.
West Ronfaure is a negative control for this specific geometry animation path;
no movement is invented for meshes without authored vectors.

Run the executable with the Konschtat and La Theine DAT paths, then `--static`
and the West Ronfaure DAT path. Running without paths performs the synthetic
parser and GPU checks only. These are automated geometry/buffer checks, not a
manual comparison of in-game wind or an interactive performance benchmark.
