# Home Point crystal: offline client trace

Examined 2026-09-09. This is static analysis of the locally installed retail client and DAT, not a live execution capture. Installed files were not modified. Production renderer code was not changed.

## Result

The Home Point asset is a layered effect with **stored triangle geometry, textures, generator instructions, and schedules**. The evidence does not support a crystal constructed solely from a texture and a procedural shape algorithm. Runtime code still controls its appearance, movement, particles, and rendering state.

The earlier concern that `bind` meant DATura had selected an unrelated file was unwarranted. The retail model-bank resolver and installed file table independently resolve the catalog's Home Point appearance ID to that same file.

## Appearance lookup

- DATura's NPC catalog encodes Home Points as look kind zero, appearance `0x33` (51).
- Retail function `0x100C51D0` dispatches kinds 1–4 separately. Its default branch at `0x100C5219` returns `1300 + modelId` for IDs below 1500. Higher IDs select other banks.
- A kind-zero caller at `0x100D3F2E` passes the model ID to this function and passes the returned logical resource ID to `0x100730F0`.
- `1300 + 51 = 1351`. Installed `VTABLE.DAT[1351]` is 1 and the corresponding `FTABLE.DAT` word is 409, resolving to **ROM/3/25.DAT**.
- This confirms the file selected by DATura for this particular ID. It does not establish that DATura's direct `358 + modelId` physical-path shortcut is valid across all models.

## Asset contents

The DAT is 77,120 bytes, SHA-256 `107a0ac5c7fe7e16868346f68c59bc142f9a2abed9a83b2cdf006ebf6a0ff1c3`.

It contains 13 type-`0x05` generators, 2 type-`0x07` schedules, 6 type-`0x19` records, 5 type-`0x1F` effect meshes, 7 type-`0x20` textures, 2 type-`0x21` resources, and small skeleton/geometry/animation and sound records. It is not simply a normal skeletal NPC model.

| Effect mesh | Triangles | Material tag | Geometric observation |
|---|---:|---|---|
| `wa` | 48 | `bind    tayu` | Flat ground layer, X/Z bounds ±1.08 |
| `sil` | 192 | `bind    nami` | Surrounding layer, X/Z bounds ±0.60 and Y from -1.26 to 0 |
| `naka` | 24 | `bind    tayu` | Planar vertical layer |
| `bind` | 30 | `bind    kori` | Narrow central shape, 2.16 units tall |
| `pou1` | 2 | `bind    pou` | Quad, X/Y bounds ±1 |

These are 296 stored triangles altogether. The wireframe SVG shows the raw geometry separately; it is not an approximation of the fully animated retail appearance.

The `aper` schedule contains references to `snd0`, `wa01`, `wa00`, `sil0`, `bnd0`, `nak0`, `nak1`, `tam0`, and `sil1`. Generator `bnd0` references mesh `bind`; `wa00`/`wa01` reference `wa`; `sil0`/`sil1` reference `sil`; `nak0`/`nak1` reference `naka`. `tam0` references `tama`, and `snd0` references sound resource `9013`.

The separate `bind` schedule references `6023`, `pou1`, `tub0`, `pou0`, and `sil2`. The exact gameplay transitions selecting these schedules were not established; the names alone should not be used to label them as activation or teleport animations.

Generator instruction streams contain rotation-update and UV-update opcodes recognized by DATura's current decoder, as well as additional particle/lifetime commands. Their presence is directly observed; exact retail timing and the semantics of every opcode remain unverified.

## Rendering path and alignment

The retail code provides stronger evidence than coordinate plausibility:

1. `0x1003FCF0` returns the effect resource address plus `0x38` (payload plus 8, given the resource fields beginning at `+0x30`).
2. For marker 6, `0x1003FD00` reads the two group-count bytes at resource `+0x34/+0x35`. Here they are 1 and 0. Its padding branch adds 6 bytes, placing materials at payload byte 14.
3. `0x1003FD80` advances by 16 bytes per material. With one material, vertices begin at **payload byte 30**, not byte 32.
4. The related effect-model draw routine at `0x10043330` reads the triangle count at resource `+0x36`, multiplies by three, allocates/copies **36 bytes per vertex**, and obtains the source through `0x1003FD80`.
5. The effect submission helper `0x10043F50` feeds the graphics wrapper `0x1000CB20`, which issues the device virtual call at `0x1000CB6E`.

DATura's `ParseEffectModel1F` currently applies `Align16(0x0E + imageCount * 16)`. For this asset, that skips two bytes into each vertex. Four of the five meshes then fail its position sanity check. The fifth also has wrong coordinates despite passing the loose sanity bound. Reading all five at the retail-derived offset produces finite, bounded positions.

This is a confirmed layout mismatch, but a production fix should implement the count/padding rules and validate other supported layouts, rather than globally subtracting two bytes. Marker-3 records use another layout.

## Crystal-specific rendering branch

`bnd0`'s standard setup flags are `0x01010000`. The effect factory branch at `0x10050F9E` tests `0x00200000`, then `0x00100000`, then bit 0 of the fourth flags byte. These flags select the constructor at `0x10041030`.

That constructor installs vtable `0x1032AD20`. Its type getter returns descriptor `0x1032AD08`, whose name is **CMoD3mSpecularElem**. The draw entry `0x100412C0` accesses the stored effect resource and the same material-pointer helper. Thus the central crystal has a dedicated specular-effect subclass in addition to the common effect geometry machinery. The class name and branch are verified; the full highlight/reflection math has not been reconstructed.

## Other string leads

- `CMoGeneratorClone` at `0x10353708` is referenced by descriptor `0x1032B108`; code performs type checks against it in effect-processing functions. It is not, by itself, a Home Point identifier.
- `Scheduler` at `0x1035461C` belongs to a name-pointer table. Concrete schedule resource references in the DAT are more useful than this generic string for identifying the asset's layers.
- `(ICRYSTALL)` is entry 48 in the 16-byte actor-state name table beginning at `0x1035AF80`. Code at `0x100AD560` indexes that table for an `EventInfo:CSTAT=...,SSTAT=...` diagnostic. It is an actor-state label, not a direct pointer to a crystal rendering function. Its specific relationship to Home Points remains unproven.

## What remains

Rendering a faithful Home Point in DATura requires correct effect geometry decoding plus actor-attached schedule/generator execution, per-layer material state, and the specular branch. Loading all effect meshes once would only yield a static approximation. Which schedules run at spawn and interaction, their exact timing, and the full specular algorithm still need further static tracing or a later live capture.

## Reproducibility

Run `python tools/analyze_homepoint_dat.py <installation-directory>`. It resolves ID 1351 from the installed tables, checks all five geometry records and all generator stream boundaries, and writes `verified-dat.json` and `geometry.svg`.

`client-excerpts.txt` contains bounded disassembly supporting the main arithmetic and branch findings. `tools/inspect_homepoint_client.py` uses local Capstone and the existing local unpacking/reference helpers; `evidence` regenerates those excerpts from `tmp/homepoint-client/FFXiMain.unpacked.dll`.

The installed client SHA-256 is `6f8844eb7f0380f30a3db2fc3c435e1145f5c450bdd0999133cc75c516ec3c3b`. Its compressed code expanded to exactly `0x32762E` bytes, matching the declared section size. Addresses above use its preferred image base, not ASLR-adjusted process addresses. Full client binaries remain in the local temporary analysis directory.
