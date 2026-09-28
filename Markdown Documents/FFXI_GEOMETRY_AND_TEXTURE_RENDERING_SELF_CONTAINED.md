# FFXI Geometry and Texture Rendering

## Purpose

This document describes how to locate, decode, and render Final Fantasy XI geometry and textures from DAT resources. It is intended to stand alone: readers need no prior knowledge of a particular importer, viewer, renderer, or source tree.

The focus is visible 3D content:

- DAT chunk framing and resource links;
- texture storage and alpha conventions;
- character and equipment geometry;
- zone placements and reusable zone meshes;
- vertex colors, lighting, fog, cutouts, blending, and culling;
- native bump maps and character reflections; and
- collision geometry.

UI resources, sound, full animation semantics, particles, shadows, and every platform-specific rendering difference are outside the completed scope.

## Confidence vocabulary

- **Confirmed** means the binary layout or behavior is supported by repeated retail DAT observations and compatible decoding behavior.
- **Strongly supported** means the interpretation predicts observed content but still lacks complete coverage.
- **Unknown** means the available evidence does not support a stable interpretation.

All offsets in this document are byte offsets unless explicitly described as 16-bit-word offsets. Multi-byte values are little-endian. Structures are packed unless stated otherwise.

## Architecture overview

FFXI uses a chunk-oriented resource pipeline suited to early fixed-function graphics hardware. A DAT can contain directories, textures, skeletons, animation, character geometry, reusable zone geometry, placement records, environment state, effects, and collision data.

The visible scene is assembled as follows:

1. Walk the DAT's aligned chunk stream.
2. Decode each chunk according to its 7-bit type.
3. Register textures and reusable geometry by their fixed 16-byte names.
4. Interpret character geometry as a compact draw-command stream.
5. Instantiate zone meshes using placement records.
6. Preserve render state per draw batch rather than assigning one policy per texture.
7. Apply environment lighting, vertex color, texture modulation, alpha classification, and fog.

There is no modern physically based material graph. FFXI instead combines named textures, vertex attributes, compact state words, fixed-function-style operations, native bump maps in selected zones, and a second-stage cubemap reflection path for selected character materials.

## DAT chunk stream

### Common 16-byte header

Every ordinary resource chunk begins with a 16-byte header:

```text
offset  size  meaning
0x00    4     short identifier or name
0x04    4     packed information word
0x08    8     type-dependent or currently unassigned header data
0x10          payload begins
```

Decode the packed information word as:

```text
type         = info & 0x7F
size         = (info >> 3) & 0xFFFFF0
is_shadow    = (info >> 26) & 1
is_extracted = (info >> 27) & 1
version      = (info >> 28) & 7
is_virtual   = (info >> 31) & 1
```

`size` includes the complete aligned chunk span. Advance by this value; do not scan for byte signatures to find the next chunk. Reject sizes smaller than the common header, sizes that exceed the containing DAT region, and arithmetic that overflows.

Types `0x01` and `0x00` commonly act as directory-open and directory-close markers. They provide hierarchy but are not visible geometry.

### Resource types relevant to rendering

| Type | Resource |
|---:|---|
| `0x05` | Effect generator command stream |
| `0x19` | Keyframe float pairs |
| `0x1C` | Zone placement, environment links, and collision |
| `0x1F` | Ordinary effect triangle model |
| `0x20` | Texture |
| `0x21` | Animated or card-based effect mesh |
| `0x25` | Weighted or morphing effect mesh |
| `0x29` | Skeleton |
| `0x2A` | Character and equipment geometry |
| `0x2B` | Skeletal animation |
| `0x2E` | Reusable zone geometry |
| `0x2F` | Environment, lighting, and fog |
| `0x3D` | Sound resource pointer |
| `0x54` | Weapon trace |
| `0x5D` | Bump-map height field |
| `0x5E` | Blur resource |

Other known resource families include tables, routes, interaction records, point lists, paths, spell and ability lists, and UI groups. They are not required for the ordinary geometry pipeline described here.

### Obfuscation

Some later chunk versions obfuscate payload data. Zone placement (`0x1C`) and zone geometry (`0x2E`) are known to use version-dependent decoding based on a 256-byte key table, rolling XOR decisions, and, for some mesh-family records, a secondary block-swap pass. Zone placement records additionally XOR each 16-byte object identifier with `0x55` after the main decoding pass.

A decoder should select this behavior from the chunk version and type. Applying it unconditionally will corrupt ordinary unencrypted resources.

### Names are links

Textures, geometry batches, and placements commonly use fixed 16-byte names. Treat these as resource identifiers, not display-only labels:

- a character draw command selects a texture/material name;
- a zone leaf batch selects a texture/material name; and
- a zone placement names a reusable zone mesh.

Names may be NUL-terminated inside the 16-byte field. Compare normalized bytes carefully and preserve the original bytes for diagnostics.

## Texture resources (`0x20`)

### Common header

The common packed texture header begins:

