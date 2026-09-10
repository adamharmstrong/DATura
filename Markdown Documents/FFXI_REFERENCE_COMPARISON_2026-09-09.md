# FFXI reference comparison for DATura

Source review dated 2026-09-09. Compared selected implementation files and documentation with DATura's current working tree and the earlier XI-Test-Client review. No upstream application, DLL, asset-writing command, or test suite was executed. Repository snapshots were fetched under `tmp/`; large asset trees were excluded from the source inspection.

## Reviewed revisions

| Repository | Commit | Commit date |
|---|---|---|
| xathei/FFXINavMeshes | `13c8dddbba54330a5a1f198ceccf89c7c0f9c763` | 2020-07-05 |
| jondwillis/kuluu-ffxi | `1c8f6bdcb7897d74dcf9298df5df31d71f758dc7` | 2026-09-08 |
| vekien/xi-zone-editor | `05d3444b1e9c92d6c6988950ec3a204e2a191e9c` | 2026-09-09 |
| vekien/xi-tools | `a36cb723722a120e8141f95b23195b065a242501` | 2026-09-09 |
| tagban/XI-Test-Client, earlier review | `5226432a14d176c697e53f161728f2b3539eaca8` | 2026-09-07 |

DATura already contains an older Kuluu reference in `tools/kuluu-ffxi-reference`, pinned to `385b0cfd4830e75e3cb0bcc1e6bcf08e448482b4` (July 15). This review used a separate current snapshot and did not update that existing reference. Several crate names have changed; current rendering code lives under `kuluu-render`.

## Recommendation by purpose

| Purpose | Best starting reference | What DATura should take |
|---|---|---|
| Direct native rendering comparison | XI-Test-Client | C++ parser/render flow, minimap, effects and audio concepts |
| Independent rendering correctness research | Kuluu | Material policies, camera collision distinctions, asset-backed regression cases |
| Persistent zone editing | xi-zone-editor | Undo/redo, final-state change sets, source identity, baseline/version comparison |
| Asset import/export and event tooling | xi-tools | Structured formats, event disassembly/compiler research, round-trip verification |
| Native navmesh generation | xi-tools | C++ Recast/Detour builder consuming collision triangles |
| Navigation query behavior | Kuluu | Polygon projection, path queries, movement along surfaces and explicit coordinate adapters |
| Historical navigation comparison | FFXINavMeshes | Optional prebuilt-data comparison and build-profile research |

There is no useful single overall winner. XI-Test-Client remains the easiest renderer to compare directly with C++, but xi-tools and Kuluu contain capabilities beyond its archived snapshot.

## FFXINavMeshes: data and interface, not a renderer

The inspected tree contains zone-numbered `.nav` files, `FFXINAV.dll`, three C# files and a README. There is no native DLL implementation or explicit root license in that tree. [Imports.cs](https://github.com/xathei/FFXINavMeshes/blob/13c8dddbba54330a5a1f198ceccf89c7c0f9c763/Imports.cs) exposes loading, building from OBJ, path queries, visibility queries and distance to a navmesh edge.

Useful DATura features inspired by it: show walkable polygons, click two points to preview a route, inspect disconnected areas, and compare navigation clearance with collision. A navmesh is derived walkability data; keep DATura's collision triangles for physical movement and do not treat a navmesh edge distance as general distance to visible geometry.

The snapshot is from 2020. Neither current installation alignment nor compatibility of its serialized data was tested. Its build settings differ from newer server-oriented profiles. Do not make these prebuilt files or the opaque DLL a required DATura dependency. Prefer a buildable implementation and locally generated meshes; no explicit license was found for copying this repository's material.

## Kuluu: substantial rendering and event research

