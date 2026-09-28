# How Final Fantasy XI Renders Geometry and Textures

## Status and scope

This document records what is currently known about the geometry and texture pipeline used by **Final Fantasy XI (FFXI)**, with an emphasis on facts recoverable from the game's DAT files and behavior reconstructed in DATura. It also records plausible interpretations and unanswered questions. It is intentionally stricter than a viewer-implementation guide: a viewer can produce a convincing image without having proved that the retail client used the same render states, ordering rules, thresholds, or coordinate transforms.

The word **retail** below means the official FFXI client. **DATura behavior** means the current implementation in this repository. They are not interchangeable.

This is a living reverse-engineering document, not an official Square Enix specification. It describes the formats and behavior observed as of **July 2026**.

### Confidence vocabulary

| Label | Meaning |
|---|---|
| **Confirmed** | Directly represented in decoded DAT bytes, reproduced across samples, or unambiguous in source code that parses those bytes. |
| **Strongly supported** | Multiple independent observations agree, but the exact retail implementation has not been captured or disassembled. |
| **DATura reconstruction** | Deliberate viewer behavior that makes decoded assets render plausibly. It may imitate retail but is not, by itself, evidence of retail behavior. |
| **Speculation** | A plausible explanation that still needs controlled testing, a client capture, or disassembly. |
| **Unknown** | No defensible conclusion yet. |

## Executive summary

FFXI uses a compact, chunk-oriented asset pipeline designed for early-2000s hardware. Geometry, textures, skeletons, animations, zone placements, and zone meshes appear as typed chunks inside DAT files. The important chunk types for visible 3D content are:

| Type | Interpreted purpose | Confidence |
|---:|---|---|
| `0x1C` | Zone object placement/map data | Confirmed |
| `0x20` | Texture | Confirmed |
| `0x05` | Effect generator command stream | Header/section framing decoded; runtime incomplete |
| `0x19` | Keyframe float pairs | Record framing decoded |
| `0x1F` | Ordinary effect triangle model | Geometry decoded |
| `0x21` | Animated/card effect model | Static geometry decoded |
| `0x25` | Morphing effect model | Base geometry decoded; morph runtime incomplete |
| `0x29` | Skeleton | Confirmed for common character assets |
| `0x2A` | Character/equipment geometry | Confirmed for common assets |
| `0x2B` | Skeletal animation | Mostly understood; some channel semantics remain uncertain |
| `0x2E` | Zone/map geometry | Confirmed for common zone assets |
| `0x2F` | Environment, lighting, and fog record | Confirmed by xim parsing |
| `0x3D` | Sound pointer | Confirmed as a resource pointer, not a rendering primitive |
| `0x5D` | Bump-map height field | Confirmed by xim parsing |

xim's broader resource map also identifies `0x04` table, `0x06` route, `0x07` effect routine, `0x21` sprite-sheet mesh, `0x25` weighted/morphing mesh, `0x30`/`0x31` UI menu/element group, `0x36` zone interactions, `0x3E` point list, `0x45` info, `0x49` spell list, `0x4A` path, `0x53` ability list, `0x54` weapon trace, and `0x5E` blur. These are included for resource identification; most are outside this document's geometry/texture focus.

At a high level:

1. The client locates a DAT through the ROM file tables.
2. It walks 16-byte chunk headers and conditionally decrypts/deobfuscates chunk payloads.
3. Texture chunks provide a 16-byte name and either indexed-color pixels, DXT-compressed pixels, or both.
4. Character `0x2A` geometry uses a draw-command stream that switches texture/material names and emits triangle lists or strips. Vertices can be rigid or two-weight skinned.
5. Zone `0x2E` geometry stores named reusable meshes as bounded groups of draw batches. Zone `0x1C` records place those meshes using non-uniform scale, X/Y/Z rotations composed as `Rz * Ry * Rx`, and translation.
6. A draw batch supplies positions, normals, packed vertex color, one UV set, a 16-byte texture/material name, 16-bit indices, and state-like flag words.
7. The visible result is consistent with a Direct3D 8-era fixed-function design: texture color multiplied by interpolated vertex color, with separate opaque, alpha-tested cutout, and source-alpha-blended cases. The exact retail equations and state transitions are not all proved.

The most important negative result is that **the DATs do not expose a modern PBR material system**. There is no confirmed per-pixel roughness/metalness workflow. Native `0x5D` bump maps and character cubemap reflections do exist, but DATura's flat normal/specular helper textures and material suffixes remain viewer-generated conveniences rather than original FFXI assets.

## Evidence base and limitations

The strongest local evidence is the parser and renderer implementation:

- [`DATura/model_ff11.h`](DATura/model_ff11.h) defines known chunk types and inspection structures.
- [`DATura/model_ff11_texture_handler.h`](DATura/model_ff11_texture_handler.h) defines the private texture handler that decodes headers and pixel payloads.
- [`DATura/model_ff11_geometry_handler.h`](DATura/model_ff11_geometry_handler.h) defines the private geometry handler for character draw commands, primitive data, mirroring, and skinning.
- [`DATura/model_ff11_map_handlers.h`](DATura/model_ff11_map_handlers.h) defines the private handlers for zone placements, geometry batches, vertex layouts, indices, and candidate state flags.
- [`DATura/model_ff11_animation_handlers.h`](DATura/model_ff11_animation_handlers.h) defines the private skeleton and animation handlers.
- [`DATura/model_ff11_creation.cpp`](DATura/model_ff11_creation.cpp) implements the separate high-poly character-creation shape and DMB texture formats.
- [`DATura/model_ff11_effect_handler.h`](DATura/model_ff11_effect_handler.h) defines the current experimental effect-mesh handler.
- [`DATura/model_ff11_decrypt.h`](DATura/model_ff11_decrypt.h) documents the known obfuscation/decryption passes and their confidence.
- [`DATura/main.cpp`](DATura/main.cpp) is the standalone D3D9 reconstruction. Its render states are useful experiments but must not automatically be attributed to retail.

Legacy FFXI Tool structures independently agree on the broad `0x2A` header, one- and two-weight vertex layouts, triangle-list and strip records, skeleton records, and Direct3D 8-era vertex format. The source explicitly targets Direct3D 8 (`DIRECT3D_VERSION 0x0800`). This corroborates the format interpretation and historical rendering family, but does not prove every state used by the current retail client.

The `thirdparty/xim` client reimplementation and cexi parsers provide additional byte-level evidence. Their resource parsers have been checked against retail DATs and decode several fields that DATura still preserves only as raw state. Claims attributed to xim/cexi below describe that corroborated implementation evidence rather than direct original-client disassembly.

Microsoft's Direct3D documentation is useful for terminology: DXT1/3/5 are native D3D compressed formats, texture stages can combine texture and interpolated diffuse color, and source-alpha/inverse-source-alpha is the conventional non-premultiplied blend pair. See [D3DFORMAT](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dformat), [texture blending](https://learn.microsoft.com/en-us/windows/win32/direct3d9/texture-blending), [texture alpha](https://learn.microsoft.com/en-us/windows/win32/direct3d9/texture-alpha), and [D3DTEXTUREOP](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dtextureop).

### What this evidence cannot prove on its own

- The exact retail draw order or batching strategy.
- The exact Direct3D render state assigned to every unknown flag bit.
- Whether current PC-client behavior exactly matches the original PS2 or early PC path.
- The complete shadow, particle, post-processing, and UI compositing pipelines, plus platform-specific differences in the decoded lighting/fog path.
- Whether a value that looks unused is truly unused by retail.
- Whether DATura's broader name-family fallbacks correspond to additional retail tests beyond xim's confirmed leading-underscore cutout rule.

## DAT container and chunk processing

### Chunk header

**Confirmed.** Visible assets are stored in a stream of chunks with a 16-byte outer header. DATura reads:

```text
offset  size  meaning
0x00    4     short chunk identifier/name
0x04    4     packed information word
0x08    8     additional header bytes not generally interpreted here
```

From the packed word:

```text
type = info & 0x7F
size = (info >> 3) & 0xFFFFF0
is_shadow    = (info >> 26) & 1
is_extracted = (info >> 27) & 1
version      = (info >> 28) & 7
is_virtual   = (info >> 31) & 1
payload starts at chunk offset + 16
```

The size is effectively 16-byte aligned. The parser advances by that size, not by searching for signatures. DATura preserves all common-header flags in its raw inspector. It does not assign speculative rendering behavior to them; notably, surveyed `0x2F` environment records use version 1 and some directory/generator records set `is_virtual`.

### Directory chunks

**Confirmed at the structural level.** Types `0x01` and `0x00` act as directory open/close markers. They supply hierarchy/context to a mixed asset stream but are not themselves geometry.

