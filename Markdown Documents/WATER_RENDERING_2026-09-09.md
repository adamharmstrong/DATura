# Generator-owned water rendering

## Scope

Normal zone loads now enable a dedicated water path. The first supported asset families are permanent river sheets (`effect  kaw1`), base sea meshes (`umi0`, `umif`, `ukro` with `effect  umi0`), city canal meshes (`allsea`, `lowsea` with `sea     sea01`), and untextured `mizu`/`funmiz` basins. Admission requires a persistent, unattached, automatically started world MMB generator. Arbitrary transparent effects and unreferenced geometry are not enabled by this feature.

This is a first water milestone, not complete FFXI effect playback. Finite-lifetime shoreline foam, spray, waterfalls, animated scale/rotation, reflections and refraction remain outside this implementation. Authored cull distances are retained as metadata; their exact visibility semantics need validation before they control rendering. Existing scene/frustum visibility still applies.

## Loading and ownership

`model_ff11_water.h` resolves linked resources and color curves from the generator's directory or its nearest ancestor within the same `effe` root. Equal-rank ambiguity is rejected; sibling effects and unrelated resources with the same four-character ID cannot satisfy a link.

Each placement preserves its full XYZ rotation, scale and translation in DATura's existing FFXI coordinate frame. There is no additional axis reflection. Zero scale components remain zero: Valkurm's sea uses `(6,0,6)`. Singular transforms render two-sided because their determinant cannot determine the surviving plane's orientation.

Submeshes own a shared `ZoneWater::Surface` containing copied generator color, curve keys, blend mode and UV velocity. Loading a room, NPC or another DAT cannot invalidate the water's animation data. A source offset and full directory/resource/generator identity distinguish repeated placements and match the object inspector's hide/show records. Water never contributes solid collision triangles.

Long-form world-effect color setup opcodes `0x60` through `0x63` identify their curves at offset `+8`. U/V updater velocities (`0x27`/`0x28`) are converted from the authored 60 Hz rate into units per second. Opcode `0x27` is not used as a hidden-water heuristic.

## Rendering

Water uses the original MMB vertices and materials. The transparent geometry pass evaluates time-of-day color/alpha curves and absolute-time UV offsets without modifying the CPU vertices. Textured and untextured water share the FFXI color/alpha shader; DXT3 alpha uses the renderer's existing expansion convention.

Valkurm's base sea placements overlap and have feathered vertex-alpha edges. Their authored layers blend normally. XI-Test-Client's stencil coverage technique was tested, but it introduced an opacity band with these authored materials and was not retained. The bright rectangular artifacts instead came from underwater transparent terrain drawing over the water; drawing water after ordinary world transparency fixes them without replacing the source colors or geometry.

Zone opaque geometry draws first, actors draw next, then ordinary zone transparency and finally water. Each transparency group sorts back to front. Water tests depth but does not write it, so opaque foreground terrain and actors occlude it while objects behind it remain visible through the blend. Water restores shader, texture-animation, lighting and blend state before subsequent draws. It is excluded from static opaque batches. Intersecting translucent objects still use conventional approximate sorting; this is not an order-independent transparency renderer.

`ModelRenderer::Context::waterRenderingEnabled` provides a deterministic comparison switch. `animationSeconds` defaults to the runtime clock; an explicit nonnegative value selects a reproducible frame. Ordinary zone loading enables water automatically without changing the diagnostic effect-mesh option.

The shader path is the validated appearance path. The fixed-function fallback preserves basic tint and flow but cannot reproduce every shader alpha operation, especially DXT3 expansion.

## Verification

Debug and Release builds and the water graphics suite pass. Installed fixtures verify 44 East Ronfaure, 11 Valkurm and 4 Bastok Markets water placements. Build commands, pixel comparisons and regression results are recorded in `tests/WATER_RENDERING.md`. Captures use installed game files locally; game assets are not included in the tests or this document.

## Reference provenance

The implementation is newly written for DATura's existing C++/D3D9 parser and renderer. XI-Test-Client's generator/water work informed the investigation, cross-checked against actual DAT records and the other repository reviews. No upstream source text was copied for this milestone. See `XI_TEST_CLIENT_REUSE_REVIEW_2026-09-09.md` and `FFXI_REFERENCE_COMPARISON_2026-09-09.md` for pinned reference revisions and reuse findings.

The world-then-water pass boundary follows the rendering approach in [XI-Test-Client's viewer](https://github.com/tagban/XI-Test-Client/blob/5226432a14d176c697e53f161728f2b3539eaca8/renderer/viewer.cpp#L10475), independently implemented in D3D9 and verified against the underwater terrain regression.
