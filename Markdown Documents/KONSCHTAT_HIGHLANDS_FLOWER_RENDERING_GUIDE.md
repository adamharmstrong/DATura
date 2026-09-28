# Konschtat Highlands Flower Rendering Guide

## Purpose

This document explains how to identify, decode, render, animate, and validate the flower sheets found in Konschtat Highlands in Final Fantasy XI. It is intended to stand alone. A renderer should be able to implement the flower path from this guide without knowledge of any particular viewer, engine, or prior reverse-engineering project.

The flowers are deceptively difficult. Their geometry is made from broad sheets lying close to the ground, and their image data occupies rectangular regions of a DXT3 texture atlas. If the renderer treats those rectangles as ordinary opaque polygons, the atlas background appears as large blue, purple, pink, or dark patches. If it treats them as generic translucent surfaces, the flowers acquire halos, unstable ordering, and incorrect depth interactions.

The correct model is simpler and stricter:

> Konschtat flower sheets are two-sided, depth-writing hard cutouts whose alpha is amplified before a fixed threshold test.

Some flower vertices also carry authored displacement vectors for vegetation motion. That animation is independent of the material classification.

## Scope and confidence

This guide covers the `_con_hana_*` zone-geometry family and the `con_f01c` texture atlas used by the observed Konschtat flowers.

The following behaviors are directly supported by FFXI data and original PS2 renderer behavior:

- a leading underscore selects the zone alpha-test path;
- zone alpha testing uses a native reference equivalent to approximately `0.375` in conventional normalized alpha;
- zone texture alpha is multiplied by vertex alpha and amplified by four before coverage testing;
- the observed flower batches use the no-cull flag;
- the flower batches are not soft-transparency batches;
- ordinary and transparent zone groups are separate native draw groups;
- 48-byte zone vertices support an additional displacement vector;
- primitive topology and displacement support are independent controls.

The exact time function driving all retail vegetation motion is not fully established here. The asset-defined displacement is authoritative; a renderer-specific wind phase is an approximation unless independently verified.

## Asset identity

### Mesh names

The relevant flower meshes use names in the following family:

```text
_con_hana_*
```

The important part is the first character. For zone geometry, a leading `_` is a native signal that the geometry belongs to the hard-alpha path. It is not merely a descriptive naming convention.

Do not remove or normalize away the leading underscore when reading fixed-length resource names. Material classification may depend on the original bytes.

### Texture atlas

The flower batches sample the following 256 by 256 texture atlas:

```text
con_f01c
```

The atlas is DXT3-compressed and contains visible flower imagery surrounded by colored texels that are intended to fail the alpha test. The RGB color of a texel is therefore not sufficient evidence that the texel should be visible.

Multiple meshes or batches can reference one atlas. Rendering state belongs to the draw batch, not globally to the texture. Never mutate the texture into a permanently opaque or permanently blended resource based only on its name or codec.

### Geometry orientation

The flowers are broad sheets placed nearly parallel to the terrain. They are not conventional upright crossed billboards. Their silhouettes are produced by texture coverage rather than by tightly fitted polygon outlines.

This orientation makes errors conspicuous:

- opaque rendering exposes the whole rectangular sheet;
- incorrect depth writes cause sheets to leak through each other;
- culling can remove a sheet when viewed from below, across a slope, or after a handedness-changing transform;
- excessive depth bias can make the flowers appear detached from the ground.

Nearby `_kusa_*` resources are generally upright grass cards. They can share the hard-alpha and no-cull principles, but they should not be used as proof that the flower sheets themselves are upright billboards.

## Keep five decisions independent

Many flower bugs come from deriving several render states from one field. Keep these decisions separate:

1. **Primitive topology:** triangle list or triangle strip.
2. **Vertex format:** static 36-byte or displacement-capable 48-byte vertices.
3. **Coverage mode:** opaque, hard alpha, or soft blend.
4. **Face culling:** ordinary culling or no-cull/two-sided drawing.
5. **LOD selection:** which authored geometry variant is active at the current distance.