```text
offset  size  meaning
0x00    1     texture type
0x01    16    resource name
0x11    4     version, commonly 40
0x15    4     width
0x19    4     height
0x1D    2     unknown
0x1F    2     bitCount
0x21    20    remaining format/state fields
0x35    4     palette-entry-depth-like field
```

The version is at packed offset `0x11`, not an aligned offset. Validate width, height, multiplication, and payload length before allocating or decoding.

### Texture type byte

| Value | Payload |
|---:|---|
| `0x01` | 256-entry palette followed by 8-bit pixel indices |
| `0x81` | Palette/indexed image followed by a block-compressed copy |
| `0x91` | Palette/indexed image, or raw 32-bit BGRA when `bitCount == 32` |
| `0xA1` | Block-compressed image with an FFXI mini-header |
| `0xB1` | Leading 32-bit value followed by palette/indexed data |

The complete semantic distinction between `0x01` and non-32-bit `0x91` remains unknown. The leading word in `0xB1` is also not fully interpreted.

### Paletted images

A common paletted payload contains:

```text
paletteBytes = 256 * (paletteEntryBits / 8)
indexBytes   = width * height
```

Observed 32-bit palette entries are BGRA. Observed 16-bit entries can be decoded as one-bit-alpha, five-bit color channels. Pixel rows are stored bottom-up relative to the usual top-down image convention:

```text
sourceIndex = (height - y - 1) * width + x
```

When a `0x91` texture has `bitCount == 32`, skip palette handling and read `width * height * 4` bytes of raw BGRA pixels.

### Block-compressed images

FFXI block-compressed textures are not standalone DDS files. Their payload begins with a proprietary 12-byte mini-header followed by native blocks. The four-byte codec tag appears byte-reversed in file order on little-endian systems; for example, file bytes `3TXD` represent the `DXT3` tag.

Confirmed retail payloads include:

- DXT1: 8 bytes per 4x4 block, with possible one-bit transparency;
- DXT3: 16 bytes per 4x4 block, with explicit 4-bit alpha.

DXT5 decoding may be implemented defensively, but retail `5TXD` content is not presently confirmed. Do not state that FFXI uses DXT5 merely because a decoder accepts it.

For each mip level, compute block dimensions with a minimum of one block:

```text
blocksWide = max(1, (width  + 3) / 4)
blocksHigh = max(1, (height + 3) / 4)
levelBytes = blocksWide * blocksHigh * bytesPerBlock
```

Do not infer a mip chain only from sampler behavior. Confirm that additional levels actually exist in the payload.

### Combination textures (`0x81`)

A combination texture stores two representations in sequence:

```text
256-entry palette
width * height index bytes
12-byte compressed mini-header
compressed blocks
```

Both images should be available to inspection tools. A renderer should choose one representation consistently and must not accidentally interpret the second representation as trailing palette data.

### Alpha convention

FFXI commonly treats `0x80` as the normalized value `1.0` for both vertex alpha and texture alpha. The resulting fragment alpha equation is:

```text
alpha = saturate(4 * vertexAlpha * textureAlpha)
```

When DXT3 alpha is viewed as a 4-bit value, the texture-side scale appears as `15/8`, or `1.875`. This is the quantized form of the same multiply-by-two convention, not a separate DXT3-specific artistic correction.

Do not apply this rule blindly to every RGBA resource. Some atlases and UI-related resources use the full conventional alpha range.

## Character and equipment geometry (`0x2A`)

### Header and regions

The common geometry header identifies:

```text
offset       meaning
0x00         version and secondary byte
0x02         type/flags; bit 7 enables a bone-reference table
0x04         mirrored-pass control
0x06         primary draw-stream offset in 16-bit words
0x0A         primary draw-stream size in 16-bit words
0x0C         optional bone-reference table offset
0x10         bone-reference count
0x12         weighted-count table offset
0x16         weight-count/limit field
0x18         packed bone descriptors and mirror axis
0x1C         descriptor count/size
0x1E         vertex-data offset
0x22         vertex-data size/count
0x24..0x32   two additional polygon or LOD regions
0x34..0x3F   unknown fields
```

Offsets in this structure frequently use 16-bit-word units. Convert only after validating against the chunk boundary.

### Draw-command stream

Character geometry behaves like a compact display list:

| Command | Meaning |
|---:|---|
| `0x8000` | Select a 16-byte material/texture name |
| `0x8010` | Set native draw state |
| `0x0054` | Textured triangle list |
| `0x5453` | Textured triangle strip |
| `0x0043` | Untextured triangle list |
| `0x4353` | Single-color untextured triangle strip |
| `0xFFFF` | End of stream |

For `0x0043`, each triangle occupies 10 bytes: three 16-bit vertex indices and one BGRA color. Preserve command order because material and state changes are interleaved with primitive emission.

### UV ownership

UVs belong to primitive corners, not exclusively to base vertices:

- each triangle-list record stores three indices and three `float2` UVs;
- a strip begins with three complete corners, then appends one index and one `float2` UV per corner.

An exported indexed mesh must split a base position whenever the same native vertex is used with different UVs. Exported vertex counts therefore need not match native position-record counts.