Current [ffxi_zone_material.rs](https://github.com/jondwillis/kuluu-ffxi/blob/1c8f6bdcb7897d74dcf9298df5df31d71f758dc7/kuluu-render/src/ffxi_zone_material.rs) separates fog policy, blended terrain depth behavior and shared zone-lighting state. Its dedicated tests include camera-skip collision, cloud canopy fog, interior grounding and underworld recovery. These provide precise comparison scenarios for DATura's existing renderer and collision tests. They are more useful than assuming a screenshot or a successful build establishes parity.

The current [event crate](https://github.com/jondwillis/kuluu-ffxi/blob/1c8f6bdcb7897d74dcf9298df5df31d71f758dc7/ffxi-event/src/lib.rs) implements a steppable VM, dialogue yields and staging cues. It is a better execution reference than XI-Test-Client's index-only reader. It still has incomplete opcode coverage: the VM skips certain known-width unsupported instructions or stops, and enforces an instruction budget. Reusing its concepts would require explicit unsupported-operation reporting and careful handling of missing server state in DATura's offline mode.

[ffxi-nav-recast](https://github.com/jondwillis/kuluu-ffxi/blob/1c8f6bdcb7897d74dcf9298df5df31d71f758dc7/ffxi-nav-recast/src/lib.rs) demonstrates loading Detour meshes, exposing polygon edges, projection and surface movement. The architectural lesson is to isolate navigation queries from rendering and player physics.

Port selected algorithms and tests into C++; adopting Bevy/Rust is not required. Some integration tests skip without retail data, so test presence is not proof they ran. Kuluu's legal notice declares GPL-3.0-or-later and identifies separate dependency/data terms.

## xi-zone-editor: the strongest editor-workflow reference

The Tauri/Three.js frontend uses xi-tools as a Python backend. Its [undo-redo.js](https://github.com/vekien/xi-zone-editor/blob/05d3444b1e9c92d6c6988950ec3a204e2a191e9c/ui/editor/undo-redo.js) maintains bounded command history and discards redo history after a new edit. [changes-tracker.js](https://github.com/vekien/xi-zone-editor/blob/05d3444b1e9c92d6c6988950ec3a204e2a191e9c/ui/editor/changes-tracker.js) derives a deduplicated final-state change set against original transforms, including additions, deletions, VFX and generator source information. Version-history and publish-mode modules distinguish working edits, published content and baseline views.

DATura already lets users manipulate object transforms and visibility, but those controls would benefit from a reusable C++ command history and a saved project change set. Identify edits by source DAT plus stable placement/chunk identity, not a display name alone; repeated model names and companion rooms make identity important. Preserve generator links when copying animated objects.

Recommended scope: undo/redo, save/reload edit projects, and baseline comparison first. DAT rewriting can follow separately. The frontend is a behavioral reference, not a reason to embed Tauri or a web server. Its renderer uses its own culling and filtering policies; importing those wholesale could undo DATura's authored LOD/room work. Root LICENSE contains GPL version 3.

## xi-tools: highest-value new tooling source

This is broader than the earlier comparison implied. Beyond texture, model, animation and zone workflows, the inspected source provides event decompilation into structured JSON, compilation, annotated explanation, and zone-wide sweep checks. [xi_decompile.py](https://github.com/vekien/xi-tools/blob/a36cb723722a120e8141f95b23195b065a242501/src/xi/event/xi_decompile.py), [xi_compile.py](https://github.com/vekien/xi-tools/blob/a36cb723722a120e8141f95b23195b065a242501/src/xi/event/xi_compile.py) and `xi_event.py` are stronger starting points for event structure and operands than XI-Test-Client's preliminary index reader.

Distinguish verification levels: the raw actor/container writer preserves untouched blocks for byte-level round trips. `check_roundtrip()` compares normalized opcode listings with resolved operands and checks a second decompilation for JSON stability. This is not a byte-for-byte equality assertion over an arbitrary recompiled whole DAT, nor proof of equivalent runtime behavior. Upstream reports a broad retail sweep; it was not reproduced here. Preserve event slot order, references and unknown bytes when building DATura inspection/export support.

The native [xi_navmesh.cpp](https://github.com/vekien/xi-tools/blob/a36cb723722a120e8141f95b23195b065a242501/misc/tools/xi-navmesh/xi_navmesh.cpp) is particularly well matched to DATura: a C interface accepts vertices, triangle indices, build settings and an output path. It runs Recast stages and serializes Detour tiles. Feed it DATura's resolved world-space collision, including loaded rooms, through one explicit coordinate adapter. This avoids requiring the old FFXINAV binary or adopting Python for the runtime.

For DATura, add navmesh generation as an optional background tool with separate player/server build profiles. Cache by collision content, room set, transform convention, generator version and build settings. Keep dynamic doors distinct from static connectivity; a route through a currently closed door needs an explicit policy. Check tile format/version and polygon-reference width before loading external `.nav` data.

Root LICENSE contains GPL version 3; the bundled Recast/Detour sources are separately identified as zlib-licensed. Preserve file-level notices for any imported code. Review scope did not authorize running its installation, publish, DLL-patch or game-file-writing commands, and none were run.

## Coordinate caution applies across all five

Do not standardize on a tuple formula copied from a README without identifying the source coordinate type. XI-Test-Client documents raw FFXI to renderer `(x,-y,-z)`. xi-zone-editor describes a scene-root `(-x,-y,z)` correction. Kuluu's navigation adapter actually uses `[v.x,-v.z,-v.y]` for its caller vector, whose height is accessed as `.z`.

Those formulas alone do not establish disagreement: raw DAT, application and navigation vectors can use different axis order and camera conventions. Validate origin plus all three basis directions, inverse conversion, triangle winding, nonzero placement rotation, model/room alignment and NPC grounding. This should be the first prerequisite to sharing geometry among tools.

## Recommended DATura sequence

1. Establish shared coordinate fixtures and import focused rendering regression cases from both native and Rust references.
2. Add undo/redo and persistent zone change sets, using xi-zone-editor's workflow as the model.
3. Add walkability overlay and route preview; generate local meshes using the xi-tools C++ builder design and query them with Detour.
4. Build the event inspector around xi-tools' fuller container/operand understanding, then consider a limited, explicitly bounded dialogue interpreter informed by Kuluu.
5. Add minimap and ambience capabilities and implement/validate effect-driven water using XI-Test-Client, cross-checking against Kuluu and DATura's existing authored behavior.

This updates the earlier event-inspector recommendation: XI-Test-Client remains useful, but it is no longer the best sole source for event parsing or execution research.

## Water-status correction, 2026-09-09

The earlier wording overstated DATura's water implementation by treating general mesh/weather support as water rendering. A focused code trace found that `IsAnimatedWaterSurface` always returns false, generator-owned environment placement is restricted to matching weather directories, and ordinary user/environment loads disable `renderEffectMeshes`. Generic MZB-placed meshes may show static water geometry, but this review did not establish working water across representative zones.

Treat effect-driven water as a missing or incomplete capability to implement and validate, not an existing feature needing only refinement. For the user's rendering emphasis, promote a representative water case—resource resolution, placement, material/depth handling, then authored animation—ahead of unrelated editing or event features. See the expanded correction in `XI_TEST_CLIENT_REUSE_REVIEW_2026-09-09.md`.
