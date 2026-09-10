# XI-Test-Client reuse review

Reviewed 2026-09-09 against XI-Test-Client commit `5226432a14d176c697e53f161728f2b3539eaca8` and DATura's current working tree, including its uncommitted changes. This was source inspection; neither application was built or run for this review. Existing DATura implementation files were not changed.

## Recommendation

Use XI-Test-Client as a source of small, independently validated features and format research. The highest-value additions are an NPC event index inspector, replaceable DAT lookup, geometry coverage diagnostics, a minimap, and ambient sound playback. Its generator animation channels also merit a focused experiment. Replacing DATura's renderer or importing its network client would be substantially larger work with less immediate benefit to DAT exploration.

The upstream README identifies this as an archived snapshot, formerly MogHouse, targeting the August 2026 installation/server version. Its documentation sometimes describes earlier states than its code. Treat format assertions as hypotheses to check against installed assets, especially arithmetic file-ID mappings.

## Ranked opportunities

| Priority | Addition | Benefit to DATura | Relative effort |
|---|---|---|---|
| 1 | Event DAT index inspector | Show the events and raw scripts owned by a selected NPC; provides a concrete next step toward dialogue research | Medium |
| 2 | DAT replacement root | Preview modified assets without overwriting the retail installation | Small–medium; routing all loaders is the main work |
| 3 | Geometry coverage diagnostics | Distinguish missing render geometry from collision-only areas and effect surfaces | Small–medium |
| 4 | Minimap generated from loaded geometry | Navigate large zones and locate selected NPCs/objects | Medium |
| 5 | Ambient sound emitters and overlapping SFX | Turn existing sound metadata and decoding into an audible zone | Medium–large |
| 6 | Implement effect-driven water using existing geometry infrastructure | Resolve and place water resources, then add validated UV, scale and opacity animation | Scope requires asset validation; this is not an established water implementation |

Effort is comparative, not a delivery estimate.

## 1. Event DAT inspection: the strongest new capability