### Vertex layouts and skinning

Common geometry supports rigid and two-weight vertices.

One-weight vertex with normals:

```text
float3 position
float3 normal
```

Two-weight vertex with normals:

```text
x0, x1, y0, y1, z0, z1
w0, w1
nx0, nx1, ny0, ny1, nz0, nz1
```

The two influences store separate position and normal contributions rather than a single shared bind-space position. Each packed influence descriptor contains a 7-bit normal-pass bone index, a 7-bit mirrored-pass bone index, and a 2-bit mirror-axis selector. An optional reference table remaps local bone indices to skeleton indices.

For a two-weight position, transform each stored contribution by its corresponding bone and combine the results using the stored weights. Transform and combine normal contributions without translation, then normalize.

The low seven bits of the type field select a vertex class. Value zero commonly carries normals; cloth-like classes may omit them. Unknown class values must be bounded and rejected rather than guessed from payload size alone.

### Mirrored pass

When the mirror field is nonzero, emit a second pass using the mirrored bone indices. Apply the encoded X, Y, or Z reflection and reverse triangle winding. This allows approximately symmetric body or equipment parts to share source geometry while binding to opposite-side bones.

### Native reflection state (`0x8010`)

The draw-state record includes a confirmed character reflection path:

- a float equal to `1.0` at state-data offset `+0x10` enables texture stage 1;
- the float at `+0x24`, multiplied by `0.5`, becomes reflection intensity through texture-factor alpha;
- camera-space normals generate reflection coordinates through a texture transform;
- the stage combines reflection using modulated alpha plus added color;
- alpha is also added; and
- a cube texture is used as the secondary image.

Retail DATs contain DXT3 textures with `cubemap`-style names. Construct a cube resource from their faces; absent faces use neutral data with alpha `0x80`. This is a true second-stage environment reflection, not a flat specular or normal-map substitute.

The remaining `0x8010` fields are not all named. Preserve their raw bytes and state boundaries.

### Character LOD

The header includes two additional polygon regions beyond the primary draw stream. These are strong evidence for authored lower-detail geometry. Exact selection thresholds and complete stream semantics remain incompletely documented; do not concatenate all regions into one mesh.

## Zone placement (`0x1C`)

### Reusable geometry model

Zones separate instances from geometry:

- `0x2E` resources define named reusable meshes;
- `0x1C` records instantiate those names with transforms and runtime links.

This allows terrain tiles, roads, buildings, props, and vegetation to repeat without duplicating vertices.

### Common 96-byte placement

```text
char[16] name
float3   translation
float3   rotation
float3   scale
float4   effectDatId, highDefThreshold, midDefThreshold, drawDistance
int32[8] flags and runtime links
```

Compose the transform as:

```text
M = T * Rz * Ry * Rx * S
```

Angles are radians. If the scale determinant is negative, reverse the final triangle winding.

The integer region contains four flag bytes, a culling-table link, an environment-record link, and point-light indices. One known behavior is `flags1 & 0x2`, which skips an object during decal rendering. Preserve all unknown bits.

The two thresholds and draw distance select authored `_h`, `_m`, and `_l` geometry variants. In the original PS2 client:

```text
if drawDistance != 0 and distance >= drawDistance: hidden
else if low exists and distance > midDefThreshold: low
else if medium exists and distance > highDefThreshold: medium
else: high
```

Names are grouped by removing a terminal `h`, `m`, or `l`. If high detail is absent, resource mapping falls back to medium and then low. A separate grid-chip path selects high detail when squared grid distance is below `5`, otherwise medium when available.

Data following the placement array includes additional lookup and spatial information. Some visibility, schedule, and hierarchy semantics remain unknown.

## Zone geometry (`0x2E`)

### Resource header and hierarchy

A zone mesh begins with:

```text
4 bytes    version/header data
uint32     unknown
char[8]    tag or short name
char[16]   reusable object name
```

Nested super-groups and sub-groups follow. A common group header contains:

```text
int32   child or segment count
float6  axis-aligned bounds
int32   configuration/flags
```

Treat every child range as bounded by its containing chunk or group. The bounds support culling and diagnostics; malformed counts must not be allowed to escape the parent range.

### Leaf draw batch

```text
char[16] material/texture name
uint32   vertexCountAndFlags
vertices vertexCount * selectedStride
uint16   indexCount
uint16   secondaryFlags
uint16[] indices
padding  to a 4-byte boundary
```

The packed vertex/count word maps as:

| Bits | Runtime meaning |
|---:|---|
| `0x0FFFFFFF` | Vertex count |
| `0x80000000` | Transparent or soft-blended batch |
| `0x40000000` | Additional unresolved state |
| `0x20000000` | Disable back-face culling |
| `0x10000000` | Additional unresolved state |

Primitive mode and vertex stride are controlled by independent bits in the geometry configuration byte:

| Bit | Meaning when set |
|---:|---|
| 0 | Triangle strip; clear means triangle list |
| 1 | Vertex blending enabled; use 48-byte vertices |

