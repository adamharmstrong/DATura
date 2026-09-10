# FFXI Rendering and DAT Structures — Adjudicated Findings

**Created:** 2026-07-27  
**Purpose:** Consolidate and reconcile:

- [`FFXI_GEOMETRY_AND_TEXTURE_RENDERING Corrections.md`](<FFXI_GEOMETRY_AND_TEXTURE_RENDERING Corrections.md>) (xim/cexi findings), and
- [`FFXI_RENDERING_FINDINGS_FOR_COLLABORATORS_2026-07-22.md`](FFXI_RENDERING_FINDINGS_FOR_COLLABORATORS_2026-07-22.md) (reported client-disassembly and DAT-corpus findings).

This is the fourth research document in DATura's `Markdown Documents` directory.
It does not replace the source documents. It records the current working interpretations, the evidence reported for them, and what remains unresolved.
It must not describe externally supplied disassembly or corpus claims as independently confirmed unless DATura's team has reproduced them or possesses inspectable artifacts.

---

## 1. Adjudication method

When the source documents disagree, this document uses the following evidence order:

1. **Locally reproducible evidence:** bytes, code, scripts, or traces we can inspect and rerun.
2. **Inspectable external evidence:** source excerpts, binaries, or corpus artifacts available for independent checking.
3. **Externally reported evidence:** findings that cite disassembly or corpus work whose underlying artifacts we do not currently possess.
4. **Reimplementation behavior:** xim/cexi or other implementations, ideally corroborated by local DAT observations.
5. **Legacy viewer behavior and visual inference.**

The evidence order matters more than words such as “confirmed” or “strongly supported.”
A detailed report is valuable, but a line-number citation or pasted pseudocode is not the same as an independently reproduced original-client trace.

Evidence labels used below:

- **Locally verified:** reproduced from artifacts available in this workspace or from a documented local test.
- **Externally reported:** asserted by one of the source documents but not independently reproduced here.
- **Corroborated working model:** multiple independent reports or implementations agree, without direct local proof of original-client behavior.
- **Strong model:** supported by xim/cexi and compatible with corpus evidence, but not yet traced through retail execution.
- **Viewer lead:** derived from community viewer/decoder code and not independently proved.
- **Open:** insufficient evidence or a known conflict remains.

---

## 2. Executive adjudication ledger

| Subject | Earlier disagreement | Adjudicated result | Reason |
|---|---|---|---|
| Chunk size mask | DATura `0x7FFFF0` versus xim `0xFFFFF0` | **Open; retain current behavior pending a discriminating case** | The 19-bit choice is supported by a reported disassembly excerpt; the 20-bit choice is implemented by xim. We have not independently verified original-client behavior. |
| Character alpha threshold | DATura `0.5`, xim `69/255` | **Working choice: `69/255`, comparison `GREATER`** | xim and the collaborator's reported render-state enumeration agree. |
| Zone alpha threshold | Approximate `0.375` | **Working choice: `96/255`, comparison `GREATER`** | DATura's visual/corpus work and the collaborator's reported state trace agree closely. |
| Zone fragment equation | Empirical DXT3 expansion versus fixed-function ×4 alpha | **Corroborated model: `MODULATE2X` RGB and `MODULATE4X` alpha** | The reported state block and xim shader agree, but DATura has not independently traced the original client. |
| Post-texture MMB dword | `u16 count + u16 blendFlags` and “multiplier nibble” | **Corroborated working model: low 28-bit count + four high flags** | The contributed loader trace identifies `VerticeCountAndFlags`, and xim provides independent reimplementation support. |
| `0x2000` runtime flag | Possible culling bit versus color multiplier | **Corroborated working model: disable culling** | The contributed loader/manager value-flow report and xim agree; DATura has not independently traced the original client. |
| 36/48-byte vertex selection | One flag selects both stride and primitive mode | **Two independent config bits** | xim parser separates primitive topology and vertex blending; corpus layout agrees. |
| Extra 12 vertex bytes | Unknown/padding/second position | **Second position for wind sway** | xim shader interpolates position to position2; vegetation corpus correlation agrees. |
| Placement rotation | Ambiguous “XYZ Euler” | **`Rz · Ry · Rx` matrix product** | xim and independent corpus decode agree; this is per-axis application X→Y→Z. |
| `0x2F` environment | Partly unknown versus “fully parsed” | **Mostly decoded, with refinements and residue** | 4,021-record corpus fixes clear-color and fog-sentinel details. |
| Sun movement | Analytic sun arc versus environment-driven | **Working model: runtime environment vectors, no analytic arc** | The collaborator reports this client behavior; xim's analytic arc is a known alternative. |
| `0x8010` tail float | Alpha-reference candidate | **Corroborated as reflection intensity** | The collaborator reports `+36 × 0.5 → TEXTUREFACTOR` alpha, consistent with xim's specular naming. |
| “Shiny” path | Flat specular, second pass, or cubemap | **Corroborated model: second-stage cubemap environment mapping** | The reported stage setup unifies the second-stage and xim cubemap interpretations. |
| Character shadows | Unknown/unstarted | **Externally reported projected blob-shadow path** | The contributed trace reports `SYSTEM_KAGE`, Z-bias 15, no depth writes, scale vectors, and a dedicated render route. |
| Native bump maps | No confirmed binding | **Type `0x5D` is a native bump/height resource** | File layout is known; xim implements height-to-normal/TBN sampling. Exact retail stage remains open. |
| DXT5 | Accepted by tolerant parsers | **No evidence retail FFXI ships DXT5** | Three independent scans and native/viewer branch absence agree. |

