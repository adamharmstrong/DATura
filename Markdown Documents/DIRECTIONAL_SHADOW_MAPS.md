# Directional shadow maps

Outdoor zones in the Dynamic Shadows lighting tier now use a 2048-square R32F
shadow map when object shadows are enabled and shader model 3 is supported.
Unsupported devices and indoor zones retain the planar shadow path. Off and
Simplified lighting retain their existing behavior.

The opaque zone pass starts collecting geometry. Actors contribute their current
poses, including separate snapshots for instances sharing animated buffers.
After actors, before world items, translucent water, and effects, the renderer
draws scene depth from the directional light. A separate camera-depth pass then
selects the nearest opaque receiver; only that surface receives the composite.
Explicitly hidden objects stay hidden; offscreen zone geometry can still cast.
Cutout geometry uses texture and expanded vertex alpha, including DXT3 expansion.
Soft transparency, water, and environment shells are excluded.

The orbit-focus-centered orthographic map uses the shadow-distance setting as its
half-width (clamped to 16–160 units). It fades at the edges, snaps its translation
to texels, uses a normal-dependent comparison bias, and uses four bilinear depth
comparisons (sixteen point samples) to avoid abrupt changes at shadow-map texels.
Each comparison adjusts the reference depth to the receiver plane at the sampled
texel, preventing striped self-shadowing at grazing light angles. Light axes are
built at the origin for stable precision; rotating around the focus does not move
the map. Callers without an explicit focus retain the camera-position fallback.
NPC caster budgets are selected by distance to the orbit focus, with a stable
entity-ID tie-break and the existing zone-cell visibility filter. A selected NPC
whose nameplate is outside the camera still submits its animated shadow geometry,
without contributing to receiver depth or color. Actor casting obeys the existing
per-actor eligibility flag. Shadow opacity controls
the composite strength. Legacy planar object-size, receiver, and projection-length
controls apply only to the fallback path. Map resources are released on device reset
and shutdown and recreated lazily.

This first version multiplies the existing scene color by shadow visibility. Fog
reduces the shadow contribution, but ambient light and unshadowed point lighting
are still dimmed along with sunlight. Separating those lighting contributions,
cascaded maps, point-light cubemap shadows, transparent receivers, and performance
tuning remain future work. This is shadow mapping, not ray tracing.

Validation is in `tests/material_rendering_tests.cpp`: native D3D9 readback checks
actor occlusion, sloped receivers, cutout holes, disabled actor casting, unoccluded
surfaces, state restoration, and resource recreation. Coplanar receivers, small
camera movements, wide camera orbits, grazing-angle self-shadowing, separated
terrain layers in both draw orders, and stencil preservation are checked with
software and hardware vertex processing. Layered terrain is also tested with 4x
MSAA when supported. An optional second argument
to MaterialRenderingTests supplies the FFXI installation root for a real Bastok
Markets comparison; the first argument is the capture directory.

The local v145 and v143 linkers encountered an MSPDB140.DLL version mismatch.
Validation builds used v143 with debug information disabled via an artifact-only
MSBuild import; the production project settings were preserved.

## Receiver selection and stacked terrain

The original receiver replay multiplied the scene color once for every coplanar
piece passing the depth test. A duplicate-ground fixture reproduced 50% shadow
opacity becoming 75% darkness. Small depth differences could change which pieces
passed as the camera moved, producing broad patches that changed brightness.

Preventing repeat composition with stencil solved coplanar duplicates, but the
biased receiver replay could pull a hidden lower layer in front of the visible
upper surface. The hidden layer then claimed coverage and darkened the upper
surface. A separated-layer fixture reproduced this false shadow across five
camera orbits; reversing the layer order removed it.

The receiver now has its own D24S8 depth/stencil surface, matching the scene's
dimensions and multisampling. A depth-only pass selects the nearest visible
opaque surface using linear view depth, preserving precision between detail
layers at long distances. Composition uses the same depth expression, an EQUAL
comparison, and zero constant/slope depth offsets. Stencil bit 7 in this private
surface still limits composition to once per pixel. Original scene depth and all
scene stencil bits remain intact, and device reset releases the new resource.
World items draw afterward because they use a separate, unsubmitted draw path.

This adds one geometry depth pass per shadowed frame; performance has not been
benchmarked. Actual terrain still casts and receives shadows. Filtering allows
normal shadow-edge movement; this does not provide temporal antialiasing or
eliminate every possible source of shimmer.

## Resolution and temporal stability

The original receiver-plane inversion used an absolute threshold on the raw
screen derivatives. At large viewports those derivatives become smaller, so the
correction could switch off even on well-conditioned surfaces. A 1920-square
native fixture reproduced 409,600 falsely shadowed interior pixels in each of
five camera positions while its 128-square counterpart stayed lit. Normalizing
the UV derivatives before applying a relative conditioning threshold keeps all
five full-resolution frames lit. The regression reads native pixels rather than
checking only a downsampled capture.

The automatic sun and directional lighting now share one fractional game-minute
value per frame, rather than advancing in integer minutes every 2.4 real seconds.
Manual time overrides remain exact; authored indoor direction behavior is
unchanged. Tiny light-direction and focus-position motion tests check filtered
shadow edges, alongside the offscreen-caster and high-resolution regressions.