Do not infer 48-byte vertices from triangle-list mode, or 36-byte vertices from strip mode. All four combinations are structurally possible.

### Vertex layouts

Ordinary 36-byte vertex:

```text
0x00 float3 position
0x0C float3 normal
0x18 u8[4] color/alpha
0x1C float2 UV
```

Vertex-blended 48-byte vertex:

```text
0x00 float3 base position
0x0C float3 displacement
0x18 float3 normal
0x24 u8[4] color/alpha
0x28 float2 UV
```

The second vector is a displacement, not another absolute position:

```text
position = basePosition + positionBlendWeight * displacement
```

Vegetation commonly stores zero displacement at anchored roots and nonzero displacement at tips, allowing wind or sway animation.

### Lists, strips, and winding

For triangle lists, consume indices in groups of three. For strips, slide over triples, exchange the second and third corner on odd triangles, and skip degenerate triples. Apply any placement-induced handedness reversal after primitive expansion.

All observed zone indices are 16-bit. Validate each index against the current leaf batch's vertex count.

### LOD and unplaced meshes

Names ending in `_h`, `_m`, and `_l` commonly represent high-, medium-, and low-detail variants. Placement thresholds select among these resources.

Not every `0x2E` resource is directly referenced by an ordinary placement. Unplaced meshes can include sky, clouds, stars, sun/moon geometry, weather effects, helpers, alternate detail levels, or state-selected resources. Do not render every unreferenced mesh at the identity transform.

## Materials and fragment evaluation

### Material identity

The 16-byte name selected by a draw command or zone batch is the primary native material link. A texture with the same name supplies image data, while draw-state fields determine culling, blending, coverage, reflection, and other behavior.

The same texture may be used by both opaque and transparent batches. Never assign alpha behavior globally from the texture name or codec alone.

### Zone lighting

Zone lighting uses:

- ambient color;
- two directional diffuse lights;
- selected point lights;
- the vertex normal;
- packed vertex color; and
- the sampled texture.

A useful expression of the known zone equation is:

```text
lit.rgb = vertex.rgb * (ambient + directional0 + directional1 + pointLights)
rgb     = saturate(2 * lit.rgb * texture.rgb)
alpha   = saturate(4 * vertexAlpha * textureAlpha)
```

Directional and point-light terms include their normal-dependent diffuse response. Keep intermediate values in sufficient precision and saturate at the same stage as the target rendering path.

Texture and vertex color still contain substantial authored shading. Adding unrelated lights will over-light the scene.

### Three coverage classes

An FFXI renderer needs at least three distinct classes:

1. **Opaque:** depth test and writes enabled; texture alpha does not remove coverage.
2. **Hard alpha:** alpha comparison plus depth test and writes; used for vegetation, nets, hair-like cards, and other silhouettes.
3. **Soft blend:** source-alpha blending, normally with depth writes disabled; used for overlays, translucent detail, clouds, effects, and decals.

Classify per draw batch. A texture can legitimately appear in more than one class.

### Hard-alpha classification

For zone geometry, a mesh name beginning with `_` selects the hard-alpha path. The confirmed threshold is:

```text
alpha >= 0.375
```

Character thresholds differ by client version. The original PS2 character display-list path uses a native reference of `0x30`, equivalent to approximately `0.375` after conversion from FFXI's half-range alpha convention. A later PC path has been observed using:

```text
alpha >= 69 / 255  // approximately 0.27
```

These rules do not override an explicit transparent/overlay batch state. Broader asset-family naming rules may be useful as compatibility fallbacks, but they should remain separate from the confirmed leading-underscore behavior. Select the character threshold according to the client behavior being reproduced.

### Soft blending and depth bias

A practical soft-blend state is:

```text
source blend      = source alpha
destination blend = inverse source alpha
depth test         = enabled
depth writes       = disabled
alpha test         = disabled
depth bias         = 8 in the native state convention
```

The bias separates near-coplanar detail layers from base terrain. Translate the native value carefully when using a different depth representation or graphics interface.

Preserve each transparent leaf batch as an independent draw unit. Merging records that share a texture destroys authored layer boundaries. When the native ordering is unavailable, sorting transparent batches from far to near is a reasonable approximation, but it is not a universal statement of FFXI's ordering algorithm.

### Culling

Zone flag `0x2000` disables culling for that batch. This is important for thin vegetation and cards that must be visible from both sides. Other geometry should retain its intended back-face culling.

Mirrored character passes and zone placements with negative scale reverse handedness and require winding correction independently of the culling flag.

## Konschtat Highlands flower sheets

The ground flowers in Konschtat Highlands are a useful reference case because several individually reasonable rendering choices produce conspicuous colored rectangles instead of flowers.

The relevant zone resources establish the following:

- flower meshes use names in the `_con_hana_*` family;
- the leading `_` selects the zone hard-alpha path;
- the meshes sample regions of the `con_f01c` 256x256 DXT3 atlas;
- observed flower batches carry the `0x2000` no-cull flag;
- those batches do not carry the `0x8000` soft-transparency flag;
- the visible geometry consists of broad, ground-parallel sheets; and
- both 36-byte and vertex-blended 48-byte forms occur, independently of list or strip topology.