### Encryption/obfuscation

**Confirmed for zone map (`0x1C`) and map geometry (`0x2E`).** Later format versions conditionally obfuscate payload bytes. The known algorithms use a 256-byte key table, rolling XOR decisions, and—in some MMB cases—a secondary block-swap pass. `0x1C` also XORs each 16-byte object ID with `0x55` after the main pass.

**Inferred for `0x20`, `0x29`, `0x2A`, and `0x2B`.** DATura applies the same MMB-family decoder conditionally. Because the decoder is version-gated, it is a no-op on common unencrypted payloads. This inference works on tested files but should not yet be called a universal format rule.

### Names as resource links

**Confirmed.** Textures and geometry batches use fixed 16-byte names. Character draw commands and zone batches refer to those names rather than embedding a modern material object. Zone placements similarly reference a 16-byte geometry object name.

Names are therefore part of the runtime binding system, not just labels. They also preserve production conventions such as `model`, `effect`, terrain/vegetation abbreviations, and `_h`/`_m`/`_l` detail suffixes.

## Texture storage and decoding

### Common texture header

**Confirmed for the commonly decoded form.** The packed texture header begins with:

```text
offset  size  interpretation
0x00    1     texture type byte
0x01    16    texture/resource name
0x11    4     version (commonly 40)
0x15    4     width
0x19    4     height
0x1D    2     unknown u16
0x1F    2     bit count (notably 32 for raw BGRA)
0x21    20    remaining format/state fields
0x35    4     palette color-depth-like field
```

Because these structures are byte-packed, the version is at `0x11`, not an aligned `0x14`. Some exploratory scanners also try aligned candidates to recover malformed or differently wrapped records; that is a scanner tolerance, not evidence of a second canonical header.

The final field is accepted as `16` or `32` by DATura and is best described as **palette entry depth**, although its full semantics remain unverified.

### Observed texture type bytes

| Byte | DATura name | Payload interpretation | Confidence |
|---:|---|---|---|
| `0x01` | Pal2 | 256-entry palette followed by 8-bit indices | Confirmed decoding; distinction from `0x91` unknown |
| `0x81` | PalCombo | Palette/indexed image followed by a DXT copy | Confirmed |
| `0x91` | Pal/raw | Palette plus indices, or raw 32-bit BGRA when `bitCount == 32` | Confirmed |
| `0xA1` | DXT | DXT-tagged compressed image | Confirmed |
| `0xB1` | PalLeadingInt | Unknown 32-bit value, then palette/indexed image | Confirmed layout in samples; purpose unknown |

The low bit is consistently set in known values. The hypothesis that only the high nibble selects the fundamental type is plausible but not proved.

### Paletted textures

**Confirmed for common files.** A paletted payload contains 256 palette entries and one byte of palette index per pixel:

```text
palette bytes = 256 * (paletteEntryBits / 8)
index bytes   = width * height
```

DATura interprets 32-bit entries as `B8G8R8A8` and 16-bit entries as `B5G5R5A1`. It then expands them to RGBA.

**Confirmed in the importer:** indexed rows are vertically reversed during expansion:

```text
sourceIndex = (height - y - 1) * width + x
```

This strongly indicates that the stored indexed image is bottom-up relative to DATura/Noesis texture coordinates. Whether the retail client changes UVs instead, changes upload orientation, or uses an equivalent addressing convention is not established.

For `0x91`, `bitCount == 32` selects a raw `width * height * 4` BGRA payload with no palette. Other observed `0x91` records use the palette/index path. The complete semantic distinction between `0x01` and `0x91`, and the meaning of the leading 32-bit word in `0xB1`, remain unknown.

### DXT textures (not standalone DDS files)

**Confirmed.** FFXI stores a proprietary 12-byte mini-header followed by native block-compressed data. This is not a standard DDS container: it has no `DDS ` magic or 124-byte `DDS_HEADER`. The first four mini-header bytes are stored in FFXI's byte order (for example, `3TXD` in the file) and compare as `DXT3` when read as a little-endian 32-bit tag by the Windows client. Retail `DXT1` and `DXT3` payloads are confirmed. DATura also accepts `DXT5`, but neither xim nor cexi has encountered a retail `5TXD` payload, so native DXT5 presence remains unverified.

- DXT1 uses 8-byte blocks for each 4×4 texel region and may encode one-bit transparency depending on endpoint ordering.
- DXT3 uses 16-byte blocks with explicit 4-bit alpha.
- DXT5 uses 16-byte blocks with interpolated alpha endpoints/indices; this is supported defensively by DATura, not confirmed in retail DATs.

Those codec properties are standard Direct3D behavior. DXT1 and DXT3 presence in FFXI is directly confirmed by FourCC and payload; DXT5 is only an accepted parser path at present.

### Combination textures (`0x81`)

**Confirmed.** A combination texture stores both representations in this order:

```text
palette
width * height index bytes
DXT mini-header and block data
```

**Strongly supported, not proved:** the retail-facing path likely chooses the DXT copy. DATura defaults to DXT because it matches observed client-like alpha behavior and keeps an option to prefer the palette copy for source inspection. Older importer commentary preferred the palette because it can retain higher color fidelity. Both can be true: the palette may be a higher-quality source representation while retail chooses the GPU-friendly compressed copy.

This dual representation may exist for platform, toolchain, fallback, or quality reasons. Which platform consumed which copy remains unknown.

### Alpha range

There are two related observations:

1. The original importer historically left-shifts texture alpha by two for decoded RGBA paths, saturating to 255.
2. DATura's hardware-DXT path historically treated authored DXT3 alpha as approximately a reduced `0..8` nibble range and expanded sampled alpha by about `1.875` before use.

xim clarifies this as the common `0x80 == 1.0` convention: texture alpha and vertex alpha are each scaled by two, producing `4 * vertexAlpha * textureAlpha`. The apparent `1.875` texture factor is the 4-bit DXT3 form of the same scale (`15/8` versus `255/128`), rather than a separate codec rule.

**Not universally confirmed:** all texture classes, all DXT3 assets, and all retail render paths may not use the same expansion. DMB character-creation atlases already contain full-range RGBA and must not receive the DXT expansion. Palette alpha also needs asset-by-asset validation.

### Mipmaps

**Unknown in the native DAT format.** The common texture header fields have not yielded a confirmed mip count or per-level table. DATura validates and retains exactly the top-level DXT block count; in the inspected Konschtat DAT, every native DXT payload was followed by 11 alignment bytes rather than another image level. The D3D9 uploader asks the driver to generate a mip chain from that validated top level and falls back to a one-level texture when automatic generation is unsupported for the format.

Possible explanations include:

- other DXT payload classes contain additional levels not yet identified;
- retail generates mipmaps at load time;
- mip levels live in unknown header fields or companion data;
- some asset classes deliberately use only a top level; or
- the original target relied heavily on bilinear filtering without full mip chains.

This should be tested by comparing the exact compressed top-level byte count against the remaining payload length across many textures.

### Addressing and filtering

**DATura reconstruction:** wrap addressing in U and V, linear minification/magnification, and optional linear mip filtering.

**Strongly supported in broad terms:** many zone UVs extend naturally across repeating surfaces, so wrap addressing is likely common. Linear filtering is consistent with the era and visual output.

**Unknown:** whether every material uses wrap, whether UI/effects use clamp, the retail anisotropy setting, per-material LOD bias, and the exact filtering chosen by each platform/client configuration.

## Character and equipment geometry (`0x2A`)

### Geometry header

**Confirmed for common character/equipment assets.** Most offsets and sizes are measured in 16-bit words rather than bytes.

| Byte offset | Field | Interpretation |
|---:|---|---|
| `0x00` | version + unknown byte | Header/version information |
| `0x02` | type/flags | Low 7 bits distinguish normal model vs cloth/class-like data; bit 7 enables a bone-reference table |
| `0x04` | mirror | Zero = one pass; nonzero = emit an additional mirrored pass |
| `0x06` | draw stream offset | Offset in 16-bit words |
| `0x0A` | draw stream size | Size in 16-bit words |
| `0x0C` | bone-reference table offset | Optional indirection table |
| `0x10` | bone-reference count | Entry count |
| `0x12` | weighted-count table offset | Separates one-weight from two-weight vertices |
| `0x16` | weight count/limit field | Historically called maximum weights per vertex |
| `0x18` | bone/weight descriptor offset | Packed bone indices and mirror axis |
| `0x1C` | descriptor count | Count/size field |
| `0x1E` | vertex offset | Vertex data offset |
| `0x22` | vertex data size | Size/count field |
| `0x24`–`0x32` | LOD offsets/counts | Two additional polygon/LOD regions |
| `0x34`–`0x3F` | unknown | Four unknown fields |