For example, a 48-byte vertex does not imply transparency, and no-cull does not imply blending. A triangle strip can use either vertex format. A texture can be referenced by batches with different coverage modes.

## Batch classification

Classify each decoded draw batch before creating render queues.

For an observed Konschtat flower batch:

```text
name begins with '_'    -> hard-alpha coverage
flag 0x2000 is set      -> disable face culling
flag 0x8000 is clear    -> not a soft-blend overlay
```

The resulting classification is:

```text
coverage class : hard alpha
blending       : disabled
depth test     : enabled
depth writes   : enabled
culling        : disabled when 0x2000 is set
```

Do not route these flowers through the transparent queue merely because the texture has alpha. Texture encoding does not choose the render pass.

## Vertex decoding

### Static 36-byte vertices

The static zone vertex form contains the ordinary position, normal, packed color, and texture-coordinate data required by the batch. Preserve the packed vertex alpha. It participates directly in the final coverage calculation.

### Displacement-capable 48-byte vertices

The known 48-byte layout is:

| Byte offset | Size | Meaning |
|---:|---:|---|
| `0x00` | 12 | Base position, three floating-point components |
| `0x0C` | 12 | Authored displacement vector, three floating-point components |
| `0x18` | 12 | Normal, three floating-point components |
| `0x24` | 4 | Packed BGRA vertex color |
| `0x28` | 8 | Texture coordinates |

The field at `0x0C` is a displacement vector, not an absolute second position. Evaluate it as:

```text
animatedPosition = basePosition + animationWeight * displacement
```

The displacement vector receives placement rotation and scale, but not placement translation. If a zone-coordinate conversion reflects an axis, reflect both the base position and the displacement consistently.

Vertices with zero displacement remain anchored. Flower-sheet edges or centers with nonzero displacement can move while roots remain fixed.

### Do not conflate format and topology

The geometry controls for triangle-strip topology and 48-byte vertex blending are independent. A correct decoder must support all four combinations:

| Topology | Vertex form | Required behavior |
|---|---|---|
| Triangle list | 36-byte | Decode static list |
| Triangle strip | 36-byte | Decode static strip with correct winding alternation |
| Triangle list | 48-byte | Decode displacement-capable list |
| Triangle strip | 48-byte | Decode displacement-capable strip with correct winding alternation |

If strips are expanded into lists, skip degenerate triangles and preserve the alternating winding. When culling is disabled this may not immediately change visibility, but incorrect winding can still contaminate normals, debugging tools, exports, or later state changes.

## Texture decoding

### Preserve DXT3 alpha exactly

DXT3 stores explicit four-bit alpha values. Expand each alpha nibble without deriving alpha from RGB or color-key rules.

A conventional eight-bit expansion is:

```text
alpha8 = alpha4 * 17
```

Equivalent normalized decoding is:

```text
textureAlpha = alpha4 / 15
```

Do not premultiply the decoded RGB unless the entire subsequent blend and shader path is designed for premultiplied data. The flower path does not require source-alpha blending.

### Respect UVs and sampler state

Use the batch's authored UVs to sample the atlas. Keep these common errors out of the flower investigation:

- vertically flipping the texture but not the UV convention;
- decoding block rows in the wrong order;
- swapping color endpoints or DXT block coordinates;
- clamping an atlas that expects wrapping, or wrapping across an edge that expects clamping;
- applying half-texel adjustments from an unrelated graphics API without validation.

Colored rectangles can be caused by a missing alpha test, but distorted or displaced flowers often indicate UV or DXT decoding errors instead.

## The alpha equation

The zone path combines texture and vertex alpha and amplifies the result:

```text
fragmentAlpha = saturate(4 * vertexAlpha * textureAlpha)
```

Coverage then uses the zone hard-alpha reference:

```text
keep fragment when fragmentAlpha >= 0.375
discard fragment otherwise
```

When reproducing a strict native greater-than comparison, use the target graphics API's integer or normalized comparison semantics carefully. The practical normalized threshold is approximately `0.375`; an exact edge test should be verified with known alpha values rather than assumed from floating-point rounding.