Render these flowers as hard cutouts, not translucent overlays:

```text
texture              = con_f01c DXT3 data
alpha equation       = saturate(4 * vertexAlpha * textureAlpha)
alpha comparison     = pass when alpha >= 0.375
source blending      = disabled
depth test           = enabled
depth writes         = enabled
back-face culling    = disabled when batch flag 0x2000 is set
```

For a 48-byte vertex, apply the displacement-vector blend before transforming the vertex. This supports foliage motion but does not select the alpha mode.

The following approaches are incorrect:

- Rendering the entire DXT3 rectangle as opaque leaves blue, purple, pink, or dark atlas-background rectangles on the ground.
- Treating DXT3 as automatically soft-blended produces halos, sorting problems, and incorrect interaction with the terrain.
- Assigning one alpha policy globally to `con_f01c` is unsafe because render behavior belongs to the referencing draw batch.
- Using `0x2000` as the transparency selector is incorrect; it controls two-sided rendering by disabling culling.
- Using the 48-byte vertex layout as the cutout selector is incorrect; vertex blending and alpha classification are independent.
- Disabling depth writes makes overlapping flower sheets and nearby vegetation sort unreliably.

The nearby `_kusa_*` resources are upright grass cards rather than ground-parallel flower sheets, but they use the same essential hard-alpha and two-sided rendering principles when their batch state requests them.

## Konschtat Highlands Cermet Crags

The Cermet Crags are a critical layered-geometry case. Their appearance depends on preserving multiple draw batches that reuse the same texture names but require different coverage and depth behavior.

### Relevant resources

Crag objects include:

- `con_ciwa_m`;
- `con_oiwa_m`; and
- the `con_koiwa*_m` family.

Their 36-byte zone batches reuse DXT3 textures such as `con_wi1`, `con_wk1`, `con_frg1`, and `con_fsg1`. A texture name does not identify one universal material class. The same image can appear in both a solid base batch and a partially transparent detail batch.

### Two batch roles

| Batch role | Observed state | Required behavior |
|---|---:|---|
| Base rock or grass surface | No `0x8000` flag | Opaque; ignore texture and vertex alpha for coverage; enable depth writes |
| Detail layer over the surface | `0x8000` | Soft source-alpha blend; use texture and vertex alpha; depth test enabled; depth writes disabled |

The `0x8000` records contain zero alpha and intermediate values including 63, 126, and 128. Those values must remain continuous. Applying a binary alpha test destroys partially visible rock and grass detail.

Some opaque base records also contain vertex alpha below 255. Therefore, non-opaque vertex alpha does not independently select transparency. The `0x8000` batch state is authoritative.

### Required draw sequence

Render the Crags as part of the zone-wide pass sequence:

1. Draw opaque terrain and Crag base surfaces with depth writes enabled.
2. Draw hard-cutout flowers and vegetation with alpha comparison and depth writes enabled.
3. Draw each `0x8000` Crag detail batch with depth testing enabled, depth writes disabled, alpha testing disabled, and source-alpha blending.

For a Crag detail layer, use:

```text
rgb equation         = saturate(2 * lit.rgb * texture.rgb)
alpha equation       = saturate(4 * vertexAlpha * textureAlpha)
source blend         = source alpha
destination blend    = inverse source alpha
alpha comparison     = disabled
depth test           = enabled
depth writes         = disabled
depth bias           = 8 in the native state convention
```

Translate the native depth-bias value appropriately for the chosen depth representation. Excessive bias can pull a nominally rearward grass or terrain layer in front of the Crag even when the depth ordering says it is behind.

### Preserve every layer boundary

Each `0x8000` leaf batch must remain an independent draw unit, even when several batches share all of the following:

- placed object;
- texture name;
- vertex layout; and
- apparent material family.

Combining such records into one mesh or draw call erases authored layer boundaries. That prevents meaningful transparent ordering and can blend detail across surfaces that were intended to remain separate.

Preserving boundaries does not require duplicating or modifying texture pixels. One decoded DXT3 texture can be referenced by multiple draw batches whose state differs.

### Ordering

Opaque and hard-cutout geometry must complete before Crag soft layers are submitted. When no complete native transparent ordering is available, sort the preserved soft batches approximately from far to near using a representative transformed center.

Center sorting is a compatibility strategy, not a proven universal ordering rule. It can fail for intersecting or very large layers, which is another reason not to merge independently authored records.

### Incorrect approaches