---

## 3. DAT chunk framing

### 3.1 Competing size formulas

DATura currently uses:

```text
chunk_type = info & 0x7F
size       = (info >> 3) & 0x7FFFF0
```

The xim/cexi corrections document uses:

```text
(info >> 3) & 0xFFFFF0
```

The collaborator document reports a disassembly sequence ending in `and ecx, 0x7FFFF0`, which supports the 19-bit interpretation.
We do not currently have the binary, decompiler project, raw function bytes, or an independently reproduced trace.
The xim implementation is inspectable evidence for the 20-bit interpretation, but it is still a reimplementation rather than proof of original-client behavior.

The two formulas differ only when the disputed size bit is set. If no available DAT chunk exercises that bit, successful corpus walks cannot distinguish them.

**DATura action:** retain the current 19-bit behavior for compatibility, but label the choice unresolved.
Add a regression fixture or corpus query that finds a header with the disputed bit set;
compare both candidate walks and, if possible, obtain inspectable original-client bytes or a reproducible trace before declaring either formula authoritative.

### 3.2 Section-type census

The combined research identifies at least 38 real block types across a 50,129-DAT census. Useful decoded types include:

| Type | Meaning / layout status |
|---:|---|
| `0x04` | Table |
| `0x05` | Effect generator/controller |
| `0x06` | Camera/keyframe route |
| `0x07` | Effect routine |
| `0x1B` | Alternate texture/DIB container |
| `0x21` | Sprite-sheet/card effect mesh |
| `0x25` | Weighted/morph effect mesh |
| `0x2A` | Character/skeleton geometry |
| `0x2F` | Environment |
| `0x30` / `0x31` | UI menu / element groups |
| `0x36` | Zone interactions |
| `0x3E` | Point list |
| `0x45` | 16-byte information records |
| `0x49` | Spell list |
| `0x4A` | Path graph |
| `0x53` | Ability list |
| `0x54` | Weapon-trace two-rail ribbon |
| `0x5D` | Bump/height map |
| `0x5E` | Blur/afterimage taps |

A reported type `117` is likely a naive-alignment scan artifact and should not be accepted without a valid chunk walk.

---

## 4. Zone mesh records (`0x2E` / MMB)

### 4.1 `VerticeCountAndFlags`

The dword immediately after the 16-byte texture name is:

```text
VerticeCountAndFlags
```

Its layout is:

