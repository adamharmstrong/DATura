# FFXI_GEOMETRY_AND_TEXTURE_RENDERING.md — Corrections from cexi-tools / xim

Sources: `thirdparty/xim` (faithful Kotlin client reimplementation), `cexi` parsers
(verified byte-exact against retail DATs). File references are to the cexi-tools repo.

## Claims that are wrong / contradicted

1. **`0x91` is not always palette+indices.** The header table calls bytes `0x1D–0x35`
   "six unknown 32-bit fields" — but the first four bytes there are `u16 unk` +
   `u16 bitCount`. When `bitCount == 32` the payload is a raw 32-bit BGRA image with
   **no palette**. (`xim/resource/TextureSection.kt:56`, `cexi xi_export.py parse_texture`)

2. **48- vs 36-byte vertex selection is misattributed.** The doc says one subheader
   flag selects both primitive mode and stride (`0` → 48-byte tri list, nonzero →
   36-byte strip). Actually these are **two independent bits** of the `0x2E` header
   config byte: bit 0 = tri strip vs tri list, bit 1 = "vertex blend enabled" =
   48-byte layout. (`xim/resource/ZoneMeshSection.kt:33–35`)

3. **The extra 12 bytes of the 48-byte zone vertex are a displacement**, used for
   foliage sway. Xim's shader computes `position0 + positionBlendWeight * position1`;
   the stored vector is not an absolute second position. Installed Konschtat grass
   confirms nonzero tip vectors and zero root vectors. The prior description of
   lerping between two absolute positions was incorrect.
   (`xim/resource/ZoneMeshSection.kt`, `xim/poc/gl/XimShader.kt`)

4. **"No confirmed native per-pixel normal-map binding" is wrong.** Section type
   `0x5D` is a native **BumpMap** resource: an 8-bit height map converted to a normal
   map and sampled with computed tangents (TBN) in the zone shader.
   (`xim/resource/BumpMapSection.kt`, `xim/poc/gl/XimShader.kt:170–173`)

5. **Chunk size formula drops the top size bit.** Doc: `size = (info >> 3) & 0x7FFFF0`.
   Client behavior per xim: `size = ((info >> 7) & 0xFFFFF) * 0x10`, i.e.
   `(info >> 3) & 0xFFFFF0`. (`xim/resource/DatParser.kt:147`)

6. **Collision triangle indices are not uniformly 14-bit.** Only `p1`/`p2` use
   `& 0x3FFF`; `p0` and the normal index use `& 0x7FFF`. The top nibbles of all four
   u16s combine into a 16-bit material/terrain-type word.
   (`cexi/zone/xi_collision.py:40–45`)

7. **The placement record's "unknown float4 + int32[8]" is mostly decoded.**
   - The `float4` = **effect DatId link, highDefThreshold, midDefThreshold,
     drawDistance** — i.e. per-placement LOD thresholds + draw distance. This also
     answers the doc's separate "how does retail choose `_h`/`_m`/`_l`? Unknown".
   - The `int32[8]` region contains 4 flag bytes (e.g. `flags1 & 0x2` =
     skip-during-decal-rendering), a **culling-table link**, an **environment DatId
     link** (per-object environment/lighting association), and point-light indices.
   - xim composes the transform as `T · Rz · Ry · Rx · S` (ZYX matrix order), not the
     "XYZ Euler" stated. (`xim/resource/ZoneDefParser.kt:201, 308–353`)

## "Unknown" in the doc but known here

8. **`0x2F` Environment is fully parsed**: ambient color, fog color, fog start/end,
   sun/diffuse light config — interpolated across time-of-day and weather states.
   Fog is linear: `f = (far − d) / (far − near)`, mixed into RGB only, alpha
   preserved. (`xim/resource/EnvironmentSection.kt:232–247`)

9. **Zone lighting is not purely baked.** The zone shader applies ambient + two
   directional diffuse lights + point lights (all driven by vertex normals),
   modulated by vertex color, before the texture modulate. Light values come from
   the `0x2F` environment records. (`xim/poc/gl/XimShader.kt:178–187`)

10. **The fragment equation**: `rgb = 2 · lit.rgb · tex.rgb`,
    `a = 4 · vcol.a · tex.a` — the 0x80 = 1.0 convention applies to **both** vertex
    alpha and texture alpha. The doc's mysterious "≈1.875 DXT3 expansion" is the same
    convention seen through 4-bit quantization (15/8 vs 255/128); a uniform ×2 per
    source is the cleaner model. (`xim/poc/gl/XimShader.kt:187`)

11. **Name-based cutout classification is the mechanism, not a correlating heuristic.**
    A leading `_` in the mesh name selects the hard-alpha path, threshold 0.375
    (matching DATura's value). (`xim/resource/ZoneMeshSection.kt:119`)
    Note: xim's *character* discard threshold is 69/255 ≈ 0.27, not 0.5.

12. **Depth bias for blended zone layers is known**: "FFXI uses zbias = 8".
    (`xim/poc/gl/GLDrawer.kt:217`)

13. **The two "unknown" `0x2A` draw commands are decoded**:
    - `0x0043` = untextured triangle mesh (three indices + one BGRA color per tri —
      the "count × 10 bytes").
    - `0x4353` = single-color untextured triangle strip.
    (`xim/resource/SkeletonMeshSection.kt:167–170`)

14. **The `0x8010` reflection path is loader/renderer confirmed**: float `1.0` at
    state-data `+16` enables texture stage 1; float `+36 × 0.5` becomes
    `TEXTUREFACTOR` alpha/reflection intensity. The stage uses camera-space normals,
    a texture transform, `MODULATEALPHA_ADDCOLOR`, `ALPHAOP=ADD`, and a `CubeTex`
    fallback. (`ModelRenderer.cpp:398–415`)

15. **"Shiny" character state = a second-stage cubemap environment map.** Retail DATs contain
    real `cubemap*`-named DXT3 textures; xim builds a cube map from them (blank
    faces filled with alpha 0x80) and applies specular power from `0x8010`.
    DATura's flat-normal/flat-spec fake approximates this.
    (`xim/resource/TextureSection.kt:107, 141`)

16. **Fuller section-type map exists** (xim `DatResource.kt:14–45`):
    `0x04` table, `0x06` route, `0x07` effect routine, `0x21` sprite-sheet mesh,
    `0x25` weighted mesh, `0x30`/`0x31` UI menu/element group, `0x36` zone
    interactions, `0x3E` point list, `0x45` info, `0x49` spell list, `0x4A` path,
    `0x53` ability list, `0x54` weapon trace, `0x5D` bump map, `0x5E` blur.

17. **DXT5 presence unverified.** Neither xim nor cexi has ever encountered a `5TXD`
    payload in retail DATs (both hard-fail/skip on it). "Accepts DXT5" is harmless,
    but there is no evidence FFXI ships any.