- **Texture-wide blending:** Making every use of `con_wi1`, `con_wk1`, `con_frg1`, or `con_fsg1` transparent weakens or removes solid rock surfaces.
- **Texture-wide alpha testing:** Applying the hard-alpha path to a `0x8000` detail layer discards intermediate alpha and breaks the layered surface.
- **Vertex-alpha classification:** Treating every non-255 vertex alpha as transparency punches holes in opaque base geometry.
- **Merging by object and texture:** Flattening same-name batches removes the boundaries required for ordering and independent state.
- **Writing depth in the soft pass:** The first detail layer can incorrectly block later layers.
- **Disabling depth testing:** Detail appears through unrelated foreground rock or terrain.
- **Applying alpha comparison and blending together:** Low-alpha detail is clipped before it can blend smoothly.
- **Using excessive depth bias:** Layers that should remain behind a Crag can win the depth test and appear on top.
- **Editing or converting the texture to fix composition:** The pixels are shared correctly; the failure is in per-batch state and draw ordering.

### Cermet Crag regression checklist

Verify all of the following after changing geometry batching, alpha, sorting, or depth handling:

- solid Crag rock remains fully opaque;
- grass and rock detail retain intermediate translucency rather than becoming binary cutouts;
- zero-alpha detail contributes nothing;
- repeated uses of the same texture can render as opaque base and soft overlay in one object;
- independently authored `0x8000` batches remain separate;
- near-side Crag geometry occludes detail that lies behind it;
- detail layers do not write depth and incorrectly block one another;
- no rearward terrain or grass layer is pulled in front by excessive bias;
- flowers and other hard-cutout vegetation remain in their own earlier depth-writing pass; and
- changing Crag behavior does not globally alter other objects using the same texture or DXT3 format.

## Selbina terrain, foliage, nets, seaweed, and fish

Selbina is a valuable whole-zone regression case because several unrelated surface types share DXT3 storage while requiring different render states. Correct results depend on classifying each draw batch, not each texture or placed object.

### Relevant texture families

The zone's native texture resources include:

| Family | Observed examples | Dimensions | Observed alpha range | Typical use |
|---|---|---:|---:|---|
| Terrain and sand | `sel_wl1`, `sel_wl2` | 512x256 | 0-136 | Base terrain plus dirt/sand layers |
| Tree foliage | `kin_w02c`, `kin_w03c`, `kin_w04c` | 128x128 or 128x256 | 0-136 | Leaf cards |
| Nets and fish | `sel_kmn1` through `sel_kmn4` | 128x128 through 512x256 | 0-136 or 119-136 | Nets, drying fish, and related props |

These are FFXI texture sections with the native 12-byte mini-header and DXT3 blocks. Decode their explicit alpha using the normal FFXI alpha convention. The reduced stored alpha range does not mean every batch using these images is transparent.

### Dirt and sand layers

Selbina terrain contains ordinary base batches and separate `0x8000` overlay batches that can reuse `sel_wl1` or `sel_wl2`. Overlay vertices commonly have alpha near 126. These batches produce dirt or sand patches over ground, stairs, and stone.

Render the two uses separately:

| Batch | Required behavior |
|---|---|
| Ordinary base terrain | Opaque, depth-writing; ignore texture alpha for coverage |
| `0x8000` dirt/sand layer | Source-alpha blend, depth test enabled, depth writes disabled, native depth bias applied |

Keep each overlay batch as a separate draw unit. Do not merge all records that share `sel_wl1` or `sel_wl2`, because doing so loses authored layer boundaries and makes ordering and depth interaction unreliable.

Two opposite failures identify incorrect classification:

- Ignoring alpha on the `0x8000` layer makes sand and dirt too solid and changes their apparent color.
- Applying the same alpha policy to base terrain punches holes through otherwise solid ground.

The correct result is a depth-aware layer attached visually to the opaque surface beneath it. This does not require assuming any particular off-screen buffer or deferred-rendering design.

### Tree foliage

The DXT3 foliage textures `kin_w02c`, `kin_w03c`, and `kin_w04c` appear on tree families including `_sel_w01_m`, `_sel_w04_m`, `_sel_w05_h`, and `_sel_w08_h`.

Tree parts must be classified per batch:

- leading-underscore leaf meshes use hard-alpha coverage;
- solid trunks and branches remain opaque;
- leaf batches carrying `0x2000` disable culling and render from both sides;
- leaf batches without `0x2000` retain normal back-face culling; and
- DXT3 storage alone does not select either hard alpha or two-sided rendering.

Without the hard-alpha test, background texels survive as dark rectangular cards. If culling is disabled globally, unrelated solid geometry becomes incorrectly two-sided. If culling is enabled globally, the back side of selected leaf cards disappears.

### Nets and overlays

Net objects can contain multiple material roles inside one placed object. `_ami01`, `_ami02`, and `_ami03` contain ordinary base batches plus separate `0x8000` overlay batches using the `sel_kmn3` or `sel_kmn4` family. Inspected overlays use reduced vertex alpha.

Apply these rules at leaf-batch granularity:

- ordinary structural geometry follows its opaque or hard-cutout state;
- rope textures use hard-alpha coverage so spaces between strands have no coverage;
- a batch explicitly marked `0x8000` remains a soft overlay even when its object name otherwise suggests a cutout; and
- reduced vertex alpha on one batch must not make the entire net object translucent.

The larger `_umisaku-ami` hanging or ocean-net family also relies on cutout coverage for the spaces between ropes. Correct rendering leaves the holes genuinely empty while preserving the strands and any separately authored soft layer.