| DAT dword bits | Runtime u16 form | Meaning |
|---:|---:|---|
| `0x0FFFFFFF` | — | Vertex count |
| `0x80000000` | `0x8000` | `IsTransparent` |
| `0x40000000` | `0x4000` | Flag present; behavior unknown |
| `0x20000000` | `0x2000` | `DisableCulling` |
| `0x10000000` | `0x1000` | Flag present; behavior unknown |

The contributed account attributes the DAT-side masks to `MeshBlockResource.h:33–39` and the transparent/culling consequences to `MeshBlockManager.cpp:514–517`. xim independently corroborates the interpretation as a reimplementation, but DATura has not reproduced the original-client value flow.

The old split into `u16 vertexCount` and `u16 blendFlags` happened to work for ordinary counts below 65,536 because the four true flags occupy the high nibble. It nevertheless misdescribed the structure and encouraged the incorrect “high nibble color multiplier” theory.

**DATura action:** parse the full dword, mask the low 28-bit count, expose all four high flags, and assign behavior only to `0x8000` and `0x2000`.

### 4.2 Primitive topology and vertex stride are independent

The zone mesh config byte has independent controls:

- bit 0: triangle strip versus triangle list;
- bit 1: vertex blending enabled, selecting the 48-byte layout.

The earlier rule “one subheader flag simultaneously selects a 48-byte list or 36-byte strip” conflated two independent properties.

**Evidence:** xim `ZoneMeshSection.kt:33–35`, with compatible retail storage patterns.

**DATura action:** decode and display the two bits separately. Do not infer stride from primitive topology or vice versa.

### 4.3 Vertex layouts and wind sway

The 36-byte form contains one position. The additional 12 bytes in the 48-byte form are a `float3` displacement:

```text
displacement
```

The zone shader applies the displacement:

```text
position_final = position + globalWindFactor * displacement
```

This is used for foliage and vegetation sway. The strong correlation between 48-byte batches and vegetation such as `_con_hana_*` supports the reimplementation trace.

**Evidence:** strong model from xim `ZoneDrawer.kt:131`, corroborated by DAT corpus structure. Independently inspectable original-client instruction-level confirmation is still desirable.

### 4.4 Alpha classification

The leading underscore convention is operational in xim:

- leading `_` selects the hard-alpha/cutout path;
- zone alpha reference is `0x60` with `GREATER`;
- soft-blended groups instead use the transparent flag and disable depth writes.

The name rule should be described carefully: it is directly implemented by xim and strongly fits retail assets. The contributed disassembly account reports the resulting alpha-test state, rather than the source-name comparison itself, and has not been independently reproduced here.

### 4.5 Culling and winding

The contributed original-client trace reports that zone rendering:

- defaults to `CULLMODE=CCW`;
- switches a material group to `CULLMODE=NONE` when the runtime cull-disable short is nonzero;
- can switch geometry winding to CW through a separate runtime geometry field;
- must still account for negative-determinant placement transforms.

Therefore, “all FFXI materials are globally two-sided” is viewer behavior and is not supported by the reported original-client trace.

---

## 5. Fixed-function zone rendering

### 5.1 Alpha-test constants are per pass

The contributed renderer trace reports that `D3DRS_ALPHAREF` is not sourced from a material float and gives these pass-specific values:

| Pass | Reference | Function |
|---|---:|---|
| Zone | `0x60` (96) | `GREATER` |
| Character / actor | `0x45` (69) | `GREATER` |
| Particle / effect | `0x7F` (127) | `GREATER` |
| UI / second renderer | parameter (`0x7F` observed) | `GREATEREQUAL` |
| Minimap | `0x10` | `GREATER` |
| Shadows / overlays / nameplates | `0` | `GREATER` |

Zone geometry has an enable/addressing selector:

| Runtime value | Alpha test | Addressing |
|---:|---|---|
| `0` | Off | Wrap |
| `1` | On | Wrap |
| `2` | Off | Clamp |

**Correction:** DATura's generic character `0.5` threshold is wrong. Use `69/255`.