The multiplication by four is essential. Testing raw texture alpha at `0.375`, or multiplying texture and vertex alpha without the amplification, removes legitimate petals and leaf edges. Ignoring vertex alpha can expose texels that were authored to disappear.

### Recommended shader expression

```hlsl
float4 texel = FlowerTexture.Sample(FlowerSampler, input.uv);
float4 color = texel * input.color;

float coverageAlpha = saturate(4.0 * texel.a * input.color.a);
clip(coverageAlpha - 0.375);

return float4(computeZoneLighting(input, texel.rgb), coverageAlpha);
```

This pseudocode shows the coverage calculation, not a complete FFXI zone shader. The RGB path should preserve the zone's vertex color and environment lighting behavior. Do not accidentally multiply RGB by four simply because alpha is amplified by four.

In a fixed-function implementation, configure the equivalent texture-stage alpha operation and alpha-test state. The semantic result matters more than reproducing a particular API call sequence.

## Required render state

Use the following state for the flower draw:

| State | Value |
|---|---|
| Color writes | Enabled |
| Alpha test or shader discard | Enabled |
| Alpha reference | Approximately `0.375` after the FFXI alpha equation |
| Source blending | Disabled |
| Destination blending | Disabled |
| Depth test | Enabled |
| Depth comparison | The zone's ordinary depth comparison |
| Depth writes | Enabled |
| Face culling | Disabled when batch flag `0x2000` is set |
| Soft-overlay depth bias | Disabled |

The flowers should behave like irregular opaque silhouettes after coverage testing. Kept pixels participate normally in depth; rejected pixels write neither color nor depth.

### Why blending is wrong

Generic source-alpha blending produces a superficially plausible image at some angles, but it changes the surface model:

- partially transparent edge pixels reveal terrain through petals;
- intersecting sheets require ordering that the hard-cutout path does not need;
- depth writes are often disabled for blended geometry, allowing distant vegetation to leak through;
- atlas-background color can tint edges and create halos;
- multisurface intersections change with camera direction.

The presence of intermediate DXT3 alpha values is not proof that soft blending is intended. Those values become coverage after FFXI's alpha equation and threshold.

### Why depth writes matter

After a fragment passes the alpha test, it is an ordinary covered surface and must update depth. This stabilizes overlapping sheets and their interaction with grass and terrain.

Do not disable depth writes as a workaround for flower disappearance. If flowers vanish unexpectedly, inspect culling, winding, placement transforms, LOD selection, alpha math, and render order first.

### Why no-cull matters

The observed `0x2000` flag disables face culling. This allows the thin sheet to be visible from either side. The flag does not select alpha testing, soft blending, or a special texture operation.

Avoid implementing no-cull by duplicating triangles unless the rendering backend requires it. Drawing the original geometry with culling disabled preserves vertex sharing and avoids accidental z-fighting between duplicate faces.

## Draw order

Submit the flowers with ordinary zone geometry that uses hard-alpha coverage, before soft transparent overlays.

A robust high-level order is:

1. Opaque zone geometry.
2. Hard-alpha zone geometry, including Konschtat flowers and eligible grass cards.
3. Shadow or other dedicated zone passes whose placement is known.
4. Soft-blended transparent geometry and overlays.

Opaque and hard-alpha geometry may be combined into one depth-writing phase if each batch retains its own alpha-test and cull state. Do not move flowers into the soft-blend phase for batching convenience.

State leakage is a common failure. Explicitly set or restore alpha testing, blending, depth writes, culling, texture addressing, and shader permutations at batch or queue boundaries.

## Lighting and color

The known zone color model combines packed vertex color, sampled texture color, ambient light, directional lights, and applicable point lights. A useful representation is:

```text
lit.rgb = vertex.rgb * (ambient + directional0 + directional1 + pointLights)
rgb     = saturate(2 * lit.rgb * texture.rgb)
```

Keep the alpha calculation separate:

```text
alpha = saturate(4 * vertexAlpha * textureAlpha)
```

Do not add a generic white headlight to make the flowers readable. The atlas and vertex colors already carry authored color information, and unrelated light can wash out the flower pattern or make it glow at night.

