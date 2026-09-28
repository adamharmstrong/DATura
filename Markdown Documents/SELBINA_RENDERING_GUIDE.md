# Selbina Rendering Guide

## Purpose

This document is a self-contained guide to rendering Selbina from Final Fantasy XI. Selbina is an unusually useful renderer test because a compact settlement combines solid terrain, coplanar dirt and sand layers, trees, leaf cards, rope nets, drying fish, seaweed, water, indoor companion geometry, fog, lighting, and authored level-of-detail behavior.

Several of those surfaces use DXT3 textures with similar alpha ranges while requiring different render states. The correct result cannot be obtained by assigning one material policy to a texture, an object name, or an entire placed model.

The governing rule is:

> Decode state and classify rendering at draw-batch granularity.

A single Selbina object can contain opaque structure, hard-cutout detail, and a soft-blended overlay. A single texture can be used by both opaque terrain and a transparent surface layer. Any renderer that collapses those distinctions will fix one landmark while breaking another.

## Scope and evidence

This guide covers:

- base terrain, stairs, stone, wood, and buildings;
- dirt and sand overlays;
- tree trunks and leaf cards;
- small display nets and large hanging or ocean nets;
- drying-fish cards and their supporting racks;
- seaweed cards;
- generator-owned water;
- placement LOD and clipping;
- room and sub-area geometry;
- environment lighting and fog;
- draw ordering, diagnostics, and regression tests.

The following are strongly established by FFXI data and original PS2 renderer behavior:

- ordinary and transparent zone geometry are separate native groups;
- a leading `_` selects the zone hard-alpha path;
- the zone hard-alpha reference is approximately `0.375` on a conventional normalized-alpha scale;
- zone alpha is computed from texture alpha and vertex alpha with a factor-of-four amplification;
- flag `0x8000` identifies a soft transparent layer in the decoded zone batch word;
- flag `0x2000` disables culling for the affected batch;
- soft transparent zone groups blend with depth testing, no depth writes, and native depth bias;
- placement records carry LOD thresholds, clip distance, environment selection, and light links;
- water has a dedicated rendering path rather than being ordinary terrain;
- room state can alter visible placed parts, clipping, environment selection, and location data.

Exact water reflection, distortion, foam, and all effect-animation equations remain separate research topics. This guide identifies the ownership and ordering needed to avoid conflating them with terrain or overlays.

## Why Selbina is difficult

Selbina defeats several attractive shortcuts:

- **“DXT3 means transparent.”** False. Solid terrain can use a DXT3 texture while ignoring its alpha for coverage.
- **“One texture means one material.”** False. `sel_wl1` and `sel_wl2` participate in both opaque base terrain and soft dirt or sand layers.
- **“One object means one pass.”** False. A net object can contain structural geometry, hard-cutout rope, and a separate soft overlay.
- **“Reduced vertex alpha means blending.”** False. Vertex alpha participates in the fragment equation but does not independently select the pass.
- **“No-cull means transparent.”** False. `0x2000` only controls face culling.
- **“A leading underscore means all children are identical.”** False. Batch flags and native group membership still take precedence.
- **“Water is another transparent zone mesh.”** False. It is generator/effect-owned and has its own pass boundary.

The result must be assembled from independent state decisions.

## Core batch model

For every decoded zone draw batch, preserve at least:

```text
resource name
texture/material name
native ordinary or transparent group
vertex count
primitive topology
vertex format
raw batch flags
vertex-color and alpha data
placement ownership
LOD family
environment and light links
```

Do not flatten those values into a texture-only material table.

### Coverage classes

Selbina requires three coverage classes:

| Class | Coverage | Blending | Depth writes | Typical Selbina use |
|---|---|---|---|---|
| Opaque | Every rasterized fragment | Disabled | Enabled | Terrain, stone, trunks, buildings, racks |
| Hard alpha | Thresholded texture and vertex alpha | Disabled | Enabled | Leaves, rope holes, fish silhouettes, seaweed |
| Soft overlay | Continuous source alpha | Enabled | Disabled | Dirt, sand, and explicitly marked net layers |