### 5.2 Fragment equation

The reported original-client stage-0 state is:

```text
COLOROP = MODULATE2X
ALPHAOP = MODULATE4X
```

Therefore:

```text
rgb = saturate(2 · texture.rgb · litVertex.rgb)
alpha = saturate(4 · texture.a · vertex.a)
```

The earlier DXT3-specific factor of approximately `1.875` was an empirical view of the same `0x80 = 1.0` authoring convention after 4-bit quantization. The cleaner model doubles each alpha source through `MODULATE4X`.

Do not blindly apply the convention to full-range DMB/RGBA atlases that are consumed through a different state path.

### 5.3 Blending, depth writes, and bias

The reported original-client zone blend state is:

```text
SRCBLEND  = SRCALPHA
DESTBLEND = INVSRCALPHA
ZFUNC     = LESSEQUAL
```

Per-group state:

| Group | Alpha blend | Z write | D3D8 ZBIAS |
|---|---|---|---:|
| Opaque/cutout | Off | On | 0 |
| Transparent layer | On | Off | 8 |
| Sky/celestial billboard | As required | Usually off | 15 |

There is a driver-workaround state set that reduces the bias values to approximately `0/1/2/2`.

**DATura action:** preserve transparent batches separately and translate the reported D3D8 integer Z-bias semantics carefully to D3D9. A small negative floating depth bias is only an approximation, not the reported original-client value.

The original-client transparent traversal order is still open. DATura's far-to-near submesh sorting is a viewer reconstruction.

---

## 6. Textures

### 6.1 `0x91` raw and palettized variants

The texture header region previously described as six unknown dwords contains at least:

```text
+0x1D  u16 constant/unknown (observed 1)
+0x1F  u16 bitCount
...
+0x35  u32 palette entry depth
```

When `bitCount == 32`, the payload is raw 32-bit BGRA with no palette. Otherwise, the common form is an 8-bit indexed image with a palette.

Palette entry depths:

- `0x20`: B8G8R8A8;
- `0x10`: B5G5R5A1.

### 6.2 Container markers

- `0x01` belongs to a distinct type-`0x1B` palettized DIB container and has no format dword before its 256-entry BGRA palette.
- `0x91` is the common palettized/raw texture marker described above.
- `0xA1` is DXT.
- `0xB1` has a leading format dword; observed values are 9 and 10, with otherwise identical parsing.
- `0x81` uses its DXT copy only when a valid `3TXD` trailer is present. This is presently viewer-source evidence.
- marker `0x05` appears to be a five-image palettized `MULTI` container with no inline palette. Whether it represents mip levels is open.

### 6.3 Texture names and resolution

The 16-byte name can be treated as:

```text
char[8] nameSpace
char[8] localName
```

Resolution tries:

1. exact namespace + local name;
2. local name alone;
3. a global table.

Bump maps key on the local half. This explains cross-DAT redirects that cannot be modeled reliably by treating all 16 bytes as one opaque identifier.

### 6.4 Mipmaps and DXT5

Observed native DXT payloads appear to contain only the top level, with alignment padding. The `MULTI` container remains the best known candidate for other multi-image semantics, but it is not proved to be a mip chain.

No retail DXT5 payload has been found by xim, cexi, the third corpus scan, AltanaView, or a native decoder path. DATura may accept DXT5 defensively, but documentation should say this is tool tolerance rather than evidence that FFXI ships DXT5.

### 6.5 Native bump maps

Type `0x5D` is an 8-bit height resource. xim converts it to a normal map using a wrapped Sobel-like gradient and samples it with tangent-space data:

```text
r = nx · 0.5 + 0.5
g = ny · 0.5 + 0.5
b = nz
```

The file format is established. xim's TBN shader is a strong runtime model. The exact retail D3D8 DOT3/bump-environment stage sequence is not yet traced, so DATura should not label a modern normal-map implementation byte-identical to retail.

---

## 7. Placement, LOD, visibility, and lighting links

### 7.1 Placement transform