Normal transformation must match the placement transform. If nonuniform scale is possible, use an inverse-transpose normal transform or an equivalent correctly normalized method.

## Vegetation motion

### Asset-defined deformation

For 48-byte vertices, compute object-space deformation before the placement transform:

```text
localAnimated = basePosition + windWeight * displacement
worldPosition = placementTransform(localAnimated)
```

Equivalently, when the transform is separated:

```text
worldPosition = translation
              + rotation * (scale * basePosition)
              + rotation * (scale * displacement) * windWeight
```

Translation applies once to the position and never to the displacement vector.

### Animation weight

Until an exact retail timing source is established, keep the weight function configurable. Useful validation modes are:

```text
0.0        static base pose
0.5        halfway deformation
1.0        full authored displacement
periodic   runtime wind approximation
```

A smooth periodic test function can be:

```text
windWeight = 0.5 - 0.5 * cos(phase)
```

This ranges from zero to one and is useful for testing, but it should not be presented as an exact statement of FFXI wind timing without further evidence.

### Avoid cumulative drift

Always deform from immutable base vertices:

```text
animated = base + weight * displacement
```

Never apply displacement to the previous frame's result. Repeated accumulation causes the flowers to crawl away from their authored positions.

### Bounds

Visibility bounds must include the full motion range. At minimum, build bounds from both endpoints:

```text
basePosition
basePosition + displacement
```

If the runtime weight can leave the `[0, 1]` interval, enlarge the bounds accordingly. Bounds computed only from the static pose can cause animated flower tips to pop at frustum edges.

## Placement and coordinate handling

Apply the same coordinate conversion to flowers as to other zone geometry. A reliable transform sequence is:

```text
1. Decode base position and displacement in source coordinates.
2. Apply any source-to-renderer axis conversion to both vectors.
3. Apply authored scale to both vectors.
4. Apply authored rotation to both vectors.
5. Apply translation only to the base position.
6. Combine transformed base and weighted displacement.
```

If the conversion changes handedness, correct triangle winding or explicitly rely on the no-cull state for visibility. Even when the flowers are no-cull, keeping global winding conventions correct prevents unrelated geometry failures.

The sheets should remain close to the terrain. A large vertical offset usually indicates an axis swap, an incorrect placement matrix, or treating displacement as an absolute position.

## LOD behavior

Zone placements can select authored high-, medium-, and low-detail resources using two distance thresholds. The original selection rule is:

```text
if clipDistance != 0 and distance >= clipDistance:
    hidden
else if low exists and distance > lodFarThreshold:
    low
else if medium exists and distance > lodNearThreshold:
    medium
else:
    high
```

If the high-detail model is missing, resource resolution can fall back to medium and then low. Preserve that fallback instead of treating a missing high variant as a missing object.

Do not infer flower LOD from texture resolution or vertex stride. LOD belongs to model-family resolution and placement distance. Test boundary equality carefully: clipping uses `>=`, while the observed LOD transitions use `>`.

## Reference implementation structure

The following pseudocode keeps the decisions separate:

```text
for each visible placement:
    model = selectPlacementLOD(placement, cameraDistance)
    if model is hidden or missing:
        continue

    for each batch in model:
        topology = decodeTopology(batch)
        vertices = decodeVertexForm(batch)

        coverage = OPAQUE
        if batch.isNativeTransparentGroup or (batch.flags & 0x8000):
            coverage = SOFT_BLEND
        else if batch.name begins with '_':
            coverage = HARD_ALPHA

        noCull = (batch.flags & 0x2000) != 0

        if vertices contain displacement:
            upload base + windWeight * displacement
        else:
            upload base

        enqueue(batch, topology, coverage, noCull)

draw opaque queue
draw hard-alpha queue with depth writes enabled
draw soft-blend queue with its native transparent state
```

For the known `_con_hana_*` batches, classification should resolve to `HARD_ALPHA`, not `SOFT_BLEND`.

## Debugging by symptom

### Large colored rectangles

Likely causes:

- alpha testing or shader discard is disabled;
- texture alpha was lost during DXT3 decoding or upload;
- the leading underscore was stripped before classification;
- the batch was forced into an opaque material;
- the alpha equation uses a constant one instead of texture and vertex alpha.