### Seaweed

The `_wakame` family uses texture alpha to define the seaweed silhouette. Render its card-like portions through the hard-alpha path with depth writes enabled. Treating the image as opaque produces a rectangular background; treating it as a soft translucent sheet weakens the silhouette and introduces avoidable ordering artifacts.

Culling remains a separate batch decision. Do not infer two-sided rendering merely from the seaweed name or from its need for alpha coverage.

### Drying-fish cards

The drying-fish displays use economical card geometry: much of each fish exists as a silhouette in `sel_kmn2` or `sel_kmn3`, not as a fully modeled volume. Relevant objects include `himono02`, `_himono03`, `_himono04`, and related rack, table, and shadow meshes.

The complete fish-card recipe is:

```text
texture              = original sel_kmn* DXT3 data
alpha equation       = saturate(4 * vertexAlpha * textureAlpha)
coverage             = hard alpha, pass when alpha >= 0.375
source blending      = disabled for the fish card
depth test           = enabled
depth writes         = enabled
rack and net batches = rendered independently using their own state
```

The dull rectangular region around the fish is background and must fail coverage. It must not be blended over the net. With the correct path, the fish silhouette writes depth normally while the net remains visible through the empty area surrounding it. The same rule is especially visible on hanging strings of dried fish viewed against the sky.

Some related table or shadow batches have reduced vertex alpha. That value alone is not an instruction to turn the entire prop into a soft-transparent object.

### Why global policies fail

Selbina demonstrates all of the following:

- A DXT3 texture is not automatically a blended material.
- A non-opaque texture alpha value is not automatically surface coverage.
- A texture name cannot define one alpha policy for every referencing batch.
- A placed object can contain opaque, hard-cutout, and soft-overlay batches at once.
- `0x8000` soft blending takes precedence for the marked batch.
- `0x2000` controls culling, not transparency.
- Vertex alpha participates in the fragment equation but does not independently choose the render class.
- Image conversion cannot repair incorrect batch-state classification.

### Selbina regression checklist

After changing texture, material, depth, or batching code, verify all of these together:

- dirt and sand layers match their terrain context without making base terrain transparent;
- base terrain, stairs, and stone remain solid when sharing textures with overlays;
- leaf cards have no rectangular background;
- only leaf batches requesting two-sided rendering remain visible from both sides;
- tree trunks and other solid geometry retain normal opaque behavior;
- fish cards have clean silhouettes without gray, green, or dark card borders;
- fish write depth and appear to rest directly on their racks or nets;
- display nets and larger hanging nets retain empty spaces between rope strands;
- `0x8000` net overlays remain softly blended without changing the base net;
- seaweed has a cutout silhouette instead of an opaque rectangle; and
- geometry closer to the camera correctly occludes a soft `0x8000` layer beneath it.

## Environment and fog (`0x2F`)

Environment resources contain:

- ambient color;
- fog color;
- fog start and end distances;
- sun and directional-light configuration; and
- state variations selected and interpolated across time of day and weather.

Placements can link to an environment record, allowing local or object-associated lighting behavior.

Fog is linear:

```text
fogFactor = (fogFar - distance) / (fogFar - fogNear)
```

Clamp the factor to the normalized range. Mix fog into RGB only and preserve fragment alpha.

The complete weather controller, sky-resource activation, and all indoor/outdoor selection rules are not yet fully specified.

## Native bump maps (`0x5D`)

Type `0x5D` is an 8-bit height map used to produce a normal map. Rendering computes surface tangents and a tangent-bitangent-normal basis, samples the derived normal, and applies it in the zone lighting calculation.

This confirms native per-pixel bump mapping in FFXI. It does not imply that every material has a bump map, and it should not be confused with the character cubemap reflection path.

## PS2 background light maps

The original PS2 renderer has a separate background light-map path. It clears an auxiliary alpha buffer, exposes framebuffer-derived data as a texture, and redraws selected background geometry through dedicated light-map routines.

This path is distinct from `0x5D` bump maps, environment fog, character environment mapping, and ordinary transparent overlays. A renderer targeting PS2 behavior should preserve that extra pass rather than attempting to fold all projected lighting into vertex color.

The exact resource command that activates every light-map case and the complete compositing equation still require further decoding.

## Collision geometry

Zone collision is separate from visible `0x2E` geometry. The `0x1C` resource contains:

- a spatial grid;
- lists of transform-offset and geometry-offset pairs for occupied cells;
- per-mesh transforms;
- `float3` positions and normals; and
- packed 8-byte triangle records.

For each triangle record:

```text
p0          = encoded0 & 0x7FFF
p1          = encoded1 & 0x3FFF
p2          = encoded2 & 0x3FFF
normalIndex = encoded3 & 0x7FFF
```

The top nibbles of all four encoded 16-bit values combine into a 16-bit material or terrain-type word. Preserve this word even if its complete gameplay semantics are not needed immediately.

Do not treat visible triangles as authoritative collision. Hidden visual helpers and collision-like names are not substitutes for the dedicated collision structure.