Use:

```text
worldPosition = (Rz · Ry · Rx) · (diag(scale) · localPosition) + translation
```

This matrix product removes the ambiguity in phrases such as “XYZ Euler.” It means the vector experiences X rotation, then Y, then Z.

### 7.2 Full 0x64-byte placement record

```text
+0x00  char[16] id                    XOR-0x55-obfuscated mesh id
+0x10  f32[3]   position
+0x1C  f32[3]   rotation
+0x28  f32[3]   scale
+0x34  4CC      effectLink
+0x38  f32      highDefThreshold
+0x3C  f32      midDefThreshold
+0x40  f32      lowDefThreshold / draw distance
+0x44  u8       flags0
+0x45  u8       flags1                bit 1: skip during decal rendering
+0x46  u8       flags2
+0x47  u8       flags3
+0x48  u32      cullingTableLink
+0x4C  4CC      environmentLink
+0x50  u32      fileIdLink
+0x54  u32[4]   pointLightIndices     1-based
```

The `_h` / `_m` / `_l` selection is data-driven by these distance thresholds. Corpus evidence shows `highDefThreshold == 0` only on `_m` objects without an `_h` sibling, explaining why DATura's name-based fallback often produced the right result.

### 7.3 Placement-table subsections

The encrypted placement header contains:

```text
+0x08 collisionMeshOffset
+0x10 spacePartitioningTreeOffset
+0x14 cullingTablesOffset
+0x18 pointLightOffset
```

All four are corpus-validated in bounds.

Still open:

- quadtree node semantics;
- culling-table/visibility-set internals;
- the exact meaning of the observed 16-byte octree descriptor;
- runtime point-light photometric parameters.

### 7.4 Point-light table

The table contains 256 records with stride `0x4C`:

```text
char[16] "pl<NN>" handle
60 bytes zero/padding
```

Colors, radius, and intensity are not inline. Some city-zone world positions come from separate `obj_light` marker objects. Runtime parameter resolution remains open.

---

## 8. Environment, fog, sky, and zone lighting

### 8.1 `0x2F` environment layout

Externally reported corpus layout:

```text
+0x08  u32   indoorFlag
+0x14  LightConfig MODEL       0x20 bytes
+0x34  LightConfig TERRAIN     0x20 bytes
+0x54  u32   clearColor        not fog color
+0x58  unknown/read-discarded
+0x5C  unknown/read-discarded
+0x60  f32   drawDistance
+0x64  u16   selector/unknown
+0x66  u16   sphereSpokeCount
+0x68  u32   packed color/unknown role
+0x70  f32   skyBoxRadius      approximately 2029.5
+0x74  u32[8] skyDomeRingColors
+0x94  f32[8] skyDomeElevations
+0xB4  u32   terminator
```

Most records are 184 bytes. Two known 136-byte variants truncate after the fifth sky ring.

Each 0x20-byte `LightConfig` contains packed sun, moon, ambient, and fog colors followed by:

```text
f32 fogFar
f32 fogNear
f32 diffuseMultiplier
```

The packed color's top byte is normally `0x80` as a marker, not a meaningful alpha.

### 8.2 Fog

The contributed client trace reports linear world fog:

```text
fogFactor = (fogFar - distance) / (fogFar - fogNear)
```

Fog affects RGB while preserving alpha.

Important correction:

```text
fogFar == 0.0
```

is a discrete fog-disabled sentinel and may coexist with nonzero `fogNear`. Do not require `near <= far`, and do not interpolate into or out of the disabled sentinel.

The clear color at `+0x54` is not the fog color. Fog color lives inside each `LightConfig`.

### 8.3 Lighting

Zone lighting is not purely baked. The working/retail-compatible model includes:

- ambient light;
- two directional lights;
- point lights;
- vertex normals;
- vertex-color modulation;
- texture modulation afterward.

The contributed client trace reports no analytic sun-arc calculation. Instead, it describes prebuilt sun/moon direction vectors in the runtime environment object whose colors are scaled with daylight factors. Those direction vectors are not present in the decoded `0x2F` DAT record shown above, so DATura should not assume that time-of-day angles are the authoritative source.