The LOD fields are structurally present and identified by legacy tools, but DATura currently renders the primary draw stream. The retail selection thresholds and exact encoding of both lower-detail sets remain unknown.

### Draw-command stream

**Confirmed.** The primary stream is a small display-list-like language:

| Command | Meaning |
|---:|---|
| `0x8000` | Set 16-byte material/texture name |
| `0x8010` | Set native draw state (only partly decoded) |
| `0x0054` | Triangle list |
| `0x5453` | Triangle strip |
| `0x4353` | Single-color untextured triangle strip |
| `0x0043` | Untextured triangle list: three indices plus one BGRA color per triangle (`count * 10` bytes) |
| `0xFFFF` | End |

This is a major architectural clue: material selection and primitive emission are interleaved. The asset is closer to a compact display list than to a modern mesh with one immutable material record.

### Primitive data and UV ownership

**Confirmed.** In character geometry, UVs are stored with primitive references rather than solely in the base vertex array:

- Triangle-list records contain three 16-bit vertex indices and three `float2` UVs per triangle.
- A strip begins with a complete three-corner record, followed by records containing one index and one `float2` UV.

Consequences:

- The same position/normal vertex can be referenced with different UVs across seams.
- Exporters may have to split vertices when converting to formats that require one UV per indexed vertex.
- Vertex counts in an exported model need not equal native position-record counts.

### Base vertex layouts

**Confirmed for common assets.** Geometry supports one-weight and two-weight records, with optional normals.

One-weight normal-model vertex:

```text
position: float3
normal:   float3
```

Two-weight normal-model vertex:

```text
position contribution 0/1 interleaved by component: x0,x1,y0,y1,z0,z1
weights: w0,w1
normal contribution 0/1 interleaved by component: nx0,nx1,ny0,ny1,nz0,nz1
```

Cloth/class-like records omit normals in the known interpretation and retain one or two position contributions plus weights.

The first entry in the weighted-count table identifies how many vertices use one weight; later vertices use the two-weight form. The pipeline therefore appears optimized for at most two influencing bones per vertex.

### Skinning model

**Confirmed at the data level; retail arithmetic partly inferred.** Each skinned vertex has up to two packed descriptors. Each descriptor contains:

- a 7-bit bone index for the normal pass;
- a 7-bit bone index for the mirrored pass; and
- a 2-bit mirror-axis selector.

An optional bone-reference table remaps local indices to skeleton indices.

FFXI's two-weight position representation is unusual. Rather than storing one bind position shared by both bones, the DAT stores a separate position contribution for each influence. DATura reconstructs model-space position by transforming each contribution with its bone matrix and placing the weight in the homogeneous `w`, which weights translation as well as rotation. Normals use separate per-influence values and conventional weighted transformed-normal accumulation followed by normalization.

The original importer notes that the position method is unconventional but visually agrees with the game. A definitive retail equation still requires client disassembly or controlled GPU capture.

### Mirrored geometry

**Confirmed.** When the mirror field is nonzero, the draw stream is emitted twice. The second pass:

- uses the mirrored bone index from each descriptor;
- optionally applies an X, Y, or Z reflection matrix according to the 2-bit mirror axis; and
- reverses triangle winding (or uses a flipped strip primitive).

This saves geometry for approximately symmetrical character parts while still allowing different bones on the opposite side.

### Normals and the type flag

DATura currently treats any low-seven-bit type value as an indication that normals are absent (`flag & 0x7F != 0`). Legacy FFXI Tool comments identify low value `0` as a normal model and `1` as cloth.

**Strongly supported for common assets; not universal:** the field likely selects a vertex class/layout, with cloth-like data omitting stored normals. Additional low-bit values and their semantics remain unknown.

### Draw state (`0x8010`)

The record presently maps as:

```text
RGBA-like 4 bytes
two floats
one 32-bit unknown
four floats
one 32-bit unknown
two floats tentatively associated with specular behavior
```

**Confirmed:** the record changes within draw streams and is state-like.

**Loader/renderer-confirmed reflection fields:** float `1.0` at state-data `+0x10` enables texture stage 1, and the float at state-data `+0x24`, multiplied by `0.5`, becomes `TEXTUREFACTOR` alpha/reflection intensity rather than an alpha-test reference. DATura now uses that enable field instead of its old tail-float heuristic when selecting the reflection-material path.

Texture stage 1 uses camera-space normals and a texture transform. Its color operation is `MODULATEALPHA_ADDCOLOR`, its alpha operation is `ADD`, and texture binding falls back to `CubeTex`. Retail DATs contain `cubemap*` DXT3 textures; xim constructs a cube map from them, filling blank faces with alpha `0x80`, and applies the `0x8010` intensity as specular power. This confirms that the shiny character state is a second-stage cubemap environment map. DATura's generated flat-normal/specular material is still only an approximation until its D3D9 path reproduces the complete stage state and cube-resource construction.

### Character LOD

The header explicitly carries two additional polygon regions and associated counts. This is good evidence that at least some character assets contain multiple detail levels.

**Unknown:**

- whether all three levels share one vertex pool;
- how UVs and state commands differ in the LOD streams;
- screen-space/distance thresholds;
- whether equipment parts can select LOD independently; and
- how mirroring interacts with lower LODs.

## Skeletons and animation as they affect rendering

### Skeleton (`0x29`)

**Confirmed for common assets.** A skeleton begins with a bone count, followed by packed bones containing:

```text
parent index: 8-bit
terminal flag: 8-bit
rotation: quaternion float4
translation: float3
```

If a bone's parent index equals its own index, it is treated as a root. Local transforms are accumulated into model-space bone matrices before geometry is rendered.

### Animation (`0x2B`)

**Mostly confirmed.** Animation chunks contain a frame count, element count, speed scale, per-bone channel descriptors, base quaternion/translation/scale values, and indexed float tracks. An element may reference up to ten scalar channels: four rotation, three translation, and three scale.

DATura computes playback rate as `speedScale * 30`. This is an implementation interpretation, not yet a proved universal retail timebase.

**Known gap:** current DATura animation reconstruction preserves bind-pose translation for non-root bones because applying the apparently indexed translation channels can collapse limbs in tested assets. This means the translation/scale index semantics, reference frame, or composition order is still incomplete. Geometry format understanding is stronger than full animation understanding.

## Zone geometry (`0x1C` placements + `0x2E` meshes)

### Separation of reusable geometry and placement

**Confirmed.** A zone does not store all visible triangles as one monolithic world mesh.

- `0x2E` chunks define named reusable geometry objects.
- `0x1C` records instantiate them using the same 16-byte object name.

This supports repetition of terrain tiles, roads, props, structures, vegetation, and other components without duplicating all vertex data.

### Placement record

**Confirmed for the common 96-byte record.** Each placement contains:

```text
name:        char[16]
translation float3
rotation    float3
scale       float3
lod/link    float4          (effect DatId, high-detail threshold, mid-detail threshold, draw distance)
data2       int32[8]        (flag bytes, culling-table link, environment DatId link, point-light indices)
```

xim composes the transform as `T * Rz * Ry * Rx * S` (angles interpreted as radians). A negative scale determinant reverses winding. DATura's older description of this as an XYZ Euler composition was imprecise.

The integer region begins with four flag bytes; among the decoded behavior, `flags1 & 0x2` skips the object during decal rendering. Other fields link a culling table and per-object environment/lighting record and identify point lights. DATura also exposes two packed words—historically called `data2[0]` and `data2[4]`—for diagnostics; bit `0x01000000` in the first correlates with explicit transparency/cutout objects in current tests, but its universal retail name and behavior are not proved.

The high/mid thresholds and draw distance provide the missing per-placement LOD selection data for `_h`, `_m`, and `_l` resources. Some remaining flag bits, visibility/schedule behavior, and placement hierarchy are still unknown.

### Placement-table tail and spatial organization

The decoded `0x1C` header points to the end of the object table. Data after the placement array remains largely unmapped.

**Speculation:** the tail likely contains visibility, spatial partitioning, cell/bucket, or runtime lookup data. Zones need efficient culling, and the official collision data demonstrably uses a grid. Similar spatial organization for visual objects would be unsurprising, but it has not been decoded.

### Map-geometry header and groups

**Confirmed.** A `0x2E` payload starts with:

```text
4 bytes       header/version-like data
uint32        unknown
char[8]       unknown name/tag
char[16]      reusable object name
```

It is followed by nested “super” and “sub” draw headers. Each header contains:

```text
int32   segment count
float6  bounds
int32   flag
```