The native `0x8000` transparent-layer state takes precedence for that marked batch. Otherwise, a leading underscore identifies the hard-alpha path. Ordinary non-cutout geometry remains opaque.

### Culling is independent

Flag `0x2000` disables face culling for the affected batch. This is useful for thin cards that must be visible from both sides.

It does not:

- enable alpha testing;
- enable source-alpha blending;
- select a vertex format;
- choose a texture;
- identify water; or
- apply to every batch in the same object.

Apply no-cull only where authored.

### Vertex form and topology are independent

Zone geometry can use triangle lists or strips and static or displacement-capable vertices. Treat these as separate controls. Never use a 48-byte stride as a transparency classifier.

## Texture families

Important Selbina texture families include:

| Family | Examples | Typical dimensions | Observed stored alpha | Uses |
|---|---|---:|---:|---|
| Terrain and sand | `sel_wl1`, `sel_wl2` | 512x256 | 0-136 | Solid terrain plus soft surface layers |
| Tree foliage | `kin_w02c`, `kin_w03c`, `kin_w04c` | 128x128 or 128x256 | 0-136 | Leaf cards |
| Nets and fish | `sel_kmn1` through `sel_kmn4` | 128x128 through 512x256 | 0-136 or 119-136 | Ropes, drying fish, props, overlays |

These textures use FFXI texture sections with a native 12-byte mini-header followed by DXT3 data. Preserve the explicit DXT3 alpha.

Observed reduced alpha ranges are part of FFXI's authored color and alpha convention. Do not stretch every texture's alpha to 0-255 as an image-normalization step, and do not infer a render class from the maximum stored alpha.

### DXT3 decoding

DXT3 stores four-bit explicit alpha. A conventional normalized decode is:

```text
textureAlpha = alpha4 / 15
```

An equivalent eight-bit expansion is:

```text
alpha8 = alpha4 * 17
```

Preserve RGB and alpha relationships. Avoid destructive conversions intended to fix one prop, because the same texture family may serve a different material role elsewhere.

### Texture alpha is data, not policy

The texture supplies sampled alpha. The batch determines what that alpha means:

- ignored for opaque coverage;
- transformed into binary coverage for hard alpha; or
- used continuously for source-alpha blending in a soft overlay.

That distinction is the heart of Selbina rendering.

## Zone fragment behavior

The known zone equations can be represented as:

```text
lit.rgb = vertex.rgb * (ambient + directional0 + directional1 + pointLights)
rgb     = saturate(2 * lit.rgb * texture.rgb)
alpha   = saturate(4 * vertexAlpha * textureAlpha)
```

Directional and point lights include normal-dependent diffuse response. Keep alpha computation separate from RGB amplification.

For hard-alpha geometry:

```text
keep fragment when alpha >= approximately 0.375
discard fragment otherwise
```

For soft overlays, use the computed alpha as blend opacity rather than thresholded coverage.

For opaque batches, texture alpha does not remove coverage. Vertex and texture RGB still contribute to color.

## Terrain, stairs, and stone

### Opaque base surfaces

Ordinary terrain, stairs, stone, and similar structural batches render as opaque depth-writing geometry even when their texture contains non-opaque alpha values.

Required state:

```text
alpha test       = disabled unless explicitly selected by batch identity
source blending  = disabled
depth test       = enabled
depth writes     = enabled
culling          = batch-specific ordinary state
```

Do not punch holes in base terrain merely because `sel_wl1` or `sel_wl2` contains alpha.

### Dirt and sand overlays

Selbina places separate `0x8000` batches over base terrain, stairs, and stone. They can reuse `sel_wl1` or `sel_wl2`. Their vertices commonly carry alpha near 126 in the native authored range.

Required state:

```text
coverage          = continuous alpha
source blend      = source alpha
destination blend = inverse source alpha
depth test        = enabled
depth writes      = disabled
alpha test        = disabled
depth bias        = native transparent-layer bias
```