Open:

- the writer/source of the runtime direction vectors;
- the daylight-curve writer;
- the runtime point-light parameter source.

---

## 9. Character geometry, draw commands, and animation

### 9.1 Additional `0x2A` draw commands

| Command | Meaning |
|---:|---|
| `0x0054` | Textured triangle list |
| `0x5453` | Textured triangle strip |
| `0x0043` | Vertex-colored, untextured triangle list |
| `0x4353` | Single-color, untextured triangle strip |
| `0x0000` | Vertex-colored triangle list, same record form as `0x0043` |
| `0x0040` | Textured strip-vertex record `[u16 index][f32 u][f32 v]` |

`0x0043` records are 10 bytes:

```text
[u16 i0][u16 i1][u16 i2][u32 BGRA]
```

`0x4353` begins with three indices plus one shared BGRA color and then stores `count - 1` additional indices with alternating strip winding.

Corpus work recovered previously missing fingernail and weapon-class blocks through these decodes.

### 9.2 `0x8010` draw state

Known fields:

| State-data offset | Meaning |
|---:|---|
| byte `+5` | Blend mode: `0x80` soft, `0x00` opaque |
| byte `+15` | Display type: ordinary/hair/face/wrist/pants/shin categories |
| float `+16` | `1.0` enables environment/reflection stage |
| approximately `+20` | Brightness/ambient multiplier candidate |
| float `+36` | Reflection intensity source; reported client use is `value × 0.5` as `TEXTUREFACTOR` alpha |

Offsets stated relative to the complete command may be two bytes higher because of the `0x8010` command word. Always state the chosen origin.

Section behavior:

- `0x8010`: full state;
- `0x8000`: texture change while inheriting state;
- nameless `0x8010`: state-only.

The tail float is not an alpha-test reference.

### 9.3 Reflection / “shiny” path

The contributed renderer trace and xim support the following unified working model:

- it is a second fixed-function texture stage;
- it is cubemap environment mapping;
- it uses camera-space normals plus a texture transform;
- `COLOROP = MODULATEALPHA_ADDCOLOR`;
- `ALPHAOP = ADD`;
- texture selection can fall back to `CubeTex`;
- the `+36` state float supplies texture-factor alpha/intensity after multiplying by `0.5`.

DATura's flat normal/specular textures are an approximation, not the mechanism described by the contributed client trace.

Still needed for a byte-faithful D3D9 implementation:

- exact texture-stage argument assignments;
- exact texture transform;
- cube-face construction and fallback resource lifetime;
- confirmation of how texture alpha and texture-factor alpha combine in every section variant.

### 9.4 Mirroring and skinning

- Mirror flag: `u16`, with apparent offset differences caused by header-origin conventions.
- Bone reference: bits 0–6 current-copy bone, 7–13 mirrored bone, 14–15 mirror axis.
- Two-weight vertices store influence-specific positions.
- Position reconstruction:

```text
pos = Ra · p1 + w1 · ta + Rb · p2 + w2 · tb
```

Weights apply to translation through homogeneous `w`; each influence rotates its own stored position.

### 9.5 Animation rules

Corpus-validated working rules:

- translation is a bind-pose delta;
- rotation is `animationQuaternion ⊗ bindQuaternion`;
- scale comes from the animation channel;
- offset zero means use the paired static value;
- negative channel offset parks the joint;
- channel constants use the format's modulo-10000 quirk;
- root joint ignores its own scale and has a special translation orientation;
- duration:

```text
(frameCount - 1) / (rate × 30)
```

- positions use a full scale/rotation/translation matrix chain;
- normals use a rotation-only chain.

### 9.6 Unresolved `0x2A` header conflict

DATura's proposed LOD fields at header offsets `0x24–0x32` overlap a corpus-verified floating-point-pool offset in the collaborator's header map. This is not adjudicated.