The six floats are consistent with an axis-aligned bounding box. Their use for retail culling is **strongly supported** but not directly proved. The nested grouping likely gives the client a hierarchy or convenient batch subdivision.

### Zone draw batch

**Confirmed.** Each leaf batch contains:

```text
char[16]   material/texture name
uint16     vertex count
uint16     blend/state flags
vertices   count * stride
uint16     index count
uint16     secondary flags
uint16[]   indices
padding    to 4-byte boundary
```

All observed indices are 16-bit. Primitive mode and vertex stride are selected independently by bits in the `0x2E` header configuration byte:

| Config bit | Meaning when set |
|---:|---|
| bit 0 | Triangle strip; clear means triangle list |
| bit 1 | Vertex blending enabled; use the 48-byte layout instead of 36-byte |

These bits can vary independently; a triangle list is not inherently a 48-byte batch, nor is a strip inherently 36-byte.

### Zone vertex layouts

**Confirmed in current parser.** Both layouts contain one position, one normal, a four-byte color, and one `float2` UV.

36-byte form:

```text
0x00 float3 position
0x0C float3 normal
0x18 u8[4] vertex color/alpha
0x1C float2 UV
```

48-byte form:

```text
0x00 float3 base position
0x0C float3 displacement vector
0x18 float3 normal
0x24 u8[4] vertex color/alpha
0x28 float2 UV
```

The extra 12 bytes are a displacement vector used for vertex blending such as foliage sway, not an absolute second position. xim evaluates `position0 + positionBlendWeight * displacement`. Installed Konschtat grass corroborates this interpretation: tip vertices have nonzero vectors while root vertices have zero vectors.

### Lists, strips, and winding

**Confirmed.** Triangle-list batches consume indices in groups of three. Strip batches slide over triples, swap the second and third corner on odd triangles, and skip degenerates. Negative placement scale reverses final winding.

DATura expands both forms to triangle lists for its D3D9 buffers. That expansion is a viewer choice; retail may submit native strips.

### Bounds and suspicious triangles

DATura rejects map triangles with an edge longer than 80 world units. This suppresses parser failures that connect unrelated memory/segments.

**DATura safety heuristic only.** It is not a native FFXI rendering rule and must never be cited as a retail culling threshold. A correct parser should ultimately make this guard unnecessary.

### Geometry references and apparent LOD names

Object names commonly use `_h`, `_m`, and `_l` suffixes suggestive of high, medium, and low detail. DATura has a fallback that changes a missing `_m` reference to `_h`.

**Strongly supported:** these suffixes encode authored detail variants.

The placement record supplies high-detail and mid-detail distance thresholds plus a maximum draw distance. The original PS2 client selects no model when nonzero draw distance is reached, low beyond the second threshold when available, medium beyond the first threshold when available, and high otherwise. Missing high geometry falls back to medium and then low during resource mapping. A separate grid-chip path uses high detail inside squared grid distance `5` and medium outside it.

### Unreferenced geometry and environment meshes

Zone DATs can contain `0x2E` objects not directly named in the decoded placement list, including clouds, stars, sun/moon spheres, lightning, dust, helper/collision meshes, and alternate LODs.

**Confirmed:** these named geometry chunks exist.

**Unknown:** their true placement and activation mechanism. DATura optionally renders selected environment names at identity plus hand-chosen scale/height. Those transforms are explicitly reconstruction heuristics, not decoded retail transforms.

## Materials, vertex color, and the likely fixed-function equation

### Native “material” identity

**Confirmed.** The strongest native material link is the 16-byte name used by a draw command or batch. A matching texture chunk supplies the image.

There is no confirmed serialized material block corresponding to modern Noesis fields such as roughness, metalness, anisotropy, rim lighting, or occlusion.

### Vertex color

**Confirmed.** Zone vertices carry four packed color bytes. The fourth byte behaves as alpha in reconstruction. Character `0x2A` geometry instead has state records and does not use the same zone vertex layout.

xim's zone shader and independent reconstruction agree on the fragment equation:

```text
lit.rgb = vertex.rgb * (ambient + directional0 + directional1 + pointLights)
RGB     = saturate(2 * lit.rgb * texture.rgb)
```

and, where authored alpha matters:

```text
A = saturate(4 * vertexAlpha * textureAlpha)
```

The `0x80 == 1.0` convention applies independently to vertex alpha and texture alpha, producing the combined factor of four. DATura's earlier approximately `1.875` DXT3 expansion was the same convention viewed through 4-bit quantization (`15/8` versus `255/128`), not a separate texture-only rule. State-specific exceptions and platform differences may still exist.

### Lighting

Zone lighting is not purely baked. xim's zone shader applies ambient light, two directional diffuse lights, and point lights using vertex normals, modulates the result by vertex color, and then applies the texture modulation above. The values come from `0x2F` environment records and are interpolated across time-of-day and weather states. Textures and vertex colors still carry substantial authored lighting, so adding unrelated modern lights will over-light the scene.

Character materials also use fixed-function lighting and the confirmed camera-normal cubemap reflection stage. Remaining uncertainty includes unmapped character state fields, indoor/local-light selection details, and PS2 versus PC differences.

### DATura-generated material variants

DATura/Noesis creates names such as:

```text
<texture>_explicitshiny
<texture>_explicitsoftblend
<texture>_explicitnoblend
<texture>_explicithardalpha
...cullback variants
```

**Confirmed viewer behavior; not native names.** These variants carry decoded/reconstructed draw state through the generic Noesis material interface.

The shiny variant uses generated 4×4 `__flat_normal` and `__flat_spec` textures plus a specular color/exponent. Those particular textures do not come from the DAT and should not be confused with either the native character cubemap path or native `0x5D` zone bump maps.

## Alpha, cutouts, blending, culling, and ordering

### Three practical classes

Current evidence supports at least three useful rendering classes:

1. **Opaque:** depth test and depth writes; authored alpha ignored for surface coverage.
2. **Hard alpha/cutout:** depth test and writes plus alpha comparison; useful for vegetation, nets, hair-like cards, and similar binary coverage.
3. **Soft blend:** source-alpha blending, usually with depth writes disabled; useful for overlays, translucent detail, clouds, effects, and decals.

The existence of multiple alpha behaviors is confirmed. Important classifiers are now known, though some state interactions remain incomplete.

### Zone `VerticeCountAndFlags`

The dword immediately following each 16-byte texture name is loader-confirmed as `VerticeCountAndFlags`:

| Dword bits | Runtime form | Confirmed interpretation |
|---:|---:|---|
| `0x0FFFFFFF` | — | Vertex count |
| `0x80000000` | u16 `0x8000` | `IsTransparent` |
| `0x20000000` | u16 `0x2000` | `DisableCulling` |
| `0x40000000` | u16 `0x4000` | Additional flag; behavior not yet named |
| `0x10000000` | u16 `0x1000` | Additional flag; behavior not yet named |

This mapping is traced through `MeshBlockResource.h:33–39` into `MeshBlockManager.cpp:514–517`. DATura historically split the dword into a 16-bit count and a `blendFlags` word; that happened to preserve common small counts and the high flags, but it obscured the actual structure. The legacy claim that the high nibble was a color multiplier is disproved.

**Unknown:** the behavior of runtime flags `0x4000` and `0x1000`, interactions with `flags2`, and interactions with super/subheader flags.

### Hard-alpha classification

For zone meshes, xim selects the hard-alpha path when the mesh name begins with `_`; this is the mechanism, not merely a correlating heuristic. The zone alpha-test threshold is `0.375`, matching DATura's value. DATura's broader vegetation/name-family list remains a compatibility fallback beyond that confirmed leading-underscore rule.

Character discard thresholds are platform-specific. A later PC path uses `69/255`, approximately `0.27`. The original PS2 display-list path emits alpha reference `0x30`; after accounting for FFXI's `0x80 == 1.0` convention, this corresponds to approximately `0.375` in conventional normalized alpha. DATura's generic threshold of `0.5` matches neither path.

### Soft blending

DATura reconstructs `0x8000` batches with:

```text
source blend      = source alpha
destination blend = inverse source alpha
depth test         = less-or-equal
depth writes       = off
alpha test         = off
depth bias         = 8
```

This produces plausible rock/detail overlays and prevents zero-alpha texels from introducing a second cutout rule.

The source/destination blend pair is strongly supported by ordinary non-premultiplied texture content. xim records the retail zone blend-layer depth bias as `8`; transparent sorting and some surrounding depth-state details remain reconstruction-dependent.

### Transparent sorting

DATura preserves each blended draw record as a separate submesh, then sorts blended submeshes roughly far-to-near using sampled centers.

