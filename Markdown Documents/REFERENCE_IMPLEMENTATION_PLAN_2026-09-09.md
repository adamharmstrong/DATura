# Reference-driven DATura implementation

Authorized by the user on 2026-09-09: implement the improvements identified from XI-Test-Client, Kuluu, xi-tools, xi-zone-editor and FFXINavMeshes, beginning with water. Work is staged so each feature has a reviewable implementation and appropriate checks. Existing uncommitted work must be preserved.

## Stages and status

1. **Water rendering — first milestone complete.** Normal zone loads now include scoped permanent river, sea, canal and basin surfaces with authored transforms, color/alpha curves and UV flow. Water blends after actors and underwater terrain overlays. East Ronfaure, Valkurm and Bastok Markets were checked with actual D3D9 captures. Finite-lifetime shoreline foam, spray, waterfalls and broader asset coverage remain follow-up water work. See `WATER_RENDERING_2026-09-09.md`.
2. **Rendering and coordinate validation — first milestone complete.** Shared scene-coordinate helpers and installed room/NPC fixtures; GPU material/depth/fog checks and a corrected textured-to-untextured shader leak; zone geometry/collision/water coverage reports with height slicing and offline HTML/CSV export. See `RENDERING_COORDINATE_VALIDATION_2026-09-09.md`. Broader installed-zone and retail-fidelity comparisons remain ongoing validation.
3. **DAT replacement resolver — complete.** Shared read policy across models, companions, resources, textures/fonts and audio DATs; startup-configured replacement root and enable setting; logical/physical source display; retail fallback only for absent overrides. File-ID tables and logical zone identity remain retail-based. See `DAT_REPLACEMENT_RESOLVER_2026-09-09.md`.
4. **Persistent zone editing — pending.** Undo/redo, project change sets, stable source/placement identity, baseline comparison and reload. Asset publishing is a separate validated operation.
5. **Minimap — pending.** Generate from loaded geometry, show player/NPC/selection markers, define multi-floor behavior.
6. **Navigation — pending.** Build local Recast/Detour meshes from collision; walkability overlay and route preview; coordinate, room, door and cache handling.
7. **Scene audio — pending.** Ambient emitters, simultaneous SFX, loop/caching/weather/zone lifecycle and independent controls.
8. **Event inspection and bounded playback — pending.** Robust event/operand inspection using xi-tools research; optional supported dialogue flow informed by Kuluu, with explicit unsupported behavior and no inferred server state.

The source reviews are reconnaissance, not proof that every upstream feature is correct or suitable. Each stage must identify actual DATura gaps and tests rather than importing another engine wholesale. No server/login/protocol stack, blanket DLL patching, asset redistribution, or renderer-backend replacement is part of this plan.

## Verification record

2026-09-09: DATura Debug/x64 and Release/x64 builds pass. The new water graphics suite passes with installed East Ronfaure (44 placements), Valkurm (11) and Bastok Markets (4). Actor composition and submerged transparent-terrain ordering match their synthetic reference images exactly. Existing room/LOD GPU checks and the sign-depth regression pass. Detailed commands and limitations are in `tests/WATER_RENDERING.md`.

### Water follow-up evidence

Valkurm's bright rectangular patches were traced to ordinary underwater transparent terrain (`ke_cl_fla1_m` and `be_ssl_st1_m`) drawing over a broad water sheet. Centroid distance alone is insufficient for this case. The renderer now finishes ordinary world transparency before water. Isolating a water family also removes the other transparent geometry, so those early captures could not establish a problem between water families.

Remaining retail-fidelity research includes clock RGB setter versus multiplier behavior, setup opcode `0x30`, and lifetime-driven shoreline geometry. XI-Test-Client's common sea shader differs from the authored per-generator approach; its material/coverage choices must not be assumed appropriate for DATura without a visual comparison.