Upstream [FfxiEventTable.cs](https://github.com/tagban/XI-Test-Client/blob/5226432a14d176c697e53f161728f2b3539eaca8/src/MogHouse.Core/Ffxi/FfxiEventTable.cs) parses a length-prefixed container into entity-owned blocks. Each block's event index starts at byte `0x0A`; it extracts event IDs and bytecode slices, preserving unnamed `0xFFFF` slots. [Tests](https://github.com/tagban/XI-Test-Client/blob/5226432a14d176c697e53f161728f2b3539eaca8/src/MogHouse.Core.Tests/Ffxi/FfxiEventTableTests.cs) cover offset ordering, the terminator, unnamed slots, and out-of-range offsets.

DATura's `ffxi_resource.h` currently lists dialog, mob names, string tables, items, audio and image metadata, but no event-script format. `npc_interaction.h`, `npc_placement.h` and `npc_chat_window.h` already provide an interaction surface; the README explicitly explains that retail dialogue is not mapped to event scripts.

Suggested implementation: port the container/index logic into a C++ event reader, expose an Events resource view, and allow an NPC's entity ID to select its block. Display event ID, slot, byte count and raw bytes with source DAT provenance. Keep “inspect event” distinct from “play dialogue.”

This does **not** solve dialogue execution. The repository does not implement the event VM, and an entity-to-event mapping does not identify which text or branch should run. Its earlier `docs/npc-dialogue.md` says scripts have not been found, while later code and `docs/wiki/Cutscenes.md` locate and segment them. The latter also describes expansion-zone file-ID ranges that the actual `Load()` method does not implement: code still uses `5820 + zone` universally. The document describes masking count flags, whereas the parser reads the full count. Its constructor can retain already-read blocks when a later block is truncated. A DATura port should resolve these issues explicitly and report unsupported/malformed input rather than silently presenting partial success.

Validation: synthetic malformed containers plus locally installed base and expansion zones; verify known entity/event pairs independently. Do not infer message IDs by scanning arbitrary bytecode for matching integers.

## 2. DAT replacements

[filetable.cpp](https://github.com/tagban/XI-Test-Client/blob/5226432a14d176c697e53f161728f2b3539eaca8/renderer/ffxi/filetable.cpp) checks a replacement root after resolving a file ID to its normal ROM path. A replacement mirrors paths such as `ROM/1/31.DAT`; absent replacements fall back to retail files.

DATura would benefit from a configurable replacement directory, an enable toggle, and displaying both the logical asset path and actual source path. Start with `FFXIResource::ResolveFileId`, but also route direct path loads through a common resolver: DATura's `ffxi_paths.cpp`, DAT tables, scene loaders and `ffxi_file_io.cpp` are not all equivalent to file-ID lookup. Preserve the logical path for zone identification, otherwise an override path can break zone detection.

Upstream duplicates resolution across C++, C# and Python. Borrow the precedence rule, not the duplicated architecture. Test fallback, numbered ROM roots, invalid replacements, and consistent companion/model/resource lookup. Make reload behavior explicit; upstream caches its replacement-root configuration on first use.

## 3. Coverage and movement diagnostics

[coverage.h](https://github.com/tagban/XI-Test-Client/blob/5226432a14d176c697e53f161728f2b3539eaca8/renderer/coverage.h) projects triangles onto a grid and measures geometry and roughly horizontal coverage. Its collision probe also motivates measuring whether a grounded character can actually walk several directions, rather than only reporting successful floor queries.

Adapt these into DATura's existing tests/tools, consuming `ZoneCollision::Mesh` and rendered zone meshes. Export spatial maps of collision-only and render-only regions alongside room/LOD regression results. This would make missing terrain and overly restrictive collision easier to localize.

Do not copy the coverage-difference implementation unchanged: `printCoverageDiff()` marks triangle vertices only, unlike `measureCoverage()`'s triangle rasterization. It can report tessellation differences as holes. Use common bounds and full triangle rasterization for both datasets. Coverage is a diagnostic, not a universal pass percentage: water, ceilings, canopies and invisible collision intentionally differ.

## 4. Geometry-based minimap

[viewer.cpp](https://github.com/tagban/XI-Test-Client/blob/5226432a14d176c697e53f161728f2b3539eaca8/renderer/viewer.cpp) includes a top-down textured map render and radar setup; [radar_shader.h](https://github.com/tagban/XI-Test-Client/blob/5226432a14d176c697e53f161728f2b3539eaca8/renderer/radar_shader.h) supplies the display shader. It supports player/entity markers and rotating versus north-up orientation.

Implement the equivalent with a D3D9 render target and DATura's existing geometry, camera and NPC placements. This can work offline. Include the selected object/NPC and camera direction; regenerate when zone/room content changes. Multi-level interiors need an explicit floor or height policy to prevent roofs and stacked rooms from obscuring navigation. The WGSL shader and WebGPU resource code need translation, not direct inclusion.

## 5. Ambient audio

Upstream `renderer/ffxi/soundrefs.cpp` reads `0x3D` references. `viewer.cpp` associates sounds with generator placements sharing a directory, filters weather branches, and creates emitters. `renderer/sounds.cpp` supports cached decoding, simultaneous voices, held loops, volume updates and cleanup.

DATura already annotates `0x3D` sound IDs and paths in `model_ff11.cpp`, decodes BGW/SPW in `bgw_player.cpp`, and offers an audio browser. The missing opportunity is connecting those pieces to the scene and providing independent simultaneous playback voices.

Reuse the emitter/lifecycle design with an appropriate audio backend. Keep music, preview playback and scene SFX separately controllable. Validate directory-based sound association and distance attenuation using known waterfall/wind locations; it is an association heuristic, not a proven general scheduler implementation. Reuse DATura's existing multiple sound-bank search rather than narrowing it to one upstream path convention. Test loop boundaries, weather changes, zone unloading and voice limits.

## 6. Water implementation and generator research

[generator.cpp](https://github.com/tagban/XI-Test-Client/blob/5226432a14d176c697e53f161728f2b3539eaca8/renderer/ffxi/generator.cpp) reads curve references for rotation X (`0x24`), scale Z (`0x29`), opacity (`0x2D`) and UV offsets (`0x2E`/`0x2F`). `viewer.cpp` evaluates them for effects including shoreline waves.

Correction after tracing the code on 2026-09-09: the original review conflated DATura's geometry/weather infrastructure with implemented water rendering. DATura parses generator metadata and evaluates weather-scoped curves, but that does not establish a working water path:

- `ZoneModelRenderMetadata::IsAnimatedWaterSurface` in `zone_model_render_metadata.cpp` unconditionally returns false; `Prepare` assigns that result to every submesh's `animatedWater` flag. The existing generic water-scroll branches therefore are not evidence of active water animation.
- `Model_FF11_FindEnvironmentGenerators` in `model_ff11.cpp` rejects geometry outside weather directories and requires matching `/weat/` roots. It does not generally place water generators from non-weather effect directories.
- `scene_model_loader.cpp` explicitly sets `parserOptions.renderEffectMeshes = false` for user/environment loads. Having decoders for `0x1F`/`0x21`/`0x25` does not mean those effects are drawn during ordinary zone loading.

Ordinary MZB-placed water-shaped geometry could still render through the generic mesh passes, but no representative water surface was visually verified in this review. The supported conclusion is **reusable infrastructure exists; effect-driven water rendering is missing or incomplete and must be implemented and validated**, not that DATura already has water rendering. A water-specific shader is not required to implement the original asset-driven appearance.

For a rendering-focused milestone, first trace a known canal or river resource from its DAT directory and generator to an actual draw, including placement, material and depth behavior. Extend resource matching to the required non-weather scope with correct identity and lifecycle handling; then implement the needed animation channels. Do not enable every unreferenced mesh or effect indiscriminately, or add name-based scrolling as a substitute for authored animation.

Use Valkurm shoreline layers as a controlled comparison. Keep lifecycle-relative animation separate from Vana'diel time-of-day curves. Upstream uses configurable `kWavePeriod` because authored timing remains unresolved, and its global curve map permits repeated names to overwrite earlier entries. Preserve DATura's directory/weather-aware identity. Different generator sections assign different meanings to the same opcode, so do not add a section-independent switch.

## Preserve DATura's current strengths

- **Rooms and LOD:** retain `zone_room_catalog.h`, `zone_room_loader.cpp` and authored replacement-link behavior. Upstream uses `subrooms.txt`, manual hidden-model lists, and documentation containing unresolved alignment assumptions. Its catalog is useful for cross-checking, not as a replacement authority.
- **Collision:** DATura already has a spatial index, horizontal collision resolution, safe-floor search and vertical motion. Borrow regression scenarios instead of replacing it with another implementation.
- **Resources/audio:** file tables, text decoding, item inspection and BGW/SPW preview already exist. Compare edge cases instead of introducing duplicate parsers wholesale.
- **Rendering architecture:** upstream requires Dawn/WebGPU and SDL3 and has a substantial C# layer. A backend migration is a separate portability project. Keep useful parser logic independent of those dependencies.
- **Networking and quest helpers:** protocol parsers could inform an eventual optional live-server mode, but that is outside DATura's present exploration focus. The quest-helper document is a proposal, not an implemented quest engine; cutscene playback is also absent.

## Reuse conditions and next milestone

The upstream root [LICENSE](https://github.com/tagban/XI-Test-Client/blob/5226432a14d176c697e53f161728f2b3539eaca8/LICENSE) is MIT, with a 2026 John Leighow copyright notice and a requirement to retain the copyright and permission notice in copies or substantial portions. DATura declares GPL-3.0-or-later and already maintains `THIRD_PARTY_NOTICES.md`. Any actual import should preserve the upstream notice and record source files and commit there; review separately identified vendor material individually. Source-code licensing does not establish permission to redistribute retail DATs or other game assets.

Recommended first milestone: **a read-only NPC event inspector with robust container validation and visible source provenance**. It delivers useful functionality without requiring a server or speculative bytecode execution. Follow with the replacement resolver and coverage diagnostics, then the minimap and scene audio. Keep generator-channel work as an independently validated rendering experiment.

Priority clarification after the user's rendering-focused follow-up: the above was a general exploration-tool recommendation. For the current emphasis, establishing visible, correctly placed water in a representative zone is a stronger candidate than assuming only additional water animation polish remains. The implementation scope is not yet measured.