First diagnostic: display decoded texture alpha as grayscale. The flower silhouette should be visible in alpha even where RGB contains a colored atlas background.

### Flowers are mostly or completely missing

Likely causes:

- raw alpha is tested without the required factor-of-four amplification;
- the alpha threshold is applied before texture and vertex alpha are combined;
- the threshold is much higher than `0.375`;
- the DXT3 alpha nibble order is wrong;
- culling remains enabled;
- the selected LOD variant is missing and no fallback is implemented;
- placement clipping is being triggered too early.

### Flowers look translucent or washed out

Likely causes:

- source-alpha blending is enabled;
- RGB was unintentionally premultiplied;
- vertex color or lighting is applied twice;
- a generic vegetation material overrides the native hard-alpha class.

### Halos around petals

Likely causes:

- soft blending is being used;
- bilinear sampling reaches colored texels outside the intended atlas region;
- mip generation averaged RGB across transparent boundaries;
- alpha and RGB were filtered or converted inconsistently.

Hard alpha should remove most background-color contribution. If mipmaps are generated rather than supplied, use an alpha-aware process and inspect distant LODs separately.

### Flickering with camera movement

Likely causes:

- depth writes are disabled;
- the flowers are incorrectly sorted as transparent;
- duplicate back faces were generated at the same depth;
- sheets are offset excessively or inconsistently from terrain;
- LOD thresholds oscillate because distance or units are wrong.

### Visible from one side only

Likely causes:

- flag `0x2000` was ignored;
- cull state leaked from the previous batch;
- a coordinate reflection reversed winding and the batch was not classified as no-cull.

### Flowers stretch or detach during wind

Likely causes:

- displacement was treated as an absolute second position;
- translation was applied to the displacement vector;
- deformation was accumulated frame to frame;
- base and displacement vectors used different axis conversions;
- placement scale or rotation was applied to only one vector.

### Flowers pop near the edge of the screen

Likely causes:

- culling bounds include only the static base pose;
- bounds were calculated before placement scale or rotation;
- an incorrect LOD or clip distance is being used.

## Instrumentation

A flower-specific debug view should expose, per selected batch:

- original 16-byte resource name;
- texture name and dimensions;
- raw batch flags;
- native ordinary or transparent group membership;
- selected coverage class;
- alpha equation and threshold;
- cull mode;
- depth-write state;
- topology;
- vertex stride;
- nonzero displacement-vector count;
- active LOD variant and thresholds;
- placement distance and clip distance;
- decoded vertex-alpha range;
- decoded texture-alpha range.

Useful display modes include:

1. RGB only, forced opaque.
2. Texture alpha as grayscale.
3. Vertex alpha as grayscale.
4. Final amplified alpha before threshold.
5. Binary coverage mask after threshold.
6. Front faces and back faces in different colors.
7. Base positions versus fully displaced positions.
8. LOD variants in distinct diagnostic colors.

These modes turn a vague rectangle problem into a specific failure in texture decoding, alpha math, state classification, geometry orientation, or placement selection.

## Validation procedure

### Stage 1: texture validation

- Decode `con_f01c` as DXT3.
- Verify the image is 256 by 256 pixels.
- Inspect all alpha nibbles independently from RGB.
- Confirm flower silhouettes appear in the alpha channel.
- Confirm texture orientation matches the batch UVs.

### Stage 2: geometry validation

- Identify `_con_hana_*` meshes without renaming them.
- Render wireframes to confirm broad ground-parallel sheets.
- Verify list and strip decoding independently.
- Confirm both 36-byte and 48-byte forms can be decoded.
- Check that 48-byte displacement vectors are plausible vectors, not world positions.

### Stage 3: material validation

- Force hard alpha with the amplified alpha equation.
- Disable source blending.
- Enable depth testing and depth writes.
- Apply the approximately `0.375` threshold.
- Honor `0x2000` by disabling culling.
- Confirm `0x8000` is not being invented for these batches.

Expected result: atlas rectangles disappear, petals and leaves remain sharply covered, and overlapping sheets remain stable.