The original zone path uses a native bias value of 8. Graphics APIs encode depth bias differently, so translate the intent rather than blindly copying the integer into an unrelated representation.

### Why the overlay needs its own batch

The dirt or sand layer is near-coplanar with the base surface. It relies on:

- the base drawing first and writing depth;
- the overlay testing against that depth;
- a small bias preventing equal-depth rejection or flicker;
- disabled depth writes so one soft layer does not incorrectly block another; and
- preserved batch boundaries for ordering.

Do not merge every batch using `sel_wl1` or `sel_wl2`. Texture equality does not imply that geometry can share one draw unit.

### Terrain failure signatures

| Symptom | Likely cause |
|---|---|
| Large holes in terrain | Texture alpha incorrectly used for opaque coverage |
| Dirt or sand too solid | `0x8000` batch rendered opaque or hard-cutout |
| Overlay missing | Strict depth comparison without suitable bias, or pass drawn too early |
| Overlay flickers | Coplanar depth conflict or unstable batch merging |
| Rear surface overlay appears in front | Excessive bias or missing ordinary depth test |
| Ground changes brightness at overlay edge | RGB/alpha equation differs between base and overlay |

## Trees and foliage

Relevant foliage textures include `kin_w02c`, `kin_w03c`, and `kin_w04c`. Observed tree families include `_sel_w01_m`, `_sel_w04_m`, `_sel_w05_h`, and `_sel_w08_h`.

### Separate trunks from leaves

A tree is not one material:

- trunks and branches are ordinary opaque geometry;
- leaf cards use hard-alpha coverage;
- only leaf batches carrying `0x2000` are two-sided;
- leaf batches without `0x2000` retain ordinary culling.

Required leaf-card state:

```text
alpha             = saturate(4 * vertexAlpha * textureAlpha)
coverage          = hard alpha at approximately 0.375
source blending   = disabled
depth test        = enabled
depth writes      = enabled
culling           = disabled only when 0x2000 is set
```

### Common foliage errors

- Without hard alpha, the leaf texture appears as a dark rectangular card.
- With global no-cull, trunks and unrelated geometry render incorrectly from inside.
- With global culling, selected cards disappear when viewed from the back.
- With source-alpha blending, leaf edges become soft, ordering-dependent, and haloed.
- With depth writes disabled, rear leaves and neighboring trees leak through the canopy.

### Vegetation displacement

If a leaf batch uses the 48-byte zone vertex form, the second position-sized field is an authored displacement vector:

| Offset | Meaning |
|---:|---|
| `0x00` | Base position |
| `0x0C` | Displacement vector |
| `0x18` | Normal |
| `0x24` | Packed BGRA color |
| `0x28` | UV |

Evaluate:

```text
animatedPosition = basePosition + windWeight * displacement
```

Apply placement rotation and scale to the displacement, but not translation. Always animate from immutable base data to avoid cumulative drift. Expand culling bounds to contain both base and fully displaced positions.

The presence of displacement does not select hard alpha. Coverage remains a separate batch decision.

## Nets

Selbina includes small display nets and larger hanging or ocean-net structures. They demonstrate why object-wide material inference fails.

### Small net families

Objects such as `_ami01`, `_ami02`, and `_ami03` contain ordinary base batches and separate `0x8000` overlay batches using `sel_kmn3` or `sel_kmn4`. Inspected overlay vertices have reduced alpha.

Treat each batch independently:

- solid support geometry can remain opaque;
- rope imagery uses hard alpha so gaps between strands have no coverage;
- a batch explicitly marked `0x8000` remains a soft overlay;
- reduced vertex alpha does not make the entire object translucent;
- no-cull is controlled only by the affected batch's `0x2000` state.

### Large nets

The `_umisaku-ami` family relies on hard-cutout coverage for spaces between ropes. Correct rendering leaves actual holes rather than dark or translucent rectangles.

For a hard-cutout rope batch:

```text
alpha equation   = saturate(4 * vertexAlpha * textureAlpha)
coverage         = threshold at approximately 0.375
blending         = disabled
depth test       = enabled
depth writes     = enabled
culling          = batch-specific
```

### Overlay precedence

A leading underscore usually selects hard alpha, but an explicit native transparent batch or decoded `0x8000` layer must retain its soft-overlay behavior. Classification should be ordered so authoritative transparent-group state is not overwritten by a broad name rule.

One safe precedence model is:

```text
if native transparent group or flag 0x8000:
    SOFT_OVERLAY
else if resource name begins with '_':
    HARD_ALPHA
else:
    OPAQUE
```

This is a rendering classifier, not permission to discard the original flags.

## Drying fish

The drying-fish displays use economical flat-card geometry. Much of each fish is a texture silhouette rather than a modeled volume.

Relevant objects include:

```text
himono02
_himono03
_himono04
```

They use members of the `sel_kmn2` and `sel_kmn3` texture families alongside rack, table, net, and shadow geometry.

### Fish-card recipe

```text
texture              = original sel_kmn* DXT3 data
alpha                 = saturate(4 * vertexAlpha * textureAlpha)
coverage              = hard alpha at approximately 0.375
source blending       = disabled
depth test            = enabled
depth writes          = enabled
culling               = batch-specific
rack and net geometry = rendered independently
```

The dull rectangular area around each fish is atlas background and must fail coverage. It should not be blended over the supporting net.

When correct:

- each fish has a clean silhouette;
- the fish writes depth as a solid covered surface;
- the rack or net remains visible through the empty area around the fish;
- hanging strings of fish remain clean against the sky; and
- nearby fish cards do not require transparent sorting.

### Fish-card failure signatures

| Symptom | Cause |
|---|---|
| Gray-green rectangle around fish | Missing alpha test or lost DXT3 alpha |
| Net disappears behind empty card area | Card rendered opaque or depth written before discard |
| Fish looks ghostly | Source-alpha blending used instead of hard coverage |
| Fish edges disappear | Missing factor-of-four alpha amplification or threshold too high |
| Whole display becomes translucent | Object-wide policy inherited from one reduced-alpha batch |

Some related table or shadow batches have reduced vertex alpha. Preserve their own state instead of applying the fish-card recipe to every child.

## Seaweed

The `_wakame` family uses texture alpha to define a seaweed silhouette.

Render card-like seaweed portions using hard alpha:

```text
alpha test      = enabled
alpha reference = approximately 0.375 after the zone alpha equation
blending        = disabled
depth test      = enabled
depth writes    = enabled
culling         = determined independently
```

Opaque rendering exposes the rectangular card background. Soft blending weakens the silhouette and creates avoidable ordering artifacts.

Do not infer two-sided rendering solely from the `_wakame` name. Honor batch culling state.

## Water

Water in FFXI has a dedicated rendering path and is coordinated through generator or effect resources. It is not an ordinary `0x2E` terrain material and not a dirt-overlay subtype.

### Ownership

Keep water separate from:

- terrain and collision geometry;
- zone `0x8000` overlays;
- linear environment fog;
- framebuffer-derived background light maps;
- general distortion effects; and
- weather records.

Weather and environment state can influence water color, visibility, or activation without owning the water mesh itself.

### Practical rendering state

For a conventional reconstruction:

```text
source blending  = enabled according to authored water blend
depth test       = enabled
depth writes     = disabled
sorting          = back to front within the water group
collision        = excluded unless separately authored
UV animation     = evaluated from generator data
color and alpha  = evaluated from authored curves when present
```

Draw opaque terrain and hard-cutout props before water so buildings, docks, nets, and actors can occlude it. Ordinary zone transparency generally precedes the final water pass when that ordering resolves underwater transparent terrain artifacts.

A useful broad order is:

1. Opaque zone geometry.
2. Hard-alpha zone geometry.
3. Actors and other solid scene objects at the renderer's established boundary.
4. Ordinary zone soft transparency.
5. Water.