Do not build additional LOD parsing on those offsets until both parsers print the same raw header bytes with an explicitly shared origin.

---

## 10. Collision

Collision indices are not uniformly 14-bit:

```text
p0 index     = raw0 & 0x7FFF
p1 index     = raw1 & 0x3FFF
p2 index     = raw2 & 0x3FFF
normal index = raw3 & 0x7FFF
```

The four high nibbles assemble a 16-bit material/terrain word:

```text
materialWord = (f0 << 12) | (f1 << 8) | (f2 << 4) | f3
```

Strong-model meanings:

- bit `0x40`: hit wall / one-way or invisible collision;
- selected high bits combine into an 11-value terrain category used for footstep/effect selection.

Known runtime behavior includes:

- 0.05-unit stepping;
- candidate ordering by `abs(normal.y)`;
- wrong-side rejection;
- step-up gating.

Collision mesh transforms can carry both forward and inverse matrices, a packed map identifier, an environment link, and light indices.

**DATura action:** preserve the original high bits and expose the assembled material word; apply the correct per-index mask instead of one uniform mask.

---

## 11. Effects

The combined research establishes:

- `0x05` is a sectioned generator command stream;
- opcode meaning depends on both section and opcode;
- `0x19` supplies keyframe curves used by generator curve-binding commands;
- effect cards are not universally camera-facing;
- billboard modes include camera, movement, movement-horizontal, XZ, XYZ, and none;
- render-mode bits include camera/world space, world-freeze, and depth-test disable;
- card Euler order is `Rx · Ry · Rz` with `Rz` innermost;
- known effect blend modes include add, alpha, reverse-subtract/darken, zero-inverse-source, and opaque;
- card texture binding comes from the geometry block rather than the generator node;
- the contributed trace reports that effect rendering inherits the same `MODULATE2X` / `MODULATE4X` fixed-function convention;
- integration is frame-based.

DATura's grouped `0x21` card decode is compatible with this larger model, but static display of every decoded card is not a substitute for generator-selected group/frame behavior.

Still open for DATura:

- complete generator opcode implementation;
- generator-selected card group/frame;
- billboard constraint runtime;
- child generators;
- timing, curves, and expiration;
- exact effect draw ordering.

---

## 12. Shadows

Character/actor shadows are not wholly unknown. A retail-compatible blob path is identified with:

- `SYSTEM_KAGE` texture;
- `ShadowZBias = 15`;
- depth writes disabled;
- dedicated shadow scale vectors;
- dedicated shadow render functions.

This supersedes older statements in the collaborator document's open-items section that shadows were “unstarted everywhere.”

It does **not** resolve:

- static zone/environment shadow resources;
- the meaning of the outer DAT `is_shadow` chunk bit;
- exact projection onto arbitrary receiving geometry;
- whether special actors use additional shadow mechanisms.

---

## 13. Encryption and resource lookup

Useful additional findings:

- scheme gates differ between section families;
- keyed-run XOR length is `((key >> 4) & 7) + 16`;
- placement names use XOR `0x55`;
- the two 256-byte key tables can be content-scanned in `FFXiMain.dll`;
- first-dword anchors reported for the tables are `0xE2E506A9` and `0xB8C5F784`;
- spell and ability lists use a third popcount/rotate cipher distinct from the common zone schemes.

These details should be validated against DATura's existing decryptors before replacing working behavior.

---

## 14. Superseded claims and internal-document corrections

The source documents contain chronological edits, so a few later sections contradict newer findings elsewhere in the same file.

Treat the following as superseded:

1. **“Shiny/env-map three-way is still open.”**  
   Superseded by the `ModelRenderer.cpp:398–415` stage trace.

2. **“Shadows are unstarted everywhere.”**  
   Superseded by the `SYSTEM_KAGE` blob-shadow render path.

3. **“The `0x8010` tail float has no consumer.”**  
   Superseded by its `TEXTUREFACTOR` alpha use.

4. **“The MMB loader trace remains highest priority.”**  
   Superseded by `VerticeCountAndFlags` and `MeshBlockManager` mapping.

