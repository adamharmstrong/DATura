# FFXI Home Point crystal

This document records what is currently known about the Final Fantasy XI Home Point crystal from the installed retail client, its DAT resources, and DATura's implementation. The evidence is primarily offline static analysis; the official client was not launched for a side-by-side capture.

## What the object is

The Home Point is not only a texture placed over a procedurally invented shape. The retail asset is a layered effect containing stored triangle geometry, textures, particle or generator instructions, animation curves, schedules, and sound references. Runtime code combines those pieces and selects the appropriate effect class and material behavior.

In DATura's NPC catalog, Home Points use look kind `0` and appearance `0x33` (`51`). The retail model-bank resolver maps that appearance to logical resource `1300 + 51 = 1351`. The installed tables resolve resource 1351 to `ROM/3/25.DAT`.

## Retail DAT contents

The inspected DAT is 77,120 bytes with SHA-256 `107a0ac5c7fe7e16868346f68c59bc142f9a2abed9a83b2cdf006ebf6a0ff1c3`. It contains:

| Resource kind | Count | Role |
| --- | ---: | --- |
| Type `0x05` | 13 | Generator / particle instruction streams |
| Type `0x07` | 2 | Effect schedules |
| Type `0x19` | 6 | Supporting records |
| Type `0x1F` | 5 | Effect meshes |
| Type `0x20` | 7 | Textures |
| Type `0x21` | 2 | Compact sprite-card resources |

The five stored effect meshes contain 296 triangles:

| Resource | Triangles | Material tag | Observed geometry |
| --- | ---: | --- | --- |
| `wa` | 48 | `bind    tayu` | Flat ground layer, X/Z bounds about ±1.08 |
| `sil` | 192 | `bind    nami` | Surrounding layer, X/Z bounds about ±0.60, Y from -1.26 to 0 |
| `naka` | 24 | `bind    tayu` | Planar vertical layer |
| `bind` | 30 | `bind    kori` | Narrow central crystal, about 2.16 units tall |
| `pou1` | 2 | `bind    pou` | Quad, X/Y bounds about ±1 |

Two type `0x21` resources add six sprite-card triangles: `tama1card2tri` and `tubu2cards4tri`. Together, the loaded geometry is 302 triangles across seven render layers.

## Schedules and generators

The `aper` schedule references `snd0`, `wa01`, `wa00`, `sil0`, `bnd0`, `nak0`, `nak1`, `tam0`, and `sil1`. The generator names associate `bnd0` with `bind`, `wa00`/`wa01` with `wa`, `sil0`/`sil1` with `sil`, `nak0`/`nak1` with `naka`, and `tam0` with `tama`.

The separate `bind` schedule references sound `6023`, `pou1`, `tub0`, `pou0`, and `sil2`. Static inspection establishes the references and timing windows, but not every gameplay transition that selects each schedule. DATura currently uses the schedules as idle and activation phases based on the observed resource organization.

Generator streams include rotation-update and UV-update operations, plus additional lifetime and particle commands. DATura evaluates supported generators at 60 Hz, applies authored curves, and emits the rising particles, aura, and ground rings. Some obscure retail particle operations remain decoded only partially.

## Retail geometry layout

The effect-model path uses a marker-6 layout in which the material table is followed by 36-byte vertices. For this asset, the first vertex begins at payload byte 30. The relevant retail arithmetic is:

1. The effect resource payload is obtained from the resource address plus `0x38`.
2. Marker 6 reads group counts at resource offsets `+0x34` and `+0x35`.
3. With one group, the padding rule places materials at payload byte 14.
4. Materials advance by 16 bytes each, so vertices begin at byte 30.
5. The draw path reads the triangle count, multiplies by three, and copies 36 bytes per vertex.

Using a generic 16-byte alignment at byte 14 skips two bytes into each vertex and produces invalid positions for most Home Point meshes. DATura's decoder implements the retail count and padding rule instead. Marker-3 records use a different layout.

## Crystal-specific rendering

The `bnd0` generator has standard setup flags `0x01010000`. Retail effect creation tests these flags and selects a subclass whose descriptor is named `CMoD3mSpecularElem`. The central crystal therefore has a dedicated specular-effect branch in addition to the common effect geometry path. The class identity and selection branch are verified statically; the exact highlight and reflection arithmetic has not been fully reconstructed.

DATura renders the authored layers with custom D3D9 shaders and material state. The central crystal receives the reconstructed specular treatment, while the surrounding layers use their texture and color data. Transparent layers are drawn after opaque actors and zone transparency, with depth testing enabled, depth writes disabled, and the previous D3D state restored afterward. The crystal rotates, textures scroll, and particle alpha, scale, color, and sprite-frame values follow the decoded curves.

## Sound effects

The ambient sound reference `snd0` has resource ID `9013` and resolves to `sound/win/se/se009/se009013.spw`. Its loop start is recorded as block offset `0x2060`, or 132,608 PCM frames. The activation reference `6023` resolves at runtime to sound ID `16023`, `sound/win/se/se016/se016023.spw`.

DATura decodes the installed SPW files to PCM/WAV data and plays them through independent XAudio2 voices. It applies distance falloff within 25 world units, stereo panning, a gain of 0.45, a bounded nearest-voice set, and a separate user sound cap. Muting, background playback, zone unload, and device-reset paths are handled without affecting background music.

## DATura behavior

DATura recognizes appearance `0x33`, loads the Home Point effect through the retail resource resolver, advances its animation clock, and draws Home Points with the effect renderer. Visible Home Points contribute ambient sound candidates. Interaction within six world units starts the activation effect with a one-second cooldown.

The renderer does not implement the game's teleport menu, destination selection, or saved-home-point state. It is a visual and audio implementation attached to DATura's existing NPC and interaction systems.

## Validation

`tests/HomePointTests.vcxproj` builds a Release/x64 test executable that loads the installed DAT and sounds, checks malformed-input rejection and long-session stability, renders on a D3D9 HAL software-vertex device, verifies idle and activation pixel changes, checks render-state restoration, tests audio decoding, distance and voice limits, muting, device reset, and preservation of the previous asset after a failed reload.

The validated run on 2026-09-09 reported:

```text
layers=7 triangles=302 audio=1
drawn=90726 moving=89602 activation=76979
failures=0
```

Both sounds decoded at 48 kHz. The idle capture visibly contained the tall faceted blue crystal, rising blue/violet aura, and ground rings; the activation capture showed a bright white/pink flash.

## Confidence and open questions

High-confidence findings include the appearance-to-DAT mapping, the seven-layer resource inventory, the retail marker-6 vertex offset, the generator and schedule references, the two sound paths, and the existence of the `CMoD3mSpecularElem` branch.

Still unverified against a live official-client capture are exact schedule transitions, every generator opcode's timing and semantics, the complete specular shader arithmetic, and pixel-for-pixel material and particle parity. The static evidence is sufficient to establish that the crystal uses stored geometry plus runtime effect processing, but it is not a complete reconstruction of the retail renderer.

## Evidence and reproduction

The detailed static trace is in [`artifacts/home-point-static-inspection/findings.md`](artifacts/home-point-static-inspection/findings.md). The rendering and test notes are in [`tests/HOME_POINT_RENDERING.md`](tests/HOME_POINT_RENDERING.md).

To regenerate the DAT inspection from an installed client:

```powershell
python tools/analyze_homepoint_dat.py "C:\Program Files (x86)\PlayOnline\SquareEnix\FINAL FANTASY XI"
```

The analysis writes verified resource data and a geometry SVG under `artifacts/home-point-static-inspection`.