**DATura reconstruction.** It fixes visible composition problems but does not establish the retail ordering algorithm. Retail may preserve author order, sort at object/batch level, use buckets, split intersecting layers, or combine these approaches.

### Culling

**Corroborated external finding:** the supplied loader/manager trace and xim agree that runtime flag `0x2000`, sourced from DAT dword bit `0x20000000`, disables culling for that zone batch. This mapping has not been independently reproduced against the original client. Negative placement scales require winding correction. Character mirroring also requires reversed winding.

DATura globally defaults many materials to two-sided unless an explicit cull-back variant is selected. That default is viewer legacy behavior and is not evidence that all retail character geometry is two-sided.

## Konschtat Highlands case study: flowers, grass, and Cermet Crags

`ROM/0/90.DAT` (Konschtat Highlands) became a useful controlled test zone because it contains all three practical coverage classes in a compact area: opaque terrain, hard-cutout vegetation/flowers, and overlapping translucent detail on the Cermet Crags. The work on this zone corrected several tempting but incorrect assumptions: a texture's alpha does not select a render mode by itself, a horizontally placed flower sheet is not the same kind of mesh as a vertical grass card, and matching material names do not mean transparent draw records may be merged.

### What the FFXI files establish

#### Zone resources and texture storage

The zone's `0x1C` placement data refers to named reusable `0x2E` map-geometry objects. Each leaf draw batch names one texture, supplies its own vertices and indices, and carries its own state-like `blendFlags`. The texture name is therefore only an image binding; it is not a complete material definition.

The relevant Konschtat images are `0xA1` texture chunks with FFXI's 12-byte DXT mini-header followed by block-compressed pixels. For example, the flower atlas `con_f01c` is 256x256 DXT3, while Crag images such as `con_wi1`, `con_wk1`, `con_frg1`, and `con_fsg1` are also DXT3. Inspection of these assets reports authored DXT3 alpha values up to 136 rather than using the whole nominal 0..255 range after generic expansion.

These chunks are **not DDS files**. DDS is a file container with a `DDS ` signature and a 124-byte `DDS_HEADER`; an FFXI DXT chunk has neither. DDS and FFXI DXT share the same block codecs (DXT1/3/5), so a tool may wrap or unwrap the blocks for inspection, but that conversion is not required for rendering. The client-like DATura path uploads the original validated blocks directly to `D3DFMT_DXT1`, `D3DFMT_DXT3`, or `D3DFMT_DXT5` and lets Direct3D decode them during sampling.

#### Flower sheets and grass cards

The ground flowers are named `_con_hana_*` and bind regions of the `con_f01c` atlas. The inspected flower batches carry `0x2000` and do **not** carry `0x8000`. Together with the atlas alpha and the observed result, that makes a cutout reconstruction appropriate rather than a source-alpha terrain-overlay reconstruction; it does not yet reveal a universal native cutout-state bit. The visible flower patches are broad, ground-parallel sheets whose alpha removes most of the rectangular atlas region. They are distinct from the nearby `_kusa_*` objects: those are upright grass-card meshes, even though both asset families need binary alpha coverage.

The `_con_hana_*` family includes both vertex-blended 48-byte batches and ordinary 36-byte batches, and primitive mode is controlled independently. The extra displacement vector in the 48-byte form supports motion such as foliage sway; it does not determine the flower alpha rule.

#### Cermet Crag layers

Crag objects such as `con_ciwa_m`, `con_oiwa_m`, and `con_koiwa*_m` demonstrate why render state must remain attached to a draw batch instead of a texture. Their 36-byte batches reuse names such as `con_wi1`, `con_wk1`, and `con_frg1` in two forms:

| Batch kind | Observed state | DATura coverage interpretation |
|---|---:|---|
| Base rock/grass surface | `blendFlags = 0` | Opaque; texture alpha is not surface coverage. |
| Detail pass over the same object | `blendFlags = 0x8000` | Alpha-layered geometry; texture and vertex alpha contribute to coverage. |

The alpha values in the `0x8000` Crag records include zero and intermediate values such as 63, 126, and 128. That is strong evidence against treating those records as a binary alpha test. Conversely, several opaque Crag base records contain non-255 vertex alpha values, so applying alpha to every use of the texture or every vertex would incorrectly punch holes in otherwise solid rock.

### What the retail framebuffer behavior does and does not prove

The DAT files prove layered geometry and per-batch state-like words; they do **not** expose a named deferred-decal system, a DBuffer, or a separate decal framebuffer. There is presently no basis for saying that the retail client renders these terrain decals through a modern auxiliary framebuffer.

The observed content is consistent with an early Direct3D fixed-function sequence using the ordinary color target and Z/depth buffer:

1. render opaque terrain and rock while writing depth;
2. render cutout vegetation/flower cards with alpha comparison and depth writes; and
3. render `0x8000` layers later with source-alpha blending, depth testing, and no depth writes.

Near-coplanar overlay geometry uses depth bias to avoid Z fighting; xim identifies the retail value as `8`. The comparison function, ordering buckets, and whether the client used additional intermediate buffers remain unknown. DATura's ordering and its renderer-specific bias tuning below are reconstructions that match the Konschtat cases.

### DATura implementation and the problems it fixed

#### Native DXT sampling and per-batch material variants

DATura keeps each FFXI DXT payload in its native block-compressed form for the normal viewer path. Its pixel shader applies the reconstructed fixed-function color equation:

```text
RGB = saturate(2 * texture.rgb * vertex.rgb)
```

For DXT3 batches that actually use authored alpha, DATura expands the sampled alpha by approximately `1.875` before combining it with the similarly reduced-range vertex alpha. This expansion is only enabled for native DXT3 textures; RGBA/DMB UI and character-creation atlases retain their existing full-range alpha.

The importer creates DATura material variants that share a single texture resource but retain a different render class. In effect, one `con_wi1` texture can be bound as an opaque base pass or as a soft-blended overlay without duplicating, converting, or editing its DAT pixels. The per-batch state chooses the variant:

| DATura class | Coverage and depth behavior |
|---|---|
| Opaque/no blend | Ignores authored texture alpha for coverage; depth test and writes enabled. |
| Hard alpha | Applies the zone cutout threshold (`0.375`); depth test and writes enabled. |
| Soft blend | Uses source alpha / inverse source alpha; depth test enabled, depth writes disabled, no second alpha-test cutoff. |

#### Flowers and grass: incorrect texture-wide alpha policy

The first Konschtat failure rendered the flower sheets as large blue, purple, and pink rectangles. A later attempt to force their alpha behavior more broadly created dark grass patches and holes in ordinary terrain. Both failures came from assigning coverage behavior to a texture globally instead of to the map-geometry batch that referenced it.

The corrected DATura rule is that `_con_hana_*` and `_kusa_*` cutout-style objects use the hard-alpha path when their batch is not an authoritative `0x8000` overlay. The `0x2000` bit selects two-sided rendering for these assets, so both sides of thin cards/sheets remain visible. The same `con_f01c` DXT3 atlas is sampled natively; its transparent texels are discarded by the hard-alpha comparison rather than blended across the entire rectangle. This produces flower patches on the terrain and upright grass cards without exposing the opaque terrain beneath as holes.

A leading `_` in the mesh name is xim's confirmed native hard-alpha classifier, with threshold `0.375`. DATura's additional family-name checks remain fallback/reconstruction rules for names not covered by that mechanism.

#### Cermet Crags: transparent batches must not be flattened

The Crags exposed two separate renderer bugs.

1. Applying the hard-alpha path to a `0x8000` Crag batch discarded its intermediate alpha and broke the layered rock detail. DATura now treats `0x8000` as authoritative for the soft-blend path, even if the object family would otherwise qualify for a cutout rule.
2. The generic mesh builder originally merged all primitives with the same object and material name. That was harmless for opaque geometry but destructive for the Crags: several distinct `0x8000` records using the same texture became one draw call, losing the authored layer boundaries needed for transparent composition.

DATura now forces each `0x8000` map batch into its own submesh. It renders opaque and hard-alpha submeshes first, gathers the remaining transparent submeshes, sorts their sampled centers far-to-near, and draws them with source-alpha blending, depth writes disabled, alpha test disabled, and only a very small negative depth bias. The reduced bias matters: an earlier stronger bias allowed grass/terrain decals to win depth tests against Crag geometry even when they were behind it.

This is why the repaired Crags can share DXT3 textures between solid rock and partial detail without their surfaces becoming globally transparent. It is also why the repair belongs in batch state preservation and render ordering rather than in a texture conversion step.

### Remaining limits of this case study