## Effect geometry summary

Several resource types contribute visible effects:

- `0x1F` stores ordinary expanded effect triangles with position, normal, color, and UV.
- `0x21` stores card or sprite-sheet geometry, including grouped card records.
- `0x25` stores a weighted or morphing mesh with separate position, color, UV, and index blocks.
- `0x05` controls resource selection, emission, movement, rotation, scale, color, curves, child effects, probable sound, and point lights.
- `0x19` stores pairs of floating-point keyframe values whose universal semantics are not completely assigned.

Static geometry can be previewed without a full effect runtime. Correct animation additionally requires generator timing, billboard constraints, curve evaluation, child spawning, texture animation, morphing, and blend-mode selection.

## Implementation sequence

1. Walk the aligned chunk stream and reject invalid sizes.
2. Apply version- and type-specific payload decoding.
3. Register named textures, skeletons, geometry, environments, and bump maps.
4. Decode paletted, raw BGRA, DXT1, DXT3, and combination textures.
5. Parse character draw commands without losing command order or state boundaries.
6. Decode per-corner UVs, rigid vertices, two-weight vertices, and mirrored passes.
7. Parse zone placements and compose `T * Rz * Ry * Rx * S` transforms.
8. Resolve placement names to reusable zone geometry and apply LOD thresholds.
9. Decode zone primitive mode and vertex stride from independent configuration bits.
10. Preserve every zone leaf batch as its own material/state unit.
11. Apply environment lighting, vertex color, texture modulation, and native alpha scaling.
12. Render opaque and hard-alpha batches with depth writes.
13. Render soft-blended batches without depth writes and with the required depth bias.
14. Apply linear fog to RGB while preserving alpha.
15. Build collision from the dedicated grid and packed collision records.

## Validation rules

- Bound every read by both its parent resource and the DAT size.
- Check all count-times-stride arithmetic for overflow.
- Validate indices before dereferencing vertices or normals.
- Preserve unknown bits and bytes; do not normalize them away.
- Keep primitive mode independent from vertex stride.
- Keep texture decoding independent from material classification.
- Keep transparent records separate even when names match.
- Treat DXT5 as unverified until a retail payload is observed.
- Do not infer mip levels from filtering settings.
- Do not render all unplaced zone meshes at identity.
- Do not use visible geometry as authoritative collision.
- Record the DAT path, chunk offset, type, name, and raw state for every rejected or novel record.

## Known unknowns

The following areas still need broader evidence or complete decoding:

- remaining character `0x8010` state fields;
- complete character LOD stream selection; the exposed PS2 `lodz` field has no sufficiently clear recovered consumer;
- animation translation, scale, and reference-frame semantics;
- zone flags `0x4000` and `0x1000` and secondary-state interactions;
- exact visibility, schedule, hierarchy, and culling-table behavior;
- universal transparent ordering rules;
- complete sky and weather-resource activation;
- static and projected shadow systems;
- full effect simulation; and
- platform-specific differences.

These uncertainties should be exposed as explicit implementation choices, not hidden behind asset-specific patches.

## Compact reference

| Subject | Rule |
|---|---|
| Chunk size | `(info >> 3) & 0xFFFFF0` |
| `0x91`, 32-bit | Raw BGRA, no palette |
| Confirmed compressed textures | DXT1 and DXT3 |
| Character textured primitives | `0x0054` list, `0x5453` strip |
| Character untextured primitives | `0x0043` list, `0x4353` strip |
| Placement transform | `T * Rz * Ry * Rx * S` |
| Zone config bit 0 | Triangle strip |
| Zone config bit 1 | 48-byte vertex with displacement |
| Zone transparent flag | `0x8000` runtime form |
| Zone no-cull flag | `0x2000` runtime form |
| Zone hard-alpha classifier | Mesh name begins with `_` |
| Zone alpha threshold | `0.375` |
| Character alpha threshold | PS2: approximately `0.375`; later PC path: `69/255` |
| Zone RGB | `2 * lit.rgb * texture.rgb` |
| Zone alpha | `4 * vertexAlpha * textureAlpha` |
| Blend-layer depth bias | `8` in native convention |
| Bump map | Type `0x5D`, 8-bit height field |
| Environment | Type `0x2F` |
| Collision masks | `p0/n = 0x7FFF`, `p1/p2 = 0x3FFF` |

## Conclusion

FFXI's visible world is built from named resources, compact draw streams, reusable zone meshes, instance records, vertex colors, fixed-function-style lighting, and several specialized state paths. Reliable rendering depends less on finding triangles than on preserving the boundaries and state attached to each draw: texture identity, primitive mode, vertex layout, alpha class, culling, lighting environment, bump mapping, reflection, and ordering.

A robust implementation should decode what is known exactly, retain raw data for what is not known, and avoid global rules based only on texture format or name. That approach supports ordinary characters, equipment, terrain, vegetation, layered scenery, environment lighting, fog, reflections, bump maps, and collision while remaining extensible as the remaining resource state is decoded.