This is not order-independent transparency. Intersections between translucent groups can still require careful sorting and targeted validation.

### What not to assume

Do not assume one universal water shader proves:

- exact retail reflection behavior;
- refraction;
- shoreline foam lifetime;
- spray generation;
- every wave equation;
- distortion-buffer use; or
- one blend mode for every body of water.

Preserve generator identity, linked resources, placement transform, UV velocity, color curves, and authored blend data so improved behavior can be added without reparsing the zone.

## Placement, LOD, and clipping

Zone placements can reference high-, medium-, and low-detail resource variants, commonly ending in `_h`, `_m`, and `_l`.

The original selection behavior is:

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

Important details:

- clipping uses `>=`;
- LOD transitions use `>`;
- if high detail is absent, resource resolution can fall back to medium and then low;
- a missing selected variant is not permission to draw every unplaced mesh;
- LOD is independent from texture format, transparency, and vertex stride.

Validate Selbina trees and props at each transition distance. A material fix applied only to `_h` will appear to regress when `_m` becomes active.

### Grid geometry

The original PS2 background path also has grid-chip LOD selection: high detail for squared grid distance below 5, medium otherwise. Keep this mechanism distinct from placed-part `_h`/`_m`/`_l` thresholds.

## Rooms and companion geometry

Selbina has five drawable room or sub-area companions in the audited zone relationships. Room behavior is not simply “draw another DAT beside the main zone.”

Entering or selecting a room can change:

- which placed parts are hidden or visible;
- the active clip identifier;
- the environment number;
- fog and lighting selection;
- shadow and transparent geometry participation;
- location-table selection; and
- which companion geometry is loaded.

### Room ownership rules

Resolve room companions through the zone's placement and interaction references. Do not scan nearby files and assume they belong to Selbina.

Directory root names are not globally unique. Selbina shares the root name `r_1s` with Southern San d'Oria, so a four-character root alone cannot establish ownership.

Preserve the actual sub-area identifier and table resolution that led to each companion.

### Room rendering checklist

For each room transition verify together:

- trigger or interaction identifier;
- resolved room/sub-area identifier;
- companion resource identity;
- placed-part membership;
- hidden flags;
- active `clip_id`;
- active `env_no`;
- fog and lighting change;
- room and exterior overlap handling;
- transparent and shadow draws; and
- restoration when leaving the room.

A mesh can parse correctly and still be absent because it belongs to another room state.

## Environment, fog, and lighting

FFXI keeps background and character environment color state distinct. A Selbina renderer should preserve:

- background ambient color;
- directional light colors and vectors;
- room light color and vector;
- point-light selection from placement links;
- fog color;
- fog near and far distances;
- light power;
- clear or sky color; and
- world focus and clip range.

During environment transitions, interpolate scalar and color quantities linearly. Normalize directional vectors and use directional interpolation that avoids collapsing through a zero-length vector.

Treat fog-disabled sentinels as discrete state rather than blindly interpolating an invalid near/far pair into visibility.

Do not use an analytic sun arc unless independently justified for the target behavior. Environment records provide authored directional vectors.

### Light selection on placements

Placed parts can reference multiple lights. Preserve all four light indices from the runtime placement structure. Do not collapse them to a single nearest light before validating the zone's intended contribution.

## Shadows and background light maps

The PS2 zone renderer separates ordinary geometry, shadow drawing, and transparent drawing. Keep those pass boundaries explicit.

Background light maps also have a dedicated path: an auxiliary alpha buffer is cleared, framebuffer-derived data is exposed as a texture, and selected background geometry is redrawn. This is distinct from:

- bump mapping;
- fog;
- environment mapping;
- soft dirt overlays; and
- water.

Do not explain every darkened Selbina surface as a transparent decal. Preserve enough batch and pass identity to add or disable each effect independently.

## Recommended draw pipeline

The following pipeline is a practical expression of the known behavior:

```text
1. Resolve room state, environment state, placements, clipping, and LOD.
2. Build independent queues for opaque, hard-alpha, soft-overlay, and water draws.
3. Draw opaque terrain, buildings, trunks, racks, and solid props with depth writes.
4. Draw hard-alpha leaves, ropes, fish, and seaweed with depth writes.
5. Draw dedicated shadow/background-light-map work at its established boundary.
6. Draw ordinary soft overlays with depth testing, no depth writes, and translated bias.
7. Draw water in its dedicated transparent phase.
8. Restore all state needed by later UI, effects, or scene draws.
```

Within transparent groups, preserve authored batches. When native ordering is unavailable, sort back to front using a representative transformed center, but treat that as an approximation rather than a universal claim about FFXI.

### Queue classification pseudocode

```text
for each visible placement:
    model = selectLOD(placement, cameraDistance)
    if model is hidden:
        continue

    for each drawBatch in model:
        if drawBatch belongs to native transparent group
           or drawBatch.flags contains 0x8000:
            queue = SOFT_OVERLAY
        else if drawBatch.originalName begins with '_':
            queue = HARD_ALPHA
        else:
            queue = OPAQUE

        cullMode = NONE if flags contains 0x2000 else AUTHORED_DEFAULT
        enqueue drawBatch without merging away its state

for each active water generator:
    enqueue generated water surface in WATER queue
```

Some named families without a leading underscore may still require verified family-specific classification. Record such compatibility rules explicitly and keep them subordinate to decoded native group state.

## State isolation

Every pass should explicitly establish and restore:

- alpha-test or shader-discard mode;
- alpha reference;
- source and destination blend factors;
- depth test and comparison;
- depth-write enable;
- depth bias;
- cull mode;
- texture addressing;
- texture-stage or shader alpha equation;
- lighting enable and light bindings;
- UV animation state; and
- active textures and samplers.

Selbina exposes state leakage quickly. A dirt overlay can disable depth writes and accidentally break fish; a leaf card can leave no-cull enabled for buildings; water UV animation can move the next ordinary texture.

## Debugging by symptom

### Terrain has holes

Likely causes:

- a DXT3 texture globally enabled alpha coverage;
- `sel_wl1` or `sel_wl2` was assigned one texture-wide alpha material;
- an ordinary base batch inherited overlay state;
- alpha test leaked from foliage.

### Sand or dirt is too bright, dark, or solid

Likely causes:

- `0x8000` was ignored;
- vertex alpha was omitted;
- the factor-of-four alpha equation was not used consistently;
- the layer was hard-cutout instead of blended;
- base and overlay RGB lighting paths differ;
- depth bias exposes the wrong surface.

### Sand or dirt vanishes

Likely causes:

- strict depth comparison rejects coplanar fragments;
- depth bias has the wrong sign or representation;
- overlay draws before base terrain;
- the batch was merged and culled with unrelated geometry.

### Leaves appear as black rectangles

Likely causes:

- missing hard-alpha test;
- DXT3 alpha was lost or decoded incorrectly;
- leading underscores were stripped;
- the leaf batch was treated as opaque trunk geometry.

### Leaves vanish from one side

Likely causes:

- `0x2000` was ignored;
- cull state leaked or was applied object-wide;
- coordinate conversion reversed winding;
- only one LOD variant received the corrected state.

### Nets are solid sheets

Likely causes:

- rope texture alpha is ignored;
- net geometry was classified as opaque by object name;
- the hard-alpha result writes depth before fragment rejection.

### Entire net is translucent

Likely causes:

- an overlay batch's state was propagated to its parent object;
- reduced vertex alpha was treated as a blend selector;
- all `sel_kmn*` textures were globally marked transparent.

### Fish have rectangular borders

Likely causes:

- fish cards are opaque;
- DXT3 alpha is missing;
- thresholding occurs before the alpha amplification;
- the fish inherited rack or table state.

### Fish disappear behind the net

Likely causes:

- incorrect draw order;
- fish depth writes disabled;
- both fish and net were merged into one soft transparent batch;
- culling or winding removed the fish card.

### Seaweed looks ghostly

Likely causes:

- soft blending used instead of hard alpha;
- incorrect vertex alpha handling;
- depth writes disabled.

### Water draws as bright rectangles

Likely causes:

- ordinary underwater transparent terrain draws after water;
- water geometry was mixed into the zone overlay queue;
- authored alpha or curve data was discarded;
- texture animation or blend state leaked.

### Interior geometry is missing or duplicated

Likely causes:

- room companions were loaded without room-state visibility;
- root name was treated as a globally unique identifier;
- exterior proxies were not retired or hidden;
- active `clip_id` or `env_no` was ignored.

## Instrumentation

A Selbina debug inspector should expose, per batch:

- original resource and texture names;
- source placement and room/sub-area identity;
- active LOD variant and threshold distances;
- placement clip distance and current distance;
- native ordinary or transparent group;
- raw batch flags;
- coverage class;
- alpha equation and threshold;
- blend factors;
- depth test, write, and bias;
- cull mode;
- topology and vertex stride;
- texture format, dimensions, and alpha range;
- vertex-color and alpha ranges;
- displacement-vector count;
- linked lights and environment number;
- water generator identity and animation inputs where applicable.

Useful visualization modes include:

1. Opaque batches in white.
2. Hard-alpha batches in green.
3. Soft overlays in yellow.
4. Water in blue.
5. Two-sided batches in magenta.
6. Native transparent-group membership versus inferred classification.
7. Texture alpha, vertex alpha, and final amplified alpha separately.
8. Depth values and depth-write masks.
9. LOD variants in distinct colors.
10. Room ownership and visibility groups.

## Validation tour

Use a repeatable camera route that includes:

- broad terrain with dirt or sand patches;
- stairs and stone receiving overlays;
- several tree families at front and back viewing angles;
- `_ami01`, `_ami02`, and `_ami03` display nets;
- a large `_umisaku-ami` net;
- `_wakame` seaweed;
- `himono02`, `_himono03`, and `_himono04` drying-fish displays;
- hanging fish viewed against the sky;
- shoreline and water with foreground occluders;
- each accessible room transition; and
- near, medium, and far LOD distances.

Repeat the route in multiple environment states or times of day. A material can appear correct under bright daylight while leaking state or using the wrong lighting equation at night.

### Expected visual results

- Base terrain, stairs, and stone remain solid.
- Dirt and sand blend into their supporting surfaces without flicker or holes.
- Leaf backgrounds disappear while authored two-sided cards remain visible from both sides.
- Tree trunks remain normally culled and opaque.
- Rope nets contain real empty spaces.
- Soft net overlays do not make the entire structure translucent.
- Fish have clean silhouettes and visibly rest on racks or nets.
- The net remains visible around and between fish silhouettes.
- Seaweed has a firm cutout silhouette.
- Water respects opaque foreground depth and does not inherit terrain-overlay behavior.
- Room geometry appears only in its intended state with matching fog and lighting.
- LOD transitions do not change material classification.

## Automated regression tests

At minimum, test:

1. DXT3 explicit-alpha expansion.
2. The zone alpha equation and hard-alpha threshold edges.
3. Opaque and `0x8000` batches sharing one texture.
4. Leading-underscore hard-alpha classification.
5. Transparent-group or `0x8000` precedence over broad name rules.
6. `0x2000` changing culling without changing coverage.
7. Vertex alpha participating in alpha without selecting the pass.
8. Preservation of independent batches that share a texture.
9. Coplanar overlay rendering with depth test, no writes, and translated bias.
10. Fish-card discard occurring before depth write.
11. Rope holes revealing geometry behind them.
12. Tree trunk and leaf batches retaining separate states.
13. 36-byte and 48-byte vertices under list and strip topology.
14. Displacement-vector transforms and motion-aware bounds.
15. LOD equality boundaries and missing-high fallback.
16. Water remaining outside opaque and ordinary overlay queues.
17. Render-state restoration between overlay, cutout, and water draws.
18. Room entry and exit restoring visibility, clip, environment, and location state.
19. Root-name collision not causing cross-zone room ownership.
20. Pixel comparisons at representative Selbina landmarks.