The Konschtat result validates the present reconstruction across the observed flowers, grass, terrain overlays, and Crags. The native leading-underscore threshold, alpha equation, and depth-bias value are now supported by xim, but DATura's far-to-near center sorting and renderer-specific bias conversion remain approximations. Future retail GPU capture or executable analysis should still supersede those unresolved ordering details.

## Selbina case study: sand decals, foliage, nets, and fish

Selbina is the most useful compact regression zone for the alpha and layered-terrain
path.  Its zone model is `ROM/1/43.DAT` (zone 248), and it combines opaque terrain,
soft dirt/sand overlays, hard-cutout foliage, nets, seaweed, and very small fish
cards.  These assets may share texture containers, but they do not share one alpha
policy.

### What the FFXI files establish

#### Native DXT3 is retained for the relevant texture families

The map-geometry inspection of `ROM/1/43.DAT` records the following texture families
as FFXI texture mini-header payloads tagged `DXT3`:

| Family | Observed examples | Dimensions | Observed alpha range | Typical use |
| --- | --- | --- | --- | --- |
| Terrain / sand | `sel_wl1`, `sel_wl2` | 512 x 256 | 0-136 | terrain and dirt/sand layers |
| Tree foliage | `kin_w02c`, `kin_w03c`, `kin_w04c` | 128 x 128 or 128 x 256 | 0-136 | leaf cards and foliage |
| Nets / fish | `sel_kmn1`-`sel_kmn4` | 128 x 128 through 512 x 256 | 0-136 or 119-136 | nets, fish displays, and related props |

The payloads are not pre-made `.dds` files.  They are FFXI texture sections with the
small engine-specific header described earlier in this document, followed by standard
DXT blocks.  A renderer can create an in-memory DDS wrapper for diagnostics, but that
is not a source-file conversion and must not be part of the render path.  DATura
uploads the original DXT payload to a matching D3D9 `D3DFMT_DXT3` texture.

The alpha range is also a warning against treating every non-255 alpha byte as a
universal transparency instruction.  The same source texture family can be sampled by
an opaque terrain batch and by a separately blended terrain layer.  Geometry state,
not the texture name or DXT format alone, decides whether the alpha channel participates
in coverage or blending.

#### Dirt and sand are layered geometry, not replacement terrain textures

Selbina terrain meshes include ordinary base draws and `0x8000` material-batch draws
which reuse `sel_wl1` / `sel_wl2`.  The overlay batches commonly carry vertex alpha
near 126 while their base counterparts are opaque.  In the retail scene these are the
patches of dirt or sand laid over the ground, stairs, and stone surfaces.

That explains two early, superficially conflicting results:

* Ignoring alpha made the overlays too solid and their color visibly disagree with the
  terrain below.
* Applying texture alpha to every use of `sel_wl1` / `sel_wl2` punched holes into the
  otherwise solid terrain.

The DAT establishes only the geometry layers, their per-batch flags, vertex colors,
and shared texture references.  It does not expose a modern decal-material label or a
retail render-target graph.  The safe conclusion is that the `0x8000` batches require
a distinct depth-aware overlay pass, while the ordinary batches remain opaque.

#### Selbina trees are one texture format with several geometry states

The foliage textures `kin_w02c`, `kin_w03c`, and `kin_w04c` are all DXT3, including
their alpha payload.  They are used by object families such as `_sel_w01_m`,
`_sel_w04_m`, `_sel_w05_h`, and `_sel_w08_h`.  Individual leaf-card batches differ:
some carry `0x2000` and some do not.  That bit controls whether normal back-face
culling must be disabled for that batch.

The apparent difference between "trees that handled transparency" and trees that did
not was therefore not evidence for a second foliage codec.  It was a combination of:

* hard alpha coverage being required by the leaf textures;
* the texture alpha needing correct DXT3 expansion;
* two-sided cards needing `D3DCULL_NONE` only where their batch requests it; and
* ordinary solid trunk geometry continuing through the opaque path.

When the hard-alpha test was skipped, leaf-card background pixels survived as dark or
black rectangles.  When culling was applied globally, one side of selected cards
vanished.  Both failures can occur in the same visual group while using the same DXT3
storage.

#### Nets, seaweed, and fish use different alpha roles within one prop set

The fish drying tables are deliberately economical PlayStation 2-era geometry: each
fish is largely a texture on a small flat card.  The relevant objects include
`himono02`, `_himono03`, `_himono04`, and the related table/shadow meshes.  They sample
the `sel_kmn2` / `sel_kmn3` DXT3 textures.  Their visible fish silhouettes require
hard cutout alpha; the dull rectangular background around every fish is supposed to be
discarded, not blended over the net beneath it.

The net set shows why object-wide alpha inference is unsafe:

* `_ami01`, `_ami02`, and `_ami03` contain an ordinary base batch and a separate
  `0x8000` overlay batch using the `sel_kmn3` / `sel_kmn4` material family.  The overlay
  uses a reduced vertex alpha in the inspected data.
* The larger hanging/ocean-net and seaweed families, including `_umisaku-ami` and
  `_wakame`, rely on alpha coverage for the spaces between rope strands and for the
  seaweed silhouettes.
* Some fish-related table and shadow batches have reduced vertex alpha but are not an
  instruction to turn the whole prop into a soft transparent object.

Thus an object named "net" can contain both opaque structural geometry and a soft
overlay, while a fish card uses the same broad DXT3 technology but needs an alpha test.
The DAT records this at batch granularity, not as one uniform material setting for the
entire placed object.

### What the file data does not prove about the original client

The observed Selbina batches demonstrate depth-sensitive layered geometry, but they do
not prove that the retail client used a named auxiliary framebuffer, Unreal DBuffer, or
any particular deferred-rendering design.  The compatible reconstruction is a normal
opaque depth-writing pass followed by a depth-tested, non-depth-writing overlay pass.
This produces the intended "dirt belongs to the surface below it" result without
claiming an unverified retail implementation detail.

Likewise, FFXI's DAT data does not label the fish texture as "alpha test" in a modern
API vocabulary.  That conclusion comes from the stored DXT3 alpha, the card geometry,
and the required visual result: background texels must have no coverage, while the fish
body must remain fully present.

### DATura implementation and the fixes

#### Preserve DXT blocks and decode alpha in the shader path

DATura keeps the original DXT payload and samples it through the native D3D9 DXT
texture format.  In particular, DXT3's 4-bit explicit alpha is expanded to the range
expected by the FFXI material path before it is combined with vertex alpha.  DATura
does not convert these Selbina textures to a persistent RGBA substitute.

This is essential for the fish cards.  Earlier RGBA experiments could make one class
of asset appear closer while changing the alpha/color relationship for another class.
Native DXT sampling keeps the RGB and alpha interpretation aligned for terrain, nets,
foliage, and fish.

#### Use per-batch material variants, not a global texture-alpha switch

For Selbina, DATura selects a material variant for each MapGeo batch:

| Batch role | DATura state |
| --- | --- |
| Ordinary terrain, rock, wood, and opaque prop surfaces | opaque; texture alpha does not remove coverage |
| Dirt/sand layer marked `0x8000` | source-alpha / inverse-source-alpha blend; depth test enabled; depth writes disabled; submitted as its own sortable submesh |
| Tree leaves, fish cards, net holes, and seaweed silhouettes | alpha test using the DXT3 alpha path; thresholded coverage; depth writes remain enabled |
| Batch marked `0x2000` | disables culling for that batch only; all other opaque and hard-alpha geometry uses normal back-face culling |

The exact threshold and alpha scaling are renderer compatibility choices, not a claim
that FFXI supplied those literal D3D9 state values.  They are centralized in DATura so
the zone behavior remains consistent rather than being patched per screenshot.

#### Dirt/sand decal correction

The final dirt/sand behavior came from treating `0x8000` as authoritative for that
specific batch.  The overlay uses its DXT3 alpha and vertex alpha in the soft blend
pass; the underlying terrain using the same texture family stays opaque and ignores
texture alpha for coverage.  Overlay batches are not merged into a single shared
submesh, because that loses their depth order and makes the layers visibly bleed across
terrain or acquire a different luminance from adjacent base terrain.

This removed both historical regressions: the bright/faint sand patches caused by a
mismatched alpha/color path, and the large terrain holes caused by globally enabling
alpha coverage on textures that also belong to opaque geometry.

#### Foliage and net correction

The tree names (`_sel_w...`) and cutout prop names (`ami`, `wakame`, and related
families) route to the hard-alpha path.  Their DXT3 alpha selects coverage, rather than
producing a translucent dark rectangle.  Culling remains a separate decision driven by
`0x2000`, so two-sided leaf cards render from both sides without accidentally changing
the culling of terrain or solid prop geometry.