5. **“DATura drops the top chunk-size bit.”**  
   **Not settled.** A contributed disassembly report supports DATura's 19-bit mask, while xim implements a 20-bit mask. We do not currently possess an independently verified original-client trace or a real chunk header with the disputed bit set that distinguishes the two formulas.

6. **“0x2F is fully parsed with no caveats.”**  
   Mostly true structurally, but the clear-color field, fog-off sentinel, rare truncated records, and residue must be retained.

---

## 15. Current implementation priorities for DATura

### Highest-value corrections

1. Use character alpha reference `69/255`.
2. Decode topology and 48-byte vertex blending as independent config bits.
3. Animate 48-byte zone vertices using their second position and wind factor.
4. Replace empirical DXT3 alpha expansion with the correct pass-specific fixed-function equation.
5. Implement the complete `0x8010` cubemap stage and per-section reflection intensity.
6. Decode/render `0x0043`, `0x4353`, `0x0000`, and `0x0040` character draw forms.
7. Correct collision index masks and assemble the material/terrain word.
8. Decode placement LOD thresholds, culling/environment links, and point-light indices.
9. Parse `0x2F` environment records into fog, clear color, sky, and light configurations.
10. Implement type-`0x5D` bump resources with a clearly labeled retail-versus-modern rendering boundary.

### Important but dependent work

11. Reproduce `SYSTEM_KAGE` blob shadows after projection/receiver behavior is understood.
12. Implement effect generator timing and grouped-card selection.
13. Decode quadtree and culling-table internals.
14. Resolve point-light runtime parameters.
15. Reconcile the conflicting `0x2A` header origins before adding LOD streams.

### Continue preserving without assigning behavior

- MMB runtime flags `0x4000` and `0x1000`;
- `flags2` and unclassified object/sub/super flags;
- `0x2F` residue at `+0x58`, `+0x5C`, `+0x64`, and `+0x68`;
- collision material bits not yet mapped;
- outer chunk `is_shadow` semantics;
- rare environment record variants.

---

## 16. Remaining research questions

The following are genuinely open after reconciliation:

1. Exact original-client behavior of MMB flags `0x4000` and `0x1000`.
2. Original-client transparent-group traversal/sorting order.
3. Quadtree and visibility/culling-table internals.
4. Runtime source of point-light colors, radii, and intensities.
5. Runtime source/writer of environment sun/moon vectors and daylight curves.
6. Exact original-client D3D8 bump-map stage state.
7. Native mip sampling and the semantic purpose of the `MULTI` image container.
8. Conflicting character `0x2A` header origins and LOD/f32-pool fields.
9. Remaining `0x8010` material/display fields and exact texture-stage arguments.
10. Static zone shadow resources and the outer `is_shadow` bit.
11. Full effect generator opcode semantics and execution.
12. Character LOD selection and DMB/character-creation compositing.

---

## 17. Sources cited by the two input documents

Primary source families:

- Contributed report of `FFXiMain.dll` disassembly and a full `SetRenderState` call-site census; underlying artifacts not independently inspected for this document
- `thirdparty/xim`
- cexi parsers and corpus tooling
- AltanaView / Noesis viewer lineage, where explicitly labeled

Frequently referenced implementation files:

- `MeshBlockResource.h`
- `MeshBlockManager.cpp`
- `ModelRenderer.cpp`
- `ZoneRenderer.cpp`
- `xim/resource/DatParser.kt`
- `xim/resource/TextureSection.kt`
- `xim/resource/ZoneMeshSection.kt`
- `xim/resource/ZoneDefParser.kt`
- `xim/resource/EnvironmentSection.kt`
- `xim/resource/BumpMapSection.kt`
- `xim/resource/SkeletonMeshSection.kt`
- `xim/poc/gl/XimShader.kt`
- `xim/poc/gl/GLDrawer.kt`
- `cexi/zone/xi_collision.py`

Line numbers refer to the versions used by the input documents and may move as those projects evolve.