### High-value pixel test

Construct or capture a scene region containing fish cards over a rope net, with solid geometry behind both.

Verify:

- fish-body pixels write fish color and depth;
- fish-background pixels write neither color nor depth;
- rope pixels remain covered and write depth;
- holes between ropes reveal the nearest geometry behind;
- no object-wide soft blending occurs; and
- changing the camera does not reorder fish and net incorrectly.

This single test exercises texture alpha, vertex alpha, hard coverage, depth timing, batch separation, and culling.

### Terrain-overlay pixel test

Render an `0x8000` sand or dirt layer over opaque terrain and place another surface behind it.

Verify:

- the base writes depth first;
- the overlay blends only where authored alpha permits;
- the overlay does not write depth;
- nearer foreground geometry occludes it;
- the layer remains visible without coplanar flicker; and
- the same texture on the base does not punch holes.

## Minimal correctness checklist

- [ ] DXT3 alpha is preserved without texture-wide material inference.
- [ ] Classification occurs per draw batch.
- [ ] Base terrain using `sel_wl1` or `sel_wl2` remains opaque.
- [ ] `0x8000` dirt and sand layers use soft blending, depth testing, no depth writes, and suitable bias.
- [ ] Overlay batches remain independent draw units.
- [ ] Leading-underscore leaves, ropes, fish, and seaweed use hard alpha unless explicit transparent state overrides it.
- [ ] Hard-alpha geometry uses `saturate(4 * vertexAlpha * textureAlpha)` and approximately `0.375` coverage.
- [ ] Hard-alpha geometry writes depth.
- [ ] `0x2000` controls culling only.
- [ ] Tree trunks and leaves do not share one forced material.
- [ ] Net structural, cutout, and soft-overlay batches remain separate.
- [ ] Fish cards do not inherit rack or net state.
- [ ] Seaweed is not rendered as an opaque or softly blended rectangle.
- [ ] Water is generator-owned and rendered in a dedicated phase.
- [ ] Water does not contribute ordinary solid collision merely because its surface is visible.
- [ ] LOD selection preserves material state across variants.
- [ ] Room companions are resolved by references rather than root-name guessing.
- [ ] Room visibility, clipping, environment, fog, lighting, and location state change together.
- [ ] Every specialized pass restores its render state.
- [ ] The complete validation tour passes at multiple view angles and environment states.

## Final implementation recipe

1. Decode Selbina textures faithfully, including DXT3 explicit alpha and the native mini-header.
2. Preserve every zone draw batch, its original name, native group, flags, topology, vertex form, and placement ownership.
3. Resolve room state, environment, clipping, and placement LOD before queueing draws.
4. Classify explicit transparent or `0x8000` batches as soft overlays.
5. Classify eligible leading-underscore leaves, ropes, fish, and seaweed as hard alpha.
6. Keep ordinary terrain, structures, trunks, racks, and solid props opaque.
7. Apply `0x2000` only as a no-cull state for the affected batch.
8. Use the zone RGB and alpha equations consistently, with hard-alpha coverage at approximately `0.375`.
9. Draw opaque and hard-alpha surfaces with depth writes enabled.
10. Draw dirt, sand, and explicit soft overlays with depth testing, no depth writes, and correctly translated bias.
11. Render water from generator/effect ownership in its dedicated transparent phase.
12. Validate fish-over-net, dirt-over-terrain, two-sided foliage, seaweed silhouettes, room transitions, and LOD changes together.

Selbina looks correct only when the renderer preserves distinctions that the assets intentionally reuse: the same codec, texture, object, and scene can contain several different surface behaviors. The solution is not a growing list of texture exceptions. It is faithful batch-level rendering.