For the nets, the hard-alpha pass makes spaces between ropes genuinely empty while
preserving the rope strands.  The `_ami*` overlay batches still go through their own
soft-overlay state when marked `0x8000`; hard-alpha classification never overrides an
authoritative overlay flag.

#### The fish-card correction

The final fish fix was not another image conversion.  It required all of the following
to agree at once:

1. Upload and sample the original `sel_kmn*` DXT3 payloads.
2. Expand and use the DXT3 alpha instead of treating the card's background as opaque
   color.
3. Classify `himono` card batches as hard alpha, so background texels fail coverage
   while the fish silhouette writes depth normally.
4. Keep the rack/net material in its own appropriate pass, instead of inheriting the
   fish card's alpha behavior.

That combination is why the final Selbina view shows fish resting directly on the net,
with no gray-green card border, while the net remains visible through the spaces around
each fish.  It also works for the hanging strings of dried fish, where the same card
construction is especially easy to see against the sky.

### Selbina regression checklist

When changing texture or render-state code, verify Selbina against all of these at
once:

* dirt/sand overlays match the color and opacity of their terrain context without
  turning base terrain transparent;
* leaf cards have no rectangular background and remain visible from the appropriate
  sides;
* fish cards have a clean silhouette with no opaque card border;
* display nets and the larger hanging/ocean net retain holes between rope strands;
* seaweed has a cutout silhouette rather than an opaque rectangle; and
* the geometry below a soft `0x8000` layer correctly occludes it when it is closer to
  the camera.

## High-poly character-creation assets

Some character-creation models use a distinct format rather than the standard chunked `0x2A` path.

### Shape records

**Confirmed in tested files.** Records contain text similar to:

```text
SHAPE: TriStrip ver.2, <tris> tris, <codes> codes, <verts> verts
```

The block provides separate float arrays for positions, normals, and UVs, followed by a mixed command/index stream. A negative command starts a strip whose vertex count is its absolute value. Certain positive multiples of three can introduce explicit triangle lists; other nonnegative values appear to be state/material selectors and remain only partially interpreted.

DATura flips the Y coordinate and Y normal component for these meshes to assemble them consistently with the in-game model path. This is a confirmed requirement of the current conversion, not proof of retail's internal coordinate convention.

### DMB texture atlases

**Partially confirmed.** Companion `DMB\0` files can contain 3- or 4-byte-per-pixel atlases. DATura searches for plausible width/height/bytes-per-pixel blocks, converts BGR(A) to RGBA, and applies asset/race-specific alpha rules.

Green-key removal, matte-black cutout detection, and per-race atlas-region masks are **DATura reconstruction heuristics**. They yield useful previews but are not a decoded specification of retail character-creation compositing.

## Effects, sky, weather, shadows, and other incomplete systems

### Effect chunks (`0x05`, `0x19`, `0x1F`, `0x21`, `0x25`)

The archived FFXI Tool analysis notes, checked against installed retail DAT samples, separate several records that DATura previously grouped together:

- `0x1F` marker-6 records are ordinary effect triangle models. Their header carries image counts and a total triangle count; each expanded vertex is 36 bytes: position `float3`, normal `float3`, RGBA bytes, and UV `float2`. The image/material ID begins at payload `+0x0E`, and the vertex stream is 16-byte aligned after the image IDs. Later marker-3 records use the same expanded triangle vertices behind a fixed `0x50`-byte extended header; DATura validates and renders both forms.
- `0x21` records are animated/card effect models. Each card contains six 24-byte vertices (position `float3`, RGBA bytes, UV `float2`). Direct records use header byte 6 as the total card count: flat records store cards contiguously at `+0x1C`, while a validated extended form stores repeated `0xA4`-byte records containing four flag bytes, 16 control/pivot bytes, and one card. Generator-produced records instead use header byte 2 as a group count. Every group starts with `{ uint16 kind = 1, uint16 cardCount }` and is followed by that many contiguous cards; zero-card groups are valid. The sum of group card counts is authoritative because the large `tam3` record has 106 groups totaling 295 cards even though header byte 6 is 39. This grouped form accounts for all 416 formerly unsupported `0x21` records, including `moon`, `hon4`, `hib1`, fire/lightning, barrier, lens-flare, and particle-sheet assets.
- `0x25` is a morphing model, not the conventional effect mesh DATura previously assumed. Its eight 16-bit header words are image count, morph count, base-position count, secondary-position count, index-block offset, triangle count, per-corner color offset, and per-corner UV offset. Base positions begin at `+0x20` with a 16-byte stride. Color, UV, and index blocks contain respectively 12, 24, and 12 bytes per triangle. Offsets are stored modulo 64 KiB; large records are unwrapped in color → UV → index order using those exact block sizes. The index block contains two sets of three 16-bit indices per triangle; DATura uses the first set to render the static base pose.
- `0x19` begins with an 8-byte header followed by 8-byte keyframe pairs. DATura exposes the pair count but does not yet assign universal semantics to both floats.
- `0x05` is the generator/controller. Its approximately `0x80`-byte header ends with four monotonically increasing section-end offsets at `+0x70`. Commands use a 4-byte prefix (`opcode`, byte payload size, two reserved bytes) followed by the payload. Known commands cover resource selection, movement and jitter, emission shapes, initial/delta rotation, scale, color, XYZ/rotation/scale/RGBA/UV curves, morph selection, child generators, probable sound, and point lights.

DATura now renders decoded `0x1F` and `0x21` static geometry and the `0x25` base pose with native per-vertex/per-corner colors and UVs. Morph interpolation, billboard/camera constraints, generator timing, child spawning, texture animation, and generator-selected blend modes still require an effect simulation runtime. Until then the preview keeps the existing soft-blend material fallback.

### Sky and celestial geometry

Named `0x2E` chunks such as cloud, star, sun, moon, lightning, and dust candidates are confirmed in zone DATs. Many are not in the ordinary placement list.

**Speculation:** a separate environment controller selects and transforms these based on time, weather, camera, and zone state. Sky meshes may be centered on or moved with the camera to avoid parallax. Repeated sun/moon/cloud chunks may represent variants for different weather/time states rather than duplicates.

DATura's fixed scale and vertical offsets are preview aids only.

### Fog

`0x2F` environment records contain ambient color, fog color, fog start/end, and sun/diffuse-light configuration. xim interpolates these values across time-of-day and weather states.

Fog is linear:

```text
factor = (far - distance) / (far - near)
```

The factor mixes fog into RGB while preserving fragment alpha.

### Shadows

The outer chunk header contains an `is_shadow`-like bit in legacy definitions, and some assets/names may represent shadow geometry.

**Reported original-client character path:** the supplied `ZoneRenderer` account describes a projected blob-shadow route using the `SYSTEM_KAGE` texture, `ShadowZBias = 15`, depth writes disabled, shadow-scale vectors, and dedicated render functions (`ZoneRenderer.cpp:1624ff`). We have not independently reproduced that trace.

**Still unknown:** how outdoor static shadows are authored, how the blob is projected onto complex receiving geometry, and what the outer chunk `is_shadow` bit controls. The blob-shadow path does not by itself decode that resource flag.

### Bump/normal mapping

Section type `0x5D` is a native `BumpMap` resource. It stores an 8-bit height field that xim converts to a normal map; the zone shader computes tangents/TBN and samples the resulting map. Native per-pixel bump mapping is therefore confirmed, although it is not necessarily used by every material.

DATura's `__flat_normal` remains a generated viewer helper and is unrelated to the native `0x5D` resource path.

### Multiple texture stages

The rendering pipeline supports multiple specialized passes. A later PC character path uses `0x8010` for a second-stage cubemap environment map, while the original PS2 display-list linker selects ordinary, environment-map, and specular passes independently. Zone bump maps add another sampled resource path. The PS2 client also contains a confirmed background light-map pass that clears an auxiliary alpha buffer, exposes framebuffer-derived data as a texture, and redraws selected background geometry. Detail-texture uses beyond these paths remain possible but unverified.

## Collision geometry is separate from visible geometry

**Confirmed.** The `0x1C` map payload includes an official collision structure distinct from render `0x2E` batches. DATura decodes:

- a zone grid;
- lists of transform-offset/geometry-offset pairs per occupied grid cell;
- per-mesh transform matrices;
- float3 vertices and normals; and
- 8-byte triangle records: `p0` and the normal index use `& 0x7FFF`, while `p1` and `p2` use `& 0x3FFF`.

This explains why helper names and visually hidden geometry should not be assumed to be the authoritative collision surface. Visual geometry can be collected as a fallback, but retail collision/navigation behavior should be based on the dedicated structure where available.