### Stage 4: animation validation

- Render with weights `0`, `0.5`, and `1`.
- Confirm zero-displacement roots do not move.
- Confirm nonzero vectors move in the expected direction.
- Return to weight `0` and verify exact restoration.
- Run many cycles and confirm there is no accumulated drift.
- Move the camera across the frustum boundary and verify animated bounds do not pop.

### Stage 5: scene validation

Inspect the flowers:

- from directly above;
- at a shallow grazing angle;
- from both sides of sloped terrain;
- at dawn, daytime, dusk, and night lighting;
- at each LOD transition;
- beside grass cards and other hard-alpha vegetation;
- in motion and in a frozen base pose.

The correct result should not show colored rectangular backgrounds, soft halos, camera-dependent sorting changes, one-sided disappearance, or motion-induced detachment.

## Automated regression tests

At minimum, add tests for:

1. DXT3 alpha-nibble expansion, including `0`, intermediate values, and `15`.
2. The final alpha equation `saturate(4 * vertexAlpha * textureAlpha)`.
3. Fragments just below, at, and above the chosen threshold semantics.
4. Leading-underscore hard-alpha classification.
5. `0x2000` selecting no-cull without selecting transparency.
6. `0x8000` remaining independent from name and vertex stride.
7. All four topology and vertex-format combinations.
8. Strip triangulation and degenerate-index handling.
9. Displacement transformed as a vector without translation.
10. Immutable-base animation with no cumulative drift.
11. Bounds containing base and fully displaced endpoints.
12. LOD boundary behavior and missing-high fallback.
13. Render-state restoration after a flower batch.
14. A pixel test proving atlas-background texels are discarded while flower texels write color and depth.

For the pixel test, render a flower sheet over two overlapping depth layers. Verify that rejected atlas-background pixels reveal the nearest underlying layer, while accepted flower pixels occlude both layers. This catches the common mistake of writing depth before alpha rejection.

## Minimal correctness checklist

A renderer is not finished with the Konschtat flowers until all of the following are true:

- [ ] `_con_hana_*` names retain their leading underscore.
- [ ] `con_f01c` is decoded as DXT3 with intact explicit alpha.
- [ ] Classification occurs per draw batch, not per texture.
- [ ] Flowers use hard alpha rather than source-alpha blending.
- [ ] Final alpha is `saturate(4 * vertexAlpha * textureAlpha)`.
- [ ] The zone threshold is approximately `0.375` with verified comparison semantics.
- [ ] Depth testing and depth writes are enabled for surviving fragments.
- [ ] `0x2000` disables culling without changing coverage mode.
- [ ] `0x8000` is not assumed merely because the texture contains alpha.
- [ ] Static and displacement-capable vertices are both supported.
- [ ] Displacement is treated as a vector and evaluated from immutable base data.
- [ ] Triangle list and strip topology are decoded independently from vertex format.
- [ ] Motion-aware bounds prevent frustum popping.
- [ ] Placement LOD and clipping use the correct boundary comparisons.
- [ ] Colored atlas rectangles, halos, sorting flicker, and one-sided disappearance are absent.

## Final rendering recipe

For each visible `_con_hana_*` flower batch:

1. Select the correct placement LOD and resolve any missing high-detail variant through the authored fallback chain.
2. Decode the batch topology and either the 36-byte or 48-byte vertex form independently.
3. If displacement exists, compute `base + weight * displacement` before applying the placement transform.
4. Sample `con_f01c` using correctly decoded DXT3 alpha and the authored UVs.
5. Compute `saturate(4 * vertexAlpha * textureAlpha)`.
6. Reject fragments below the zone hard-alpha reference of approximately `0.375`.
7. Keep source blending disabled.
8. Keep depth testing and depth writes enabled.
9. Disable culling when the batch has flag `0x2000`.
10. Draw the batch in the depth-writing zone phase before soft transparent overlays.

Those ten steps reproduce the essential behavior. The central lesson is that the flowers are not translucent rectangles. They are animated, two-sided cutout surfaces, and every part of that description matters.