The top nibbles of all four encoded u16 values combine into a 16-bit material/terrain-type word. DATura preserves the raw fields for diagnostics. The collision grid also strengthens the hypothesis that visual culling/streaming may use spatial partitions, though it does not prove they share a format.

## Reconstructed end-to-end render recipe

The following recipe summarizes what a practical viewer can do today. Steps marked “reconstruction” should remain configurable.

1. Walk 16-byte chunk headers and validate sizes.
2. Conditionally decode `0x1C`, `0x20`, `0x29`, `0x2A`, `0x2B`, and `0x2E` payloads.
3. Decode all `0x20` textures into named resources:
   - expand palette/indexed images with vertical reversal;
   - accept confirmed DXT1/3 and defensively support unverified DXT5;
   - prefer the DXT half of `0x81` for client-like preview, but expose the palette half for inspection.
4. For character geometry:
   - parse the `0x2A` draw list;
   - bind texture names on `0x8000`;
   - preserve `0x8010` data and reproduce its confirmed cubemap enable/intensity state;
   - emit lists/strips and split UV seams as needed;
   - perform up-to-two-weight skinning;
   - emit the mirrored pass and reverse winding when requested.
5. For zones:
   - parse `0x1C` placements;
   - hash `0x2E` objects by their 16-byte name;
   - build `T * Rz * Ry * Rx * S` transforms and apply placement LOD/draw-distance thresholds;
   - parse nested bounded batches;
   - decode 36- and 48-byte vertices and 16-bit list/strip indices.
6. Preserve each draw batch's raw state fields.
7. Apply environment lighting and reconstruct `rgb = 2 * lit.rgb * texture.rgb`, `a = 4 * vertexAlpha * textureAlpha`.
8. Classify leading-underscore zone meshes as hard alpha, preserve known flags, and use broader empirical name rules only as fallback.
9. Render opaque/cutout surfaces with depth writes, then blended surfaces without depth writes.
10. Keep culling, alpha expansion, alpha threshold, depth bias, and transparent sorting configurable until retail behavior is better established.

## Common mistakes when implementing an FFXI renderer

1. **Treating Noesis fields as native material data.** Most modern fields are part of the viewer abstraction.
2. **Calling generated suffixes native names.** `_explicitsoftblend`, `_explicitnoblend`, and similar suffixes are DATura/Noesis transport conventions.
3. **Ignoring vertex color.** Zone textures alone often look wrong; the packed color materially affects the result.
4. **Applying generic modern lighting.** Arbitrary PBR or point lighting can double-light baked content.
5. **Using texture alpha on every surface.** Opaque surfaces may contain irrelevant alpha; enabling blend globally produces halos and sorting failures.
6. **Treating all alpha as blending.** Vegetation and card-like geometry often needs an alpha test/cutout path.
7. **Merging transparent batches.** This destroys authored layer boundaries and makes sorting less controllable.
8. **Assuming one UV per base character vertex.** `0x2A` UVs live on primitive corners.
9. **Assuming conventional shared-position skinning.** Two-weight records store separate per-influence position/normal contributions.
10. **Forgetting winding changes.** Mirrored character passes and negative-scale zone placements reverse handedness.
11. **Rendering every unreferenced map mesh at identity.** Many are environment, helper, collision, alternate LOD, or state-selected resources.
12. **Using visible meshes as authoritative collision.** The zone map has a dedicated collision structure.
13. **Claiming mip support because the sampler enables mip filtering.** Source levels and uploaded levels must be independently verified.
14. **Over-trusting parser cleanup heuristics.** DATura's 80-unit edge rejection and name-based alpha lists are guards/reconstructions, not file-format rules.

## Unknowns and prioritized research plan

### Highest priority

1. **Finish character `0x8010` implementation.** The reflection enable, intensity consumer, cubemap stage, and blend operations are known; reproduce the cube-resource construction, exact texture transform/arguments, and remaining state fields in DATura.
2. **Finish zone state words.** The `VerticeCountAndFlags` count/transparent/culling mapping is confirmed; determine runtime flags `0x4000`/`0x1000`, `flags2`, and object/sub/super interactions.
3. **Validate fragment-equation exceptions.** The xim equation is known; capture retail draws to identify state-specific or platform-specific departures, blend factors, and alpha comparisons.
4. **Determine native mip layout.** Compare payload sizes with exact DXT/palette level sizes and inspect unknown header fields.
5. **Decode the remaining placement-table tail.** LOD thresholds, draw distance, culling/environment links, and point-light indices are known; look for remaining spatial cells, visibility sets, and schedule/hierarchy data.

### Medium priority

6. Complete `0x2A` LOD stream parsing and identify selection rules.
7. Implement and validate 48-byte vertex displacement/foliage sway in DATura.
8. Complete animation translation/scale semantics and matrix composition order.
9. Integrate decoded `0x2F` environment lighting and fog with sky/weather selection.
10. Decode `0x21`/`0x25` effect systems, including billboards and texture/color animation.
11. Implement and validate the known `SYSTEM_KAGE` blob-shadow path; separately decode static/resource shadow flags.
12. Implement native `0x5D` bump maps and the confirmed character cubemap path; test for additional multi-texture operations.

### Suggested experimental discipline

For each hypothesis:

1. Record DAT path, chunk offset/type/name, zone/object/material name, and exact bytes.
2. Compare multiple assets where only one suspected field changes.
3. Capture retail output or draw state if possible.
4. Change one reconstruction state at a time.
5. Record counterexamples, not only matches.
6. Promote a claim from speculation only when it predicts new samples.

## Compact confidence ledger

### Confirmed

- 16-byte chunk stream with typed, aligned chunks.
- Core texture, skeleton, geometry, animation, placement, and map-geometry chunk identities.
- 16-byte resource names linking draw data to textures and placements to map meshes.
- Paletted, raw 32-bit BGRA, DXT1, DXT3, and palette+DXT texture payloads; DXT5 parsing is defensive and retail presence unverified.
- Palette/index image expansion and bottom-up row order in the decoded path.
- Character triangle-list and triangle-strip commands with per-corner UVs.
- One- and two-weight character vertex records, optional bone indirection, and mirrored pass data.
- Zone reusable mesh placement using `T * Rz * Ry * Rx * S`, per-placement LOD thresholds, draw distance, and environment/light links.
- Zone 36-/48-byte vertex records with positions, normals, packed color, UVs, and displacement vectors for the 48-byte blended form.
- Zone triangle lists/strips, 16-bit indices, and nested bounded draw groups.
- Dedicated grid-organized collision geometry separate from visible map geometry.
- `0x2F` environment lighting/fog, `0x5D` bump maps, leading-underscore cutout classification, and zone fragment/alpha equations.
- Character `0x8010` second-stage cubemap reflections and untextured `0x2A` commands `0x0043`/`0x4353`.
- High-poly character-creation shapes and DMB atlases form a distinct path.

### Strongly supported

- Direct3D 8-era fixed-function rendering heritage.
- The decoded environment/lighting path as reproduced by xim, pending original-client/platform cross-checks.
- Reduced authored alpha range in at least some DXT3 assets.
- `0x2000` as a culling-related zone bit and `0x8000` as an alpha-layer bit.
- Opaque, alpha-test, and source-alpha-blended material classes.
- `_h`/`_m`/`_l` object suffixes as authored detail levels.
- Bounds and hidden zone data supporting runtime culling/partitioning.

### Reconstruction/speculation

- Additional alpha classifiers and thresholds beyond the confirmed zone `_`/`0.375`, 2003 PS2 character approximately `0.375`, and later-PC character `69/255` paths.
- Exact DXT3 expansion factor across all asset classes.
- DATura's broader name-family cutout classification beyond the confirmed leading-underscore rule.
- Transparent far-to-near sorting and DATura's conversion of the confirmed native depth-bias value.
- Sky scale/height placement.
- DMB green/matte-black and race-region alpha rules.
- DATura's shiny-material approximation using flat normal/specular textures instead of the confirmed cubemap path.
- Effect-mesh rendering details.

## Conclusion

The known FFXI rendering architecture is a compact display-list and instancing system built around named textures, per-draw primitive streams, vertex colors, simple UV mapping, two-weight skinning, mirrored geometry, and reusable zone meshes. It is much closer to a PS2/Direct3D 8 fixed-function content pipeline than to a modern shader/material graph.

We can reconstruct ordinary characters, equipment, zone surfaces, textures, alpha layers, environment lighting/fog, and collision with useful fidelity. The remaining uncertainty is concentrated not in “where are the triangles?” but in the unmapped portions of draw state, visibility/schedule selection, transparent ordering, full environment control, effects, and shadows. Those areas should remain explicitly labeled and configurable until confirmed by stronger evidence.
